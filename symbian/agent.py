"""Authenticated, manually addressed development-agent sessions."""

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
from symbian.websocket import WebSocketStream


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
    """One service event with a clamped, process-relative elapsed timestamp."""

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


class AgentWorkspaceEntry(BaseModel):
    """One name in the agent-owned workspace root."""

    model_config = ConfigDict(frozen=True)

    name: str
    directory: bool
    read_only: bool
    size_bytes: int | None = None


class AgentWorkspacePage(BaseModel):
    """A bounded, read-only workspace listing; offsets are not snapshots."""

    model_config = ConfigDict(frozen=True)

    entries: tuple[AgentWorkspaceEntry, ...]
    next_offset: int
    more: bool


class AgentHello(BaseModel):
    """Limits and read-only operations agreed before issuing requests."""

    model_config = ConfigDict(frozen=True, extra="allow")

    protocol_version: int
    maximum_control_bytes: int
    maximum_requests: int
    capabilities: tuple[str, ...]


class AgentScreenCapture(BaseModel):
    """An owned RGB565 snapshot of the primary screen."""

    model_config = ConfigDict(frozen=True)

    width: int
    height: int
    stride_bytes: int
    pixels: bytes

    def image(self):
        """Decode this capture with Pillow's native RGB565 decoder."""
        from PIL import Image

        return Image.frombytes(
            "RGB",
            (self.width, self.height),
            self.pixels,
            "raw",
            "BGR;16",
            self.stride_bytes,
            1,
        )


class AgentSession:
    """One authenticated connection to a bounded guest development agent.

    The session authenticates both peers with a private 32-byte key and fresh
    nonces. The channel has no confidentiality; use a trusted local network.
    """

    def __init__(self, stream: WebSocketStream, timeout: float):
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
                            websocket_server=True,
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
        cls,
        raw: socket.socket,
        *,
        key_file: Path,
        timeout: float = 5.0,
        websocket_server: bool = False,
    ) -> Self:
        """Authenticate the agent on an already connected TCP socket."""
        if timeout <= 0:
            raw.abort() if isinstance(raw, WebSocketStream) else raw.close()
            raise StatusError(Code.INVALID_ARGUMENT, "Invalid agent timeout")
        key = key_file.read_bytes()
        if len(key) != 32:
            raw.abort() if isinstance(raw, WebSocketStream) else raw.close()
            raise StatusError(
                Code.INVALID_ARGUMENT, "Agent key must contain 32 bytes"
            )
        try:
            deadline = time.monotonic() + timeout
            phase = "WebSocket handshake"
            raw = WebSocketStream.from_socket(
                raw, server=websocket_server, timeout=timeout
            )
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
            raw.abort() if isinstance(raw, WebSocketStream) else raw.close()
            raise StatusError(
                Code.DEADLINE_EXCEEDED,
                f"Agent {phase} timed out after TCP connected",
            ) from error
        except OSError as error:
            raw.abort() if isinstance(raw, WebSocketStream) else raw.close()
            raise StatusError(
                Code.UNAVAILABLE, f"Agent connection failed: {error}"
            ) from error
        except BaseException:
            raw.abort() if isinstance(raw, WebSocketStream) else raw.close()
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
        stream: WebSocketStream, count: int, deadline: float
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
            or not 2 <= hello.maximum_requests <= 1024
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
        self._stream.abort()

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

    def recent_logs(self, *, limit: int = 8) -> AgentLogPage:
        """Read the newest retained service events in bounded pages.

        This walks the agent's fixed 32-record ring. A ``gap`` means earlier
        records had already been overwritten; it is not a transport error.
        """
        if not 1 <= limit <= 8:
            raise StatusError(Code.INVALID_ARGUMENT, "Invalid log limit")
        cursor = 0
        records: list[AgentLogRecord] = []
        gap = False
        for _ in range(5):
            page = self.logs(after=cursor, limit=8)
            gap = gap or page.gap
            records.extend(page.records)
            if page.next_cursor <= cursor or len(page.records) < 8:
                return AgentLogPage(
                    records=tuple(records[-limit:]),
                    next_cursor=page.next_cursor,
                    gap=gap,
                )
            cursor = page.next_cursor
        raise StatusError(Code.DATA_LOSS, "Agent log cursor did not settle")

    def workspace_list(
        self, *, after: int = 0, limit: int = 8
    ) -> AgentWorkspacePage:
        """List only the agent's private workspace root, one bounded page.

        Concurrent directory changes can shift offset pages. No arbitrary
        device path or write operation is exposed by this method.
        """
        if (
            self.hello is None
            or "workspace-list" not in self.hello.capabilities
        ):
            raise StatusError(
                Code.FAILED_PRECONDITION, "Workspace listing unavailable"
            )
        if self._next_request_id > self.hello.maximum_requests:
            raise StatusError(
                Code.RESOURCE_EXHAUSTED, "Session request cap reached"
            )
        if not 0 <= after < 256 or not 1 <= limit <= 8:
            raise StatusError(Code.INVALID_ARGUMENT, "Invalid workspace page")
        request_id = self._next_request_id
        self._next_request_id += 1
        frame = _native.pack_agent_workspace_request(request_id, after, limit)
        return AgentWorkspacePage.model_validate(
            self._exchange(request_id, frame)
        )

    def capture_screen(self) -> AgentScreenCapture:
        """Capture the complete primary screen as an owned RGB565 image."""
        if (
            self.hello is None
            or "screen-capture" not in self.hello.capabilities
        ):
            raise StatusError(
                Code.FAILED_PRECONDITION, "Screen capture unavailable"
            )
        if self._next_request_id > self.hello.maximum_requests:
            raise StatusError(
                Code.RESOURCE_EXHAUSTED, "Session request cap reached"
            )
        request_id = self._next_request_id
        self._next_request_id += 1
        body = self._exchange(
            request_id, _native.pack_agent_display_request(request_id, 8)
        )
        width = body.get("width")
        height = body.get("height")
        stride = body.get("stride_bytes")
        length = body.get("data_bytes")
        if (
            body.get("format") != "rgb565-le"
            or not all(
                type(value) is int for value in (width, height, stride, length)
            )
            or not 0 < width <= 4096
            or not 0 < height <= 4096
            or not width * 2 <= stride <= 8192
            or length != stride * height
            or length > 32 * 1024 * 1024
        ):
            raise StatusError(Code.DATA_LOSS, "Invalid screen metadata")
        pixels = self._receive_exact(
            length, time.monotonic() + max(15, self._timeout)
        )
        return AgentScreenCapture(
            width=width, height=height, stride_bytes=stride, pixels=pixels
        )

    def pointer_event(self, action: str, x: int, y: int) -> None:
        """Send one move, down, or up event in primary screen pixels."""
        if self.hello is None or "pointer-event" not in self.hello.capabilities:
            raise StatusError(
                Code.FAILED_PRECONDITION, "Pointer input unavailable"
            )
        if self._next_request_id > self.hello.maximum_requests:
            raise StatusError(
                Code.RESOURCE_EXHAUSTED, "Session request cap reached"
            )
        actions = {"move": 1, "down": 2, "up": 3}
        if action not in actions or not 0 <= x <= 4095 or not 0 <= y <= 4095:
            raise StatusError(Code.INVALID_ARGUMENT, "Invalid pointer event")
        request_id = self._next_request_id
        self._next_request_id += 1
        self._exchange(
            request_id,
            _native.pack_agent_display_request(
                request_id, 9, x, y, actions[action]
            ),
        )

    def resource_read(self, name: str, *, app_uid: int | None = None) -> bytes:
        """Read an agent workspace file or a cooperating app's shared resource.

        App resources live under ``C:\\Data\\SymbianAgent\\apps\\<uid>``.
        Native Symbian data cages remain enforced by the File Server.
        """
        if self.hello is None or "resource-read" not in self.hello.capabilities:
            raise StatusError(
                Code.FAILED_PRECONDITION, "Resource read unavailable"
            )
        self._validate_resource_name(name)
        scope, uid = self._resource_scope(app_uid)
        output = bytearray()
        total = None
        while total is None or len(output) < total:
            if self._next_request_id > self.hello.maximum_requests:
                raise StatusError(
                    Code.RESOURCE_EXHAUSTED, "Session request cap reached"
                )
            request_id = self._next_request_id
            self._next_request_id += 1
            frame = _native.pack_agent_resource_request(
                request_id, 10, scope, uid, name, len(output), 32768
            )
            body = self._exchange(request_id, frame)
            count = body.get("data_bytes")
            reported_total = body.get("total_bytes")
            if (
                type(count) is not int
                or type(reported_total) is not int
                or not 0 <= count <= 32768
                or not 0 <= reported_total <= 16 * 1024 * 1024
                or (total is not None and total != reported_total)
                or len(output) + count > reported_total
                or (count == 0 and len(output) < reported_total)
            ):
                raise StatusError(Code.DATA_LOSS, "Invalid resource metadata")
            total = reported_total
            if count:
                output.extend(self._receive_exact(count, time.monotonic() + 15))
        return bytes(output)

    def resource_write(
        self,
        name: str,
        data: bytes,
        *,
        app_uid: int | None = None,
        replace: bool = False,
    ) -> None:
        """Create or explicitly replace a bounded resource, then flush it."""
        if (
            self.hello is None
            or "resource-write" not in self.hello.capabilities
        ):
            raise StatusError(
                Code.FAILED_PRECONDITION, "Resource write unavailable"
            )
        self._validate_resource_name(name)
        if not data or len(data) > 16 * 1024 * 1024:
            raise StatusError(
                Code.INVALID_ARGUMENT, "Resource size is out of bounds"
            )
        scope, uid = self._resource_scope(app_uid)
        for offset in range(0, len(data), 32768):
            if self._next_request_id > self.hello.maximum_requests:
                raise StatusError(
                    Code.RESOURCE_EXHAUSTED, "Session request cap reached"
                )
            chunk = data[offset : offset + 32768]
            request_id = self._next_request_id
            self._next_request_id += 1
            mode = (2 if replace else 0) if offset == 0 else 1
            frame = _native.pack_agent_resource_request(
                request_id, 11, scope, uid, name, offset, len(chunk), mode
            )
            body = self._exchange(request_id, frame, upload=chunk)
            if body.get("written") != len(chunk):
                raise StatusError(Code.DATA_LOSS, "Resource write incomplete")

    def package_open(self, name: str, app_uid: int) -> bool:
        """Ask AppArc to launch the registered installer for a staged SIS.

        Returns whether the app UID was already registered before launch.
        Installer UI and its outcome are separate; use screen and pointer
        controls to interact with it, then check registration again.
        """
        if self.hello is None or "package-open" not in self.hello.capabilities:
            raise StatusError(
                Code.FAILED_PRECONDITION, "Installer launch unavailable"
            )
        self._validate_resource_name(name)
        if not name.endswith(".sis"):
            raise StatusError(Code.INVALID_ARGUMENT, "Package must end in .sis")
        self._resource_scope(app_uid)
        if self._next_request_id > self.hello.maximum_requests:
            raise StatusError(
                Code.RESOURCE_EXHAUSTED, "Session request cap reached"
            )
        request_id = self._next_request_id
        self._next_request_id += 1
        frame = _native.pack_agent_application_request(
            request_id, 12, app_uid, name
        )
        body = self._exchange(request_id, frame)
        if (
            body.get("state") != "installer-launched"
            or type(body.get("registered_before")) is not bool
        ):
            raise StatusError(Code.DATA_LOSS, "Invalid installer response")
        return body["registered_before"]

    def app_registered(self, app_uid: int) -> bool:
        """Ask AppArc whether an app UID is currently registered."""
        if (
            self.hello is None
            or "app-registered" not in self.hello.capabilities
        ):
            raise StatusError(
                Code.FAILED_PRECONDITION, "AppArc query unavailable"
            )
        self._resource_scope(app_uid)
        if self._next_request_id > self.hello.maximum_requests:
            raise StatusError(
                Code.RESOURCE_EXHAUSTED, "Session request cap reached"
            )
        request_id = self._next_request_id
        self._next_request_id += 1
        frame = _native.pack_agent_application_request(request_id, 13, app_uid)
        body = self._exchange(request_id, frame)
        if type(body.get("registered")) is not bool:
            raise StatusError(Code.DATA_LOSS, "Invalid AppArc response")
        return body["registered"]

    @staticmethod
    def _validate_resource_name(name: str) -> None:
        if (
            not isinstance(name, str)
            or not 1 <= len(name) <= 64
            or name in (".", "..")
            or any(
                not (ch.isascii() and (ch.isalnum() or ch in "._-"))
                for ch in name
            )
        ):
            raise StatusError(Code.INVALID_ARGUMENT, "Invalid resource name")

    @staticmethod
    def _resource_scope(app_uid: int | None) -> tuple[int, int]:
        if app_uid is None:
            return 0, 0
        if type(app_uid) is not int or not 0 < app_uid <= 0xFFFFFFFF:
            raise StatusError(Code.INVALID_ARGUMENT, "Invalid app UID")
        return 1, app_uid

    def _exchange(
        self, request_id: int, frame: bytes, *, upload: bytes = b""
    ) -> dict:
        deadline = time.monotonic() + self._timeout
        self._set_remaining_timeout(deadline)
        self._stream.sendall(frame)
        for offset in range(0, len(upload), 4096):
            self._set_remaining_timeout(deadline)
            self._stream.sendall(upload[offset : offset + 4096])
        prefix = self._receive_exact(4, deadline)
        length = _native.agent_control_payload_length(prefix)
        result = _native.parse_agent_result_frame(
            prefix + self._receive_exact(length, deadline)
        )
        if result["request_id"] != request_id:
            raise StatusError(Code.DATA_LOSS, "Unexpected agent result")
        if result["kind"] == 5:
            body = result["body"]
            code = body.get("code")
            message = body.get("message")
            if type(code) is not int or type(message) is not str:
                raise StatusError(Code.DATA_LOSS, "Invalid agent error")
            try:
                raise StatusError(Code(code), message)
            except ValueError as error:
                raise StatusError(
                    Code.DATA_LOSS, "Unknown agent error"
                ) from error
        if result["kind"] != 4:
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


# Older integrations can migrate without changing their authentication flow.
ReadOnlyAgentSession = AgentSession
