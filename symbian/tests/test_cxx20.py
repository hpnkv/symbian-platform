"""C++20 language contracts and a separately configured libc++ experiment."""

import json
import os
import shlex
import shutil
import subprocess
from pathlib import Path

import pytest

from symbian import packaging, toolchain
from symbian.packaging.verification import verify_package
from symbian.toolchain.verification import verify_pointers

PROJECT = Path(__file__).parents[2] / "examples/cxx20_probe"
TOOLS = shutil.which("clang++") and shutil.which("ld.lld")
LIBCXX_INCLUDE = os.environ.get("SYMBIAN_CXX20_LIBCXX_INCLUDE")
LIBCXX_CONFIG = os.environ.get("SYMBIAN_CXX20_LIBCXX_CONFIG_INCLUDE")
ORACLES = os.environ.get("SYMBIAN_EKA2L1_ORACLES_BUILD")


@pytest.fixture(scope="module")
def cxx20(tmp_path_factory):
    if not TOOLS:
        pytest.skip("Clang/LLD required")
    output = tmp_path_factory.mktemp("C++20 output with spaces")
    return toolchain.build(PROJECT, output)


def _syntax_check(report, source, standard="c++20"):
    """Uses the actual target compilation command with a replacement source."""
    database = json.loads(Path(report["compile_commands"]).read_text())
    row = next(row for row in database if row["file"].endswith("transform.cc"))
    arguments = iter(shlex.split(row["command"]))
    command = []
    for argument in arguments:
        if argument == "-o":
            next(arguments)
        elif argument == "-c" or argument == row["file"]:
            continue
        elif argument.startswith("-std="):
            command.append(f"-std={standard}")
        else:
            command.append(argument)
    command.extend(
        ["-fsyntax-only", "-I", str(PROJECT), str(source), "-pedantic-errors"]
    )
    return subprocess.run(
        command,
        cwd=row["directory"],
        capture_output=True,
        text=True,
        timeout=15,
    )


def test_cxx20_language_build_has_real_compiler_and_loader_inputs(cxx20):
    assert cxx20["reproducible"]
    assert not cxx20["runtime_verified"]
    assert not cxx20["symbian_loader_verified"]
    assert len(cxx20["e32"]["code_relocations"]) == 4
    assert not cxx20["e32"]["imports"]
    assert str(PROJECT / "probe.h") in cxx20["inputs"]
    database = json.loads(Path(cxx20["compile_commands"]).read_text())
    for row in database:
        if row["file"].endswith(".cc"):
            arguments = shlex.split(row["command"])
            assert "-std=c++20" in arguments
            assert "-fno-exceptions" in arguments
            assert "-nostdinc" in arguments


@pytest.mark.parametrize(
    ("standard", "body", "diagnostic"),
    [
        ("c++17", "", "requires C++20"),
        (
            "c++20",
            "unsigned int Reject() { return Transform<kParameters>(-1); }",
            "constraints not satisfied",
        ),
        (
            "c++20",
            "extern Callback Dynamic();\n"
            "constinit const Callback invalid = Dynamic();",
            "constant initializer",
        ),
    ],
)
def test_feature_controls_fail_for_the_expected_reason(
    cxx20, tmp_path, standard, body, diagnostic
):
    source = tmp_path / "control.cc"
    source.write_text('#include "probe.h"\n', encoding="utf-8")
    positive = _syntax_check(cxx20, source)
    assert positive.returncode == 0, positive.stderr
    source.write_text('#include "probe.h"\n' + body + "\n", encoding="utf-8")
    negative = _syntax_check(cxx20, source, standard)
    assert negative.returncode != 0
    assert diagnostic in negative.stderr


@pytest.fixture(scope="module")
def libcxx(tmp_path_factory):
    if not TOOLS or not LIBCXX_INCLUDE or not LIBCXX_CONFIG:
        pytest.skip("Supply the external libc++ header/config experiment paths")
    root = tmp_path_factory.mktemp("Target libc++ header experiment")
    project = root / "project"
    shutil.copytree(
        PROJECT, project, ignore=shutil.ignore_patterns("CMakeUserPresets.json")
    )
    manifest = project / "symbian.toml"
    manifest.write_text(
        manifest.read_text().replace(
            'cmake_preset = "symbian-pic"',
            'cmake_preset = "symbian-libcxx"',
        ),
        encoding="utf-8",
    )
    configuration = {
        "SYMBIAN_CXX20_USE_LIBCXX": "ON",
        "SYMBIAN_CXX20_LIBCXX_INCLUDE": LIBCXX_INCLUDE,
        "SYMBIAN_CXX20_LIBCXX_CONFIG_INCLUDE": LIBCXX_CONFIG,
    }
    (project / "CMakeUserPresets.json").write_text(
        json.dumps(
            {
                "version": 6,
                "configurePresets": [
                    {
                        "name": "symbian-libcxx",
                        "inherits": "symbian-pic",
                        "cacheVariables": configuration,
                    }
                ],
            },
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )
    return toolchain.build(project, root / "build"), project


def test_target_header_subset_retains_dependencies_and_distinct_code(
    cxx20, libcxx
):
    report, _ = libcxx
    assert report["reproducible"]
    assert report["sha256"] != cxx20["sha256"]
    assert not report["e32"]["imports"]
    assert len(report["e32"]["code_relocations"]) == 4
    configuration = (Path(LIBCXX_CONFIG) / "__config_site").resolve()
    assert str(configuration) in report["inputs"]
    for header in ("bit", "concepts", "span"):
        assert (
            str((Path(LIBCXX_INCLUDE) / header).resolve()) in report["inputs"]
        )


@pytest.mark.skipif(not ORACLES, reason="Set SYMBIAN_EKA2L1_ORACLES_BUILD")
@pytest.mark.parametrize("variant", ["language", "libcxx"])
def test_original_consumers_install_and_execute_cxx20_variants(
    cxx20, request, tmp_path, variant
):
    if variant == "libcxx":
        report, project = request.getfixturevalue("libcxx")
    else:
        report, project = cxx20, PROJECT
    package = packaging.package(
        project, Path(report["artifact"]), tmp_path / "package"
    )
    verification = verify_pointers(
        Path(report["artifact"]),
        Path(ORACLES),
        tmp_path / "checks",
        Path(package["artifact"]),
    )
    assert verification["tests_passed"] == 21
    assert verification["cpu_backends"] == ["dyncom", "dynarmic"]
    assert verification["mapped_pointer_words_verified"]
    assert verification["indirect_calls_verified"]
    assert verification["eka2l1_install_launch_verified"]
    assert verification["registry_reload_verified"]
    assert verification["uninstall_reinstall_verified"]
    assert not verification["runtime_verified"]
    assert not verification["symbian_loader_verified"]
    assert not verification["physical_installation_verified"]


MODULE_PROJECT = PROJECT.parent / "cxx20_module_probe"
MODULE_COMPILER = os.environ.get("SYMBIAN_CXX20_MODULE_COMPILER")


@pytest.fixture(scope="module")
def modules(tmp_path_factory):
    if not TOOLS or not MODULE_COMPILER:
        pytest.skip("Supply SYMBIAN_CXX20_MODULE_COMPILER with upstream Clang")
    root = tmp_path_factory.mktemp("C++20 module project with spaces")
    project = root / "project"
    shutil.copytree(MODULE_PROJECT, project)
    return toolchain.build(project, root / "build", MODULE_COMPILER), project


def test_cmake_scans_module_sources_and_retains_actual_bmi(modules):
    report, project = modules
    assert report["reproducible"]
    assert not report["runtime_verified"]
    assert not report["e32"]["imports"]
    assert not report["e32"]["code_relocations"]
    assert str(project / "transform.cppm") in report["inputs"]
    assert "Scanning" in report["build_system"]["primary_log"]
    tree = Path(report["build_system"]["tree"])
    assert list(tree.rglob("*.pcm"))
    database = json.loads(Path(report["compile_commands"]).read_text())
    module = next(row for row in database if row["file"].endswith(".cppm"))
    assert "-fno-exceptions" in shlex.split(module["command"])
    assert "-std=c++20" in shlex.split(module["command"])


@pytest.mark.skipif(not ORACLES, reason="Set SYMBIAN_EKA2L1_ORACLES_BUILD")
def test_module_package_runs_all_original_consumers(modules, tmp_path):
    report, project = modules
    package = packaging.package(
        project, Path(report["artifact"]), tmp_path / "package"
    )
    verification = verify_package(
        Path(package["artifact"]),
        Path(report["artifact"]),
        Path(ORACLES),
        tmp_path / "checks",
    )
    assert verification["tests_passed"] == 35
    assert verification["eka2l1_install_launch_verified"]
    assert verification["image_verification"]["cpu_probe_verified"]
    assert verification["image_verification"]["eka2l1_process_verified"]
    assert not verification["runtime_verified"]
    assert not verification["phone_installation_verified"]


def test_module_constant_edit_rebuilds_its_unchanged_importer(
    modules, tmp_path
):
    _, original = modules
    project = tmp_path / "project"
    shutil.copytree(original, project)
    first = toolchain.build(project, tmp_path / "build", MODULE_COMPILER)
    importer = (project / "probe.cc").read_bytes()
    module = project / "transform.cppm"
    module.write_text(
        module.read_text().replace("17U", "19U").replace("0x918U", "0x938U"),
        encoding="utf-8",
    )
    second = toolchain.build(project, tmp_path / "build", MODULE_COMPILER)
    assert second["sha256"] != first["sha256"]
    assert (project / "probe.cc").read_bytes() == importer
    assert first["inputs"][str(module)] != second["inputs"][str(module)]
    assert "probe.cc.obj" in second["build_system"]["primary_log"]
    assert second["reproducible"]
