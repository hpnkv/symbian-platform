"""One policy for machine-local, per-user SDK assets."""

import os
from pathlib import Path
from typing import Literal

from symbian.status import Code, StatusError

Kind = Literal["config", "data", "cache"]


def _absolute(value: str, name: str) -> Path:
    path = Path(value)
    if not path.is_absolute():
        raise StatusError(Code.INVALID_ARGUMENT, f"{name} must be absolute")
    return path


def asset_directory(kind: Kind) -> Path:
    """Returns an asset root without creating it.

    SYMBIAN_HOME selects one root. Explicit XDG overrides remain supported
    for isolated environments. Otherwise assets live below ~/.symbian.
    """
    if value := os.environ.get("SYMBIAN_HOME"):
        base = _absolute(value, "SYMBIAN_HOME")
    elif value := os.environ.get(f"XDG_{kind.upper()}_HOME"):
        return _absolute(value, f"XDG_{kind.upper()}_HOME") / "symbian"
    else:
        base = Path.home() / ".symbian"
    return base if kind == "data" else base / kind
