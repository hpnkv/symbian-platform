"""Local Run/Remote Debug configuration for the prepared GUI experiment."""

import os
import shlex
import sys
import uuid
import xml.etree.ElementTree as ET
from pathlib import Path

from symbian.status import Code, StatusError


def _write(path: Path, element: ET.Element) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    ET.indent(element, space="  ")
    ET.ElementTree(element).write(path, encoding="utf-8", xml_declaration=True)


def configure(root: Path, gdb: Path, python: Path | None = None) -> dict:
    """Writes ignored IDE settings and a GDB supervisor, preserving profiles.

    Args:
        root: Prepared repository workspace.
        gdb: Actual ARM GDB executable to supervise.
        python: Python environment containing the platform package.

    Returns:
        Paths to the launcher and configuration directories.
    """
    root = root.resolve()
    gdb = gdb.resolve()
    python = Path(python or sys.executable).absolute()
    for executable in (gdb, python):
        if not executable.is_file():
            raise StatusError(
                Code.NOT_FOUND, f"Missing executable: {executable}"
            )
    wrapper = root / ".symbian/clion-setup/gui-gdb"
    wrapper.parent.mkdir(parents=True, exist_ok=True)
    wrapper.write_text(
        "#!/bin/sh\n"
        'export PATH="/opt/homebrew/bin:/opt/homebrew/opt/llvm/bin:$PATH"\n'
        "exec "
        + " ".join(
            shlex.quote(str(a))
            for a in (
                python,
                "-m",
                "symbian.emulator.launch",
                "--root",
                root,
                "--gdb",
                gdb,
            )
        )
        + ' "$@"\n'
    )
    wrapper.chmod(0o755)
    profile_id = str(uuid.uuid5(uuid.NAMESPACE_URL, str(root) + "/gui-gdb"))
    for project in (root, root / "examples/gui_app"):
        idea = project / ".idea"
        profile = "clion-arm"
        if project == root:
            profile = "debug"
            workspace = idea / "workspace.xml"
            if workspace.exists():
                state = ET.parse(workspace).getroot()
                enabled = state.findall(
                    "./component[@name='CMakeSettings']/configurations/"
                    "configuration[@ENABLED='true']"
                )
                if enabled:
                    profile = enabled[0].get("PROFILE_NAME", profile)
        configurations = idea / "runConfigurations"
        container = ET.Element(
            "component", name="ProjectRunConfigurationManager"
        )
        run = ET.SubElement(
            container,
            "configuration",
            default="false",
            name="GUI Run",
            type="CMakeRunConfiguration",
            factoryName="Application",
            PROJECT_NAME="symbian_platform" if project == root else "gui_app",
            TARGET_NAME="gui_app_run" if project == root else "gui_app",
            CONFIG_NAME=profile,
            RUN_PATH=str(python),
            PROGRAM_PARAMS="-m symbian.emulator.launch --root "
            + shlex.quote(str(root)),
            WORKING_DIR=str(root),
            PASS_PARENT_ENVS_2="true",
            EMULATE_TERMINAL="false",
        )
        if project == root:
            # CLion keeps an empty executable on a formerly custom target.
            # Use the tested host artifact explicitly rather than depending
            # on its cached target-kind/executable inference.
            run.set("RUN_PATH", str(root / "build/debug/gui_app_run"))
            run.set("PROGRAM_PARAMS", "")
        envs = ET.SubElement(run, "envs")
        ET.SubElement(
            envs,
            "env",
            name="PATH",
            value=(
                "/opt/homebrew/bin:/opt/homebrew/opt/llvm/bin:"
                + os.environ.get("PATH", "")
            ),
        )
        ET.SubElement(envs, "env", name="PYTHONPATH", value=str(root))
        ET.SubElement(run, "method", v="2")
        _write(configurations / "GUI_Run.xml", container)
        container = ET.Element(
            "component", name="ProjectRunConfigurationManager"
        )
        debug = ET.SubElement(
            container,
            "configuration",
            default="false",
            name="GUI Debug",
            type="CLion_Remote",
            factoryName="Remote Debug",
            version="1",
            remoteCommand="127.0.0.1:24689",
            symbolFile=str(root / ".symbian/gui-app/gui_app.elf"),
            sysroot="",
            toolchain="Default",
        )
        ET.SubElement(debug, "debugger", kind="GDB", isBundled="false").text = (
            str(wrapper)
        )
        for remote, local in (
            ("/symbian-src/gui_app", root / "examples/gui_app"),
            ("/symbian-sdk/include", root / ".symbian/gui-sdk/include"),
        ):
            ET.SubElement(
                debug, "pathMapping", remote=":" + remote, local=str(local)
            )
        ET.SubElement(debug, "method", v="2")
        _write(configurations / "GUI_Debug.xml", container)
        profiles_path = idea / "debug-profiles.xml"
        profiles = (
            ET.parse(profiles_path).getroot()
            if profiles_path.exists()
            else ET.Element("project", version="4")
        )
        component = profiles.find("./component[@name='Debug Profiles']")
        if component is None:
            component = ET.SubElement(
                profiles, "component", name="Debug Profiles"
            )
        entries = component.find("debug-profiles")
        if entries is None:
            entries = ET.SubElement(component, "debug-profiles")
        for entry in list(entries):
            if (
                entry.find(f"option[@name='id'][@value='{profile_id}']")
                is not None
            ):
                entries.remove(entry)
        entry = ET.SubElement(entries, "debug-profile")
        for name, value in (
            ("id", profile_id),
            ("type", "gdb"),
            ("name", "Symbian GUI GDB"),
        ):
            ET.SubElement(entry, "option", name=name, value=value)
        settings = ET.SubElement(entry, "settings")
        ET.SubElement(settings, "option", name="executable", value=str(wrapper))
        ET.SubElement(settings, "option", name="workingDir", value=str(root))
        _write(profiles_path, profiles)
        # The dedicated GUI project can select its guest debugger. Preserve
        # the root project's existing host-debugger selection.
        workspace = idea / "workspace.xml"
        if project != root and workspace.exists():
            state = ET.parse(workspace).getroot()
            current = state.find("./component[@name='CurrentDebugProfile']")
            if current is None:
                current = ET.SubElement(
                    state, "component", name="CurrentDebugProfile"
                )
            current.clear()
            current.set("name", "CurrentDebugProfile")
            ET.SubElement(
                current, "option", name="debugProfileId", value=profile_id
            )
            manager = state.find("./component[@name='RunManager']")
            if manager is None:
                manager = ET.SubElement(state, "component", name="RunManager")
            manager.set("selected", "CMake Application.GUI Run")
            _write(workspace, state)
    return {
        "wrapper": str(wrapper),
        "debug_profile": "Symbian GUI GDB",
        "projects": [str(root), str(root / "examples/gui_app")],
    }
