"""The CLI distinguishes host readiness, target claims and device authority."""

import argparse
import json
import re
from types import SimpleNamespace

import pytest

from symbian import device
from symbian.agent import AgentLogPage, AgentLogRecord, ReadOnlyAgentSession
from symbian.cli import output
from symbian.cli.__main__ import _parser
from symbian.cli.__main__ import main as raw_main
from symbian.cli.output import render
from symbian.tests.cli_json import main


def test_bare_cli_shows_top_level_help(capsys):
    assert raw_main([]) == 0
    captured = capsys.readouterr()
    assert captured.err == ""
    assert "usage: symbian" in captured.out
    assert "device" in captured.out


def test_application_commands_use_application_help(capsys):
    assert raw_main([]) == 0
    help_text = capsys.readouterr().out
    assert "Build an ARM/E32 application executable" in help_text
    assert "Build an unsigned SISX application package" in help_text
    assert "Build an ARM/E32 experiment" not in help_text


def test_help_colours_commands_and_nested_options(monkeypatch, capsys):
    monkeypatch.setenv("CLICOLOR_FORCE", "1")
    monkeypatch.delenv("NO_COLOR", raising=False)
    assert raw_main([]) == 0
    root_help = capsys.readouterr().out
    assert "\x1b[1;36musage:\x1b[0m" in root_help
    assert "\x1b[1;32mdoctor\x1b[0m" in root_help

    with pytest.raises(SystemExit) as exit_info:
        raw_main(["device", "info", "--help"])
    assert exit_info.value.code == 0
    nested_help = capsys.readouterr().out
    assert "\x1b[1;32m--mtp\x1b[0m" in nested_help

    monkeypatch.setenv("NO_COLOR", "1")
    assert raw_main([]) == 0
    assert "\x1b[" not in capsys.readouterr().out


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


def test_agent_listen_logs_uses_authenticated_session(monkeypatch, capsys):
    calls = []

    class Agent:
        peer_ip = "192.0.2.7"

        def __enter__(self):
            return self

        def __exit__(self, *_exc):
            pass

        def recent_logs(self, *, limit):
            calls.append(("recent", limit))
            return AgentLogPage(
                records=(AgentLogRecord(sequence=5, code=2),),
                next_cursor=5,
                gap=False,
            )

        def logs(self, *, after, limit):
            calls.append(("cursor", after, limit))
            return AgentLogPage(records=(), next_cursor=after, gap=False)

    monkeypatch.setattr(
        ReadOnlyAgentSession,
        "accept",
        classmethod(lambda _cls, *_args, **_kwargs: Agent()),
    )
    base = ["--output-format=json", "agent", "listen", "--key-file", "key"]
    assert raw_main(base + ["--logs"]) == 0
    result = json.loads(capsys.readouterr().out)["result"]
    assert result["peer_ip"] == "192.0.2.7"
    assert result["logs"]["records"][0]["sequence"] == 5
    assert raw_main(base + ["--logs", "--after", "5", "--limit", "2"]) == 0
    result = json.loads(capsys.readouterr().out)["result"]
    assert result["logs"]["next_cursor"] == 5
    assert calls == [("recent", 8), ("cursor", 5, 2)]
    assert raw_main(base + ["--after", "5"]) == 1
    assert json.loads(capsys.readouterr().out)["status"]["name"] == (
        "INVALID_ARGUMENT"
    )


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


def test_device_info_human_names_functions_and_respects_terminal_color(
    monkeypatch,
):
    response = {
        "status": {"code": 0, "name": "OK", "message": ""},
        "result": {
            "device": {
                "manufacturer": "Nokia",
                "product": "808 PureView",
                "vendor_id": 0x0421,
                "product_id": 0x05D1,
                "selector": "usb:0421:05d1:example",
                "interface_profile": "composite",
                "interfaces": [
                    {
                        "number": 0,
                        "class_code": 6,
                        "subclass_code": 1,
                        "protocol_code": 1,
                        "declared_name": "MTP",
                        "function": "Still imaging / PTP transport",
                        "endpoint_count": 3,
                        "alternate_setting": 0,
                    }
                ],
                "volumes": [],
            },
            "scope": "USB descriptors",
        },
    }
    monkeypatch.setenv("TERM", "xterm")
    monkeypatch.delenv("NO_COLOR", raising=False)
    monkeypatch.setattr(
        output.sys, "stdout", SimpleNamespace(isatty=lambda: True)
    )
    human = render(response, "human", "device", "info")
    plain = re.sub(r"\x1b\[[0-9;]*m", "", human)
    assert "MTP — Still imaging / PTP transport" in plain
    assert "3 endpoints; alternate 0" in plain
    assert "\x1b[1;36mUSB interfaces" in human
    monkeypatch.setenv("NO_COLOR", "1")
    assert "\x1b[" not in render(response, "human", "device", "info")


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
