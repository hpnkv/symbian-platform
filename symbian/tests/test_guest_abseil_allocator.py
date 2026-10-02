"""Execute the pinned Abseil allocator on a disposable guest instance."""

import os
import shutil
import subprocess
import tempfile
from pathlib import Path

import pytest

from symbian.e32.images import convert_imported_executable
from symbian.emulator import Control
from symbian.emulator.background import (
    background_environment,
    executable_for_session,
)
from symbian.emulator.firmware import EUSER_808, ROM_808
from symbian.emulator.launch import _digest, _stop
from symbian.project.sdk import AppSdk

WORKSPACE = os.environ.get("SYMBIAN_RUNTIME_WORKSPACE")
ABSEIL_SOURCE = os.environ.get("SYMBIAN_ABSEIL_SOURCE")
SDK_MANIFEST = os.environ.get("SYMBIAN_APP_SDK")
pytestmark = pytest.mark.skipif(
    not all((WORKSPACE, ABSEIL_SOURCE, SDK_MANIFEST)),
    reason="Set runtime workspace, prepared Abseil source and SDK manifest",
)


@pytest.fixture
def guest_root():
    """Keep emulator IPC endpoints within macOS UNIX socket path limits."""
    with tempfile.TemporaryDirectory(prefix="absl-guest-", dir="/tmp") as name:
        yield Path(name)


@pytest.mark.parametrize("changed", [False, True])
def test_pinned_abseil_low_level_alloc(tmp_path, guest_root, changed):
    """Check real pages, arena lifetime and a changed-result control."""
    root = Path(WORKSPACE).resolve()
    source = Path(ABSEIL_SOURCE).resolve()
    sdk = AppSdk.load(Path(SDK_MANIFEST)).prefix
    expected_revision = "5650e9cf76d3be4318d5fa3af38ee483ddfd5e4a"
    revision = subprocess.check_output(
        ["git", "-C", str(source), "rev-parse", "HEAD"], text=True
    ).strip()
    assert revision == expected_revision

    project = root / "examples/abseil_allocator_probe"
    build = tmp_path / "build"
    environment = dict(os.environ)
    environment.update(
        SYMBIAN_ABSEIL_SOURCE=str(source),
        SYMBIAN_SDK_PREFIX=str(sdk),
    )
    subprocess.run(
        [
            "cmake",
            "--preset",
            "symbian-pic",
            "-B",
            str(build),
            f"-DSYMBIAN_ABSEIL_CHANGED_ALLOCATOR={'ON' if changed else 'OFF'}",
        ],
        cwd=project,
        env=environment,
        check=True,
        capture_output=True,
        text=True,
    )
    subprocess.run(
        [
            "cmake",
            "--build",
            str(build),
            "--target",
            "abseil_allocator_probe",
            "-j",
            "6",
        ],
        cwd=project,
        env=environment,
        check=True,
        capture_output=True,
        text=True,
    )
    proxies = [
        (sdk / "proxies" / name / f"{name}.dso").read_bytes()
        for name in ("euser", "libc", "libpthread", "drtaeabi")
    ]
    image = convert_imported_executable(
        (build / "abseil_allocator_probe.elf").read_bytes(),
        proxies,
        0xE0000813,
    )

    golden = root / ".symbian/instances/delight-import-01"
    pinned = {
        golden / "data/roms/rm-807/SYM.ROM": ROM_808,
        golden / "data/drives/z/rm-807/sys/bin/euser.dll": EUSER_808,
    }
    assert {path: _digest(path) for path in pinned} == pinned
    instance = guest_root / "instance"
    shutil.copytree(golden, instance)
    target = instance / "data/drives/rm-807/c/sys/bin/runtime_probe.exe"
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(image)
    (instance / "config.yml").write_text(
        "data-storage: data\ncpu: dynarmic\ndevice: 0\nlanguage: 1\n"
        "enable-gdb-stub: false\nlog-svc: true\n"
    )
    control = Control(guest_root / "control.sock")
    environment.update(
        EKA2L1_DATA_ROOT=str(instance),
        EKA2L1_EXPERIMENTAL_SVC_PROFILE="rm807-113.010.1508",
        EKA2L1_RESEARCH_CONTROL_SOCKET=str(control.endpoint),
        **background_environment(),
    )
    executable = root / "build/eka2l1/bin/EKA2L1.app/Contents/MacOS/EKA2L1"
    with (tmp_path / "frontend.log").open("w") as log:
        process = subprocess.Popen(
            [
                executable_for_session(executable, guest_root),
                "--device",
                "RM-807",
                "--run",
                "C:\\sys\\bin\\runtime_probe.exe",
            ],
            env=environment,
            stdout=log,
            stderr=subprocess.STDOUT,
        )
        try:
            assert process.wait(timeout=30) == 0
            exits = control.exit_report()["process_exits"]
            assert len(exits) == 1
            assert exits[0]["reason"] == (-286 if changed else 0)
            assert exits[0]["type"] == 0
        finally:
            _stop(process)
    assert {path: _digest(path) for path in pinned} == pinned
