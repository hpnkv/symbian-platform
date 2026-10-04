"""Host-side read-only agent framing, authentication and deadline controls."""

import hashlib
import hmac
import secrets
import socket
import threading
import time
from pathlib import Path

import msgpack
import pytest

from symbian import _native
from symbian.agent import (
    AgentLogPage,
    AgentLogRecord,
    ReadOnlyAgentSession,
)
from symbian.status import Code, StatusError, StatusException

KEY_FILE = Path(__file__).parents[2] / "agent_service/test-agent.key"
KEY = KEY_FILE.read_bytes()


def _receive_exact(stream, count):
    data = bytearray()
    while len(data) < count:
        chunk = stream.recv(count - len(data))
        if not chunk:
            raise EOFError("Peer closed before the frame completed")
        data.extend(chunk)
    return bytes(data)


def _authenticate(stream, *, correct=True):
    server_nonce = bytes(range(32))
    stream.sendall(b"SAG1" + server_nonce)
    reply = _receive_exact(stream, 64)
    nonces = server_nonce + reply[:32]
    expected = hmac.digest(
        KEY, b"symbian-agent-client-v1" + nonces, hashlib.sha256
    )
    assert hmac.compare_digest(reply[32:], expected)
    proof = hmac.digest(
        KEY, b"symbian-agent-server-v1" + nonces, hashlib.sha256
    )
    stream.sendall(proof if correct else bytes(32))


def _request(stream):
    length = _native.agent_control_payload_length(_receive_exact(stream, 4))
    return msgpack.unpackb(_receive_exact(stream, length), raw=False)


def _reply(stream, request, body):
    response = msgpack.packb(
        {"v": 1, "id": request["id"], "kind": 4, "body": body},
        use_bin_type=True,
    )
    stream.sendall(len(response).to_bytes(4, "big") + response)


def _listener():
    listener = socket.socket()
    listener.bind(("127.0.0.1", 0))
    listener.listen(1)
    return listener, listener.getsockname()[1]


def test_native_control_prefix_rejects_oversized_payload():
    assert _native.agent_control_payload_length(b"\x00\x00\x00\x03") == 3
    with pytest.raises(StatusException):
        _native.agent_control_payload_length(b"\x00\x00\x10\x01")


def test_read_only_status_over_authenticated_socket():
    listener, port = _listener()
    observed = []

    def serve():
        try:
            with listener.accept()[0] as stream:
                _authenticate(stream)
                for body in (
                    {
                        "protocol_version": 1,
                        "maximum_control_bytes": 4096,
                        "maximum_requests": 16,
                        "capabilities": ["status", "logs", "workspace-list"],
                    },
                    {
                        "service": "symbian-agent",
                        "state": "ready",
                        "capabilities": ["status", "logs"],
                        "system": {"tick_count": 91, "tick_period_us": 1000},
                        "display": {"width_pixels": 640, "height_pixels": 360},
                    },
                    {
                        "records": [{"sequence": 5, "code": 2}],
                        "next_cursor": 5,
                        "gap": False,
                    },
                    {
                        "entries": [
                            {
                                "name": "café.txt",
                                "directory": False,
                                "read_only": True,
                                "size_bytes": 17,
                            }
                        ],
                        "next_offset": 1,
                        "more": False,
                    },
                ):
                    request = _request(stream)
                    observed.append(request)
                    _reply(stream, request, body)
        finally:
            listener.close()

    thread = threading.Thread(target=serve, daemon=True)
    thread.start()
    with ReadOnlyAgentSession.connect(
        "127.0.0.1", port, key_file=KEY_FILE
    ) as agent:
        status = agent.status()
        logs = agent.logs(after=4)
        workspace = agent.workspace_list()
    thread.join(timeout=5)
    assert not thread.is_alive()
    assert status.state == "ready"
    assert status.system.tick_count == 91
    assert status.display.width_pixels == 640
    assert [request["kind"] for request in observed] == [1, 2, 6, 7]
    assert logs.records[0].sequence == 5
    assert workspace.entries[0].name == "café.txt"
    assert workspace.entries[0].size_bytes == 17


def test_recent_logs_returns_newest_bounded_page(monkeypatch):
    """A fixed ring may require several reads to reach its newest records."""
    observed = []

    def page(_self, *, after=0, limit=8):
        observed.append((after, limit))
        end = min(after + limit, 18)
        return AgentLogPage(
            records=tuple(
                AgentLogRecord(sequence=number, code=2)
                for number in range(after + 1, end + 1)
            ),
            next_cursor=end,
            gap=after == 0,
        )

    monkeypatch.setattr(ReadOnlyAgentSession, "logs", page)
    session = object.__new__(ReadOnlyAgentSession)
    recent = session.recent_logs(limit=8)
    assert [record.sequence for record in recent.records] == list(range(11, 19))
    assert recent.next_cursor == 18
    assert recent.gap
    assert observed == [(0, 8), (8, 8), (16, 8)]


def test_phone_initiated_discovery_and_status():
    """A keyed probe locates the host without storing either IP address."""
    with socket.socket() as probe:
        probe.bind(("127.0.0.1", 0))
        port = probe.getsockname()[1]
    observed = []

    def phone():
        time.sleep(0.1)
        nonce = secrets.token_bytes(8)
        digest = hmac.digest(
            KEY, b"symbian-agent-discover-v1" + nonce, hashlib.sha256
        )
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as discovery:
            discovery.settimeout(3)
            discovery.sendto(b"SAGD1" + nonce + digest, ("127.0.0.1", 39104))
            reply, _ = discovery.recvfrom(64)
            expected = hmac.digest(
                KEY, b"symbian-agent-offer-v1" + nonce, hashlib.sha256
            )
            assert reply == b"SAGR1" + nonce + expected
        with socket.create_connection(("127.0.0.1", port)) as stream:
            _authenticate(stream)
            for body in (
                {
                    "protocol_version": 1,
                    "maximum_control_bytes": 4096,
                    "maximum_requests": 16,
                    "capabilities": ["status"],
                },
                {
                    "service": "symbian-agent",
                    "state": "ready",
                    "capabilities": ["status"],
                },
            ):
                request = _request(stream)
                observed.append(request)
                _reply(stream, request, body)

    thread = threading.Thread(target=phone, daemon=True)
    thread.start()
    with ReadOnlyAgentSession.accept(
        "127.0.0.1", port, key_file=KEY_FILE, timeout=5
    ) as agent:
        status = agent.status()
    thread.join(timeout=3)
    assert not thread.is_alive()
    assert status.state == "ready"
    assert [item["kind"] for item in observed] == [1, 2]


def test_wrong_discovery_key_does_not_get_host_offer():
    """A broadcast without the paired key receives no endpoint reply."""
    with socket.socket() as probe:
        probe.bind(("127.0.0.1", 0))
        port = probe.getsockname()[1]
    observed = []

    def phone():
        time.sleep(0.05)
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as discovery:
            discovery.settimeout(0.25)
            discovery.sendto(
                b"SAGD1" + bytes(8) + bytes(32), ("127.0.0.1", 39104)
            )
            try:
                observed.append(discovery.recvfrom(64))
            except TimeoutError:
                pass

    thread = threading.Thread(target=phone, daemon=True)
    thread.start()
    with pytest.raises(StatusError) as error:
        ReadOnlyAgentSession.accept(
            "127.0.0.1", port, key_file=KEY_FILE, timeout=0.35
        )
    thread.join(timeout=1)
    assert error.value.code == Code.DEADLINE_EXCEEDED
    assert observed == []


def test_rejects_wrong_server_proof_before_control_frames():
    listener, port = _listener()
    observed = []

    def serve():
        try:
            with listener.accept()[0] as stream:
                _authenticate(stream, correct=False)
                stream.settimeout(0.5)
                observed.append(stream.recv(1))
        finally:
            listener.close()

    thread = threading.Thread(target=serve, daemon=True)
    thread.start()
    with pytest.raises(StatusError) as error:
        ReadOnlyAgentSession.connect("127.0.0.1", port, key_file=KEY_FILE)
    thread.join(timeout=5)
    assert error.value.code == Code.UNAUTHENTICATED
    assert observed == [b""]


def test_read_only_status_has_one_aggregate_response_deadline():
    listener, port = _listener()

    def serve():
        try:
            with listener.accept()[0] as stream:
                _authenticate(stream)
                _reply(
                    stream,
                    _request(stream),
                    {
                        "protocol_version": 1,
                        "maximum_control_bytes": 4096,
                        "maximum_requests": 16,
                        "capabilities": ["status"],
                    },
                )
                _request(stream)
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
        "127.0.0.1", port, key_file=KEY_FILE, timeout=0.5
    ) as agent:
        started = time.monotonic()
        with pytest.raises(TimeoutError):
            agent.status()
        assert time.monotonic() - started < 0.8
    thread.join(timeout=5)
    assert not thread.is_alive()


def test_rejects_unsupported_hello_before_status():
    listener, port = _listener()
    observed = []

    def serve():
        try:
            with listener.accept()[0] as stream:
                _authenticate(stream)
                request = _request(stream)
                observed.append(request["kind"])
                _reply(
                    stream,
                    request,
                    {
                        "protocol_version": 2,
                        "maximum_control_bytes": 4096,
                        "maximum_requests": 16,
                        "capabilities": ["status"],
                    },
                )
        finally:
            listener.close()

    thread = threading.Thread(target=serve, daemon=True)
    thread.start()
    with pytest.raises(StatusError) as error:
        ReadOnlyAgentSession.connect("127.0.0.1", port, key_file=KEY_FILE)
    thread.join(timeout=5)
    assert error.value.code == Code.FAILED_PRECONDITION
    assert observed == [1]
