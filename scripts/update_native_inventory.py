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

from symbian.project.native_surface import (
    apply_header_edits,
    generate_stringtable,
)


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
                    ".rsg",
                    ".mbg",
                    ".loc",
                    ".rssi",
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
    compatibility = json.loads(
        (
            workspace / "research/native-sdk/header-compatibility.json"
        ).read_text()
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
    # These overlapping exports differ only in relocated component ownership
    # or select the actual strong-crypto variant used by the chosen DEF.
    selections = {
        "biditext.h": "textandloc-sdk/fontservices/textbase/inc/BidiText.h",
        "padding.h": "security/crypto/weakcryptospi/inc/padding.h",
        "khronos_types.h": (
            "graphics/egl/eglinterface/include/1.4/khronos_types.h"
        ),
    }
    for name, source in selections.items():
        choices = [r for r in candidates[name] if r["source"] == source]
        if choices:
            candidates[name] = choices
    for name, choices in list(candidates.items()):
        spi = [
            r
            for r in choices
            if r["source"].startswith("security/crypto/weakcryptospi/")
        ]
        if spi and len({r["sha256"] for r in spi}) == 1:
            candidates[name] = spi
    for choices in candidates.values():
        for record in choices:
            edits = compatibility.get(record["include"].casefold())
            if edits:
                record["source_sha256"] = record["sha256"]
                record["compatibility_edits"] = edits
                record["sha256"] = hashlib.sha256(
                    apply_header_edits(
                        (root / record["source"]).read_bytes(), edits
                    )
                ).hexdigest()
    # MMP STRINGTABLE blocks export generated public headers. Reuse the
    # original SDK generator rather than inventing a parallel implementation.
    http = "netprotocols/applayerprotocols/httptransportfw"
    table = f"{http}/strings/HttpStringConstants.st"
    generator = "ossrv/lowlevellibsandfws/apputils/stringtools/stringtable.pl"
    if (root / table).is_file() and (root / generator).is_file():
        record = {
            "include": "httpstringconstants.h",
            "source": table,
            "input_sha256": digest(root / table),
            "sha256": hashlib.sha256(
                generate_stringtable(root, table, generator)
            ).hexdigest(),
            "generator": {
                "source": generator,
                "sha256": digest(root / generator),
            },
            "manifest": f"{http}/group/http.mmp",
            "declaration": (
                "START STRINGTABLE HttpStringConstants.st; "
                "EXPORTPATH /epoc32/include"
            ),
            "classification": "public_base_platform",
            "conditions": [],
        }
        candidates[record["include"]].append(record)
        public.append(record)
    facilities = json.loads(
        (workspace / "research/native-sdk/facilities.json").read_text()
    )["facilities"]
    libraries = library_definitions(root, list(profile["sources"]))
    for facility in facilities:
        dll = facility["dll"].removesuffix(".dll")
        choices = libraries[dll]
        if (
            choices
            and facility.get("blocked", "").startswith("No reviewed MMP")
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
                    and Path(r["include"]).name
                    not in ("gi18n.h", "gi18n-lib.h")
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
    for facility in facilities:
        if facility["dll"] in (
            "libglib.dll",
            "libgobject.dll",
            "libgmodule.dll",
            "libgthread.dll",
        ):
            facility["classification"] = "public_standard_extension"
            facility["dependencies"] = ["OpenC"]
            if facility["dll"] != "libglib.dll":
                facility["dependencies"].append("Native_libglib")
        if facility["dll"] in ("libstdcpp.dll", "libstdcppv5.dll") or any(
            "stlport/" in h for h in facility["headers"]
        ):
            facility["classification"] = "public_standard_extension"
            facility["blocked"] = (
                "Historical STLport/Open C++ runtime ABI and startup are "
                "not verified with Clang; modern libc++ is a separate API"
            )
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
            queue.append(({**choice, "include": header}, facility["target"]))
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
        if installed["include"] != path:
            aliases = installed.setdefault("aliases", [])
            destination = "include/native/" + path
            if destination not in aliases:
                aliases.append(destination)
        installed["targets"].append(owner)
        text = (
            generate_stringtable(
                root, record["source"], record["generator"]["source"]
            ).decode()
            if record.get("generator")
            else apply_header_edits(
                (root / record["source"]).read_bytes(),
                record.get("compatibility_edits", []),
            ).decode(errors="replace")
        )
        for include in re.findall(
            r'^\s*#\s*include\s*[<"]([^>"\n]+)', text, re.M
        ):
            include = include.replace("\\", "/")
            choices = candidates[include.casefold()]
            if not choices:
                relative = str(Path(record["include"]).parent / include)
                choices = candidates[relative.casefold()]
                if choices:
                    include = relative
            if not choices:
                source = resolve_case(
                    (root / record["source"]).parent / include
                )
                # A public header can require an unexported implementation
                # header. Preserve this exact textual dependency; it does not
                # become an independently selectable public API.
                if source.is_file() and source.is_relative_to(root):
                    relative = str(Path(record["include"]).parent / include)
                    choices = [
                        {
                            "include": relative,
                            "source": str(source.relative_to(root)),
                            "sha256": digest(source),
                            "classification": "private_internal",
                            "exposure": "required_textual_support",
                            "included_by": record["source"],
                        }
                    ]
                    include = relative
                else:
                    continue
            if len({r["sha256"] for r in choices}) > 1:
                same_repo = [
                    r
                    for r in choices
                    if r["source"].split("/")[0]
                    == record["source"].split("/")[0]
                ]
                if same_repo:
                    scores = {
                        r["source"]: len(
                            Path(
                                os.path.commonpath(
                                    [record["source"], r["source"]]
                                )
                            ).parts
                        )
                        for r in same_repo
                    }
                    best = max(scores.values())
                    same_repo = [
                        r for r in same_repo if scores[r["source"]] == best
                    ]
                if len({r["sha256"] for r in same_repo}) != 1:
                    continue
                choices = same_repo
            queue.append(({**choices[0], "include": include}, owner))
    headers = []
    for record in public:
        item = delivered.pop(record["include"].casefold(), None)
        if item:
            headers.append(item)
        else:
            reason = (
                "Alternative legacy cstdlib/libc export tree conflicts with "
                "the selected Open C headers in independent C canaries; "
                "consumer include layout and ABI ownership require review"
                if record["manifest"]
                == "ossrv/genericopenlibs/cstdlib/group/bld.inf"
                else "Public export inventoried; target ownership "
                "has not been reviewed"
            )
            headers.append(
                {
                    **record,
                    "blocked": record.get("blocked", reason),
                }
            )
    headers += list(delivered.values())
    for record in headers:
        if record.get("targets"):
            record["targets"] = sorted(set(record["targets"]))
        if record.get("destination"):
            text = (root / record["source"]).read_text(errors="replace")
            if record["include"].startswith("stdapis/glib-2.0/gobject/"):
                record["include_via"] = "glib-object.h"
            umbrella = re.search(r"#error[^\n]*?<([^>]+)>", text)
            if umbrella and umbrella[1] in ("glib.h", "glib-object.h"):
                record["include_via"] = umbrella[1]
                guard = re.search(r"^#ifndef\s+(\w+)", text, re.M)
                if guard:
                    record["include_guard"] = guard[1]
    for facility in facilities:
        facility["header_payloads"] = sorted(
            {
                p
                for record in headers
                if facility["target"] in record.get("targets", [])
                for p in [record["destination"], *record.get("aliases", [])]
            }
        )
        facility["ms_extensions"] = any(
            facility["target"] in record.get("targets", [])
            and Path(record["include"]).name.casefold()
            in (
                "cmdefconnvalues.h",
                "lbtstartuptrigger.h",
                "aiwvariant.h",
                "eikedwin.h",
            )
            for record in headers
        )
    avkon_resources = next(
        facility["selection_blocked"]
        for facility in facilities
        if facility["target"] == "Avkon"
    )
    for facility in facilities:
        if "include/native/AknUtils.h" in facility["header_payloads"]:
            facility.setdefault("selection_blocked", avkon_resources)
    by_name = {f["target"]: f for f in facilities}
    changed = True
    while changed:
        changed = False
        for facility in facilities:
            if facility.get("blocked"):
                continue
            for dependency in facility["dependencies"]:
                if dependency in by_name and by_name[dependency].get("blocked"):
                    facility["blocked"] = (
                        f"Required public dependency Symbian::{dependency} "
                        f"is blocked: {by_name[dependency]['blocked']}"
                    )
                    changed = True
                    break
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
        "auxiliary_manifests": [
            {
                "destination": "share/symbian/native/openc-header-usage.json",
                "sha256": digest(
                    workspace / "research/native-sdk/openc-header-usage.json"
                ),
            }
        ],
        "licenses": [
            {
                "source": str(path.relative_to(root)),
                "sha256": digest(path),
                "destination": "licenses/native/" + str(path.relative_to(root)),
            }
            for name in profile["sources"]
            for path in sorted((root / name).rglob("*"))
            if path.is_file()
            and re.fullmatch(
                r"(LICENSE|LICENCE|COPYING)(\.[A-Za-z0-9_-]+)?", path.name, re.I
            )
            and ".git" not in path.parts
        ],
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
