"""Binary analysis uses the native implementation."""

from pathlib import Path

from symbian.status import Code, StatusError


def inspect_elf(path: Path) -> dict:
    """Reads bounded ELF32 metadata; does not prove loader acceptance."""
    try:
        from symbian._native import inspect_elf32
    except ImportError as error:
        raise StatusError(
            Code.FAILED_PRECONDITION,
            "Native analysis extension is missing; run uv sync first",
        ) from error
    header = inspect_elf32(path.read_bytes())
    return {
        name: getattr(header, name)
        for name in (
            "type",
            "machine",
            "entry",
            "flags",
            "program_count",
            "section_count",
        )
    }
