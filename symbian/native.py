"""Access to the native boundary; doctor avoids importing this module."""

import importlib
from types import ModuleType

from symbian.status import Code, StatusError


def require_native() -> ModuleType:
    """Returns the extension or a canonical installation error."""
    try:
        return importlib.import_module("symbian._native")
    except ImportError as error:
        raise StatusError(
            Code.FAILED_PRECONDITION,
            "Native extension is missing; run uv sync first",
        ) from error
