"""Authenticated, manually addressed read-only development-agent sessions."""

from __future__ import annotations

import hashlib
import hmac
import secrets
import select
import socket
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
    """One authenticated connection to a read-only guest agent.

    The session authenticates both peers with a private 32-byte key and fresh
    nonces. The channel has no confidentiality; use a trusted local network.
    """

    def __init__(self, stream: socket.socket, timeout: float):
        self._stream = stream
        self.peer_ip = stream.getpeername()[0]
        self._timeout = timeout
        self._next_request_id = 1
        self.hello: AgentHello | None = None

    @classmethod
    def connect(
        cls,
        host: str,
        port: int,
        *,
        key_file: Path,
        timeout: float = 5.0,
    ) -> Self:
        """Authenticate both peers and open one read-only session.

        Args:
            host: Explicit IP address or host name; discovery is not active.
            port: Port on which the guest is already listening.
            key_file: Private 32-byte pairing key embedded in the guest build.
            timeout: Deadline in seconds for each socket operation.

        Returns:
            Connected session. Close it or use it as a context manager.
        """
        if not host or not 0 < port < 65536 or timeout <= 0:
            raise StatusError(Code.INVALID_ARGUMENT, "Invalid agent endpoint")
        key = key_file.read_bytes()
        if len(key) != 32:
            raise StatusError(
                Code.INVALID_ARGUMENT, "Agent key must contain 32 bytes"
            )
        try:
            raw = socket.create_connection((host, port), timeout=timeout)
        except TimeoutError as error:
            raise StatusError(
                Code.DEADLINE_EXCEEDED,
                f"TCP port {port} at {host} did not answer within {timeout:g}s",
            ) from error
        except OSError as error:
            raise StatusError(
                Code.UNAVAILABLE, f"Agent endpoint unavailable: {error}"
            ) from error
        return cls.from_socket(raw, key_file=key_file, timeout=timeout)

    @classmethod
    def accept(
        cls,
        listen_host: str,
        port: int,
        *,
        expected_peer: str | None = None,
        key_file: Path,
        timeout: float = 20.0,
    ) -> Self:
        """Wait for a phone-initiated connection and authenticate its key.

        A keyed UDP discovery exchange locates this listener without a stored
        address. The optional expected peer is checked before TCP handshake.
        Both listeners are scoped to one status attempt and always close.
        """
        if not 0 < port < 65536 or timeout <= 0:
            raise StatusError(Code.INVALID_ARGUMENT, "Invalid agent listener")
        key = key_file.read_bytes()
        if len(key) != 32:
            raise StatusError(Code.INVALID_ARGUMENT, "Invalid agent key")
        deadline = time.monotonic() + timeout
        discovered: set[str] = set()
        try:
            with (
                socket.socket(socket.AF_INET, socket.SOCK_STREAM) as listener,
                socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as discovery,
            ):
                listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
                listener.bind((listen_host, port))
                listener.listen(4)
                discovery.bind((listen_host, 39104))
                while True:
                    remaining = deadline - time.monotonic()
                    if remaining <= 0:
                        break
                    ready, _, _ = select.select(
                        [listener, discovery], [], [], remaining
                    )
                    if discovery in ready:
                        packet, peer = discovery.recvfrom(64)
                        if len(packet) == 45 and packet[:5] == b"SAGD1":
                            nonce = packet[5:13]
                            expected = hmac.digest(
                                key,
                                b"symbian-agent-discover-v1" + nonce,
                                hashlib.sha256,
                            )
                            if hmac.compare_digest(packet[13:], expected):
                                if (
                                    expected_peer is None
                                    or peer[0] == expected_peer
                                ):
                                    discovered.add(peer[0])
                                    proof = hmac.digest(
                                        key,
                                        b"symbian-agent-offer-v1" + nonce,
                                        hashlib.sha256,
                                    )
                                    discovery.sendto(
                                        b"SAGR1" + nonce + proof, peer
                                    )
                    if listener in ready:
                        raw, peer = listener.accept()
                        if peer[0] not in discovered:
                            raw.close()
                            continue
                        return cls.from_socket(
                            raw,
                            key_file=key_file,
                            timeout=min(5.0, remaining),
                        )
        except OSError as error:
            raise StatusError(
                Code.UNAVAILABLE, f"Agent listener unavailable: {error}"
            ) from error
        raise StatusError(
            Code.DEADLINE_EXCEEDED,
            (
                "Phone discovered this host but did not connect within "
                f"{timeout:g}s"
                if discovered
                else f"No keyed phone discovery arrived within {timeout:g}s"
            ),
        )

    @classmethod
    def from_socket(
        cls, raw: socket.socket, *, key_file: Path, timeout: float = 5.0
    ) -> Self:
        """Authenticate the agent on an already connected TCP socket."""
        if timeout <= 0:
            raw.close()
            raise StatusError(Code.INVALID_ARGUMENT, "Invalid agent timeout")
        key = key_file.read_bytes()
        if len(key) != 32:
            raw.close()
            raise StatusError(
                Code.INVALID_ARGUMENT, "Agent key must contain 32 bytes"
            )
        try:
            raw.settimeout(timeout)
            deadline = time.monotonic() + timeout
            phase = "challenge"
            challenge = cls._read_exact(raw, 36, deadline)
            if challenge[:4] != b"SAG1":
                raise StatusError(
                    Code.UNAUTHENTICATED, "Invalid agent challenge"
                )
            server_nonce = challenge[4:]
            client_nonce = secrets.token_bytes(32)
            nonces = server_nonce + client_nonce
            raw.sendall(
                client_nonce
                + hmac.digest(
                    key, b"symbian-agent-client-v1" + nonces, hashlib.sha256
                )
            )
            phase = "server proof"
            proof = cls._read_exact(raw, 32, deadline)
            expected = hmac.digest(
                key, b"symbian-agent-server-v1" + nonces, hashlib.sha256
            )
            if not hmac.compare_digest(proof, expected):
                raise StatusError(
                    Code.UNAUTHENTICATED, "Agent identity did not match"
                )
        except TimeoutError as error:
            raw.close()
            raise StatusError(
                Code.DEADLINE_EXCEEDED,
                f"Agent {phase} timed out after TCP connected",
            ) from error
        except OSError as error:
            raw.close()
            raise StatusError(
                Code.UNAVAILABLE, f"Agent connection failed: {error}"
            ) from error
        except BaseException:
            raw.close()
            raise
        session = cls(raw, timeout)
        try:
            session.negotiate()
        except BaseException:
            session.close()
            raise
        return session

    @staticmethod
    def _read_exact(
        stream: socket.socket, count: int, deadline: float
    ) -> bytes:
        result = bytearray()
        while len(result) < count:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError("Agent authentication deadline expired")
            stream.settimeout(remaining)
            part = stream.recv(count - len(result))
            if not part:
                raise StatusError(Code.UNAVAILABLE, "Agent connection closed")
            result.extend(part)
        return bytes(result)

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
        """Release the underlying socket."""
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
