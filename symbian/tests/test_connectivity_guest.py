"""Opt-in execution of the SDK's native TCP helper in a pinned emulator."""

import json
import os
import shutil
import socket
import subprocess
import tempfile
import threading
import time
from pathlib import Path

import pytest

from symbian import toolchain
from symbian.emulator import Control
from symbian.emulator.background import (
    background_environment,
    executable_for_session,
)
from symbian.emulator.firmware import EUSER_808, ROM_808
from symbian.emulator.launch import _digest, _stop
from symbian.project.sdk import AppSdk

ROOT = Path(__file__).parents[2]
ESOCK_808 = "555a8c00ced78b7350f9f08f510c8e742cdc328f1f2780420c813b1fe2eb4a08"
INSOCK_808 = "d619f48a95b16aa89b3f34b813c3182d9440f80a6f23cead49b9f3aa988e9110"

pytestmark = pytest.mark.skipif(
    not os.environ.get("SYMBIAN_CONNECTIVITY_GUEST"),
    reason="Set SYMBIAN_CONNECTIVITY_GUEST for the pinned emulator experiment",
)


@pytest.fixture(scope="module")
def guest_image(tmp_path_factory):
    """Builds a normal SDK consumer with the public connectivity target."""
    sdk = AppSdk.load(Path(os.environ["SYMBIAN_SDK_MANIFEST"]))
    assert (sdk.prefix / "proxies/esock/esock.dso").is_file()
    assert (sdk.prefix / "proxies/insock/insock.dso").is_file()
    output = tmp_path_factory.mktemp("connectivity-guest")
    project = output / "project"
    shutil.copytree(ROOT / "probes/connectivity_probe", project)
    presets = project / "CMakePresets.json"
    data = json.loads(presets.read_text())
    data["configurePresets"][0]["toolchainFile"] = str(
        sdk.prefix / "cmake/symbian-arm.cmake"
    )
    data["configurePresets"][0]["cacheVariables"]["SYMBIAN_SDK_PREFIX"] = str(
        sdk.prefix
    )
    presets.write_text(json.dumps(data))
    report = toolchain.build(
        project,
        output / "build",
        str(sdk.compiler),
        str(sdk.linker),
        architecture="armv6",
    )
    return Path(report["artifact"])


@pytest.fixture(scope="module")
def listener_image(tmp_path_factory):
    """Builds the native listener probe as an ordinary SDK application."""
    sdk = AppSdk.load(Path(os.environ["SYMBIAN_SDK_MANIFEST"]))
    output = tmp_path_factory.mktemp("connectivity-listener")
    project = output / "project"
    shutil.copytree(ROOT / "probes/connectivity_probe", project)
    cmake_file = project / "CMakeLists.txt"
    cmake_file.write_text(
        cmake_file.read_text().replace(
            "connectivity_probe probe.cc",
            "connectivity_probe listener_probe.cc",
        )
    )
    presets = project / "CMakePresets.json"
    data = json.loads(presets.read_text())
    data["configurePresets"][0]["toolchainFile"] = str(
        sdk.prefix / "cmake/symbian-arm.cmake"
    )
    data["configurePresets"][0]["cacheVariables"]["SYMBIAN_SDK_PREFIX"] = str(
        sdk.prefix
    )
    presets.write_text(json.dumps(data))
    report = toolchain.build(
        project,
        output / "build",
        str(sdk.compiler),
        str(sdk.linker),
        architecture="armv6",
    )
    return Path(report["artifact"])


@pytest.fixture(scope="module")
def active_listener_image(tmp_path_factory):
    """Builds the public active listener with the original scheduler bridge."""
    sdk = AppSdk.load(Path(os.environ["SYMBIAN_SDK_MANIFEST"]))
    output = tmp_path_factory.mktemp("active-connectivity-listener")
    project = output / "project"
    shutil.copytree(ROOT / "probes/connectivity_probe", project)
    cmake_file = project / "CMakeLists.txt"
    cmake_file.write_text(
        cmake_file.read_text().replace(
            "connectivity_probe probe.cc",
            "connectivity_probe active_listener_probe.cc scheduler_bridge.cc",
        )
    )
    presets = project / "CMakePresets.json"
    data = json.loads(presets.read_text())
    data["configurePresets"][0]["toolchainFile"] = str(
        sdk.prefix / "cmake/symbian-arm.cmake"
    )
    data["configurePresets"][0]["cacheVariables"]["SYMBIAN_SDK_PREFIX"] = str(
        sdk.prefix
    )
    presets.write_text(json.dumps(data))
    report = toolchain.build(
        project,
        output / "build",
        str(sdk.compiler),
        str(sdk.linker),
        architecture="armv6",
    )
    return Path(report["artifact"])


@pytest.fixture(scope="module")
def worker_listener_image(tmp_path_factory):
    """Builds a listener that hands accepted streams to the SDK worker."""
    sdk = AppSdk.load(Path(os.environ["SYMBIAN_SDK_MANIFEST"]))
    output = tmp_path_factory.mktemp("worker-connectivity-listener")
    project = output / "project"
    shutil.copytree(ROOT / "probes/connectivity_probe", project)
    cmake_file = project / "CMakeLists.txt"
    cmake_file.write_text(
        cmake_file.read_text().replace(
            "connectivity_probe probe.cc",
            "connectivity_probe worker_listener_probe.cc scheduler_bridge.cc",
        )
        + "\ntarget_link_libraries(connectivity_probe PRIVATE "
        "Symbian::Stackless)\n"
    )
    presets = project / "CMakePresets.json"
    data = json.loads(presets.read_text())
    data["configurePresets"][0]["toolchainFile"] = str(
        sdk.prefix / "cmake/symbian-arm.cmake"
    )
    data["configurePresets"][0]["cacheVariables"]["SYMBIAN_SDK_PREFIX"] = str(
        sdk.prefix
    )
    presets.write_text(json.dumps(data))
    report = toolchain.build(
        project,
        output / "build",
        str(sdk.compiler),
        str(sdk.linker),
        architecture="armv6",
    )
    return Path(report["artifact"])


@pytest.mark.parametrize(
    "backend,reply,expected",
    [("dynarmic", b"R", 0), ("dyncom", b"R", 0), ("dynarmic", b"X", -203)],
)
def test_native_tcp_client_sends_and_receives(
    guest_image, tmp_path, backend, reply, expected
):
    """Checks real guest RSocket I/O and a wrong-response control."""
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
    shutil.copyfile(guest_image, guest_bin / "connectivity_probe.exe")
    (instance / "config.yml").write_text(
        f"data-storage: data\ncpu: {backend}\ndevice: 0\nlanguage: 1\n"
        "enable-gdb-stub: false\nlog-svc: true\n"
    )
    received = []
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as listener:
        listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        listener.bind(("127.0.0.1", 39094))
        listener.listen(1)

        def serve():
            try:
                listener.settimeout(25)
                connection, _ = listener.accept()
                with connection:
                    connection.settimeout(25)
                    received.append(connection.recv(1))
                    connection.sendall(reply)
            except OSError as error:
                received.append(error)

        server = threading.Thread(target=serve)
        server.start()
        executable = ROOT / "build/eka2l1/bin/EKA2L1.app/Contents/MacOS/EKA2L1"
        with tempfile.TemporaryDirectory(
            prefix="native-tcp-", dir="/tmp"
        ) as private:
            control = Control(Path(private) / "control.sock")
            env = dict(os.environ)
            env.update(
                EKA2L1_DATA_ROOT=str(instance),
                EKA2L1_EXPERIMENTAL_SVC_PROFILE="rm807-113.010.1508",
                EKA2L1_RESEARCH_CONTROL_SOCKET=str(control.endpoint),
                **background_environment(),
            )
            with (tmp_path / "frontend.log").open("w") as log:
                process = subprocess.Popen(
                    [
                        executable_for_session(executable, Path(private)),
                        "--device",
                        "RM-807",
                        "--run",
                        "C:\\sys\\bin\\connectivity_probe.exe",
                    ],
                    env=env,
                    stdout=log,
                    stderr=subprocess.STDOUT,
                )
                try:
                    assert process.wait(timeout=30) == 0
                    exits = control.exit_report()["process_exits"]
                    assert len(exits) == 1
                    assert exits[0]["reason"] == expected
                finally:
                    _stop(process)
        server.join(timeout=2)
    assert received == [b"N"]
    assert {path: _digest(path) for path in pinned} == pinned


def test_native_tcp_listener_accepts_host_and_retains_client(
    listener_image, tmp_path
):
    """Checks guest bind/accept and accepted stream after listener close."""
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
    shutil.copyfile(listener_image, guest_bin / "connectivity_probe.exe")
    (instance / "config.yml").write_text(
        "data-storage: data\ncpu: dynarmic\ndevice: 0\nlanguage: 1\n"
        "enable-gdb-stub: false\nlog-svc: true\n"
    )
    executable = ROOT / "build/eka2l1/bin/EKA2L1.app/Contents/MacOS/EKA2L1"
    with tempfile.TemporaryDirectory(
        prefix="native-listen-", dir="/tmp"
    ) as private:
        control = Control(Path(private) / "control.sock")
        env = dict(os.environ)
        env.update(
            EKA2L1_DATA_ROOT=str(instance),
            EKA2L1_EXPERIMENTAL_SVC_PROFILE="rm807-113.010.1508",
            EKA2L1_RESEARCH_CONTROL_SOCKET=str(control.endpoint),
            **background_environment(),
        )
        with (tmp_path / "frontend.log").open("w") as log:
            process = subprocess.Popen(
                [
                    executable_for_session(executable, Path(private)),
                    "--device",
                    "RM-807",
                    "--run",
                    "C:\\sys\\bin\\connectivity_probe.exe",
                ],
                env=env,
                stdout=log,
                stderr=subprocess.STDOUT,
            )
            try:
                deadline = time.monotonic() + 25
                while True:
                    try:
                        with socket.create_connection(
                            ("127.0.0.1", 39096), 1
                        ) as client:
                            client.settimeout(5)
                            client.sendall(b"Q")
                            assert client.recv(1) == b"A"
                            time.sleep(0.15)
                            client.sendall(b"R")
                        break
                    except ConnectionRefusedError:
                        if time.monotonic() >= deadline:
                            raise
                        time.sleep(0.05)
                assert process.wait(timeout=15) == 0
                assert control.exit_report()["process_exits"][0]["reason"] == 0
            finally:
                _stop(process)
    assert {path: _digest(path) for path in pinned} == pinned


@pytest.mark.parametrize("backend", ["dynarmic", "dyncom"])
def test_active_listener_rearms_and_cancels_idle_accept(
    active_listener_image, tmp_path, backend
):
    """Checks two active accepts, reconnect and pending-accept cleanup."""
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
    shutil.copyfile(active_listener_image, guest_bin / "connectivity_probe.exe")
    (instance / "config.yml").write_text(
        f"data-storage: data\ncpu: {backend}\ndevice: 0\nlanguage: 1\n"
        "enable-gdb-stub: false\nlog-svc: true\n"
    )
    executable = ROOT / "build/eka2l1/bin/EKA2L1.app/Contents/MacOS/EKA2L1"
    with tempfile.TemporaryDirectory(
        prefix="active-listen-", dir="/tmp"
    ) as private:
        control = Control(Path(private) / "control.sock")
        env = dict(os.environ)
        env.update(
            EKA2L1_DATA_ROOT=str(instance),
            EKA2L1_EXPERIMENTAL_SVC_PROFILE="rm807-113.010.1508",
            EKA2L1_RESEARCH_CONTROL_SOCKET=str(control.endpoint),
            **background_environment(),
        )
        with (tmp_path / "frontend.log").open("w") as log:
            process = subprocess.Popen(
                [
                    executable_for_session(executable, Path(private)),
                    "--device",
                    "RM-807",
                    "--run",
                    "C:\\sys\\bin\\connectivity_probe.exe",
                ],
                env=env,
                stdout=log,
                stderr=subprocess.STDOUT,
            )
            try:
                for _ in range(2):
                    deadline = time.monotonic() + 25
                    while True:
                        try:
                            with socket.create_connection(
                                ("127.0.0.1", 39099), 1
                            ) as client:
                                client.settimeout(5)
                                client.sendall(b"Q")
                                assert client.recv(1) == b"A"
                            break
                        except ConnectionRefusedError:
                            if time.monotonic() >= deadline:
                                raise
                            time.sleep(0.05)
                assert process.wait(timeout=15) == 0
                assert control.exit_report()["process_exits"][0]["reason"] == 0
            finally:
                _stop(process)
    assert {path: _digest(path) for path in pinned} == pinned


@pytest.mark.parametrize("backend", ["dynarmic", "dyncom"])
def test_active_listener_hands_clients_to_worker(
    worker_listener_image, tmp_path, backend
):
    """Checks shared RSocket handles and worker I/O across two connections."""
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
    shutil.copyfile(worker_listener_image, guest_bin / "connectivity_probe.exe")
    (instance / "config.yml").write_text(
        f"data-storage: data\ncpu: {backend}\ndevice: 0\nlanguage: 1\n"
        "enable-gdb-stub: false\nlog-svc: true\n"
    )
    executable = ROOT / "build/eka2l1/bin/EKA2L1.app/Contents/MacOS/EKA2L1"
    with tempfile.TemporaryDirectory(
        prefix="worker-listen-", dir="/tmp"
    ) as private:
        control = Control(Path(private) / "control.sock")
        env = dict(os.environ)
        env.update(
            EKA2L1_DATA_ROOT=str(instance),
            EKA2L1_EXPERIMENTAL_SVC_PROFILE="rm807-113.010.1508",
            EKA2L1_RESEARCH_CONTROL_SOCKET=str(control.endpoint),
            **background_environment(),
        )
        with (tmp_path / "frontend.log").open("w") as log:
            process = subprocess.Popen(
                [
                    executable_for_session(executable, Path(private)),
                    "--device",
                    "RM-807",
                    "--run",
                    "C:\\sys\\bin\\connectivity_probe.exe",
                ],
                env=env,
                stdout=log,
                stderr=subprocess.STDOUT,
            )
            try:
                for _ in range(2):
                    deadline = time.monotonic() + 25
                    while True:
                        try:
                            with socket.create_connection(
                                ("127.0.0.1", 39100), 1
                            ) as client:
                                client.settimeout(5)
                                assert client.recv(1) == b"W"
                                client.sendall(b"Q")
                                assert client.recv(1) == b"A"
                            break
                        except ConnectionRefusedError:
                            if time.monotonic() >= deadline:
                                raise
                            time.sleep(0.05)
                assert process.poll() is None
                time.sleep(0.15)
                assert process.poll() is None
            finally:
                _stop(process)
    assert {path: _digest(path) for path in pinned} == pinned
