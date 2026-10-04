"""Host-side read-only agent framing and mutual-TLS session controls."""

import socket
import ssl
import threading
import time
from pathlib import Path

import msgpack
import pytest

from symbian import _native
from symbian.agent import ReadOnlyAgentSession
from symbian.status import StatusException

FIXTURES = (
    Path(__file__).parents[2] / "third_party/mbedtls-symbian/tests/fixtures"
)
CERTIFICATE = FIXTURES / "server-cert.pem"
PRIVATE_KEY = FIXTURES / "server-key.pem"


def _receive_exact(stream, count):
    data = bytearray()
    while len(data) < count:
        chunk = stream.recv(count - len(data))
        if not chunk:
            raise EOFError("Peer closed before the frame completed")
        data.extend(chunk)
    return bytes(data)


def test_native_control_prefix_rejects_oversized_payload():
    assert _native.agent_control_payload_length(b"\x00\x00\x00\x03") == 3
    with pytest.raises(StatusException):
        _native.agent_control_payload_length(b"\x00\x00\x10\x01")


def test_read_only_status_over_mutual_tls():
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(str(CERTIFICATE), str(PRIVATE_KEY))
    context.load_verify_locations(str(CERTIFICATE))
    context.verify_mode = ssl.CERT_REQUIRED
    listener = socket.socket()
    listener.bind(("127.0.0.1", 0))
    listener.listen(1)
    port = listener.getsockname()[1]
    observed = []

    def serve():
        try:
            with listener.accept()[0] as raw:
                with context.wrap_socket(raw, server_side=True) as stream:
                    prefix = _receive_exact(stream, 4)
                    length = _native.agent_control_payload_length(prefix)
                    request = msgpack.unpackb(
                        _receive_exact(stream, length), raw=False
                    )
                    observed.append(request)
                    response = msgpack.packb(
                        {
                            "v": 1,
                            "id": request["id"],
                            "kind": 4,
                            "body": {
                                "service": "symbian-agent",
                                "state": "ready",
                                "capabilities": ["status"],
                                "system": {
                                    "tick_count": 91,
                                    "tick_period_us": 1000,
                                },
                                "display": {
                                    "width_pixels": 640,
                                    "height_pixels": 360,
                                },
                            },
                        },
                        use_bin_type=True,
                    )
                    stream.sendall(len(response).to_bytes(4, "big") + response)
        finally:
            listener.close()

    thread = threading.Thread(target=serve, daemon=True)
    thread.start()
    with ReadOnlyAgentSession.connect(
        "127.0.0.1",
        port,
        server_name="sdk-test",
        ca_bundle=CERTIFICATE,
        client_certificate=CERTIFICATE,
        client_key=PRIVATE_KEY,
    ) as agent:
        result = agent.status()
    thread.join(timeout=5)
    assert not thread.is_alive()
    assert result.request_id == 1
    assert result.service == "symbian-agent"
    assert result.state == "ready"
    assert result.capabilities == ("status",)
    assert result.system is not None
    assert result.system.tick_count == 91
    assert result.display is not None
    assert result.display.width_pixels == 640
    assert len(observed) == 1
    assert observed[0]["kind"] == 2
    assert observed[0]["id"] == 1


def test_read_only_status_has_one_aggregate_response_deadline():
    """A peer cannot extend a request indefinitely by dripping frame bytes."""
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(str(CERTIFICATE), str(PRIVATE_KEY))
    context.load_verify_locations(str(CERTIFICATE))
    context.verify_mode = ssl.CERT_REQUIRED
    listener = socket.socket()
    listener.bind(("127.0.0.1", 0))
    listener.listen(1)
    port = listener.getsockname()[1]

    def serve():
        try:
            with listener.accept()[0] as raw:
                with context.wrap_socket(raw, server_side=True) as stream:
                    prefix = _receive_exact(stream, 4)
                    length = _native.agent_control_payload_length(prefix)
                    _receive_exact(stream, length)
                    for byte in b"\x00\x00\x00\x01":
                        try:
                            stream.sendall(bytes([byte]))
                        except OSError:
                            break
                        time.sleep(0.22)
        finally:
            listener.close()

    thread = threading.Thread(target=serve, daemon=True)
    thread.start()
    with ReadOnlyAgentSession.connect(
        "127.0.0.1",
        port,
        server_name="sdk-test",
        ca_bundle=CERTIFICATE,
        client_certificate=CERTIFICATE,
        client_key=PRIVATE_KEY,
        timeout=0.5,
    ) as agent:
        started = time.monotonic()
        with pytest.raises(TimeoutError):
            agent.status()
        assert time.monotonic() - started < 0.8
    thread.join(timeout=5)
    assert not thread.is_alive()
