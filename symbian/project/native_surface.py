"""Stages reviewed public native interfaces from the SDK inventory.

Frozen export parsing and proxy construction belong to the native SDK library.
This module owns only source provenance, file staging and CMake metadata.
"""

import hashlib
import json
import shutil
import subprocess
import tempfile
from pathlib import Path

from symbian.native import require_native
from symbian.sdk import build_import_proxy

INVENTORY = "research/native-sdk/inventory.json"


def apply_header_edits(contents: bytes, edits: list[dict]) -> bytes:
    """Applies reviewed, exact edits without changing declarations or layout."""
    for edit in edits:
        before, after = edit["before"].encode(), edit["after"].encode()
        if contents.count(before) != 1:
            raise ValueError(
                "Native header compatibility edit no longer matches"
            )
        contents = contents.replace(before, after, 1)
    return contents


def generate_stringtable(root: Path, source: str, generator: str) -> bytes:
    """Runs the original EPL SDK generator with a reproducible input path."""
    with tempfile.TemporaryDirectory(prefix="symbian-stringtable-") as scratch:
        directory = Path(scratch) / "input"
        directory.mkdir()
        table = Path(source).name
        shutil.copyfile(root / source, directory / table)
        subprocess.run(
            ["perl", str((root / generator).resolve()), f"input/{table}"],
            cwd=scratch,
            check=True,
            capture_output=True,
        )
        return (directory / (Path(table).stem.lower() + ".h")).read_bytes()


def inventory(workspace: Path) -> dict:
    """Loads the reviewed, versioned native delivery contract."""
    return json.loads((workspace / INVENTORY).read_text())


def _copy(workspace: Path, output: Path, record: dict) -> None:
    """Copies a pinned input, rejecting missing or changed source bytes."""
    source = workspace / "research/upstream" / record["source"]
    if not source.is_file():
        raise ValueError(
            f"Missing native SDK input {source}; run "
            "scripts/prepare_native_sources.py"
        )
    digest = hashlib.sha256(source.read_bytes()).hexdigest()
    if digest != record.get(
        "source_sha256", record.get("input_sha256", record["sha256"])
    ):
        raise ValueError(f"Native SDK source digest mismatch: {source}")
    contents = source.read_bytes()
    if record.get("compatibility_edits"):
        contents = apply_header_edits(contents, record["compatibility_edits"])
        if hashlib.sha256(contents).hexdigest() != record["sha256"]:
            raise ValueError(
                f"Native SDK adjusted header digest mismatch: {source}"
            )
    if record.get("generator"):
        generator = record["generator"]
        generator_path = workspace / "research/upstream" / generator["source"]
        if (
            hashlib.sha256(generator_path.read_bytes()).hexdigest()
            != generator["sha256"]
        ):
            raise ValueError(
                f"Native SDK generator digest mismatch: {generator_path}"
            )
        contents = generate_stringtable(
            workspace / "research/upstream",
            record["source"],
            generator["source"],
        )
        if hashlib.sha256(contents).hexdigest() != record["sha256"]:
            raise ValueError(
                f"Native SDK generated header digest mismatch: {source}"
            )
    destination = output / record["destination"]
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(contents)
    for alias in record.get("aliases", []):
        alias_path = output / alias
        alias_path.parent.mkdir(parents=True, exist_ok=True)
        alias_path.write_bytes(contents)


def stage_native_headers(workspace: Path, output: Path) -> None:
    """Stages public headers and their explicitly recorded textual closure."""
    data = inventory(workspace)
    for record in data["headers"]:
        if record.get("destination"):
            _copy(workspace, output, record)
    for record in data.get("licenses", []):
        _copy(workspace, output, record)
    notices = output / "licenses/native"
    notices.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(
        workspace / "third_party/symbian/EPL-1.0.html",
        notices / "EPL-1.0.html",
    )
    metadata = output / "share/symbian/native"
    metadata.mkdir(parents=True, exist_ok=True)
    (metadata / "inventory.json").write_text(json.dumps(data, indent=2) + "\n")
    for name in (
        "sources.json",
        "facilities.json",
        "header-compatibility.json",
    ):
        shutil.copyfile(
            workspace / "research/native-sdk" / name, metadata / name
        )


def stage_native_imports(
    workspace: Path, output: Path, compiler: Path, linker: Path
) -> None:
    """Builds full frozen interfaces without copying firmware DLLs."""
    data = inventory(workspace)
    for facility in data["facilities"]:
        if facility.get("blocked"):
            continue
        definition = facility["definition"]
        _copy(workspace, output, definition)
        dll = facility["dll"].removesuffix(".dll")
        proxy = output / "proxies" / dll / f"{dll}.dso"
        if proxy.is_file():
            native = require_native()
            expected = {
                (e.symbol, e.ordinal, e.data)
                for e in native.parse_def(
                    (output / definition["destination"]).read_bytes()
                )
                if not e.absent
            }
            actual = native.inspect_import_proxy(proxy.read_bytes())
            if (
                actual.target_dll == facility["dll"]
                and {(e.symbol, e.ordinal, e.data) for e in actual.exports}
                == expected
            ):
                continue
        build_import_proxy(
            output / definition["destination"],
            [],
            facility["dll"],
            output / "proxies" / dll,
            str(compiler),
            str(linker),
        )
    validate_native_payload(output)


def validate_native_payload(prefix: Path) -> None:
    """Rejects omissions or altered inputs in an export or relocated bundle."""
    data = json.loads(
        (prefix / "share/symbian/native/inventory.json").read_text()
    )
    records = [h for h in data["headers"] if h.get("destination")]
    records += data.get("licenses", [])
    records += [
        f["definition"] for f in data["facilities"] if not f.get("blocked")
    ]
    for record in records:
        for destination in [record["destination"], *record.get("aliases", [])]:
            path = prefix / destination
            if not path.is_file():
                raise ValueError(f"Missing native SDK payload: {path}")
            if (
                hashlib.sha256(path.read_bytes()).hexdigest()
                != record["sha256"]
            ):
                raise ValueError(f"Native SDK payload digest mismatch: {path}")
    for facility in data["facilities"]:
        if facility.get("blocked"):
            continue
        dll = facility["dll"].removesuffix(".dll")
        path = prefix / "proxies" / dll / f"{dll}.dso"
        if not path.is_file():
            raise ValueError(f"Missing native SDK import interface: {path}")
        native = require_native()
        definition = prefix / facility["definition"]["destination"]
        expected = {
            (e.symbol, e.ordinal, e.data)
            for e in native.parse_def(definition.read_bytes())
            if not e.absent
        }
        actual = native.inspect_import_proxy(path.read_bytes())
        delivered = {(e.symbol, e.ordinal, e.data) for e in actual.exports}
        if actual.target_dll != facility["dll"] or delivered != expected:
            raise ValueError(
                f"Incomplete frozen native import interface: {path}"
            )
