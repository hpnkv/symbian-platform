"""Opt-in GUI rendering, input and SDK exit tests on copied firmware."""

import hashlib
import json
import os
import shutil
import socket
import subprocess
import tempfile
import time
from functools import partial
from pathlib import Path

import pytest
from PIL import Image

from symbian.e32 import convert_imported_executable
from symbian.emulator import Control
from symbian.emulator.background import (
    background_environment,
    executable_for_session,
)
from symbian.status import Code, StatusError

GOLDEN = os.environ.get("SYMBIAN_GUI_DEBUG_GOLDEN_ROOT")
EMULATOR = os.environ.get("SYMBIAN_EKA2L1_EXECUTABLE")
BUILD = os.environ.get("SYMBIAN_GUI_DEBUG_BUILD")
pytestmark = pytest.mark.skipif(
    not all((GOLDEN, EMULATOR, BUILD)),
    reason="Set GUI debug golden root/build and patched EKA2L1 executable",
)


def _digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _published_gui(build: Path) -> dict[Path, str]:
    """Checks the current source/published ELF/E32 pair, allowing IDE edits."""
    report = json.loads((build / "report.json").read_text())
    assert report["reproducible"] is True
    assert all(
        _digest(Path(p)) == digest for p, digest in report["inputs"].items()
    )
    inputs = {
        build / "gui_app.exe": report["sha256"],
        build / "gui_app.elf": report["linked_elf_sha256"],
    }
    assert {path: _digest(path) for path in inputs} == inputs
    proxies = [
        Path(path).read_bytes()
        for path in report["inputs"]
        if path.endswith(".dso")
    ]
    assert (
        convert_imported_executable(
            (build / "gui_app.elf").read_bytes(), proxies, 0xE0000811
        )
        == (build / "gui_app.exe").read_bytes()
    )
    return inputs


def _ready(operation, *, timeout=15):
    """Retries explicit not-delivered/busy results, never ambiguous timeouts."""
    deadline = time.monotonic() + timeout
    while True:
        try:
            return operation()
        except StatusError as error:
            if error.code != Code.UNAVAILABLE or time.monotonic() >= deadline:
                raise
            time.sleep(0.05)


def _counter(path: Path) -> int | None:
    """Reads independent seven-segment centers in the real captured pixels."""
    with Image.open(path) as image:
        assert image.size == (720, 1280)
        image = image.convert("RGB")
        # Fixture: logical 360x640, display scale 2, digits 12 units thick.
        # Explicit sample points keep this oracle independent of model code.
        centers = [
            (30, 6),
            (54, 30),
            (54, 78),
            (30, 102),
            (6, 78),
            (6, 30),
            (30, 54),
        ]
        patterns = {
            63: 0,
            6: 1,
            91: 2,
            79: 3,
            102: 4,
            109: 5,
            125: 6,
            7: 7,
            127: 8,
            111: 9,
        }
        digits = []
        for origin in (42, 114, 186, 258):
            mask = 0
            for index, (x, y) in enumerate(centers):
                rgb = image.getpixel((2 * (origin + x), 2 * (160 + y)))
                if min(rgb) > 200:
                    mask |= 1 << index
            if mask not in patterns:
                return None
            digits.append(patterns[mask])
        # Distinct control colors and the background also prove orientation.
        assert image.getpixel((2, 2)) == (37, 28, 27)
        assert image.getpixel((40, 1080)) == (118, 159, 78)
        assert image.getpixel((280, 1080)) == (56, 138, 191)
        assert image.getpixel((520, 1080)) == (103, 89, 186)
        return 1000 * digits[0] + 100 * digits[1] + 10 * digits[2] + digits[3]


def _frame(control, expected, label, output):
    """Waits for guest redraw through the real graphics driver."""
    deadline = time.monotonic() + 15
    attempt = 0
    while True:
        result = _ready(partial(control.capture, f"{label}-{attempt}"))
        assert result["source"] == "eka2l1-screen-texture"
        path = Path(result["path"])
        assert path.parent == control.endpoint.parent.resolve()
        observed = _counter(path)
        if observed == expected:
            shutil.copyfile(path, output / f"{label}.png")
            return
        assert time.monotonic() < deadline, (expected, observed)
        attempt += 1
        time.sleep(0.05)


def _pulse(control, expected, label):
    """Checks the timer-backed marker independently of the counter digits."""
    deadline = time.monotonic() + 5
    attempt = 0
    while True:
        result = _ready(partial(control.capture, f"{label}-{attempt}"))
        with Image.open(result["path"]) as image:
            pixel = image.convert("RGB").getpixel((40, 40))
        if pixel == ((118, 159, 78) if expected else (37, 28, 27)):
            return
        assert time.monotonic() < deadline, pixel
        attempt += 1
        time.sleep(0.05)


def _raw(endpoint, payload):
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as stream:
        stream.settimeout(5)
        stream.connect(str(endpoint))
        stream.sendall(payload)
        response = bytearray()
        while not response.endswith(b"\n"):
            part = stream.recv(4096)
            assert part, "Incomplete native response"
            response.extend(part)
        return json.loads(response)


@pytest.mark.parametrize("backend", ["dynarmic", "dyncom"])
def test_rendered_counter_input_reset_and_normal_sdk_exit(tmp_path, backend):
    """Checks real EUSER/WS32 rendering, input and normal process exit."""
    golden = Path(GOLDEN).resolve()
    build = Path(BUILD).resolve()
    inputs = {
        golden
        / "data/roms/rm-807/SYM.ROM": (
            "b5c1ea63cb6359270c5b7cfb1bb45359"
            "4e208a01b8aeb5b5e020f37d546f7086"
        ),
        golden
        / "data/drives/z/rm-807/sys/bin/euser.dll": (
            "3cec7e1546f8ed0cf64a73fece9fdd8f"
            "e6e4976535ddffd18b7068c19c01357b"
        ),
        **_published_gui(build),
    }
    assert {p: _digest(p) for p in inputs} == inputs
    instance = tmp_path / "instance"
    shutil.copytree(golden, instance)
    target = instance / "data/drives/rm-807/c/sys/bin/gui_app.exe"
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(build / "gui_app.exe", target)
    (instance / "config.yml").write_text(
        f"data-storage: data\ncpu: {backend}\ndevice: 0\nlanguage: 1\n"
        "enable-gdb-stub: false\nlog-svc: true\n"
    )
    # macOS Unix socket names are short; this also enforces a private parent.
    with tempfile.TemporaryDirectory(prefix="symbian-", dir="/tmp") as private:
        endpoint = Path(private) / "control.sock"
        control = Control(endpoint)
        env = os.environ.copy()
        env.update(
            EKA2L1_DATA_ROOT=str(instance),
            EKA2L1_EXPERIMENTAL_SVC_PROFILE="rm807-113.010.1508",
            EKA2L1_RESEARCH_CONTROL_SOCKET=str(endpoint),
            **background_environment(),
        )
        with (tmp_path / "frontend.log").open("w") as log:
            process = subprocess.Popen(
                [
                    executable_for_session(Path(EMULATOR), tmp_path),
                    "--device",
                    "RM-807",
                    "--run",
                    "C:\\sys\\bin\\gui_app.exe",
                ],
                env=env,
                stdout=log,
                stderr=subprocess.STDOUT,
            )
            try:
                _frame(control, 0, "initial", tmp_path)
                assert control.status() == {"process_exits": []}
                assert _raw(endpoint, b"not-json\n")["status"]["code"] == 3
                assert _raw(endpoint, b"x" * 4097)["status"]["code"] == 3
                assert (
                    _raw(endpoint, b'{"operation":"flash"}\n')["status"]["code"]
                    == 12
                )
                for name in ("../escape", "", "a" * 65):
                    with pytest.raises(StatusError) as error:
                        _ready(partial(control.capture, name))
                    assert error.value.code == Code.INVALID_ARGUMENT
                saved = _ready(lambda: control.capture("unique"))
                original = _digest(Path(saved["path"]))
                with pytest.raises(StatusError) as error:
                    _ready(lambda: control.capture("unique"))
                assert error.value.code == Code.ALREADY_EXISTS
                assert _digest(Path(saved["path"])) == original
                for x, y, action in (
                    (-1, 0, "press"),
                    (360, 0, "press"),
                    (0, 640, "release"),
                    (0, 0, "drag"),
                ):
                    with pytest.raises(StatusError) as error:
                        _ready(partial(control.pointer, x, y, action))
                    assert error.value.code == Code.INVALID_ARGUMENT
                for expected, x, y, label in (
                    (1, 64, 575, "one"),
                    (2, 64, 575, "two"),
                    (2, 10, 10, "outside"),
                    (0, 180, 575, "reset"),
                ):
                    _ready(partial(control.pointer, x, y, "press"))
                    _ready(partial(control.pointer, x, y, "release"))
                    _frame(control, expected, label, tmp_path)
                    if label == "one":
                        _pulse(control, True, "delayed-marker")
                    if label == "reset":
                        _pulse(control, False, "reset-marker")
                # Cancellation must keep an already scheduled timer from
                # restoring the marker after the counter has been reset.
                _ready(lambda: control.pointer(64, 575, "press"))
                _ready(lambda: control.pointer(64, 575, "release"))
                _ready(lambda: control.pointer(180, 575, "press"))
                _ready(lambda: control.pointer(180, 575, "release"))
                time.sleep(0.45)
                _pulse(control, False, "cancelled-marker")
                _frame(control, 0, "cancelled-count", tmp_path)
                _ready(lambda: control.pointer(296, 575, "press"))
                # The app exits on down; its window no longer accepts an up.
                # The real frontend automatically closes after this app exits.
                # Its final report survives endpoint closure during teardown.
                try:
                    process.wait(timeout=15)
                except subprocess.TimeoutExpired:
                    # Retain native stacks of this exact owned frontend before
                    # cleanup; successful guest exit is not host-exit proof.
                    subprocess.run(
                        [
                            "/usr/bin/sample",
                            str(process.pid),
                            "1",
                            "1",
                            "-file",
                            str(tmp_path / "frontend-hang.sample"),
                        ],
                        capture_output=True,
                        timeout=5,
                        check=False,
                    )
                    raise
                assert process.returncode == 0
                exits = control.exit_report()["process_exits"]
                shutil.copyfile(
                    endpoint.with_name(endpoint.name + ".status.json"),
                    tmp_path / "control.sock.status.json",
                )
                assert exits == [
                    {
                        "uid": 0xE0000811,
                        "name": "gui_app[e0000811]0001",
                        "type": 0,
                        "reason": 0,
                    }
                ]
                (tmp_path / "process-exit.json").write_text(json.dumps(exits))
                assert (
                    "Calling SVC 0x75 thread_kill"
                    in (tmp_path / "frontend.log").read_text()
                )
            finally:
                if process.poll() is None:
                    process.terminate()
                try:
                    process.wait(timeout=2)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=5)
                (tmp_path / "host-exit.txt").write_text(str(process.returncode))
                assert {p: _digest(p) for p in inputs} == inputs
