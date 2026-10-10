"""Frozen DLL conversion with Clang/LLD and independent consumers."""

import os
import shutil
import struct
from pathlib import Path

import pytest

from symbian import toolchain
from symbian._native import convert_dll, inspect_e32
from symbian.status import Code, StatusError
from symbian.toolchain.verification import run_oracles

PROJECT = Path(__file__).parents[2] / "probes/dll_probe"
TOOLS = shutil.which("clang++") and shutil.which("ld.lld")


@pytest.fixture(scope="module")
def dll(tmp_path_factory):
    if not TOOLS:
        pytest.skip("Clang/LLD required")
    root = tmp_path_factory.mktemp("Native DLL with spaces")
    project = root / "project"
    shutil.copytree(PROJECT, project)
    (project / "exports.def").write_text(
        "EXPORTS\nSymbianProbeTransform @ 7 NONAME\n"
    )
    cmake = project / "CMakeLists.txt"
    cmake.write_text(
        cmake.read_text().replace(
            "SOURCES probe.cc",
            'SOURCES probe.cc EXPORT_DEFINITION "exports.def"',
        )
    )
    manifest = project / "symbian.toml"
    manifest.write_text(
        manifest.read_text().replace(
            "[project]", '[project]\nexport_definition = "exports.def"'
        )
    )
    report = toolchain.build(project, root / "build")
    return report, Path(report["linked_elf"]).read_bytes()


def test_frozen_dll_build_records_definition_and_actual_symbol(dll):
    report, elf = dll
    assert report["schema"] == "symbian.e32-dll-experiment/v1"
    assert report["artifact_kind"] == "experimental-e32-dll"
    assert report["reproducible"]
    assert not report["symbian_loader_verified"]
    assert not report["runtime_verified"]
    info = report["e32"]
    assert info["dll"]
    assert info["header_size"] == 156
    assert len(info["exports"]) == 7
    assert len(info["code_relocations"]) == 8
    assert info["exports"][-1]["ordinal"] == 7
    assert not info["exports"][-1]["absent"]
    assert info["exports"][-1]["address"] != 0x8080  # No fixed fixture address.
    assert all(slot["absent"] for slot in info["exports"][:-1])
    definition = Path(report["linked_elf"]).parents[1] / "project/exports.def"
    assert str(definition) in report["inputs"]
    assert (
        convert_dll(elf, definition.read_bytes(), [], 0xE0000810)
        == Path(report["artifact"]).read_bytes()
    )


def test_dll_metadata_readonly_and_absence_bitmap(dll):
    report, _ = dll
    image = Path(report["artifact"]).read_bytes()
    info = inspect_e32(image)
    assert image[155] == 0xC0  # Unused bitmap bits remain set.
    with pytest.raises(AttributeError):
        info.dll = False
    with pytest.raises(AttributeError):
        info.exports[6].address = 0
    for length in range(len(image)):
        with pytest.raises(StatusError):
            inspect_e32(image[:length])


@pytest.mark.parametrize(
    "ordinal,header_size", [(1, 156), (641, 236), (65535, 8348)]
)
def test_frozen_ordinal_header_and_relocation_page_bounds(
    dll, ordinal, header_size
):
    _, elf = dll
    definition = f"EXPORTS\nSymbianProbeTransform @ {ordinal} NONAME\n".encode()
    image = convert_dll(elf, definition, [], 0xE0000810)
    info = inspect_e32(image)
    assert info.header_size == header_size
    assert len(info.exports) == ordinal
    assert len(info.code_relocations) == ordinal + 1
    assert not info.exports[-1].absent
    assert all(slot.absent for slot in info.exports[:-1])
    if ordinal == 1:
        assert image[152:156] == b"\0\0\0\0"
    else:
        changed = bytearray(image)
        changed[156] ^= 1
        with pytest.raises(StatusError) as caught:
            inspect_e32(bytes(changed))
        assert caught.value.code == Code.DATA_LOSS


@pytest.mark.parametrize(
    "definition,code",
    [
        (b"EXPORTS\nMissing @ 1 NONAME\n", Code.NOT_FOUND),
        (
            b"EXPORTS\nSymbianProbeTransform @ 1 NONAME DATA 4\n",
            Code.UNIMPLEMENTED,
        ),
        (b"EXPORTS\nSymbianProbeTransform @ 0 NONAME\n", Code.INVALID_ARGUMENT),
    ],
)
def test_dll_export_errors_keep_native_status(dll, definition, code):
    _, elf = dll
    with pytest.raises(StatusError) as caught:
        convert_dll(elf, definition, [], 0xE0000810)
    assert caught.value.code == code


def test_relocation_and_export_payload_corruption_is_detected(dll):
    report, _ = dll
    image = Path(report["artifact"]).read_bytes()
    directory = struct.unpack_from("<I", image, 88)[0]
    relocations = struct.unpack_from("<I", image, 112)[0]
    for offset in (
        directory - 4,
        directory,
        directory + 24,
        relocations,
        relocations + 4,
    ):
        changed = bytearray(image)
        struct.pack_into("<I", changed, offset, 0xFFFFFFFF)
        with pytest.raises(StatusError):
            inspect_e32(bytes(changed))


def test_dll_definition_cannot_escape_project(tmp_path):
    from symbian.toolchain.executable import build_executable

    with pytest.raises(StatusError) as caught:
        build_executable(
            PROJECT,
            tmp_path,
            {
                "kind": "e32-dll-experiment",
                "uid3": 0xE0000810,
                "export_definition": "../../doc/README.md",
            },
            "clang++",
            "ld.lld",
        )
    assert caught.value.code == Code.INVALID_ARGUMENT


@pytest.mark.skipif(
    not os.environ.get("SYMBIAN_EKA2L1_ORACLES_BUILD"),
    reason="Set SYMBIAN_EKA2L1_ORACLES_BUILD for original checksum/validator",
)
@pytest.mark.parametrize("ordinal", [7, 641, 65535])
def test_historical_validation_accepts_complete_variable_dll_header(
    dll, tmp_path, ordinal
):
    _, elf = dll
    image = tmp_path / "probe.dll"
    image.write_bytes(
        convert_dll(
            elf,
            f"EXPORTS\nSymbianProbeTransform @ {ordinal} NONAME\n".encode(),
            [],
            0xE0000810,
        )
    )
    checks, _ = run_oracles(
        {"SYMBIAN_E32_TEST_IMAGE": image},
        (("symbian_checksum_oracle", 1), ("symbian_validator_oracle", 7)),
        Path(os.environ["SYMBIAN_EKA2L1_ORACLES_BUILD"]).resolve(),
        tmp_path / "checks",
    )
    assert all(check["passed"] for check in checks)
