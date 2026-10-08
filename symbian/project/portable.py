"""Pinned, source-built portable guest libraries."""

import hashlib
import json
import shutil
import tempfile
from pathlib import Path

from symbian.process import run


def sdl3_manifest(workspace: Path) -> dict:
    """Verifies the pinned SDL3 release and public C headers."""
    record = json.loads((workspace / "research/portable/sdl3.json").read_text())
    source = workspace / "research/upstream" / record["source"]
    archive = workspace / "research/upstream/sdl3/SDL3-3.2.22.tar.gz"
    if (
        hashlib.sha256(archive.read_bytes()).hexdigest()
        != record["archive_sha256"]
    ):
        raise ValueError(f"Pinned SDL3 archive changed: {archive}")
    if hashlib.sha256((source / "LICENSE.txt").read_bytes()).hexdigest() != (
        record["license_sha256"]
    ):
        raise ValueError("Pinned SDL3 license changed")
    for name, expected in record["headers"].items():
        path = source / "include" / name
        if hashlib.sha256(path.read_bytes()).hexdigest() != expected:
            raise ValueError(f"Pinned SDL3 header changed: {path}")
    return record


def stage_sdl3_headers(workspace: Path, output: Path) -> None:
    """Stages SDL3 C and C++ APIs with their pinned license."""
    record = sdl3_manifest(workspace)
    record["target_config_sha256"] = hashlib.sha256(
        (
            workspace / "cpp/symbian/portable/sdl3/SDL_build_config.h"
        ).read_bytes()
    ).hexdigest()
    source = workspace / "research/upstream" / record["source"]
    destination = output / "include/portable/sdl3"
    for name in record["headers"]:
        path = destination / name
        path.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source / "include" / name, path)
    shutil.copyfile(
        workspace / "cpp/symbian/portable/sdl3/SDL_build_config.h",
        destination / "SDL3/SDL_build_config.h",
    )
    cxx = output / "include/symbian/sdl3"
    cxx.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(
        workspace / "cpp/symbian/portable/sdl3/include/symbian/sdl3/sdl3.h",
        cxx / "sdl3.h",
    )
    notice = output / "licenses/portable/SDL3-LICENSE.txt"
    notice.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source / "LICENSE.txt", notice)
    manifest = output / "share/symbian/portable/sdl3.json"
    manifest.parent.mkdir(parents=True, exist_ok=True)
    manifest.write_text(json.dumps(record, indent=2) + "\n")


def build_sdl3(workspace: Path, output: Path, compiler: Path) -> None:
    """Builds SDL3 for both guest architectures."""
    record = sdl3_manifest(workspace)
    source = workspace / "research/upstream" / record["source"]
    for architecture in ("armv5t", "armv6"):
        with tempfile.TemporaryDirectory(
            prefix=f"portable-sdl3-{architecture}-"
        ) as directory:
            build = Path(directory) / "build"
            run(
                [
                    "cmake",
                    "-S",
                    str(workspace / "cpp/symbian/portable/sdl3"),
                    "-B",
                    str(build),
                    "-G",
                    "Ninja",
                    f"-DSYMBIAN_SDL3_SOURCE={source}",
                    "-DSYMBIAN_SDK_BUILDING_SDL3=ON",
                    "-DSYMBIAN_SDK_BUILDING_SDL2=ON",
                    f"-DSYMBIAN_SDK_PREFIX={output}",
                    f"-DSYMBIAN_TARGET_ARCH={architecture}",
                    "-DCMAKE_TOOLCHAIN_FILE="
                    f"{output / 'cmake/symbian-arm.cmake'}",
                    f"-DCMAKE_C_COMPILER={compiler.parent / 'clang'}",
                    f"-DCMAKE_CXX_COMPILER={compiler}",
                    f"-DCMAKE_AR={compiler.parent / 'llvm-ar'}",
                    f"-DCMAKE_RANLIB={compiler.parent / 'llvm-ranlib'}",
                    "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
                ],
                cwd=workspace,
                timeout=120,
            )
            run(
                [
                    "cmake",
                    "--build",
                    str(build),
                    "--target",
                    "symbian_portable_sdl3",
                    "symbian_portable_sdl3_gpu",
                    "-j",
                    "6",
                ],
                cwd=workspace,
                timeout=240,
            )
            destination = output / record["archive"].format(
                architecture=architecture
            )
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(build / "libsymbian_portable_sdl3.a", destination)
            shutil.copyfile(
                build / "libsymbian_portable_sdl3_gpu.a",
                destination.with_name("libsymbian_portable_sdl3_gpu.a"),
            )
            for archive in (
                destination,
                destination.with_name("libsymbian_portable_sdl3_gpu.a"),
            ):
                members = run(
                    [str(compiler.parent / "llvm-ar"), "t", str(archive)],
                    cwd=workspace,
                ).splitlines()
                if "SDL.c.obj" not in members:
                    raise ValueError(
                        f"SDL3 archive lacks its core object: {archive}"
                    )


def validate_sdl3_payload(prefix: Path) -> None:
    """Rejects incomplete or changed SDL3 SDK payloads."""
    record = json.loads(
        (prefix / "share/symbian/portable/sdl3.json").read_text()
    )
    for name, expected in record["headers"].items():
        path = prefix / "include/portable/sdl3" / name
        if hashlib.sha256(path.read_bytes()).hexdigest() != expected:
            raise ValueError(f"Installed SDL3 header changed: {path}")
    config = prefix / "include/portable/sdl3/SDL3/SDL_build_config.h"
    if (
        hashlib.sha256(config.read_bytes()).hexdigest()
        != record["target_config_sha256"]
    ):
        raise ValueError(f"Installed SDL3 config changed: {config}")
    notice = prefix / "licenses/portable/SDL3-LICENSE.txt"
    if (
        hashlib.sha256(notice.read_bytes()).hexdigest()
        != record["license_sha256"]
    ):
        raise ValueError("Installed SDL3 license changed")
    for path in (
        prefix / "include/portable/sdl3/SDL3/SDL_build_config.h",
        prefix / "include/symbian/sdl3/sdl3.h",
    ):
        if not path.is_file():
            raise ValueError(f"Installed SDL3 API missing: {path}")
    for architecture in ("armv5t", "armv6"):
        archive = prefix / record["archive"].format(architecture=architecture)
        if not archive.is_file() or archive.stat().st_size < 1024:
            raise ValueError(f"Installed SDL3 archive missing: {archive}")
        gpu_archive = archive.with_name("libsymbian_portable_sdl3_gpu.a")
        if not gpu_archive.is_file() or gpu_archive.stat().st_size < 1024:
            raise ValueError(
                f"Installed SDL3 GPU archive missing: {gpu_archive}"
            )


def sdl2_manifest(workspace: Path) -> dict:
    """Verifies the pinned SDL2 release and every exported C header."""
    record = json.loads((workspace / "research/portable/sdl2.json").read_text())
    source = workspace / "research/upstream" / record["source"]
    archive = workspace / "research/upstream/sdl2/SDL2-2.30.11.tar.gz"
    if (
        not archive.is_file()
        or hashlib.sha256(archive.read_bytes()).hexdigest()
        != record["archive_sha256"]
    ):
        raise ValueError(f"Pinned SDL2 release missing or changed: {archive}")
    if hashlib.sha256((source / "LICENSE.txt").read_bytes()).hexdigest() != (
        record["license_sha256"]
    ):
        raise ValueError("Pinned SDL2 license changed")
    for name, expected in record["headers"].items():
        path = source / "include" / name
        if hashlib.sha256(path.read_bytes()).hexdigest() != expected:
            raise ValueError(f"Pinned SDL2 header changed: {path}")
    return record


def stage_sdl2_headers(workspace: Path, output: Path) -> None:
    """Stages the SDL2 C API, C++ owner and upstream license."""
    record = sdl2_manifest(workspace)
    record["target_config_sha256"] = hashlib.sha256(
        (workspace / "cpp/symbian/portable/sdl2/SDL_config.h").read_bytes()
    ).hexdigest()
    source = workspace / "research/upstream" / record["source"]
    destination = output / "include/portable/sdl2"
    destination.mkdir(parents=True, exist_ok=True)
    for name in record["headers"]:
        shutil.copyfile(source / "include" / name, destination / name)
    shutil.copyfile(
        workspace / "cpp/symbian/portable/sdl2/SDL_config.h",
        destination / "SDL_config.h",
    )
    cxx = output / "include/symbian/sdl2"
    cxx.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(
        workspace / "cpp/symbian/portable/sdl2/include/symbian/sdl2/sdl2.h",
        cxx / "sdl2.h",
    )
    license_path = output / "licenses/portable/SDL2-LICENSE.txt"
    license_path.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source / "LICENSE.txt", license_path)
    manifest = output / "share/symbian/portable/sdl2.json"
    manifest.parent.mkdir(parents=True, exist_ok=True)
    manifest.write_text(json.dumps(record, indent=2) + "\n")


def build_sdl2(workspace: Path, output: Path, compiler: Path) -> None:
    """Builds the owned SDL2 port for both guest architectures."""
    record = sdl2_manifest(workspace)
    source = workspace / "research/upstream" / record["source"]
    for architecture in ("armv5t", "armv6"):
        with tempfile.TemporaryDirectory(
            prefix=f"portable-sdl2-{architecture}-"
        ) as directory:
            build = Path(directory) / "build"
            run(
                [
                    "cmake",
                    "-S",
                    str(workspace / "cpp/symbian/portable/sdl2"),
                    "-B",
                    str(build),
                    "-G",
                    "Ninja",
                    f"-DSYMBIAN_SDL2_SOURCE={source}",
                    "-DSYMBIAN_SDK_BUILDING_SDL2=ON",
                    "-DSYMBIAN_SDK_BUILDING_SDL3=ON",
                    f"-DSYMBIAN_SDK_PREFIX={output}",
                    f"-DSYMBIAN_TARGET_ARCH={architecture}",
                    "-DCMAKE_TOOLCHAIN_FILE="
                    f"{output / 'cmake/symbian-arm.cmake'}",
                    f"-DCMAKE_C_COMPILER={compiler.parent / 'clang'}",
                    f"-DCMAKE_CXX_COMPILER={compiler}",
                    f"-DCMAKE_AR={compiler.parent / 'llvm-ar'}",
                    f"-DCMAKE_RANLIB={compiler.parent / 'llvm-ranlib'}",
                    "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
                ],
                cwd=workspace,
                timeout=120,
            )
            run(
                [
                    "cmake",
                    "--build",
                    str(build),
                    "--target",
                    "symbian_portable_sdl2",
                    "symbian_portable_sdl2_gpu",
                    "-j",
                    "6",
                ],
                cwd=workspace,
                timeout=240,
            )
            destination = output / record["archive"].format(
                architecture=architecture
            )
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(build / "libsymbian_portable_sdl2.a", destination)
            shutil.copyfile(
                build / "libsymbian_portable_sdl2_gpu.a",
                destination.with_name("libsymbian_portable_sdl2_gpu.a"),
            )
            for archive in (
                destination,
                destination.with_name("libsymbian_portable_sdl2_gpu.a"),
            ):
                members = run(
                    [str(compiler.parent / "llvm-ar"), "t", str(archive)],
                    cwd=workspace,
                ).splitlines()
                if "SDL.c.obj" not in members:
                    raise ValueError(
                        f"SDL2 archive lacks its core object: {archive}"
                    )


def validate_sdl2_payload(prefix: Path) -> None:
    """Rejects incomplete SDL2 C and C++ SDK payloads."""
    record = json.loads(
        (prefix / "share/symbian/portable/sdl2.json").read_text()
    )
    for name, expected in record["headers"].items():
        path = prefix / "include/portable/sdl2" / name
        if name == "SDL_config.h":
            # The SDK's target configuration replaces the upstream template.
            if (
                hashlib.sha256(path.read_bytes()).hexdigest()
                != record["target_config_sha256"]
            ):
                raise ValueError(f"Installed SDL2 config changed: {path}")
            continue
        if hashlib.sha256(path.read_bytes()).hexdigest() != expected:
            raise ValueError(f"Installed SDL2 header changed: {path}")
    license_path = prefix / "licenses/portable/SDL2-LICENSE.txt"
    if hashlib.sha256(license_path.read_bytes()).hexdigest() != (
        record["license_sha256"]
    ):
        raise ValueError("Installed SDL2 license changed")
    for path in (
        prefix / "include/portable/sdl2/SDL_config.h",
        prefix / "include/symbian/sdl2/sdl2.h",
    ):
        if not path.is_file():
            raise ValueError(f"Installed SDL2 API missing: {path}")
    for architecture in ("armv5t", "armv6"):
        archive = prefix / record["archive"].format(architecture=architecture)
        if not archive.is_file() or archive.stat().st_size == 0:
            raise ValueError(f"Installed SDL2 archive missing: {archive}")
        gpu_archive = archive.with_name("libsymbian_portable_sdl2_gpu.a")
        if not gpu_archive.is_file() or gpu_archive.stat().st_size == 0:
            raise ValueError(
                f"Installed SDL2 GPU archive missing: {gpu_archive}"
            )


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
