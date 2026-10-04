"""Authenticated, manually addressed read-only development-agent sessions.

This narrow client works with the emulator research listener. It requires an
explicit CA and client identity, and uses native bindings for wire formatting.
Pairing, discovery and persistent device identity are later service work.
"""

from __future__ import annotations

import socket
import ssl
import time
from pathlib import Path
from typing import Self

from pydantic import BaseModel, ConfigDict

from symbian import _native
from symbian.status import Code, StatusError


class AgentTickSnapshot(BaseModel):
    """Native tick count and its platform-reported microsecond period."""

    model_config = ConfigDict(frozen=True)

    tick_count: int
    tick_period_us: int


class AgentDisplaySnapshot(BaseModel):
    """Primary HAL dimensions, which may differ from Window Server layout."""

    model_config = ConfigDict(frozen=True)

    width_pixels: int
    height_pixels: int


class AgentStatus(BaseModel):
    """Verified read-only status returned by the guest agent profile."""

    model_config = ConfigDict(frozen=True, extra="allow")

    request_id: int
    service: str
    state: str
    capabilities: tuple[str, ...]
    system: AgentTickSnapshot | None = None
    display: AgentDisplaySnapshot | None = None


class AgentLogRecord(BaseModel):
    """One service event with a process-relative monotonic timestamp."""

    model_config = ConfigDict(frozen=True)

    sequence: int
    code: int
    severity: int | None = None
    elapsed_us: int | None = None


class AgentLogPage(BaseModel):
    """Bounded service events following a sequence cursor."""

    model_config = ConfigDict(frozen=True)

    records: tuple[AgentLogRecord, ...]
    next_cursor: int
    gap: bool


class AgentHello(BaseModel):
    """Limits and read-only operations agreed before issuing requests."""

    model_config = ConfigDict(frozen=True, extra="allow")

    protocol_version: int
    maximum_control_bytes: int
    maximum_requests: int
    capabilities: tuple[str, ...]


class ReadOnlyAgentSession:
    """One TLS connection to a manually addressed read-only guest listener.

    The caller supplies an explicit CA, client certificate and key. TLS checks
    the requested server name and certificate chain before any control frame
    is sent. One session performs synchronous requests with a socket timeout.
    """

    def __init__(self, stream: ssl.SSLSocket, timeout: float):
        self._stream = stream
        self._timeout = timeout
        self._next_request_id = 1
        self.hello: AgentHello | None = None

    @classmethod
    def connect(
        cls,
        host: str,
        port: int,
        *,
        server_name: str,
        ca_bundle: Path,
        client_certificate: Path,
        client_key: Path,
        timeout: float = 5.0,
    ) -> Self:
        """Authenticate both TLS peers and open one read-only session.

        Args:
            host: Explicit IP address or host name; discovery is not active.
            port: Port on which the guest is already listening.
            server_name: Certificate name to validate independently of host.
            ca_bundle: Project-local PEM trust roots for this connection.
            client_certificate: PEM identity presented to the guest.
            client_key: Private key matching ``client_certificate``.
            timeout: Deadline in seconds for each socket operation.

        Returns:
            Connected session. Close it or use it as a context manager.
        """
        if not server_name or not 0 < port < 65536 or timeout <= 0:
            raise StatusError(Code.INVALID_ARGUMENT, "Invalid agent endpoint")
        context = ssl.create_default_context(cafile=str(ca_bundle))
        context.load_cert_chain(str(client_certificate), str(client_key))
        raw = socket.create_connection((host, port), timeout=timeout)
        try:
            stream = context.wrap_socket(raw, server_hostname=server_name)
            stream.settimeout(timeout)
        except BaseException:
            raw.close()
            raise
        session = cls(stream, timeout)
        try:
            session.negotiate()
        except BaseException:
            session.close()
            raise
        return session

    def negotiate(self) -> AgentHello:
        """Verify the guest's version, limits and supported operations once."""
        if self.hello is not None:
            return self.hello
        request_id = self._next_request_id
        self._next_request_id += 1
        frame = _native.pack_agent_read_request(request_id, 1)
        hello = AgentHello.model_validate(self._exchange(request_id, frame))
        if (
            hello.protocol_version != 1
            or not 128 <= hello.maximum_control_bytes <= 4096
            or not 2 <= hello.maximum_requests <= 16
            or "status" not in hello.capabilities
        ):
            raise StatusError(
                Code.FAILED_PRECONDITION, "Unsupported agent profile"
            )
        self.hello = hello
        return hello

    def __enter__(self) -> Self:
        return self

    def __exit__(self, *_exc: object) -> None:
        self.close()

    def close(self) -> None:
        """Release the TLS connection and its underlying socket."""
        self._stream.close()

    def status(self) -> AgentStatus:
        """Request the agent's current read-only service state."""
        if self.hello is None or "status" not in self.hello.capabilities:
            raise StatusError(Code.FAILED_PRECONDITION, "Status unavailable")
        if self._next_request_id > self.hello.maximum_requests:
            raise StatusError(
                Code.RESOURCE_EXHAUSTED, "Session request cap reached"
            )
        request_id = self._next_request_id
        self._next_request_id += 1
        frame = _native.pack_agent_read_request(request_id, 2)
        body = self._exchange(request_id, frame)
        return AgentStatus.model_validate({"request_id": request_id, **body})

    def logs(self, *, after: int = 0, limit: int = 8) -> AgentLogPage:
        """Read at most eight recent service events after a cursor.

        A true gap means older records were overwritten before this read.
        The next call should use ``next_cursor``; the ring is process-local.
        """
        if self.hello is None or "logs" not in self.hello.capabilities:
            raise StatusError(Code.FAILED_PRECONDITION, "Logs unavailable")
        if self._next_request_id > self.hello.maximum_requests:
            raise StatusError(
                Code.RESOURCE_EXHAUSTED, "Session request cap reached"
            )
        if after < 0 or not 1 <= limit <= 8:
            raise StatusError(
                Code.INVALID_ARGUMENT, "Invalid log cursor or limit"
            )
        request_id = self._next_request_id
        self._next_request_id += 1
        frame = _native.pack_agent_logs_request(request_id, after, limit)
        return AgentLogPage.model_validate(self._exchange(request_id, frame))

    def _exchange(self, request_id: int, frame: bytes) -> dict:
        deadline = time.monotonic() + self._timeout
        self._set_remaining_timeout(deadline)
        self._stream.sendall(frame)
        prefix = self._receive_exact(4, deadline)
        length = _native.agent_control_payload_length(prefix)
        result = _native.parse_agent_result_frame(
            prefix + self._receive_exact(length, deadline)
        )
        if result["request_id"] != request_id or result["kind"] != 4:
            raise StatusError(Code.DATA_LOSS, "Unexpected agent result")
        return result["body"]

    def _set_remaining_timeout(self, deadline: float) -> None:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise TimeoutError("Agent request deadline expired")
        self._stream.settimeout(remaining)

    def _receive_exact(self, length: int, deadline: float) -> bytes:
        data = bytearray()
        while len(data) < length:
            self._set_remaining_timeout(deadline)
            chunk = self._stream.recv(length - len(data))
            if not chunk:
                raise StatusError(Code.UNAVAILABLE, "Agent connection closed")
            data.extend(chunk)
        return bytes(data)
