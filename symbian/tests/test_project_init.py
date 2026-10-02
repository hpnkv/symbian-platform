"""Wizard preferences, independent app builds and actual owned GUI launches."""

import json
import os
import re
import shlex
import shutil
import subprocess
import sys
import time
import tomllib
import xml.etree.ElementTree as ET
from pathlib import Path

import pytest
from PIL import Image

from symbian.emulator import Control
from symbian.emulator.launch import session
from symbian.project.generate import Preferences, generate, wizard
from symbian.project.sdk import AppSdk, activate_sdk, discover_sdk
from symbian.status import Code, StatusError
from symbian.tests.test_guest_gui import _ready

SDK = os.environ.get("SYMBIAN_APP_SDK")
live = pytest.mark.skipif(not SDK, reason="Set SYMBIAN_APP_SDK to prepared SDK")


def _fake_sdk(tmp_path):
    prefix = tmp_path / "SDK with spaces"
    for name in (
        "compiler",
        "linker",
        "python",
        "emulator",
        "lib/libsymbian_guest_runtime.a",
        "lib/armv6/libsymbian_guest_runtime.a",
        "lib/armv5t/libsymbian_guest_runtime.a",
        "cmake/SymbianApp.cmake",
    ):
        path = prefix / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.touch()
    (prefix / "golden").mkdir()
    sdk = AppSdk(
        prefix=prefix,
        architectures=("armv5t", "armv6"),
        compiler=prefix / "compiler",
        linker=prefix / "linker",
        python=Path(sys.executable),
        emulator=prefix / "emulator",
        golden=prefix / "golden",
    )
    (prefix / "sdk.json").write_text(sdk.model_dump_json())
    return sdk


def test_wizard_choices_and_nonempty_project_preserved(tmp_path, monkeypatch):
    sdk = _fake_sdk(tmp_path)
    monkeypatch.setattr(sys.stdin, "isatty", lambda: True)
    answers = iter(["time_notes", "n", "0xe0000830", "armv6"])
    monkeypatch.setattr("builtins.input", lambda _: next(answers))
    project = tmp_path / "new project"
    result = wizard(project, sdk.prefix / "sdk.json")
    assert result["name"] == "time_notes"
    assert result["uid3"] == 0xE0000830
    assert not (project / ".idea").exists()
    assert "#include <string>" in (project / "model.h").read_text()
    assert 'extern "C"' not in (project / "model.cc").read_text()
    assert "void*" not in (project / "model.cc").read_text()
    assert "#include <w32std.h>" in (project / "app.cc").read_text()
    assert (project / ".clang-format").read_bytes() == (
        Path(__file__).resolve().parents[2] / ".clang-format"
    ).read_bytes()
    assert 'libpthread" ON)' in (project / "CMakeLists.txt").read_text()
    assert tomllib.loads((project / "symbian.toml").read_text())[
        "application"
    ] == {
        "caption": "time_notes",
        "short_caption": "time_notes",
        "icon": "icon.svg",
    }
    assert "<svg" in (project / "icon.svg").read_text()
    assert "project-relative SVG icon" in (project / "README.md").read_text()
    assert (
        '"Use guest Abseil Status/StatusOr in application logic" ON)'
        in (project / "CMakeLists.txt").read_text()
    )
    before = {p.name: p.read_bytes() for p in project.iterdir() if p.is_file()}
    with pytest.raises(StatusError) as failure:
        generate(project, Preferences(name="other", uid3=0xE0000831), sdk)
    assert failure.value.code == Code.ALREADY_EXISTS
    assert before == {
        p.name: p.read_bytes() for p in project.iterdir() if p.is_file()
    }


def test_invalid_identity_and_missing_sdk_create_no_project(tmp_path):
    sdk = _fake_sdk(tmp_path)
    project = tmp_path / "new"
    with pytest.raises(StatusError) as failure:
        wizard(
            project,
            sdk.prefix / "sdk.json",
            name="bad-name",
            non_interactive=True,
        )
    assert failure.value.code == Code.INVALID_ARGUMENT
    assert not project.exists()
    (sdk.prefix / "lib/libsymbian_guest_runtime.a").unlink()
    with pytest.raises(StatusError) as failure:
        wizard(project, sdk.prefix / "sdk.json", non_interactive=True)
    assert failure.value.code == Code.NOT_FOUND
    assert not project.exists()


def test_cli_portable_runtime_choice_is_saved(tmp_path):
    sdk = _fake_sdk(tmp_path)
    project = tmp_path / "portable"
    result = subprocess.run(
        [
            sys.executable,
            "-m",
            "symbian.cli",
            "init",
            str(project),
            "--sdk",
            str(sdk.prefix / "sdk.json"),
            "--name",
            "portable",
            "--non-interactive",
            "--portable-runtime",
            "--no-build",
        ],
        capture_output=True,
        text=True,
        check=False,
    )
    assert result.returncode == 0, result.stdout + result.stderr
    assert 'libpthread" OFF)' in (project / "CMakeLists.txt").read_text()
    assert not json.loads((project / "symbian-project.json").read_text())[
        "preferences"
    ]["timer_tasks"]


def test_sdk_discovery_from_unrelated_directory(tmp_path, monkeypatch):
    sdk = _fake_sdk(tmp_path)
    monkeypatch.setenv("XDG_CONFIG_HOME", str(tmp_path / "config"))
    monkeypatch.delenv("SYMBIAN_SDK_MANIFEST", raising=False)
    activate_sdk(sdk)
    unrelated = tmp_path / "unrelated"
    unrelated.mkdir()
    monkeypatch.chdir(unrelated)
    assert discover_sdk() == sdk.prefix / "sdk.json"
    monkeypatch.setenv("SYMBIAN_SDK_MANIFEST", "/explicit/env.json")
    assert discover_sdk() == Path("/explicit/env.json")
    assert discover_sdk(Path("/manual/sdk.json")) == Path("/manual/sdk.json")


@live
def test_cli_initial_build_sdk_copy_and_moved_project(tmp_path):
    original = AppSdk.load(Path(SDK))
    environment = dict(os.environ)
    environment["XDG_CONFIG_HOME"] = str(tmp_path / "config")
    environment["SYMBIAN_SDK_MANIFEST"] = str(original.prefix / "sdk.json")
    prefix = tmp_path / "visible SDK"

    def cli(*arguments):
        return subprocess.run(
            [
                sys.executable,
                "-m",
                "symbian.cli",
                "--output-format=json",
                *map(str, arguments),
            ],
            env=environment,
            check=True,
            capture_output=True,
            text=True,
        )

    cli("sdk", "install", prefix)
    copied = AppSdk.load(prefix / "sdk.json")
    assert copied.c_compiler == prefix / "bin/clang"
    assert copied.c_compiler.is_file()
    assert (prefix / "include/symbian/concurrency/future.h").is_file()
    assert (prefix / "include/symbian/concurrency/channel.h").is_file()
    assert (prefix / "include/thread/channel.h").is_file()
    assert (prefix / "include/thread/boost_primitives.h").is_file()
    assert (prefix / "include/symbian/concurrency/mutex.h").is_file()
    assert (prefix / "include/symbian/concurrency/native_timer.h").is_file()
    assert (prefix / "include/symbian/concurrency/timer_pump.h").is_file()
    assert (prefix / "include/config/stdarg_e.h").is_file()
    assert not (prefix / "include/a11").exists()
    template = prefix / "lib/python/symbian/project/templates/model.cc"
    template.write_text("// Selected SDK template.\n" + template.read_text())
    project = tmp_path / "original app"
    initialized = json.loads(
        cli(
            "init",
            project,
            "--sdk",
            prefix / "sdk.json",
            "--name",
            "relocated",
            "--non-interactive",
        ).stdout
    )
    assert initialized["result"]["initial_build"] is True
    assert not (project / "sdk.json").exists()
    assert (project / ".clang-format").read_bytes() == (
        prefix / "lib/python/symbian/project/templates/.clang-format"
    ).read_bytes()
    assert 'libpthread" ON)' in (project / "CMakeLists.txt").read_text()
    assert (
        (project / "model.cc")
        .read_text()
        .startswith("// Selected SDK template.")
    )
    assert (project / ".symbian/build/relocated.elf").is_file()
    moved = tmp_path / "moved app"
    project.rename(moved)
    shutil.rmtree(moved / ".symbian/build")
    (moved / "sdk-location.json").write_text(
        json.dumps({"sdk": os.path.relpath(prefix, moved)})
    )
    cli("app", "build", "--project", moved)
    database = json.loads((moved / "compile_commands.json").read_text())
    command = next(
        row["command"] for row in database if row["file"].endswith("/model.cc")
    )
    assert str(prefix / "bin/clang++") in command
    assert str(prefix / "include/c++") in command
    assert str(prefix / "include/platform") in command
    imported = subprocess.run(
        [
            str(prefix / "bin/python"),
            "-c",
            "import symbian.cli, symbian._native; "
            "print(symbian.cli.__file__); print(symbian._native.__file__)",
        ],
        check=True,
        capture_output=True,
        text=True,
    ).stdout.splitlines()
    assert all(
        Path(path).is_relative_to(prefix / "lib/python") for path in imported
    )
    for selected in (original.prefix, prefix):
        (moved / "sdk-location.json").write_text(
            json.dumps({"sdk": os.path.relpath(selected, moved)})
        )
        subprocess.run(
            [
                "cmake",
                "--build",
                str(moved / ".symbian/build/cmake"),
                "--target",
                "relocated",
            ],
            check=True,
            capture_output=True,
            text=True,
        )
        database = json.loads((moved / "compile_commands.json").read_text())
        command = next(
            row["command"]
            for row in database
            if row["file"].endswith("/model.cc")
        )
        assert str(selected / "bin/clang++") in command


@pytest.fixture
def app(tmp_path):
    sdk = AppSdk.load(Path(SDK))
    project = tmp_path / "outside project with spaces"
    generate(project, Preferences(name="hello_time", uid3=0xE0000830), sdk)
    # Generation is independent of cwd and carries every required SDK location.
    run = ET.parse(project / ".idea/runConfigurations/App_Run.xml").getroot()[0]
    assert run.get("RUN_PATH") == "$PROJECT_DIR$/sdk-run"
    assert run.get("PROGRAM_PARAMS") == ""
    workspace = ET.parse(project / ".idea/workspace.xml").getroot()
    assert (
        workspace.find(
            "./component[@name='CMakeSettings']/configurations/"
            "configuration[@PROFILE_NAME='Symbian App']"
        ).get("ENABLED")
        == "true"
    )
    assert str(sdk.prefix) not in (project / "symbian.toml").read_text()
    assert str(sdk.prefix) not in (project / "CMakePresets.json").read_text()
    assert run.get("TARGET_NAME") == "hello_time"
    assert run.get("CONFIG_NAME") == "Symbian App"
    assert (
        ET.parse(project / ".idea/misc.xml")
        .getroot()
        .find("./component[@name='CMakeWorkspace']")
        .get("PROJECT_DIR")
        == "$PROJECT_DIR$"
    )
    assert (
        ET.parse(project / ".idea/hello_time.CMake.iml").getroot().get("type")
        == "CPP_MODULE"
    )
    selected = workspace.find(
        "./component[@name='SelectedDebugProfileService']"
    )
    if sdk.gdb:
        assert json.loads(selected.text)["profileIdByStamp"][0]["first"] == {
            "##RUN_CONFIGURATION##": "Remote Debug.App Debug"
        }
    return project


def _capture(control, label, expected_lines, output):
    deadline = time.monotonic() + 15
    attempt = 0
    while True:
        attempt += 1
        result = _ready(
            lambda attempt=attempt: control.capture(f"{label}-{attempt}")
        )
        with Image.open(result["path"]) as image:
            image.save(output / f"last-{label}.png")
            rgb = image.convert("RGB")

            # The real font must put white ink in the greeting and in each
            # requested log row, leaving all unused log rows blank.
            def ink(top, bottom, rgb=rgb):
                return sum(
                    min(pixel) > 150
                    for pixel in rgb.crop(
                        (25, top, 650, bottom)
                    ).get_flattened_data()
                )

            counts = [
                ink(2 * (112 + row * 28), 2 * (140 + row * 28))
                for row in range(12)
            ]
            if ink(30, 90) > 200 and all(
                (count > 100) == (row < expected_lines)
                for row, count in enumerate(counts)
            ):
                image.save(output / f"{label}.png")
                return
        assert time.monotonic() < deadline, counts
        time.sleep(0.05)


@live
@pytest.mark.parametrize("backend", ["dynarmic", "dyncom"])
def test_generated_project_native_text_clock_clear_exit(app, tmp_path, backend):
    with session(app, project=app, backend=backend) as active:
        control = Control(active.endpoint)
        _capture(control, "hello", 0, tmp_path)
        _ready(lambda: control.pointer(100, 200, "press"))
        _ready(lambda: control.pointer(100, 200, "release"))
        _capture(control, "one-time", 1, tmp_path)
        _ready(lambda: control.pointer(100, 200, "press"))
        _ready(lambda: control.pointer(100, 200, "release"))
        _capture(control, "two-times", 2, tmp_path)
        _ready(lambda: control.pointer(40, 610, "press"))
        _ready(lambda: control.pointer(40, 610, "release"))
        _capture(control, "cleared", 0, tmp_path)
        _ready(lambda: control.pointer(295, 610, "press"))
        assert active.process.wait(timeout=15) == 0
    report = Control(active.directory / "control.sock").exit_report()
    assert report["process_exits"][0]["uid"] == 0xE0000830
    assert report["process_exits"][0]["reason"] == 0
    assert report["process_exits"][0]["type"] == 0
    evidence = json.loads((app / ".symbian/build/report.json").read_text())
    assert evidence["reproducible"] is True
    assert any(
        path.endswith("libsymbian_guest_runtime.a")
        for path in evidence["inputs"]
    )


@live
@pytest.mark.parametrize("backend", ["dynarmic", "dyncom"])
def test_generated_timer_tasks_share_window_wait(app, tmp_path, backend):
    cmake = app / "CMakeLists.txt"
    source = cmake.read_text()
    assert (
        '"Stackless timer Tasks; requires firmware libpthread" OFF)' in source
    )
    cmake.write_text(
        source.replace(
            'libpthread" OFF)',
            'libpthread" ON)',
        )
    )
    with session(app, project=app, backend=backend) as active:
        control = Control(active.endpoint)
        _capture(control, "async-hello", 0, tmp_path)
        _ready(lambda: control.pointer(100, 200, "press"))
        _ready(lambda: control.pointer(100, 200, "release"))
        _capture(control, "async-immediate", 1, tmp_path)
        _capture(control, "async-delayed", 2, tmp_path)
        _ready(lambda: control.pointer(100, 200, "press"))
        _ready(lambda: control.pointer(100, 200, "release"))
        _ready(lambda: control.pointer(40, 610, "press"))
        _ready(lambda: control.pointer(40, 610, "release"))
        _capture(control, "async-cleared", 0, tmp_path)
        time.sleep(1.8)
        _capture(control, "async-still-cleared", 0, tmp_path)
        _ready(lambda: control.pointer(295, 610, "press"))
        assert active.process.wait(timeout=15) == 0
    exits = Control(active.directory / "control.sock").exit_report()[
        "process_exits"
    ]
    assert len(exits) == 1
    assert exits[0]["reason"] == 0


@live
@pytest.mark.parametrize(
    ("backend", "architecture"),
    [
        ("dynarmic", "armv6"),
        ("dyncom", "armv6"),
        ("dynarmic", "armv5t"),
        ("dyncom", "armv5t"),
    ],
)
def test_init_default_runs_stackless_tasks(tmp_path, backend, architecture):
    """The CLI's ordinary starter exercises a timer Future and cancellation."""
    project = tmp_path / "new task app"
    result = subprocess.run(
        [
            sys.executable,
            "-m",
            "symbian.cli",
            "--output-format=json",
            "init",
            str(project),
            "--sdk",
            SDK,
            "--name",
            "task_app",
            "--architecture",
            architecture,
            "--ide",
            "none",
            "--non-interactive",
        ],
        capture_output=True,
        text=True,
        timeout=120,
        check=False,
    )
    assert result.returncode == 0, result.stdout + result.stderr
    assert json.loads(result.stdout)["result"]["initial_build"] is True
    cmake = (project / "CMakeLists.txt").read_text()
    assert 'libpthread" ON)' in cmake
    assert (
        '"Use guest Abseil Status/StatusOr in application logic" ON)' in cmake
    )
    with session(project, project=project, backend=backend) as active:
        control = Control(active.endpoint)
        _capture(control, "init-hello", 0, tmp_path)
        _ready(lambda: control.pointer(100, 200, "press"))
        _ready(lambda: control.pointer(100, 200, "release"))
        _capture(control, "init-immediate", 1, tmp_path)
        _capture(control, "init-future", 2, tmp_path)
        _ready(lambda: control.pointer(100, 200, "press"))
        _ready(lambda: control.pointer(100, 200, "release"))
        _ready(lambda: control.pointer(40, 610, "press"))
        _ready(lambda: control.pointer(40, 610, "release"))
        time.sleep(1.8)
        _capture(control, "init-cancelled", 0, tmp_path)
        _ready(lambda: control.pointer(100, 200, "press"))
        _ready(lambda: control.pointer(100, 200, "release"))
        _ready(lambda: control.pointer(295, 610, "press"))
        assert active.process.wait(timeout=15) == 0
    exits = Control(active.directory / "control.sock").exit_report()[
        "process_exits"
    ]
    assert len(exits) == 1 and exits[0]["reason"] == 0


@live
@pytest.mark.parametrize("backend", ["dynarmic", "dyncom"])
def test_generated_abseil_status_and_timer_tasks(app, tmp_path, backend):
    """Run Status/StatusOr model logic with the same native timer loop."""
    cmake = app / "CMakeLists.txt"
    source = cmake.read_text()
    cmake.write_text(
        source.replace(
            '"Use guest Abseil Status/StatusOr in application logic" OFF)',
            '"Use guest Abseil Status/StatusOr in application logic" ON)',
        ).replace(
            '"Stackless timer Tasks; requires firmware libpthread" OFF)',
            '"Stackless timer Tasks; requires firmware libpthread" ON)',
        )
    )
    with session(app, project=app, backend=backend) as active:
        control = Control(active.endpoint)
        _capture(control, "status-hello", 0, tmp_path)
        _ready(lambda: control.pointer(100, 200, "press"))
        _ready(lambda: control.pointer(100, 200, "release"))
        _capture(control, "status-now", 1, tmp_path)
        _capture(control, "status-timer", 2, tmp_path)
        _ready(lambda: control.pointer(295, 610, "press"))
        assert active.process.wait(timeout=15) == 0
    exits = Control(active.directory / "control.sock").exit_report()[
        "process_exits"
    ]
    assert len(exits) == 1
    assert exits[0]["reason"] == 0


@live
def test_generated_abseil_error_reaches_native_exit(app, tmp_path):
    """A model Status failure must leave through the owned GUI cleanup path."""
    cmake = app / "CMakeLists.txt"
    cmake.write_text(
        cmake.read_text().replace(
            '"Use guest Abseil Status/StatusOr in application logic" OFF)',
            '"Use guest Abseil Status/StatusOr in application logic" ON)',
        )
    )
    model = app / "model.cc"
    source = model.read_text()
    assert "return absl::OkStatus();" in source
    model.write_text(
        source.replace(
            "return absl::OkStatus();",
            'return absl::InvalidArgumentError("changed model result");',
        )
    )
    with session(app, project=app, backend="dynarmic") as active:
        control = Control(active.endpoint)
        _capture(control, "status-error-ready", 0, tmp_path)
        _ready(lambda: control.pointer(100, 200, "press"))
        _ready(lambda: control.pointer(100, 200, "release"))
        assert active.process.wait(timeout=15) == 0
    exits = Control(active.directory / "control.sock").exit_report()[
        "process_exits"
    ]
    assert len(exits) == 1
    assert exits[0]["reason"] == -6


@live
@pytest.mark.parametrize("backend", ["dynarmic", "dyncom"])
def test_generated_project_model_allocation_failure_closes_resources(
    app, backend
):
    bridge = app / "app_bridge.cc"
    text = bridge.read_text()
    assert "return new (std::nothrow) AppModel;" in text
    bridge.write_text(
        text.replace(
            "return new (std::nothrow) AppModel;",
            "return ::operator new(4 * 1024 * 1024, std::nothrow);",
        )
    )
    # Fast guest exit can precede the live endpoint readiness check. In either
    # case the owned launcher must retain the actual native exit and reap it.
    try:
        with session(app, project=app, backend=backend) as active:
            assert active.process.wait(timeout=15) == 0
    except StatusError as error:
        assert error.code == Code.FAILED_PRECONDITION
        assert "Emulator exited 0;" in error.message
    runs = list((app / ".symbian/runs").glob("run-*"))
    assert len(runs) == 1
    manifest = json.loads((runs[0] / "launch.json").read_text())
    assert manifest["frontend_exit"] == 0
    assert manifest["inputs_unchanged"]
    exits = Control(runs[0] / "control.sock").exit_report()["process_exits"]
    assert len(exits) == 1
    assert exits[0]["uid"] == 0xE0000830
    assert exits[0]["type"] == 0
    assert exits[0]["reason"] == -4


@live
def test_generated_project_debugger_breakpoint_and_clock_values(app, tmp_path):
    sdk = AppSdk.load(Path(SDK))
    if sdk.gdb is None:
        pytest.skip("ARM GDB required")
    with session(app, project=app, debug=True) as active:
        script = tmp_path / "commands.gdb"
        script.write_text(
            f"target remote 127.0.0.1:{active.port}\n"
            "break AppLogTime\nbreak AppModel::LogTime\ncontinue\n"
            'printf "CLOCK=%d-%02d-%02d\\n", now.year, now.month, now.day\n'
            "bt\ncontinue\nbt\nquit\n"
        )
        with (tmp_path / "gdb.log").open("w") as log:
            gdb = subprocess.Popen(
                [
                    str(sdk.gdb),
                    "--batch",
                    "-x",
                    str(active.gdb_hook()),
                    "-x",
                    str(script),
                ],
                stdout=log,
                stderr=subprocess.STDOUT,
            )
        try:
            control = Control(active.endpoint)
            _capture(control, "debug-hello", 0, tmp_path)
            _ready(lambda: control.pointer(100, 200, "press"))
            assert gdb.wait(timeout=20) == 0
            text = (tmp_path / "gdb.log").read_text()
            assert "Breakpoint 1, AppLogTime" in text, text
            assert re.search(r"CLOCK=20\d\d-\d\d-\d\d", text), text
            assert "Breakpoint 2, AppModel::LogTime" in text, text
            assert "model.cc" in text, text
            assert (active.directory / "gdb-mapping.json").is_file()
        finally:
            if gdb.poll() is None:
                gdb.kill()
                gdb.wait(timeout=5)


@live
def test_saved_generated_run_configuration_executes_and_reaps_emulator(
    app, tmp_path
):
    """Runs the actual executable/arguments saved for CLion, from the app."""
    run = ET.parse(app / ".idea/runConfigurations/App_Run.xml").getroot()[0]
    log_path = tmp_path / "run.log"
    with log_path.open("w") as log:
        process = subprocess.Popen(
            [
                run.get("RUN_PATH").replace("$PROJECT_DIR$", str(app)),
                *shlex.split(run.get("PROGRAM_PARAMS")),
            ],
            cwd=app,
            stdout=log,
            stderr=subprocess.STDOUT,
        )
    try:
        deadline = time.monotonic() + 20
        while True:
            found = re.search(
                r"Emulator session: ([^\n]+)", log_path.read_text()
            )
            if found:
                directory = Path(found[1])
                manifest = json.loads((directory / "launch.json").read_text())
                control = Control(Path(manifest["endpoint"]))
                break
            assert process.poll() is None, log_path.read_text()
            assert time.monotonic() < deadline
            time.sleep(0.05)
        _capture(control, "saved-run", 0, tmp_path)
        _ready(lambda: control.pointer(295, 610, "press"))
        assert process.wait(timeout=15) == 0
        manifest = json.loads((directory / "launch.json").read_text())
        assert manifest["inputs_unchanged"]
        assert manifest["frontend_exit"] == 0
        assert not control.endpoint.parent.exists()
    finally:
        from symbian.emulator.launch import _stop

        _stop(process)
