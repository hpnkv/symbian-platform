#!/usr/bin/env python3
"""Starts ARM GDB and an owned source-built SDK example session."""

import os
import shutil
import sys
from pathlib import Path


def main() -> None:
    """Resolves ARM GDB and delegates its lifetime to the supervisor."""
    root = Path(__file__).resolve().parents[1]
    name = sys.argv[1]
    if name not in ("gl_app", "qt_app_classic", "sdl2_app"):
        sys.exit("Expected gl_app, qt_app_classic or sdl2_app")
    debugger = (
        os.environ.get("SYMBIAN_GDB")
        or shutil.which("arm-none-eabi-gdb")
        or shutil.which("gdb-multiarch")
    )
    if not debugger:
        sys.exit("Install ARM GDB or set SYMBIAN_GDB to its executable.")
    python = root / ".venv/bin/python"
    os.execv(
        str(python),
        [
            str(python),
            "-m",
            "symbian.emulator.launch",
            "--root",
            str(root),
            "--project",
            str(root / "examples" / name),
            "--workspace",
            "--gdb",
            debugger,
            *sys.argv[2:],
        ],
    )


if __name__ == "__main__":
    main()
