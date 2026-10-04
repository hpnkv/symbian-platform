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
from symbian.status import Code, StatusError, StatusException

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

    def reply(stream, request, body):
        response = msgpack.packb(
            {"v": 1, "id": request["id"], "kind": 4, "body": body},
            use_bin_type=True,
        )
        stream.sendall(len(response).to_bytes(4, "big") + response)

    def receive(stream):
        prefix = _receive_exact(stream, 4)
        length = _native.agent_control_payload_length(prefix)
        request = msgpack.unpackb(_receive_exact(stream, length), raw=False)
        observed.append(request)
        return request

    def serve():
        try:
            with listener.accept()[0] as raw:
                with context.wrap_socket(raw, server_side=True) as stream:
                    reply(
                        stream,
                        receive(stream),
                        {
                            "protocol_version": 1,
                            "maximum_control_bytes": 4096,
                            "maximum_requests": 16,
                            "capabilities": ["status", "logs"],
                        },
                    )
                    reply(
                        stream,
                        receive(stream),
                        {
                            "service": "symbian-agent",
                            "state": "ready",
                            "capabilities": ["status", "logs"],
                            "system": {
                                "tick_count": 91,
                                "tick_period_us": 1000,
                            },
                            "display": {
                                "width_pixels": 640,
                                "height_pixels": 360,
                            },
                        },
                    )
                    reply(
                        stream,
                        receive(stream),
                        {
                            "records": [
                                {
                                    "sequence": 5,
                                    "code": 2,
                                    "severity": 1,
                                    "elapsed_us": 250,
                                },
                                {"sequence": 6, "code": 99},
                            ],
                            "next_cursor": 6,
                            "gap": False,
                        },
                    )
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
        logs = agent.logs(after=4, limit=2)
    thread.join(timeout=5)
    assert not thread.is_alive()
    assert result.request_id == 2
    assert result.service == "symbian-agent"
    assert result.state == "ready"
    assert result.capabilities == ("status", "logs")
    assert result.system is not None
    assert result.system.tick_count == 91
    assert result.display is not None
    assert result.display.width_pixels == 640
    assert len(observed) == 3
    assert observed[0]["kind"] == 1
    assert observed[1]["kind"] == 2
    assert observed[1]["id"] == 2
    assert observed[2]["kind"] == 6
    assert observed[2]["body"] == {"after": 4, "limit": 2}
    assert logs.records[0].sequence == 5
    assert logs.records[0].code == 2
    assert logs.records[0].severity == 1
    assert logs.records[0].elapsed_us == 250
    assert logs.records[1].code == 99
    assert logs.records[1].severity is None
    assert logs.next_cursor == 6
    assert logs.gap is False


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
                    request = msgpack.unpackb(
                        _receive_exact(stream, length), raw=False
                    )
                    hello = msgpack.packb(
                        {
                            "v": 1,
                            "id": request["id"],
                            "kind": 4,
                            "body": {
                                "protocol_version": 1,
                                "maximum_control_bytes": 4096,
                                "maximum_requests": 16,
                                "capabilities": ["status"],
                            },
                        },
                        use_bin_type=True,
                    )
                    stream.sendall(len(hello).to_bytes(4, "big") + hello)
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


def test_rejects_unsupported_hello_before_status():
    """A TLS peer cannot downgrade the required control profile."""
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
                    observed.append(request["kind"])
                    response = msgpack.packb(
                        {
                            "v": 1,
                            "id": request["id"],
                            "kind": 4,
                            "body": {
                                "protocol_version": 2,
                                "maximum_control_bytes": 4096,
                                "maximum_requests": 16,
                                "capabilities": ["status"],
                            },
                        },
                        use_bin_type=True,
                    )
                    stream.sendall(len(response).to_bytes(4, "big") + response)
        finally:
            listener.close()

    thread = threading.Thread(target=serve, daemon=True)
    thread.start()
    with pytest.raises(StatusError) as error:
        ReadOnlyAgentSession.connect(
            "127.0.0.1",
            port,
            server_name="sdk-test",
            ca_bundle=CERTIFICATE,
            client_certificate=CERTIFICATE,
            client_key=PRIVATE_KEY,
        )
    thread.join(timeout=5)
    assert not thread.is_alive()
    assert error.value.code == Code.FAILED_PRECONDITION
    assert observed == [1]
