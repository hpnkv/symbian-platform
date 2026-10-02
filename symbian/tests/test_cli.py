"""The CLI distinguishes host readiness, target claims and device authority."""

import argparse
import json

import pytest

from symbian import device
from symbian.cli.__main__ import _parser
from symbian.cli.__main__ import main as raw_main
from symbian.cli.output import render
from symbian.tests.cli_json import main


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


def test_device_list_is_readable_by_default_and_json_is_explicit(
    monkeypatch, capsys
):
    sample = {
        "schema": "symbian.device-list/v1",
        "devices": [
            {
                "manufacturer": "Nokia",
                "product": "808 PureView",
                "selector": "usb:test",
                "transport": "usb",
                "capabilities": ["inspect-usb", "stage-sis"],
                "volumes": [
                    {
                        "mount": "/Volumes/PHONE",
                        "disk": "disk4",
                        "filesystem": "FAT32",
                        "free_bytes": 1024,
                        "stage_sis": True,
                    }
                ],
            }
        ],
    }
    monkeypatch.setattr(
        "symbian.device.connection.list_devices", lambda: sample
    )
    assert raw_main(["device", "list"]) == 0
    human = capsys.readouterr().out
    assert "1. Nokia 808 PureView" in human
    assert "Selector: usb:test" in human
    assert "Storage: /Volumes/PHONE" in human
    assert '"schema"' not in human
    for arguments in (
        ["--output-format=json", "device", "list"],
        ["device", "list", "--output-format=json"],
    ):
        assert raw_main(arguments) == 0
        assert json.loads(capsys.readouterr().out)["result"] == sample


def test_default_error_is_readable_and_goes_to_stderr(tmp_path, capsys):
    assert raw_main(["inspect", str(tmp_path / "missing.o")]) == 1
    captured = capsys.readouterr()
    assert captured.out == ""
    assert "Error (NOT_FOUND):" in captured.err


def test_nested_help_describes_command_and_options(capsys):
    with pytest.raises(SystemExit) as exit_info:
        raw_main(["firmware", "import", "--help"])
    assert exit_info.value.code == 0
    help_text = capsys.readouterr().out
    assert (
        "Import a ROM/Z source into the independent content store." in help_text
    )
    assert "Shared content store for imported ROM" in help_text
    assert "--output-format {human,json}" in help_text


def test_generic_human_summary_formats_nested_package_data():
    response = {
        "schema": "symbian.cli/v1",
        "status": {"code": 0, "name": "OK", "message": ""},
        "result": {
            "package": "hello.sis",
            "size_bytes": 2048,
            "registered": True,
            "languages": ["en", "fr"],
        },
    }
    human = render(response, "human", "package")
    assert "SIS package" in human
    assert "2.0 KiB (2,048 bytes)" in human
    assert "Registered:" in human and "Yes" in human
    assert "en, fr" in human
    assert '"schema"' not in human
    assert json.loads(render(response, "json", "package")) == response


def test_long_recommendations_are_separate_lines():
    response = {
        "status": {"code": 0, "name": "OK", "message": ""},
        "result": {
            "next_steps": [
                "Import separately supplied ROM/Z material into the shared "
                "store",
                "Preserve artifacts with an independently held offline "
                "reference copy",
                "Build an application with the selected SDK and target "
                "architecture",
            ]
        },
    }
    human = render(response, "human", "doctor")
    assert "  1. Import separately" in human
    assert "  2. Preserve artifacts" in human
    assert "  3. Build an application" in human


def test_every_cli_command_and_option_has_help_text():
    def inspect(parser):
        assert parser.description
        for action in parser._actions:
            if isinstance(action, argparse._SubParsersAction):
                assert len(action._choices_actions) == len(action.choices)
                assert all(choice.help for choice in action._choices_actions)
                for child in action.choices.values():
                    inspect(child)
            elif action.dest != "help":
                assert action.help, (parser.prog, action.dest)

    inspect(_parser())
