"""Regenerate native typing declarations through the installed extension."""

import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    with tempfile.TemporaryDirectory(prefix="symbian-stubs-") as temporary:
        subprocess.run(
            [
                sys.executable,
                "-m",
                "pybind11_stubgen",
                "symbian._native",
                "-o",
                temporary,
            ],
            cwd=root,
            check=True,
        )
        output = Path(temporary) / "symbian/_native.pyi"
        subprocess.run(
            [sys.executable, "-m", "black", "--quiet", str(output)],
            cwd=root,
            check=True,
        )
        destination = root / "symbian/_native.pyi"
        if "--check" in sys.argv:
            return 0 if destination.read_bytes() == output.read_bytes() else 1
        shutil.copyfile(output, destination)
    return 0


if __name__ == "__main__":
    sys.exit(main())
