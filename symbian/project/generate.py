"""Interactive and scripted creation of complete standalone app projects."""

import json
import re
import secrets
import sys
from importlib.resources import files
from pathlib import Path

from symbian.project.configuration import Preferences
from symbian.project.ide import configure_ide
from symbian.project.sdk import AppSdk
from symbian.status import Code, StatusError


def _json(path: Path, value) -> None:
    path.write_text(json.dumps(value, indent=2) + "\n")


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
    if not settings.timer_tasks:
        raise StatusError(
            Code.INVALID_ARGUMENT,
            "New projects require the modern C++ runtime and timer tasks",
        )
    project.mkdir(parents=True, exist_ok=True)
    for name in (
        ".clang-format",
        "app.cc",
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
    if not settings.timer_tasks:
        raise StatusError(
            Code.INVALID_ARGUMENT,
            "The starter requires the modern C++ runtime and timer tasks",
        )
    (project / "CMakeLists.txt").write_text(
        f"""cmake_minimum_required(VERSION 3.28)
project({name} LANGUAGES CXX ASM)
include(SymbianApp)
symbian_add_executable({name} app.cc)
set(SYMBIAN_ENABLE_ABSEIL_STATUS ON CACHE BOOL "Required by this app" FORCE)
set(SYMBIAN_ENABLE_TIMER_TASKS ON CACHE BOOL "Required by this app" FORCE)
if(NOT TARGET Symbian::AbseilStatusOr OR NOT TARGET Symbian::Stackless OR
   NOT EXISTS "${{SYMBIAN_SDK_PREFIX}}/cmake/modern_cpp.h")
  message(FATAL_ERROR
    "This app requires modern C++, Abseil Status and timer tasks; "
    "update the SDK")
endif()
target_link_libraries({name} PRIVATE Symbian::WindowServer Symbian::Gdi
  Symbian::AbseilStatusOr Symbian::Stackless)
target_compile_definitions({name} PRIVATE
  SYMBIAN_ENABLE_ABSEIL_STATUS=1 SYMBIAN_ENABLE_TIMER_TASKS=1)
set(modern_cpp "${{SYMBIAN_SDK_PREFIX}}/cmake/modern_cpp.h")
target_compile_options({name} PRIVATE
  "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include \\\"${{modern_cpp}}\\\">")
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
    _json(
        project / "sdk-location.json",
        {"sdk": str(sdk.prefix), "python": str(sdk.python)},
    )
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
        configure_ide(project, settings)
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
set a breakpoint in `LogTime`, continue, and tap the screen.
GDB uses the app's actual runtime mapping. Stop reaps the emulator.
The generated debug configuration requires the declared ARM GDB executable.

The app displays hello world and appends local system time for each tap or
Enter/centre key. It adds a second row after a 1.5-second timer Future;
Clear cancels those Tasks before they can update the view. Its TimerPump and
EventMailbox share one Window Server event-thread wait. The modern C++ runtime,
Abseil Status and timer tasks are required and enabled unconditionally.
The selected firmware must provide libpthread.dll. The app adapts to compact
screens and uses the default font.
It retains the latest twelve lines. Clear removes them; Exit or Escape
cancels and drains outstanding requests and closes
handles. Left/right softkeys provide Clear/Exit; Backspace clears.
On the smallest screens only the newest fitting rows are visible.
Focus loss disables input; focus gain redraws.

Edit `app.cc` for drawing, input and timer behavior. State lives in the
window event loop that uses it; `LogTime` formats the local time and returns
an Abseil Status. ASCII is converted to UTF-16 at the W32 API.
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
