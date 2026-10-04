"""Discover host LLVM tools without assuming a platform package manager."""

import os
import platform
import shutil
from pathlib import Path

from symbian.status import Code, StatusError


def llvm_tool(name: str, *, sibling: Path | None = None) -> Path:
    """Finds an LLVM executable, preserving its driver-selecting filename.

    Args:
        name: Executable name, such as ``clang++`` or ``ld.lld``.
        sibling: Directory of a previously selected LLVM compiler. Used to
            keep C, C++ and archive tools in the same toolchain when possible.

    Returns:
        Absolute executable path without resolving a multicall symlink.

    Raises:
        StatusError: The selected tool is unavailable.
    """
    override = os.environ.get("SYMBIAN_LLVM_BIN")
    if override:
        candidate = Path(override).expanduser().absolute() / name
        if candidate.is_file() and os.access(candidate, os.X_OK):
            return candidate
        raise StatusError(
            Code.NOT_FOUND, f"SYMBIAN_LLVM_BIN has no {name}: {candidate}"
        )

    candidates: list[Path] = []
    if sibling is not None:
        candidates.append(sibling / name)
    if platform.system() == "Darwin":
        candidates.extend(
            (
                Path("/opt/homebrew/opt/llvm/bin") / name,
                Path("/opt/homebrew/bin") / name,
            )
        )
    found = shutil.which(name)
    if found:
        candidates.append(Path(found))
    for candidate in candidates:
        if candidate.is_file() and os.access(candidate, os.X_OK):
            return candidate.absolute()
    raise StatusError(
        Code.NOT_FOUND,
        f"LLVM tool {name} is unavailable; install LLVM or set "
        "SYMBIAN_LLVM_BIN to its bin directory",
    )
