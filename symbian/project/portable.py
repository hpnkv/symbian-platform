"""Pinned, source-built portable guest libraries."""

import hashlib
import json
import shutil
import tempfile
from pathlib import Path

from symbian.process import run


def zlib_manifest(workspace: Path) -> dict:
    """Verifies all zlib inputs against the reviewed source manifest."""
    record = json.loads((workspace / "research/portable/zlib.json").read_text())
    source = workspace / "research/upstream" / record["source"]
    for name, expected in record["files"].items():
        path = source / name
        if (
            not path.is_file()
            or hashlib.sha256(path.read_bytes()).hexdigest() != expected
        ):
            raise ValueError(
                f"Pinned portable zlib input missing or changed: {path}"
            )
    return record


def stage_zlib_headers(workspace: Path, output: Path) -> None:
    """Stages version-matched public headers, provenance and license."""
    record = zlib_manifest(workspace)
    source = workspace / "research/upstream" / record["source"]
    for name, destination in record["headers"].items():
        path = output / destination
        path.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source / name, path)
    license_path = output / "licenses/portable/zlib-README.txt"
    license_path.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source / "README", license_path)
    manifest_path = output / "share/symbian/portable/zlib.json"
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    manifest_path.write_text(json.dumps(record, indent=2) + "\n")


def build_zlib(workspace: Path, output: Path, compiler: Path) -> None:
    """Builds the same complete zlib source set for both guest ABIs."""
    record = zlib_manifest(workspace)
    source = workspace / "research/upstream" / record["source"]
    for architecture in ("armv5t", "armv6"):
        with tempfile.TemporaryDirectory(
            prefix=f"portable-zlib-{architecture}-"
        ) as directory:
            build = Path(directory) / "build"
            run(
                [
                    "cmake",
                    "-S",
                    str(workspace / "cpp/symbian/portable/zlib"),
                    "-B",
                    str(build),
                    "-G",
                    "Ninja",
                    "-DCMAKE_TOOLCHAIN_FILE="
                    f"{output / 'cmake/symbian-arm.cmake'}",
                    f"-DSYMBIAN_SDK_PREFIX={output}",
                    f"-DSYMBIAN_TARGET_ARCH={architecture}",
                    f"-DCMAKE_C_COMPILER={compiler.parent / 'clang'}",
                    f"-DCMAKE_CXX_COMPILER={compiler}",
                    f"-DSYMBIAN_ZLIB_SOURCE={source}",
                ],
                cwd=workspace,
                timeout=120,
            )
            run(
                ["cmake", "--build", str(build), "-j", "6"],
                cwd=workspace,
                timeout=120,
            )
            archive = build / "libsymbian_portable_zlib.a"
            destination = output / record["archive"].format(
                architecture=architecture
            )
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(archive, destination)


def validate_zlib_payload(prefix: Path) -> None:
    """Rejects missing or mismatched portable payloads after relocation."""
    record = json.loads(
        (prefix / "share/symbian/portable/zlib.json").read_text()
    )
    for name, destination in record["headers"].items():
        path = prefix / destination
        if (
            not path.is_file()
            or hashlib.sha256(path.read_bytes()).hexdigest()
            != record["files"][name]
        ):
            raise ValueError(f"Portable zlib header missing or changed: {path}")
    notice = prefix / "licenses/portable/zlib-README.txt"
    if (
        not notice.is_file()
        or hashlib.sha256(notice.read_bytes()).hexdigest()
        != record["files"]["README"]
    ):
        raise ValueError(f"Portable zlib license missing or changed: {notice}")
    for architecture in ("armv5t", "armv6"):
        archive = prefix / record["archive"].format(architecture=architecture)
        if not archive.is_file() or archive.stat().st_size == 0:
            raise ValueError(f"Portable zlib archive missing: {archive}")
