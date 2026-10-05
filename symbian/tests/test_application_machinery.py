"""Real SDK application entry, library dependency and Qt data-import builds."""

import json
import os
import shutil
import struct
from pathlib import Path

import pytest

from symbian.e32 import convert_imported_executable, inspect_image
from symbian.process import run
from symbian.project.sdk import AppSdk
from symbian.sdk import inspect_proxy
from symbian.status import StatusError
from symbian.toolchain import build

SDK = os.environ.get("SYMBIAN_APP_SDK")
pytestmark = pytest.mark.skipif(not SDK, reason="Set SYMBIAN_APP_SDK")


@pytest.fixture
def sdk():
    return AppSdk.load(Path(SDK))


def configure(sdk, project, tree, architecture):
    run(
        [
            str(sdk.prefix / "bin/cmake"),
            "-G",
            "Ninja",
            "-S",
            str(project),
            "-B",
            str(tree),
            f"-DCMAKE_TOOLCHAIN_FILE={sdk.prefix}/cmake/symbian-arm.cmake",
            f"-DSYMBIAN_SDK_PREFIX={sdk.prefix}",
            f"-DSYMBIAN_TARGET_ARCH={architecture}",
        ],
        cwd=project,
    )
    run([str(sdk.prefix / "bin/cmake"), "--build", str(tree)], cwd=project)


@pytest.mark.parametrize("architecture", ["armv5t", "armv6"])
@pytest.mark.parametrize(
    "signature", ["int main()", "int main(int argc, char** argv)"]
)
def test_standard_entry_and_automatic_transitive_imports(
    sdk, tmp_path, architecture, signature
):
    source = tmp_path / "application"
    source.mkdir()
    (source / "main.cc").write_text(
        "#include <string>\n"
        'const std::string label = "application";\n'
        + signature
        + " { return label.size() == 11 ? 0 : 1; }\n"
    )
    (source / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.28)\n"
        "project(application LANGUAGES CXX ASM)\ninclude(SymbianApp)\n"
        "symbian_add_executable(application main.cc)\n"
        "target_link_libraries(application PRIVATE Symbian::Runtime)\n"
        "symbian_publish_executable(application UID3 0xe0000825)\n"
    )
    tree = tmp_path / "build"
    configure(sdk, source, tree, architecture)
    image = inspect_image(tree / "e32/application.exe")
    assert image["architecture"] == architecture
    assert any(item["dll"] == "euser.dll" for item in image["imports"])
    assert sorted(path.name for path in source.iterdir()) == [
        "CMakeLists.txt",
        "main.cc",
    ]


@pytest.mark.parametrize("architecture", ["armv5t", "armv6"])
def test_dynamic_library_is_a_normal_linkable_target(
    sdk, tmp_path, architecture
):
    (tmp_path / "answer.cc").write_text("int Answer() { return 42; }\n")
    (tmp_path / "main.cc").write_text(
        "int Answer();\nint main() { return Answer() == 42 ? 0 : 1; }\n"
    )
    (tmp_path / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.28)\n"
        "project(application LANGUAGES CXX ASM)\ninclude(SymbianApp)\n"
        "symbian_add_dynamic_library(answer SOURCES answer.cc\n"
        "  UID3 0xe0000826)\n"
        "target_compile_definitions(answer PRIVATE APP_LIBRARY=1)\n"
        "target_include_directories(answer PRIVATE\n"
        "  ${CMAKE_CURRENT_SOURCE_DIR})\n"
        "symbian_add_executable(application main.cc)\n"
        "target_link_libraries(application PRIVATE Symbian::Runtime answer)\n"
        "symbian_publish_executable(application UID3 0xe0000827)\n"
    )
    tree = tmp_path / "build"
    configure(sdk, tmp_path, tree, architecture)
    library = inspect_image(tree / "answer.dll")
    assert library["dll"] and library["architecture"] == architecture
    assert (tree / "answer_elf.elf").is_file()
    assert (
        tree / "answer.def"
    ).read_text() == "EXPORTS\n_Z6Answerv @ 1 NONAME\n"
    assert inspect_proxy(tree / "answer.dso")["exports"][0]["ordinal"] == 1
    assert any(
        item["dll"] == "answer.dll"
        for item in inspect_image(tree / "e32/application.exe")["imports"]
    )
    assert not list(tmp_path.glob("*.def"))


def test_mixed_workspace_inherits_application_module_and_keeps_host_linker(
    sdk, tmp_path
):
    guest = tmp_path / "guest"
    guest.mkdir()
    (guest / "sdk-location.json").write_text(
        json.dumps({"sdk": str(sdk.prefix)})
    )
    (guest / "main.cc").write_text("int main() { return 0; }\n")
    (guest / "CMakeLists.txt").write_text(
        "include(SymbianApp)\n"
        "symbian_add_executable(guest main.cc)\n"
        "target_link_libraries(guest PRIVATE Symbian::Runtime)\n"
        "symbian_publish_executable(guest UID3 0xe0000828)\n"
    )
    (tmp_path / "host.cc").write_text("int main() { return 0; }\n")
    (tmp_path / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.28)\n"
        "project(workspace LANGUAGES CXX)\n"
        "set(host_linker ${CMAKE_LINKER})\n"
        f'include("{sdk.prefix}/cmake/SymbianSdk.cmake")\n'
        "symbian_add_guest_subdirectory(guest)\n"
        "if(NOT CMAKE_LINKER STREQUAL host_linker)\n"
        '  message(FATAL_ERROR "Guest setup changed the host linker")\n'
        "endif()\nadd_executable(host host.cc)\n"
    )
    tree = tmp_path / "build"
    run(
        ["cmake", "-G", "Ninja", "-S", str(tmp_path), "-B", str(tree)],
        cwd=tmp_path,
    )
    run(
        ["cmake", "--build", str(tree), "--target", "host", "guest_e32"],
        cwd=tmp_path,
    )
    run([str(tree / "host")], cwd=tmp_path)
    assert (
        inspect_image(tree / "guest/e32/guest.exe")["architecture"] == "armv6"
    )


@pytest.mark.parametrize("architecture", ["armv5t", "armv6"])
def test_qt_complete_library_includes_data_and_rejects_malformed_slots(
    sdk, tmp_path, architecture, monkeypatch
):
    monkeypatch.setenv(
        "PATH", str(sdk.compiler.parent) + os.pathsep + os.environ["PATH"]
    )
    source = tmp_path / "application"
    shutil.copytree(
        Path(__file__).parents[2] / "examples/qt_app",
        source,
        ignore=shutil.ignore_patterns(".symbian", "sdk-location.json"),
    )
    (source / "sdk-location.json").write_text(
        json.dumps({"sdk": str(sdk.prefix)})
    )
    report = build(
        source,
        tmp_path / "output",
        str(sdk.compiler),
        str(sdk.linker),
        architecture=architecture,
    )
    assert report["reproducible"]
    assert not list(source.glob("*.S")) and not list(source.glob("*.ld"))
    proxy = inspect_proxy(sdk.prefix / "proxies/qtcore/qtcore.dso")
    assert len(proxy["exports"]) > 4000
    item = next(
        e
        for e in proxy["exports"]
        if e["symbol"] == "_ZN10QByteArray11shared_nullE"
    )
    assert item["data"]
    proxies = [
        Path(p).read_bytes() for p in report["inputs"] if p.endswith(".dso")
    ]
    elf = bytearray(Path(report["linked_elf"]).read_bytes())
    section_start = struct.unpack_from("<I", elf, 32)[0]
    stride, count = struct.unpack_from("<HH", elf, 46)
    dynamic = None
    for i in range(count):
        header = section_start + i * stride
        kind, offset, size, flags = (
            struct.unpack_from("<I", elf, header + field)[0]
            for field in (4, 16, 20, 8)
        )
        if kind == 9 and flags == 2 and size:
            dynamic = offset
    assert dynamic is not None
    info = struct.unpack_from("<I", elf, dynamic + 4)[0]
    assert info & 255 == 21  # R_ARM_GLOB_DAT
    malformed = bytearray(elf)
    struct.pack_into("<I", malformed, dynamic, 0x7FFFFFFC)
    with pytest.raises(StatusError):
        convert_imported_executable(bytes(malformed), proxies, 0xE0000821)
    malformed = bytearray(elf)
    struct.pack_into("<I", malformed, dynamic + 4, (info & ~255) | 20)
    with pytest.raises(StatusError):
        convert_imported_executable(bytes(malformed), proxies, 0xE0000821)
