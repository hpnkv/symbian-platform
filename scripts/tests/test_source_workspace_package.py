"""Source CMake must see the installed native extension."""

import os
import subprocess
import sys
from pathlib import Path


def test_checkout_finds_installed_native_extension(tmp_path):
    wheel_package = tmp_path / "symbian"
    wheel_package.mkdir()
    (wheel_package / "_native.py").write_text("marker = 'installed wheel'\n")
    root = Path(__file__).resolve().parents[2]
    environment = {
        **os.environ,
        "PYTHONPATH": os.pathsep.join((str(root), str(tmp_path))),
    }
    result = subprocess.run(
        [
            sys.executable,
            "-S",
            "-c",
            "import symbian; from symbian import _native; "
            "assert _native.marker == 'installed wheel'; "
            "assert symbian.__file__.startswith(" + repr(str(root)) + ")",
        ],
        cwd=root,
        env=environment,
        capture_output=True,
        text=True,
    )
    assert result.returncode == 0, result.stderr
