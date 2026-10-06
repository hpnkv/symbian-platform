"""SDK secure RNG acceptance against explicitly selected preserved firmware."""

import json
import os
import shutil
import subprocess
import tempfile
from pathlib import Path

import pytest

from symbian import toolchain
from symbian.emulator import Control
from symbian.emulator.background import (
    background_environment,
    executable_for_session,
)
from symbian.emulator.launch import _digest, _stop
from symbian.project.sdk import AppSdk

ROOT = Path(__file__).parents[2]
FIXTURES = json.loads(os.environ.get("SYMBIAN_ENTROPY_FIXTURES", "[]"))
pytestmark = pytest.mark.skipif(
    not FIXTURES, reason="Set SYMBIAN_ENTROPY_FIXTURES to preserved OS controls"
)


@pytest.fixture(scope="module", params=["armv5t", "armv6"])
def entropy_binaries(request, tmp_path_factory):
    """Build the installed SDK provider's success and absence controls."""
    sdk = AppSdk.load(Path(os.environ["SYMBIAN_APP_SDK"]))
    binaries = {}
    for secure in (False, True):
        output = tmp_path_factory.mktemp(f"entropy-{request.param}-{secure}")
        report = toolchain.build(
            ROOT / "probes/entropy_probe",
            output,
            str(sdk.compiler),
            str(sdk.linker),
            architecture=request.param,
            cmake_variables={
                "SYMBIAN_SDK_PREFIX": str(sdk.prefix),
                "SYMBIAN_EXPECT_SECURE_ENTROPY": "ON" if secure else "OFF",
            },
        )
        binaries[secure] = Path(report["artifact"])
    return binaries


@pytest.mark.parametrize("fixture", FIXTURES)
@pytest.mark.parametrize("backend", ["dynarmic", "dyncom"])
def test_os_secure_entropy(entropy_binaries, fixture, backend, tmp_path):
    """Check native success/zeroized failure without changing pinned inputs."""
    golden = Path(fixture["instance"])
    device = fixture["device"]
    pinned = {
        path: _digest(path)
        for path in (
            golden / f"data/roms/{device.lower()}/SYM.ROM",
            golden / f"data/drives/z/{device.lower()}/sys/bin/euser.dll",
        )
    }
    instance = tmp_path / "instance"
    shutil.copytree(golden, instance)
    guest_bin = instance / f"data/drives/{device.lower()}/c/sys/bin"
    guest_bin.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(
        entropy_binaries[fixture["secure"]], guest_bin / "entropy_probe.exe"
    )
    (instance / "config.yml").write_text(
        f"data-storage: data\ncpu: {backend}\ndevice: 0\nlanguage: 1\n"
        "enable-gdb-stub: false\nlog-svc: true\n"
    )
    sdk = AppSdk.load(Path(os.environ["SYMBIAN_APP_SDK"]))
    executable = Path(os.environ.get("SYMBIAN_TEST_EMULATOR", sdk.emulator))
    with tempfile.TemporaryDirectory(prefix="sdk-entropy-", dir="/tmp") as work:
        work = Path(work)
        control = Control(work / "control.sock")
        env = dict(os.environ)
        env.pop("EKA2L1_EXPERIMENTAL_SVC_PROFILE", None)
        if fixture.get("profile"):
            env["EKA2L1_EXPERIMENTAL_SVC_PROFILE"] = fixture["profile"]
        env.update(
            EKA2L1_DATA_ROOT=str(instance),
            EKA2L1_RESEARCH_CONTROL_SOCKET=str(control.endpoint),
            **background_environment(),
        )
        with (tmp_path / "frontend.log").open("w") as log:
            process = subprocess.Popen(
                [
                    executable_for_session(executable, work),
                    "--device",
                    device,
                    "--run",
                    "C:\\sys\\bin\\entropy_probe.exe",
                ],
                env=env,
                stdout=log,
                stderr=subprocess.STDOUT,
            )
            try:
                assert process.wait(timeout=30) == 0
                exits = control.exit_report()["process_exits"]
                (tmp_path / "exits.json").write_text(json.dumps(exits))
                assert len(exits) == 1
                assert exits[0]["uid"] == 0xE0000835
                assert exits[0]["reason"] == 0
                assert exits[0]["type"] == 0
            finally:
                _stop(process)
                assert {path: _digest(path) for path in pinned} == pinned
