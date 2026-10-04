"""Host process launch paths that keep owned GUI tests behind the active app."""

import sys
from pathlib import Path

from symbian.status import Code, StatusError


def background_environment() -> dict[str, str]:
    """Returns both Qt and EKA settings for a nonactivating macOS launch."""
    if sys.platform != "darwin":
        return {}
    return {
        "EKA2L1_RESEARCH_BACKGROUND_WINDOW": "1",
        "QT_MAC_DISABLE_FOREGROUND_APPLICATION_TRANSFORM": "1",
    }


def executable_for_session(executable: Path, directory: Path) -> Path:
    """Returns an owned nonbundle launch path for a macOS emulator app.

    macOS activates a program launched at its .app executable path before Qt
    shows a window. A symlink in the disposable session keeps subprocess PID,
    exit and signal ownership while preventing that bundle activation. The
    emulator still locates its files through its explicit data root.
    """
    binary = executable.resolve()
    if not binary.is_file():
        raise StatusError(Code.NOT_FOUND, f"Emulator executable: {binary}")
    if (
        sys.platform != "darwin"
        or binary.parent.name != "MacOS"
        or binary.parent.parent.name != "Contents"
        or binary.parent.parent.parent.suffix != ".app"
    ):
        return binary
    link = directory.resolve() / binary.name
    if link.exists() or link.is_symlink():
        raise StatusError(Code.ALREADY_EXISTS, f"Emulator launch path: {link}")
    link.symlink_to(binary)
    return link
