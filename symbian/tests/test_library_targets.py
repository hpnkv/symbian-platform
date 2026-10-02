"""Installed SDK archives link into independently converted applications."""

import json
import os
import shutil
from pathlib import Path

import pytest

from symbian import toolchain
from symbian.e32 import convert_imported_executable, inspect_image
from symbian.process import run
from symbian.project.configuration import Preferences
from symbian.project.generate import generate
from symbian.project.sdk import AppSdk
from symbian.sdk import inspect_proxy


@pytest.mark.skipif(
    not os.environ.get("SYMBIAN_APP_SDK"),
    reason="Set SYMBIAN_APP_SDK to a prepared SDK manifest",
)
@pytest.mark.parametrize("architecture,cpu", [("armv5t", 3), ("armv6", 6)])
def test_static_archive_links_with_debug_symbols(tmp_path, architecture, cpu):
    sdk = AppSdk.load(Path(os.environ["SYMBIAN_APP_SDK"]))
    project = tmp_path / "library-user"
    generate(
        project,
        Preferences(
            name="library_user",
            ide="none",
            architecture=architecture,
            uid3=0xE0000A11,
        ),
        sdk,
    )
    (project / "year.cc").write_text(
        'extern "C" int LibraryYear(int year) { return year; }\n'
    )
    model = project / "model.cc"
    model.write_text(
        model.read_text()
        .replace(
            "namespace {\n",
            'extern "C" int LibraryYear(int year);\n\nnamespace {\n',
            1,
        )
        .replace("now.year / 100", "LibraryYear(now.year) / 100", 1)
    )
    cmake = project / "CMakeLists.txt"
    cmake.write_text(
        cmake.read_text()
        + "\nsymbian_add_static_library(year SOURCES year.cc)\n"
        + "target_link_libraries(library_user PRIVATE year)\n"
    )
    report = toolchain.build(
        project,
        tmp_path / "build",
        str(sdk.compiler),
        str(sdk.linker),
    )
    assert report["e32"]["architecture"] == architecture
    assert report["elf"]["arm_attributes"]["cpu_arch"] == cpu
    archive = tmp_path / "build/cmake/libyear.a"
    assert archive.is_file()
    database = json.loads(Path(report["compile_commands"]).read_text())
    assert any(Path(item["file"]).name == "year.cc" for item in database)
    nm = shutil.which("llvm-nm") or "/opt/homebrew/opt/llvm/bin/llvm-nm"
    symbols = run([nm, str(report["linked_elf"])], cwd=project)
    assert "LibraryYear" in symbols
    dwarfdump = (
        shutil.which("llvm-dwarfdump")
        or "/opt/homebrew/opt/llvm/bin/llvm-dwarfdump"
    )
    dwarf = run(
        [dwarfdump, "--name=LibraryYear", str(report["linked_elf"])],
        cwd=project,
    )
    assert "LibraryYear" in dwarf


@pytest.mark.skipif(
    not os.environ.get("SYMBIAN_APP_SDK"),
    reason="Set SYMBIAN_APP_SDK to a prepared SDK manifest",
)
@pytest.mark.parametrize("architecture", ["armv5t", "armv6"])
def test_c_dynamic_library_publishes_elf_dll_and_proxy(tmp_path, architecture):
    """CMake's C target publishes a frozen DLL and consumer ordinal proxy."""
    sdk = AppSdk.load(Path(os.environ["SYMBIAN_APP_SDK"]))
    assert sdk.c_compiler is not None
    project = tmp_path / "C DLL source"
    shutil.copytree(
        Path(__file__).parents[2] / "examples/dll_data_probe", project
    )
    (project / "probe.c").write_text(
        "volatile unsigned SymbianProbeSeed = 0x808U;\n"
        "volatile unsigned SymbianProbeCalls;\n"
        "unsigned SymbianProbeTransform(unsigned input) {\n"
        "  return SymbianProbeSeed + input + ++SymbianProbeCalls;\n"
        "}\n"
    )
    shutil.copyfile(
        Path(__file__).parents[2] / "examples/import_probe/image.ld",
        project / "import.ld",
    )
    (project / "client.c").write_text(
        "extern unsigned SymbianProbeTransform(unsigned);\n"
        "unsigned Client(void) { return SymbianProbeTransform(1U); }\n"
    )
    (project / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.28)\n"
        "project(probe LANGUAGES C ASM)\n"
        "include(SymbianPic)\n"
        "symbian_add_dynamic_library(probe STARTUP startup.S "
        "LINKER_SCRIPT image.ld SOURCES probe.c "
        "EXPORT_DEFINITION exports.def UID3 0xE0000D17 "
        "IMPORT_SYMBOLS SymbianProbeTransform)\n"
        "symbian_add_pic_executable(client_elf STARTUP startup.S "
        "LINKER_SCRIPT import.ld SOURCES client.c)\n"
        "target_link_options(client_elf PRIVATE --hash-style=sysv "
        "--no-dynamic-linker)\n"
        "target_link_libraries(client_elf PRIVATE probe_import)\n"
    )
    build = tmp_path / "C DLL build"
    run(
        [
            "cmake",
            "-S",
            str(project),
            "-B",
            str(build),
            "-G",
            "Ninja",
            f"-DCMAKE_TOOLCHAIN_FILE={sdk.prefix}/cmake/symbian-arm.cmake",
            f"-DSYMBIAN_SDK_PREFIX={sdk.prefix}",
            f"-DSYMBIAN_TARGET_ARCH={architecture}",
        ],
        cwd=project,
    )
    run(
        ["cmake", "--build", str(build), "--target", "probe_proxy"], cwd=project
    )
    run(["cmake", "--build", str(build)], cwd=project)
    image = inspect_image(build / "probe.dll")
    assert image["architecture"] == architecture
    assert image["data_size"] == image["bss_size"] == 4
    assert image["exports"][-1]["ordinal"] == 7
    assert (
        inspect_proxy(build / "probe-import/probe.dso")["target_dll"]
        == "probe.dll"
    )
    client = convert_imported_executable(
        (build / "client_elf.elf").read_bytes(),
        [(build / "probe-import/probe.dso").read_bytes()],
        0xE0000D19,
    )
    (build / "client.exe").write_bytes(client)
    assert (
        inspect_image(build / "client.exe")["imports"][0]["dll"] == "probe.dll"
    )
    assert (build / "probe_elf.elf").is_file()
    database = json.loads((build / "compile_commands.json").read_text())
    assert any(Path(item["file"]).name == "probe.c" for item in database)
    assert str(sdk.c_compiler) in next(
        item["command"]
        for item in database
        if Path(item["file"]).name == "probe.c"
    )
