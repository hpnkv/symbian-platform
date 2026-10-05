"""Real TCP client/server checks for the native RFC 8441 endpoint."""

import socket
import threading

import pytest

from symbian.status import StatusException
from symbian.websocket import WebSocketServer, WebSocketStream


def test_binary_messages_and_graceful_close():
    failures = []
    with WebSocketServer(maximum_message_bytes=32768) as server:

        def serve():
            try:
                with server.accept(timeout=3) as stream:
                    for _ in range(40):
                        message = stream.receive()
                        stream.send(message)
                    assert stream.receive() is None
            except BaseException as error:
                failures.append(error)

        thread = threading.Thread(target=serve)
        thread.start()
        stream = WebSocketStream.connect(
            "127.0.0.1", server.port, maximum_message_bytes=32768, timeout=3
        )
        try:
            for index in range(40):
                payload = bytes([index]) * 32768
                stream.send(payload)
                assert stream.receive() == payload
            stream.close()
        finally:
            stream.abort()
        thread.join(timeout=5)
        assert not thread.is_alive()
        assert failures == []


def test_wrong_path_rejected_and_listener_can_accept_again():
    with WebSocketServer(path="/expected") as server:

        def reject():
            with pytest.raises(StatusException):
                server.accept(timeout=1)

        thread = threading.Thread(target=reject)
        thread.start()
        with pytest.raises((StatusException, OSError)):
            WebSocketStream.connect("127.0.0.1", server.port, path="/wrong")
        thread.join(timeout=3)
        assert not thread.is_alive()


def test_handshake_has_an_aggregate_deadline():
    first, second = socket.socketpair()
    try:
        with pytest.raises(TimeoutError):
            WebSocketStream.from_socket(first, timeout=0.05)
        assert first.fileno() == -1
    finally:
        first.close()
        second.close()


def test_independent_http2_client_accepts_native_server():
    """Hyper-h2 validates extended CONNECT against the native server."""
    from h2.config import H2Configuration
    from h2.connection import H2Connection
    from h2.events import DataReceived, ResponseReceived

    from symbian import _native

    reference = H2Connection(H2Configuration(client_side=True))
    reference.initiate_connection()
    native = _native.WebSocketCodec(server=True)
    reference.receive_data(native.take_output())
    reference.send_headers(
        1,
        [
            (":method", "CONNECT"),
            (":scheme", "http"),
            (":authority", "symbian"),
            (":path", "/symbian-agent"),
            (":protocol", "websocket"),
            ("sec-websocket-version", "13"),
        ],
        end_stream=False,
    )
    native.feed(reference.data_to_send())
    events = reference.receive_data(native.take_output())
    response = next(
        event for event in events if isinstance(event, ResponseReceived)
    )
    assert (b":status", b"200") in response.headers
    assert native.open
    # RFC 6455 binary "hi", masked by 01 02 03 04.
    reference.send_data(1, b"\x82\x82\x01\x02\x03\x04ik")
    native.feed(reference.data_to_send())
    assert native.receive() == b"hi"
    native.send(b"reference")
    events = reference.receive_data(native.take_output())
    assert (
        b"".join(
            event.data for event in events if isinstance(event, DataReceived)
        )
        == b"\x82\x09reference"
    )


def test_native_client_accepts_independent_http2_server():
    """The native client waits for the reference's CONNECT setting."""
    from h2.config import H2Configuration
    from h2.connection import H2Connection
    from h2.events import DataReceived, RequestReceived
    from h2.settings import SettingCodes, Settings

    from symbian import _native

    reference = H2Connection(H2Configuration(client_side=False))
    reference.local_settings = Settings(
        client=False,
        initial_values={SettingCodes.ENABLE_CONNECT_PROTOCOL: 1},
    )
    reference.initiate_connection()
    native = _native.WebSocketCodec()
    native.feed(reference.data_to_send())
    events = reference.receive_data(native.take_output())
    request = next(
        event for event in events if isinstance(event, RequestReceived)
    )
    assert (b":protocol", b"websocket") in request.headers
    reference.send_headers(request.stream_id, [(":status", "200")])
    native.feed(reference.data_to_send())
    assert native.open
    reference.send_data(request.stream_id, b"\x82\x03ref")
    native.feed(reference.data_to_send())
    assert native.receive() == b"ref"
    native.send(b"hi")
    events = reference.receive_data(native.take_output())
    wire = b"".join(
        event.data for event in events if isinstance(event, DataReceived)
    )
    assert wire[:2] == b"\x82\x82"
    assert (
        bytes(wire[6 + index] ^ wire[2 + index] for index in range(2)) == b"hi"
    )
