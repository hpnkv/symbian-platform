"""Opt-in firmware-backed failure-task concurrency and window lifetime tests."""

import hashlib
import json
import os
import shutil
import subprocess
import tempfile
import time
from pathlib import Path

import pytest
from PIL import Image

from symbian.emulator import Control
from symbian.emulator.background import (
    background_environment,
    executable_for_session,
)
from symbian.emulator.launch import _stop
from symbian.status import Code, StatusError

WORKSPACE = os.environ.get("SYMBIAN_FAILURE_HANDLER_WORKSPACE")
pytestmark = pytest.mark.skipif(
    not WORKSPACE,
    reason="Set SYMBIAN_FAILURE_HANDLER_WORKSPACE to the prepared source tree",
)


def _wait_for_marker(log, marker, process, timeout=30):
    """Waits for guest evidence while detecting an early native exit."""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if marker in log.read_text(errors="replace"):
            return
        assert process.poll() is None, log.read_text(errors="replace")
        time.sleep(0.02)
    pytest.fail(f"Missing {marker}: {log.read_text(errors='replace')}")


def _control_call(method, *args):
    """Retries the frontend's nonblocking kernel-lock contention response."""
    deadline = time.monotonic() + 5
    while True:
        try:
            return method(*args)
        except StatusError as error:
            if error.code != Code.UNAVAILABLE or time.monotonic() >= deadline:
                raise
            time.sleep(0.01)


@pytest.mark.parametrize("backend", ["dynarmic", "dyncom"])
@pytest.mark.parametrize("application", ["probe", "demo"])
def test_failure_tasks_on_firmware(tmp_path, backend, application):
    """Checks overlap, post-close persistence and fatal exit safety."""
    root = Path(WORKSPACE).resolve()
    build = root / "build/guest-probes-armv6-release-gpu"
    name = (
        "failure_handler_probe"
        if application == "probe"
        else "failure_handler_app"
    )
    uid = 0xE0000A73 if application == "probe" else 0xE0000A72
    parent = "probes" if application == "probe" else "examples"
    artifact = build / parent / name / "e32" / f"{name}.exe"
    assert artifact.is_file(), f"Build target {name}_e32 first"
    golden = Path(
        os.environ.get(
            "SYMBIAN_FAILURE_HANDLER_INSTANCE",
            str(root / ".symbian/instances/delight-import-01"),
        )
    )
    executable = Path(
        os.environ.get(
            "SYMBIAN_FAILURE_HANDLER_EMULATOR",
            str(
                root
                / ".symbian/emulator-control-build/build/bin"
                / "EKA2L1.app/Contents/MacOS/EKA2L1"
            ),
        )
    )
    (tmp_path / "artifacts.json").write_text(
        json.dumps(
            {
                "backend": backend,
                "application": name,
                "artifact_sha256": hashlib.sha256(
                    artifact.read_bytes()
                ).hexdigest(),
                "emulator_sha256": hashlib.sha256(
                    executable.read_bytes()
                ).hexdigest(),
                "fixture_instance": str(golden),
                "executive_profile": "rm807-113.010.1508",
            },
            indent=2,
        )
        + "\n"
    )
    instance = tmp_path / "instance"
    shutil.copytree(golden, instance)
    drive = instance / "data/drives/rm-807/c"
    target = drive / "sys/bin" / f"{name}.exe"
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(artifact, target)
    (instance / "config.yml").write_text(
        f"data-storage: data\ncpu: {backend}\ndevice: 0\nlanguage: 1\n"
        "enable-gdb-stub: false\nlog-svc: false\n"
        'log-filter: "*:error Emulated.Stdout:trace"\n'
    )
    report = drive / "private" / f"{uid:08x}" / "failure.txt"
    report.unlink(missing_ok=True)
    log = tmp_path / "frontend.log"
    with tempfile.TemporaryDirectory(prefix="failure-", dir="/tmp") as private:
        control = Control(Path(private) / "control.sock")
        environment = {
            **os.environ,
            **background_environment(),
            "EKA2L1_DATA_ROOT": str(instance),
            "EKA2L1_EXPERIMENTAL_SVC_PROFILE": "rm807-113.010.1508",
            "EKA2L1_RESEARCH_CONTROL_SOCKET": str(control.endpoint),
            "EKA2L1_RESEARCH_FIT_DISPLAY": "1",
        }
        with log.open("w") as output:
            process = subprocess.Popen(
                [
                    executable_for_session(executable, Path(private)),
                    "--device",
                    "RM-807",
                    "--run",
                    f"C:\\sys\\bin\\{name}.exe",
                ],
                cwd=tmp_path,
                env=environment,
                stdout=output,
                stderr=subprocess.STDOUT,
            )
            try:
                marker = (
                    "FA:UI_READY"
                    if application == "probe"
                    else "The next CHECK fails intentionally"
                )
                _wait_for_marker(log, marker, process)
                if application == "demo":
                    # The fatal observer precedes Abseil's fatal log sink.
                    # Wait for real presentation rather than that later log.
                    deadline = time.monotonic() + 20
                    attempt = 0
                    while True:
                        attempt += 1
                        try:
                            frame = control.capture(
                                f"waiting-for-view-{attempt}"
                            )
                        except StatusError as error:
                            assert error.code == Code.UNAVAILABLE
                            assert time.monotonic() < deadline
                            time.sleep(0.05)
                            continue
                        shutil.copyfile(frame["path"], tmp_path / "waiting.png")
                        with Image.open(frame["path"]) as image:
                            width, height = image.size
                            rgb = image.convert("RGB").getpixel(
                                (width // 4, height * 94 // 100)
                            )
                        if all(
                            abs(a - b) <= 4
                            for a, b in zip(rgb, (99, 49, 66), strict=True)
                        ):
                            break
                        assert time.monotonic() < deadline, rgb
                        time.sleep(0.05)
                capture = _control_call(control.capture, "visible")
                shutil.copyfile(capture["path"], tmp_path / "visible.png")
                with Image.open(capture["path"]) as image:
                    width, height = image.size
                    if application == "probe":
                        red, green, blue = image.convert("RGB").getpixel(
                            (width // 2, height // 2)
                        )
                        assert green > 220 and red < 20 and blue < 20
                if application == "probe":
                    _wait_for_marker(
                        log, "FA:WRITER_WAITING_FOR_UI_EXIT", process
                    )
                    assert "FA:UI_FAILURE_WRITE_DONE" in log.read_text()
                    assert report.read_bytes() == (
                        b"report survived UI failure\n"
                    )
                    _control_call(control.close_task, uid)
                    _wait_for_marker(
                        log, "FA:WRITER_DRAINING_WITHOUT_WINDOW", process
                    )
                    # The process is alive, but its window has already gone.
                    assert process.poll() is None
                    assert not any(
                        item["uid"] == uid
                        for item in _control_call(control.status)[
                            "process_exits"
                        ]
                    )
                    capture = _control_call(
                        control.capture, "closed-while-writing"
                    )
                    shutil.copyfile(
                        capture["path"], tmp_path / "closed-while-writing.png"
                    )
                    # A bare Window Server may retain the last frame until
                    # another app repaints it. Query actual task membership.
                    with pytest.raises(StatusError) as closed:
                        _control_call(control.close_task, uid)
                    assert closed.value.code == Code.NOT_FOUND
                    assert report.read_bytes() == (
                        b"report survived UI failure\n"
                    )
                else:
                    x = capture["logical_width"] // 4
                    y = capture["logical_height"] - 30
                    _control_call(control.pointer, x, y, "press")
                    _control_call(control.pointer, x, y, "release")
                assert process.wait(timeout=30) == 0
                exits = control.exit_report()["process_exits"]
                (tmp_path / "exits.json").write_text(json.dumps(exits))
                app_exits = [item for item in exits if item["uid"] == uid]
                assert len(app_exits) == 1
                assert app_exits[0]["type"] == 0
                assert app_exits[0]["reason"] == (
                    0 if application == "probe" else 1
                )
                if application == "probe":
                    assert report.read_bytes() == (
                        b"failure report survived UI exit\n"
                    )
                    assert "FA:ALL_TASKS_DONE" in log.read_text()
                else:
                    assert b"Intentional failure-handler example" in (
                        report.read_bytes()
                    )
                shutil.copyfile(report, tmp_path / "failure.txt")
            finally:
                _stop(process)
                # Keep captures/logs, but never retain copied firmware in tests.
                shutil.rmtree(instance)
