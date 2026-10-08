"""Portable saved IDE launchers and opt-in project generation."""

import json
import os
import subprocess
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

import pytest

from symbian.project.configuration import Preferences
from symbian.project.generate import generate
from symbian.project.ide import configure_workspace_ide
from symbian.tests.test_project_init import _fake_sdk


@pytest.mark.parametrize("ide", ["intellij", "none"])
def test_init_ide_choice_controls_both_run_and_debug_helpers(tmp_path, ide):
    sdk = _fake_sdk(tmp_path)
    assert sdk.gdb is None
    project = tmp_path / "app"
    generate(project, Preferences(name="app", uid3=0xE0000831, ide=ide), sdk)
    for name in ("sdk-run", "sdk-debug", "sdk.cmake"):
        assert (project / name).exists() == (ide == "intellij")
    for name in ("App_Run.run.xml", "App_Debug.run.xml"):
        assert (project / ".run" / name).exists() == (ide == "intellij")
    if ide == "intellij":
        debug = ET.parse(project / ".run/App_Debug.run.xml").getroot()[0]
        assert debug.get("remoteCommand") == "127.0.0.1:24690"
        assert debug.get("symbolFile") == "$PROJECT_DIR$/.symbian/build/app.elf"
        assert debug.find("debugger").text == "$PROJECT_DIR$/sdk-debug"
        root = ET.parse(project / ".idea/workspace.xml").getroot()
        profile = root.find(
            "./component[@name='CMakeSettings']/configurations/configuration"
        )
        assert "-DCMAKE_TOOLCHAIN_FILE=$PROJECT_DIR$/sdk.cmake" in profile.get(
            "GENERATION_OPTIONS"
        )


@pytest.mark.parametrize(
    "name,port", [("gl_app", 24701), ("qt_app_classic", 24702)]
)
def test_examples_share_portable_run_and_debug_settings(name, port):
    project = Path(__file__).parents[2] / "examples" / name
    settings = json.loads((project / "symbian-project.json").read_text())
    assert settings["preferences"]["port"] == port
    run = ET.parse(
        project / ".idea/runConfigurations/Standalone_Run.xml"
    ).getroot()[0]
    debug = ET.parse(
        project / ".idea/runConfigurations/Standalone_Debug.xml"
    ).getroot()[0]
    assert run.get("TARGET_NAME") == name
    assert run.get("RUN_PATH") == "$PROJECT_DIR$/sdk-run"
    assert run.get("CONFIG_NAME") == "symbian-pic"
    assert debug.get("remoteCommand") == f"127.0.0.1:{port}"
    assert debug.get("symbolFile") == f"$PROJECT_DIR$/.symbian/build/{name}.elf"
    assert os.access(project / "sdk-run", os.X_OK)
    assert os.access(project / "sdk-debug", os.X_OK)


@pytest.mark.parametrize(
    "name,label,port",
    [
        ("gl_app", "GL", 24701),
        ("qt_app_classic", "Qt", 24702),
        ("sdl2_app", "SDL2", 24703),
    ],
)
def test_sdk_root_has_distinct_example_run_and_debug_settings(
    name, label, port
):
    root = Path(__file__).parents[2]
    run = ET.parse(root / f".run/{label}_App_Run.run.xml").getroot()[0]
    debug = ET.parse(root / f".run/{label}_App_Debug.run.xml").getroot()[0]
    assert run.get("PROJECT_NAME") == "symbian_platform"
    assert run.get("TARGET_NAME") == f"{name}_run"
    assert run.get("CONFIG_NAME") == "debug"
    assert run.get("RUN_PATH") == f"$PROJECT_DIR$/.run/{name}-run"
    assert os.access(root / ".run" / f"{name}-run", os.X_OK)
    assert debug.get("remoteCommand") == f"127.0.0.1:{port}"
    assert debug.get("symbolFile") == (
        f"$PROJECT_DIR$/.symbian/workspace-apps/{name}/{name}.elf"
    )
    assert debug.find("debugger").text == (f"$PROJECT_DIR$/.run/{name}-debug")


def test_workspace_debugger_profiles_preserve_existing_choices(tmp_path):
    configure_workspace_ide(tmp_path)
    assert not (tmp_path / ".idea").exists()
    idea = tmp_path / ".idea"
    idea.mkdir()
    (idea / "debug-profiles.xml").write_text(
        '<project version="4"><component name="Debug Profiles">'
        '<debug-profiles><debug-profile><option name="id" value="host"/>'
        "</debug-profile></debug-profiles></component></project>"
    )
    configure_workspace_ide(tmp_path)
    profiles = ET.parse(idea / "debug-profiles.xml").getroot()
    assert len(profiles.findall(".//debug-profile")) == 4
    choices = (
        ET.parse(idea / "workspace.xml")
        .getroot()
        .find("./component[@name='SelectedDebugProfileService']")
    )
    saved = json.loads(choices.text)
    assert {
        entry["first"]["##RUN_CONFIGURATION##"]
        for entry in saved["profileIdByStamp"]
    } == {
        "Remote Debug.GL App Debug",
        "Remote Debug.Qt App Debug",
        "Remote Debug.SDL2 App Debug",
    }
    before = {p: p.read_bytes() for p in idea.iterdir()}
    configure_workspace_ide(tmp_path)
    assert before == {p: p.read_bytes() for p in idea.iterdir()}


@pytest.mark.parametrize("source", ["manifest", "path"])
def test_debug_wrapper_discovers_arm_gdb_after_generation(tmp_path, source):
    sdk = _fake_sdk(tmp_path)
    project = tmp_path / "app"
    generate(project, Preferences(name="app", uid3=0xE0000831, port=24703), sdk)
    # A fake Python launcher records the supervisor's exact argv without
    # starting an emulator. GDB is installed after project generation.
    (sdk.prefix / "bin").mkdir()
    python = sdk.prefix / "bin/python"
    python.write_text('#!/bin/sh\nprintf "%s\\n" "$@"\n')
    python.chmod(0o755)
    debugger = tmp_path / "gdb-multiarch"
    debugger.write_text("#!/bin/sh\nexit 0\n")
    debugger.chmod(0o755)
    if source == "manifest":
        manifest = json.loads((sdk.prefix / "sdk.json").read_text())
        manifest["gdb"] = str(debugger)
        (sdk.prefix / "sdk.json").write_text(json.dumps(manifest))
    env = {**os.environ, "PATH": str(tmp_path)}
    env.pop("SYMBIAN_GDB", None)
    result = subprocess.run(
        [sys.executable, str(project / "sdk-debug"), "--version"],
        capture_output=True,
        text=True,
        check=True,
        env=env,
    )
    args = result.stdout.splitlines()
    assert args[args.index("--gdb") + 1] == str(debugger)
    assert args[args.index("--port") + 1] == "24703"
    assert args[-1] == "--version"
