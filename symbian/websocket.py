"""Worker-owned RFC 8441 WebSockets using the native nghttp2 runtime codec."""

from __future__ import annotations

import socket
import time
from typing import Self

from symbian import _native
from symbian.status import Code, StatusError


class WebSocketStream:
    """One synchronous binary WebSocket, confined to its calling thread.

    Connect and accept use HTTP/2 prior knowledge and extended CONNECT. Socket
    I/O and deadlines are Python policy; native code owns HTTP/2 and WebSocket
    formats, masking, bounds, ping/pong and message assembly. No TLS is implied.
    """

    def __init__(self, raw: socket.socket, codec: _native.WebSocketCodec):
        self._raw = raw
        self._codec = codec
        self._timeout: float | None = raw.gettimeout()
        self._pending = b""

    @classmethod
    def from_socket(
        cls,
        raw: socket.socket,
        *,
        server: bool = False,
        path: str = "/symbian-agent",
        timeout: float = 5.0,
        maximum_message_bytes: int = 4100,
    ) -> Self:
        """Handshake a connected socket; take ownership even on failure."""
        if timeout <= 0:
            raw.close()
            raise StatusError(
                Code.INVALID_ARGUMENT, "Invalid handshake timeout"
            )
        try:
            if raw.family in (socket.AF_INET, socket.AF_INET6):
                raw.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            stream = cls(
                raw,
                _native.WebSocketCodec(server, path, maximum_message_bytes),
            )
            stream.settimeout(timeout)
            deadline = time.monotonic() + timeout
            stream._flush(deadline)
            while not stream._codec.open:
                stream._pump(deadline)
            return stream
        except BaseException:
            raw.close()
            raise

    @classmethod
    def connect(
        cls,
        host: str,
        port: int,
        *,
        timeout: float = 5.0,
        path: str = "/symbian-agent",
        maximum_message_bytes: int = 4100,
    ) -> Self:
        """Dial and complete extended CONNECT within one deadline."""
        deadline = time.monotonic() + timeout
        raw = socket.create_connection((host, port), timeout=timeout)
        return cls.from_socket(
            raw,
            path=path,
            timeout=max(0.0, deadline - time.monotonic()),
            maximum_message_bytes=maximum_message_bytes,
        )

    def _deadline(self) -> float | None:
        return (
            None if self._timeout is None else time.monotonic() + self._timeout
        )

    def _remaining(self, deadline: float | None) -> None:
        if deadline is None:
            self._raw.settimeout(None)
            return
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise TimeoutError("WebSocket operation deadline expired")
        self._raw.settimeout(remaining)

    def _flush(self, deadline: float | None) -> None:
        output = self._codec.take_output()
        if output:
            self._remaining(deadline)
            self._raw.sendall(output)

    def _pump(self, deadline: float | None) -> None:
        self._remaining(deadline)
        data = self._raw.recv(16384)
        if not data:
            raise StatusError(Code.UNAVAILABLE, "WebSocket transport EOF")
        self._codec.feed(data)
        self._flush(deadline)

    def send(self, message: bytes) -> None:
        """Send one bounded binary message and flush available HTTP/2 data."""
        try:
            self._codec.send(message)
            self._flush(self._deadline())
        except BaseException:
            self.abort()
            raise

    def receive(self) -> bytes | None:
        """Return one binary message; None follows drained peer close."""
        deadline = self._deadline()
        try:
            while True:
                message = self._codec.receive()
                if message is not None:
                    return message
                if self._codec.closed:
                    return None
                self._pump(deadline)
        except BaseException:
            self.abort()
            raise

    def sendall(self, data: bytes) -> None:
        """Carry a WireStream packet in one binary WebSocket message."""
        self.send(data)

    def recv(self, count: int) -> bytes:
        """Read bytes across binary messages for a native WireStream decoder."""
        if count <= 0:
            return b""
        deadline = self._deadline()
        while not self._pending:
            message = self._codec.receive()
            if message is not None:
                self._pending = message
                if self._pending:
                    break
            if self._codec.closed:
                return b""
            try:
                self._pump(deadline)
            except BaseException:
                self.abort()
                raise
        result, self._pending = self._pending[:count], self._pending[count:]
        return result

    def settimeout(self, timeout: float | None) -> None:
        """Set the budget for each operation, independently of stream life."""
        self._timeout = timeout

    def getpeername(self) -> tuple:
        """Return the underlying TCP peer address."""
        return self._raw.getpeername()

    @property
    def buffered_amount(self) -> int:
        """Bytes awaiting HTTP/2 peer flow-control credit."""
        return self._codec.buffered_amount

    def close(self) -> None:
        """Send close, await the peer by the operation deadline, release TCP."""
        try:
            self._codec.close()
            deadline = self._deadline()
            self._flush(deadline)
            while not self._codec.closed:
                self._pump(deadline)
        finally:
            self._raw.close()

    def abort(self) -> None:
        """Release TCP immediately after a protocol or transport failure."""
        self._codec.abort()
        self._raw.close()

    def __enter__(self) -> Self:
        return self

    def __exit__(self, *_exc: object) -> None:
        self.abort()


class WebSocketServer:
    """Reusable IPv4 listener; accepted streams outlive the listener."""

    def __init__(
        self,
        host: str = "127.0.0.1",
        port: int = 0,
        *,
        path: str = "/symbian-agent",
        maximum_message_bytes: int = 4100,
    ):
        self._path = path
        self._maximum_message_bytes = maximum_message_bytes
        self._listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            self._listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self._listener.bind((host, port))
            self._listener.listen(4)
        except BaseException:
            self._listener.close()
            raise

    @property
    def port(self) -> int:
        """Return the bound port, including an ephemeral port."""
        return self._listener.getsockname()[1]

    def accept(self, *, timeout: float = 5.0) -> WebSocketStream:
        """Accept and handshake one peer within one deadline."""
        deadline = time.monotonic() + timeout
        self._listener.settimeout(timeout)
        raw, _ = self._listener.accept()
        return WebSocketStream.from_socket(
            raw,
            server=True,
            path=self._path,
            timeout=max(0.0, deadline - time.monotonic()),
            maximum_message_bytes=self._maximum_message_bytes,
        )

    def close(self) -> None:
        """Stop accepting new connections."""
        self._listener.close()

    def __enter__(self) -> Self:
        return self

    def __exit__(self, *_exc: object) -> None:
        self.close()
