"""Local Run/Remote Debug configuration for the prepared GUI application."""

import os
import shlex
import shutil
import sys
import uuid
import xml.etree.ElementTree as ET
from pathlib import Path

from pydantic import BaseModel, ConfigDict, Field

from symbian.status import Code, StatusError


class _DebugProfileStamp(BaseModel):
    """Run configuration and CMake profile identifying a debugger choice."""

    model_config = ConfigDict(extra="allow", populate_by_name=True)

    run_configuration: str = Field(
        alias="##RUN_CONFIGURATION##",
        description="IDE run configuration identifier",
    )
    cmake: str | None = Field(
        default=None,
        description="IDE CMake build profile identifier when applicable",
        exclude_if=lambda value: value is None,
    )


class _DebugProfileChoice(BaseModel):
    """IDE debug profile chosen for one run/CMake profile combination."""

    model_config = ConfigDict(extra="allow")

    first: _DebugProfileStamp = Field(description="IDE profile group stamp")
    second: str = Field(description="Selected debugger profile ID")


class _DebugProfileSelection(BaseModel):
    """Serialized state of CLion's active debug profile service."""

    model_config = ConfigDict(extra="allow", populate_by_name=True)

    profile_id_by_stamp: list[_DebugProfileChoice] = Field(
        default_factory=list,
        alias="profileIdByStamp",
        description="Per run configuration debugger selections",
        exclude_if=lambda value: not value,
    )
    modification_count: int = Field(
        default=0,
        alias="modificationCount",
        description="IDE state revision counter",
        exclude_if=lambda value: value == 0,
    )


def _write(path: Path, element: ET.Element) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    ET.indent(element, space="  ")
    ET.ElementTree(element).write(path, encoding="utf-8", xml_declaration=True)


def _select_debugger(
    workspace: ET.Element,
    stamp: _DebugProfileStamp,
    profile_id: str,
) -> None:
    """Choose a debugger in CLion's actual per-target state."""
    component = workspace.find(
        "./component[@name='SelectedDebugProfileService']"
    )
    if component is None:
        component = ET.SubElement(
            workspace, "component", name="SelectedDebugProfileService"
        )
    try:
        selection = _DebugProfileSelection.model_validate_json(
            component.text or "{}"
        )
    except ValueError as error:
        raise StatusError(
            Code.DATA_LOSS, "Invalid IDE debug-profile selection state"
        ) from error
    matches = [
        choice
        for choice in selection.profile_id_by_stamp
        if choice.first == stamp
    ]
    if len(matches) == 1 and matches[0].second == profile_id:
        return
    selection.profile_id_by_stamp = [
        choice
        for choice in selection.profile_id_by_stamp
        if choice.first != stamp
    ]
    selection.profile_id_by_stamp.append(
        _DebugProfileChoice(first=stamp, second=profile_id)
    )
    selection.modification_count += 1
    component.text = selection.model_dump_json(by_alias=True, indent=2)


def configure(root: Path, gdb: Path | None, python: Path | None = None) -> dict:
    """Writes ignored IDE settings and a GDB supervisor, preserving profiles.

    Args:
        root: Prepared repository workspace.
        gdb: Actual ARM GDB executable to supervise.
        python: Python environment containing the platform package.

    Returns:
        Paths to the launcher and configuration directories.
    """
    root = root.resolve()
    if gdb is None:
        found = shutil.which("arm-none-eabi-gdb") or shutil.which(
            "gdb-multiarch"
        )
        if found is None:
            raise StatusError(
                Code.NOT_FOUND,
                "ARM GDB is unavailable; install arm-none-eabi-gdb or "
                "gdb-multiarch, or pass --gdb",
            )
        gdb = Path(found)
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
        "export PATH=" + shlex.quote(os.environ.get("PATH", "")) + "\n"
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
    run_wrapper = root / ".run/gui_app-run"
    if not run_wrapper.exists():
        run_wrapper.parent.mkdir(parents=True, exist_ok=True)
        run_wrapper.write_text(
            "#!/bin/sh\nexec "
            + " ".join(
                shlex.quote(str(a))
                for a in (
                    python,
                    "-m",
                    "symbian.emulator.launch",
                    "--root",
                    root,
                )
            )
            + ' "$@"\n'
        )
    run_wrapper.chmod(0o755)
    profile_id = str(uuid.uuid5(uuid.NAMESPACE_URL, str(root) + "/gui-gdb"))
    host_debugger_path = (
        "/usr/bin/lldb"
        if sys.platform == "darwin"
        else shutil.which("lldb") or shutil.which("gdb")
    )
    host_debugger = (
        Path(host_debugger_path) if host_debugger_path is not None else None
    )
    host_profile_type = (
        "lldb"
        if host_debugger is not None and host_debugger.name == "lldb"
        else "gdb"
    )
    host_profile_id = str(
        uuid.uuid5(uuid.NAMESPACE_URL, str(root) + "/gui-host-debugger")
    )
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
            # The optional host target may not have been built when Run is
            # pressed. This wrapper is present in the source checkout and
            # builds the guest before starting its owned emulator session.
            run.set("RUN_PATH", str(run_wrapper))
            run.set("PROGRAM_PARAMS", "")
        envs = ET.SubElement(run, "envs")
        ET.SubElement(
            envs,
            "env",
            name="PATH",
            value=(os.environ.get("PATH", "")),
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
            if any(
                entry.find(f"option[@name='id'][@value='{entry_id}']")
                is not None
                for entry_id in (profile_id, host_profile_id)
            ):
                entries.remove(entry)
        if host_debugger is not None and host_debugger.is_file():
            host_entry = ET.SubElement(entries, "debug-profile")
            host_profile_name = (
                "GUI Host LLDB"
                if host_profile_type == "lldb"
                else "GUI Host GDB"
            )
            for name, value in (
                ("id", host_profile_id),
                ("type", host_profile_type),
                ("name", host_profile_name),
            ):
                ET.SubElement(host_entry, "option", name=name, value=value)
            host_settings = ET.SubElement(host_entry, "settings")
            ET.SubElement(
                host_settings,
                "option",
                name="executable",
                value=str(host_debugger),
            )
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
        # CLion 2026.2 stores debugger selection per run configuration,
        # not in CurrentDebugProfile. GUI Run launches a host process;
        # GUI Debug attaches to the guest ARM process.
        workspace = idea / "workspace.xml"
        if workspace.exists():
            state = ET.parse(workspace).getroot()
            if host_debugger is not None and host_debugger.is_file():
                _select_debugger(
                    state,
                    _DebugProfileStamp(
                        run_configuration="CMake Application.GUI Run",
                        cmake=f"CMakeBuildProfile:{profile}",
                    ),
                    host_profile_id,
                )
            _select_debugger(
                state,
                _DebugProfileStamp(run_configuration="Remote Debug.GUI Debug"),
                profile_id,
            )
            if project != root:
                manager = state.find("./component[@name='RunManager']")
                if manager is None:
                    manager = ET.SubElement(
                        state, "component", name="RunManager"
                    )
                manager.set("selected", "CMake Application.GUI Run")
            _write(workspace, state)
    return {
        "wrapper": str(wrapper),
        "debug_profile": "Symbian GUI GDB",
        "host_debug_profile": (
            "GUI Host LLDB" if host_profile_type == "lldb" else "GUI Host GDB"
        ),
        "projects": [str(root), str(root / "examples/gui_app")],
    }
