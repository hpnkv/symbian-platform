"""Policy-free access to the experimental native E32 format utilities."""

from pathlib import Path

from symbian.native import require_native


def convert_pic_executable(data: bytes, uid3: int) -> bytes:
    """Converts trusted ELF with retained relocations in the native core."""
    return require_native().convert_pic_executable(data, uid3)


def inspect_image(path: Path) -> dict:
    """Checks the narrow E32 profile and returns its native metadata."""
    info = require_native().inspect_e32(path.read_bytes())
    return {
        field: getattr(info, field)
        for field in (
            "uid3",
            "header_crc",
            "flags",
            "code_size",
            "code_base",
            "entry_offset",
            "secure_id",
        )
    }


__all__ = ["convert_pic_executable", "inspect_image"]
