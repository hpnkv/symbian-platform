"""The CLI distinguishes host readiness, target claims and device authority."""

import json

import pytest

from symbian import device
from symbian.cli.__main__ import main


def test_doctor_does_not_claim_target_readiness(capsys):
    assert main(["doctor"]) == 0
    result = json.loads(capsys.readouterr().out)["result"]
    assert result["native_analysis"]
    assert not result["target"]["symbian_loader_verified"]
    assert not result["target"]["device_identity_verified"]


@pytest.mark.parametrize("operation", sorted(device.RECOVERY_OPERATIONS))
def test_recovery_has_no_executor(operation, capsys):
    assert main(["device", "policy", operation]) == 0
    result = json.loads(capsys.readouterr().out)["result"]
    assert result["tier"] == "outside-agent-authority"
    assert not result["executor_available"]
    assert not result["authorized"]


def test_dangerous_operation_needs_human_broker(capsys):
    assert main(["device", "policy", "reboot"]) == 0
    result = json.loads(capsys.readouterr().out)["result"]
    assert result["tier"] == "human-authorization-required"
    assert not result["executor_available"]


def test_unknown_device_operation_is_denied(capsys):
    assert main(["device", "policy", "arbitrary-command"]) == 1
    status = json.loads(capsys.readouterr().out)["status"]
    assert status["name"] == "PERMISSION_DENIED"


def test_flashing_is_not_a_command():
    with pytest.raises(SystemExit):
        main(["device", "flash"])


def test_missing_input_returns_structured_error(tmp_path, capsys):
    assert main(["inspect", str(tmp_path / "missing.o")]) == 1
    status = json.loads(capsys.readouterr().out)["status"]
    assert status["name"] == "NOT_FOUND"


def test_malformed_artifact_returns_native_status(tmp_path, capsys):
    artifact = tmp_path / "bad.o"
    artifact.write_bytes(b"broken")
    assert main(["inspect", str(artifact)]) == 1
    assert json.loads(capsys.readouterr().out)["status"]["name"] == "DATA_LOSS"
