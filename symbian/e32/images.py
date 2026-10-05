"""Policy-free access to the native E32 application format utilities."""

from pathlib import Path

from symbian.native import require_native


def convert_pic_executable(
    data: bytes, uid3: int, capabilities: int = 0
) -> bytes:
    """Converts trusted ELF with retained relocations in the native core."""
    return require_native().convert_pic_executable(data, uid3, capabilities)


def convert_eka1_executable(
    data: bytes, uid3: int, proxies: list[bytes] | None = None
) -> bytes:
    """Converts the bounded EKA1 no-UI process profile in the native core."""
    return require_native().convert_eka1_executable(data, uid3, proxies or [])


def convert_imported_executable(
    data: bytes, proxies: list[bytes], uid3: int, capabilities: int = 0
) -> bytes:
    """Converts retained eager function calls using native ordinal proxies."""
    return require_native().convert_imported_executable(
        data, proxies, uid3, capabilities
    )


def convert_dll(
    data: bytes,
    definition: bytes,
    proxies: list[bytes],
    uid3: int,
    capabilities: int = 0,
) -> bytes:
    """Converts frozen function exports and optional imports in native code."""
    return require_native().convert_dll(
        data, definition, proxies, uid3, capabilities
    )


def inspect_image(path: Path) -> dict:
    """Checks the narrow E32 profile and returns its native metadata."""
    info = require_native().inspect_e32(path.read_bytes())
    result = {
        field: getattr(info, field)
        for field in (
            "kernel",
            "uid3",
            "header_crc",
            "flags",
            "architecture",
            "code_size",
            "code_base",
            "data_size",
            "bss_size",
            "data_base",
            "entry_offset",
            "secure_id",
            "capabilities",
            "dll",
            "header_size",
            "exception_descriptor_offset",
            "code_relocations",
            "code_data_relocations",
            "data_relocations",
            "data_data_relocations",
        )
    }
    result["imports"] = [
        {
            "dll": block.dll,
            "slots": [
                {"code_offset": slot.code_offset, "ordinal": slot.ordinal}
                for slot in block.slots
            ],
        }
        for block in info.imports
    ]
    result["exports"] = [
        {
            "ordinal": slot.ordinal,
            "address": slot.address,
            "absent": slot.absent,
        }
        for slot in info.exports
    ]
    return result


__all__ = [
    "convert_pic_executable",
    "convert_eka1_executable",
    "convert_imported_executable",
    "inspect_image",
    "convert_dll",
]
