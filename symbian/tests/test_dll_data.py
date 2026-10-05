"""Writable E32 DLL data through native conversion and an independent loader."""

import os
import shutil
from pathlib import Path

import pytest

from symbian import toolchain
from symbian.e32 import inspect_image
from symbian.sdk import build_import_proxy
from symbian.toolchain.verification import run_oracles

ROOT = Path(__file__).parents[2]
DLL_PROJECT = ROOT / "probes/dll_data_probe"
CLIENT_PROJECT = ROOT / "probes/import_probe"
COMPILER = os.environ.get("SYMBIAN_RUNTIME_COMPILER") or (
    "/opt/homebrew/opt/llvm/bin/clang++"
    if Path("/opt/homebrew/opt/llvm/bin/clang++").is_file()
    else shutil.which("clang++")
)
LINKER = os.environ.get("SYMBIAN_RUNTIME_LINKER") or (
    "/opt/homebrew/bin/ld.lld"
    if Path("/opt/homebrew/bin/ld.lld").is_file()
    else shutil.which("ld.lld")
)


@pytest.fixture(scope="module")
def dll_and_client(tmp_path_factory):
    if not COMPILER or not LINKER:
        pytest.skip("ARM-capable Clang and LLD are required")
    root = tmp_path_factory.mktemp("Writable DLL and client with spaces")
    dll = toolchain.build(DLL_PROJECT, root / "dll", COMPILER, LINKER)
    proxy = build_import_proxy(
        root / "dll/cmake/probe.def",
        ["SymbianProbeTransform"],
        "probe.dll",
        root / "proxy",
        COMPILER,
        LINKER,
    )
    client = root / "client"
    shutil.copytree(CLIENT_PROJECT, client)
    (client / "probe.cc").write_text(
        'extern "C" unsigned SymbianProbeTransform(unsigned);\n'
        "int main() {\n"
        "  volatile unsigned input = 16;\n"
        "  unsigned first = SymbianProbeTransform(input);\n"
        "  unsigned second = SymbianProbeTransform(input);\n"
        "  return first == 0x918U && second == 0x919U ? 0 : 42;\n"
        "}\n"
    )
    (client / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.28)\n"
        "project(import_probe LANGUAGES CXX ASM)\ninclude(SymbianPic)\n"
        "symbian_add_executable(import_probe probe.cc)\n"
        f'target_link_libraries(import_probe PRIVATE "{proxy["artifact"]}")\n'
    )
    app = toolchain.build(client, root / "app", COMPILER, LINKER)
    return dll, proxy, app


def test_dll_data_and_client_have_reproducible_contracts(dll_and_client):
    dll, proxy, app = dll_and_client
    assert dll["reproducible"] and app["reproducible"]
    image = inspect_image(Path(dll["artifact"]))
    assert image["dll"]
    assert image["data_size"] == 4
    assert image["bss_size"] == 4
    assert len(image["code_data_relocations"]) == 2
    assert image["data_relocations"] == []
    assert image["exports"][-1]["ordinal"] == 1
    assert app["e32"]["imports"][0]["dll"] == "probe.dll"
    assert Path(proxy["artifact"]).is_file()


@pytest.mark.skipif(
    not os.environ.get("SYMBIAN_EKA2L1_ORACLES_BUILD"),
    reason="Set SYMBIAN_EKA2L1_ORACLES_BUILD for DLL data execution",
)
def test_dll_data_executes_and_resets_on_both_backends(
    dll_and_client, tmp_path
):
    dll, _, app = dll_and_client
    checks, _ = run_oracles(
        {
            "SYMBIAN_E32_TEST_IMAGE": Path(app["artifact"]),
            "SYMBIAN_DLL_TEST_IMAGE": Path(dll["artifact"]),
        },
        (("symbian_import_data_probe", 4),),
        Path(os.environ["SYMBIAN_EKA2L1_ORACLES_BUILD"]).resolve(),
        tmp_path / "checks",
    )
    assert all(check["passed"] for check in checks)
