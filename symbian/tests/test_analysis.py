"""Tests native parsing and canonical status propagation to Python."""

import struct

import pytest
from symbian._native import inspect_elf32

from symbian.status import Code, StatusError


def elf_header() -> bytes:
    identification = b"\x7fELF\x01\x01\x01" + b"\0" * 9
    return identification + struct.pack(
        "<HHIIIIIHHHHHH",
        1,
        40,
        1,
        0,
        0,
        0,
        0x05000000,
        52,
        0,
        0,
        0,
        0,
        0,
    )


def test_native_metadata_is_read_only():
    header = inspect_elf32(elf_header())
    assert header.machine == 40
    assert header.flags == 0x05000000
    with pytest.raises(AttributeError):
        header.machine = 3


def test_native_status_crosses_boundary():
    with pytest.raises(StatusError) as caught:
        inspect_elf32(b"not an object")
    assert caught.value.code == Code.DATA_LOSS
    assert caught.value.as_dict()["name"] == "DATA_LOSS"


def test_binding_rejects_non_bytes():
    with pytest.raises(TypeError):
        inspect_elf32("not bytes")


def test_unsupported_native_format_keeps_status_code():
    data = bytearray(elf_header())
    data[4] = 2
    with pytest.raises(StatusError) as caught:
        inspect_elf32(bytes(data))
    assert caught.value.code == Code.UNIMPLEMENTED
