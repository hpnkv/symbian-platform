"""Rejects incomplete or mislabelled independently published emulator assets."""

import importlib.util
import io
import json
import tarfile
from pathlib import Path

import pytest

spec = importlib.util.spec_from_file_location(
    "emulator_release", Path(__file__).parents[1] / "check_emulator_release.py"
)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


def archive(path, files):
    with tarfile.open(path, "w:gz") as output:
        for name, data in files.items():
            member = tarfile.TarInfo("./" + name)
            member.size = len(data)
            member.mode = 0o755 if name.startswith("bin/") else 0o644
            output.addfile(member, io.BytesIO(data))


@pytest.fixture
def release(tmp_path):
    contract = {
        "schema": "symbian.emulator-release/v1",
        "version": "0.1.0",
        "control_protocol": "symbian.emulator-control/v1",
        "capabilities": ["dynarmic", "dyncom"],
    }
    (tmp_path / "symbian-emulator-0.1.0.json").write_text(json.dumps(contract))
    for system, arch in (
        ("linux", "x86_64"),
        ("linux", "aarch64"),
        ("macos", "x86_64"),
        ("macos", "arm64"),
    ):
        metadata = dict(
            contract,
            system=system,
            architecture=arch,
            frontend="bin/frontend",
            importer="bin/importer",
        )
        archive(
            tmp_path / f"symbian-emulator-0.1.0-{system}-{arch}.tar.gz",
            {
                "emulator.json": json.dumps(metadata).encode(),
                "bin/frontend": b"fixture",
                "bin/importer": b"fixture",
                "licenses/Qt/LICENSES/LGPL-3.0.txt": b"notice",
            },
        )
        archive(
            tmp_path
            / f"symbian-emulator-runtime-source-0.1.0-{system}-{arch}.tar.gz",
            {"library.tar.gz": b"source"},
        )
    archive(
        tmp_path / "symbian-emulator-source-0.1.0.tar.gz",
        {
            name: b"source"
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
            )
        },
    )
    return tmp_path


def test_complete_independent_matrix(release):
    module.check(release, "0.1.0")


def test_mislabelled_native_architecture_fails(release):
    source = release / "symbian-emulator-0.1.0-linux-x86_64.tar.gz"
    target = release / "symbian-emulator-0.1.0-linux-aarch64.tar.gz"
    target.write_bytes(source.read_bytes())
    with pytest.raises(ValueError, match="Wrong native bundle"):
        module.check(release, "0.1.0")


def test_missing_corresponding_build_input_fails(release):
    archive(
        release / "symbian-emulator-source-0.1.0.tar.gz",
        {"BUILD.md": b"source"},
    )
    with pytest.raises(ValueError, match="Missing corresponding build input"):
        module.check(release, "0.1.0")


def test_empty_runtime_source_payload_fails(release):
    archive(
        release / "symbian-emulator-runtime-source-0.1.0-macos-arm64.tar.gz", {}
    )
    with pytest.raises(ValueError, match="Empty runtime corresponding sources"):
        module.check(release, "0.1.0")
