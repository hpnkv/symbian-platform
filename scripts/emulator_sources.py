"""Packages corresponding emulator sources and required runtime library sources.

Source payloads contain build material and licenses, not provenance inventories.
Qt is fetched once and shared by all native host bundles.
"""

import argparse
import ctypes
import hashlib
import os
import re
import shutil
import subprocess
import tarfile
import tempfile
from pathlib import Path

from build_emulator import QT_VERSION, ROOT, SDL_VERSION, acquire, host, run

QT_SHA256 = "cdd3a69967208276bb01af7ace7dba0ba53e679f886a4cbe624225c60fb73f2c"


def qt(workspace):
    """Fetches the exact corresponding Qt source and its original notices."""
    archive = workspace / f"qt-everywhere-src-{QT_VERSION}.tar.xz"
    if not archive.is_file():
        run(
            [
                "curl",
                "-fL",
                "--retry",
                "3",
                f"https://download.qt.io/archive/qt/6.8/{QT_VERSION}/single/{archive.name}",
                "-o",
                archive,
            ]
        )
    with archive.open("rb") as stream:
        if hashlib.file_digest(stream, "sha256").hexdigest() != QT_SHA256:
            raise RuntimeError(
                "Qt source archive differs from its pinned digest"
            )
    notices = workspace / "qt-notices"
    notices.mkdir(exist_ok=True)
    with tarfile.open(archive) as bundle:
        members = [
            member
            for member in bundle
            if member.isfile()
            and (
                re.match(
                    r"(LICENSE|COPYING|NOTICE|COPYRIGHT)",
                    Path(member.name).name,
                    re.I,
                )
                or "LICENSES" in Path(member.name).parts
            )
        ]
        bundle.extractall(notices, members=members, filter="data")
    return archive


def add_source(bundle, source, prefix):
    """Exports patched files and recursively pinned submodule sources."""
    tracked = (
        subprocess.check_output(
            ["git", "-C", str(source), "ls-files", "--recurse-submodules", "-z"]
        )
        .decode()
        .split("\0")
    )
    for name in sorted(filter(None, tracked)):
        path = source / name
        # Upstream's prepared host SDL binaries are replaced by our source
        # build. Keep guest patch build inputs and their maintained binaries.
        if name.startswith("src/external/sdl2/"):
            continue
        if path.is_file() or path.is_symlink():
            bundle.add(path, arcname=str(Path(prefix) / name), recursive=False)


def snapshot(workspace, output):
    """Exports the tested core and adapter/dependency build inputs once."""
    source = acquire(workspace, host()[0])
    with tarfile.open(output, "w:gz") as bundle:
        add_source(bundle, source, "source")
        for name in ("abseil", "nlohmann_json", "libuv"):
            dependency = workspace / "build/_deps" / (name + "-src")
            if not (dependency / "CMakeLists.txt").is_file():
                raise RuntimeError(
                    f"Missing configured source dependency: {dependency}"
                )
            bundle.add(
                dependency, arcname=f"dependencies/{name}", filter=source_filter
            )
        bundle.add(
            workspace / f"SDL2-{SDL_VERSION}.tar.gz",
            arcname=f"dependencies/SDL2-{SDL_VERSION}.tar.gz",
        )
        for name in ("cpp/symbian/emulator", "research/eka2l1", "scripts"):
            for path in (ROOT / name).rglob("*"):
                if path.is_file() and "__pycache__" not in path.parts:
                    bundle.add(
                        path,
                        arcname=str(Path("platform") / path.relative_to(ROOT)),
                        recursive=False,
                    )
        bundle.add(ROOT / "LICENSE", arcname="platform/LICENSE")


def source_filter(member):
    """Omits Git databases and generated Python/build products."""
    if any(
        part in {".git", "__pycache__", "build"}
        for part in Path(member.name).parts
    ):
        return None
    return member


def runtime_sources(prefix, destination):
    """Collects package source and rebuild recipes for runtime libraries."""
    destination.mkdir()
    packages = set()
    for path in (prefix / "licenses/tools").iterdir():
        if os.uname().sysname == "Darwin":
            package = path.name
            formula = (
                Path(
                    subprocess.check_output(
                        ["brew", "--prefix", package], text=True
                    ).strip()
                )
                / ".brew"
                / (package + ".rb")
            )
            # The installed keg's recipe describes its actual sources. The
            # live Homebrew catalog may already describe a newer version.
            sources = re.findall(
                r'(?m)^[ \t]*url "([^"\n]+)"[^\n]*\n'
                r'(?:(?![ \t]*(?:url |sha256 ))[^\n]*\n)*'
                r'[ \t]*sha256 "([a-f0-9]{64})"',
                formula.read_text(),
            )
            if not sources or any("#{" in url for url, _ in sources):
                raise RuntimeError(
                    f"Cannot resolve installed source recipe: {formula}"
                )
            target = destination / package
            target.mkdir()
            shutil.copy(formula, target / formula.name)
            for index, (url, checksum) in enumerate(sources):
                archive = target / f"{index}-{Path(url).name}"
                run(["curl", "-fL", "--retry", "3", url, "-o", archive])
                with archive.open("rb") as stream:
                    if (
                        hashlib.file_digest(stream, "sha256").hexdigest()
                        != checksum
                    ):
                        raise RuntimeError(
                            f"Installed source checksum differs: {archive}"
                        )
        else:
            result = subprocess.check_output(
                [
                    "dpkg-query",
                    "-W",
                    "-f=${source:Package} ${source:Version}",
                    path.name,
                ],
                text=True,
            ).split()
            packages.add("=".join(result))
    for package in sorted(packages):
        run(["apt-get", "source", "--download-only", package], cwd=destination)


def qt_icu(workspace, qt_prefix):
    """Supplies ICU source/notices for Qt's shipped Linux runtime."""
    candidates = sorted((qt_prefix / "lib").glob("libicuuc.so.*"))
    if not candidates:
        return
    library = candidates[0].resolve()
    major = library.name.split(".so.")[1].split(".")[0]
    runtime = ctypes.CDLL(str(library))
    version = (ctypes.c_uint8 * 4)()
    getattr(runtime, f"u_getVersion_{major}")(version)
    number = f"{version[0]}-{version[1]}"
    archive = workspace / f"icu4c-{version[0]}_{version[1]}-src.tgz"
    if not archive.is_file():
        run(
            [
                "curl",
                "-fL",
                "--retry",
                "3",
                f"https://github.com/unicode-org/icu/releases/download/release-{number}/{archive.name}",
                "-o",
                archive,
            ]
        )
    with tarfile.open(archive) as source:
        source.extractall(workspace / "qt-icu-source", filter="data")


def complete(snapshot_path, workspace, output):
    """Combines the common core snapshot with exact Qt corresponding source."""
    archive = qt(workspace)
    with tempfile.TemporaryDirectory(prefix="symbian-emulator-source-") as temp:
        root = Path(temp)
        with tarfile.open(snapshot_path) as bundle:
            bundle.extractall(root, filter="data")
        shutil.copy(archive, root / archive.name)
        (root / "BUILD.md").write_text(
            "# Rebuild the emulator\n\n"
            "Install Clang, CMake, Ninja, pkg-config and the platform "
            "development "
            "libraries listed in the emulator source guide. Extract the "
            "included "
            "Qt source archive and build Qt base, Svg, Tools, translations and "
            "Wayland (Linux) into a native prefix with Qt's LGPL/open-source "
            "configure option. The upstream configure and module licenses are "
            "included in that archive.\n\n"
            "```sh\n"
            "tar -xf qt-everywhere-src-6.8.3.tar.xz\n"
            "mkdir qt-build\ncd qt-build\n"
            "../qt-everywhere-src-6.8.3/configure -opensource "
            "-confirm-license -prefix /path/to/qt-prefix "
            "-nomake tests -nomake examples "
            "-submodules qtbase,qtsvg,qttools,qttranslations,qtwayland\n"
            "cmake --build . --parallel\ncmake --install .\ncd ..\n```\n\n"
            "Run from this source root, selecting that Qt prefix:\n\n"
            "```sh\npython3 platform/scripts/build_emulator.py all "
            "--workspace build-emulator --source-tree source "
            "--dependency-sources dependencies --qt-prefix "
            "/path/to/qt-prefix\n```\n\n"
            "All emulator, FFmpeg, SDL, Abseil, JSON and libuv inputs are "
            "included; "
            "this route disables CMake dependency downloads. Source builds "
            "retain "
            "the upstream native tests and guest patch inputs. The separate "
            "host "
            "runtime-source archives contain the exact redistribution sources "
            "and "
            "package rebuild recipes for bundled shared libraries. Firmware is "
            "supplied separately by the user.\n"
        )
        with tarfile.open(output, "w:gz") as bundle:
            bundle.add(root, arcname=".")


def main():
    """Collects shared or per-host source artifacts for independent releases."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "stage", choices=("qt", "icu", "snapshot", "runtime", "complete")
    )
    parser.add_argument("--workspace", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--prefix", type=Path)
    parser.add_argument("--snapshot", type=Path)
    parser.add_argument("--qt-prefix", type=Path)
    args = parser.parse_args()
    workspace = args.workspace.resolve()
    workspace.mkdir(parents=True, exist_ok=True)
    if args.stage == "qt":
        qt(workspace)
    elif args.stage == "icu":
        qt_icu(workspace, args.qt_prefix)
    elif args.stage == "snapshot":
        snapshot(workspace, args.output)
    elif args.stage == "runtime":
        runtime_sources(args.prefix, args.output)
    else:
        complete(args.snapshot, workspace, args.output)


if __name__ == "__main__":
    main()
