"""Authenticated, manually addressed read-only development-agent sessions.

This narrow client works with the emulator research listener. It requires an
explicit CA and client identity, and uses native bindings for wire formatting.
Pairing, discovery and persistent device identity are later service work.
"""

from __future__ import annotations

import socket
import ssl
from pathlib import Path
from typing import Self

from pydantic import BaseModel, ConfigDict

from symbian import _native
from symbian.status import Code, StatusError


class AgentStatus(BaseModel):
    """Verified read-only status returned by the guest agent profile."""

    model_config = ConfigDict(frozen=True, extra="allow")

    request_id: int
    service: str
    state: str
    capabilities: tuple[str, ...]


class ReadOnlyAgentSession:
    """One TLS connection to a manually addressed read-only guest listener.

    The caller supplies an explicit CA, client certificate and key. TLS checks
    the requested server name and certificate chain before any control frame
    is sent. One session performs synchronous requests with a socket timeout.
    """

    def __init__(self, stream: ssl.SSLSocket):
        self._stream = stream
        self._next_request_id = 1

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
        return cls(stream)

    def __enter__(self) -> Self:
        return self

    def __exit__(self, *_exc: object) -> None:
        self.close()

    def close(self) -> None:
        """Release the TLS connection and its underlying socket."""
        self._stream.close()

    def status(self) -> AgentStatus:
        """Request the agent's current read-only service state."""
        request_id = self._next_request_id
        self._next_request_id += 1
        frame = _native.pack_agent_read_request(request_id, 2)
        self._stream.sendall(frame)
        prefix = self._receive_exact(4)
        length = _native.agent_control_payload_length(prefix)
        result = _native.parse_agent_result_frame(
            prefix + self._receive_exact(length)
        )
        if result["request_id"] != request_id or result["kind"] != 4:
            raise StatusError(Code.DATA_LOSS, "Unexpected agent result")
        body = result["body"]
        return AgentStatus.model_validate({"request_id": request_id, **body})

    def _receive_exact(self, length: int) -> bytes:
        data = bytearray()
        while len(data) < length:
            chunk = self._stream.recv(length - len(data))
            if not chunk:
                raise StatusError(Code.UNAVAILABLE, "Agent connection closed")
            data.extend(chunk)
        return bytes(data)
