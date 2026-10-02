"""CMake graph, incremental header rebuilds and real clangd input evidence."""

import hashlib
import json
import shlex
import shutil
from pathlib import Path

import pytest

from symbian import toolchain
from symbian.status import Code, StatusError

PROJECT = Path(__file__).parents[2] / "examples/e32_probe"
AVAILABLE = all(
    shutil.which(tool) for tool in ("clang++", "ld.lld", "cmake", "ninja")
)
pytestmark = pytest.mark.skipif(
    not AVAILABLE, reason="Clang/LLD/CMake/Ninja required"
)


def test_real_multi_source_header_rebuild_and_persistent_database(tmp_path):
    project = tmp_path / "project with spaces"
    shutil.copytree(PROJECT, project)
    header = project / "value.h"
    header.write_text("constexpr unsigned int kMultiplier = 17U;\n")
    (project / "multiply.cc").write_text(
        '#include "value.h"\n'
        'extern "C" __attribute__((noinline)) unsigned int Multiply(\n'
        "    unsigned int v) {\n"
        "  return v * kMultiplier;\n}\n"
    )
    probe = project / "probe.cc"
    probe.write_text(
        'extern "C" unsigned int Multiply(unsigned int);\n'
        + probe.read_text().replace("value * 17U", "Multiply(value)")
    )
    cmake = project / "CMakeLists.txt"
    cmake.write_text(
        cmake.read_text().replace(
            "SOURCES probe.cc", "SOURCES probe.cc multiply.cc"
        )
    )
    output = tmp_path / "build with spaces"
    first = toolchain.build(project, output)
    assert (
        first["inputs"][str(header)]
        == hashlib.sha256(header.read_bytes()).hexdigest()
    )
    database = json.loads(Path(first["compile_commands"]).read_text())
    assert len(database) == 3
    for entry in database:
        assert Path(entry["directory"]).is_dir()
        assert Path(entry["file"]).is_file()
        assert (Path(entry["directory"]) / entry["output"]).is_file()
        assert "armv5t-none-eabi" in entry["command"]
    header.write_text("constexpr unsigned int kMultiplier = 18U;\n")
    second = toolchain.build(project, output)
    assert first["sha256"] != second["sha256"]
    assert second["reproducible"]
    third = toolchain.build(project, output)
    assert third["sha256"] == second["sha256"]
    assert "no work to do" in third["build_system"]["primary_log"]


def test_missing_cmake_target_is_structured(tmp_path):
    project = tmp_path / "project"
    shutil.copytree(PROJECT, project)
    manifest = project / "symbian.toml"
    manifest.write_text(
        manifest.read_text().replace('"e32_probe"', '"missing"')
    )
    with pytest.raises(StatusError) as caught:
        toolchain.build(project, tmp_path / "out")
    assert caught.value.code == Code.NOT_FOUND


def test_legacy_source_fields_are_not_silently_ignored(tmp_path):
    project = tmp_path / "project"
    shutil.copytree(PROJECT, project)
    manifest = project / "symbian.toml"
    manifest.write_text(
        manifest.read_text().replace(
            "[project]", '[project]\nsource = "probe.cc"'
        )
    )
    with pytest.raises(StatusError) as caught:
        toolchain.build(project, tmp_path / "out")
    assert caught.value.code == Code.INVALID_ARGUMENT


def test_compiler_alias_change_keeps_required_preset_values(tmp_path):
    """CMake's implicit reset must not discard the SDK/preset configuration."""
    project = tmp_path / "project"
    shutil.copytree(PROJECT, project)
    presets = project / "CMakePresets.json"
    settings = json.loads(presets.read_text())
    settings["configurePresets"][0].setdefault("cacheVariables", {})[
        "SYMBIAN_TEST_PRESET_PRESENT"
    ] = "ON"
    presets.write_text(json.dumps(settings))
    cmake = project / "CMakeLists.txt"
    cmake.write_text(
        cmake.read_text() + "\n"
        "if(NOT SYMBIAN_TEST_PRESET_PRESENT)\n"
        '  message(FATAL_ERROR "Preset was discarded")\n'
        "endif()\n"
    )
    compiler = Path(shutil.which("clang++")).absolute()
    alias = tmp_path / "alternate-clang++"
    alias.write_text(
        "#!/bin/sh\nexec " + shlex.quote(str(compiler)) + ' "$@"\n'
    )
    alias.chmod(0o755)
    output = tmp_path / "out"
    first = toolchain.build(project, output, str(compiler))
    second = toolchain.build(project, output, str(alias))
    assert second["reproducible"]
    assert second["sha256"] == first["sha256"]
    assert (
        "SYMBIAN_TEST_PRESET_PRESENT:UNINITIALIZED=ON"
        in (output / "cmake/CMakeCache.txt").read_text()
    )


def test_updated_compiler_at_same_path_discards_old_objects(tmp_path):
    """Driver updates must not mix cached and newly generated guest objects."""
    project = tmp_path / "project"
    shutil.copytree(PROJECT, project)
    probe = project / "probe.cc"
    probe.write_text(probe.read_text().replace("17U", "SDK_MULTIPLIER"))
    compiler = shlex.quote(shutil.which("clang++"))
    driver = tmp_path / "clang++"

    def version(multiplier):
        driver.write_text(
            '#!/bin/sh\nif [ "$1" = "--version" ]; then\n'
            f"  {compiler} --version\n"
            f"  echo 'SDK revision {multiplier}'\n  exit 0\nfi\n"
            f'exec {compiler} -DSDK_MULTIPLIER={multiplier}U "$@"\n'
        )
        driver.chmod(0o755)

    output = tmp_path / "out"
    version(17)
    first = toolchain.build(project, output, str(driver))
    version(18)
    second = toolchain.build(project, output, str(driver))
    assert second["reproducible"]
    assert first["sha256"] != second["sha256"]
