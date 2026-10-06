"""Interactive and scripted creation of complete standalone app projects."""

import json
import re
import secrets
import sys
import uuid
import xml.etree.ElementTree as ET
from importlib.resources import files
from pathlib import Path

from symbian.project.configuration import Preferences
from symbian.project.sdk import AppSdk
from symbian.status import Code, StatusError


def _json(path: Path, value) -> None:
    path.write_text(json.dumps(value, indent=2) + "\n")


def _xml(path: Path, root: ET.Element) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    ET.indent(root, space="  ")
    ET.ElementTree(root).write(path, encoding="utf-8", xml_declaration=True)


def _ide(project: Path, settings: Preferences, sdk: AppSdk) -> None:
    """Creates actual CMake application and owned remote-debug launchers."""
    name = settings.name
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
        CONFIG_NAME="Symbian App",
        RUN_PATH="$PROJECT_DIR$/sdk-run",
        PROGRAM_PARAMS="",
        WORKING_DIR="$PROJECT_DIR$",
        PASS_PARENT_ENVS_2="true",
    )
    ET.SubElement(run, "method", v="2")
    _xml(project / ".idea/runConfigurations/App_Run.xml", container)
    if sdk.gdb:
        container = ET.Element(
            "component", name="ProjectRunConfigurationManager"
        )
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
        _xml(project / ".idea/runConfigurations/App_Debug.xml", container)
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
    entry = configurations.find("./configuration[@PROFILE_NAME='Symbian App']")
    if entry is None:
        entry = ET.SubElement(
            configurations, "configuration", PROFILE_NAME="Symbian App"
        )
    entry.set("ENABLED", "true")
    entry.attrib.pop("FROM_PRESET", None)
    entry.set("CONFIG_NAME", "Debug")
    entry.set(
        "GENERATION_OPTIONS", "-G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
    )
    entry.set("GENERATION_DIR", "$PROJECT_DIR$/.symbian/build/cmake")
    if sdk.gdb:
        current = root.find("./component[@name='CurrentDebugProfile']")
        if current is None:
            current = ET.SubElement(
                root, "component", name="CurrentDebugProfile"
            )
        current.clear()
        current.set("name", "CurrentDebugProfile")
        ET.SubElement(
            current, "option", name="debugProfileId", value=profile_id
        )
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


def generate(destination: Path, settings: Preferences, sdk: AppSdk) -> dict:
    """Writes sources, manifests, CMake presets and optional IDE integration.

    Args:
        destination: New or empty project directory.
        settings: Validated application identity and IDE choice.
        sdk: Prepared target SDK and explicit host/firmware dependencies.

    Returns:
        Project location, identity and build/run instructions.
    """
    project = destination.absolute()
    if project.is_symlink() or (
        project.exists() and (not project.is_dir() or any(project.iterdir()))
    ):
        raise StatusError(
            Code.ALREADY_EXISTS, f"Project is not empty: {project}"
        )
    if any(
        c in str(project) + str(sdk.prefix) for c in ("\n", "\r", ";", '"', "$")
    ):
        raise StatusError(
            Code.INVALID_ARGUMENT,
            "Path cannot be represented by this CMake profile",
        )
    if settings.architecture not in sdk.architectures:
        raise StatusError(
            Code.FAILED_PRECONDITION,
            f"SDK has no {settings.architecture} runtime; update the SDK "
            "or choose --architecture armv5t",
        )
    project.mkdir(parents=True, exist_ok=True)
    for name in (
        ".clang-format",
        "app.cc",
        "model.cc",
        "model.h",
        "clock_time.h",
        "app_bridge.h",
        "app_bridge.cc",
        "icon.svg",
    ):
        (project / name).write_bytes(
            files("symbian.project").joinpath("templates", name).read_bytes()
        )
    return configure_project(project, settings, sdk)


def configure_project(
    project: Path, settings: Preferences, sdk: AppSdk
) -> dict:
    """Refreshes build/IDE integration, preserving application sources."""
    project = project.resolve()
    if settings.architecture not in sdk.architectures:
        raise StatusError(
            Code.FAILED_PRECONDITION,
            f"Selected SDK has no {settings.architecture} runtime. "
            "Update the SDK "
            "or choose --architecture armv5t for an SDK providing ARMv5T",
        )
    name = settings.name
    task_profile = "ON" if settings.timer_tasks else "OFF"
    (project / "CMakeLists.txt").write_text(
        f"""cmake_minimum_required(VERSION 3.28)
project({name} LANGUAGES CXX ASM)
include(SymbianApp)
symbian_add_executable({name} app.cc model.cc app_bridge.cc)
target_link_libraries({name} PRIVATE Symbian::WindowServer Symbian::Gdi)
option(SYMBIAN_ENABLE_ABSEIL_STATUS
  "Use guest Abseil Status/StatusOr in application logic" {task_profile})
option(SYMBIAN_ENABLE_TIMER_TASKS
  "Stackless timer Tasks; requires firmware libpthread" {task_profile})
if(SYMBIAN_ENABLE_ABSEIL_STATUS OR SYMBIAN_ENABLE_TIMER_TASKS)
  if(NOT TARGET Symbian::AbseilStatusOr)
    message(FATAL_ERROR
      "The selected SDK lacks guest Abseil Status; update the SDK")
  endif()
  target_link_libraries({name} PRIVATE Symbian::AbseilStatusOr)
else()
  target_link_libraries({name} PRIVATE Symbian::Runtime)
endif()
if(SYMBIAN_ENABLE_ABSEIL_STATUS)
  target_compile_definitions({name} PRIVATE SYMBIAN_ENABLE_ABSEIL_STATUS=1)
endif()
if(SYMBIAN_ENABLE_TIMER_TASKS)
  target_link_libraries({name} PRIVATE Symbian::Stackless)
  target_compile_definitions({name} PRIVATE SYMBIAN_ENABLE_TIMER_TASKS=1)
endif()
"""
    )
    (project / "symbian.toml").write_text(f"""[project]
name = "{name}"
kind = "e32-import"
cmake_preset = "symbian-pic"
uid3 = {settings.uid3:#x}

[package]
uid = {settings.uid3:#x}
name = "{name}"
vendor = "Local development"
executable_name = "{name}.exe"
version = [0, 1, 0]

[application]
caption = "{name}"
short_caption = "{name}"
icon = "icon.svg"

# Add translations as [application.localizations.fr], with caption and
# short_caption. The SDK compiles the menu resources; no RSS files are needed.
""")
    _json(
        project / "CMakePresets.json",
        {
            "version": 6,
            "configurePresets": [
                {
                    "name": "symbian-pic",
                    "generator": "Ninja",
                    "binaryDir": "${sourceDir}/.symbian/build/cmake",
                    "toolchainFile": "${sourceDir}/.symbian/sdk.cmake",
                    "cacheVariables": {
                        "CMAKE_BUILD_TYPE": "Debug",
                        "CMAKE_EXPORT_COMPILE_COMMANDS": "ON",
                    },
                }
            ],
            "buildPresets": [
                {"name": "symbian-pic", "configurePreset": "symbian-pic"}
            ],
        },
    )
    _json(
        project / "symbian-project.json", {"preferences": settings.model_dump()}
    )
    _json(project / "sdk-location.json", {"sdk": str(sdk.prefix)})
    local = project / ".symbian"
    local.mkdir(exist_ok=True)
    # Only a generated locator lives with the project; all policy is SDK-owned.
    (local / "sdk.cmake").write_text(
        f'include("{sdk.prefix}/cmake/symbian-arm.cmake")\n'
    )
    database = project / "compile_commands.json"
    if not database.exists() and not database.is_symlink():
        database.symlink_to(".symbian/build/cmake/compile_commands.json")
    (project / ".clangd").write_text(
        "CompileFlags:\n  CompilationDatabase: .symbian/build/cmake\n"
    )
    (project / ".gitignore").write_text(
        ".symbian/\n.idea/workspace.xml\nsdk-location.json\ncompile_commands.json\n"
    )
    if settings.ide == "intellij":
        _ide(project, settings, sdk)
    (project / "README.md").write_text(f"""# {name}

Open this project in CLion: the `Symbian App` CMake profile is enabled.
SDK location is the local `sdk-location.json` setting.
Both the IDE profile and `symbian-pic` CLI preset select the actual
cross compiler;
no separately named “Symbian ARM” toolchain is required.

```sh
symbian app build --project .
symbian app run --project .
symbian emu resolve --project .
```

ROM / drive Z lives in the user's shared firmware store. Settings inherit
global -> SDK -> project -> command overrides. Import a dump with
`symbian firmware import DUMP.7z --name device --use global`, or select one
for this project with `symbian emu configure --scope project --project .
--firmware device`. Project selections use exact portable content IDs.
`emulator.json` is optional; firmware is not included in this project or SDK.
Use `sdk-run --firmware device` for one run, or put the same override in the
IDE's program arguments. Debug accepts the same flags through `sdk-debug`.
The current starter needs EKA2 ARM EABI imports and Window Server/font services;
importing an EKA1 dump does not supply that missing application ABI.

Choose **App Run** in CLion. It executes the installed SDK supervisor,
which publishes a matching ELF/E32 pair and owns a fresh emulator instance.
Choose **App Debug** to launch the emulator halted and attach ARM GDB;
set a breakpoint in `AppModel::LogTime`, continue, and tap the screen.
GDB uses the app's actual runtime mapping. Stop reaps the emulator.
The generated debug configuration requires the declared ARM GDB executable.

The app displays hello world and appends local system time for each tap or
Enter/centre key. With the default task profile it adds a second row after a
1.5-second timer Future; Clear cancels those Tasks before they can update the
view. Its TimerPump and EventMailbox share one Window Server event-thread wait.
The task profile requires libpthread.dll in the selected firmware. If
`symbian init --firmware` selects a dump without it, init chooses the portable
profile; `--portable-runtime` selects that profile explicitly. Both can be
changed later with the CMake options in CMakeLists.txt. The app adapts to
compact screens and uses the default font.
It retains the latest twelve lines. Clear removes them; Exit or Escape
cancels and drains outstanding requests, destroys the model, and closes
handles. Left/right softkeys provide Clear/Exit; Backspace clears.
On the smallest screens only the newest fitting rows are visible.
Focus loss disables input; focus gain redraws.

Edit `model.h` and `model.cc` for ordinary typed C++ application state and
behavior. `app.cc` converts ASCII to UTF-16 only at the W32 API. The SDK-owned
`app_bridge.cc` contains the narrow C ABI boundary and fallible model creation;
application model code needs neither `extern "C"` nor opaque `void*` handles.
No Qt dependency is needed by this native W32 app.

The runtime disables C++ exceptions and RTTI. Ordinary allocation failure
exits with KErrNoMemory; STL allocation is not a recoverable StatusOr API.
The selected SDK supplies the target headers, frozen ordinal proxies,
runtime libraries and compiler. Firmware, the emulator and guest GDB are
configured separately; firmware is copied into a disposable instance for
each run.

Target ISA: **{settings.architecture}**, 32-bit ARM EABI soft-float.
New projects use armv6 by default. Use `symbian init --architecture armv5t`
to select the older ISA.
ISA selection does not identify the physical processor from a ROM dump. Keep
separate CMake build trees for different targets; the SDK chooses the matching
runtime archive. Target selection lives once in `symbian-project.json`'s
`preferences.architecture`; CMake, the CLI and the IDE use that same value.
Change it and reconfigure in a separate build tree. Reconfiguration checks
compiler/target identity.

Application UID {settings.uid3:#x} is an experimental development identity.
The example is fixed-orientation and has no persistent storage. It has
not been validated on a physical phone. Its package registers an application
menu entry. Edit `[application]` in `symbian.toml` for the fallback caption,
short caption and project-relative SVG icon. Add a
`[application.localizations.fr]` table with `caption` and `short_caption`
to translate the menu entry. The SDK compiles these assets; application code
needs no RSS files. In-app text remains separate from menu translations.
""")
    return {
        "project": str(project),
        "name": name,
        "uid3": settings.uid3,
        "ide": settings.ide,
        "architecture": settings.architecture,
    }


def wizard(
    destination: Path,
    sdk_path: Path,
    *,
    name: str | None = None,
    ide: str | None = None,
    uid3: int | None = None,
    architecture: str | None = None,
    timer_tasks: bool = True,
    non_interactive: bool = False,
) -> dict:
    """Collects preferences, then creates the validated application project."""
    suggested = re.sub(r"[^a-z0-9_]", "_", destination.name.lower())
    if not suggested or not suggested[0].isalpha():
        suggested = "hello_time"
    if not non_interactive:
        if not sys.stdin.isatty():
            raise StatusError(
                Code.INVALID_ARGUMENT,
                "Use --non-interactive with redirected input",
            )
        if name is None:
            name = (
                input(f"Application name [{suggested}]: ").strip() or suggested
            )
        if ide is None:
            choice = (
                input("Create IntelliJ/CLion Run and Debug configs? [Y/n]: ")
                .strip()
                .lower()
            )
            ide = "none" if choice in ("n", "no") else "intellij"
        if uid3 is None:
            choice = input(
                "Experimental UID3 [automatically assigned]: "
            ).strip()
            if choice:
                try:
                    uid3 = int(choice, 0)
                except ValueError as error:
                    raise StatusError(
                        Code.INVALID_ARGUMENT, "UID3 must be an integer"
                    ) from error
        if architecture is None:
            architecture = (
                input("Target architecture [armv6; or armv5t]: ").strip()
                or "armv6"
            )
    from pydantic import ValidationError

    try:
        settings = Preferences(
            name=name or suggested,
            architecture=architecture or "armv6",
            timer_tasks=timer_tasks,
            ide=ide or "intellij",
            uid3=(
                uid3 if uid3 is not None else 0xE0000000 | secrets.randbits(28)
            ),
        )
    except ValidationError as error:
        raise StatusError(Code.INVALID_ARGUMENT, str(error)) from error
    return generate(destination, settings, AppSdk.load(sdk_path))
