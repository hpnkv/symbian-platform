"""Checks the host release matrix before publishing distributions."""

import argparse
import itertools
import tarfile
from pathlib import Path

from packaging.utils import parse_wheel_filename

HOSTS = (
    ("linux", "x86_64", "manylinux_2_28_x86_64"),
    ("linux", "aarch64", "manylinux_2_28_aarch64"),
    ("macos", "x86_64", "macosx_15_0_x86_64"),
    ("macos", "arm64", "macosx_15_0_arm64"),
)
PYTHONS = ("cp311", "cp312", "cp313", "cp314")


def check(dist: Path, assets: Path, version: str) -> None:
    """Rejects missing, duplicate or incorrectly versioned release payloads."""
    expected = set(itertools.product(PYTHONS, (h[2] for h in HOSTS)))
    actual = set()
    wheels = list(dist.glob("*.whl"))
    if len(wheels) != len(expected):
        raise ValueError(f"Expected 16 wheels, found {len(wheels)}")
    for wheel in wheels:
        name, wheel_version, _, tags = parse_wheel_filename(wheel.name)
        if name != "symbian-platform" or str(wheel_version) != version:
            raise ValueError(f"Unexpected distribution: {wheel.name}")
        matches = {
            (tag.interpreter, tag.platform)
            for tag in tags
            if tag.abi == tag.interpreter
        } & expected
        if len(matches) != 1 or actual & matches:
            raise ValueError(f"Unexpected or duplicate wheel: {wheel.name}")
        actual.update(matches)
    if actual != expected:
        raise ValueError(f"Missing wheel targets: {expected - actual}")
    archives = {
        dist / f"symbian_platform-{version}.tar.gz": (
            f"symbian_platform-{version}/VERSION",
            f"symbian_platform-{version}/cpp/python/CMakeLists.txt",
            f"symbian_platform-{version}/cpp/symbian/concurrency/upstream/cpp/thread/thread/fiber.h",
        ),
        assets / f"symbian-source-{version}.tar.gz": (
            f"symbian-{version}/VERSION",
            f"symbian-{version}/LICENSE",
            f"symbian-{version}/CMakeLists.txt",
        ),
    }
    for system, arch, _ in HOSTS:
        archives[assets / f"symbian-host-{version}-{system}-{arch}.tar.gz"] = (
            "./bin/symbian-native",
            "./lib/cmake/SymbianHost/SymbianHostConfig.cmake",
            "./share/symbian/LICENSE",
            "./share/symbian/VERSION",
        )
        archives[assets / f"symbian-sdk-{version}-{system}-{arch}.tar.gz"] = (
            "./VERSION",
            "./sdk.json",
            "./bin/clang++",
            "./bin/ld.lld",
            "./bin/llvm-ar",
            "./bin/llvm-ranlib",
            "./bin/clang-scan-deps",
            "./bin/symbian-native",
            "./bin/rcomp",
            "./bin/uidcrc",
            "./bin/cmake",
            "./bin/ninja",
            "./cmake/SymbianApp.cmake",
            "./host/lib/cmake/SymbianHost/SymbianHostConfig.cmake",
            "./licenses/Symbian-Apache-2.0.txt",
            "./examples/hello_time/CMakeLists.txt",
            *(
                f"./lib/{target}/lib{component}.a"
                for target in ("armv5t", "armv6")
                for component in (
                    "symbian_guest_runtime",
                    "symbian_http",
                    "symbian_api_tls",
                    "symbian_api_websocket",
                )
            ),
        )
    for archive, required in archives.items():
        with tarfile.open(archive) as stream:
            members = set(stream.getnames())
            missing = set(required) - members
            if missing:
                raise ValueError(f"{archive.name}: missing {missing}")
            if archive.name.startswith("symbian-sdk-"):
                forbidden = {
                    member
                    for member in members
                    if member.startswith(("./lib/python/", "./bin/python"))
                    or member.endswith(
                        ("/digests.json", "/host-dependencies.json")
                    )
                }
                if forbidden:
                    raise ValueError(
                        f"{archive.name}: unexpected Python/provenance payload"
                    )
            version_path = next(p for p in required if p.endswith("VERSION"))
            contents = stream.extractfile(version_path)
            if contents is None or contents.read().decode().strip() != version:
                raise ValueError(f"{archive.name}: incorrect VERSION")
    print(f"Verified {len(wheels)} wheels and {len(archives)} archives")


def main() -> None:
    """Validates staged artifacts with the repository's release version."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dist", type=Path, default=Path("dist"))
    parser.add_argument("--assets", type=Path, default=Path("release-assets"))
    parser.add_argument("--version", required=True)
    args = parser.parse_args()
    check(args.dist, args.assets, args.version)


if __name__ == "__main__":
    main()
