"""Opt-in resident, read-only agent experiment in the pinned emulator."""

import json
import os
import shutil
import socket
import ssl
import subprocess
import sys
import tempfile
import time
from pathlib import Path

import pytest

from symbian import toolchain
from symbian.agent import ReadOnlyAgentSession
from symbian.emulator.background import (
    background_environment,
    executable_for_session,
)
from symbian.emulator.firmware import EUSER_808, ROM_808
from symbian.emulator.launch import _digest, _stop
from symbian.project.sdk import AppSdk

ROOT = Path(__file__).parents[2]
CERTIFICATES = ROOT / "third_party/mbedtls-symbian/tests/fixtures"
ESOCK_808 = "555a8c00ced78b7350f9f08f510c8e742cdc328f1f2780420c813b1fe2eb4a08"
INSOCK_808 = "d619f48a95b16aa89b3f34b813c3182d9440f80a6f23cead49b9f3aa988e9110"

pytestmark = pytest.mark.skipif(
    not os.environ.get("SYMBIAN_AGENT_SERVICE_GUEST"),
    reason="Set SYMBIAN_AGENT_SERVICE_GUEST for the pinned emulator experiment",
)


@pytest.fixture(scope="module")
def service_image(tmp_path_factory):
    """Builds the research service against a clean selected SDK."""
    sdk = AppSdk.load(Path(os.environ["SYMBIAN_SDK_MANIFEST"]))
    output = tmp_path_factory.mktemp("agent-service")
    project = output / "project"
    shutil.copytree(ROOT / "examples/agent_service", project)
    presets = project / "CMakePresets.json"
    data = json.loads(presets.read_text())
    cache = data["configurePresets"][0]["cacheVariables"]
    cache["SYMBIAN_SDK_PREFIX"] = str(sdk.prefix)
    cache["SYMBIAN_RESEARCH_CERTIFICATE_DIR"] = str(CERTIFICATES)
    data["configurePresets"][0]["toolchainFile"] = str(
        sdk.prefix / "cmake/symbian-arm.cmake"
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


def _connect(timeout=10.0):
    return ReadOnlyAgentSession.connect(
        "127.0.0.1",
        39101,
        server_name="sdk-test",
        ca_bundle=CERTIFICATES / "server-cert.pem",
        client_certificate=CERTIFICATES / "server-cert.pem",
        client_key=CERTIFICATES / "server-key.pem",
        timeout=timeout,
    )


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
                            assert first.capabilities == ("status",)
                            assert (first.request_id, second.request_id) == (
                                1,
                                2,
                            )
                        break
                    except ConnectionRefusedError:
                        if time.monotonic() >= deadline:
                            raise
                        time.sleep(0.05)

                context = ssl.create_default_context(
                    cafile=str(CERTIFICATES / "server-cert.pem")
                )
                context.load_cert_chain(
                    CERTIFICATES / "server-cert.pem",
                    CERTIFICATES / "server-key.pem",
                )
                with socket.create_connection(("127.0.0.1", 39101), 10) as raw:
                    with context.wrap_socket(
                        raw, server_hostname="sdk-test"
                    ) as tls:
                        tls.sendall((4097).to_bytes(4, "big"))
                with _connect() as agent:
                    assert agent.status().state == "ready"
                command = subprocess.run(
                    [
                        sys.executable,
                        "-m",
                        "symbian.cli",
                        "agent",
                        "status",
                        "127.0.0.1",
                        "39101",
                        "--server-name",
                        "sdk-test",
                        "--ca-bundle",
                        str(CERTIFICATES / "server-cert.pem"),
                        "--client-certificate",
                        str(CERTIFICATES / "server-cert.pem"),
                        "--client-key",
                        str(CERTIFICATES / "server-key.pem"),
                        "--output-format=json",
                    ],
                    cwd=ROOT,
                    capture_output=True,
                    text=True,
                    timeout=15,
                )
                assert command.returncode == 0, command.stdout + command.stderr
                assert json.loads(command.stdout)["result"]["state"] == "ready"
                assert process.poll() is None
            finally:
                _stop(process)
    assert {path: _digest(path) for path in pinned} == pinned
