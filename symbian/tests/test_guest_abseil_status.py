"""Execute the A11-pinned Abseil Status/StatusOr on both guest targets."""

import os
import shutil
import subprocess
import tempfile
import time
from pathlib import Path

import pytest
from PIL import Image, ImageChops

from symbian.e32.images import convert_imported_executable, inspect_image
from symbian.emulator import Control
from symbian.emulator.background import (
    background_environment,
    executable_for_session,
)
from symbian.emulator.firmware import (
    EUSER_808,
    ROM_808,
    locate,
    validate_manifest,
)
from symbian.emulator.launch import _digest, _stop
from symbian.paths import asset_directory
from symbian.project.sdk import AppSdk
from symbian.tests.test_guest_gui import _ready

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
    project = root / "probes/abseil_status_probe"
    proxies = [
        (sdk / "proxies" / name / f"{name}.dso").read_bytes()
        for name in ("euser", "libc", "libpthread", "libm", "drtaeabi")
    ]
    environment = dict(os.environ)
    environment["SYMBIAN_SDK_PREFIX"] = str(sdk)
    if not USE_SDK:
        environment["SYMBIAN_ABSEIL_SOURCE"] = str(source)
    cached = {}

    def build(
        architecture, changed, fatal=False, legacy=False, foreground=False
    ):
        key = (architecture, changed, fatal, legacy, foreground)
        if key not in cached:
            mode = (
                "foreground"
                if foreground
                else "fatal" if fatal else "changed" if changed else "normal"
            )
            suffix = "-legacy" if legacy else ""
            directory = output / f"{architecture}-{mode}{suffix}"
            subprocess.run(
                [
                    "cmake",
                    "--preset",
                    "symbian-sdk" if USE_SDK else "symbian-pic",
                    "-B",
                    str(directory),
                    f"-DSYMBIAN_TARGET_ARCH={architecture}",
                    "-DSYMBIAN_RUNTIME_LEGACY_EKA2="
                    + ("ON" if legacy else "OFF"),
                    f"-DSYMBIAN_ABSEIL_USE_SDK={'ON' if USE_SDK else 'OFF'}",
                    "-DSYMBIAN_ABSEIL_CHANGED_STATUS="
                    + ("ON" if changed else "OFF"),
                    "-DSYMBIAN_ABSEIL_FAILED_CHECK="
                    + ("ON" if fatal else "OFF"),
                    "-DSYMBIAN_ABSEIL_FOREGROUND_CHECK="
                    + ("ON" if foreground else "OFF"),
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
                    (
                        "abseil_status_probe_e32"
                        if USE_SDK
                        else "abseil_status_probe"
                    ),
                    "-j",
                    "8",
                ],
                cwd=project,
                env=environment,
                check=True,
                capture_output=True,
                text=True,
            )
            if USE_SDK:
                cached[key] = (
                    directory / "e32/abseil_status_probe.exe"
                ).read_bytes()
            else:
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
    source = locate(asset_directory("data") / "firmware", "nokia808")
    validate_manifest(source)
    golden = source / "instance"
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


@pytest.mark.skipif(
    not USE_SDK, reason="Requires installed SDK failure handler"
)
@pytest.mark.parametrize("reference", ["nokia808", "c7", "e6", "6120", "e71"])
@pytest.mark.parametrize("mode", ["normal", "fatal", "foreground"])
def test_abseil_logging_on_named_roms(images, tmp_path, reference, mode):
    """Run logging and CHECK reporting on each named EKA2 ROM."""
    root = Path(WORKSPACE).resolve()
    fatal = mode == "fatal"
    foreground = mode == "foreground"
    architecture = "armv5t" if reference in ("6120", "e71") else "armv6"
    image = images(
        architecture, False, fatal, reference in ("6120", "e71"), foreground
    )
    source = locate(asset_directory("data") / "firmware", reference)
    manifest = validate_manifest(source)
    assert manifest.device.kernel == "eka2"
    with tempfile.TemporaryDirectory(prefix="absl-rom-", dir="/tmp") as name:
        session = Path(name)
        instance = session / "instance"
        shutil.copytree(source / "instance", instance)
        target = (
            instance / manifest.device.c_drive / "sys/bin/runtime_probe.exe"
        )
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(image)
        (instance / "config.yml").write_text(
            "data-storage: data\ncpu: dynarmic\ndevice: 0\nlanguage: 1\n"
            "enable-gdb-stub: false\nlog-svc: true\n"
        )
        control = Control(session / "control.sock")
        executable = root / "build/eka2l1/bin/EKA2L1.app/Contents/MacOS/EKA2L1"
        environment = dict(os.environ)
        environment.update(
            EKA2L1_DATA_ROOT=str(instance),
            EKA2L1_RESEARCH_CONTROL_SOCKET=str(control.endpoint),
            **background_environment(),
        )
        if reference == "nokia808":
            environment["EKA2L1_EXPERIMENTAL_SVC_PROFILE"] = (
                "rm807-113.010.1508"
            )
        else:
            environment.pop("EKA2L1_EXPERIMENTAL_SVC_PROFILE", None)
        with (tmp_path / "fatal-frontend.log").open("w") as log:
            process = subprocess.Popen(
                [
                    executable_for_session(executable, session),
                    "--device",
                    manifest.device.firmware_code,
                    "--run",
                    "C:\\sys\\bin\\runtime_probe.exe",
                ],
                env=environment,
                stdout=log,
                stderr=subprocess.STDOUT,
            )
            try:
                if foreground:
                    _exercise_failure_view(control, process, tmp_path)
                assert process.wait(timeout=30) == 0
                exits = control.exit_report()["process_exits"]
                assert len(exits) == 1
                assert exits[0]["reason"] == (0 if mode == "normal" else 1)
                if mode != "normal":
                    report = (
                        instance
                        / manifest.device.c_drive
                        / "private/e0000814/failure.txt"
                    )
                    text = report.read_text()
                    assert f"{mode} guest reporting probe" in text
                    if foreground:
                        assert "foreground recent log 39" in text
            finally:
                report = (
                    instance
                    / manifest.device.c_drive
                    / "private/e0000814/failure.txt"
                )
                if report.is_file():
                    shutil.copyfile(report, tmp_path / "failure.txt")
                _stop(process)
    validate_manifest(source)


def _exercise_failure_view(control, process, output):
    """Verify text, scrolling, clipboard feedback and the Exit button."""

    def capture(name):
        assert process.poll() is None
        result = _ready(lambda: control.capture(name))
        with Image.open(result["path"]) as image:
            frame = image.convert("RGB").resize(
                (result["logical_width"], result["logical_height"])
            )
        frame.save(output / "failure-latest.png")
        return frame

    def ink(image):
        return image.convert("L").point(lambda value: 255 if value > 160 else 0)

    deadline = time.monotonic() + 20
    attempt = 0
    while True:
        first = capture(f"failure-{attempt}")
        attempt += 1
        width, height = first.size
        body = (24, 80, width - 30, height - 88)
        left = (24, height - 72, width // 2 - 8, height - 10)
        right = (width // 2 + 8, height - 72, width - 16, height - 10)
        if all(
            sum(ink(first.crop(box)).histogram()[1:]) > 30
            for box in (body, left, right)
        ):
            break
        assert time.monotonic() < deadline, "Failure text or labels missing"
        time.sleep(0.05)
    first.save(output / "failure.png")
    # Sample during sustained scrolling, including between move events. Static
    # chrome must survive every sampled frame, not merely the settled result:
    # separate background/text submissions used to expose blank labels here.
    chrome = (0, height - 76, width, height)
    expected_chrome = first.crop(chrome)
    _ready(lambda: control.pointer(width // 2, height // 2, "press"))
    for step in range(24):
        y = height // 2 - (step + 1) * 2
        _ready(lambda y=y: control.pointer(width // 2, y, "move"))
        for sample in range(2):
            frame = capture(f"scroll-stability-{step}-{sample}")
            assert (
                ImageChops.difference(
                    expected_chrome, frame.crop(chrome)
                ).getbbox()
                is None
            ), "Failure-view buttons flickered during scroll"
            assert (
                sum(ink(frame.crop(body)).histogram()[1:]) > 30
            ), "Failure-view text disappeared during scroll"
    _ready(lambda: control.pointer(width // 2, y, "release"))
    # Return to the beginning for the independent three-pixel movement check.
    _ready(lambda: control.pointer(width // 2, height // 2 - 48, "press"))
    _ready(lambda: control.pointer(width // 2, height // 2, "move"))
    _ready(lambda: control.pointer(width // 2, height // 2, "release"))
    deadline = time.monotonic() + 5
    while True:
        restored = capture(f"scroll-restored-{attempt}")
        attempt += 1
        if ImageChops.difference(first, restored).getbbox() is None:
            break
        assert (
            time.monotonic() < deadline
        ), "Failure report did not return to top"
        time.sleep(0.01)
    # A movement smaller than any font row must still move the text. This
    # catches whole-line quantization independently of a large swipe.
    _ready(lambda: control.pointer(width // 2, height // 2, "press"))
    _ready(lambda: control.pointer(width // 2, height // 2 - 3, "move"))
    _ready(lambda: control.pointer(width // 2, height // 2 - 3, "release"))
    deadline = time.monotonic() + 5
    while True:
        shifted = capture(f"pixel-scroll-{attempt}")
        attempt += 1
        difference = ImageChops.difference(first.crop(body), shifted.crop(body))
        if sum(difference.convert("L").histogram()[20:]) > 100:
            break
        assert (
            time.monotonic() < deadline
        ), "Sub-row scrolling did not move text"
        time.sleep(0.05)
    shifted.save(output / "failure-pixel-scrolled.png")
    # Measure translation away from the clipping edges. Some native fonts
    # dither RGB565 antialiasing by pixel position, so compare alignment errors
    # rather than requiring identical glyph intensities after an odd-pixel move.
    alignment_errors = []
    for distance in range(7):
        difference = ImageChops.difference(
            first.crop(
                (24, 100 + distance, width - 40, height - 104 + distance)
            ),
            shifted.crop((24, 100, width - 40, height - 104)),
        )
        alignment_errors.append(
            sum(
                intensity * count
                for intensity, count in enumerate(
                    difference.convert("L").histogram()
                )
            )
        )
    assert (
        min(range(7), key=alignment_errors.__getitem__) == 3
    ), "Drag distance and content movement are not 1:1"

    _ready(lambda: control.pointer(width // 2, height // 2, "press"))
    _ready(lambda: control.pointer(width // 2, 85, "move"))
    _ready(lambda: control.pointer(width // 2, 85, "release"))
    deadline = time.monotonic() + 5
    while True:
        scrolled = capture(f"scroll-{attempt}")
        attempt += 1
        difference = ImageChops.difference(
            first.crop(body), scrolled.crop(body)
        )
        if sum(difference.convert("L").histogram()[20:]) > 100:
            break
        assert time.monotonic() < deadline, "Failure report did not scroll"
        time.sleep(0.05)
    scrolled.save(output / "failure-scrolled.png")
    initial_label = ink(first.crop(right)).getbbox()
    _ready(lambda: control.pointer(width * 3 // 4, height - 40, "press"))
    _ready(lambda: control.pointer(width * 3 // 4, height - 40, "release"))
    deadline = time.monotonic() + 5
    while True:
        copied = capture(f"copy-{attempt}")
        attempt += 1
        label = ink(copied.crop(right)).getbbox()
        # COPIED is narrower than COPY LOGS; COPY FAILED is wider.
        if label and label[2] - label[0] < initial_label[2] - initial_label[0]:
            break
        assert time.monotonic() < deadline, "Clipboard copy did not succeed"
        time.sleep(0.05)
    copied.save(output / "failure-copied.png")
    _ready(lambda: control.pointer(width // 4, height - 40, "press"))
    _ready(lambda: control.pointer(width // 4, height - 40, "release"))


@pytest.mark.skipif(not USE_SDK, reason="Requires installed Abseil SDK mode")
def test_copied_project_uses_only_installed_abseil(tmp_path):
    """Builds a moved project without source-tree runtime or Abseil files."""
    root = Path(WORKSPACE).resolve()
    sdk = AppSdk.load(Path(SDK_MANIFEST)).prefix
    project = tmp_path / "project"
    shutil.copytree(root / "probes/abseil_status_probe", project)
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
        [
            "cmake",
            "--build",
            str(build),
            "--target",
            "abseil_status_probe_e32",
        ],
        cwd=project,
        env=environment,
        check=True,
        capture_output=True,
        text=True,
    )
    commands = (build / "compile_commands.json").read_text()
    assert str(root / "cpp/symbian/runtime") not in commands
    assert str(root / "research/upstream") not in commands
    image_path = build / "e32/abseil_status_probe.exe"
    assert inspect_image(image_path)["code_size"] > 0
