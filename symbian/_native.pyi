"""
Stateless native Symbian analysis utilities.
"""

from __future__ import annotations

__all__: list[str] = ["Elf32Header", "inspect_elf32"]

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

def inspect_elf32(data: bytes) -> Elf32Header:
    """
    Inspect complete ELF32 bytes, releasing the GIL for native work.
    """
