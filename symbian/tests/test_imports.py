"""Real LLD eager imports and native E32 conversion boundaries."""

import json
import os
import shutil
import struct
from pathlib import Path

import pytest

from symbian import toolchain
from symbian._native import (
    convert_dll,
    convert_imported_executable,
    inspect_e32,
)
from symbian.e32 import convert_pic_executable
from symbian.sdk import build_import_proxy
from symbian.status import Code, StatusError

PROJECT = Path(__file__).parents[2] / "probes/import_probe"
TOOLS = shutil.which("clang++") and shutil.which("ld.lld")


@pytest.fixture(scope="module")
def imported(tmp_path_factory):
    if not TOOLS:
        pytest.skip("Clang/LLD required")
    root = tmp_path_factory.mktemp("Imported project with spaces")
    project = root / "project"
    shutil.copytree(PROJECT, project)
    # A separate frozen ABI fixture exercises sparse ordinals and corruption.
    (project / "exports.def").write_text(
        "EXPORTS\nSymbianProbeTransform @ 7 NONAME\n"
    )
    proxy = build_import_proxy(
        project / "exports.def",
        ["SymbianProbeTransform"],
        "probe.dll",
        root / "proxy",
    )
    (project / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.28)\n"
        "project(import_probe LANGUAGES CXX ASM)\ninclude(SymbianPic)\n"
        "symbian_add_executable(import_probe probe.cc)\n"
        f'target_link_libraries(import_probe PRIVATE "{proxy["artifact"]}")\n'
    )
    report = toolchain.build(project, root / "build")
    return report, Path(proxy["artifact"]).read_bytes(), root


def test_real_import_build_preserves_ordinal_and_dependency_evidence(imported):
    report, _, root = imported
    assert report["schema"] == "symbian.e32-import-experiment/v1"
    assert report["reproducible"]
    assert not report["import_execution_verified"]
    assert not report["symbian_loader_verified"]
    block = report["e32"]["imports"][0]
    assert block["dll"] == "probe.dll"
    assert [slot["ordinal"] for slot in block["slots"]] == [7]
    assert str(root / "proxy/probe.dso") in report["inputs"]
    assert "probe.dso" in json.dumps(report["build_system"]["link_fragments"])
    commands = json.loads(Path(report["compile_commands"]).read_text())
    assert len(commands) == 2


def test_imported_image_truncations_and_readonly_slots(imported):
    report, _, _ = imported
    image = Path(report["artifact"]).read_bytes()
    for length in range(len(image)):
        with pytest.raises(StatusError):
            inspect_e32(image[:length])
    with pytest.raises(AttributeError):
        inspect_e32(image).imports[0].slots[0].ordinal = 1


def test_imported_image_rejects_offsets_and_preserves_addends(imported):
    report, _, _ = imported
    image = Path(report["artifact"]).read_bytes()
    info = inspect_e32(image)
    section = info.header_size + info.code_size
    slot = info.header_size + info.imports[0].slots[0].code_offset
    for offset, value in (
        (section, 0),
        (section + 4, 4),
        (section + 8, 0xFFFFFFFF),
        (section + 12, 12),
        (slot, 0),
    ):
        changed = bytearray(image)
        changed[offset : offset + 4] = value.to_bytes(4, "little")
        with pytest.raises(StatusError):
            inspect_e32(bytes(changed))
    changed = bytearray(image)
    changed[slot : slot + 4] = (0x10007).to_bytes(4, "little")
    imported_slot = inspect_e32(bytes(changed)).imports[0].slots[0]
    assert (imported_slot.ordinal, imported_slot.addend) == (7, 1)
    changed = bytearray(image)
    changed[-1] = 1
    with pytest.raises(StatusError):
        inspect_e32(bytes(changed))


def test_import_converter_requires_correct_proxy_and_rejects_pic_path(imported):
    report, proxy, _ = imported
    elf = Path(report["linked_elf"]).read_bytes()
    with pytest.raises(StatusError):
        convert_pic_executable(elf, 0xE0000808)
    with pytest.raises(StatusError) as caught:
        convert_imported_executable(elf, [], 0xE0000808)
    assert caught.value.code == Code.INVALID_ARGUMENT
    with pytest.raises(StatusError):
        convert_imported_executable(elf, [proxy, proxy], 0xE0000808)
    with pytest.raises(StatusError):
        convert_imported_executable(elf, [b"bad proxy"], 0xE0000808)
    with pytest.raises(StatusError):
        convert_imported_executable(elf, [proxy], 1)
    assert (
        convert_imported_executable(elf, [proxy], 0xE0000808)
        == Path(report["artifact"]).read_bytes()
    )


def test_converter_accepts_declared_but_unused_proxy(imported, tmp_path):
    report, proxy, _ = imported
    definition = tmp_path / "other.def"
    definition.write_text("EXPORTS\nOtherProbeTransform @ 641 NONAME\n")
    other = build_import_proxy(
        definition, ["OtherProbeTransform"], "other.dll", tmp_path / "other"
    )
    elf = Path(report["linked_elf"]).read_bytes()
    assert (
        convert_imported_executable(
            elf, [proxy, Path(other["artifact"]).read_bytes()], 0xE0000808
        )
        == Path(report["artifact"]).read_bytes()
    )


def test_dynamic_contract_tampering_is_rejected(imported):
    report, proxy, _ = imported
    elf = Path(report["linked_elf"]).read_bytes()
    section_table = struct.unpack_from("<I", elf, 32)[0]
    count = struct.unpack_from("<H", elf, 48)[0]
    mutations = []
    for index in range(1, count):
        header = section_table + index * 40
        kind = struct.unpack_from("<I", elf, header + 4)[0]
        offset = struct.unpack_from("<I", elf, header + 16)[0]
        if kind == 11:  # Undefined dynsym function must have value zero.
            mutations.append((offset + 20, 4, 0x8000))
        elif kind == 0x6FFFFFFF:  # DLL version index must resolve.
            mutations.append((offset + 2, 2, 99))
        elif kind == 6:  # First tag must be DT_NEEDED.
            mutations.append((offset, 4, 7))
        elif kind == 9 and struct.unpack_from("<I", elf, header + 8)[0] & 2:
            mutations.append((offset + 4, 1, 2))  # ABS32 is unsupported.
    assert len(mutations) == 4
    for offset, size, value in mutations:
        changed = bytearray(elf)
        changed[offset : offset + size] = value.to_bytes(size, "little")
        with pytest.raises(StatusError):
            convert_imported_executable(bytes(changed), [proxy], 0xE0000808)
    with pytest.raises(StatusError) as caught:
        convert_imported_executable(
            elf.replace(b"probe.dll\0", b"bogus.dll\0"), [proxy], 0xE0000808
        )
    assert caught.value.code == Code.FAILED_PRECONDITION


def test_retained_call_must_reach_the_correct_import_slot(imported):
    report, proxy, _ = imported
    elf = bytearray(Path(report["linked_elf"]).read_bytes())
    # The maintained linked Thumb BLX must reach its ARM PLT veneer.
    # Locate the retained R_ARM_THM_CALL, independent of startup size.
    section_table = struct.unpack_from("<I", elf, 32)[0]
    count = struct.unpack_from("<H", elf, 48)[0]
    calls = []
    for index in range(count):
        header = section_table + index * 40
        kind, _, _, offset, size = struct.unpack_from("<5I", elf, header + 4)
        if kind != 9:
            continue
        target = struct.unpack_from("<I", elf, header + 28)[0]
        target_header = section_table + target * 40
        address, file_offset = struct.unpack_from(
            "<2I", elf, target_header + 12
        )
        for position in range(offset, offset + size, 8):
            location, info = struct.unpack_from("<2I", elf, position)
            if info & 0xFF == 10:
                calls.append(file_offset + location - address)
    assert len(calls) == 1
    elf[calls[0] + 2] ^= 2
    with pytest.raises(StatusError):
        convert_imported_executable(bytes(elf), [proxy], 0xE0000808)


def test_invalid_proxy_policy_leaves_sources_intact(imported):
    report, _, root = imported
    project = root / "project"
    options = {
        "kind": "e32-import-experiment",
        "name": "import_probe",
        "uid3": 0xE0000808,
        "import_proxies": [str(Path(report["artifact"]))],
    }
    from symbian.toolchain.executable import build_executable

    with pytest.raises(StatusError) as caught:
        build_executable(project, root / "build", options, "clang++", "ld.lld")
    assert caught.value.code == Code.INVALID_ARGUMENT


def test_two_dlls_resolve_independent_versioned_ordinals(imported, tmp_path):
    _, _, root = imported
    project = tmp_path / "project"
    shutil.copytree(PROJECT, project)
    definition = project / "other.def"
    definition.write_text("EXPORTS\nOtherProbeTransform @ 641 NONAME\n")
    other = build_import_proxy(
        definition, ["OtherProbeTransform"], "other.dll", tmp_path / "other"
    )
    (project / "probe.cc").write_text(
        'extern "C" unsigned SymbianProbeTransform(unsigned);\n'
        'extern "C" unsigned OtherProbeTransform(unsigned);\n'
        "int main() { volatile unsigned input = 16;\n"
        "return SymbianProbeTransform(input) == 0x918U && "
        "OtherProbeTransform(input) == 42 ? 0 : 42; }\n"
    )
    (project / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.28)\n"
        "project(import_probe LANGUAGES CXX ASM)\ninclude(SymbianPic)\n"
        "symbian_add_executable(import_probe probe.cc)\n"
        f'target_link_libraries(import_probe PRIVATE "{root}/proxy/probe.dso" '
        f'"{other["artifact"]}")\n'
    )
    result = toolchain.build(project, tmp_path / "build")
    assert {
        block["dll"]: [slot["ordinal"] for slot in block["slots"]]
        for block in result["e32"]["imports"]
    } == {"other.dll": [641], "probe.dll": [7]}


@pytest.mark.skipif(
    not os.environ.get("SYMBIAN_EKA2L1_ORACLES_BUILD"),
    reason="Set SYMBIAN_EKA2L1_ORACLES_BUILD for ROMless import runtime",
)
def test_independent_import_runtime_uses_compiled_development_dll(
    imported, tmp_path
):
    from symbian.toolchain.verification import run_oracles

    report, _, _ = imported
    oracles = Path(os.environ["SYMBIAN_EKA2L1_ORACLES_BUILD"]).resolve()
    source = PROJECT.parents[1] / "probes/dll_probe"
    implementation = toolchain.build(source, tmp_path / "implementation")
    dll = Path(implementation["artifact"])
    checks, _ = run_oracles(
        {
            "SYMBIAN_E32_TEST_IMAGE": Path(report["artifact"]),
            "SYMBIAN_DLL_TEST_IMAGE": dll,
        },
        (("symbian_import_probe", 6),),
        oracles,
        tmp_path / "checks",
    )
    assert checks[0]["passed"]


def test_dll_conversion_preserves_imports_alongside_frozen_exports(
    imported, tmp_path
):
    report, proxy, _ = imported
    # This tests the combined image layout; the executable startup in this input
    # is not a runnable DLL initialization routine.
    image = convert_dll(
        Path(report["linked_elf"]).read_bytes(),
        b"EXPORTS\nmain @ 7 NONAME\n",
        [proxy],
        0xE0000810,
    )
    info = inspect_e32(image)
    assert info.dll
    assert info.imports[0].dll == "probe.dll"
    assert info.imports[0].slots[0].ordinal == 7
    assert len(info.exports) == 7
    assert len(info.code_relocations) >= 7
    assert not info.exports[-1].absent
    if os.environ.get("SYMBIAN_EKA2L1_ORACLES_BUILD"):
        from symbian.toolchain.verification import run_oracles

        artifact = tmp_path / "combined.dll"
        artifact.write_bytes(image)
        checks, _ = run_oracles(
            {"SYMBIAN_E32_TEST_IMAGE": artifact},
            (("symbian_checksum_oracle", 1), ("symbian_validator_oracle", 7)),
            Path(os.environ["SYMBIAN_EKA2L1_ORACLES_BUILD"]).resolve(),
            tmp_path / "checks",
        )
        assert all(check["passed"] for check in checks)


@pytest.mark.skipif(not TOOLS, reason="Clang/LLD required")
def test_internal_pointer_relocations_coexist_with_eager_imports(
    imported, tmp_path
):
    _, _, root = imported
    project = tmp_path / "project"
    shutil.copytree(PROJECT, project)
    (project / "probe.cc").write_text(
        'extern "C" unsigned SymbianProbeTransform(unsigned);\n'
        'extern "C" int InvokeImport() { volatile unsigned input = 16;\n'
        "return SymbianProbeTransform(input) == 0x918U ? 0 : 42; }\n"
        "using Function = int (*)();\n"
        'extern "C" __attribute__((visibility("hidden"))) const Function '
        "functions[] = {InvokeImport};\n"
        "int main() { "
        "const Function* volatile table = functions; "
        "return table[0](); }\n"
    )
    (project / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.28)\n"
        "project(import_probe LANGUAGES CXX ASM)\ninclude(SymbianPic)\n"
        "symbian_add_executable(import_probe probe.cc)\n"
        "target_link_libraries(import_probe PRIVATE "
        f'"{root}/proxy/probe.dso")\n'
    )
    report = toolchain.build(project, tmp_path / "build")
    assert report["e32"]["code_relocations"]
    assert report["e32"]["imports"][0]["slots"][0]["ordinal"] == 7
    image = bytearray(Path(report["artifact"]).read_bytes())
    relocation = struct.unpack_from("<I", image, 112)[0]
    import_slot = report["e32"]["imports"][0]["slots"][0]["code_offset"]
    # Code-pointer relocation must not alias an eager ordinal slot.
    struct.pack_into("<H", image, relocation + 16, 0x1000 | import_slot)
    with pytest.raises(StatusError):
        inspect_e32(bytes(image))
    if os.environ.get("SYMBIAN_EKA2L1_ORACLES_BUILD"):
        from symbian.toolchain.verification import run_oracles

        checks, _ = run_oracles(
            {"SYMBIAN_E32_TEST_IMAGE": Path(report["artifact"])},
            (("symbian_checksum_oracle", 1), ("symbian_validator_oracle", 7)),
            Path(os.environ["SYMBIAN_EKA2L1_ORACLES_BUILD"]).resolve(),
            tmp_path / "checks",
        )
        assert all(check["passed"] for check in checks)
