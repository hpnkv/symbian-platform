"""
Native Symbian runtime and analysis utilities.
"""

from __future__ import annotations

import collections.abc
import typing

import pybind11_abseil.status

import symbian.status

__all__: list[str] = [
    "ArmAttributes",
    "WebSocketCodec",
    "agent_control_payload_length",
    "pack_agent_read_request",
    "parse_agent_result_frame",
    "E32ExportSlot",
    "E32ImageInfo",
    "E32ImportBlock",
    "E32ImportSlot",
    "Elf32Header",
    "ProxyInfo",
    "ProxySources",
    "SdkExport",
    "SisEmbeddedFile",
    "SisPackageInfo",
    "SisPackageOptions",
    "Status",
    "build_application_sis",
    "build_registered_sis",
    "build_sis",
    "build_svg_mif",
    "convert_dll",
    "convert_imported_executable",
    "convert_pic_executable",
    "generate_import_proxy",
    "inspect_e32",
    "inspect_elf32",
    "inspect_import_proxy",
    "inspect_sis",
    "parse_def",
    "sign_sis",
    "status_code_from_http",
    "status_code_from_websocket",
    "status_code_to_http",
    "status_code_to_websocket",
]

def agent_control_payload_length(prefix: bytes) -> int:
    """Validate a complete four-byte agent control prefix."""
    ...

def pack_agent_read_request(
    request_id: int, kind: int, deadline_millis: int = 0
) -> bytes:
    """Encode a version-one hello or status request and its frame prefix."""
    ...

def parse_agent_result_frame(frame: bytes) -> dict[str, typing.Any]:
    """Validate and parse one complete agent result frame."""
    ...

class ArmAttributes:
    @property
    def cpu_arch(self) -> int: ...
    @property
    def fp_arch(self) -> int: ...
    @property
    def simd_arch(self) -> int: ...
    @property
    def thumb_isa(self) -> int: ...
    @property
    def vfp_args(self) -> int: ...

class E32ExportSlot:
    @property
    def absent(self) -> bool: ...
    @property
    def address(self) -> int: ...
    @property
    def ordinal(self) -> int: ...

class E32ImageInfo:
    """
    E32 application metadata; no runtime verdict.
    """

    @property
    def kernel(self) -> str: ...
    @property
    def architecture(self) -> str: ...
    @property
    def bss_size(self) -> int: ...
    @property
    def code_base(self) -> int: ...
    @property
    def code_data_relocations(self) -> list[int]: ...
    @property
    def code_relocations(self) -> list[int]: ...
    @property
    def code_size(self) -> int: ...
    @property
    def data_base(self) -> int: ...
    @property
    def data_data_relocations(self) -> list[int]: ...
    @property
    def data_relocations(self) -> list[int]: ...
    @property
    def data_size(self) -> int: ...
    @property
    def dll(self) -> bool: ...
    @property
    def entry_offset(self) -> int: ...
    @property
    def exception_descriptor_offset(self) -> int: ...
    @property
    def exports(self) -> list[E32ExportSlot]: ...
    @property
    def flags(self) -> int: ...
    @property
    def header_crc(self) -> int: ...
    @property
    def header_size(self) -> int: ...
    @property
    def imports(self) -> list[E32ImportBlock]: ...
    @property
    def secure_id(self) -> int: ...
    @property
    def capabilities(self) -> int: ...
    @property
    def uid3(self) -> int: ...

class E32ImportBlock:
    @property
    def dll(self) -> str: ...
    @property
    def slots(self) -> list[E32ImportSlot]: ...

class E32ImportSlot:
    @property
    def code_offset(self) -> int: ...
    @property
    def ordinal(self) -> int: ...

class Elf32Header:
    """
    ELF32 metadata; not a loader acceptance result.
    """

    @property
    def arm(self) -> ArmAttributes: ...
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

class ProxyInfo:
    @property
    def exports(self) -> list[SdkExport]: ...
    @property
    def soname(self) -> str: ...
    @property
    def target_dll(self) -> str: ...

class ProxySources:
    @property
    def assembly(self) -> str: ...
    @property
    def exports(self) -> list[SdkExport]: ...
    @property
    def linker_script(self) -> str: ...
    @property
    def version_script(self) -> str: ...

class SdkExport:
    @property
    def absent(self) -> bool: ...
    @property
    def data(self) -> bool: ...
    @property
    def ordinal(self) -> int: ...
    @property
    def symbol(self) -> str: ...

class SisEmbeddedFile:
    @property
    def capabilities(self) -> int: ...
    @property
    def sha1(self) -> str: ...
    @property
    def size(self) -> int: ...
    @property
    def target(self) -> str: ...

class SisPackageInfo:
    @property
    def application_registered(self) -> bool: ...
    @property
    def signed_package(self) -> bool: ...
    @property
    def executable_sha1(self) -> str: ...
    @property
    def executable_size(self) -> int: ...
    @property
    def executable_uid(self) -> int: ...
    @property
    def files(self) -> list[SisEmbeddedFile]: ...
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

class Status:
    __hash__: typing.ClassVar[None] = None
    def __eq__(self, right: Status) -> bool:
        """
        Returns whether two statuses have equal code, message, and details.
        """

    def __init__(
        self,
        code: typing.SupportsInt | typing.SupportsIndex = 0,
        message: str = "OK",
        details: typing.Any = [],
    ) -> None:
        """
        Creates a status from a canonical code, message, and details list.
        """

    def __repr__(self) -> str:
        """
        Returns a debug representation of the status.
        """

    def __str__(self) -> str:
        """
        Returns a 'CODE: message' string form of the status.
        """

    def _as_dict(self) -> dict[str, typing.Any]:
        """
        Returns the status as a JSON-compatible dict.
        """

    def _copy(self) -> Status:
        """
        Returns a copy of this status.
        """

    def is_ok(self) -> bool:
        """
        Returns whether the status is OK (no error).
        """

    @property
    def code(self) -> symbian.status.StatusCode:
        """
        The canonical status code.
        """

    @code.setter
    def code(self, arg1: typing.SupportsInt | typing.SupportsIndex) -> None: ...
    @property
    def details(self) -> list[typing.Any]:
        """
        The structured status details, as a list.
        """

    @details.setter
    def details(self, arg1: list[typing.Any]) -> None: ...
    @property
    def message(self) -> str:
        """
        The human-readable status message.
        """

    @message.setter
    def message(self, arg1: str) -> None: ...

def _absl_status_roundtrip(arg0: None) -> pybind11_abseil.status.Status: ...
def _status_from_callback(arg0: collections.abc.Callable) -> typing.Any: ...
def _status_or_value(arg0: typing.Any, arg1: str) -> str: ...
def _status_roundtrip(arg0: typing.Any) -> typing.Any: ...
def build_application_sis(
    data: bytes,
    files: collections.abc.Sequence[tuple[str, bytes]],
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
    Build a localized application SISX, releasing the GIL.
    """

def build_registered_sis(
    data: bytes,
    registration: bytes,
    caption: bytes,
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
    Build a registered unsigned SISX, releasing the GIL.
    """

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
    Build the canonical unsigned SISX application package, releasing the GIL.
    """

def build_svg_mif(data: bytes) -> bytes:
    """
    Compile a bounded SVG icon into MIF, releasing the GIL.
    """

def convert_dll(
    data: bytes,
    definition: bytes,
    proxies: collections.abc.Sequence[bytes],
    uid3: typing.SupportsInt | typing.SupportsIndex,
    capabilities: typing.SupportsInt | typing.SupportsIndex = 0,
) -> bytes:
    """
    Convert frozen DLL exports and eager imports, releasing the GIL.
    """

def convert_imported_executable(
    data: bytes,
    proxies: collections.abc.Sequence[bytes],
    uid3: typing.SupportsInt | typing.SupportsIndex,
    capabilities: typing.SupportsInt | typing.SupportsIndex = 0,
) -> bytes:
    """
    Convert retained calls through eager ordinal slots, releasing the GIL.
    """

def convert_pic_executable(
    data: bytes,
    uid3: typing.SupportsInt | typing.SupportsIndex,
    capabilities: typing.SupportsInt | typing.SupportsIndex = 0,
) -> bytes:
    """
    Convert a restricted, retained-relocation ELF.
    """

def generate_import_proxy(
    data: bytes,
    symbols: collections.abc.Sequence[str],
    soname: str,
    target_dll: str,
) -> ProxySources:
    """
    Generate ordinal proxy sources, releasing the GIL.
    """

def inspect_e32(data: bytes) -> E32ImageInfo:
    """
    Check the E32 application profile, releasing the GIL.
    """

def inspect_elf32(data: bytes) -> Elf32Header:
    """
    Inspect complete ELF32 bytes, releasing the GIL for native work.
    """

def inspect_import_proxy(data: bytes) -> ProxyInfo:
    """
    Check the generated ELF ordinal proxy contract, releasing the GIL.
    """

def inspect_sis(data: bytes) -> SisPackageInfo:
    """
    Check the canonical SISX application package, releasing the GIL.
    """

def sign_sis(data: bytes, certificate: bytes, private_key: bytes) -> bytes:
    """Sign a canonical SISX package, releasing the GIL."""

def parse_def(data: bytes) -> list[SdkExport]:
    """
    Parse bounded EABI export declarations, releasing the GIL.
    """

def status_code_from_http(
    arg0: typing.SupportsInt | typing.SupportsIndex,
) -> int: ...
def status_code_from_websocket(
    arg0: typing.SupportsInt | typing.SupportsIndex,
) -> int: ...
def status_code_to_http(
    arg0: typing.SupportsInt | typing.SupportsIndex,
) -> int: ...
def status_code_to_websocket(
    arg0: typing.SupportsInt | typing.SupportsIndex,
) -> int: ...

class WebSocketCodec:
    """Single-owner native nghttp2 RFC 8441 binary WebSocket endpoint."""

    def __init__(
        self,
        server: bool = False,
        path: str = "/symbian-agent",
        maximum_message_bytes: int = 4100,
    ) -> None: ...
    def feed(self, data: bytes) -> None: ...
    def take_output(self) -> bytes: ...
    def send(self, message: bytes) -> None: ...
    def receive(self) -> bytes | None: ...
    def close(self) -> None: ...
    def abort(self) -> None: ...
    @property
    def open(self) -> bool: ...
    @property
    def closed(self) -> bool: ...
    @property
    def buffered_amount(self) -> int: ...

def convert_eka1_executable(data: bytes, uid3: int) -> bytes: ...
