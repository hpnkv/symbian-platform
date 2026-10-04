"""Bounded read-only AT identity queries on an observed CDC ACM port."""

import os
import re
import select
import stat
import termios
import time
import tty

from pydantic import BaseModel, ConfigDict, Field

from symbian.status import Code, StatusError


class AtModel(BaseModel):
    """Immutable AT reply model with absent optional fields omitted."""

    model_config = ConfigDict(frozen=True)


class AtIdentityQuery(AtModel):
    """Result of one bounded AT identity command."""

    state: str = Field(description="AT exchange state")
    value: str | None = Field(
        default=None,
        description="Safe identity text",
        exclude_if=lambda value: value is None,
    )


class AtIdentityQueries(AtModel):
    """Results of the fixed identity and capability command set."""

    capabilities: AtIdentityQuery | None = Field(
        default=None,
        description="AT+GCAP result",
        exclude_if=lambda value: value is None,
    )
    manufacturer: AtIdentityQuery = Field(description="AT+CGMI result")
    model: AtIdentityQuery = Field(description="AT+CGMM result")
    revision: AtIdentityQuery = Field(description="AT+CGMR result")


class AtBatteryStatus(AtModel):
    """AT+CBC numeric status codes."""

    connection_status: int = Field(description="Modem connection state code")
    charge_percent: int = Field(description="Modem charge percentage")


class AtSignalStatus(AtModel):
    """AT+CSQ numeric status codes."""

    rssi_code: int = Field(description="Modem RSSI code; 99 means unavailable")
    ber_code: int = Field(description="Modem BER code; 99 means unavailable")


class AtStatusQuery(AtModel):
    """Result of one optional read-only modem status command."""

    state: str = Field(description="AT exchange or decode state")
    value: AtBatteryStatus | AtSignalStatus | None = Field(
        default=None,
        description="Parsed modem status",
        exclude_if=lambda value: value is None,
    )


class AtStatusQueries(AtModel):
    """Results of the optional battery and signal commands."""

    battery: AtStatusQuery = Field(description="AT+CBC result")
    signal: AtStatusQuery = Field(description="AT+CSQ result")


class AtProbe(AtModel):
    """Read-only CDC ACM AT probe result."""

    transport: str = Field(
        default="cdc-acm", description="Host serial transport"
    )
    state: str = Field(description="Probe state")
    detail: str | None = Field(
        default=None,
        description="Host I/O error",
        exclude_if=lambda value: value is None,
    )
    queries: AtIdentityQueries | None = Field(
        default=None,
        description="Identity query results",
        exclude_if=lambda value: value is None,
    )
    status_queries: AtStatusQueries | None = Field(
        default=None,
        description="Optional modem status results",
        exclude_if=lambda value: value is None,
    )


_QUERIES = (
    ("capabilities", b"AT+GCAP\r"),
    ("manufacturer", b"AT+CGMI\r"),
    ("model", b"AT+CGMM\r"),
    ("revision", b"AT+CGMR\r"),
)
_STATUS_QUERIES = (
    ("battery", b"AT+CBC\r"),
    ("signal", b"AT+CSQ\r"),
)


def _exchange(
    fd: int, command: bytes, timeout: float = 1.0
) -> tuple[str, list[str]]:
    """Send one fixed command and read at most 4 KiB through its final line."""
    deadline = time.monotonic() + timeout
    sent = 0
    while sent < len(command) and time.monotonic() < deadline:
        _, writable, _ = select.select(
            [], [fd], [], max(0, deadline - time.monotonic())
        )
        if not writable:
            break
        try:
            sent += os.write(fd, command[sent:])
        except BlockingIOError:
            continue
    if sent != len(command):
        return "timeout", []
    payload = bytearray()
    while len(payload) < 4096 and time.monotonic() < deadline:
        readable, _, _ = select.select(
            [fd], [], [], max(0, deadline - time.monotonic())
        )
        if not readable:
            break
        try:
            chunk = os.read(fd, min(512, 4096 - len(payload)))
        except BlockingIOError:
            continue
        if not chunk:
            break
        payload.extend(chunk)
        lines = [
            line.strip() for line in payload.replace(b"\r", b"\n").split(b"\n")
        ]
        if any(
            line in (b"OK", b"ERROR") or line.startswith(b"+CME ERROR:")
            for line in lines
        ):
            break
    lines = [
        line.strip().decode("ascii", errors="replace")
        for line in payload.replace(b"\r", b"\n").split(b"\n")
        if line.strip()
    ]
    final_index = next(
        (
            index
            for index, line in enumerate(lines)
            if line in ("OK", "ERROR") or line.startswith("+CME ERROR:")
        ),
        None,
    )
    if final_index is None:
        return "timeout" if len(payload) < 4096 else "oversize", []
    if lines[final_index] != "OK":
        return "error", []
    return "ok", [
        line
        for line in lines[:final_index]
        if line != command.decode("ascii").strip()
        and not line.startswith("+CME ERROR:")
    ]


def _safe_identity(lines: list[str]) -> str | None:
    """Keep short printable identity text; suppress likely subscriber IDs."""
    value = " ".join(lines).strip()
    if (
        not value
        or len(value) > 256
        or any(
            ord(character) < 32 or ord(character) > 126 for character in value
        )
        or re.search(r"\b[0-9]{14,16}\b", value)
    ):
        return None
    return value


def _status_value(
    name: str, lines: list[str]
) -> AtBatteryStatus | AtSignalStatus | None:
    """Parse only the two fixed numeric status replies."""
    prefix = "+CBC:" if name == "battery" else "+CSQ:"
    match = next(
        (
            re.fullmatch(rf"{re.escape(prefix)}\s*(\d+)\s*,\s*(\d+)", line)
            for line in lines
            if line.startswith(prefix)
        ),
        None,
    )
    if match is None:
        return None
    first, second = int(match.group(1)), int(match.group(2))
    if name == "battery":
        if first > 3 or second > 100:
            return None
        return AtBatteryStatus(connection_status=first, charge_percent=second)
    if first not in (*range(32), 99) or second not in (*range(8), 99):
        return None
    return AtSignalStatus(rssi_code=first, ber_code=second)


def probe(port: str, include_status: bool = False) -> AtProbe:
    """Query AT identity without dialing, writing settings, or asking for IDs.

    Args:
        port: A serial path obtained from the same USB device's IORegistry tree.

    Returns:
        A bounded result with only successful model/manufacturer/revision text.
    """
    if not re.fullmatch(r"/dev/cu\.[A-Za-z0-9._-]+", port):
        raise StatusError(Code.INVALID_ARGUMENT, "Invalid host serial port")
    flags = os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK
    if hasattr(os, "O_NOFOLLOW"):
        flags |= os.O_NOFOLLOW
    try:
        fd = os.open(port, flags)
    except OSError as error:
        return AtProbe(state="port-unavailable", detail=str(error))
    try:
        if not stat.S_ISCHR(os.fstat(fd).st_mode):
            raise StatusError(
                Code.FAILED_PRECONDITION,
                "Serial path is not a character device",
            )
        original = termios.tcgetattr(fd)
        try:
            tty.setraw(fd)
            state, _ = _exchange(fd, b"AT\r")
            if state != "ok":
                return AtProbe(state=f"at-{state}")
            results: dict[str, AtIdentityQuery] = {}
            for name, command in _QUERIES:
                query_state, lines = _exchange(fd, command)
                results[name] = AtIdentityQuery(
                    state=query_state,
                    value=(
                        _safe_identity(lines) if query_state == "ok" else None
                    ),
                )
            identity_queries = AtIdentityQueries(**results)
            status_queries = None
            if include_status:
                status: dict[str, AtStatusQuery] = {}
                for name, command in _STATUS_QUERIES:
                    query_state, lines = _exchange(fd, command)
                    parsed = (
                        _status_value(name, lines)
                        if query_state == "ok"
                        else None
                    )
                    status[name] = AtStatusQuery(
                        state=(
                            "ok"
                            if parsed is not None
                            else (
                                query_state
                                if query_state != "ok"
                                else "unrecognized-response"
                            )
                        ),
                        value=parsed,
                    )
                status_queries = AtStatusQueries(**status)
            return AtProbe(
                state="at-ready",
                queries=identity_queries,
                status_queries=status_queries,
            )
        finally:
            termios.tcsetattr(fd, termios.TCSANOW, original)
    except OSError as error:
        return AtProbe(state="io-error", detail=str(error))
    finally:
        os.close(fd)
