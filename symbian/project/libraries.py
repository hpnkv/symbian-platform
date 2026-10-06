"""Stages CMake-owned application DLLs for automatic SIS packaging."""

import hashlib
import json
from pathlib import Path

from symbian.status import Code, StatusError


def built_libraries(tree: Path, name: str) -> dict[str, bytes]:
    """Reads only application graph outputs named by the SDK CMake helper."""
    manifest = tree / f"{name}.libraries.json"
    if not manifest.is_file():
        return {}
    data = json.loads(manifest.read_text())
    if data.get("schema") != "symbian.application-libraries/v1":
        raise StatusError(Code.INVALID_ARGUMENT, "Unknown DLL graph schema")
    result = {}
    for filename in data["libraries"]:
        path = Path(filename).resolve()
        if not path.is_relative_to(tree.resolve()) or path.suffix != ".dll":
            raise StatusError(
                Code.INVALID_ARGUMENT, "Application DLL must be a build output"
            )
        if path.name in result:
            raise StatusError(
                Code.INVALID_ARGUMENT, "Duplicate bundled DLL name"
            )
        result[path.name] = path.read_bytes()
    return result


def stage_libraries(
    output: Path, name: str, libraries: dict[str, bytes]
) -> None:
    """Publishes DLL payloads and a relative digest manifest beside the EXE."""
    directory = output / "libraries"
    directory.mkdir(exist_ok=True)
    records = []
    for filename, contents in sorted(libraries.items(), key=lambda row: row[0].casefold()):
        (directory / filename).write_bytes(contents)
        records.append(
            {
                "file": f"libraries/{filename}",
                "sha256": hashlib.sha256(contents).hexdigest(),
            }
        )
    (output / f"{name}.libraries.json").write_text(
        json.dumps(
            {"schema": "symbian.application-payload/v1", "libraries": records},
            indent=2,
        )
        + "\n"
    )


def package_libraries(artifact: Path) -> list[tuple[str, bytes]]:
    """Checks only the staged DLL manifest after relocation."""
    manifest = artifact.with_suffix(".libraries.json")
    if not manifest.is_file():
        return []
    data = json.loads(manifest.read_text())
    if data.get("schema") != "symbian.application-payload/v1":
        raise StatusError(Code.INVALID_ARGUMENT, "Unknown DLL payload schema")
    result = []
    for record in data["libraries"]:
        relative = Path(record["file"])
        path = (artifact.parent / relative).resolve()
        if (
            relative.is_absolute()
            or relative.parts != ("libraries", relative.name)
            or path.parent != (artifact.parent / "libraries").resolve()
            or path.suffix != ".dll"
        ):
            raise StatusError(Code.INVALID_ARGUMENT, "Invalid bundled DLL path")
        contents = path.read_bytes()
        if hashlib.sha256(contents).hexdigest() != record["sha256"]:
            raise StatusError(Code.DATA_LOSS, "Bundled DLL digest mismatch")
        result.append((f"!:\\sys\\bin\\{path.name}", contents))
    return sorted(result, key=lambda row: row[0].casefold())
