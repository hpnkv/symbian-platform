"""Checks the independent emulator release matrix before publication."""

import argparse
import json
import tarfile
from pathlib import Path


def check(directory, version):
    """Requires four tested native payloads and all corresponding sources."""
    declaration = json.loads(
        (directory / f"symbian-emulator-{version}.json").read_text()
    )
    if (
        declaration["schema"] != "symbian.emulator-release/v1"
        or declaration["version"] != version
    ):
        raise ValueError(
            "Incorrect independent emulator compatibility metadata"
        )
    expected = {
        f"symbian-emulator-{version}.json",
        f"symbian-emulator-source-{version}.tar.gz",
    }
    for system, arch in (
        ("linux", "x86_64"),
        ("linux", "aarch64"),
        ("macos", "x86_64"),
        ("macos", "arm64"),
    ):
        filename = f"symbian-emulator-{version}-{system}-{arch}.tar.gz"
        runtime = (
            f"symbian-emulator-runtime-source-{version}-{system}-{arch}.tar.gz"
        )
        expected.update((filename, runtime))
        with tarfile.open(directory / filename) as bundle:
            names = {
                member.name.removeprefix("./"): member for member in bundle
            }
            metadata = json.load(bundle.extractfile(names["emulator.json"]))
            if (
                metadata["version"],
                metadata["system"],
                metadata["architecture"],
            ) != (version, system, arch):
                raise ValueError(f"Wrong native bundle: {filename}")
            if metadata["control_protocol"] != declaration[
                "control_protocol"
            ] or set(metadata["capabilities"]) != set(
                declaration["capabilities"]
            ):
                raise ValueError(f"Different executable contract: {filename}")
            for name in (metadata["frontend"], metadata["importer"]):
                if not names[name].mode & 0o111:
                    raise ValueError(
                        f"Non-executable delivered tool: {filename}/{name}"
                    )
            if not any(name.startswith("licenses/Qt/") for name in names):
                raise ValueError(f"Missing original Qt notices: {filename}")
        with tarfile.open(directory / runtime) as sources:
            if not any(member.isfile() for member in sources):
                raise ValueError(
                    f"Empty runtime corresponding sources: {runtime}"
                )
    with tarfile.open(
        directory / f"symbian-emulator-source-{version}.tar.gz"
    ) as sources:
        names = {member.name.removeprefix("./") for member in sources}
        for name in (
            "BUILD.md",
            "source/CMakeLists.txt",
            "source/src/external/ffmpeg/configure",
            "dependencies/abseil/CMakeLists.txt",
            "dependencies/nlohmann_json/CMakeLists.txt",
            "dependencies/libuv/CMakeLists.txt",
            "dependencies/SDL2-2.30.11.tar.gz",
            "platform/scripts/build_emulator.py",
            "qt-everywhere-src-6.8.3.tar.xz",
        ):
            if name not in names:
                raise ValueError(f"Missing corresponding build input: {name}")
    if {path.name for path in directory.iterdir()} != expected:
        raise ValueError("Unexpected or missing independent release assets")
    print(
        "Four native bundles, runtime sources and shared Qt/core sources passed"
    )


def main():
    """Audits assets without loading another architecture's binaries."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("--version", required=True)
    args = parser.parse_args()
    check(args.directory, args.version)


if __name__ == "__main__":
    main()
