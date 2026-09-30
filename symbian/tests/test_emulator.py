"""Optional native EKA2L1 smoke tests; these do not boot a Symbian image."""

import os
import subprocess
from pathlib import Path

import pytest

EXECUTABLE = os.environ.get("SYMBIAN_EKA2L1_EXECUTABLE")
pytestmark = pytest.mark.skipif(
    not EXECUTABLE, reason="Set SYMBIAN_EKA2L1_EXECUTABLE to the patched build"
)


def _run(root: Path | str, cwd: Path, option: str = "--help"):
    env = os.environ.copy()
    env["EKA2L1_DATA_ROOT"] = str(root)
    return subprocess.run(
        [EXECUTABLE, option],
        cwd=cwd,
        env=env,
        capture_output=True,
        text=True,
        timeout=15,
        check=False,
    )


def test_help_without_rom_and_independent_settings_roots(tmp_path):
    first = tmp_path / "a"
    second = tmp_path / "b"
    first_settings = first / "settings/EKA2L1/EKA2L1.ini"
    first_settings.parent.mkdir(parents=True)
    first_settings.write_text(
        "[General]\nactiveUILanguage=fr_FR\n[Isolation]\nmarker=first\n"
    )
    original = first_settings.read_bytes()
    for root in (first, second):
        result = _run(root, tmp_path)
        assert result.returncode == 0, result.stderr
        assert "--install" in result.stdout
        assert (root / "resources").is_dir()
        assert (root / "patch").is_dir()
    assert first_settings.read_bytes() == original
    second_settings = second / "settings/EKA2L1/EKA2L1.ini"
    assert second_settings.exists()
    assert b"marker=first" not in second_settings.read_bytes()
    assert not (tmp_path / "config.yml").exists()


def test_relative_root_is_rejected_before_emulator_writes(tmp_path):
    result = _run("relative-root", tmp_path)
    assert result.returncode == 2
    assert "absolute directory" in result.stderr
    assert not (tmp_path / "relative-root").exists()


def test_file_cannot_be_used_as_instance_directory(tmp_path):
    path = tmp_path / "file"
    path.write_bytes(b"original")
    result = _run(path, tmp_path)
    assert result.returncode == 2
    assert path.read_bytes() == b"original"


def test_cli_error_without_devices_shuts_down_workers(tmp_path):
    root = tmp_path / "instance"
    result = _run(root, tmp_path, "--unknown-symbian-option")
    assert result.returncode != 0
    assert result.returncode > 0  # Normal failure exit, not abort or signal.
    assert "unknown-symbian-option" in result.stdout
    assert (root / "EKA2L1.log").is_file()
