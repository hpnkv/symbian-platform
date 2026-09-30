"""
Stateless native Symbian analysis utilities.
"""

from __future__ import annotations

import collections.abc
import typing

__all__: list[str] = [
    "E32ImageInfo",
    "Elf32Header",
    "SisPackageInfo",
    "SisPackageOptions",
    "build_sis",
    "convert_pic_executable",
    "inspect_e32",
    "inspect_elf32",
    "inspect_sis",
]

class E32ImageInfo:
    """
    Experimental E32 metadata; no runtime verdict.
    """

    @property
    def code_base(self) -> int: ...
    @property
    def code_size(self) -> int: ...
    @property
    def entry_offset(self) -> int: ...
    @property
    def flags(self) -> int: ...
    @property
    def header_crc(self) -> int: ...
    @property
    def secure_id(self) -> int: ...
    @property
    def uid3(self) -> int: ...

class Elf32Header:
    """
    ELF32 metadata; not a loader acceptance result.
    """

    @property
    def entry(self) -> int:
        """
        ELF entry address.
        """

    @property
    def flags(self) -> int:
        """
        Target-specific ELF flags.
        """

    @property
    def machine(self) -> int:
        """
        ELF machine identifier.
        """

    @property
    def program_count(self) -> int:
        """
        Number of program headers.
        """

    @property
    def section_count(self) -> int:
        """
        Number of section headers.
        """

    @property
    def type(self) -> int:
        """
        ELF object type.
        """

class SisPackageInfo:
    @property
    def executable_size(self) -> int: ...
    @property
    def executable_uid(self) -> int: ...
    @property
    def options(self) -> SisPackageOptions: ...
    @property
    def target(self) -> str: ...

class SisPackageOptions:
    @property
    def executable_name(self) -> str: ...
    @property
    def name(self) -> str: ...
    @property
    def uid(self) -> int: ...
    @property
    def vendor(self) -> str: ...
    @property
    def version(self) -> typing.Annotated[list[int], "FixedSize(3)"]: ...

def build_sis(
    data: bytes,
    uid: typing.SupportsInt | typing.SupportsIndex,
    name: str,
    vendor: str,
    executable_name: str,
    version: typing.Annotated[
        collections.abc.Sequence[typing.SupportsInt | typing.SupportsIndex],
        "FixedSize(3)",
    ] = [1, 0, 0],
) -> bytes:
    """
    Build the canonical unsigned SISX experiment, releasing the GIL.
    """

def convert_pic_executable(
    data: bytes, uid3: typing.SupportsInt | typing.SupportsIndex
) -> bytes:
    """
    Convert a restricted, retained-relocation ELF.
    """

def inspect_e32(data: bytes) -> E32ImageInfo:
    """
    Check the experimental E32 profile, releasing the GIL.
    """

def inspect_elf32(data: bytes) -> Elf32Header:
    """
    Inspect complete ELF32 bytes, releasing the GIL for native work.
    """

def inspect_sis(data: bytes) -> SisPackageInfo:
    """
    Check the canonical unsigned SISX experiment, releasing the GIL.
    """
