"""Opt-in resident agent behavior in the pinned emulator."""

import json
import os
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

import pytest
from PIL import Image

from symbian import _native, toolchain
from symbian.agent import ReadOnlyAgentSession
from symbian.emulator import Control
from symbian.emulator.background import (
    background_environment,
    executable_for_session,
)
from symbian.emulator.firmware import EUSER_808, ROM_808
from symbian.emulator.launch import _digest, _stop
from symbian.project.sdk import AppSdk
from symbian.status import Code, StatusError

ROOT = Path(__file__).parents[2]
TEST_KEY = ROOT / "agent_service/test-agent.key"
ESOCK_808 = "555a8c00ced78b7350f9f08f510c8e742cdc328f1f2780420c813b1fe2eb4a08"
INSOCK_808 = "d619f48a95b16aa89b3f34b813c3182d9440f80a6f23cead49b9f3aa988e9110"

pytestmark = pytest.mark.skipif(
    not os.environ.get("SYMBIAN_AGENT_SERVICE_GUEST"),
    reason="Set SYMBIAN_AGENT_SERVICE_GUEST for the pinned emulator experiment",
)


@pytest.fixture(scope="module")
def service_image(tmp_path_factory):
    """Build the service against a clean selected SDK."""
    sdk = AppSdk.load(Path(os.environ["SYMBIAN_SDK_MANIFEST"]))
    output = tmp_path_factory.mktemp("agent-service")
    project = output / "project"
    shutil.copytree(
        ROOT / "agent_service",
        project,
        ignore=shutil.ignore_patterns(".symbian"),
    )
    presets = project / "CMakePresets.json"
    data = json.loads(presets.read_text())
    cache = data["configurePresets"][0]["cacheVariables"]
    cache["SYMBIAN_SDK_PREFIX"] = str(sdk.prefix)
    data["configurePresets"][0]["toolchainFile"] = str(
        sdk.prefix / "cmake/symbian-arm.cmake"
    )
    presets.write_text(json.dumps(data))
    private_key = os.environ.get("SYMBIAN_AGENT_PRIVATE_GUEST_KEY_FILE")
    variables = None
    if private_key:
        variables = {
            "SYMBIAN_AGENT_PRIVATE_KEY_FILE": private_key,
            "SYMBIAN_AGENT_PAIRING_CODE": os.environ[
                "SYMBIAN_AGENT_PRIVATE_GUEST_PAIRING_CODE"
            ],
        }
    report = toolchain.build(
        project,
        output / "build",
        str(sdk.compiler),
        str(sdk.linker),
        architecture="armv6",
        cmake_variables=variables,
    )
    return Path(report["artifact"])


def _connect(timeout=10.0):
    return ReadOnlyAgentSession.connect(
        "127.0.0.1",
        39101,
        key_file=Path(
            os.environ.get("SYMBIAN_AGENT_PRIVATE_GUEST_KEY_FILE", TEST_KEY)
        ),
        timeout=timeout,
    )


@pytest.mark.skipif(
    not os.environ.get("SYMBIAN_AGENT_PRIVATE_GUEST_KEY_FILE"),
    reason="Requires an ephemeral private guest key",
)
def test_private_agent_discovers_host(service_image, tmp_path):
    """A private guest connects outward after keyed local discovery."""
    golden = ROOT / ".symbian/instances/delight-import-01"
    instance = tmp_path / "instance"
    shutil.copytree(golden, instance)
    guest_bin = instance / "data/drives/rm-807/c/sys/bin"
    guest_bin.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(service_image, guest_bin / "agent_service.exe")
    (instance / "config.yml").write_text(
        "data-storage: data\ncpu: dynarmic\ndevice: 0\nlanguage: 1\n"
        "enable-gdb-stub: false\nlog-svc: true\n"
    )
    executable = ROOT / "build/eka2l1/bin/EKA2L1.app/Contents/MacOS/EKA2L1"
    with tempfile.TemporaryDirectory(
        prefix="agent-outbound-", dir="/tmp"
    ) as name:
        env = dict(os.environ)
        env.update(
            EKA2L1_DATA_ROOT=str(instance),
            EKA2L1_EXPERIMENTAL_SVC_PROFILE="rm807-113.010.1508",
            **background_environment(),
        )
        with (tmp_path / "frontend.log").open("w") as log:
            process = subprocess.Popen(
                [
                    executable_for_session(executable, Path(name)),
                    "--device",
                    "RM-807",
                    "--run",
                    "C:\\sys\\bin\\agent_service.exe",
                ],
                env=env,
                stdout=log,
                stderr=subprocess.STDOUT,
            )
            try:
                with ReadOnlyAgentSession.accept(
                    "0.0.0.0",
                    39103,
                    key_file=Path(
                        os.environ["SYMBIAN_AGENT_PRIVATE_GUEST_KEY_FILE"]
                    ),
                    timeout=25,
                ) as agent:
                    assert agent.status().state == "ready"
            finally:
                _stop(process)


@pytest.mark.parametrize("action", ["stop", "background"])
def test_local_window_controls(service_image, tmp_path, action):
    """The guest panel stops locally or leaves the service running."""
    golden = ROOT / ".symbian/instances/delight-import-01"
    instance = tmp_path / "instance"
    shutil.copytree(golden, instance)
    guest_bin = instance / "data/drives/rm-807/c/sys/bin"
    guest_bin.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(service_image, guest_bin / "agent_service.exe")
    (instance / "config.yml").write_text(
        "data-storage: data\ncpu: dynarmic\ndevice: 0\nlanguage: 1\n"
        "enable-gdb-stub: false\nlog-svc: true\n"
    )
    executable = ROOT / "build/eka2l1/bin/EKA2L1.app/Contents/MacOS/EKA2L1"
    with tempfile.TemporaryDirectory(
        prefix="agent-window-", dir="/tmp"
    ) as name:
        endpoint = Path(name) / "control.sock"
        control = Control(endpoint)
        env = dict(os.environ)
        env.update(
            EKA2L1_DATA_ROOT=str(instance),
            EKA2L1_EXPERIMENTAL_SVC_PROFILE="rm807-113.010.1508",
            EKA2L1_RESEARCH_CONTROL_SOCKET=str(endpoint),
            **background_environment(),
        )
        with (tmp_path / "frontend.log").open("w") as log:
            process = subprocess.Popen(
                [
                    executable_for_session(executable, Path(name)),
                    "--device",
                    "RM-807",
                    "--run",
                    "C:\\sys\\bin\\agent_service.exe",
                ],
                env=env,
                stdout=log,
                stderr=subprocess.STDOUT,
            )
            try:
                deadline = time.monotonic() + 25
                while True:
                    try:
                        with _connect() as agent:
                            assert agent.status().state == "ready"
                        break
                    except StatusError as error:
                        if (
                            error.code != Code.UNAVAILABLE
                            or time.monotonic() >= deadline
                        ):
                            raise
                        time.sleep(0.05)
                seen = False
                for attempt in range(20):
                    try:
                        captured = control.capture(f"agent-{attempt}")
                    except StatusError:
                        time.sleep(0.1)
                        continue
                    path = Path(captured["path"])
                    shutil.copyfile(path, tmp_path / "agent-screen.png")
                    with Image.open(path) as image:
                        pixel = image.convert("RGB").getpixel((500, 300))
                    if pixel[1] > pixel[0] + 20 and pixel[1] > pixel[2] + 50:
                        seen = True
                        break
                    time.sleep(0.1)
                assert seen, "The agent status panel did not render"
                if action == "stop":
                    control.pointer(180, 520, "press")
                    process.wait(timeout=15)
                    assert process.returncode == 0
                    exits = control.exit_report()["process_exits"]
                    assert any(
                        item["uid"] == 0xE0000A31 and item["reason"] == 0
                        for item in exits
                    )
                else:
                    control.pointer(180, 420, "press")
                    time.sleep(0.3)
                    with _connect() as agent:
                        assert agent.status().state == "ready"
                    assert process.poll() is None
                    for attempt in range(20):
                        try:
                            captured = control.capture("agent-background")
                            break
                        except StatusError as error:
                            if error.code != Code.UNAVAILABLE or attempt == 19:
                                raise
                            time.sleep(0.1)
                    shutil.copyfile(
                        captured["path"], tmp_path / "agent-background.png"
                    )
            finally:
                if process.poll() is None:
                    process.terminate()
                try:
                    process.wait(timeout=2)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=5)


@pytest.mark.parametrize("backend", ["dynarmic", "dyncom"])
def test_resident_agent_status_and_recovery(service_image, tmp_path, backend):
    """Status survives another request, a bad frame, and a new connection."""
    golden = ROOT / ".symbian/instances/delight-import-01"
    pinned = {
        golden / "data/roms/rm-807/SYM.ROM": ROM_808,
        golden / "data/drives/z/rm-807/sys/bin/euser.dll": EUSER_808,
        golden / "data/drives/z/rm-807/sys/bin/esock.dll": ESOCK_808,
        golden / "data/drives/z/rm-807/sys/bin/insock.dll": INSOCK_808,
    }
    assert {path: _digest(path) for path in pinned} == pinned
    instance = tmp_path / "instance"
    shutil.copytree(golden, instance)
    guest_bin = instance / "data/drives/rm-807/c/sys/bin"
    guest_bin.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(service_image, guest_bin / "agent_service.exe")
    (instance / "config.yml").write_text(
        f"data-storage: data\ncpu: {backend}\ndevice: 0\nlanguage: 1\n"
        "enable-gdb-stub: false\nlog-svc: true\n"
    )
    executable = ROOT / "build/eka2l1/bin/EKA2L1.app/Contents/MacOS/EKA2L1"
    with tempfile.TemporaryDirectory(
        prefix="agent-service-", dir="/tmp"
    ) as name:
        env = dict(os.environ)
        env.update(
            EKA2L1_DATA_ROOT=str(instance),
            EKA2L1_EXPERIMENTAL_SVC_PROFILE="rm807-113.010.1508",
            **background_environment(),
        )
        with (tmp_path / "frontend.log").open("w") as log:
            process = subprocess.Popen(
                [
                    executable_for_session(executable, Path(name)),
                    "--device",
                    "RM-807",
                    "--run",
                    "C:\\sys\\bin\\agent_service.exe",
                ],
                env=env,
                stdout=log,
                stderr=subprocess.STDOUT,
            )
            try:
                deadline = time.monotonic() + 25
                while True:
                    try:
                        with _connect() as agent:
                            first = agent.status()
                            second = agent.status()
                            assert (
                                first.service
                                == second.service
                                == "symbian-agent"
                            )
                            assert first.state == second.state == "ready"
                            assert first.capabilities == ("status", "logs")
                            assert first.system is not None
                            assert first.system.tick_period_us > 0
                            assert first.display is not None
                            assert first.display.width_pixels > 0
                            assert first.display.height_pixels > 0
                            assert (first.request_id, second.request_id) == (
                                2,
                                3,
                            )
                            page = agent.logs(limit=8)
                            assert [record.code for record in page.records] == [
                                1,
                                2,
                                2,
                            ]
                            assert [
                                record.severity for record in page.records
                            ] == [
                                2,
                                1,
                                1,
                            ]
                            assert all(
                                record.elapsed_us is not None
                                for record in page.records
                            )
                            assert [
                                record.elapsed_us for record in page.records
                            ] == sorted(
                                record.elapsed_us for record in page.records
                            )
                            cursor = page.next_cursor
                            recent = agent.recent_logs()
                            assert [
                                record.sequence for record in recent.records
                            ] == [record.sequence for record in page.records]
                            assert recent.next_cursor == cursor
                        break
                    except StatusError as error:
                        if (
                            error.code != Code.UNAVAILABLE
                            or time.monotonic() >= deadline
                        ):
                            raise
                        time.sleep(0.05)

                wrong_key = tmp_path / "wrong-agent.key"
                wrong_key.write_bytes(bytes(32))
                with pytest.raises(StatusError):
                    ReadOnlyAgentSession.connect(
                        "127.0.0.1", 39101, key_file=wrong_key
                    )
                with _connect() as agent:
                    agent._stream.sendall((4097).to_bytes(4, "big"))
                with _connect() as agent:
                    agent._stream.sendall(_native.pack_agent_read_request(1, 1))
                    try:
                        assert agent._stream.recv(1) == b""
                    except ConnectionResetError:
                        pass
                with _connect() as agent:
                    assert agent.status().state == "ready"
                    page = agent.logs(after=cursor)
                    assert any(record.code == 3 for record in page.records)
                    assert all(
                        record.severity == 3
                        for record in page.records
                        if record.code == 3
                    )
                    assert page.next_cursor > cursor
                command = subprocess.run(
                    [
                        sys.executable,
                        "-m",
                        "symbian.cli",
                        "agent",
                        "status",
                        "127.0.0.1",
                        "39101",
                        "--key-file",
                        str(
                            os.environ.get(
                                "SYMBIAN_AGENT_PRIVATE_GUEST_KEY_FILE", TEST_KEY
                            )
                        ),
                        "--output-format=json",
                    ],
                    cwd=ROOT,
                    capture_output=True,
                    text=True,
                    timeout=15,
                )
                assert command.returncode == 0, command.stdout + command.stderr
                assert json.loads(command.stdout)["result"]["state"] == "ready"
                hello_args = command.args.copy()
                hello_args[4] = "hello"
                hello_command = subprocess.run(
                    hello_args,
                    cwd=ROOT,
                    capture_output=True,
                    text=True,
                    timeout=15,
                )
                assert hello_command.returncode == 0, (
                    hello_command.stdout + hello_command.stderr
                )
                assert (
                    json.loads(hello_command.stdout)["result"][
                        "maximum_control_bytes"
                    ]
                    == 4096
                )
                log_args = command.args.copy()
                log_args[4] = "logs"
                log_args.extend(["--after", str(cursor)])
                log_command = subprocess.run(
                    log_args,
                    cwd=ROOT,
                    capture_output=True,
                    text=True,
                    timeout=15,
                )
                assert log_command.returncode == 0, (
                    log_command.stdout + log_command.stderr
                )
                assert json.loads(log_command.stdout)["result"]["records"]
                with _connect() as agent:
                    started = time.monotonic()
                    for _ in range(3):
                        agent._stream.sendall(b"\x00")
                        time.sleep(2)
                    try:
                        assert agent._stream.recv(1) == b""
                    except ConnectionResetError:
                        pass
                    assert time.monotonic() - started < 7.5
                with _connect() as agent:
                    assert agent.status().state == "ready"
                if backend == "dynarmic":
                    for _ in range(2):
                        with _connect() as agent:
                            for _request in range(15):
                                assert agent.status().state == "ready"
                            with pytest.raises(StatusError) as error:
                                agent.status()
                            assert error.value.code == Code.RESOURCE_EXHAUSTED
                    with _connect() as agent:
                        wrapped = agent.logs(after=0, limit=8)
                        assert wrapped.gap
                        assert wrapped.records[0].sequence > 1
                assert process.poll() is None
            finally:
                _stop(process)
    assert {path: _digest(path) for path in pinned} == pinned
