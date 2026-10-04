"""Recognize generated applications and standalone source projects."""

from pathlib import Path


def has_application_manifest(folder: Path) -> bool:
    """Return whether a folder declares a Symbian application project."""
    return any(
        (folder / name).is_file()
        for name in ("symbian.toml", "symbian-project.json")
    )
