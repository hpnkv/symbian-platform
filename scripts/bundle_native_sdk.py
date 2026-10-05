"""Assembles a relocatable host and EKA2 target SDK without Python payloads."""

import argparse
import json
import os
import platform
import re
import shutil
import subprocess
from importlib.metadata import distributions
from pathlib import Path

from check_native_sdk import write_example

TOOLS = (
    "clang",
    "clang++",
    "ld.lld",
    "llvm-ar",
    "llvm-ranlib",
    "clang-scan-deps",
)
# These are the host OS ABI, rather than third-party SDK dependencies. Copying
# glibc without its loader is unsafe; Linux distributions require glibc 2.28+.
GLIBC = re.compile(r"^(ld-linux.*|lib(c|m|dl|pthread|rt|resolv|util)\.so\..*)$")


def run(*args: str) -> str:
    """Runs a host tool, reporting failures with its actual diagnostic."""
    return subprocess.check_output(args, text=True).strip()


class Closure:
    """Copies and relocates the complete non-system dynamic tool closure."""

    def __init__(self, output: Path):
        self.output = output
        self.libraries = output / "libexec/host"
        self.libraries.mkdir(parents=True)
        self.copied: dict[Path, Path] = {}
        self.pending: list[tuple[Path, Path]] = []
        self.macos = platform.system() == "Darwin"

    def copy(self, original: Path, target: Path) -> None:
        """Copies once, retaining multicall driver names through symlinks."""
        original = original.resolve(strict=True)
        if original in self.copied:
            target.symlink_to(
                os.path.relpath(self.copied[original], target.parent)
            )
            return
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy(original, target)
        target.chmod(target.stat().st_mode | 0o200)
        self.copied[original] = target
        self.pending.append((original, target))
        # Homebrew bottles retain licenses at the keg root. Copy the notices,
        # not Homebrew's SBOMs or install receipts.
        for parent in original.parents:
            if parent.parent.parent.name == "Cellar":
                notices = [
                    file
                    for file in parent.iterdir()
                    if file.is_file()
                    and re.match(r"(LICENSE|COPYING|NOTICE)", file.name, re.I)
                ]
                if not notices:
                    raise RuntimeError(f"No bundled license for {original}")
                destination = (
                    self.output / "licenses/tools" / parent.parent.name
                )
                destination.mkdir(parents=True, exist_ok=True)
                for file in notices:
                    shutil.copy(file, destination / file.name)
                break
        if "site-packages" in original.parts:
            for distribution in distributions():
                files = distribution.files or ()
                if not any(
                    distribution.locate_file(file).resolve() == original
                    for file in files
                ):
                    continue
                destination = (
                    self.output
                    / "licenses/tools"
                    / distribution.metadata["Name"]
                )
                for file in files:
                    if "licenses" in file.parts:
                        destination.mkdir(parents=True, exist_ok=True)
                        shutil.copy(
                            distribution.locate_file(file),
                            destination / file.name,
                        )
                if not destination.exists():
                    raise RuntimeError(f"No installed tool license: {original}")
                break
        if (
            not self.macos
            and str(original).startswith(("/usr/", "/lib/"))
            and "site-packages" not in original.parts
        ):
            # Debian's copyright file includes the applicable source notices
            # for packaged tools and the shared libraries we redistribute.
            query = subprocess.run(
                ["dpkg-query", "-S", str(original)],
                capture_output=True,
                text=True,
            )
            if query.returncode != 0:
                query = subprocess.run(
                    ["dpkg-query", "-S", str(original).removeprefix("/usr")],
                    capture_output=True,
                    text=True,
                )
            if query.returncode != 0:
                raise RuntimeError(f"Cannot locate package license: {original}")
            package = query.stdout.split(": ", 1)[0].split(":", 1)[0]
            notice = Path("/usr/share/doc") / package / "copyright"
            destination = self.output / "licenses/tools" / package
            destination.mkdir(parents=True, exist_ok=True)
            shutil.copy(notice, destination / "copyright")

    def mac_dependencies(self, original: Path) -> list[tuple[str, Path]]:
        """Resolves Mach-O install names using the original binary's rpaths."""
        lines = run("otool", "-l", str(original)).splitlines()
        rpaths = []
        for index, line in enumerate(lines):
            if line.strip() == "cmd LC_RPATH":
                rpaths.append(
                    lines[index + 2].strip().split(" (offset", 1)[0][5:]
                )
        own_id = run("otool", "-D", str(original)).splitlines()[1:]
        result = []
        for line in run("otool", "-L", str(original)).splitlines()[1:]:
            name = line.strip().split(" (compatibility", 1)[0]
            if name in own_id or name.startswith(
                ("/usr/lib/", "/System/Library/")
            ):
                continue
            candidates = [name]
            if name.startswith("@rpath/"):
                candidates = [p + name[len("@rpath") :] for p in rpaths]
                candidates.append(
                    str(original.parent.parent / "lib" / Path(name).name)
                )
            candidates = [
                p.replace("@loader_path", str(original.parent)).replace(
                    "@executable_path", str(original.parent)
                )
                for p in candidates
            ]
            resolved = next(
                (Path(p) for p in candidates if Path(p).is_file()), None
            )
            if resolved is None:
                raise RuntimeError(
                    f"Unresolved dependency {name} of {original}"
                )
            result.append((name, resolved))
        return result

    def finish(self) -> None:
        """Relocates every library and executable, then signs Mach-O files."""
        while self.pending:
            original, target = self.pending.pop(0)
            if self.macos:
                dependencies = self.mac_dependencies(original)
            else:
                dependencies = []
                for line in run("ldd", str(original)).splitlines():
                    if "=> not found" in line:
                        raise RuntimeError(
                            f"Unresolved tool dependency: {line}"
                        )
                    match = re.match(r"\s*(\S+) => (/\S+) ", line)
                    if match and not GLIBC.match(match[1]):
                        dependencies.append((match[1], Path(match[2])))
            for name, dependency in dependencies:
                destination = self.libraries / Path(name).name
                if not destination.exists():
                    self.copy(dependency, destination)
                elif (
                    self.copied.get(dependency.resolve())
                    != destination.resolve()
                ):
                    raise RuntimeError(
                        f"Library basename collision: {dependency}"
                    )
                if self.macos:
                    relative = os.path.relpath(destination, target.parent)
                    run(
                        "install_name_tool",
                        "-change",
                        name,
                        f"@loader_path/{relative}",
                        str(target),
                    )
            if self.macos:
                if target.parent == self.libraries:
                    run(
                        "install_name_tool",
                        "-id",
                        f"@loader_path/{target.name}",
                        str(target),
                    )
            else:
                relative = os.path.relpath(self.libraries, target.parent)
                run(
                    "patchelf",
                    "--set-rpath",
                    f"$ORIGIN/{relative}",
                    str(target),
                )
        if self.macos:
            for target in self.copied.values():
                run("codesign", "--force", "--sign", "-", str(target))


def bundle(args: argparse.Namespace) -> None:
    """Combines separately built host, target and compiler payloads."""
    output = args.output.resolve()
    if output.exists():
        raise RuntimeError(f"Output already exists: {output}")
    output.mkdir(parents=True)
    for name in ("include", "lib", "cmake", "proxies", "source", "licenses"):
        shutil.copytree(args.guest / name, output / name, symlinks=False)
    # An installed development SDK can also contain Python/host wrappers;
    # target payloads must never retain those external-host dependencies.
    shutil.rmtree(output / "lib/host", ignore_errors=True)
    shutil.rmtree(output / "lib/python", ignore_errors=True)
    # Proxy build reports and temporary build trees are unnecessary at install
    # time. Keep the ordinal transport files; their notices remain in licenses.
    for directory in (output / "proxies").iterdir():
        for item in directory.iterdir():
            if item.is_dir():
                shutil.rmtree(item)
            elif item.suffix != ".dso":
                item.unlink()
    shutil.copytree(args.host, output / "host", symlinks=True)
    (output / "bin").mkdir(exist_ok=True)
    closure = Closure(output)
    for name in TOOLS:
        source = args.llvm / name
        if name == "ld.lld" and args.lld:
            source = args.lld
        closure.copy(source, output / "bin" / name)
    closure.copy(
        args.host / "bin/symbian-native", output / "bin/symbian-native"
    )
    for name in ("rcomp", "uidcrc"):
        closure.copy(args.resources / name, output / "bin" / name)
    # CMake must find its own modules after relocation, not site-packages or a
    # package-manager prefix. Its official binary tree is supplied as input.
    closure.copy(args.cmake / "bin/cmake", output / "bin/cmake")
    for modules in (args.cmake / "share").glob("cmake*"):
        shutil.copytree(modules, output / "share" / modules.name)
    closure.copy(args.ninja, output / "bin/ninja")
    for resource in (args.llvm.parent / "lib/clang").glob("*/include"):
        shutil.copytree(
            resource, output / "lib/clang" / resource.parent.name / "include"
        )
    shutil.copy(
        args.resources.parent / "licenses/rcomp-EPL-1.0.html",
        output / "licenses/rcomp-EPL-1.0.html",
    )
    closure.finish()
    for notices in args.license:
        destination = output / "licenses/tools" / notices.parent.name
        if notices.is_dir():
            shutil.copytree(
                notices, destination / notices.name, dirs_exist_ok=True
            )
        else:
            destination.mkdir(parents=True, exist_ok=True)
            shutil.copy(notices, destination / notices.name)
    manifest = dict(
        schema_version=1,
        prefix=".",
        compiler="bin/clang++",
        c_compiler="bin/clang",
        linker="bin/ld.lld",
        ar="bin/llvm-ar",
        ranlib="bin/llvm-ranlib",
        python=None,
        emulator=None,
        architectures=["armv5t", "armv6"],
    )
    (output / "sdk.json").write_text(json.dumps(manifest, indent=2) + "\n")
    # Use the maintained toolchain rather than an older development export.
    root = Path(__file__).resolve().parent.parent
    shutil.copy(
        root / "symbian/toolchain/cmake/symbian-arm.cmake",
        output / "cmake/symbian-arm.cmake",
    )
    shutil.copy(root / "LICENSE", output / "licenses/Symbian-Apache-2.0.txt")
    shutil.copy(root / "VERSION", output / "VERSION")
    shutil.copy(
        root / "symbian/toolchain/cmake/SymbianPic.cmake",
        output / "cmake/SymbianPic.cmake",
    )
    write_example(output / "examples/hello_time", root)
    counter = output / "examples/gui_app"
    counter.mkdir()
    for pattern in (
        "*.cc",
        "*.h",
        "*.S",
        "*.ld",
        "CMakeLists.txt",
        "symbian.toml",
    ):
        for source in (root / "examples/gui_app").glob(pattern):
            shutil.copy(source, counter / source.name)
    shutil.copytree(root / "examples/gui_app/assets", counter / "assets")
    shutil.copy(root / ".clang-format", counter / ".clang-format")
    presets = json.loads(
        (root / "examples/gui_app/CMakePresets.json").read_text()
    )
    preset = presets["configurePresets"][0]
    preset["toolchainFile"] = "${sourceDir}/../../cmake/symbian-arm.cmake"
    preset["cacheVariables"]["SYMBIAN_GUI_SDK_INCLUDE"] = (
        "${sourceDir}/../../include/platform"
    )
    (counter / "CMakePresets.json").write_text(
        json.dumps(presets, indent=2) + "\n"
    )
    (counter / "sdk-location.json").write_text('{"sdk": "../.."}\n')


def main() -> None:
    """Assembles the SDK from explicit, previously built inputs."""
    parser = argparse.ArgumentParser(description=__doc__)
    for name in (
        "guest",
        "host",
        "llvm",
        "resources",
        "cmake",
        "ninja",
        "output",
    ):
        parser.add_argument(f"--{name}", type=Path, required=True)
    parser.add_argument("--lld", type=Path)
    parser.add_argument("--license", type=Path, action="append", default=[])
    bundle(parser.parse_args())


if __name__ == "__main__":
    main()
