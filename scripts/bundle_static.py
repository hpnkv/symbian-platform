"""Builds a single SDK archive from private static dependency archives."""

import argparse
import subprocess
from pathlib import Path


def main() -> None:
    """Merges archives through LLVM ar's portable MRI command interface."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ar", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--library", type=Path, action="append", required=True)
    args = parser.parse_args()
    for library in args.library:
        if not library.is_file():
            parser.error(f"archive missing: {library}")
        if library.suffix != ".a":
            parser.error(f"static archive required: {library}")
    args.output.unlink(missing_ok=True)
    commands = [f"CREATE {args.output}"]
    commands.extend(f"ADDLIB {library}" for library in args.library)
    commands.extend(("SAVE", "END", ""))
    subprocess.run(
        [str(args.ar), "-M"],
        input="\n".join(commands),
        text=True,
        check=True,
    )
    if not args.output.is_file():
        parser.error(f"archive not created: {args.output}")


if __name__ == "__main__":
    main()
