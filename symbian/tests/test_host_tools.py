"""Host LLVM discovery for SDK exports on macOS and Linux."""

import pytest

from symbian.emulator.background import background_environment
from symbian.project.sdk import _emulator_binary
from symbian.status import Code, StatusError
from symbian.toolchain.host_tools import llvm_tool


def test_linux_llvm_selection_keeps_matching_compilers_and_driver_name(
    tmp_path, monkeypatch
):
    bin_dir = tmp_path / "llvm bin"
    bin_dir.mkdir()
    for name in ("clang++", "clang", "ld.lld", "llvm-ar", "llvm-ranlib"):
        tool = bin_dir / name
        tool.write_text("#!/bin/sh\nexit 0\n")
        tool.chmod(0o755)
    monkeypatch.setattr(
        "symbian.toolchain.host_tools.platform.system", lambda: "Linux"
    )
    monkeypatch.setenv("PATH", str(bin_dir))
    monkeypatch.delenv("SYMBIAN_LLVM_BIN", raising=False)

    compiler = llvm_tool("clang++")
    assert compiler == bin_dir / "clang++"
    assert llvm_tool("clang", sibling=compiler.parent) == bin_dir / "clang"
    assert llvm_tool("ld.lld", sibling=compiler.parent).name == "ld.lld"


def test_explicit_llvm_directory_fails_when_incomplete(tmp_path, monkeypatch):
    monkeypatch.setenv("SYMBIAN_LLVM_BIN", str(tmp_path))
    with pytest.raises(StatusError) as failure:
        llvm_tool("clang++")
    assert failure.value.code == Code.NOT_FOUND
    assert "SYMBIAN_LLVM_BIN" in str(failure.value)


def test_linux_frontend_path_and_focus_settings(tmp_path, monkeypatch):
    monkeypatch.setattr("symbian.project.sdk.platform.system", lambda: "Linux")
    assert _emulator_binary(tmp_path) == tmp_path / "build/eka2l1/bin/eka2l1_qt"
    monkeypatch.setattr("symbian.emulator.background.sys.platform", "linux")
    assert background_environment() == {}
