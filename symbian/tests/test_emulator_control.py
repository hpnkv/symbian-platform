"""Control transport bounds and explicit final-report policy."""

import json
from pathlib import Path

import pytest

from symbian.cli.__main__ import main
from symbian.emulator import Control
from symbian.status import Code, StatusError


@pytest.mark.parametrize("timeout", [0, -1, 61, float("nan"), float("inf")])
def test_invalid_timeout_is_rejected(tmp_path, timeout):
    with pytest.raises(StatusError) as error:
        Control(tmp_path / "control.sock", timeout=timeout)
    assert error.value.code == Code.INVALID_ARGUMENT


def test_relative_endpoint_is_rejected():
    with pytest.raises(StatusError) as error:
        Control(Path("control.sock"))
    assert error.value.code == Code.INVALID_ARGUMENT


def test_absent_endpoint_does_not_fall_back_to_saved_status(tmp_path):
    endpoint = tmp_path / "control.sock"
    endpoint.with_name("control.sock.status.json").write_text(
        json.dumps(
            {
                "schema": "symbian.emulator-control/v1",
                "status": {"code": 0, "message": ""},
                "result": {"process_exits": []},
            }
        )
    )
    with pytest.raises(StatusError) as error:
        Control(endpoint).status()
    assert error.value.code == Code.UNAVAILABLE
    assert Control(endpoint).exit_report() == {"process_exits": []}


@pytest.mark.parametrize(
    "contents",
    [
        "{",
        "[]",
        '{"schema":"different"}',
        '{"schema":"symbian.emulator-control/v1",'
        '"status":{"code":true,"message":""}}',
    ],
)
def test_invalid_saved_report_has_data_loss(tmp_path, contents):
    endpoint = tmp_path / "control.sock"
    endpoint.with_name("control.sock.status.json").write_text(contents)
    with pytest.raises(StatusError) as error:
        Control(endpoint).exit_report()
    assert error.value.code == Code.DATA_LOSS


def test_saved_report_size_is_bounded(tmp_path):
    endpoint = tmp_path / "control.sock"
    endpoint.with_name("control.sock.status.json").write_bytes(
        b" " * (1024 * 1024 + 1)
    )
    with pytest.raises(StatusError) as error:
        Control(endpoint).exit_report()
    assert error.value.code == Code.RESOURCE_EXHAUSTED


def test_cli_status_requires_explicit_saved_flag(tmp_path, capsys):
    endpoint = tmp_path / "control.sock"
    args = ["emu", "status", "--endpoint", str(endpoint)]
    endpoint.with_name("control.sock.status.json").write_text(
        json.dumps(
            {
                "schema": "symbian.emulator-control/v1",
                "status": {"code": 0, "message": ""},
                "result": {"process_exits": []},
            }
        )
    )
    assert main(args) == 1
    assert json.loads(capsys.readouterr().out)["status"]["code"] == 14
    assert main(args + ["--saved"]) == 0
    assert json.loads(capsys.readouterr().out)["result"]["process_exits"] == []
