"""Run one E32 image against an isolated named EKA2L1 firmware fixture.

This is an emulator evidence tool. It never changes the imported baseline and
does not infer physical-device compatibility from a successful guest run.
"""

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import tempfile
import time
from pathlib import Path

from symbian.e32.images import inspect_image
from symbian.emulator import Control
from symbian.emulator.background import (
    background_environment,
    executable_for_session,
)
from symbian.emulator.firmware import locate, svc_profile, validate_manifest
from symbian.paths import asset_directory
from symbian.status import StatusException


def _missing_import_libraries(
    instance: Path, z_drive: str, image: dict
) -> list[str]:
    """Find direct DLL names absent from the selected firmware Z drive."""
    system_bin = instance / z_drive / "sys/bin"
    available = {path.name.lower() for path in system_bin.glob("*.dll")}
    return sorted(
        {block["dll"].lower() for block in image["imports"]} - available
    )


def _close_task(control: Control, uid: int) -> None:
    """Retry while the frontend is still servicing guest kernel startup."""
    deadline = time.monotonic() + 8
    while True:
        try:
            control.close_task(uid)
            return
        except StatusException:
            if time.monotonic() >= deadline:
                raise
            time.sleep(0.25)


def main() -> None:
    """Copy a baseline, run one guest, and print evidence as JSON."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--firmware", required=True, help="Imported fixture alias"
    )
    parser.add_argument(
        "--store", type=Path, default=asset_directory("data") / "firmware"
    )
    parser.add_argument("--image", required=True, type=Path)
    parser.add_argument("--emulator", required=True, type=Path)
    parser.add_argument(
        "--backend", choices=("dyncom", "dynarmic"), default="dynarmic"
    )
    parser.add_argument("--interactive-uid", type=lambda value: int(value, 0))
    parser.add_argument("--seconds", type=float, default=5.0)
    args = parser.parse_args()

    source = locate(args.store, args.firmware)
    manifest = validate_manifest(source)
    if manifest.device.kernel != "eka2":
        parser.error("This runner requires an EKA2 firmware fixture")
    image = inspect_image(args.image)
    report = {
        "firmware": args.firmware,
        "firmware_identity": manifest.identity,
        "model": manifest.device.firmware_code,
        "svc_profile": svc_profile(manifest, None),
        "backend": args.backend,
        "image_sha256": hashlib.sha256(args.image.read_bytes()).hexdigest(),
        "e32_conversion_accepted": True,
        "direct_imports": {
            block["dll"]: sorted({slot["ordinal"] for slot in block["slots"]})
            for block in image["imports"]
        },
        "physical_device_tested": False,
        "loader_accepted": False,
        "guest_executed": False,
    }
    with tempfile.TemporaryDirectory(prefix="symbian-firmware-matrix-") as temp:
        work = Path(temp)
        instance = work / "instance"
        shutil.copytree(source / "instance", instance)
        missing = _missing_import_libraries(
            instance, manifest.device.z_drive, image
        )
        report["missing_direct_dlls"] = missing
        if missing:
            print(json.dumps(report, sort_keys=True, indent=2))
            raise SystemExit(2)
        guest_name = args.image.name
        guest_path = instance / manifest.device.c_drive / "sys/bin" / guest_name
        guest_path.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(args.image, guest_path)
        (instance / "config.yml").write_text(
            "data-storage: data\n"
            f"cpu: {args.backend}\ndevice: 0\nlanguage: 1\n"
            "enable-gdb-stub: false\nlog-svc: true\n"
        )
        endpoint = work / "control.sock"
        control = Control(endpoint)
        env = dict(os.environ)
        env.pop("EKA2L1_EXPERIMENTAL_SVC_PROFILE", None)
        env.update(
            EKA2L1_DATA_ROOT=str(instance),
            EKA2L1_RESEARCH_CONTROL_SOCKET=str(endpoint),
            **background_environment(),
        )
        if report["svc_profile"] != "default":
            env["EKA2L1_EXPERIMENTAL_SVC_PROFILE"] = report["svc_profile"]
        with (work / "frontend.log").open("w") as log:
            process = subprocess.Popen(
                [
                    executable_for_session(args.emulator, work),
                    "--device",
                    manifest.device.firmware_code,
                    "--run",
                    f"C:\\sys\\bin\\{guest_name}",
                ],
                env=env,
                stdout=log,
                stderr=subprocess.STDOUT,
            )
            try:
                if args.interactive_uid is None:
                    report["frontend_exit"] = process.wait(timeout=30)
                else:
                    deadline = time.monotonic() + args.seconds
                    while time.monotonic() < deadline:
                        if process.poll() is not None:
                            break
                        try:
                            frame = control.capture("firmware-matrix")
                            report["frame_sha256"] = hashlib.sha256(
                                Path(frame["path"]).read_bytes()
                            ).hexdigest()
                            report["guest_executed"] = True
                            break
                        except (
                            OSError,
                            RuntimeError,
                            KeyError,
                            StatusException,
                        ):
                            time.sleep(0.25)
                    if report["guest_executed"]:
                        try:
                            _close_task(control, args.interactive_uid)
                        except StatusException as error:
                            report["close_task_error"] = str(error)
                            process.kill()
                    report["frontend_exit"] = process.wait(timeout=15)
                try:
                    exits = control.exit_report()["process_exits"]
                except (OSError, RuntimeError, KeyError, StatusException):
                    exits = []
                report["process_exits"] = exits
                report["loader_accepted"] = (
                    bool(exits) or report["guest_executed"]
                )
                if args.interactive_uid is None:
                    report["guest_executed"] = any(
                        item["reason"] == 0 and item["type"] == 0
                        for item in exits
                    )
                report["normal_guest_exit"] = any(
                    item["reason"] == 0 and item["type"] == 0 for item in exits
                )
            finally:
                if process.poll() is None:
                    process.kill()
                    process.wait(timeout=10)
        report["frontend_log_sha256"] = hashlib.sha256(
            (work / "frontend.log").read_bytes()
        ).hexdigest()
    print(json.dumps(report, sort_keys=True, indent=2))
    if not report["guest_executed"] or not report["normal_guest_exit"]:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
