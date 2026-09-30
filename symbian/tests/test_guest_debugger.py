"""Opt-in live debugging regression for the preserved RM-807 research fixture.

This test executes real guest SDK instructions. It expects the current heap
failure and proves debugger behavior, not successful GUI execution.
"""

import hashlib
import os
import re
import shutil
import socket
import subprocess
import time
from pathlib import Path

import pytest

GOLDEN = os.environ.get("SYMBIAN_GUI_DEBUG_GOLDEN_ROOT")
EMULATOR = os.environ.get("SYMBIAN_EKA2L1_EXECUTABLE")
BUILD = os.environ.get("SYMBIAN_GUI_DEBUG_BUILD")
pytestmark = pytest.mark.skipif(
    not all((GOLDEN, EMULATOR, BUILD)),
    reason="Set GUI debug golden root/build and EKA2L1 executable",
)


def _digest(path: Path) -> str:
    """Returns the digest of a research input."""
    return hashlib.sha256(path.read_bytes()).hexdigest()


def test_live_source_breakpoints_and_single_step_remain_halted(tmp_path):
    """Checks actual GDB stops and retains the observed SDK startup failure."""
    gdb = shutil.which("arm-none-eabi-gdb")
    assert gdb, "Install the ARM GDB prerequisite for this opt-in test"
    golden = Path(GOLDEN).resolve()
    build = Path(BUILD).resolve()
    executable = Path(EMULATOR).resolve()
    source = Path(__file__).parents[2] / "examples/gui_app"
    inputs = {
        golden
        / "data/roms/rm-807/SYM.ROM": (
            "b5c1ea63cb6359270c5b7cfb1bb453594e208a01b8aeb5b5e020f37d546f7086"
        ),
        golden
        / "data/drives/z/rm-807/sys/bin/euser.dll": (
            "3cec7e1546f8ed0cf64a73fece9fdd8fe6e4976535ddffd18b7068c19c01357b"
        ),
        build
        / "gui_app.exe": (
            "93428ab91029784854db74f36c87d32d441c305572a9476eaeda7d3068a03641"
        ),
        build
        / "gui_app.elf": (
            "1d02b5449ac1b765a77749ee61889efd3fcc3d550888f31910b13784321d38ee"
        ),
    }
    assert {p: _digest(p) for p in inputs} == inputs
    assert executable.is_file()
    instance = tmp_path / "instance"
    shutil.copytree(golden, instance)
    with socket.socket() as reservation:
        reservation.bind(("127.0.0.1", 0))
        port = reservation.getsockname()[1]
    (instance / "config.yml").write_text(
        "data-storage: data\ncpu: dynarmic\ndevice: 0\nlanguage: 1\n"
        f"enable-gdb-stub: true\ngdb-port: {port}\nlog-svc: true\n"
    )
    target = instance / "data/drives/rm-807/c/sys/bin/gui_app.exe"
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(build / "gui_app.exe", target)
    # These addresses are specific to the digest-checked fixture above.
    script = tmp_path / "debug.gdb"
    script.write_text(
        "set pagination off\nset confirm off\nset architecture arm\n"
        "set remotetimeout 10\n"
        f'symbol-file -o 0x6fff8000 "{build / "gui_app.elf"}"\n'
        f'set substitute-path /symbian-src/gui_app "{source}"\n'
        f"target remote 127.0.0.1:{port}\n"
        "break GuiRunThread\ncontinue\n"
        'printf "START_PC=%#x REASON=%d INFO=%#x\\n", $pc, $r0, $r1\n'
        "list startup.cc:18\n"
        "stepi\n"
        'printf "STEP_PC=%#x\\n", $pc\n'
        "python import time; time.sleep(0.1)\n"
        "maintenance flush register-cache\n"
        'printf "STABLE_PC=%#x\\n", $pc\n'
        "stepi\n"
        'printf "SECOND_STEP_PC=%#x\\n", $pc\n'
        "set arm fallback-mode arm\n"
        "break *0x804bf730\ncontinue\n"
        'printf "SVC51=%#x,%#x,%#x,%#x LR=%#x\\n", '
        "$r0, $r1, $r2, $r3, $lr\n"
        "x/4i $pc\n"
        "break *0x804bf810\ncontinue\n"
        'printf "SVC6D=%#x,%#x,%#x LR=%#x\\n", '
        "$r0, $r1, $r2, $lr\n"
        "x/4i $pc\nx/8wx $r2\n"
        "break startup.cc:19\ncontinue\n"
        'printf "HEAP_RESULT=%d HEAP_PC=%#x\\n", $r0, $pc\n'
        "break *0x804bfc40\ncontinue\n"
        'printf "EXIT_SVC=%#x REASON=%d\\n", $pc, $r0\n'
        "x/4i $pc\ndisconnect\nquit\n"
    )
    env = os.environ.copy()
    env["EKA2L1_DATA_ROOT"] = str(instance)
    with (tmp_path / "frontend.log").open("w") as log:
        process = subprocess.Popen(
            [
                executable,
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
            deadline = time.monotonic() + 15
            while (
                "Waiting for gdb" not in (tmp_path / "frontend.log").read_text()
            ):
                assert process.poll() is None, "Emulator exited before GDB"
                assert time.monotonic() < deadline, "GDB listener unavailable"
                time.sleep(0.05)
            result = subprocess.run(
                [gdb, "-q", "-nx", "--batch", "-x", script],
                capture_output=True,
                text=True,
                timeout=30,
                check=False,
            )
            (tmp_path / "gdb.log").write_text(result.stdout + result.stderr)
            assert result.returncode == 0, result.stdout + result.stderr
            values = dict(
                re.findall(
                    r"(START_PC|STEP_PC|STABLE_PC|SECOND_STEP_PC)=(0x[0-9a-f]+)",
                    result.stdout,
                )
            )
            assert len(values) == 4, result.stdout
            start = int(values["START_PC"], 16)
            assert int(values["STEP_PC"], 16) == start + 2
            assert values["STABLE_PC"] == values["STEP_PC"]
            assert int(values["SECOND_STEP_PC"], 16) == start + 4
            assert "REASON=0 INFO=0x40ffc0" in result.stdout
            assert "UserHeap::SetupThreadHeap(EFalse, *info)" in result.stdout
            assert "SVC51=0,0x7,0x40ff80,0 LR=0x804cadab" in result.stdout
            assert "SVC6D=0x1,0x40febc,0x40ff14 LR=0x804cd55f" in (
                result.stdout
            )
            assert "HEAP_RESULT=-1 HEAP_PC=0x700009e8" in result.stdout
            assert "EXIT_SVC=0x804bfc40 REASON=-1" in result.stdout
            kernel_log = (instance / "EKA2L1.log").read_text()
            assert "gui_app.exe (UID3=0xE0000811) runtime code: 0x70000000" in (
                kernel_log
            )
            assert "Can't open object: $HEAP" in kernel_log
        finally:
            process.terminate()
            try:
                process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                # Only the exact subprocess owned by this test is stopped.
                process.kill()
                process.wait(timeout=5)
            (tmp_path / "emulator-exit.txt").write_text(str(process.returncode))
    assert {p: _digest(p) for p in inputs} == inputs
