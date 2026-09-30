"""Opt-in live debugging regression for the preserved RM-807 research fixture.

These tests execute real guest SDK instructions. The default profile retains
the heap failure; the opt-in profile completes the initial drawing function.
Neither establishes rendered pixels, input delivery or normal guest shutdown.
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


@pytest.mark.parametrize("mismatched_rom", [False, True])
def test_experimental_profile_rejects_unknown_selection_and_changed_rom(
    tmp_path, mismatched_rom
):
    """Checks actual frontend rejection before the GUI is mapped."""
    golden = Path(GOLDEN).resolve()
    original = golden / "data/roms/rm-807/SYM.ROM"
    digest = "b5c1ea63cb6359270c5b7cfb1bb453594e208a01b8aeb5b5e020f37d546f7086"
    assert _digest(original) == digest
    instance = tmp_path / "instance"
    shutil.copytree(golden, instance)
    target = instance / "data/drives/rm-807/c/sys/bin/gui_app.exe"
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(Path(BUILD).resolve() / "gui_app.exe", target)
    (instance / "config.yml").write_text(
        "data-storage: data\ncpu: dynarmic\ndevice: 0\nlanguage: 1\n"
        "enable-gdb-stub: false\nlog-svc: true\n"
    )
    if mismatched_rom:
        private_rom = instance / "data/roms/rm-807/SYM.ROM"
        with private_rom.open("r+b") as file:
            file.seek(-1, os.SEEK_END)
            byte = file.read(1)[0]
            file.seek(-1, os.SEEK_END)
            file.write(bytes([byte ^ 1]))
        profile = "rm807-113.010.1508"
        rejection = "rejected mismatched ROM fingerprint"
    else:
        profile = "unknown-profile"
        rejection = "Unsupported experimental executive profile or ROM"
    env = os.environ.copy()
    env["EKA2L1_DATA_ROOT"] = str(instance)
    env["EKA2L1_EXPERIMENTAL_SVC_PROFILE"] = profile
    log_path = tmp_path / "frontend.log"
    with log_path.open("w") as log:
        process = subprocess.Popen(
            [
                Path(EMULATOR).resolve(),
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
            while rejection not in log_path.read_text():
                assert process.poll() is None, log_path.read_text()
                assert time.monotonic() < deadline, log_path.read_text()
                time.sleep(0.05)
            assert "gui_app.exe (UID3=" not in log_path.read_text()
            assert "Using experimental RM-807" not in log_path.read_text()
        finally:
            process.terminate()
            try:
                process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=5)
            (tmp_path / "emulator-exit.txt").write_text(str(process.returncode))
            assert _digest(original) == digest


@pytest.mark.parametrize("experimental", [False, True])
def test_live_source_breakpoints_and_single_step_remain_halted(
    tmp_path, experimental
):
    """Checks actual GDB stops under default and guarded firmware profiles."""
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
            "2ef4145fa9323837d3d16d8914652d69f0b703a747b4dbb52ab83db83b727792"
        ),
        build
        / "gui_app.elf": (
            "7b2918ba000c8faf039069a3571816a6203edde129a5cb9c2144e475d582d05f"
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
    if experimental:
        final_stops = (
            "delete breakpoints\nbreak GuiMain\ncontinue\n"
            'printf "GUI_MAIN_PC=%#x\\n", $pc\n'
            "delete breakpoints\nbreak *0x804c26f0\ncontinue\n"
            "set arm force-mode arm\nx/2i $pc\n"
            'printf "REGISTER_READ_PC=%#x\\n", $pc\n'
            "stepi\n"
            'printf "REGISTER_READ_DONE=%#x URO=%#x\\n", $pc, $r0\n'
            "delete breakpoints\nset arm force-mode auto\n"
            "break app.cc:174\ncontinue\n"
            'printf "SESSION_CONNECT_PC=%#x RESULT=%d\\n", $pc, $r0\n'
            "delete breakpoints\nbreak DrawGui\ncontinue\n"
            'printf "DRAW_GUI_PC=%#x\\n", $pc\n'
            "print model\nprint layout\nfinish\n"
            'printf "DRAW_RETURN_PC=%#x\\n", $pc\n'
            "disconnect\nquit\n"
        )
    else:
        final_stops = (
            "break *0x804bfc40\ncontinue\n"
            'printf "EXIT_SVC=%#x REASON=%d\\n", $pc, $r0\n'
            "x/4i $pc\ndisconnect\nquit\n"
        )
    script.write_text(
        "set pagination off\nset confirm off\nset architecture arm\n"
        "set remotetimeout 10\n"
        f'file "{build / "gui_app.elf"}"\n'
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
        "delete breakpoints\nset arm fallback-mode arm\n"
        "break *0x804bf730\ncontinue\n"
        'printf "SVC51=%#x,%#x,%#x,%#x LR=%#x\\n", '
        "$r0, $r1, $r2, $r3, $lr\n"
        "x/4i $pc\n"
        "delete breakpoints\nbreak *0x804bf810\ncontinue\n"
        'printf "SVC6D=%#x,%#x,%#x LR=%#x\\n", '
        "$r0, $r1, $r2, $lr\n"
        "x/4i $pc\nx/8wx $r2\n"
        "delete breakpoints\nbreak startup.cc:19\ncontinue\n"
        'printf "HEAP_RESULT=%d HEAP_PC=%#x\\n", $r0, $pc\n' + final_stops
    )
    env = os.environ.copy()
    env.pop("EKA2L1_EXPERIMENTAL_SVC_PROFILE", None)
    if experimental:
        env["EKA2L1_EXPERIMENTAL_SVC_PROFILE"] = "rm807-113.010.1508"
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
            assert "SVC51=0,0x7,0x40ff78,0 LR=0x804cadab" in result.stdout
            assert "SVC6D=0x1,0x40feb4,0x40ff0c LR=0x804cd55f" in (
                result.stdout
            )
            kernel_log = (instance / "EKA2L1.log").read_text()
            assert "gui_app.exe (UID3=0xE0000811) runtime code: 0x70000000" in (
                kernel_log
            )
            if experimental:
                assert "HEAP_RESULT=0 HEAP_PC=0x700009e8" in result.stdout
                assert "GUI_MAIN_PC=0x7000002a" in result.stdout
                assert "REGISTER_READ_PC=0x804c26f0" in result.stdout
                assert "REGISTER_READ_DONE=0x804c26f4 URO=0" in result.stdout
                assert "SESSION_CONNECT_PC=0x70000032 RESULT=0" in result.stdout
                assert "DRAW_GUI_PC=0x700003ae" in result.stdout
                assert "DRAW_RETURN_PC=0x700002f0" in result.stdout
                assert "count_ = 0, running_ = true" in result.stdout
                assert "width = 360, height = 640" in result.stdout
                assert "Using experimental RM-807" in kernel_log
                assert "Can't open object: $HEAP" not in kernel_log
            else:
                assert "HEAP_RESULT=-1 HEAP_PC=0x700009e8" in result.stdout
                assert "EXIT_SVC=0x804bfc40 REASON=-1" in result.stdout
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
