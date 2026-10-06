"""Opt-in original native utility calls on a named emulator fixture."""

import json
import os
import shutil
import time
from pathlib import Path

import pytest
from PIL import Image

from symbian.emulator import Control
from symbian.emulator.launch import session
from symbian.status import StatusError
from symbian.tests.test_guest_gui import _ready

ROOT = Path(__file__).resolve().parents[2]
SDK = os.environ.get("SYMBIAN_APP_SDK")
FIRMWARE = os.environ.get("SYMBIAN_NATIVE_FIRMWARE")
STORE = os.environ.get("SYMBIAN_NATIVE_FIRMWARE_STORE")
pytestmark = pytest.mark.skipif(
    not all((SDK, FIRMWARE, STORE)),
    reason="Set SDK and exact native-utility firmware fixture/store",
)


@pytest.mark.parametrize(
    ("name", "uid"),
    [
        ("central_repository_app_classic", 0xE0000E0D),
        ("bafl_app_classic", 0xE0000E0E),
        ("calendar_app_classic", 0xE0000E0F),
        ("messaging_app_classic", 0xE0000E10),
        ("uri_app_classic", 0xE0000E11),
        ("zlib_app_classic", 0xE0000E12),
        ("png_app_classic", 0xE0000E13),
        ("jpeg_app_classic", 0xE0000E14),
        ("freetype_app_classic", 0xE0000E15),
    ],
)
def test_public_native_utility_on_named_firmware(tmp_path, name, uid):
    """Requires clean process exits after real setting/file/stream reads."""
    project = tmp_path / name
    shutil.copytree(ROOT / "examples" / name, project)
    (project / "sdk-location.json").write_text(
        json.dumps({"sdk": str(Path(SDK).resolve().parent)})
    )
    with session(
        ROOT,
        project=project,
        overrides={"firmware": FIRMWARE, "store": Path(STORE)},
    ) as active:
        control = Control(active.endpoint)
        deadline = time.monotonic() + 25
        for attempt in range(100):
            assert time.monotonic() < deadline
            try:
                frame = control.capture(f"{name}-ready-{attempt}")
                with Image.open(frame["path"]) as captured:
                    pixel = (
                        captured.convert("RGB")
                        .resize((360, 640))
                        .getpixel((80, 600))
                    )
                if pixel[1] > pixel[0] + 20:
                    break
            except StatusError:
                pass
            time.sleep(0.1)
        else:
            pytest.fail("The native feature button did not appear")
        _ready(lambda: control.pointer(80, 600, "press"))
        _ready(lambda: control.pointer(80, 600, "release"))
        for attempt in range(100):
            assert time.monotonic() < deadline
            frame = control.capture(f"{name}-result-{attempt}")
            with Image.open(frame["path"]) as captured:
                pixel = (
                    captured.convert("RGB")
                    .resize((360, 640))
                    .getpixel((40, 290))
                )
            if pixel[1] > pixel[0] + 50:
                break
            time.sleep(0.1)
        else:
            pytest.fail("The native API action did not show success")
        if name == "jpeg_app_classic":
            with Image.open(frame["path"]) as captured:
                red, green, blue = (
                    captured.convert("RGB")
                    .resize((360, 640))
                    .getpixel((180, 430))
                )
            assert red > green + 80 and red > blue + 80
        _ready(lambda: control.pointer(270, 600, "press"))
        _ready(lambda: control.pointer(270, 600, "release"))
        matching = []
        while time.monotonic() < deadline:
            try:
                matching = [
                    item
                    for item in control.exit_report()["process_exits"]
                    if item["uid"] == uid
                ]
            except StatusError:
                pass
            if matching:
                break
            time.sleep(0.2)
        assert len(matching) == 1
        assert matching[0]["type"] == 0
        assert matching[0]["reason"] == 0
