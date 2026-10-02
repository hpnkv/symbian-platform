"""Test helper for the CLI's explicit machine-readable output mode."""

from symbian.cli.__main__ import main as _main


def main(arguments: list[str]) -> int:
    """Runs the CLI with its canonical JSON output selected."""
    return _main(["--output-format=json", *map(str, arguments)])
