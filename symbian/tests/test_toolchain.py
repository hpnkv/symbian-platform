"""Real ARM compiler experiments with no SDK or target runtime dependency."""

import json
import shutil
from pathlib import Path

import pytest

from symbian import toolchain
from symbian.analysis import inspect_elf
from symbian.status import Code, StatusError


@pytest.mark.skipif(shutil.which("clang++") is None, reason="Clang unavailable")
def test_real_clang_reproducibility_and_compilation_database(tmp_path):
    report = toolchain.probe(tmp_path / "output")
    assert report["reproducible"]
    assert not report["symbian_loader_verified"]
    assert report["artifact_kind"] == "arm-elf-relocatable"
    assert inspect_elf(Path(report["artifact"]))["machine"] == 40
    database = json.loads(Path(report["compile_commands"]).read_text())
    assert "--target=armv6-none-eabi" in database[0]["arguments"]
    assert Path(database[0]["file"]).exists()
    assert Path(report["artifact"]).exists()


@pytest.mark.skipif(shutil.which("clang++") is None, reason="Clang unavailable")
def test_example_build(tmp_path):
    project = Path(__file__).parents[2] / "probes/abi_probe"
    report = toolchain.build(project, tmp_path / "output")
    assert Path(report["artifact"]).name == "abi_probe.o"
    assert report["elf"]["flags"] >> 24 == 5


def test_missing_compiler_has_status(tmp_path):
    with pytest.raises(StatusError) as caught:
        toolchain.probe(tmp_path / "output", "symbian-does-not-exist-clang")
    assert caught.value.code == Code.NOT_FOUND


def test_source_cannot_escape_project(tmp_path):
    project = tmp_path / "project"
    project.mkdir()
    (tmp_path / "outside.cc").write_text("int outside;")
    (project / "symbian.toml").write_text(
        '[project]\nname="test"\nkind="arm-object-experiment"\n'
        'source="../outside.cc"\n'
    )
    with pytest.raises(StatusError) as caught:
        toolchain.build(project, tmp_path / "output")
    assert caught.value.code == Code.INVALID_ARGUMENT


def test_unknown_project_kind_has_no_fake_success(tmp_path):
    (tmp_path / "symbian.toml").write_text('[project]\nkind="application"\n')
    with pytest.raises(StatusError) as caught:
        toolchain.build(tmp_path, tmp_path / "output")
    assert caught.value.code == Code.UNIMPLEMENTED


@pytest.mark.skipif(shutil.which("clang++") is None, reason="Clang unavailable")
def test_build_preserves_local_header_resolution(tmp_path):
    project = tmp_path / "project"
    project.mkdir()
    (project / "symbian.toml").write_text(
        '[project]\nname="header_test"\nkind="arm-object-experiment"\n'
        'source="source.cc"\n'
    )
    (project / "value.h").write_text("constexpr unsigned int kValue = 808;\n")
    (project / "source.cc").write_text(
        '#include "value.h"\n'
        'extern "C" unsigned int Value() { return kValue; }\n'
    )
    assert toolchain.build(project, tmp_path / "output")["reproducible"]
