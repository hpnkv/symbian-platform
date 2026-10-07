"""Synchronous client for the native emulator's private control socket."""

import json
import math
import socket
import time
from pathlib import Path

from symbian.status import Code, StatusError


class Control:
    """Accesses captures, bounded input and guest exit records.

    The endpoint belongs to an explicitly started emulator. No hardware
    transport or arbitrary guest/host command execution is exposed.

    Args:
        endpoint: Absolute Unix socket in the emulator's private directory.
        timeout: Maximum total seconds per request, including reads.
    """

    def __init__(self, endpoint: Path, *, timeout: float = 5):
        self.endpoint = Path(endpoint)
        if not self.endpoint.is_absolute():
            raise StatusError(
                Code.INVALID_ARGUMENT, "Endpoint must be absolute"
            )
        if not math.isfinite(timeout) or timeout <= 0 or timeout > 60:
            raise StatusError(Code.INVALID_ARGUMENT, "Invalid control timeout")
        self.timeout = timeout

    def status(self) -> dict:
        """Returns bounded process exit records from the native kernel."""
        return self._request({"operation": "status"})

    def exit_report(self) -> dict:
        """Reads the final native status saved before frontend shutdown.

        This explicitly reads a saved report; it does not test liveness.
        """
        report = self.endpoint.with_name(self.endpoint.name + ".status.json")
        try:
            with report.open("rb") as file:
                data = file.read(1024 * 1024 + 1)
        except OSError as error:
            raise StatusError(Code.UNAVAILABLE, str(error)) from error
        if len(data) > 1024 * 1024:
            raise StatusError(
                Code.RESOURCE_EXHAUSTED, "Control report too large"
            )
        return _decode(data)

    def capture(self, name: str) -> dict:
        """Saves a new PNG of the guest screen in the endpoint directory."""
        return self._request({"operation": "capture", "name": name})

    def pointer(self, x: int, y: int, action: str) -> dict:
        """Queues a press, move or release at logical screen coordinates."""
        return self._request(
            {"operation": "pointer", "x": x, "y": y, "action": action}
        )

    def key(self, key: str, action: str) -> dict:
        """Queues a named guest key press or release."""
        return self._request({"operation": "key", "key": key, "action": action})

    def close_focused_task(self) -> dict:
        """Request normal AppArc shutdown of the focused window group."""
        return self._request({"operation": "task_close"})

    def close_task(self, uid: int) -> dict:
        """Request normal AppArc shutdown for an application by UID."""
        if type(uid) is not int or not 0 < uid <= 0xFFFFFFFF:
            raise StatusError(Code.INVALID_ARGUMENT, "Invalid task UID")
        return self._request({"operation": "task_close", "uid": uid})

    def _request(self, request: dict) -> dict:
        data = json.dumps(request, separators=(",", ":")).encode() + b"\n"
        if len(data) > 4096:
            raise StatusError(
                Code.INVALID_ARGUMENT, "Control request too large"
            )
        deadline = time.monotonic() + self.timeout
        try:
            with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as stream:
                stream.settimeout(self.timeout)
                stream.connect(str(self.endpoint))
                stream.sendall(data)
                response = bytearray()
                while not response.endswith(b"\n"):
                    remaining = deadline - time.monotonic()
                    if remaining <= 0:
                        raise TimeoutError("Control request timed out")
                    stream.settimeout(remaining)
                    part = stream.recv(4096)
                    if not part:
                        raise StatusError(
                            Code.DATA_LOSS, "Incomplete control response"
                        )
                    response.extend(part)
                    if len(response) > 1024 * 1024:
                        raise StatusError(
                            Code.RESOURCE_EXHAUSTED,
                            "Control response too large",
                        )
        except TimeoutError as error:
            raise StatusError(Code.DEADLINE_EXCEEDED, str(error)) from error
        except OSError as error:
            raise StatusError(Code.UNAVAILABLE, str(error)) from error
        return _decode(response)


def _decode(response: bytes | bytearray) -> dict:
    """Validates the versioned native JSON status envelope."""
    try:
        decoded = json.loads(response)
        if decoded["schema"] != "symbian.emulator-control/v1":
            raise ValueError("Unexpected control schema")
        status = decoded["status"]
        if type(status["code"]) is not int:
            raise ValueError("Invalid status code")
        code = Code(status["code"])
        if not isinstance(status["message"], str):
            raise ValueError("Invalid status message")
        if code != Code.OK:
            raise StatusError(code, status["message"])
        if not isinstance(decoded["result"], dict):
            raise ValueError("Invalid control result")
        return decoded["result"]
    except (ValueError, TypeError, KeyError, UnicodeError) as error:
        raise StatusError(Code.DATA_LOSS, "Invalid control response") from error
