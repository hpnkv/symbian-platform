"""Bounded host process execution without shell interpretation."""

import subprocess
from pathlib import Path

from symbian.status import Code, StatusError


def run(argv: list[str], *, cwd: Path, timeout: float = 30) -> str:
    """Runs a host tool and returns UTF-8 output or a structured failure."""
    try:
        result = subprocess.run(
            argv,
            cwd=cwd,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=timeout,
            check=False,
        )
    except subprocess.TimeoutExpired as error:
        raise StatusError(
            Code.DEADLINE_EXCEEDED, f"Tool timed out: {argv[0]}"
        ) from error
    except FileNotFoundError as error:
        raise StatusError(
            Code.NOT_FOUND, f"Tool not found: {argv[0]}"
        ) from error
    if result.returncode != 0:
        details = (result.stderr or result.stdout)[-4000:].strip()
        raise StatusError(
            Code.FAILED_PRECONDITION,
            f"Tool exited {result.returncode}: {argv[0]}: {details}",
        )
    return result.stdout.strip()
