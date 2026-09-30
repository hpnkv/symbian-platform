"""Canonical Abseil status codes at the Python and command-line boundary."""

from enum import IntEnum


class Code(IntEnum):
    """Canonical status values shared with the native library."""

    OK = 0
    CANCELLED = 1
    UNKNOWN = 2
    INVALID_ARGUMENT = 3
    DEADLINE_EXCEEDED = 4
    NOT_FOUND = 5
    ALREADY_EXISTS = 6
    PERMISSION_DENIED = 7
    RESOURCE_EXHAUSTED = 8
    FAILED_PRECONDITION = 9
    ABORTED = 10
    OUT_OF_RANGE = 11
    UNIMPLEMENTED = 12
    INTERNAL = 13
    UNAVAILABLE = 14
    DATA_LOSS = 15
    UNAUTHENTICATED = 16


class StatusError(Exception):
    """An operation failed with a canonical, machine-readable status."""

    def __init__(self, code: int | Code, message: str):
        self.code = Code(code)
        self.message = message
        super().__init__(message)

    def as_dict(self) -> dict:
        """Returns a status object for a CLI response."""
        return {
            "code": int(self.code),
            "name": self.code.name,
            "message": self.message,
        }
