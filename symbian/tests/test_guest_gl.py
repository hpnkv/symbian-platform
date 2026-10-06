"""Opt-in real GLES2 render, animation, input and shutdown acceptance."""

import json
import os
import shutil
import subprocess
import tempfile
import time
from pathlib import Path

import pytest
from PIL import Image, ImageChops

from symbian.e32 import convert_imported_executable
from symbian.emulator import Control
from symbian.emulator.background import (
    background_environment,
    executable_for_session,
)
from symbian.tests.test_guest_gui import _ready

GOLDEN = os.environ.get("SYMBIAN_GL_GOLDEN_ROOT")
BUILD = os.environ.get("SYMBIAN_GL_BUILD")
EMULATOR = os.environ.get("SYMBIAN_EKA2L1_EXECUTABLE")
pytestmark = pytest.mark.skipif(
    not all((GOLDEN, BUILD, EMULATOR)),
    reason="Set GL build, preserved firmware and emulator executable",
)


def _frame(control, name):
    capture = _ready(lambda: control.capture(name))
    assert capture["source"] == "eka2l1-screen-texture"
    with Image.open(capture["path"]) as captured:
        return captured.convert("RGB").resize((360, 640))


@pytest.mark.parametrize("backend", ["dyncom", "dynarmic"])
def test_lit_cube_animation_black_background_and_exit(tmp_path, backend):
    build = Path(BUILD)
    report = json.loads((build / "report.json").read_text())
    assert report["reproducible"]
    proxies = [
        Path(path).read_bytes()
        for path in report["inputs"]
        if path.endswith(".dso")
    ]
    image = (build / "gl_app.exe").read_bytes()
    assert (
        convert_imported_executable(
            (build / "gl_app.elf").read_bytes(), proxies, 0xE0000831
        )
        == image
    )
    instance = tmp_path / "instance"
    shutil.copytree(Path(GOLDEN), instance)
    destination = instance / "data/drives/rm-807/c/sys/bin/gl_app.exe"
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(image)
    (instance / "config.yml").write_text(
        f"data-storage: data\ncpu: {backend}\ndevice: 0\nlanguage: 1\n"
        "enable-gdb-stub: false\n"
    )
    with tempfile.TemporaryDirectory(prefix="symbian-gl-") as private:
        control = Control(Path(private) / "control.sock")
        env = {
            **os.environ,
            **background_environment(),
            "EKA2L1_DATA_ROOT": str(instance),
            "EKA2L1_EXPERIMENTAL_SVC_PROFILE": "rm807-113.010.1508",
            "EKA2L1_RESEARCH_CONTROL_SOCKET": str(control.endpoint),
        }
        with (tmp_path / "frontend.log").open("w") as log:
            process = subprocess.Popen(
                [
                    executable_for_session(Path(EMULATOR), tmp_path),
                    "--device",
                    "RM-807",
                    "--run",
                    r"C:\sys\bin\gl_app.exe",
                ],
                env=env,
                stdout=log,
                stderr=subprocess.STDOUT,
            )
            try:
                deadline = time.monotonic() + 60
                attempt = 0
                while True:
                    assert process.poll() is None
                    first = _frame(control, f"cube-{attempt}")
                    attempt += 1
                    # Require a substantial nonblack cube and a separate UI.
                    lit = sum(
                        first.crop((60, 130, 300, 440))
                        .convert("L")
                        .histogram()[30:]
                    )
                    button = first.crop((60, 545, 300, 595)).convert("L")
                    background = first.crop((5, 5, 350, 80)).getextrema()
                    if (
                        lit > 6000
                        and sum(button.histogram()[150:]) > 500
                        and all(high < 5 for low, high in background)
                    ):
                        break
                    assert time.monotonic() < deadline
                    time.sleep(0.1)
                first.save(tmp_path / "cube.png")
                # The requested background stays black away from cube/button.
                for channel in first.crop((5, 5, 350, 80)).getextrema():
                    assert channel[1] < 5
                time.sleep(0.7)
                second = _frame(control, "cube-later")
                second.save(tmp_path / "cube-later.png")
                difference = ImageChops.difference(first, second)
                changed = sum(
                    difference.crop((60, 130, 300, 440))
                    .convert("L")
                    .histogram()[15:]
                )
                assert changed > 2000, "Cube did not rotate"
                cube = first.crop((60, 130, 300, 440))
                colors = list(cube.get_flattened_data())
                assert sum(r > b + 25 for r, g, b in colors) > 1000
                assert sum(b > r + 10 for r, g, b in colors) > 100
                # Outside input must leave the app running.
                _ready(lambda: control.pointer(10, 40, "press"))
                _ready(lambda: control.pointer(10, 40, "release"))
                time.sleep(0.2)
                assert control.status()["process_exits"] == []
                _ready(lambda: control.pointer(180, 572, "press"))
                time.sleep(0.2)
                _ready(lambda: control.pointer(180, 572, "release"))
                assert process.wait(timeout=30) == 0
                final = json.loads(
                    control.endpoint.with_name(
                        control.endpoint.name + ".status.json"
                    ).read_text()
                )
                assert any(
                    item["uid"] == 0xE0000831
                    and item["reason"] == 0
                    and item["type"] == 0
                    for item in final["result"]["process_exits"]
                )
            finally:
                if process.poll() is None:
                    process.kill()
                    process.wait(timeout=10)
