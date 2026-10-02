"""Native A11 status payloads and failures cross Python boundaries intact."""

import asyncio
import copy
import json

import httpx
import msgpack
import pytest
from pydantic import BaseModel, ValidationError

from pybind11_abseil import status as absl_status
from symbian import _native
from symbian.status import (
    Code,
    Status,
    StatusCode,
    StatusError,
    StatusException,
    StatusExceptionCasters,
    reraise_exceptions_as_status,
)


class Outcome(BaseModel):
    """A serializable policy result containing the actual native status."""

    status: Status


@pytest.fixture
def failure():
    return Status(
        code=StatusCode.NOT_FOUND,
        message="missing SDK symbol",
        details=[{"symbol": "example", "candidates": [1, 2]}],
    )


def test_public_status_is_native_and_roundtrips_payloads(failure):
    assert Status is _native.Status
    assert Code is StatusCode
    assert StatusError is StatusException
    assert _native._status_roundtrip(failure) == failure
    restored = Outcome.model_validate_json(
        Outcome(status=failure).model_dump_json()
    )
    assert restored.status == failure
    assert isinstance(restored.status, _native.Status)
    assert json.loads(failure.model_dump_json())["details"] == failure.details
    # A11's legacy compact encoder appends a two-field record to a buffered
    # packer. Structured details use the full native JSON/MessagePack bridge.
    packer = msgpack.Packer(autoreset=False)
    failure.to_msgpack(packer)
    assert msgpack.unpackb(bytes(packer)) == [5, failure.message]
    assert (
        Outcome.model_json_schema()["properties"]["status"]["title"] == "Status"
    )


def test_status_or_unwraps_value_or_raises_complete_status(failure):
    assert _native._status_or_value(Status.ok(), "result") == "result"
    with pytest.raises(StatusException) as caught:
        _native._status_or_value(failure, "never")
    assert caught.value.status == failure
    assert caught.value.as_dict()["details"] == failure.details


def test_python_callback_exceptions_preserve_status_and_cancel(failure):
    def fail():
        raise failure.to_exception()

    def cancel():
        raise asyncio.CancelledError()

    def unknown():
        raise ValueError("invalid input")

    assert _native._status_from_callback(fail) == failure
    assert _native._status_from_callback(cancel).code == StatusCode.CANCELLED
    assert _native._status_from_callback(unknown).code == StatusCode.UNKNOWN
    assert _native._status_from_callback(lambda: Status.ok()).is_ok()


def test_actual_abseil_caster_retains_payloads():
    original = absl_status.not_found_error("not found")
    original.SetPayload(
        "type.a11.dev/status-details+json", b'[{"path":"missing"}]'
    )
    native = _native._status_roundtrip(original)
    assert native.details == [{"path": "missing"}]
    restored = _native._absl_status_roundtrip(original)
    assert restored.AllPayloads() == original.AllPayloads()
    assert _native._status_roundtrip(restored) == native


def test_copy_and_property_updates_own_native_payloads(failure):
    clone = copy.deepcopy(failure)
    clone.details = [{"different": True}]
    clone.code = StatusCode.DATA_LOSS
    clone.message = "corrupt"
    assert failure.code == StatusCode.NOT_FOUND
    assert failure.details[0]["candidates"] == [1, 2]
    assert clone.model_dump(mode="json") == {
        "code": 15,
        "message": "corrupt",
        "details": [{"different": True}],
    }
    # Getter mutations do not silently mutate the native payload.
    clone.details.append({"uncommitted": True})
    assert len(clone.details) == 1


@pytest.mark.parametrize("code", [-1, 17, 65536])
def test_noncanonical_native_code_is_a_status_failure(code):
    with pytest.raises(StatusException) as caught:
        Status(code=code)
    assert caught.value.code == StatusCode.INVALID_ARGUMENT


def test_policy_exception_registry_and_validation_errors(failure):
    casters = StatusExceptionCasters()
    casters.register(ValueError, lambda _: failure)
    with pytest.raises(StatusException) as caught:
        with reraise_exceptions_as_status(casters):
            raise ValueError("application failure")
    assert caught.value.status == failure
    with pytest.raises(StatusException) as duplicate:
        casters.register(ValueError, lambda _: Status.ok())
    assert duplicate.value.code == StatusCode.ALREADY_EXISTS
    assert (
        Status.from_exception(httpx.ConnectError("offline")).code
        == StatusCode.UNAVAILABLE
    )
    with pytest.raises(ValidationError) as invalid:
        Outcome(status={"code": 17})
    assert (
        Status.from_exception(invalid.value).code == StatusCode.INVALID_ARGUMENT
    )


def test_parse_result_is_pydantic_and_rejects_invalid_code(failure):
    assert Status.parse_from_json(failure.model_dump_json()).parsed == failure
    result = Status.parse_from_json('{"code": 17, "message": "invalid"}')
    assert not result.is_ok()
    assert result.validation_status.code == StatusCode.OUT_OF_RANGE
    assert result.validation_status.details[0]["input"]
    assert result.model_dump(mode="json")["parsed"]["code"] == 2
