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
from PIL import Image, ImageChops

from symbian.e32 import convert_imported_executable
from symbian.emulator import Control
from symbian.emulator.background import (
    background_environment,
    executable_for_session,
)
from symbian.emulator.launch import session
from symbian.packaging.registration import compile_registration
from symbian.tests.test_guest_gui import _ready

GOLDEN = os.environ.get("SYMBIAN_QT_GOLDEN_ROOT")
BUILD = os.environ.get("SYMBIAN_QT_BUILD")
EMULATOR = os.environ.get("SYMBIAN_EKA2L1_EXECUTABLE")
direct = pytest.mark.skipif(
    not all((GOLDEN, BUILD, EMULATOR)),
    reason="Set Qt build, preserved RM-807 firmware and emulator executable",
)


@direct
@pytest.mark.parametrize("backend", ("dyncom", "dynarmic"))
def test_original_qt_button_and_shutdown(tmp_path, backend):
    """Checks delivered pixels, clicked→quit, destructors and host teardown."""
    build = Path(BUILD).resolve()
    project = Path(__file__).resolve().parents[2] / "examples/qt_app_classic"
    report = json.loads((build / "report.json").read_text())
    assert report["reproducible"] is True
    for path, digest in report["inputs"].items():
        assert hashlib.sha256(Path(path).read_bytes()).hexdigest() == digest
    executable = (build / "qt_app_classic.exe").read_bytes()
    assert hashlib.sha256(executable).hexdigest() == report["sha256"]
    proxies = [
        Path(path).read_bytes()
        for path in report["inputs"]
        if path.endswith(".dso")
    ]
    assert (
        convert_imported_executable(
            (build / "qt_app_classic.elf").read_bytes(), proxies, 0xE0000821
        )
        == executable
    )

    instance = tmp_path / "instance"
    shutil.copytree(Path(GOLDEN), instance)
    drive = instance / "data/drives/rm-807/c"
    target = drive / "sys/bin/qt_app_classic.exe"
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(executable)
    assets, _ = compile_registration(
        project,
        {"caption": "Symbian Qt", "short_caption": "Qt"},
        "qt_app_classic.exe",
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
                    r"C:\sys\bin\qt_app_classic.exe",
                ],
                env=env,
                stdout=log,
                stderr=subprocess.STDOUT,
            )
            try:
                _button_and_shutdown(process, control, tmp_path)
            finally:
                if process.poll() is None:
                    process.kill()
                    process.wait(timeout=10)


def _button_and_shutdown(process, control, tmp_path):
    """Requires visible QtCore result, explicit Close and normal guest exit."""
    deadline = time.monotonic() + 60
    attempt = 0
    while True:
        assert process.poll() is None
        name = f"qt-button-{attempt}"
        frame = _ready(lambda name=name: control.capture(name))
        attempt += 1
        assert frame["source"] == "eka2l1-screen-texture"
        with Image.open(frame["path"]) as captured:
            image = captured.convert("RGB").resize((360, 640))
            histogram = image.convert("L").crop((90, 70, 270, 100)).histogram()
            dark = sum(histogram[:100])
            light = sum(histogram[150:])
        if 150 < dark < 3000 and light > 3000:
            shutil.copyfile(frame["path"], tmp_path / "button.png")
            before = image.crop((10, 100, 320, 135))
            break
        assert time.monotonic() < deadline
        time.sleep(0.1)
    time.sleep(0.5)
    assert control.status()["process_exits"] == []
    _ready(lambda: control.pointer(180, 84, "press"))
    time.sleep(0.2)
    _ready(lambda: control.pointer(180, 84, "release"))
    result_attempt = 0
    while True:
        assert process.poll() is None
        name = f"qt-result-{result_attempt}"
        frame = _ready(lambda name=name: control.capture(name))
        result_attempt += 1
        with Image.open(frame["path"]) as captured:
            after = captured.convert("RGB").resize((360, 640))
        difference = ImageChops.difference(
            before, after.crop((10, 100, 320, 135))
        )
        if difference.getbbox() is not None:
            shutil.copyfile(frame["path"], tmp_path / "result.png")
            break
        assert time.monotonic() < deadline
        time.sleep(0.1)
    assert control.status()["process_exits"] == []
    _ready(lambda: control.pointer(180, 615, "press"))
    time.sleep(0.2)
    _ready(lambda: control.pointer(180, 615, "release"))
    assert process.wait(timeout=30) == 0
    final = json.loads(
        control.endpoint.with_name(
            control.endpoint.name + ".status.json"
        ).read_text()
    )
    exits = final["result"]["process_exits"]
    assert any(
        item["uid"] == 0xE0000821 and item["reason"] == 0 and item["type"] == 0
        for item in exits
    ), exits


@pytest.mark.skipif(
    not os.environ.get("SYMBIAN_QT_LAUNCH_PROJECT"),
    reason="Set SYMBIAN_QT_LAUNCH_PROJECT with a selected SDK and firmware",
)
@pytest.mark.parametrize("backend", ("dyncom", "dynarmic"))
def test_sdk_qt_launch_stages_resources_and_exits(tmp_path, backend):
    """Exercises the SDK supervisor rather than staging resources by hand."""
    project = Path(os.environ["SYMBIAN_QT_LAUNCH_PROJECT"]).resolve()
    with session(project, project=project, backend=backend) as active:
        manifest = json.loads((active.directory / "launch.json").read_text())
        assert any(
            asset.endswith("qt_app_classic_reg.rsc")
            for asset in manifest["application"]["assets"]
        )
        _button_and_shutdown(active.process, Control(active.endpoint), tmp_path)
    final = json.loads((active.directory / "launch.json").read_text())
    assert final["frontend_exit"] == 0
    assert final["inputs_unchanged"]
