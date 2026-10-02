"""Compiler pointer tables, virtual dispatch, and original loader consumers."""

import hashlib
import json
import os
import shutil
import struct
from pathlib import Path

import pytest

from symbian import packaging, toolchain
from symbian._native import convert_dll, inspect_e32
from symbian.e32 import convert_pic_executable
from symbian.status import Code, StatusError
from symbian.tests.cli_json import main
from symbian.toolchain.verification import (
    run_oracles,
    verify_pointers,
    verify_probe,
)

PROJECT = Path(__file__).parents[2] / "examples/pointer_probe"
TOOLS = shutil.which("clang++") and shutil.which("ld.lld")


@pytest.fixture(scope="module")
def pointers(tmp_path_factory):
    if not TOOLS:
        pytest.skip("Clang/LLD required")
    root = tmp_path_factory.mktemp("Pointer project with spaces")
    report = toolchain.build(PROJECT, root / "build")
    package = packaging.package(
        PROJECT, Path(report["artifact"]), root / "package"
    )
    return report, package, root


def test_real_pointer_build_retains_four_fixups_and_header_dependencies(
    pointers,
):
    report, _, _ = pointers
    assert report["reproducible"]
    assert not report["runtime_verified"]
    assert not report["symbian_loader_verified"]
    assert len(report["e32"]["code_relocations"]) == 4
    assert report["e32"]["imports"] == []
    assert not report["e32"]["dll"]
    assert str(PROJECT / "probe.h") in report["inputs"]
    database = json.loads(Path(report["compile_commands"]).read_text())
    assert len(database) == 4
    assert all(
        "-fno-exceptions" in row["command"]
        for row in database
        if row["file"].endswith(".cc")
    )


def test_inspection_rejects_corrupted_mapped_pointer_values(pointers):
    report, _, _ = pointers
    image = Path(report["artifact"]).read_bytes()
    info = inspect_e32(image)
    for offset in info.code_relocations:
        changed = bytearray(image)
        struct.pack_into("<I", changed, info.header_size + offset, 0xFFFFFFFF)
        with pytest.raises(StatusError) as caught:
            inspect_e32(bytes(changed))
        assert caught.value.code == Code.DATA_LOSS


def test_linked_pointer_state_must_agree_with_thumb_symbol(pointers):
    report, _, _ = pointers
    elf = bytearray(Path(report["linked_elf"]).read_bytes())
    program = struct.unpack_from("<I", elf, 28)[0]
    code_offset = struct.unpack_from("<I", elf, program + 4)[0]
    offset = code_offset + report["e32"]["code_relocations"][0]
    elf[offset] ^= 1
    with pytest.raises(StatusError) as caught:
        convert_pic_executable(bytes(elf), 0xE0000808)
    assert caught.value.code == Code.DATA_LOSS


def test_unknown_writable_section_cannot_claim_relro_policy(pointers):
    report, _, _ = pointers
    elf = Path(report["linked_elf"]).read_bytes()
    assert b".data.rel.ro\0" in elf
    with pytest.raises(StatusError) as caught:
        convert_pic_executable(
            elf.replace(b".data.rel.ro\0", b".data.rex.ro\0"), 0xE0000808
        )
    assert caught.value.code == Code.UNIMPLEMENTED


def test_relro_section_name_bounds_are_checked(pointers):
    report, _, _ = pointers
    elf = bytearray(Path(report["linked_elf"]).read_bytes())
    sections = struct.unpack_from("<I", elf, 32)[0]
    count = struct.unpack_from("<H", elf, 48)[0]
    for index in range(1, count):
        position = sections + index * 40
        if struct.unpack_from("<I", elf, position + 8)[0] == 3:
            struct.pack_into("<I", elf, position, 0xFFFFFFFF)
            break
    else:
        pytest.fail("Compiler did not retain a RELRO table")
    with pytest.raises(StatusError) as caught:
        convert_pic_executable(bytes(elf), 0xE0000808)
    assert caught.value.code == Code.DATA_LOSS


def test_native_dll_keeps_code_pointer_and_export_fixups(pointers):
    report, _, _ = pointers
    # Layout evidence only: this input's startup exits a process, not a DLL.
    image = convert_dll(
        Path(report["linked_elf"]).read_bytes(),
        b"EXPORTS\nSymbianAbiProbe @ 7 NONAME\n",
        [],
        0xE0000810,
    )
    info = inspect_e32(image)
    assert len(info.code_relocations) == 11
    assert info.code_relocations[:4] == report["e32"]["code_relocations"]
    assert len(info.exports) == 7
    assert info.dll


def test_package_contains_complete_pointer_image(pointers):
    report, package, _ = pointers
    assert (
        package["sis"]["executable_size"]
        == Path(report["artifact"]).stat().st_size
    )
    assert package["sis"]["executable_uid"] == 0xE0000808
    assert not package["runtime_verified"]


def test_probe_verifiers_reject_other_profiles_before_running(
    pointers, tmp_path
):
    report, _, _ = pointers
    with pytest.raises(StatusError) as caught:
        verify_probe(
            Path(report["artifact"]), tmp_path / "missing", tmp_path / "checks"
        )
    assert caught.value.code == Code.INVALID_ARGUMENT
    wrong = tmp_path / "wrong.exe"
    wrong.write_bytes(
        convert_pic_executable(
            Path(report["linked_elf"]).read_bytes(), 0xE0000809
        )
    )
    with pytest.raises(StatusError) as caught:
        verify_pointers(wrong, tmp_path / "missing", tmp_path / "checks")
    assert caught.value.code == Code.INVALID_ARGUMENT

    collision = tmp_path / "expected-image.sha1"
    shutil.copyfile(report["artifact"], collision)
    original = collision.read_bytes()
    with pytest.raises(StatusError) as caught:
        verify_pointers(
            collision,
            tmp_path / "missing",
            tmp_path,
            Path(pointers[1]["artifact"]),
        )
    assert caught.value.code == Code.INVALID_ARGUMENT
    assert collision.read_bytes() == original


@pytest.mark.skipif(
    not os.environ.get("SYMBIAN_EKA2L1_ORACLES_BUILD"),
    reason="Set SYMBIAN_EKA2L1_ORACLES_BUILD for pointer loader/installer",
)
def test_independent_pointer_cli_runs_install_and_dispatch(
    pointers, tmp_path, capsys
):
    report, package, _ = pointers
    assert (
        main(
            [
                "toolchain",
                "verify-pointers",
                report["artifact"],
                "--package",
                package["artifact"],
                "--oracles-build",
                os.environ["SYMBIAN_EKA2L1_ORACLES_BUILD"],
                "--output",
                str(tmp_path / "checks"),
            ]
        )
        == 0
    )
    result = json.loads(capsys.readouterr().out)["result"]
    assert result["tests_passed"] == 21
    assert result["mapped_pointer_words_verified"]
    assert result["virtual_dispatch_verified"]
    assert result["eka2l1_install_launch_verified"]
    assert not result["symbian_loader_verified"]
    assert not result["runtime_verified"]
    assert not result["physical_installation_verified"]
    assert (
        result["expected_image_sha1"]
        == hashlib.sha1(Path(report["artifact"]).read_bytes()).hexdigest()
    )
    assert all(
        check["passed"]
        for check in result["oracles"] + result["package_oracles"]
    )


@pytest.mark.skipif(
    not os.environ.get("SYMBIAN_EKA2L1_ORACLES_BUILD"),
    reason="Set SYMBIAN_EKA2L1_ORACLES_BUILD for combined DLL validation",
)
def test_original_validator_accepts_code_and_export_relocations(
    pointers, tmp_path
):
    report, _, _ = pointers
    image = tmp_path / "combined.dll"
    image.write_bytes(
        convert_dll(
            Path(report["linked_elf"]).read_bytes(),
            b"EXPORTS\nSymbianAbiProbe @ 7 NONAME\n",
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
