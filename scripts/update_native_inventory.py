"""Regenerates native SDK coverage from pinned public export manifests.

This is a source-manifest scanner, not a Symbian binary or DEF parser. Unknown
conditions, unresolved public exports and unreviewed ownership remain visible.
"""

import argparse
import hashlib
import json
import os
import re
import xml.etree.ElementTree as ET
from collections import defaultdict, deque
from pathlib import Path


def digest(path: Path) -> str:
    """Returns the preserved input's byte digest."""
    return hashlib.sha256(path.read_bytes()).hexdigest()


def resolve_case(path: Path) -> Path:
    """Resolves historical Windows path casing on case-sensitive hosts."""
    path = path.absolute()
    current = Path(path.anchor)
    for part in path.parts[1:]:
        if part == "..":
            current = current.parent
        elif part != ".":
            exact = current / part
            if exact.exists():
                current = exact
            elif current.is_dir():
                matches = [
                    p
                    for p in current.iterdir()
                    if p.name.casefold() == part.casefold()
                ]
                if len(matches) != 1:
                    return path
                current = matches[0]
            else:
                return path
    return current


def library_definitions(root: Path, names: list[str]) -> dict[str, list[dict]]:
    """Finds actual DLL names and frozen EABI files from library definitions."""
    libraries = defaultdict(list)
    for name in names:
        files = sorted((root / name).rglob("*"))
        definitions = [
            p
            for p in files
            if p.suffix.lower() == ".def" and "/eabi/" in str(p).lower()
        ]
        for mmp in files:
            if mmp.suffix.lower() != ".mmp":
                continue
            if re.search(
                r"/(test[^/]*|tsrc|examples?)/",
                str(mmp.relative_to(root)),
                re.I,
            ):
                continue
            text = re.sub(
                r"/\*.*?\*/|//[^\n]*",
                "",
                mmp.read_text(errors="replace"),
                flags=re.S,
            )
            target = re.search(r"^\s*TARGET\s+(\S+)\.dll", text, re.M | re.I)
            if not target:
                continue
            dll = target[1].lower()
            deffile = re.search(r"^\s*DEFFILE\s+(\S+)", text, re.M | re.I)
            stems = {dll, dll + "u", dll + "2u"}
            if deffile:
                stem = Path(deffile[1].replace("\\", "/")).stem.lower()
                stems = {stem, stem + "u", stem + "2u"}
            candidates = [p for p in definitions if p.stem.lower() in stems]
            if not candidates:
                continue
            # Shared API DEF directories may be above the MMP's subdirectory.
            # Prefer the closest common component and refuse variant ambiguity.
            scores = {
                p: len(Path(os.path.commonpath([mmp, p])).parts)
                for p in candidates
            }
            best = max(scores.values())
            candidates = [p for p in candidates if scores[p] == best]
            if len({digest(p) for p in candidates}) != 1:
                continue
            libraries[dll].append(
                {
                    "definition": str(candidates[0].relative_to(root)),
                    "mmp": str(mmp.relative_to(root)),
                    "implementation_dependencies": re.findall(
                        r"([\w-]+)\.lib",
                        " ".join(
                            re.findall(
                                r"^\s*LIBRARY\s+([^\n]+)", text, re.M | re.I
                            )
                        ),
                        re.I,
                    ),
                }
            )
    return libraries


def exports(root: Path, names: list[str]) -> tuple[list[dict], list[dict]]:
    """Inventories public and partner exports, retaining branch conditions."""
    public, support = [], []
    for name in names:
        for manifest in sorted((root / name).rglob("*")):
            if manifest.suffix.lower() != ".inf":
                continue
            if re.search(
                r"/(test[^/]*|tsrc|examples?)/",
                str(manifest.relative_to(root)),
                re.I,
            ):
                continue
            section = ""
            conditions = []
            text = re.sub(
                r"/\*.*?\*/",
                "",
                manifest.read_text(errors="replace"),
                flags=re.S,
            )
            for raw in text.splitlines():
                line = raw.split("//", 1)[0].strip()
                if line.startswith(("#if", "#else", "#elif")):
                    if line.startswith(("#else", "#elif")) and conditions:
                        conditions[-1] += " / " + line
                    else:
                        conditions.append(line)
                    continue
                if line.startswith("#endif"):
                    if conditions:
                        conditions.pop()
                    continue
                if line.startswith("PRJ_"):
                    section = line.split()[0]
                    continue
                if section != "PRJ_EXPORTS":
                    continue
                match = re.match(
                    r"(\S+)\s+(?:\w*(PUBLIC|PLATFORM)_EXPORT_PATH\(([^)]+)\)"
                    r"|/epoc32/include/(\S+))",
                    line,
                    re.I,
                )
                if not match:
                    continue
                filename, kind, macro_path, literal_path = match.groups()
                include = (macro_path or literal_path).replace("\\", "/")
                if Path(include).suffix.lower() not in (
                    ".h",
                    ".inl",
                    ".hrh",
                    ".rh",
                ):
                    continue
                source = resolve_case(
                    manifest.parent / filename.replace("\\", "/")
                )
                record = {
                    "include": include,
                    "source": str(source.relative_to(root)),
                    "manifest": str(manifest.relative_to(root)),
                    "declaration": line,
                    "conditions": list(conditions),
                    "classification": (
                        "public_base_platform"
                        if kind == "PUBLIC"
                        else "private_internal"
                    ),
                }
                if source.is_file():
                    record["sha256"] = digest(source)
                else:
                    record["blocked"] = (
                        "Exported source file absent in pinned checkout"
                    )
                # Nokia's API packaging metadata explicitly marks several S60
                # SDK APIs as SDK releases despite PLATFORM export macros.
                for metadata in manifest.parent.parent.glob("*.metaxml"):
                    try:
                        api = ET.parse(metadata).getroot()
                    except ET.ParseError:
                        continue
                    release = api.find("release")
                    if release is not None and release.get("category") in (
                        "sdk",
                        "public",
                    ):
                        record["classification"] = "public_base_platform"
                        record["api_metadata"] = str(metadata.relative_to(root))
                        record["api_libraries"] = [
                            lib.get("name") for lib in api.findall("./libs/lib")
                        ]
                (
                    public
                    if record["classification"] == "public_base_platform"
                    else support
                ).append(record)
    return public, support


def generate(workspace: Path) -> dict:
    """Connects expected public exports to targets and delivered files."""
    root = workspace / "research/upstream"
    profile = json.loads(
        (workspace / "research/native-sdk/sources.json").read_text()
    )
    public, support = exports(root, list(profile["sources"]))
    candidates = defaultdict(list)
    for record in public + support:
        if ".." in Path(record["include"]).parts:
            record["blocked"] = (
                "Historical export destination escapes its include root; "
                "layout requires explicit review"
            )
            continue
        if "sha256" in record:
            candidates[record["include"].casefold()].append(record)
    facilities = json.loads(
        (workspace / "research/native-sdk/facilities.json").read_text()
    )["facilities"]
    libraries = library_definitions(root, list(profile["sources"]))
    for facility in facilities:
        dll = facility["dll"].removesuffix(".dll")
        choices = libraries[dll]
        if (
            choices
            and facility.get("blocked")
            and len({c["definition"] for c in choices}) == 1
        ):
            facility.pop("blocked")
            facility.update(choices[0])
    # Public Nokia API packaging lists are an independent expectation source.
    # Keep every named library visible even when its payload is unavailable.
    packaged = defaultdict(list)
    for record in public:
        for library in record.get("api_libraries", []):
            packaged[library.lower().removesuffix(".lib")].append(record)
    existing = {f["dll"].removesuffix(".dll"): f for f in facilities}
    for dll, records in sorted(packaged.items()):
        if dll in existing:
            continue
        facility = {
            "target": "Native_" + dll,
            "dll": dll + ".dll",
            "headers": sorted(
                {
                    r["include"]
                    for r in records
                    if Path(r["include"]).suffix.lower() == ".h"
                }
            ),
            "dependencies": [],
            "classification": "public_base_platform",
            "availability": "unknown_firmware",
            "api_metadata": sorted({r["api_metadata"] for r in records}),
            "blocked": (
                "SDK API metadata names this library; its frozen EABI "
                "definition and library identity have not been reviewed"
            ),
        }
        choices = libraries[dll]
        if choices and len({c["definition"] for c in choices}) == 1:
            facility.pop("blocked")
            facility.update(choices[0])
        facilities.append(facility)
    delivered = {}
    queue = deque()
    for facility in facilities:
        if facility.get("blocked"):
            continue
        definition = resolve_case(root / facility["definition"])
        facility["definition"] = {
            "source": str(definition.relative_to(root)),
            "sha256": digest(definition),
            "destination": "share/symbian/native/defs/"
            + facility["dll"]
            + ".def",
        }
        missing = []
        for header in facility["headers"]:
            choices = candidates[header.casefold()]
            choices = [
                r for r in choices if r["classification"] != "private_internal"
            ]
            if not choices:
                missing.append(header)
                continue
            choice = choices[0]
            if len({r["sha256"] for r in choices}) != 1:
                same_repo = [
                    r
                    for r in choices
                    if r["source"].split("/")[0]
                    == facility["mmp"].split("/")[0]
                ]
                # The facility DEF selects a component variant (for example
                # weakcrypto versus weakcryptospi), not just a repository.
                same_component = [
                    r
                    for r in same_repo
                    if r["source"].startswith(
                        str(
                            Path(facility["definition"]["source"]).parent.parent
                        )
                    )
                ]
                if same_component:
                    same_repo = same_component
                if len({r["sha256"] for r in same_repo}) != 1:
                    missing.append(header + " (ambiguous export)")
                    continue
                choice = same_repo[0]
            queue.append((choice, facility["target"]))
        if missing:
            facility["blocked"] = (
                "Public header export not established: " + ", ".join(missing)
            )
    # Deliver only the reviewed API's include closure. Never publish every
    # partner export merely because its source exists.
    seen = set()
    while queue:
        record, owner = queue.popleft()
        key = (record["include"].casefold(), owner)
        if key in seen:
            continue
        seen.add(key)
        path = record["include"]
        installed = delivered.setdefault(
            path.casefold(),
            {
                **record,
                "destination": "include/native/" + path,
                "targets": [],
            },
        )
        if installed["sha256"] != record["sha256"]:
            raise ValueError(f"Conflicting native header payload: {path}")
        installed["targets"].append(owner)
        text = (root / record["source"]).read_text(errors="replace")
        for include in re.findall(
            r'^\s*#\s*include\s*[<"]([^>"\n]+)', text, re.M
        ):
            include = include.replace("\\", "/")
            choices = candidates[include.casefold()]
            if not choices:
                continue
            if len({r["sha256"] for r in choices}) > 1:
                same_repo = [
                    r
                    for r in choices
                    if r["source"].split("/")[0]
                    == record["source"].split("/")[0]
                ]
                if len({r["sha256"] for r in same_repo}) != 1:
                    continue
                choices = same_repo
            queue.append((choices[0], owner))
    headers = []
    for record in public:
        item = delivered.pop(record["include"].casefold(), None)
        if item:
            headers.append(item)
        else:
            headers.append(
                {
                    **record,
                    "blocked": record.get(
                        "blocked",
                        "Public export inventoried; target ownership "
                        "has not been reviewed",
                    ),
                }
            )
    headers += list(delivered.values())
    for record in headers:
        if record.get("targets"):
            record["targets"] = sorted(set(record["targets"]))
    return {
        "schema": "symbian.native-inventory/v1",
        "baseline": profile["baseline"],
        "license": (
            "Original per-file notices govern; platform sources "
            "predominantly EPL-1.0"
        ),
        "architectures": ["armv5t", "armv6"],
        "kernel": "eka2",
        "firmware_compatibility": "unknown unless separately recorded",
        "sources": profile["sources"],
        "facilities": facilities,
        "headers": headers,
        "private_export_count": len(support),
    }


def main() -> None:
    """Updates or checks the inventory against prepared source snapshots."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--workspace", type=Path, default=Path.cwd())
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    result = json.dumps(generate(args.workspace.resolve()), indent=2) + "\n"
    output = args.workspace / "research/native-sdk/inventory.json"
    if args.check:
        if output.read_text() != result:
            raise SystemExit(
                "Native SDK inventory is stale; regenerate and review changes"
            )
    else:
        output.write_text(result)


if __name__ == "__main__":
    main()
