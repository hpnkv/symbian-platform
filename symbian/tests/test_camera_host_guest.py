"""Opt-in real host camera → ECam → GLES2 integration on named firmware."""

import hashlib
import json
import os
import re
import shutil
import subprocess
import tempfile
import time
from pathlib import Path

import pytest
from PIL import Image, ImageStat

from symbian.emulator import Control
from symbian.emulator.background import (
    background_environment,
    executable_for_session,
)
from symbian.emulator.launch import _stop
from symbian.status import Code, StatusError

WORKSPACE = os.environ.get("SYMBIAN_CAMERA_HOST_WORKSPACE")
pytestmark = pytest.mark.skipif(
    not WORKSPACE,
    reason="Requires a host camera, permission and prepared Belle firmware",
)


def _retry(method, *args):
    """Retries only contention rejected before a control request is enqueued."""
    deadline = time.monotonic() + 10
    while True:
        try:
            return method(*args)
        except StatusError as error:
            if error.code != Code.UNAVAILABLE or time.monotonic() >= deadline:
                raise
            time.sleep(0.01)


def _button_center(landscape):
    """Returns the smoothing control at the same physical-screen anchor."""
    return (584, 180) if landscape else (180, 584)


@pytest.mark.parametrize("backend", ["dynarmic", "dyncom"])
@pytest.mark.parametrize(
    "landscape", [False, True], ids=["portrait", "landscape"]
)
def test_camera_gl_receives_host_frames(tmp_path, backend, landscape):
    """Requires sustained real frames, GLES presentation and orderly release."""
    root = Path(WORKSPACE).resolve()
    artifact = root / (
        "build/guest-probes-armv6-release-gpu/examples/camera_app/e32/"
        "camera_app_gles2.exe"
    )
    executable = Path(
        os.environ.get(
            "SYMBIAN_CAMERA_HOST_EMULATOR",
            str(
                root / ".symbian/emulator-control-build/build/bin/"
                "EKA2L1.app/Contents/MacOS/EKA2L1"
            ),
        )
    )
    golden = Path(
        os.environ.get(
            "SYMBIAN_CAMERA_HOST_INSTANCE",
            str(root / ".symbian/instances/delight-import-01"),
        )
    )
    assert artifact.is_file(), "Build camera_app_gles2_e32 first"
    instance = tmp_path / "instance"
    shutil.copytree(golden, instance)
    destination = instance / (
        "data/drives/rm-807/c/sys/bin/camera_app_gles2.exe"
    )
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(artifact, destination)
    (instance / "config.yml").write_text(
        f"data-storage: data\ncpu: {backend}\ndevice: 0\nlanguage: 1\n"
        "enable-gdb-stub: false\nlog-svc: false\n"
        'log-filter: "*:error Driver.Camera:info Emulated.Stdout:trace"\n'
    )
    compat = instance / "compat" / "e0000a5a.yml"
    compat.parent.mkdir(parents=True, exist_ok=True)
    compat.write_text(f"screen-mode: {int(landscape)}\nscreen-rotation: 0\n")
    log = tmp_path / "frontend.log"
    with tempfile.TemporaryDirectory(prefix="camera-", dir="/tmp") as d:
        control = Control(Path(d) / "control.sock")
        environment = {
            **os.environ,
            **background_environment(),
            "EKA2L1_DATA_ROOT": str(instance),
            "EKA2L1_RESEARCH_CONTROL_SOCKET": str(control.endpoint),
            "EKA2L1_RESEARCH_FIT_DISPLAY": "1",
            "EKA2L1_EXPERIMENTAL_SVC_PROFILE": "rm807-113.010.1508",
        }
        with log.open("w") as output:
            process = subprocess.Popen(
                [
                    executable_for_session(executable, Path(d)),
                    "--device",
                    "RM-807",
                    "--run",
                    r"C:\sys\bin\camera_app_gles2.exe",
                ],
                cwd=tmp_path,
                env=environment,
                stdout=output,
                stderr=subprocess.STDOUT,
            )
            try:
                deadline = time.monotonic() + 120
                waiting_captured = False
                waiting_observed_at = None
                first_frame_observed_at = None
                while True:
                    diagnostic = log.read_text(errors="replace")
                    assert process.poll() is None, diagnostic
                    if "GPU: waiting UI presented" in diagnostic:
                        if waiting_observed_at is None:
                            waiting_observed_at = time.monotonic()
                        if not waiting_captured:
                            waiting = _retry(control.capture, "camera-waiting")
                            shutil.copyfile(
                                waiting["path"], tmp_path / "waiting.png"
                            )
                            with Image.open(waiting["path"]) as image:
                                # Slate background and cyan camera glyph;
                                # catches the old solid blue/magenta surface.
                                assert all(
                                    abs(a - b) <= 2
                                    for a, b in zip(
                                        image.convert("RGB").getpixel((0, 0)),
                                        (9, 12, 16),
                                        strict=True,
                                    )
                                )
                                colors = image.convert("RGB").getcolors(4096)
                                assert colors is not None
                                assert any(
                                    g > 150 and b > 150 and r < 100
                                    for _, (r, g, b) in colors
                                )
                            waiting_captured = True
                    if "GPU: first frame" in diagnostic:
                        if first_frame_observed_at is None:
                            first_frame_observed_at = time.monotonic()
                    if "GPU: presented 60 frames in " in diagnostic:
                        break
                    assert time.monotonic() < deadline, diagnostic
                    time.sleep(0.01)
                assert waiting_captured
                assert "GPU: first frame" in diagnostic
                assert diagnostic.index("GPU: waiting UI presented") < (
                    diagnostic.index("GPU: discover begin")
                )
                logical_size = "640x360" if landscape else "360x640"
                assert f"GPU: geometry window={logical_size}" in diagnostic
                assert "GPU: fit viewport=" in diagnostic
                assert "window=360x640" in diagnostic
                assert "camera GPU swap done" in diagnostic
                assert "GPU: stream unavailable" not in diagnostic
                frame = _retry(control.capture, "live-host-camera")
                shutil.copyfile(frame["path"], tmp_path / "live.png")
                with Image.open(frame["path"]) as image:
                    assert image.width > 0 and image.height > 0
                    dimensions = image.size
                with Image.open(frame["path"]) as image:
                    baseline_luma = ImageStat.Stat(image.convert("L")).mean[0]
                assert baseline_luma > 5
                source_size = re.findall(r"GPU: source (\d+x\d+)", diagnostic)[
                    -1
                ]
                assert source_size == "640x480"
                crop = re.findall(r"GPU: view crop=([\d,]+)", diagnostic)[-1]
                assert crop == "0,0,640,480,"
                transition_samples = []
                # Native EGL storage and camera sampling stay fixed. Only the
                # logical window and the overlay follow display orientation.
                for phase in range(3):
                    for mode in [int(not landscape), int(landscape)]:
                        previous = log.read_text(errors="replace")
                        change = _retry(control.screen_mode, mode)
                        assert (change["width"], change["height"]) == (
                            (640, 360) if mode else (360, 640)
                        )
                        # Observe during the transition, not only after the
                        # viewfinder has recovered. Catch blank surfaces.
                        for sample in range(12):
                            capture = _retry(
                                control.capture,
                                f"transition-{phase}-{mode}-{sample}",
                            )
                            with Image.open(capture["path"]) as image:
                                luma = ImageStat.Stat(image.convert("L")).mean[
                                    0
                                ]
                            transition_samples.append(luma)
                            assert luma > baseline_luma * 0.25, (
                                phase,
                                mode,
                                sample,
                                luma,
                            )
                            if sample in (0, 11):
                                shutil.copyfile(
                                    capture["path"],
                                    tmp_path
                                    / f"transition-{phase}-{mode}-{sample}.png",
                                )
                            time.sleep(0.01)
                        rotation_deadline = time.monotonic() + 15
                        while True:
                            updated = log.read_text(errors="replace")[
                                len(previous) :
                            ]
                            if (
                                "GPU: fit viewport=360x480" in updated
                                and "GPU: presented 60 frames in " in updated
                            ):
                                break
                            assert process.poll() is None, updated
                            assert time.monotonic() < rotation_deadline, updated
                            time.sleep(0.01)
                        for unexpected in (
                            "GPU: source texture resized",
                            "GPU: capture resume begin",
                            "GPU: source ",
                            "GPU: surface refresh",
                            "mode rotate",
                        ):
                            assert unexpected not in updated
                        assert set(
                            re.findall(r"GPU: view crop=([\d,]+)", updated)
                        ) == {crop}
                        assert "orientation=1" in updated
                        assert (
                            f"GPU: orientation metadata={0 if mode else 1}"
                            in updated
                        )
                        expected = "640x360" if mode else "360x640"
                        assert (
                            f"GPU: geometry window={expected} egl=360x640"
                            in updated
                        )
                        # Toggle smoothing at its logical button position.
                        x, y = _button_center(bool(mode))
                        _retry(control.pointer, x, y, "press")
                        _retry(control.pointer, x, y, "release")
                        time.sleep(0.1)
                        capture = _retry(
                            control.capture, f"smooth-{phase}-{mode}"
                        )
                        shutil.copyfile(
                            capture["path"],
                            tmp_path / f"smooth-{phase}-{mode}.png",
                        )
                        with Image.open(capture["path"]) as image:
                            assert image.size == (
                                (1280, 720) if mode else (720, 1280)
                            )
                            actual = image.convert("RGB").getpixel(
                                (x * 2, (y - 18) * 2)
                            )
                            smooth = mode == int(not landscape)
                            expected_color = (
                                (67, 196, 204) if smooth else (83, 99, 110)
                            )
                            assert all(
                                abs(a - b) <= 2
                                for a, b in zip(
                                    actual, expected_color, strict=True
                                )
                            ), (actual, expected_color)
                _retry(control.close_task, 0xE0000A5A)
                assert process.wait(timeout=20) == 0
                exits = control.exit_report()["process_exits"]
                app = [x for x in exits if x["uid"] == 0xE0000A5A]
                assert len(app) == 1
                assert app[0]["type"] == 0 and app[0]["reason"] == 0
                assert "GPU: stream close returned" in log.read_text()
                (tmp_path / "evidence.json").write_text(
                    json.dumps(
                        {
                            "backend": backend,
                            "transition_luma": transition_samples,
                            "baseline_luma": baseline_luma,
                            "landscape": landscape,
                            "waiting_to_frame_observed_ms": round(
                                1000
                                * (
                                    first_frame_observed_at
                                    - waiting_observed_at
                                )
                            ),
                            "guest_sha256": hashlib.sha256(
                                artifact.read_bytes()
                            ).hexdigest(),
                            "frontend_sha256": hashlib.sha256(
                                executable.read_bytes()
                            ).hexdigest(),
                            "framebuffer_size": dimensions,
                            "process_exits": app,
                        },
                        indent=2,
                    )
                    + "\n"
                )
            finally:
                _stop(process)
