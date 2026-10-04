"""Bounded AT query parsing and private-identity filtering."""

import socket
import threading

from symbian.device.at import _exchange, _safe_identity


def test_at_exchange_accepts_complete_final_line():
    host, modem = socket.socketpair()

    def respond():
        assert modem.recv(64) == b"AT+CGMM\r"
        modem.sendall(
            b"AT+CGMM\r\r\nNokia 808 PureView\r\nOK\r\n" b'+CMTI: "ME",1\r\n'
        )

    worker = threading.Thread(target=respond)
    worker.start()
    try:
        state, lines = _exchange(host.fileno(), b"AT+CGMM\r")
        assert state == "ok"
        assert lines == ["Nokia 808 PureView"]
    finally:
        worker.join(timeout=2)
        host.close()
        modem.close()


def test_at_identity_filter_suppresses_serial_like_response():
    assert _safe_identity(["Nokia 808 PureView"]) == "Nokia 808 PureView"
    assert _safe_identity(["123456789012345"]) is None
    assert _safe_identity(["bad\x00value"]) is None
