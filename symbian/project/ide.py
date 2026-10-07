"""Portable CLion Run/Debug helpers for generated and existing applications."""

import json
import uuid
import xml.etree.ElementTree as ET
from importlib.resources import files
from pathlib import Path

from symbian.project.configuration import Preferences


def _xml(path: Path, root: ET.Element) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    ET.indent(root, space="  ")
    ET.ElementTree(root).write(path, encoding="utf-8", xml_declaration=True)


def configure_workspace_ide(root: Path) -> None:
    """Registers ARM example debuggers in an existing root IDE project."""
    from symbian.emulator.ide import _DebugProfileStamp, _select_debugger

    idea = root / ".idea"
    if not idea.is_dir():
        return
    profiles_path = idea / "debug-profiles.xml"
    profiles = (
        ET.parse(profiles_path).getroot()
        if profiles_path.exists()
        else ET.Element("project", version="4")
    )
    component = profiles.find("./component[@name='Debug Profiles']")
    if component is None:
        component = ET.SubElement(profiles, "component", name="Debug Profiles")
    entries = component.find("debug-profiles")
    if entries is None:
        entries = ET.SubElement(component, "debug-profiles")
    workspace_path = idea / "workspace.xml"
    workspace = (
        ET.parse(workspace_path).getroot()
        if workspace_path.exists()
        else ET.Element("project", version="4")
    )
    profiles_changed = False
    previous_workspace = ET.tostring(workspace)
    for name, label in (
        ("gl_app", "GL"),
        ("qt_app_classic", "Qt"),
        ("sdl2_app", "SDL2"),
    ):
        profile_id = str(
            uuid.uuid5(
                uuid.NAMESPACE_URL, f"symbian/workspace-debugger/v1/{name}"
            )
        )
        entry = entries.find(f"debug-profile/option[@value='{profile_id}']/..")
        if entry is None:
            entry = ET.SubElement(entries, "debug-profile")
            for key, value in (
                ("id", profile_id),
                ("type", "gdb"),
                ("name", f"Symbian {label} GDB"),
            ):
                ET.SubElement(entry, "option", name=key, value=value)
            settings = ET.SubElement(entry, "settings")
            ET.SubElement(
                settings,
                "option",
                name="executable",
                value=f"$PROJECT_DIR$/.run/{name}-debug",
            )
            ET.SubElement(
                settings, "option", name="workingDir", value="$PROJECT_DIR$"
            )
            profiles_changed = True
        _select_debugger(
            workspace,
            _DebugProfileStamp(
                run_configuration=f"Remote Debug.{label} App Debug"
            ),
            profile_id,
        )
    if profiles_changed:
        _xml(profiles_path, profiles)
    if ET.tostring(workspace) != previous_workspace:
        _xml(workspace_path, workspace)


def configure_ide(
    project: Path, settings: Preferences, *, profile_name: str = "Symbian App"
) -> None:
    """Creates actual CMake application and owned remote-debug launchers."""
    name = settings.name
    (project / "sdk.cmake").write_bytes(
        files("symbian.project").joinpath("templates", "sdk.cmake").read_bytes()
    )
    idea = project / ".idea"
    misc = idea / "misc.xml"
    model = (
        ET.parse(misc).getroot()
        if misc.exists()
        else ET.Element("project", version="4")
    )
    cmake = model.find("./component[@name='CMakeWorkspace']")
    if cmake is None:
        cmake = ET.SubElement(model, "component", name="CMakeWorkspace")
    cmake.set("PROJECT_DIR", "$PROJECT_DIR$")
    _xml(misc, model)
    module_name = name + ".CMake.iml"
    _xml(
        idea / module_name,
        ET.Element("module", classpath="CIDR", type="CPP_MODULE", version="4"),
    )
    module_file = idea / "modules.xml"
    model = (
        ET.parse(module_file).getroot()
        if module_file.exists()
        else (ET.Element("project", version="4"))
    )
    manager = model.find("./component[@name='ProjectModuleManager']")
    if manager is None:
        manager = ET.SubElement(model, "component", name="ProjectModuleManager")
    modules = manager.find("modules")
    if modules is None:
        modules = ET.SubElement(manager, "modules")
    module_path = "$PROJECT_DIR$/.idea/" + module_name
    if modules.find(f"module[@filepath='{module_path}']") is None:
        ET.SubElement(
            modules,
            "module",
            fileurl="file://" + module_path,
            filepath=module_path,
        )
    _xml(module_file, model)
    for filename in ("sdk-run", "sdk-debug"):
        wrapper = project / filename
        wrapper.write_bytes(
            files("symbian.project")
            .joinpath("templates", "sdk-tool")
            .read_bytes()
        )
        wrapper.chmod(0o755)
    for filename, target in (("run.sh", "sdk-run"), ("debug.sh", "sdk-debug")):
        wrapper = project / filename
        wrapper.write_text(
            '#!/bin/sh\nexec "$(dirname "$0")/' + target + '" "$@"\n'
        )
        wrapper.chmod(0o755)
    container = ET.Element("component", name="ProjectRunConfigurationManager")
    run = ET.SubElement(
        container,
        "configuration",
        default="false",
        name="App Run",
        type="CMakeRunConfiguration",
        factoryName="Application",
        PROJECT_NAME=name,
        TARGET_NAME=name,
        CONFIG_NAME=profile_name,
        RUN_PATH="$PROJECT_DIR$/sdk-run",
        PROGRAM_PARAMS="",
        WORKING_DIR="$PROJECT_DIR$",
        PASS_PARENT_ENVS_2="true",
    )
    ET.SubElement(run, "method", v="2")
    _xml(project / ".run/App_Run.run.xml", container)
    container = ET.Element("component", name="ProjectRunConfigurationManager")
    debug = ET.SubElement(
        container,
        "configuration",
        default="false",
        name="App Debug",
        type="CLion_Remote",
        factoryName="Remote Debug",
        version="1",
        remoteCommand=f"127.0.0.1:{settings.port}",
        symbolFile=f"$PROJECT_DIR$/.symbian/build/{name}.elf",
        sysroot="",
        toolchain="Default",
    )
    ET.SubElement(debug, "debugger", kind="GDB", isBundled="false").text = (
        "$PROJECT_DIR$/sdk-debug"
    )
    ET.SubElement(debug, "method", v="2")
    _xml(project / ".run/App_Debug.run.xml", container)
    profile_id = str(
        uuid.uuid5(uuid.NAMESPACE_URL, "symbian/project-debugger/v1")
    )
    root = ET.Element("project", version="4")
    component = ET.SubElement(root, "component", name="Debug Profiles")
    profiles = ET.SubElement(component, "debug-profiles")
    entry = ET.SubElement(profiles, "debug-profile")
    for key, value in (
        ("id", profile_id),
        ("type", "gdb"),
        ("name", "Symbian App GDB"),
    ):
        ET.SubElement(entry, "option", name=key, value=value)
    settings_node = ET.SubElement(entry, "settings")
    ET.SubElement(
        settings_node,
        "option",
        name="executable",
        value="$PROJECT_DIR$/sdk-debug",
    )
    ET.SubElement(
        settings_node, "option", name="workingDir", value="$PROJECT_DIR$"
    )
    _xml(project / ".idea/debug-profiles.xml", root)
    workspace = project / ".idea/workspace.xml"
    root = (
        ET.parse(workspace).getroot()
        if workspace.exists()
        else ET.Element("project", version="4")
    )
    component = root.find("./component[@name='CMakeSettings']")
    if component is None:
        component = ET.SubElement(
            root, "component", name="CMakeSettings", AUTO_RELOAD="true"
        )
    configurations = component.find("configurations")
    if configurations is None:
        configurations = ET.SubElement(component, "configurations")
    # IDE releases differ in how configure/build presets become profile IDs.
    # Keep a stable ordinary IDE profile; CMake itself still owns the SDK/ABI.
    entry = configurations.find(
        f"./configuration[@PROFILE_NAME='{profile_name}']"
    )
    if entry is None:
        entry = ET.SubElement(
            configurations, "configuration", PROFILE_NAME=profile_name
        )
    entry.set("ENABLED", "true")
    entry.attrib.pop("FROM_PRESET", None)
    entry.set("CONFIG_NAME", "Debug")
    entry.set(
        "GENERATION_OPTIONS",
        "-G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON "
        "-DCMAKE_TOOLCHAIN_FILE=$PROJECT_DIR$/sdk.cmake",
    )
    entry.set("GENERATION_DIR", "$PROJECT_DIR$/.symbian/build/cmake")
    current = root.find("./component[@name='CurrentDebugProfile']")
    if current is None:
        current = ET.SubElement(root, "component", name="CurrentDebugProfile")
    current.clear()
    current.set("name", "CurrentDebugProfile")
    ET.SubElement(current, "option", name="debugProfileId", value=profile_id)
    # Current IDEs remember the debugger separately for each run config.
    # A global profile alone leaves Remote Debug using the bundled GDB.
    selected = root.find("./component[@name='SelectedDebugProfileService']")
    if selected is None:
        selected = ET.SubElement(
            root, "component", name="SelectedDebugProfileService"
        )
    state = (
        json.loads(selected.text)
        if selected.text
        else {"profileIdByStamp": [], "modificationCount": 0}
    )
    stamp = {"##RUN_CONFIGURATION##": "Remote Debug.App Debug"}
    entries = state["profileIdByStamp"]
    entries[:] = [e for e in entries if e["first"] != stamp]
    entries.append({"first": stamp, "second": profile_id})
    state["modificationCount"] += 1
    selected.text = json.dumps(state, indent=2)
    manager = root.find("./component[@name='RunManager']")
    if manager is None:
        manager = ET.SubElement(root, "component", name="RunManager")
    manager.set("selected", "CMake Application.App Run")
    _xml(workspace, root)
