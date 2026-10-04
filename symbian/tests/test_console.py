"""In-memory console boundary and generated workflow tests."""

import asyncio
import subprocess
import sys
import threading
import time
from pathlib import Path
from types import SimpleNamespace

import httpx
import pytest

from symbian.cli.__main__ import main
from symbian.console.catalog import catalog
from symbian.console.client import ConsoleClient
from symbian.console.defaults import initial_value
from symbian.console.devices import usb_class_name
from symbian.console.models import (
    CommandRequest,
    ConsoleContext,
    DeviceInspectRequest,
)
from symbian.console.presentation import (
    PRESENTATIONS,
    summarize_command,
    task_steps,
)
from symbian.console.runner import run_cli
from symbian.console.selection import DeviceSelection, device_labels
from symbian.console.service import ConsoleService, create_app
from symbian.console.syntax import highlighted_spans
from symbian.console.validation import validate_task
from symbian.device.connection import ConnectedDevice, DeviceListResult
from symbian.device.usb_models import UsbDeviceDescriptor
from symbian.process import run
from symbian.status import Status, StatusCode, StatusException


def test_catalog_tracks_cli_and_excludes_frontend() -> None:
    commands = catalog().commands
    paths = {command.path for command in commands}
    assert ("doctor",) in paths
    assert ("device", "info") in paths
    assert ("console",) not in paths
    assert next(
        command for command in commands if command.path == ("device", "info")
    ).arguments
    assert paths == set(PRESENTATIONS)


def test_guided_steps_and_input_validation() -> None:
    commands = {spec.path: spec for spec in catalog().commands}
    application = commands[("init",)]
    assert [step.title for step in task_steps(application)] == [
        "Application",
        "SDK and build",
        "Emulator",
    ]
    issues = validate_task(
        application, {"destination": "/tmp/new-app", "uid3": "no"}
    )
    assert [(issue.field, issue.message) for issue in issues] == [
        ("uid3", "Enter a 32-bit UID in decimal or 0x hexadecimal.")
    ]
    firmware = commands[("firmware", "import")]
    assert [step.title for step in task_steps(firmware)] == [
        "Source",
        "Identity",
        "Environment",
    ]
    source_issues = validate_task(
        firmware, {"rom": "image.rom", "bundle": "other.zip"}
    )
    assert source_issues[0].field == "source"
    export_steps = task_steps(commands[("firmware", "export")])
    assert export_steps[0].title == "Inputs"
    assert [argument.name for argument in export_steps[0].arguments] == [
        "reference",
        "destination",
    ]
    assert export_steps[1].optional


def test_task_result_puts_useful_facts_before_details() -> None:
    from symbian.console.models import CommandResult

    result = CommandResult(
        path=("init",),
        result={"project": "/tmp/phone-app", "initial_build": True},
    )
    summary = summarize_command(result)
    assert summary.facts[0].value == "/tmp/phone-app"
    assert "Open the application folder" in summary.message


def test_cli_runner_isolates_process_replacement(monkeypatch) -> None:
    captured = []

    def child(command, **options):
        captured.append((command, options))
        return subprocess.CompletedProcess(
            command,
            0,
            stdout=(
                '{"schema":"symbian.cli/v1","status":'
                '{"code":0,"message":""},'
                '"result":{"project":"/tmp/example"}}'
            ),
            stderr="",
        )

    monkeypatch.setattr("symbian.console.runner.subprocess.run", child)
    result = run_cli(CommandRequest(path=("init",), argv=("/tmp/example",)))
    assert result.result == {"project": "/tmp/example"}
    assert captured[0][0][-4:] == [
        "init",
        "/tmp/example",
        "--output-format=json",
        "--non-interactive",
    ]
    assert captured[0][1]["capture_output"] is True
    assert captured[0][1]["stdin"] == subprocess.DEVNULL


def test_cli_runner_uses_selected_sdk_for_discovery(monkeypatch) -> None:
    captured = []

    def child(command, **options):
        captured.append(options)
        return subprocess.CompletedProcess(
            command,
            0,
            stdout=(
                '{"schema":"symbian.cli/v1","status":'
                '{"code":0,"message":""},"result":{}}'
            ),
            stderr="",
        )

    monkeypatch.setattr("symbian.console.runner.subprocess.run", child)
    run_cli(
        CommandRequest(
            path=("doctor",),
            sdk_manifest="/tmp/selected-sdk/sdk.json",
        )
    )
    assert captured[0]["env"]["SYMBIAN_SDK_MANIFEST"] == (
        "/tmp/selected-sdk/sdk.json"
    )


def test_cli_runner_preserves_child_status(monkeypatch) -> None:
    def child(command, **_options):
        return subprocess.CompletedProcess(
            command,
            1,
            stdout=(
                '{"schema":"symbian.cli/v1","status":'
                '{"code":9,"message":"SDK unavailable",'
                '"details":[{"reason":"missing source"}]}}'
            ),
            stderr="",
        )

    monkeypatch.setattr("symbian.console.runner.subprocess.run", child)
    with pytest.raises(StatusException) as caught:
        run_cli(CommandRequest(path=("init",), argv=("/tmp/example",)))
    assert caught.value.code == StatusCode.FAILED_PRECONDITION
    assert caught.value.status.details == [{"reason": "missing source"}]


def test_status_round_trip_and_command_dispatch() -> None:
    service = ConsoleService(
        command_runner=lambda arguments: {"command": arguments.command}
    )
    client = ConsoleClient(create_app(service))

    async def exercise() -> None:
        result = await client.run(CommandRequest(path=("doctor",)))
        assert result.result == {"command": "doctor"}
        with pytest.raises(StatusException) as missing:
            await client.run(CommandRequest(path=("missing",)))
        assert missing.value.code == StatusCode.NOT_FOUND
        with pytest.raises(StatusException) as invalid:
            await client.run(CommandRequest(path=("doctor",), argv=("--help",)))
        assert invalid.value.code == StatusCode.INVALID_ARGUMENT

    asyncio.run(exercise())


def test_native_status_details_survive_asgi_transport() -> None:
    original = Status(
        code=StatusCode.FAILED_PRECONDITION,
        message="probe unavailable",
        details=[{"interface": 3}],
    )

    def fail(_arguments):
        raise original.to_exception()

    client = ConsoleClient(create_app(ConsoleService(command_runner=fail)))

    async def exercise() -> None:
        with pytest.raises(StatusException) as caught:
            await client.run(CommandRequest(path=("doctor",)))
        assert caught.value.status.model_dump(mode="json") == (
            original.model_dump(mode="json")
        )

    asyncio.run(exercise())


def test_request_validation_uses_status() -> None:
    application = create_app()

    async def exercise() -> None:
        async with httpx.AsyncClient(
            transport=httpx.ASGITransport(app=application),
            base_url="http://symbian-console.local",
        ) as connection:
            response = await connection.post(
                "/api/v1/devices/inspect",
                json={"operation": "arbitrary-write"},
            )
        assert response.status_code == 400
        assert Status.model_validate(response.json()).code == (
            StatusCode.INVALID_ARGUMENT
        )

    asyncio.run(exercise())


def test_protocol_routes_use_bounded_existing_probe(monkeypatch) -> None:
    captured = []

    def probe(selector, **options):
        captured.append((selector, options))
        raise Status(
            code=StatusCode.UNAVAILABLE,
            message="test probe stopped",
        ).to_exception()

    monkeypatch.setattr("symbian.device.connection.inspect_device", probe)
    service = ConsoleService()
    with pytest.raises(StatusException):
        service.inspect_device(
            DeviceInspectRequest(
                selector="selected", operation="mtp-list", limit=3
            )
        )
    with pytest.raises(StatusException):
        service.inspect_device(
            DeviceInspectRequest(selector="selected", operation="obex")
        )
    assert captured[0] == (
        "selected",
        {
            "probe_protocol": False,
            "at_status": False,
            "mtp": True,
            "mtp_list": 3,
            "obex_connect": False,
        },
    )
    assert captured[1][1]["obex_connect"] is True


def test_highlighting_preserves_source_exactly() -> None:
    source = '{"device": 808, "name": "Nokia", "ready": true}\n'
    spans = highlighted_spans(source, "JSON")
    assert "".join(fragment for _, fragment in spans) == source
    assert {category for category, _ in spans} >= {"string", "number"}


def test_console_cli_launches_frontend(monkeypatch) -> None:
    captured = []
    monkeypatch.setattr(
        "symbian.console.launcher.launch_detached", captured.append
    )
    assert main(["console"]) == 0
    assert captured == [None]


def test_console_cli_forwards_workdir(monkeypatch, tmp_path) -> None:
    captured = []
    monkeypatch.setattr(
        "symbian.console.launcher.launch_detached", captured.append
    )
    assert main(["console", "--workdir", str(tmp_path)]) == 0
    assert captured == [tmp_path]


def test_standalone_application_run_uses_selected_firmware(
    monkeypatch, capsys
) -> None:
    """TOML-only projects reach the shared emulator launcher."""
    project = Path(__file__).resolve().parents[2] / "examples/gui_app"
    launched = []

    def launch(arguments, *, raise_errors):
        launched.append((arguments, raise_errors))
        return 0

    monkeypatch.setattr("symbian.emulator.launch.main", launch)
    assert (
        main(
            [
                "app",
                "run",
                "--project",
                str(project),
                "--firmware",
                "phone-firmware",
                "--output-format=json",
            ]
        )
        == 0
    )
    assert launched == [
        (["--project", str(project), "--firmware", "phone-firmware"], True)
    ]
    assert '"frontend_exit": 0' in capsys.readouterr().out


def test_console_launcher_detaches_from_terminal(monkeypatch) -> None:
    captured = []

    def spawn(command, **options):
        captured.append((command, options))

    monkeypatch.setattr("symbian.console.launcher.subprocess.Popen", spawn)
    monkeypatch.setattr(
        "symbian.console.launcher.find_spec", lambda name: object()
    )
    monkeypatch.setattr(
        "symbian.console.launcher.terminal_screen_index", lambda: 1
    )
    from symbian.console.launcher import launch_detached

    launch_detached()
    assert captured[0][0][-2:] == [
        "-m",
        "symbian.console.web_frontend.app",
    ]
    assert captured[0][1]["stdin"] == subprocess.DEVNULL
    assert captured[0][1]["stdout"] == subprocess.DEVNULL
    assert captured[0][1]["cwd"] == Path.cwd().resolve()
    assert captured[0][1]["env"]["SYMBIAN_CONSOLE_SCREEN_INDEX"] == "1"


def test_build_output_reaches_console_before_process_finishes(
    monkeypatch, tmp_path: Path
) -> None:
    """The build view can show output while a tool is still working."""
    log = tmp_path / "build.log"
    log.touch()
    monkeypatch.setenv("SYMBIAN_CONSOLE_BUILD_LOG", str(log))
    output: list[str] = []

    def invoke() -> None:
        output.append(
            run(
                [
                    sys.executable,
                    "-u",
                    "-c",
                    (
                        "import time; print('first'); "
                        "time.sleep(.4); print('second')"
                    ),
                ],
                cwd=tmp_path,
                timeout=3,
            )
        )

    worker = threading.Thread(target=invoke)
    worker.start()
    deadline = time.monotonic() + 2
    while "first" not in log.read_text() and time.monotonic() < deadline:
        time.sleep(0.01)
    assert "first" in log.read_text()
    assert worker.is_alive()
    worker.join(timeout=3)
    assert output == ["first\nsecond"]


def test_run_output_reaches_console_before_emulator_finishes(
    monkeypatch, tmp_path: Path
) -> None:
    """CLI diagnostics must stream while the emulator run is still active."""
    executable = tmp_path / "fake-sdk"
    executable.write_text(
        f"#!{sys.executable}\n"
        "import os, sys, time\n"
        "print('Preparing emulator', file=sys.stderr, flush=True)\n"
        "time.sleep(.4)\n"
        'print(\'{"schema":"symbian.cli/v1","status":{"code":0,'
        '"message":""},"result":{"frontend_exit":0}}\')\n'
    )
    executable.chmod(0o755)
    monkeypatch.setattr(
        "symbian.console.runner.sys",
        SimpleNamespace(executable=str(executable)),
    )
    log = tmp_path / "run.log"
    result = []

    def invoke() -> None:
        result.append(
            run_cli(
                CommandRequest(
                    path=("app", "run"),
                    build_log_path=str(log),
                    run_session_path=str(tmp_path / "session.txt"),
                )
            )
        )

    worker = threading.Thread(target=invoke)
    worker.start()
    deadline = time.monotonic() + 2
    while (
        not log.exists() or "Preparing emulator" not in log.read_text()
    ) and time.monotonic() < deadline:
        time.sleep(0.01)
    assert "Preparing emulator" in log.read_text()
    assert worker.is_alive()
    worker.join(timeout=3)
    assert result[0].result == {"frontend_exit": 0}


def _phone(
    product_id: int,
    selector: str,
    anchor: str,
    basis: str = "usb-serial",
) -> ConnectedDevice:
    return ConnectedDevice(
        selector=selector,
        manufacturer="Nokia",
        product="808 PureView",
        vendor_id=0x0421,
        product_id=product_id,
        location_id=1234,
        identity_anchor=anchor,
        identity_basis=basis,
    )


def test_device_selection_survives_serial_backed_reconnect_only() -> None:
    phone = _phone(0x05D1, "pc-suite", "same-phone")
    new_mode = _phone(0x0661, "mass-storage", "same-phone")
    stranger = _phone(0x0661, "stranger", "other-phone")
    state = DeviceSelection()
    assert state.refresh((phone,)) == "pc-suite"
    assert state.refresh(()) is None
    assert state.refresh((stranger,)) is None
    assert state.refresh((new_mode,)) == "mass-storage"
    state.clear()
    assert state.refresh((new_mode,)) is None

    port_phone = _phone(0x05D1, "port", "same-port", "port-location")
    port_state = DeviceSelection()
    assert port_state.refresh((port_phone,)) == "port"
    assert port_state.refresh(()) is None
    assert port_state.refresh((port_phone,)) is None
    labels = device_labels((phone, stranger))
    assert all("usb:" not in label for label in labels)
    assert set(labels.values()) == {"pc-suite", "stranger"}


def test_forms_show_resolved_current_values() -> None:
    context = ConsoleContext(
        workspace="/tmp/application",
        project="/tmp/application",
        sdk_manifest="/tmp/sdk/sdk.json",
        firmware_store="/tmp/firmware",
        devices=(_phone(0x05D1, "phone", "anchor"),),
    )
    commands = {spec.path: spec for spec in catalog().commands}
    application = commands[("app", "build")]
    project = next(
        arg for arg in application.arguments if arg.name == "project"
    )
    assert initial_value(context, application.path, project, "phone") == (
        "/tmp/application"
    )
    assert usb_class_name(9) == "Hub (0x09)"


def test_context_reuses_device_discovery_until_usb_changes(monkeypatch) -> None:
    service = ConsoleService()
    phone = _phone(0x05D1, "phone", "anchor")
    state = {"address": 1, "discoveries": 0}

    def usb_inventory():
        return [
            UsbDeviceDescriptor(
                vendor_id=0x0421,
                product_id=0x05D1,
                bus=0,
                address=state["address"],
                ports=(1, 2),
                device_class=2,
                configuration_count=1,
            )
        ]

    def devices():
        state["discoveries"] += 1
        return DeviceListResult(devices=(phone,)).model_dump(mode="json")

    monkeypatch.setattr(service, "usb_inventory", usb_inventory)
    monkeypatch.setattr("symbian.device.connection.list_devices", devices)
    service.current_context()
    service.current_context()
    assert state["discoveries"] == 1
    state["address"] = 2
    service.current_context()
    assert state["discoveries"] == 2
