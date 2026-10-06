"""Opt-in original native utility calls on a named emulator fixture."""

import json
import os
import shutil
import time
from pathlib import Path

import pytest

from symbian.emulator import Control
from symbian.emulator.launch import session
from symbian.status import StatusError

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
        deadline = time.monotonic() + 12
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
