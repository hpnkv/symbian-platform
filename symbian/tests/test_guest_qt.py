"""Opt-in original Qt widget rendering, input and complete emulator shutdown."""

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

from symbian.e32 import convert_imported_executable
from symbian.emulator import Control
from symbian.emulator.background import (
    background_environment,
    executable_for_session,
)
from symbian.packaging.registration import compile_registration
from symbian.tests.test_guest_gui import _ready

GOLDEN = os.environ.get("SYMBIAN_QT_GOLDEN_ROOT")
BUILD = os.environ.get("SYMBIAN_QT_BUILD")
EMULATOR = os.environ.get("SYMBIAN_EKA2L1_EXECUTABLE")
pytestmark = pytest.mark.skipif(
    not all((GOLDEN, BUILD, EMULATOR)),
    reason="Set Qt build, preserved RM-807 firmware and emulator executable",
)


@pytest.mark.parametrize("backend", ("dyncom", "dynarmic"))
def test_original_qt_button_and_shutdown(tmp_path, backend):
    """Checks delivered pixels, clicked→quit, destructors and host teardown."""
    build = Path(BUILD).resolve()
    project = Path(__file__).resolve().parents[2] / "examples/qt_app"
    report = json.loads((build / "report.json").read_text())
    assert report["reproducible"] is True
    for path, digest in report["inputs"].items():
        assert hashlib.sha256(Path(path).read_bytes()).hexdigest() == digest
    executable = (build / "qt_app.exe").read_bytes()
    assert hashlib.sha256(executable).hexdigest() == report["sha256"]
    proxies = [
        Path(path).read_bytes()
        for path in report["inputs"]
        if path.endswith(".dso")
    ]
    assert (
        convert_imported_executable(
            (build / "qt_app.elf").read_bytes(), proxies, 0xE0000821
        )
        == executable
    )

    instance = tmp_path / "instance"
    shutil.copytree(Path(GOLDEN), instance)
    drive = instance / "data/drives/rm-807/c"
    target = drive / "sys/bin/qt_app.exe"
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(executable)
    assets, _ = compile_registration(
        project,
        {"caption": "Symbian Qt", "short_caption": "Qt"},
        "qt_app.exe",
        0xE0000821,
    )
    for virtual, data in assets:
        target = drive / virtual.removeprefix("!:\\").replace("\\", "/")
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
    (instance / "config.yml").write_text(
        f"data-storage: data\ncpu: {backend}\ndevice: 0\nlanguage: 1\n"
        "enable-gdb-stub: false\n"
    )

    with tempfile.TemporaryDirectory(prefix="symbian-qt-") as private:
        endpoint = Path(private) / "control.sock"
        control = Control(endpoint)
        env = {
            **os.environ,
            **background_environment(),
            "EKA2L1_DATA_ROOT": str(instance),
            "EKA2L1_EXPERIMENTAL_SVC_PROFILE": "rm807-113.010.1508",
            "EKA2L1_RESEARCH_CONTROL_SOCKET": str(endpoint),
        }
        with (tmp_path / "frontend.log").open("w") as log:
            process = subprocess.Popen(
                [
                    executable_for_session(Path(EMULATOR), tmp_path),
                    "--device",
                    "RM-807",
                    "--run",
                    r"C:\sys\bin\qt_app.exe",
                ],
                env=env,
                stdout=log,
                stderr=subprocess.STDOUT,
            )
            try:
                deadline = time.monotonic() + 60
                while True:
                    assert process.poll() is None
                    frame = _ready(lambda: control.capture("qt-button"))
                    assert frame["source"] == "eka2l1-screen-texture"
                    with Image.open(frame["path"]) as captured:
                        image = captured.convert("RGB").resize((360, 640))
                        # Both text lines occupy the center of the full-screen
                        # Plastique button. The background and unpainted screen
                        # do not contain this many dark pixels in this region.
                        histogram = (
                            image.convert("L")
                            .crop((90, 290, 270, 350))
                            .histogram()
                        )
                        dark = sum(histogram[:100])
                    if dark > 150:
                        shutil.copyfile(frame["path"], tmp_path / "button.png")
                        break
                    assert time.monotonic() < deadline
                    time.sleep(0.1)
                time.sleep(0.5)
                assert control.status()["process_exits"] == []
                _ready(lambda: control.pointer(180, 320, "press"))
                time.sleep(0.2)
                _ready(lambda: control.pointer(180, 320, "release"))
                assert process.wait(timeout=30) == 0
                final = json.loads(
                    endpoint.with_name(
                        endpoint.name + ".status.json"
                    ).read_text()
                )
                exits = final["result"]["process_exits"]
                assert any(
                    item["uid"] == 0xE0000821
                    and item["reason"] == 0
                    and item["type"] == 0
                    for item in exits
                ), exits
            finally:
                if process.poll() is None:
                    process.kill()
                    process.wait(timeout=10)
