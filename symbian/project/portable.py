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


def png_manifest(workspace: Path) -> dict:
    """Verifies every reviewed libpng source and its prebuilt configuration."""
    record = json.loads((workspace / "research/portable/png.json").read_text())
    source = workspace / "research/upstream" / record["source"]
    for name, expected in record["files"].items():
        path = source / name
        if (
            not path.is_file()
            or hashlib.sha256(path.read_bytes()).hexdigest() != expected
        ):
            raise ValueError(
                f"Pinned portable libpng input missing or changed: {path}"
            )
    return record


def stage_png_headers(workspace: Path, output: Path) -> None:
    """Stages the exact libpng headers and license matching its archive."""
    record = png_manifest(workspace)
    source = workspace / "research/upstream" / record["source"]
    for name, destination in record["headers"].items():
        path = output / destination
        path.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source / name, path)
    license_path = output / "licenses/portable/libpng-LICENSE.txt"
    license_path.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source / "LICENSE", license_path)
    manifest_path = output / "share/symbian/portable/png.json"
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    manifest_path.write_text(json.dumps(record, indent=2) + "\n")


def build_png(workspace: Path, output: Path, compiler: Path) -> None:
    """Builds both ARM libpng archives against the exported portable zlib."""
    record = png_manifest(workspace)
    source = workspace / "research/upstream" / record["source"]
    for architecture in ("armv5t", "armv6"):
        with tempfile.TemporaryDirectory(
            prefix=f"portable-png-{architecture}-"
        ) as directory:
            build = Path(directory) / "build"
            run(
                [
                    "cmake",
                    "-S",
                    str(workspace / "cpp/symbian/portable/png"),
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
                    f"-DSYMBIAN_PNG_SOURCE={source}",
                ],
                cwd=workspace,
                timeout=120,
            )
            run(
                ["cmake", "--build", str(build), "-j", "6"],
                cwd=workspace,
                timeout=120,
            )
            destination = output / record["archive"].format(
                architecture=architecture
            )
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(build / "libsymbian_portable_png.a", destination)


def validate_png_payload(prefix: Path) -> None:
    """Rejects missing or mismatched installed libpng payloads."""
    record = json.loads(
        (prefix / "share/symbian/portable/png.json").read_text()
    )
    for name, destination in record["headers"].items():
        path = prefix / destination
        if (
            not path.is_file()
            or hashlib.sha256(path.read_bytes()).hexdigest()
            != record["files"][name]
        ):
            raise ValueError(
                f"Portable libpng header missing or changed: {path}"
            )
    notice = prefix / "licenses/portable/libpng-LICENSE.txt"
    if (
        not notice.is_file()
        or hashlib.sha256(notice.read_bytes()).hexdigest()
        != record["files"]["LICENSE"]
    ):
        raise ValueError(
            f"Portable libpng license missing or changed: {notice}"
        )
    for architecture in ("armv5t", "armv6"):
        archive = prefix / record["archive"].format(architecture=architecture)
        if not archive.is_file() or archive.stat().st_size == 0:
            raise ValueError(f"Portable libpng archive missing: {archive}")


def jpeg_manifest(workspace: Path) -> dict:
    """Verifies the pinned IJG source files and original qmake source list."""
    record = json.loads((workspace / "research/portable/jpeg.json").read_text())
    source = workspace / "research/upstream" / record["source"]
    manifest = workspace / "research/upstream" / record["source_manifest"]
    if (
        not manifest.is_file()
        or hashlib.sha256(manifest.read_bytes()).hexdigest()
        != record["manifest_sha256"]
    ):
        raise ValueError(
            f"Pinned libjpeg source manifest missing or changed: {manifest}"
        )
    for name, expected in record["files"].items():
        path = source / name
        if (
            not path.is_file()
            or hashlib.sha256(path.read_bytes()).hexdigest() != expected
        ):
            raise ValueError(f"Pinned libjpeg input missing or changed: {path}")
    return record


def stage_jpeg_headers(workspace: Path, output: Path) -> None:
    """Stages matching IJG public headers, license and provenance."""
    record = jpeg_manifest(workspace)
    source = workspace / "research/upstream" / record["source"]
    for name, destination in record["headers"].items():
        path = output / destination
        path.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source / name, path)
    license_path = output / "licenses/portable/libjpeg-README.txt"
    license_path.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source / "README", license_path)
    manifest_path = output / "share/symbian/portable/jpeg.json"
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    manifest_path.write_text(json.dumps(record, indent=2) + "\n")


def build_jpeg(workspace: Path, output: Path, compiler: Path) -> None:
    """Builds both guest archives from the complete original source list."""
    record = jpeg_manifest(workspace)
    source = workspace / "research/upstream" / record["source"]
    for architecture in ("armv5t", "armv6"):
        with tempfile.TemporaryDirectory(
            prefix=f"portable-jpeg-{architecture}-"
        ) as directory:
            build = Path(directory) / "build"
            run(
                [
                    "cmake",
                    "-S",
                    str(workspace / "cpp/symbian/portable/jpeg"),
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
                    f"-DSYMBIAN_JPEG_SOURCE={source}",
                ],
                cwd=workspace,
                timeout=120,
            )
            run(
                ["cmake", "--build", str(build), "-j", "6"],
                cwd=workspace,
                timeout=120,
            )
            destination = output / record["archive"].format(
                architecture=architecture
            )
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(build / "libsymbian_portable_jpeg.a", destination)


def validate_jpeg_payload(prefix: Path) -> None:
    """Rejects missing or mismatched installed IJG libjpeg payloads."""
    record = json.loads(
        (prefix / "share/symbian/portable/jpeg.json").read_text()
    )
    for name, destination in record["headers"].items():
        path = prefix / destination
        if (
            not path.is_file()
            or hashlib.sha256(path.read_bytes()).hexdigest()
            != record["files"][name]
        ):
            raise ValueError(
                f"Portable libjpeg header missing or changed: {path}"
            )
    notice = prefix / "licenses/portable/libjpeg-README.txt"
    if (
        not notice.is_file()
        or hashlib.sha256(notice.read_bytes()).hexdigest()
        != record["files"]["README"]
    ):
        raise ValueError(
            f"Portable libjpeg license missing or changed: {notice}"
        )
    for architecture in ("armv5t", "armv6"):
        archive = prefix / record["archive"].format(architecture=architecture)
        if not archive.is_file() or archive.stat().st_size == 0:
            raise ValueError(f"Portable libjpeg archive missing: {archive}")


def freetype_manifest(workspace: Path) -> dict:
    """Verifies the pinned FreeType source, headers and license inputs."""
    record = json.loads(
        (workspace / "research/portable/freetype.json").read_text()
    )
    source = workspace / "research/upstream" / record["source"]
    for name, expected in record["files"].items():
        path = source / name
        if (
            not path.is_file()
            or hashlib.sha256(path.read_bytes()).hexdigest() != expected
        ):
            raise ValueError(
                f"Pinned FreeType input missing or changed: {path}"
            )
    return record


def stage_freetype_headers(workspace: Path, output: Path) -> None:
    """Stages the exact public layout and original dual-license notices."""
    record = freetype_manifest(workspace)
    source = workspace / "research/upstream" / record["source"]
    for name, destination in record["headers"].items():
        path = output / destination
        path.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source / "include" / name, path)
    for name, destination in record["licenses"].items():
        path = output / destination
        path.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source / name, path)
    manifest_path = output / "share/symbian/portable/freetype.json"
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    manifest_path.write_text(json.dumps(record, indent=2) + "\n")


def build_freetype(workspace: Path, output: Path, compiler: Path) -> None:
    """Builds FreeType for both guest ABIs with external codecs disabled."""
    record = freetype_manifest(workspace)
    source = workspace / "research/upstream" / record["source"]
    for architecture in ("armv5t", "armv6"):
        with tempfile.TemporaryDirectory(
            prefix=f"portable-freetype-{architecture}-"
        ) as directory:
            build = Path(directory) / "build"
            run(
                [
                    "cmake",
                    "-S",
                    str(workspace / "cpp/symbian/portable/freetype"),
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
                    f"-DSYMBIAN_FREETYPE_SOURCE={source}",
                ],
                cwd=workspace,
                timeout=120,
            )
            run(
                ["cmake", "--build", str(build), "-j", "6"],
                cwd=workspace,
                timeout=120,
            )
            destination = output / record["archive"].format(
                architecture=architecture
            )
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(build / "upstream/libfreetype.a", destination)


def validate_freetype_payload(prefix: Path) -> None:
    """Rejects missing or changed public FreeType headers and archives."""
    record = json.loads(
        (prefix / "share/symbian/portable/freetype.json").read_text()
    )
    for name, destination in record["headers"].items():
        path = prefix / destination
        if (
            not path.is_file()
            or hashlib.sha256(path.read_bytes()).hexdigest()
            != record["files"][f"include/{name}"]
        ):
            raise ValueError(
                f"Portable FreeType header missing or changed: {path}"
            )
    for name, destination in record["licenses"].items():
        path = prefix / destination
        if (
            not path.is_file()
            or hashlib.sha256(path.read_bytes()).hexdigest()
            != record["files"][name]
        ):
            raise ValueError(
                f"Portable FreeType license missing or changed: {path}"
            )
    for architecture in ("armv5t", "armv6"):
        archive = prefix / record["archive"].format(architecture=architecture)
        if not archive.is_file() or archive.stat().st_size == 0:
            raise ValueError(f"Portable FreeType archive missing: {archive}")
