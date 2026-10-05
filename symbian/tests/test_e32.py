"""Real Clang/LLD integration and native E32 boundary behavior."""

import shutil
from pathlib import Path

import pytest

from symbian import toolchain
from symbian._native import inspect_e32
from symbian.e32 import convert_pic_executable, inspect_image
from symbian.status import Code, StatusError

TOOLS_AVAILABLE = shutil.which("clang++") and shutil.which("ld.lld")
PROJECT = Path(__file__).parents[2] / "examples/e32_probe"


@pytest.fixture
def image(tmp_path):
    if not TOOLS_AVAILABLE:
        pytest.skip("Clang and LLD required")
    report = toolchain.build(PROJECT, tmp_path / "output")
    return report, Path(report["artifact"]).read_bytes()


def test_link_conversion_reproducibility_and_limits(image):
    report, data = image
    assert report["reproducible"]
    assert report["elf"]["type"] == 2
    assert report["e32"]["uid3"] == 0xE0000808
    assert report["e32"]["entry_offset"] == 0
    assert report["e32"]["kernel"] == "eka2"
    code_end = report["e32"]["header_size"] + report["e32"]["code_size"]
    # This probe retains its EHABI descriptor and absolute pointer fixups.
    assert code_end <= len(data)
    assert report["e32"]["code_relocations"]
    assert len(data) > code_end
    assert inspect_image(Path(report["artifact"])) == report["e32"]
    assert not report["symbian_loader_verified"]
    assert not report["runtime_verified"]
    # A multi-call LLVM tool must retain ld.lld as argv[0].
    assert Path(report["linker"]["path"]).name == "ld.lld"


def test_native_metadata_readonly_and_status_translation(image):
    _, data = image
    info = inspect_e32(data)
    with pytest.raises(AttributeError):
        info.uid3 = 1
    changed = bytearray(data)
    changed[128] ^= 1
    with pytest.raises(StatusError) as caught:
        inspect_e32(bytes(changed))
    assert caught.value.code == Code.DATA_LOSS


def test_native_converter_rejects_protected_identity(image):
    report, _ = image
    with pytest.raises(StatusError) as caught:
        convert_pic_executable(Path(report["linked_elf"]).read_bytes(), 1)
    assert caught.value.code == Code.INVALID_ARGUMENT


@pytest.mark.skipif(not TOOLS_AVAILABLE, reason="Clang/LLD unavailable")
def test_project_auxiliary_files_cannot_escape(tmp_path):
    project = tmp_path / "project"
    shutil.copytree(PROJECT, project)
    shutil.copyfile(PROJECT / "startup.S", tmp_path / "startup.S")
    cmake = project / "CMakeLists.txt"
    cmake.write_text(cmake.read_text().replace("startup.S", "../startup.S"))
    with pytest.raises(StatusError) as caught:
        toolchain.build(project, tmp_path / "output")
    assert caught.value.code == Code.FAILED_PRECONDITION
    assert "must be a project or selected SDK file" in str(caught.value)


@pytest.mark.skipif(not TOOLS_AVAILABLE, reason="Clang/LLD unavailable")
def test_unaligned_absolute_pointer_rejected_after_real_link(tmp_path):
    project = tmp_path / "project"
    shutil.copytree(PROJECT, project)
    (project / "probe.cc").write_text(
        'extern "C" unsigned int SymbianAbiProbe(unsigned int v) { return v; }'
        '\nextern "C" int ProbeMain() { return 0; }\n'
    )
    with (project / "startup.S").open("a") as stream:
        stream.write(
            '.section .rodata, "a", %progbits\n.byte 0\n.word ProbeMain\n'
        )
    with pytest.raises(StatusError) as caught:
        toolchain.build(project, tmp_path / "output")
    assert caught.value.code == Code.UNIMPLEMENTED


def test_missing_linker_is_structured(tmp_path):
    with pytest.raises(StatusError) as caught:
        toolchain.build(PROJECT, tmp_path, linker="missing-symbian-linker")
    assert caught.value.code == Code.NOT_FOUND
