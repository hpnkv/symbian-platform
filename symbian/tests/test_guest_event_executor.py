"""Execute one timer, property, mailbox and fiber owner on the guest."""

import os
import shutil
import subprocess
import tempfile
from pathlib import Path

import pytest

from symbian.e32.images import convert_imported_executable
from symbian.emulator import Control
from symbian.emulator.background import (
    background_environment,
    executable_for_session,
)
from symbian.emulator.firmware import EUSER_808, ROM_808
from symbian.emulator.launch import _digest, _stop
from symbian.project.sdk import AppSdk

WORKSPACE = os.environ.get("SYMBIAN_RUNTIME_WORKSPACE")
SDK_MANIFEST = os.environ.get("SYMBIAN_APP_SDK")
pytestmark = pytest.mark.skipif(
    not WORKSPACE or not SDK_MANIFEST,
    reason="Set SYMBIAN_RUNTIME_WORKSPACE and SYMBIAN_APP_SDK",
)


@pytest.fixture(scope="module")
def images(tmp_path_factory):
    """Build both ISAs and the deliberately changed result control."""
    root = Path(WORKSPACE).resolve()
    sdk = AppSdk.load(Path(SDK_MANIFEST)).prefix
    output = tmp_path_factory.mktemp("event-executor-builds")
    proxies = [
        (sdk / "proxies" / name / f"{name}.dso").read_bytes()
        for name in ("euser", "libc", "libpthread", "libm", "drtaeabi")
    ]
    result = {}
    for architecture in ("armv5t", "armv6"):
        for changed in (False, True):
            build = (
                output / f"{architecture}-{'changed' if changed else 'normal'}"
            )
            subprocess.run(
                [
                    "cmake",
                    "-S",
                    str(root / "examples/runtime_probe"),
                    "-B",
                    str(build),
                    "-G",
                    "Ninja",
                    f"-DCMAKE_TOOLCHAIN_FILE={sdk / 'cmake/symbian-arm.cmake'}",
                    f"-DSYMBIAN_SDK_PREFIX={sdk}",
                    f"-DSYMBIAN_TARGET_ARCH={architecture}",
                    "-DSYMBIAN_IMPORT_PROXIES="
                    f"{sdk / 'proxies/euser/euser.dso'}",
                    "-DSYMBIAN_RUNTIME_EVENT_EXECUTOR=ON",
                    "-DSYMBIAN_RUNTIME_CHANGED_EVENT_EXECUTOR="
                    + ("ON" if changed else "OFF"),
                ],
                check=True,
                capture_output=True,
                text=True,
            )
            subprocess.run(
                ["cmake", "--build", str(build), "--target", "runtime_probe"],
                check=True,
                capture_output=True,
                text=True,
            )
            result[architecture, changed] = convert_imported_executable(
                (build / "runtime_probe.elf").read_bytes(), proxies, 0xE0000813
            )
    return result


@pytest.mark.parametrize("architecture", ["armv5t", "armv6"])
@pytest.mark.parametrize("backend", ["dyncom", "dynarmic"])
@pytest.mark.parametrize("changed", [False, True])
def test_event_executor_guest(images, architecture, backend, changed):
    """Require real property values, timer wake and fiber Await on each CPU."""
    root = Path(WORKSPACE).resolve()
    golden = root / ".symbian/instances/delight-import-01"
    pinned = {
        golden / "data/roms/rm-807/SYM.ROM": ROM_808,
        golden / "data/drives/z/rm-807/sys/bin/euser.dll": EUSER_808,
    }
    assert {path: _digest(path) for path in pinned} == pinned
    with tempfile.TemporaryDirectory(
        prefix="event-executor-", dir="/tmp"
    ) as name:
        session = Path(name)
        instance = session / "instance"
        shutil.copytree(golden, instance)
        target = instance / "data/drives/rm-807/c/sys/bin/runtime_probe.exe"
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(images[architecture, changed])
        (instance / "config.yml").write_text(
            f"data-storage: data\ncpu: {backend}\ndevice: 0\n"
            "language: 1\nenable-gdb-stub: false\nlog-svc: true\n"
        )
        control = Control(session / "control.sock")
        executable = root / "build/eka2l1/bin/EKA2L1.app/Contents/MacOS/EKA2L1"
        environment = dict(os.environ)
        environment.update(
            EKA2L1_DATA_ROOT=str(instance),
            EKA2L1_EXPERIMENTAL_SVC_PROFILE="rm807-113.010.1508",
            EKA2L1_RESEARCH_CONTROL_SOCKET=str(control.endpoint),
            **background_environment(),
        )
        with (session / "frontend.log").open("w") as log:
            process = subprocess.Popen(
                [
                    executable_for_session(executable, session),
                    "--device",
                    "RM-807",
                    "--run",
                    "C:\\sys\\bin\\runtime_probe.exe",
                ],
                env=environment,
                stdout=log,
                stderr=subprocess.STDOUT,
            )
            try:
                assert process.wait(timeout=30) == 0
                exits = control.exit_report()["process_exits"]
                assert len(exits) == 1
                assert exits[0]["reason"] == (-344 if changed else 0)
                assert exits[0]["type"] == 0
            finally:
                _stop(process)
    assert {path: _digest(path) for path in pinned} == pinned
