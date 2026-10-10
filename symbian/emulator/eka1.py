"""Owned execution of the bounded, named-fixture EKA1 process profile."""

import os
import shutil
import subprocess
import tempfile
from pathlib import Path

from symbian.e32 import inspect_image
from symbian.emulator import Control
from symbian.emulator.background import (
    background_environment,
    executable_for_session,
)
from symbian.emulator.configuration import Resolution, atomic_json
from symbian.emulator.firmware import digest, selected, validate_manifest
from symbian.emulator.launch import _stop
from symbian.status import Code, StatusError

# An execution gate, not a claim for every EKA1 device/firmware revision.
NOKIA_7610 = "80c85c43e74cd6f6bc2a071e32d2efdebe1fa0a3c5352c08217327b6415bfbe3"


def run_probe(
    image: Path,
    output: Path,
    resolution: Resolution,
    *,
    expected_reason: int = 7610,
    timeout: float = 30,
) -> dict:
    """Runs an EKA1 process in fresh state and checks its native exit record.

    Args:
        image: Native-converter EKA1 read-only executable.
        output: New evidence directory; never an imported firmware directory.
        resolution: Explicit firmware, frontend and CPU-backend selection.
        expected_reason: Result returned by the process; a mismatch fails.
        timeout: Bounded frontend execution time in seconds.

    Returns:
        Firmware/image identities, native exit and preservation evidence.
    """
    image, output = image.resolve(), output.resolve()
    golden, firmware = selected(resolution)
    if firmware.identity != NOKIA_7610 or firmware.device.kernel != "eka1":
        raise StatusError(
            Code.FAILED_PRECONDITION,
            "EKA1 process execution is tested only on the preserved Nokia "
            "7610 RH-51 fixture; see archived ~/.symbian-dev/EKA1.md",
        )
    info = inspect_image(image)
    if info["kernel"] != "eka1":
        raise StatusError(
            Code.FAILED_PRECONDITION, "EKA1 firmware requires an EKA1 image"
        )
    backend = resolution.settings.backend or "dynarmic"
    if backend not in ("dynarmic", "dyncom"):
        raise StatusError(Code.INVALID_ARGUMENT, "Unsupported CPU backend")
    if (
        not 0 < timeout <= 60
        or type(expected_reason) is not int
        or not -(1 << 31) <= expected_reason < (1 << 31)
    ):
        raise StatusError(Code.INVALID_ARGUMENT, "Invalid EKA1 probe limits")
    executable = resolution.settings.emulator
    if executable is None or not executable.is_file():
        raise StatusError(Code.NOT_FOUND, "Emulator frontend is unavailable")
    if resolution.settings.profile not in (None, "auto", "default"):
        raise StatusError(
            Code.INVALID_ARGUMENT, "EKA1 uses its default executive map"
        )
    if output.is_relative_to(golden.parent):
        raise StatusError(
            Code.INVALID_ARGUMENT,
            "Evidence must stay outside preserved firmware",
        )
    if output.exists():
        raise StatusError(Code.ALREADY_EXISTS, str(output))
    output.mkdir(parents=True)
    instance = output / "instance"
    report = {
        "schema": "symbian.eka1-process/v1",
        "firmware": firmware.identity,
        "device": firmware.device.model_dump(),
        "backend": backend,
        "image_sha256": digest(image),
        "e32": info,
        "expected_reason": expected_reason,
        "emulator_sha256": digest(executable),
        "physical_device_verified": False,
        "golden_preserved": False,
        "accepted": False,
    }
    process = None
    try:
        shutil.copytree(golden, instance)
        drive = instance / firmware.device.c_drive
        guest = drive / "system/programs/eka1_probe.exe"
        guest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(image, guest)
        (instance / "config.yml").write_text(
            f"data-storage: data\ncpu: {backend}\ndevice: 0\n"
            f"language: {resolution.settings.language or 1}\n"
            "enable-gdb-stub: false\nlog-svc: false\n"
        )
        with tempfile.TemporaryDirectory(prefix="eka1-probe-") as temporary:
            session = Path(temporary)
            control = Control(session / "control.sock")
            env = dict(os.environ)
            env.pop("EKA2L1_EXPERIMENTAL_SVC_PROFILE", None)
            env.update(
                EKA2L1_DATA_ROOT=str(instance),
                EKA2L1_RESEARCH_CONTROL_SOCKET=str(control.endpoint),
                **background_environment(),
            )
            command = [
                str(executable_for_session(executable, session)),
                "--device",
                firmware.device.firmware_code,
                "--run",
                "C:\\system\\programs\\eka1_probe.exe",
            ]
            report["command"] = command
            with (output / "frontend.log").open("w") as log:
                process = subprocess.Popen(
                    command,
                    cwd=instance,
                    env=env,
                    stdout=log,
                    stderr=subprocess.STDOUT,
                )
                try:
                    report["frontend_exit"] = process.wait(timeout=timeout)
                except subprocess.TimeoutExpired as error:
                    raise StatusError(
                        Code.DEADLINE_EXCEEDED, "EKA1 probe timed out"
                    ) from error
                finally:
                    _stop(process)
            exits = control.exit_report()["process_exits"]
            report["process_exits"] = exits
            matches = [
                record for record in exits if record["uid"] == info["uid3"]
            ]
            report["accepted"] = (
                report["frontend_exit"] == 0
                and len(matches) == 1
                and matches[0]["type"] == 0
                and matches[0]["reason"] == expected_reason
            )
    finally:
        _stop(process)
        # Emulator startup may change disposable Z. Never compare that copy
        # with the imported manifest as if startup were a read-only operation.
        validate_manifest(golden.parent)
        report["golden_preserved"] = True
        atomic_json(output / "report.json", report)
    if not report["accepted"]:
        raise StatusError(
            Code.FAILED_PRECONDITION,
            f"EKA1 process exit disagrees with {expected_reason}; "
            f"see {output / 'report.json'}",
        )
    return report
