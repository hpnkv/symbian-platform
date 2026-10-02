"""Binary analysis uses the native implementation."""

from pathlib import Path

from symbian.native import require_native


def inspect_elf(path: Path) -> dict:
    """Reads bounded ELF32 metadata; does not prove loader acceptance."""
    header = require_native().inspect_elf32(path.read_bytes())
    result = {
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

    result["arm_attributes"] = {
        name: getattr(header.arm, name)
        for name in (
            "cpu_arch",
            "fp_arch",
            "simd_arch",
            "thumb_isa",
            "vfp_args",
        )
    }
    return result
