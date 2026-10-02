"""Owned IDE launches, cancellation and automatic ARM symbol relocation."""

import json
import os
import re
import selectors
import shutil
import socket
import subprocess
import sys
import time
from functools import partial
from pathlib import Path

import pytest

from symbian.emulator import Control
from symbian.emulator.gdb import quote
from symbian.emulator.ide import configure
from symbian.emulator.launch import _stop, session
from symbian.status import Code, StatusError
from symbian.tests.test_guest_gui import _frame, _ready

WORKSPACE = os.environ.get("SYMBIAN_IDE_WORKSPACE")
live = pytest.mark.skipif(
    not WORKSPACE, reason="Set SYMBIAN_IDE_WORKSPACE to the prepared workspace"
)


def test_missing_fixture_fails_before_creating_outputs(tmp_path):
    with pytest.raises(StatusError) as failure:
        with session(tmp_path):
            pytest.fail("Missing fixture started")
    assert failure.value.code == Code.FAILED_PRECONDITION
    assert list(tmp_path.iterdir()) == []


def test_gdb_paths_preserve_quotes_and_backslashes():
    assert quote(Path('/tmp/source "name"/a\\b')) == (
        '"/tmp/source \\"name\\"/a\\\\b"'
    )


def test_stop_reaps_owned_child_and_leaves_other_child_running():
    children = [
        subprocess.Popen([sys.executable, "-c", "import time; time.sleep(20)"])
        for _ in range(2)
    ]
    try:
        _stop(children[0])
        assert children[0].poll() is not None
        assert children[1].poll() is None
    finally:
        for child in children:
            _stop(child)


def test_ide_configuration_preserves_profiles_and_quotes_launcher_paths(
    tmp_path,
):
    root = tmp_path / 'workspace "with quotes"'
    idea = root / "examples/gui_app/.idea"
    idea.mkdir(parents=True)
    (idea / "workspace.xml").write_text(
        '<project version="4"><component name="Existing" /></project>'
    )
    (idea / "debug-profiles.xml").write_text(
        '<project version="4"><component name="Debug Profiles">'
        '<debug-profiles><debug-profile><option name="id" value="other" />'
        "</debug-profile></debug-profiles></component></project>"
    )
    host = root / ".idea"
    host.mkdir()
    (host / "workspace.xml").write_text(
        '<project version="4"><component name="CurrentDebugProfile">'
        '<option name="debugProfileId" value="host-lldb" /></component>'
        '<component name="CMakeSettings"><configurations>'
        '<configuration PROFILE_NAME="Debug" ENABLED="true" />'
        "</configurations></component></project>"
    )
    result = configure(root, Path(sys.executable))
    assert 'value="other"' in (idea / "debug-profiles.xml").read_text()
    assert 'name="Existing"' in (idea / "workspace.xml").read_text()
    assert 'value="host-lldb"' in (host / "workspace.xml").read_text()
    host_run = (host / "runConfigurations/GUI_Run.xml").read_text()
    assert 'TARGET_NAME="gui_app_run"' in host_run
    assert 'CONFIG_NAME="Debug"' in host_run
    assert (
        str(root / "build/debug/gui_app_run").replace('"', "&quot;") in host_run
    )
    # Version discovery must work without a fixture or emulator process.
    probe = subprocess.run(
        [result["wrapper"], "--version"],
        capture_output=True,
        text=True,
        timeout=5,
        check=False,
    )
    assert probe.returncode == 0, probe.stderr
    assert "Python" in probe.stdout
    assert not (root / ".symbian/gui-runs").exists()


def _launch(tmp_path, executable=None):
    log_path = tmp_path / "launcher.log"
    with log_path.open("w") as log:
        process = subprocess.Popen(
            (
                [str(executable)]
                if executable
                else [
                    sys.executable,
                    "-m",
                    "symbian.emulator.launch",
                    "--root",
                    WORKSPACE,
                ]
            ),
            stdout=log,
            stderr=subprocess.STDOUT,
        )
    deadline = time.monotonic() + 20
    try:
        while time.monotonic() < deadline:
            text = log_path.read_text(errors="replace")
            found = re.search(r"Emulator session: ([^\n]+)", text)
            if found:
                directory = Path(found[1])
                manifest = json.loads((directory / "launch.json").read_text())
                control = Control(Path(manifest["endpoint"]))
                _ready(control.status)
                return process, directory, manifest, control
            assert process.poll() is None, text
            time.sleep(0.05)
        pytest.fail("Launcher startup timed out")
    except BaseException:
        _stop(process)
        raise


@live
def test_root_run_target_starts_emulator_and_exits_through_sdk(tmp_path):
    """Runs the actual host executable selected by CLion's root CMake model."""
    launcher = Path(WORKSPACE) / "build/debug/gui_app_run"
    assert launcher.is_file(), "Build the root gui_app_run target first"
    process, _directory, _manifest, control = _launch(tmp_path, launcher)
    try:
        _frame(control, 0, "root-run", tmp_path)
        _ready(partial(control.pointer, 296, 575, "press"))
        assert process.wait(timeout=20) == 0
    finally:
        _stop(process)


@live
def test_foreground_run_renders_input_and_exits_normally(tmp_path):
    process, directory, _manifest, control = _launch(tmp_path)
    try:
        _frame(control, 0, "initial", tmp_path)
        for action in ("press", "release"):
            _ready(partial(control.pointer, 64, 575, action))
        _frame(control, 1, "one", tmp_path)
        # Exit is triggered on down and removes the window immediately.
        _ready(partial(control.pointer, 296, 575, "press"))
        assert process.wait(timeout=15) == 0
        manifest = json.loads((directory / "launch.json").read_text())
        assert manifest["frontend_exit"] == 0
        assert manifest["inputs_unchanged"] is True
        report = Control(directory / "control.sock").exit_report()
        assert report["process_exits"] == [
            {
                "uid": 0xE0000811,
                "type": 0,
                "reason": 0,
                "name": "gui_app[e0000811]0001",
            }
        ]
        assert not control.endpoint.parent.exists()
    finally:
        _stop(process)


@live
def test_ide_stop_reaps_frontend_and_removes_endpoint(tmp_path):
    process, directory, original, control = _launch(tmp_path)
    try:
        _frame(control, 0, "before-stop", tmp_path)
        process.terminate()
        assert process.wait(timeout=10) == 130
        with pytest.raises(ProcessLookupError):
            os.kill(original["pid"], 0)
        assert not control.endpoint.parent.exists()
        manifest = json.loads((directory / "launch.json").read_text())
        assert manifest["inputs_unchanged"] is True
        assert manifest["frontend_exit"] is not None
    finally:
        _stop(process)


@live
def test_debug_port_collision_refuses_new_instance():
    with socket.socket() as occupied:
        occupied.bind(("127.0.0.1", 0))
        with pytest.raises(StatusError) as failure:
            with session(
                Path(WORKSPACE), debug=True, port=occupied.getsockname()[1]
            ):
                pytest.fail("Occupied port accepted")
        assert failure.value.code == Code.ALREADY_EXISTS


@live
def test_debug_launcher_relocates_before_source_breakpoints(tmp_path):
    gdb = shutil.which("arm-none-eabi-gdb")
    assert gdb
    with socket.socket() as reservation:
        reservation.bind(("127.0.0.1", 0))
        port = reservation.getsockname()[1]
    result = subprocess.run(
        [
            sys.executable,
            "-m",
            "symbian.emulator.launch",
            "--root",
            WORKSPACE,
            "--port",
            str(port),
            "--gdb",
            gdb,
            "-q",
            "-nx",
            "--batch",
            "-ex",
            "file " + quote(Path(WORKSPACE) / ".symbian/gui-app/gui_app.elf"),
            "-ex",
            f"target remote 127.0.0.1:{port}",
            "-ex",
            "break GuiMain",
            "-ex",
            "continue",
            "-ex",
            'printf "IDE_GUI_PC=%#x\\n", $pc',
            "-ex",
            "break DrawGui",
            "-ex",
            "continue",
            "-ex",
            "print model",
            "-ex",
            "stepi",
            "-ex",
            'printf "IDE_STEP_PC=%#x\\n", $pc',
            "-ex",
            "disconnect",
        ],
        capture_output=True,
        text=True,
        timeout=40,
        check=False,
    )
    (tmp_path / "gdb.log").write_text(result.stdout + result.stderr)
    assert result.returncode == 0, result.stdout + result.stderr
    assert "IDE_GUI_PC=0x7000002a" in result.stdout
    assert "count_ = 0, running_ = true" in result.stdout
    draw = re.search(r"Breakpoint 2 at (0x[0-9a-f]+):", result.stdout)
    step = re.search(r"IDE_STEP_PC=(0x[0-9a-f]+)", result.stdout)
    assert draw and step, result.stdout
    assert 0 < int(step[1], 16) - int(draw[1], 16) < 32
    directory = Path(re.search(r"Emulator session: ([^\n]+)", result.stderr)[1])
    mapping = json.loads((directory / "gdb-mapping.json").read_text())
    assert mapping == {"runtime_base": 0x70000000, "symbol_slide": 0x6FFF8000}
    manifest = json.loads((directory / "launch.json").read_text())
    assert manifest["inputs_unchanged"] is True
    with pytest.raises(ProcessLookupError):
        os.kill(manifest["pid"], 0)


@live
def test_debug_launcher_preserves_gdb_machine_interface(tmp_path):
    """Exercises the same MI protocol used by the IDE debugger frontend."""
    gdb = shutil.which("arm-none-eabi-gdb")
    assert gdb
    with socket.socket() as reservation:
        reservation.bind(("127.0.0.1", 0))
        port = reservation.getsockname()[1]
    transcript = bytearray()
    with (tmp_path / "mi-launch.log").open("w") as log:
        process = subprocess.Popen(
            [
                sys.executable,
                "-m",
                "symbian.emulator.launch",
                "--root",
                WORKSPACE,
                "--port",
                str(port),
                "--gdb",
                gdb,
                "-q",
                "-nx",
                "--interpreter=mi2",
            ],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=log,
        )
    selector = selectors.DefaultSelector()
    selector.register(process.stdout, selectors.EVENT_READ)

    def exchange(command, expected):
        start = len(transcript)
        process.stdin.write(command.encode() + b"\n")
        process.stdin.flush()
        deadline = time.monotonic() + 20
        while expected.encode() not in transcript[start:]:
            assert process.poll() is None, transcript.decode(errors="replace")
            assert time.monotonic() < deadline, transcript.decode(
                errors="replace"
            )
            if selector.select(timeout=0.1):
                data = os.read(process.stdout.fileno(), 65536)
                assert data, transcript.decode(errors="replace")
                transcript.extend(data)
        return transcript[start:].decode(errors="replace")

    try:
        symbols = Path(WORKSPACE) / ".symbian/gui-app/gui_app.elf"
        exchange("1-file-exec-and-symbols " + quote(symbols), "1^done")
        exchange(f"2-target-select remote 127.0.0.1:{port}", "2^connected")
        result = exchange("3-break-insert GuiMain", "3^done")
        assert 'addr="0x7000002a"' in result
        result = exchange("4-exec-continue", '*stopped,reason="breakpoint-hit"')
        assert 'addr="0x7000002a"' in result
        result = exchange('5-data-evaluate-expression "$pc"', "5^done")
        assert 'value="0x7000002a' in result
        # Read the actual compiler's named veneer. Its position changes when
        # the owner edits the GUI, while the ARM interworking contract remains.
        disassembly = subprocess.run(
            ["/opt/homebrew/opt/llvm/bin/llvm-objdump", "-d", symbols],
            capture_output=True,
            text=True,
            check=True,
            timeout=5,
        ).stdout
        veneer = re.search(
            r"^([0-9a-f]+) <_ZN10RWsSessionC1Ev@plt>:",
            disassembly,
            re.MULTILINE,
        )
        assert veneer is not None
        directory = Path(
            re.search(
                r"Emulator session: ([^\n]+)",
                (tmp_path / "mi-launch.log").read_text(),
            )[1]
        )
        mapping = json.loads((directory / "gdb-mapping.json").read_text())
        expected = int(veneer[1], 16) + mapping["symbol_slide"]
        result = exchange("6-exec-step-instruction", "*stopped")
        assert f'addr="{expected:#010x}"' in result
        result = exchange('7-data-evaluate-expression "$cpsr & 0x20"', "7^done")
        assert 'value="0"' in result
        exchange("8-gdb-exit", "8^exit")
        assert process.wait(timeout=10) == 0
    finally:
        _stop(process)
        selector.close()
        process.stdin.close()
        process.stdout.close()
        (tmp_path / "mi.log").write_bytes(transcript)
