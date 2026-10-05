"""Formats public C++ examples with the repository clang-format rules."""

from __future__ import annotations

import argparse
import re
import shutil
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FENCES = re.compile(
    r"^(?P<indent>[ \t]*)(?P<fence>`{3,}|~{3,})(?P<language>[^\n]*)\n"
    r"(?P<body>.*?)^(?P=indent)(?P=fence)[ \t]*$",
    re.MULTILINE | re.DOTALL,
)
CPP_LANGUAGES = {"cpp", "c++", "cxx", "cc", "c", "h", "hpp"}


def public_pages() -> list[Path]:
    """Returns public articles, reference pages and the project README."""
    return [
        ROOT / "README.md",
        *(ROOT / "doc/docs").rglob("*.md"),
        *(ROOT / "doc/cpp").rglob("*.md"),
    ]


def main() -> int:
    """Formats examples in place, or reports differences with --check."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--clang-format", default="clang-format")
    args = parser.parse_args()
    formatter = shutil.which(args.clang_format)
    if formatter is None:
        parser.error(f"{args.clang_format} is required (clang-format 15+)")
    changed = 0
    for page in public_pages():
        source = page.read_text()
        pieces = []
        end = 0
        for block in FENCES.finditer(source):
            language = block["language"].strip().split()
            if not language or language[0] not in CPP_LANGUAGES:
                continue
            indent = block["indent"]
            body = "".join(
                line[len(indent) :] if line.startswith(indent) else line
                for line in block["body"].splitlines(keepends=True)
            )
            result = subprocess.run(
                [
                    formatter,
                    "--style=file",
                    "--assume-filename=" + str(ROOT / "documentation.cc"),
                ],
                input=body,
                text=True,
                capture_output=True,
                check=True,
            ).stdout
            formatted = "".join(
                indent + line if line.strip() else line
                for line in result.splitlines(keepends=True)
            )
            pieces.extend([source[end : block.start("body")], formatted])
            end = block.end("body")
        pieces.append(source[end:])
        updated = "".join(pieces)
        if updated != source:
            changed += 1
            print(
                f"{page.relative_to(ROOT)}: "
                + (
                    "C++ snippets need clang-format"
                    if args.check
                    else "formatted"
                )
            )
            if not args.check:
                page.write_text(updated)
    return int(args.check and changed > 0)


if __name__ == "__main__":
    raise SystemExit(main())
