"""Execute the A11-pinned Abseil Status/StatusOr on both guest targets."""

import os
import shutil
import subprocess
import tempfile
from pathlib import Path

import pytest

from symbian.e32.images import convert_imported_executable, inspect_image
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
USE_SDK = os.environ.get("SYMBIAN_ABSEIL_USE_SDK") == "1"
pytestmark = pytest.mark.skipif(
    not WORKSPACE or not SDK_MANIFEST or (not USE_SDK and not ABSEIL_SOURCE),
    reason="Set runtime workspace, SDK manifest and Abseil source or SDK mode",
)


@pytest.fixture(scope="module")
def images(tmp_path_factory):
    """Build actual pinned Abseil once per architecture and control."""
    root = Path(WORKSPACE).resolve()
    sdk = AppSdk.load(Path(SDK_MANIFEST)).prefix
    if not USE_SDK:
        source = Path(ABSEIL_SOURCE).resolve()
        revision = subprocess.check_output(
            ["git", "-C", str(source), "rev-parse", "HEAD"], text=True
        ).strip()
        assert revision == "5650e9cf76d3be4318d5fa3af38ee483ddfd5e4a"
    output = tmp_path_factory.mktemp("abseil-status-builds")
    project = root / "examples/abseil_status_probe"
    proxies = [
        (sdk / "proxies" / name / f"{name}.dso").read_bytes()
        for name in ("euser", "libc", "libpthread", "libm", "drtaeabi")
    ]
    environment = dict(os.environ)
    environment["SYMBIAN_SDK_PREFIX"] = str(sdk)
    if not USE_SDK:
        environment["SYMBIAN_ABSEIL_SOURCE"] = str(source)
    cached = {}

    def build(architecture, changed):
        key = (architecture, changed)
        if key not in cached:
            directory = (
                output / f"{architecture}-{'changed' if changed else 'normal'}"
            )
            subprocess.run(
                [
                    "cmake",
                    "--preset",
                    "symbian-sdk" if USE_SDK else "symbian-pic",
                    "-B",
                    str(directory),
                    f"-DSYMBIAN_TARGET_ARCH={architecture}",
                    f"-DSYMBIAN_ABSEIL_USE_SDK={'ON' if USE_SDK else 'OFF'}",
                    "-DSYMBIAN_ABSEIL_CHANGED_STATUS="
                    + ("ON" if changed else "OFF"),
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
                    str(directory),
                    "--target",
                    "abseil_status_probe",
                    "-j",
                    "8",
                ],
                cwd=project,
                env=environment,
                check=True,
                capture_output=True,
                text=True,
            )
            cached[key] = convert_imported_executable(
                (directory / "abseil_status_probe.elf").read_bytes(),
                proxies,
                0xE0000814,
            )
        return cached[key]

    return build


@pytest.mark.parametrize("architecture", ["armv5t", "armv6"])
@pytest.mark.parametrize("backend", ["dyncom", "dynarmic"])
@pytest.mark.parametrize("changed", [False, True])
def test_guest_abseil_status_or(
    images, tmp_path, architecture, backend, changed
):
    """Check payload/copy/move and reject a deliberately changed result."""
    root = Path(WORKSPACE).resolve()
    image = images(architecture, changed)
    golden = root / ".symbian/instances/delight-import-01"
    pinned = {
        golden / "data/roms/rm-807/SYM.ROM": ROM_808,
        golden / "data/drives/z/rm-807/sys/bin/euser.dll": EUSER_808,
    }
    assert {path: _digest(path) for path in pinned} == pinned
    with tempfile.TemporaryDirectory(prefix="absl-status-", dir="/tmp") as name:
        session = Path(name)
        instance = session / "instance"
        shutil.copytree(golden, instance)
        target = instance / "data/drives/rm-807/c/sys/bin/runtime_probe.exe"
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(image)
        (instance / "config.yml").write_text(
            f"data-storage: data\ncpu: {backend}\ndevice: 0\nlanguage: 1\n"
            "enable-gdb-stub: false\nlog-svc: true\n"
        )
        control = Control(session / "control.sock")
        executable = root / "build/eka2l1/bin/EKA2L1.app/Contents/MacOS/EKA2L1"
        environment = dict(os.environ)
        environment.update(
            EKA2L1_DATA_ROOT=str(instance),
            EKA2L1_EXPERIMENTAL_SVC_PROFILE="rm807-113.010.1508",
            EKA2L1_RESEARCH_CONTROL_SOCKET=str(control.endpoint),
            **background_environment(),
        )
        with (tmp_path / "frontend.log").open("w") as log:
            process = subprocess.Popen(
                [
                    executable_for_session(executable, session),
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
                assert exits[0]["reason"] == (-204 if changed else 0)
                assert exits[0]["type"] == 0
            finally:
                _stop(process)
    assert {path: _digest(path) for path in pinned} == pinned


@pytest.mark.skipif(not USE_SDK, reason="Requires installed Abseil SDK mode")
def test_copied_project_uses_only_installed_abseil(tmp_path):
    """Builds a moved project without source-tree runtime or Abseil files."""
    root = Path(WORKSPACE).resolve()
    sdk = AppSdk.load(Path(SDK_MANIFEST)).prefix
    project = tmp_path / "project"
    shutil.copytree(root / "examples/abseil_status_probe", project)
    build = tmp_path / "build"
    environment = dict(os.environ)
    environment.pop("SYMBIAN_ABSEIL_SOURCE", None)
    environment["SYMBIAN_SDK_PREFIX"] = str(sdk)
    subprocess.run(
        [
            "cmake",
            "--preset",
            "symbian-sdk",
            "-B",
            str(build),
            "-DSYMBIAN_ABSEIL_USE_SDK=ON",
        ],
        cwd=project,
        env=environment,
        check=True,
        capture_output=True,
        text=True,
    )
    subprocess.run(
        ["cmake", "--build", str(build), "--target", "abseil_status_probe"],
        cwd=project,
        env=environment,
        check=True,
        capture_output=True,
        text=True,
    )
    commands = (build / "compile_commands.json").read_text()
    assert str(root / "cpp/symbian/runtime") not in commands
    assert str(root / "research/upstream") not in commands
    proxies = [
        (sdk / "proxies" / name / f"{name}.dso").read_bytes()
        for name in ("euser", "libc", "libpthread", "libm", "drtaeabi")
    ]
    image = convert_imported_executable(
        (build / "abseil_status_probe.elf").read_bytes(),
        proxies,
        0xE0000814,
    )
    image_path = tmp_path / "abseil-status.exe"
    image_path.write_bytes(image)
    assert inspect_image(image_path)["code_size"] > 0
