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
from PIL import Image, ImageDraw, ImageStat

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


@pytest.mark.parametrize("sensor_frame", ["live", "phone-reference"])
@pytest.mark.parametrize("backend", ["dynarmic", "dyncom"])
@pytest.mark.parametrize(
    "landscape", [False, True], ids=["portrait", "landscape"]
)
def test_camera_gl_receives_host_frames(
    tmp_path, backend, landscape, sensor_frame
):
    """Requires sustained real frames, GLES presentation and orderly release."""
    root = Path(WORKSPACE).resolve()
    artifact = Path(
        os.environ.get(
            "SYMBIAN_CAMERA_HOST_ARTIFACT",
            str(
                root
                / (
                    "build/guest-probes-armv6-release-gpu/examples/camera_app/e32/"
                    "camera_app_gles2.exe"
                )
            ),
        )
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
    reference = None
    if sensor_frame == "phone-reference":
        # A native phone frame has a physical up direction. Aspect ratio must
        # never override that reference. Distinct horizontal bands reveal a
        # quarter-turn, upside-down output, channel swap or incorrect crop.
        reference = tmp_path / "phone-reference.png"
        image = Image.new("RGB", (640, 480), (32, 48, 64))
        draw = ImageDraw.Draw(image)
        draw.rectangle((0, 0, 639, 47), fill=(240, 32, 48))
        draw.rectangle((0, 432, 639, 479), fill=(32, 64, 240))
        draw.rectangle((140, 64, 171, 415), fill=(32, 224, 64))
        draw.rectangle((468, 64, 499, 415), fill=(224, 192, 32))
        draw.polygon(
            [
                (320, 96),
                (272, 160),
                (304, 160),
                (304, 352),
                (336, 352),
                (336, 160),
                (368, 160),
            ],
            fill=(240, 240, 240),
        )
        image.save(reference)

    def check_phone_reference(path, mode):
        if reference is None:
            return
        with Image.open(path) as screenshot:
            physical = screenshot.convert("RGB")
            # A capture immediately after screen_mode can still contain the
            # previous compositor frame. Use its actual coordinate system.
            if physical.width > physical.height:
                physical = physical.transpose(Image.Transpose.ROTATE_270)
            # The retained upright 360x480 image starts at physical (0,80).
            # Source x=140 maps to physical x=0, all rows remain visible.
            for x, y, expected in (
                (180, 96, (240, 32, 48)),
                (240, 544, (32, 64, 240)),
                (16, 320, (32, 224, 64)),
                (344, 320, (224, 192, 32)),
                (180, 208, (240, 240, 240)),
                (148, 384, (32, 48, 64)),
                (180, 32, (0, 0, 0)),
            ):
                actual = physical.getpixel((x * 2, y * 2))
                matches = all(
                    abs(a - b) <= 2
                    for a, b in zip(actual, expected, strict=True)
                )
                if not matches:
                    screenshot.save(tmp_path / "reference-failure.png")
                assert matches, (mode, x, y, actual, expected)

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
        if reference is not None:
            environment["EKA2L1_RESEARCH_CAMERA_FRAME"] = str(reference)
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
                            # The socket acknowledges the guest swap before
                            # the host compositor necessarily consumes it.
                            # Do not inspect a partially initialized texture.
                            clear_deadline = time.monotonic() + 1
                            attempt = 0
                            while True:
                                waiting = _retry(
                                    control.capture, f"camera-waiting-{attempt}"
                                )
                                with Image.open(waiting["path"]) as image:
                                    corner = image.convert("RGB").getpixel(
                                        (0, 0)
                                    )
                                if all(
                                    abs(a - b) <= 2
                                    for a, b in zip(
                                        corner, (9, 12, 16), strict=True
                                    )
                                ):
                                    break
                                assert time.monotonic() < clear_deadline, corner
                                attempt += 1
                                time.sleep(0.01)
                            shutil.copyfile(
                                waiting["path"], tmp_path / "waiting.png"
                            )
                            with Image.open(waiting["path"]) as image:
                                # Check asymmetric camera housing and dots in
                                # logical coordinates: landscape must rotate
                                # the complete loading glyph, not just controls.
                                rgb = image.convert("RGB")
                                width, height = (
                                    (640, 360) if landscape else (360, 640)
                                )
                                assert rgb.size == (width * 2, height * 2)
                                unit = min(width, height) // 120
                                cx, cy = width // 2, height // 2 - 6 * unit
                                for x, y, expected in (
                                    (0, 0, (9, 12, 16)),
                                    (cx, cy - 12 * unit, (133, 153, 168)),
                                    (cx + 6 * unit, cy, (66, 196, 204)),
                                    (cx, cy + 15 * unit, (9, 12, 16)),
                                ):
                                    actual = rgb.getpixel((x * 2, y * 2))
                                    assert all(
                                        abs(a - b) <= 2
                                        for a, b in zip(
                                            actual, expected, strict=True
                                        )
                                    ), (landscape, x, y, actual, expected)
                                for dot in range(3):
                                    x = cx + (dot - 1) * 7 * unit
                                    y = cy + 24 * unit
                                    r, g, b = rgb.getpixel((x * 2, y * 2))
                                    assert g > r and b > r and g >= 54
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
                check_phone_reference(frame["path"], int(landscape))
                source_size = re.findall(r"GPU: source (\d+x\d+)", diagnostic)[
                    -1
                ]
                assert source_size == "640x480"
                crop = re.findall(r"GPU: view crop=([\d,]+)", diagnostic)[-1]
                assert crop == "140,0,360,480,"
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
                        assert "orientation=0" in updated
                        assert (
                            f"GPU: orientation metadata={3 if mode else 0}"
                            in updated
                        )
                        expected = "640x360" if mode else "360x640"
                        assert (
                            f"GPU: geometry window={expected} egl=360x640"
                            in updated
                        )
                        settled = _retry(
                            control.capture, f"settled-reference-{phase}-{mode}"
                        )
                        check_phone_reference(settled["path"], mode)
                        # Toggle smoothing at its logical button position.
                        x, y = _button_center(bool(mode))
                        _retry(control.pointer, x, y, "press")
                        _retry(control.pointer, x, y, "release")
                        smooth = mode == int(not landscape)
                        expected_filter = "smooth" if smooth else "nearest"
                        toggle_deadline = time.monotonic() + 5
                        while (
                            f"GPU: filter {expected_filter}"
                            not in log.read_text()[len(previous) :]
                        ):
                            assert time.monotonic() < toggle_deadline
                            time.sleep(0.01)
                        # The input trace precedes the next buffer swap.
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
                            "sensor_frame": sensor_frame,
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
