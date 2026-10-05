"""Formats Doxygen C++ examples without changing native source declarations."""

import re
import shutil
import subprocess
import sys
import textwrap
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
_CODE = re.compile(
    r"^(?P<prefix>[ \t]*(?:\*[ \t]?)?)[@\\]code"
    r"(?:\{(?P<language>[^}]+)\})?[^\n]*\n"
    r"(?P<body>.*?)^[ \t]*(?:\*[ \t]?)?[@\\]endcode",
    re.MULTILINE | re.DOTALL,
)
_CPP = {None, ".cc", ".cpp", ".cxx", ".c", ".h", ".hpp"}


def format_examples(source: str) -> str:
    """Formats code blocks inside documentation comments, preserving prose."""
    formatter = shutil.which("clang-format")
    if formatter is None:
        raise RuntimeError("clang-format is required for Doxygen examples")

    def format_block(match: re.Match[str]) -> str:
        if match["language"] not in _CPP:
            return match.group(0)
        body = match["body"]
        if "*" in match["prefix"]:
            body = re.sub(r"^[ \t]*\* ?", "", body, flags=re.MULTILINE)
        body = textwrap.dedent(body)
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
            match["prefix"] + line for line in result.splitlines(keepends=True)
        )
        return (
            match.group(0)[: match.start("body") - match.start()]
            + formatted
            + match.group(0)[match.end("body") - match.start() :]
        )

    def format_comment(match: re.Match[str]) -> str:
        return _CODE.sub(format_block, match.group(0))

    return re.sub(r"/\*[*!].*?\*/", format_comment, source, flags=re.DOTALL)


if __name__ == "__main__":
    source = (
        Path(sys.argv[1]).read_bytes().decode("utf-8", errors="surrogateescape")
    )
    sys.stdout.buffer.write(
        format_examples(source).encode("utf-8", errors="surrogateescape")
    )
