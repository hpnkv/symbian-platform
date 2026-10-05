"""Deploys the external GPL frontend, Qt plugins and non-system libraries."""

import argparse
import json
import os
import platform
import plistlib
import re
import shutil
import tarfile
from pathlib import Path

from build_emulator import QT_VERSION
from bundle_native_sdk import Closure, run

ROOT = Path(__file__).resolve().parents[1]
# Leave the OS loader, glibc and driver dispatch interfaces to the host.
# Bundle Qt's XCB/Wayland/font/audio libraries, never a runner's GPU driver.
SYSTEM = re.compile(
    r"^(ld-linux.*|lib(c|m|dl|pthread|rt|resolv|util)\.so\..*|"
    r"lib(GL|GLX|GLdispatch|EGL|OpenGL|vulkan)\.so\..*)$"
)
PLUGINS = (
    "platforms",
    "platformthemes",
    "styles",
    "imageformats",
    "iconengines",
    "tls",
    "xcbglintegrations",
    "wayland-graphics-integration-client",
    "wayland-shell-integration",
    "wayland-decoration-client",
)


def macho(path):
    """Identifies Mach-O code without loading entire binary resources."""
    if not path.is_file() or path.is_symlink():
        return False
    with path.open("rb") as stream:
        return stream.read(4) in (
            b"\xcf\xfa\xed\xfe",
            b"\xce\xfa\xed\xfe",
            b"\xca\xfe\xba\xbe",
            b"\xca\xfe\xba\xbf",
        )


def mac_relocate(app):
    """Makes deployed framework references independent of the build prefix."""
    frameworks = app / "Contents/Frameworks"
    for path in app.rglob("*"):
        if not macho(path):
            continue
        own_ids = run("otool", "-D", str(path)).splitlines()[1:]
        for line in run("otool", "-L", str(path)).splitlines()[1:]:
            name = line.strip().split(" (compatibility", 1)[0]
            if name in own_ids:
                if path.is_relative_to(frameworks):
                    relative = path.relative_to(frameworks)
                    run(
                        "install_name_tool",
                        "-id",
                        f"@rpath/{relative}",
                        str(path),
                    )
                continue
            if name.startswith(("/usr/lib/", "/System/Library/")):
                continue
            if ".framework/" in name:
                framework = name.split(".framework/", 1)
                relative = (
                    Path(framework[0]).name + ".framework/" + framework[1]
                )
                target = frameworks / relative
            else:
                target = frameworks / Path(name).name
            if not target.is_file():
                raise RuntimeError(
                    f"Missing deployed dependency {name} of {path}"
                )
            relative = os.path.relpath(target, path.parent)
            run(
                "install_name_tool",
                "-change",
                name,
                f"@loader_path/{relative}",
                str(path),
            )


def notices(source, destination):
    """Copies original redistribution notices, retaining their source paths."""
    for directory, folders, files in os.walk(source):
        folders[:] = [name for name in folders if name not in {".git", "build"}]
        parent = Path(directory)
        for name in files:
            if (
                re.match(
                    r"(LICENSE|COPYING|NOTICE|COPYRIGHT)(\b|[._-])", name, re.I
                )
                or "LICENSES" in parent.parts
            ):
                path = parent / name
                if path.is_symlink():
                    continue
                target = destination / path.relative_to(source)
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy(path, target)


def mac_bundle(args, output):
    """Deploys a normal app plus the independent firmware-import executable."""
    app = output / "EKA2L1.app"
    shutil.copytree(args.workspace / "build/bin/EKA2L1.app", app, symlinks=True)
    for name in ("resources", "patch", "scripts", "compat", "panic.json"):
        original = app / "Contents/MacOS" / name
        if original.exists():
            destination = app / "Contents/Resources" / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            original.rename(destination)
    sdl = app / "Contents/Frameworks/SDL2.framework"
    if sdl.is_dir() and "SDL2.framework" not in run(
        "otool", "-L", str(app / "Contents/MacOS/EKA2L1")
    ):
        shutil.rmtree(sdl)
    deploy = args.qt_prefix / "bin/macdeployqt"
    run(str(deploy), str(app), "-always-overwrite")
    mac_relocate(app)
    closure = Closure(output)
    closure.copy(
        args.workspace / "build/platform-control/symbian_firmware_tool",
        output / "bin/symbian_firmware_tool",
    )
    closure.finish()
    # Qt deployment does not copy licenses for prebundled system-package libs.
    # Reinspect their original link inputs so Closure retains those notices.
    original = args.workspace / "build/bin/EKA2L1.app/Contents/Frameworks"
    brew = shutil.which("brew")
    brew_lib = Path(run(brew, "--prefix")) / "lib" if brew else None
    for library in original.rglob("*"):
        if macho(library):
            # Upstream fixup may already replace a library's own install name.
            # Its UUID survives relocation/signing and identifies the actual
            # installed keg. Do not choose a merely matching library filename.
            candidate = brew_lib / library.name if brew_lib else None
            own_ids = run("otool", "-D", str(library)).splitlines()[1:]
            for name in own_ids:
                if name.startswith("/opt/homebrew/") and Path(name).is_file():
                    candidate = Path(name)
            qt_candidate = (
                args.qt_prefix / "lib" / library.relative_to(original)
            )
            if qt_candidate.is_file():
                candidate = qt_candidate
            if candidate is not None and candidate.is_file():

                def uuids(path):
                    return set(
                        re.findall(
                            r"UUID: ([0-9A-F-]+) \(([^)]+)\)",
                            run("dwarfdump", "--uuid", str(path)),
                        )
                    )

                if uuids(library) and uuids(library) == uuids(candidate):
                    # Qt's notices and corresponding sources are supplied
                    # separately; its Homebrew keg omits root license files.
                    if not any(
                        part.startswith("Qt") and part.endswith(".framework")
                        for part in candidate.parts
                    ):
                        closure.copy_notices(candidate)
                    for _, dependency in closure.mac_dependencies(candidate):
                        if str(dependency).startswith(
                            "/opt/homebrew/"
                        ) and not any(
                            part.startswith("Qt")
                            and part.endswith(".framework")
                            for part in dependency.parts
                        ):
                            closure.copy_notices(dependency)
    closure.finish()
    plist = app / "Contents/Info.plist"
    value = plistlib.loads(plist.read_bytes())
    value.update(
        CFBundleIdentifier="io.github.hpnkv.symbian.emulator",
        CFBundleVersion=args.version,
        CFBundleShortVersionString=args.version,
    )
    plist.write_bytes(plistlib.dumps(value))
    # Sign nested code after deployment. This initial archive is ad hoc signed;
    # Developer ID/notarization is a separate protected publication option.
    code = [path for path in app.rglob("*") if macho(path)]
    for path in sorted(code, key=lambda file: len(file.parts), reverse=True):
        if path == app / "Contents/MacOS/EKA2L1":
            continue
        run("codesign", "--force", "--sign", "-", str(path))
    for framework in (app / "Contents/Frameworks").glob("*.framework"):
        run("codesign", "--force", "--sign", "-", str(framework))
    run("codesign", "--force", "--sign", "-", str(app))
    run("codesign", "--verify", "--deep", "--strict", str(app))
    return "EKA2L1.app/Contents/MacOS/EKA2L1"


def linux_bundle(args, output):
    """Copies the complete ELF/plugin closure into a relocatable prefix."""
    closure = Closure(output, system_libraries=SYSTEM)
    closure.copy(
        args.workspace / "build/bin/eka2l1_qt", output / "bin/eka2l1_qt"
    )
    closure.copy(
        args.workspace / "build/platform-control/symbian_firmware_tool",
        output / "bin/symbian_firmware_tool",
    )
    for name in ("resources", "patch", "scripts", "compat"):
        shutil.copytree(
            args.workspace / "build/bin" / name, output / "bin" / name
        )
    plugin_root = args.qt_prefix / "plugins"
    if not plugin_root.is_dir():
        plugin_root = Path(
            run(
                str(args.qt_prefix / "lib/qt6/bin/qtpaths"),
                "--query",
                "QT_INSTALL_PLUGINS",
            )
        )
    for group in PLUGINS:
        directory = plugin_root / group
        if directory.is_dir():
            for plugin in directory.glob("*.so"):
                closure.copy(plugin, output / "plugins" / group / plugin.name)
    if not (output / "plugins/platforms/libqxcb.so").is_file():
        raise RuntimeError("Qt XCB platform plugin is missing")
    # Qt's OpenSSL backend loads these with dlopen, so ldd cannot find them.
    catalog = run("/sbin/ldconfig", "-p")
    for name in ("libssl.so.3", "libcrypto.so.3"):
        match = re.search(
            r"^\s*" + re.escape(name) + r" .* => (\S+)$", catalog, re.M
        )
        if match is None:
            raise RuntimeError(f"Required Qt TLS runtime is missing: {name}")
        destination = closure.libraries / name
        if not destination.exists():
            closure.copy(Path(match[1]), destination)
    closure.finish()
    (output / "bin/qt.conf").write_text(
        "[Paths]\nPrefix=..\nPlugins=plugins\nTranslations=translations\n"
    )
    translations = args.qt_prefix / "translations"
    if translations.is_dir():
        shutil.copytree(translations, output / "translations")
    launcher = output / "bin/eka2l1"
    launcher.write_text(
        "#!/bin/sh\nset -eu\n"
        'emulator_bin=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)\n'
        'exec "$emulator_bin/eka2l1_qt" "$@"\n'
    )
    launcher.chmod(0o755)
    desktop = output / "share/applications"
    desktop.mkdir(parents=True)
    (desktop / "symbian-emulator.desktop").write_text(
        "[Desktop Entry]\nType=Application\nName=Symbian Emulator\n"
        "Comment=Run Symbian applications with EKA2L1\nExec=eka2l1\n"
        "Icon=symbian-emulator\nCategories=Development;Emulator;\nTerminal=false\n"
    )
    icon = output / "share/icons/hicolor/256x256/apps"
    icon.mkdir(parents=True)
    shutil.copy(
        args.workspace / "source/src/emu/qt/assets/duck_tank.png",
        icon / "symbian-emulator.png",
    )
    return "bin/eka2l1_qt"


def audit(output, system):
    """Rejects unresolved runtime libraries and external bundle symlinks."""
    for path in output.rglob("*"):
        if path.is_symlink() and not path.resolve().is_relative_to(
            output.resolve()
        ):
            raise RuntimeError(f"External bundle symlink: {path}")
        if not path.is_file() or path.is_symlink():
            continue
        with path.open("rb") as stream:
            magic = stream.read(4)
        if system == "linux" and magic == b"\x7fELF":
            machine = (
                "AArch64"
                if platform.machine() in {"aarch64", "arm64"}
                else "Advanced Micro Devices X86-64"
            )
            if machine not in run("readelf", "-h", str(path)):
                raise RuntimeError(f"Wrong runtime architecture: {path}")
            dependencies = run("ldd", str(path))
            if "not found" in dependencies:
                raise RuntimeError(
                    f"Missing runtime dependency for {path}: {dependencies}"
                )
            for line in dependencies.splitlines():
                match = re.match(r"\s*(\S+) => (/.+?) \(", line)
                if (
                    match
                    and not SYSTEM.match(match[1])
                    and not Path(match[2])
                    .resolve()
                    .is_relative_to(output.resolve())
                ):
                    raise RuntimeError(
                        f"Unbundled dependency for {path}: {line}"
                    )
        elif system == "macos" and magic in (
            b"\xcf\xfa\xed\xfe",
            b"\xce\xfa\xed\xfe",
            b"\xca\xfe\xba\xbe",
            b"\xca\xfe\xba\xbf",
        ):
            run("lipo", str(path), "-verify_arch", platform.machine())
            own_ids = run("otool", "-D", str(path)).splitlines()[1:]
            for line in run("otool", "-L", str(path)).splitlines()[1:]:
                name = line.strip().split(" (compatibility", 1)[0]
                if name in own_ids:
                    continue
                if name.startswith("@loader_path/"):
                    dependency = (
                        path.parent / name.removeprefix("@loader_path/")
                    ).resolve()
                    if (
                        not dependency.is_relative_to(output)
                        or not dependency.is_file()
                    ):
                        raise RuntimeError(
                            f"Unresolved deployed dependency in {path}: {name}"
                        )
                elif name.startswith("@"):
                    raise RuntimeError(
                        f"Unresolved relative dependency in {path}: {name}"
                    )
                if name.startswith("/") and not name.startswith(
                    ("/usr/lib/", "/System/Library/")
                ):
                    raise RuntimeError(
                        f"Absolute non-system dependency in {path}: {name}"
                    )


def main():
    """Assembles and audits the actual delivered platform payload."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--workspace", type=Path, required=True)
    parser.add_argument("--qt-prefix", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--qt-source", type=Path, required=True)
    parser.add_argument(
        "--development-qt",
        action="store_true",
        help="Allow a local Qt version that differs from the release pin",
    )
    args = parser.parse_args()
    args.workspace = args.workspace.resolve()
    args.qt_prefix = args.qt_prefix.resolve()
    if not args.development_qt:
        qmake = next(
            (
                path
                for path in (
                    args.qt_prefix / "bin/qmake",
                    args.qt_prefix / "bin/qmake6",
                    args.qt_prefix / "lib/qt6/bin/qmake",
                )
                if path.is_file()
            ),
            None,
        )
        if (
            qmake is None
            or run(str(qmake), "-query", "QT_VERSION") != QT_VERSION
        ):
            raise RuntimeError(
                f"Release bundles require Qt {QT_VERSION} and matching source"
            )
    args.version = (ROOT / "research/eka2l1/VERSION").read_text().strip()
    output = args.output.resolve()
    output.mkdir()
    (output / "licenses").mkdir()
    notices(args.workspace / "source", output / "licenses/EKA2L1")
    notices(args.qt_source, output / "licenses/Qt")
    if (args.workspace / "qt-icu-source").is_dir():
        notices(args.workspace / "qt-icu-source", output / "licenses/ICU")
    fonts = output / "licenses/Fonts"
    fonts.mkdir()
    (fonts / "Roboto-NOTICE.txt").write_text(
        "Roboto: Copyright 2011 Google Inc. All Rights Reserved.\n"
        "Licensed under the Apache License, Version 2.0.\n"
        "https://www.apache.org/licenses/LICENSE-2.0\n"
    )
    shutil.copy(ROOT / "LICENSE", fonts / "Apache-2.0.txt")
    for source in args.workspace.glob("SDL2-*"):
        if source.is_dir():
            notices(source, output / "licenses/SDL2")
    for name in ("abseil-src", "nlohmann_json-src"):
        notices(
            args.workspace / "build/_deps" / name, output / "licenses" / name
        )
    notices(ROOT / "cpp/symbian/emulator", output / "licenses/SDK-adapters")
    shutil.copy(ROOT / "LICENSE", output / "licenses/Symbian-Apache-2.0.txt")
    system = "macos" if platform.system() == "Darwin" else "linux"
    frontend = (
        mac_bundle(args, output)
        if system == "macos"
        else linux_bundle(args, output)
    )
    actual = json.loads(
        run(str(output / frontend), "--symbian-sdk-capabilities")
    )
    arch = platform.machine()
    if arch in {"aarch64", "arm64"}:
        arch = "arm64" if system == "macos" else "aarch64"
    declaration = {
        "schema": "symbian.emulator-distribution/v1",
        "version": args.version,
        "system": system,
        "architecture": arch,
        "frontend": frontend,
        "importer": "bin/symbian_firmware_tool",
        "control_protocol": actual["control_protocol"],
        "capabilities": actual["capabilities"],
    }
    if actual["version"] != args.version:
        raise RuntimeError("Built executable does not match emulator VERSION")
    (output / "emulator.json").write_text(
        json.dumps(declaration, indent=2) + "\n"
    )
    audit(output, system)
    archive = (
        output.parent
        / f"symbian-emulator-{args.version}-{system}-{arch}.tar.gz"
    )
    with tarfile.open(archive, "w:gz") as bundle:
        bundle.add(output, arcname=".")
    (output.parent / f"symbian-emulator-{args.version}.json").write_text(
        json.dumps(
            {
                "schema": "symbian.emulator-release/v1",
                "version": args.version,
                "control_protocol": actual["control_protocol"],
                "capabilities": actual["capabilities"],
            },
            indent=2,
        )
        + "\n"
    )
    print(archive)


if __name__ == "__main__":
    main()
