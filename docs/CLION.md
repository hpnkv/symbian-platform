# CLion for the host platform and ARM GUI

For generated applications, visible SDK installation, relative project settings
and IDE Run/Debug integration, see [Standalone projects](PROJECTS.md).

The root CMake project now includes the prepared ARM GUI alongside native
utilities and tests. Its compilation database supplies the real ARM triple,
macros and SDK headers for `examples/gui_app/app.cc`, while tooling sources keep
native flags. The standalone GUI project remains available.
The root GUI now defaults to ARMv6 and its CMake file API records an explicit
`--target=armv6-none-eabi` flag. This lets CLion probe the guest compiler with
the correct target while probing native tooling with the Mac host target.
Set `SYMBIAN_GUI_TARGET_ARCH=armv5t` in a user preset for an older guest;
reload CMake so both the editor model and published E32 use that choice.

The root project also has dedicated **guest probe** CMake profiles. The shared
`guest-probes-armv6` and `guest-probes-armv5t` presets select the real ARM
toolchain; ignored local `clion-guest-probes-*` presets select this Mac's LLVM,
LLD and Ninja explicitly. The ARMv6 IDE profile was enabled alongside the
existing host Debug profile without changing its other settings; ARMv5T is
available but disabled until selected. Reload CMake in the IDE after opening
the new presets. The `symbian_probe_index` aggregate builds per-project object
targets for ABI, pinned Abseil allocator/Status, C++20, C++20 modules,
DLL/data/lifecycle, E32, import, pointer and runtime probes. The Abseil
objects use the selected SDK's real `Symbian::AbseilStatusOr` headers and
streams profile. Each source belongs to a real target with the ARM
compiler triple, its local headers and special exception/locale settings
where needed. The C++20 module uses CMake's module scanner. The local presets
also include the Mbed TLS DLL probe using `MBEDTLS_SOURCE` and
`SYMBIAN_SDK_PREFIX`; a checkout without that external adaptation omits only
that target. The C++20 module requires upstream Clang with `clang-scan-deps`;
CMake diagnoses a host compiler without it during configure. Both local ARM
profiles compiled the original 48 probe
`.cc`/`.c`/`.cppm`/`.S` files; the later two Abseil objects separately
configured and compiled on both ARM profiles. The later ARM/Thumb fiber-context
probe is also an ordinary runtime index source. Both compilation databases
contain the new probe sources as well as the original 48
with the matching target triple. The index creates no runnable ARM ELF in the
host IDE project. It does not publish or execute an E32 image; execution
acceptance remains in the probe integration tests and standalone projects.

| Target in the root project | Purpose |
| --- | --- |
| `gui_app` | Compile/link the ARM ELF for source development |
| `gui_app_e32` | Independently build twice and publish the matching ELF/E32 |
| `gui_app_run` | Native executable that starts the owned emulator supervisor |

Reload the root CMake project after these changes. Select **GUI Run** (or the
`gui_app_run` CMake Application target) and press Run. The launcher replaces
itself with the Python supervisor, which publishes the app, copies the stopped
golden instance, opens EKA2L1 and reaps its owned child on Stop/exit. Building
`gui_app_run` only builds the launcher; pressing Run executes it. The ARM
`gui_app.elf` cannot be launched directly by the host.

The saved root **GUI Run** explicitly sets Executable to
`/Users/helena/dev/symbian/build/debug/gui_app_run`; Program arguments is empty.
The launcher target writes that stable path even in the IDE's existing Debug
profile. If an already-open configuration dialog still says **Not selected**,
close/reopen the dialog after reloading the project, or select that executable
path in its Executable field. No firmware/emulator arguments belong there.

Root Run and Remote Debug configurations are installed in ignored `.idea`
files. Root **GUI Debug** uses the **Symbian GUI GDB** debug profile; select it
before guest debugging and restore the host debugger profile for native tests.
The generator preserves the root's existing current debugger selection because
this IDE version selects debug profiles at project scope.

| Source | Compilation database |
| --- | --- |
| Host tooling and root GUI | `build/debug/compile_commands.json` |
| Root platform probes (ARMv6/ARMv5T) | `build/guest-probes-armv6/compile_commands.json` / `build/guest-probes-armv5t/compile_commands.json` |
| Standalone guest project | `.symbian/gui-app/compile_commands.json` |
| Research Qt adapter | `build/eka2l1/compile_commands.json` |

The root target graph/ARM and native builds and actual emulator launch are
verified from the terminal. The IDE UI's newly reloaded root Run/debug frontend
has not been driven through authenticated desktop automation.

For the research adapter, open that existing database as a project or configure
the upstream CMake project with the hook/options from WALKTHROUGH.md section 6.
It is intentionally absent from the production host library's target graph.

## Settings installed on this machine

The running application is IntelliJ IDEA 2026.2.1 with the CLion plugin. Its
macOS toolchain settings now contain **Symbian ARM**, with the explicit CMake,
Ninja, Clang and ARM GDB paths listed below. The original Default toolchain is
preserved. Backups are in `.symbian/clion-setup/backups`.

`examples/gui_app/CMakeUserPresets.json` contains the ignored **clion-arm**
configure/build preset. It inherits `symbian-pic`, prepends Homebrew to PATH,
sets Clang/Ninja/LLD explicitly and builds into `.symbian/clion/gui-app`. The
GUI's ignored `.idea` settings enable that build profile and disable host Debug.
This plugin names build profiles directly (`clion-arm`); older versions used
combined names such as `clion-arm - clion-arm`.

The current preset uses the IDE's existing **Default** toolchain. The running
IDE had cached its application settings and reported “Symbian ARM is not found”
after that toolchain was saved externally. Selecting Default avoids the missing
name; CMake still loads the actual ARM cross-toolchain file and explicit paths.
**Use Reload CMake Project after loading the changed preset.** Restarting the
IDE loads the separately saved Symbian ARM settings. The build preset can
continue using Default; GUI Debug selects its separate native GDB profile
below and does not require that restart. The real debugger executable is
`/opt/homebrew/bin/arm-none-eabi-gdb`.

The GUI project is now open and its actual IDE CMake configure exits zero.
The IDE-generated CMake API target contains `app.cc`, `startup.cc` and
`startup.S`; its initial model reports two resolved C++ sources, one assembly
source and zero unknown sources. The IDE also invokes the `gui_app` build.
Opening the changed local preset through the normal launcher triggered the
reload. This confirms target membership in the actual IDE, beyond terminal
configure/build and the zero-error clangd check. Select **clion-arm** in the
GUI project window when editing the app; the repository window now has a combined host/guest target graph.

The remote debugger frontend and detailed editor inspections remain untested.
The IDE control endpoint and desktop automation were unavailable for
authenticated UI interaction; their authorization settings were left unchanged.
Private evidence is in `.symbian/clion-setup/ide-model.log` and `target.json`.

```sh
cd /Users/helena/dev/symbian/examples/gui_app
/opt/homebrew/bin/cmake --preset clion-arm
/opt/homebrew/bin/cmake --build --preset clion-arm
```

## Open the GUI CMake project

1. In **File → Open**, select `/Users/helena/dev/symbian/examples/gui_app` and
   open it in a new window. Keep the repository window for host development.
2. Under **Settings → Build, Execution, Deployment → CMake**, enable and select
   the local `clion-arm` profile on this prepared machine. On a fresh checkout
   use the shared `symbian-pic` profile. If absent, use **Load CMake Presets**.
   Disable an automatically created host `Debug` profile for this GUI project.
3. Reload CMake. `gui_app` should appear as a target; `app.cc`, `startup.cc` and
   `startup.S` now belong to it. The preset selects the actual ARM toolchain,
   C++20, staged SDK includes and the two frozen import proxies.

CLion imports presets as profiles which may initially be disabled. See
[JetBrains' preset instructions](https://www.jetbrains.com/help/clion/cmake-presets.html).
The updated example preset is independently usable; it no longer depends on the
CLI supplying `SYMBIAN_IMPORT_PROXIES` to configure successfully.

Use the local macOS toolchain. On this Apple Silicon machine the relevant tools
are:

| Setting | Path |
| --- | --- |
| CMake | `/opt/homebrew/bin/cmake` |
| Build tool | `/opt/homebrew/bin/ninja` |
| C++ compiler | `/usr/bin/clang++` |
| ARM ELF linker, found by the CMake toolchain | `/opt/homebrew/bin/ld.lld` |
| Guest debugger | `/opt/homebrew/bin/arm-none-eabi-gdb` |

Ensure CLion's toolchain environment includes `/opt/homebrew/bin` in `PATH` so
CMake can find LLD and Ninja. The compiler's ARM target comes from the CMake
toolchain file; selecting the macOS compiler executable does not make this a
macOS target. There is no target macOS sysroot or host C++ standard library.

The prepared inputs must exist at `.symbian/gui-sdk/include`,
`.symbian/gui-sdk/euser/euser.dso` and `.symbian/gui-sdk/ws32/ws32.dso` beneath the
repository. They already exist in this workspace. On a fresh checkout, follow
WALKTHROUGH.md sections 2–3 first. For alternate SDK locations, override both
`SYMBIAN_GUI_SDK_INCLUDE` and `SYMBIAN_IMPORT_PROXIES` in an ignored
`CMakeUserPresets.json`; both proxies must match the staged SDK profile.

From a terminal the same project can be checked with:

```sh
cd /Users/helena/dev/symbian/examples/gui_app
cmake --preset symbian-pic
cmake --build --preset symbian-pic
```

This configures `.symbian/cmake/gui-app` and builds an ELF there. The real ARM
compilation database includes the source SDK declarations and target macros.
Configure/build passed, and LLVM clangd parsed/indexed `app.cc` with zero errors
using this database and `--tweaks=ExpandAutoType`. The actual IDE target model also resolves the GUI sources, as recorded
above. If the warning persists, check the active project's
source directory, enabled profile and CMake errors before clearing caches.

## Installed Run and Debug buttons

Use the **GUI project window**, opened at `examples/gui_app`, with the
`clion-arm` CMake profile. Its local run configurations are now installed:

* Select **GUI Run**, then click **Run**. This is a native CLion CMake run
  configuration with an explicit host Python launcher, so no registered Python
  SDK is required. The launcher builds the current source into ELF/E32, copies
  the preserved golden into a fresh instance and runs the counter. The guest's
  Exit button completes a normal exit; IDE Stop reaps only this owned frontend.
* Select **GUI Debug** and the native debug profile **Symbian GUI GDB**, then
  click **Debug**. This is CLion Remote Debug with the ARM GDB supervisor as
  its debugger executable and `127.0.0.1:24689` as its target. It builds/publishes
  before connection, starts a fresh halted instance, waits for its listener and
  then starts the actual ARM GDB in the IDE's machine-interface mode. Set a
  breakpoint in `GuiMain` or `DrawGui` and use Resume. If the new debug profile
  is absent, reopen the GUI project to load its local settings.

The IDE version in use enables native Debug Profiles separately from toolchains.
The installed `.idea/debug-profiles.xml` provides the supervisor there; setting
only the older Remote Debug configuration's debugger field is insufficient.
The dedicated GUI project's saved current profile selects Symbian GUI GDB;
the repository project's existing host-debugger selection is preserved. Run
and Debug use different run configurations. Debugging GUI Run would debug the
host launcher rather than the ARM guest.

The GDB hook relocates symbols after remote connection using the actual kernel
mapping and the native parser's link-time code base, before IDE breakpoints.
It also applies both source mappings. This removes the manual symbol-slide
console step for GUI Debug. The emulator listener opens before the GUI maps,
so determining the runtime mapping before connecting is too early. A busy port
is rejected rather than connecting the IDE to an unrelated instance.

The golden is never booted. Each run retains `launch.json`, input/binary digests,
frontend/kernel logs and a copied instance beneath `.symbian/gui-runs`. Normal
exit retains native final status; debug connection retains `gdb-mapping.json`.
The short private `/tmp` control directory is removed on launcher completion.
Ending GDB or stopping Run cleans up the owned child; stopped debug sessions do
not claim normal guest exit. Retained copies consume disk space and are not yet
a managed snapshot/retention service. The supported scope is the existing
RM-807 Delight research fixture and GUI example, with the guarded ABI profile.

To regenerate the ignored run/debug settings in this prepared workspace:

```sh
cd /Users/helena/dev/symbian
uv run symbian emu configure-ide
```

This installs the GUI configurations and supervisor, preserving other native
debug profiles. It replaces the generated GUI Run/GUI Debug configurations.
It saves GUI Run as the dedicated GUI project's default selection. It does not
configure the SDK inputs or import the CMake profile. The foreground
Run command can also be exercised outside the IDE:

```sh
uv run python -m symbian.emulator.launch --root /Users/helena/dev/symbian
```

Real integration checks exercise rendering/input/normal exit, cancellation and
child reaping, occupied debug ports, relocated source breakpoints/variables,
and the GDB machine interface including instruction stepping across Thumb/ARM.
The actual IDE target model is verified; clicking the IDE toolbar and its
complete debugger frontend/stack unwinding remain separate, unautomated checks.

## Keep retained runtime data out of project analysis

The root project had more than two million files in its content model after
retaining copied firmware for experiments. The dedicated GUI project scans
535 files and still resolves its two C++ sources and one assembly source. Its
small source root is the preferred editing context. Root-project local settings
now save exclusions for `.symbian`, `research/upstream`, `build`, `out` and
`.venv`, with `.symbian/gui-sdk` unexcluded. Live application of those root
exclusions has not been verified; the GUI model is verified. If they have not
loaded, use **Mark Directory As → Excluded** on the private artifact directories
and keep the staged SDK headers available. See
[JetBrains' project analysis guide](https://www.jetbrains.com/help/clion/project-analysis.html).

## Publish the E32 after edits

GUI Run and GUI Debug publish automatically. CLion's CMake Build action by
itself updates the ELF. Use the platform command for manual conversion,
check reproducibility and publish the actual executable for the emulator:

```sh
cd /Users/helena/dev/symbian
uv run symbian build --project examples/gui_app --output .symbian/gui-app
```

Add this as an external tool named `Build GUI E32`: program
`/opt/homebrew/bin/uv`, arguments
`run symbian build --project examples/gui_app --output .symbian/gui-app`, working
directory `/Users/helena/dev/symbian`. Repeat package creation after an executable
change if using the SIS installer. A CMake ELF build alone does not update the
published E32 or an installed guest copy. Keep the repository's `.clang-format`.

## Use the existing compilation database instead

For immediate navigation, **File → Open**
`/Users/helena/dev/symbian/.symbian/gui-app/compile_commands.json`, then choose
**Open as Project**. Under **Tools → Compilation Database → Change Project
Root**, select `/Users/helena/dev/symbian` to recover the repository tree.
Reload the database after a platform build changes it. This provides the GUI's
real compilation context; other host files require their own database/project.
Full builds require a custom build target or the external tool above. See
[JetBrains' database guide](https://www.jetbrains.com/help/clion/compilation-database.html)
and [custom targets](https://www.jetbrains.com/help/clion/custom-build-targets.html).
Do not add a host-compiled dummy GUI target to silence the warning: SDK integer
sizes, calling conventions and platform macros come from the ARM toolchain.

## Manual remote debugging without the installed supervisor

Start a disposable emulator as described in WALKTHROUGH.md section 9, with
`enable-gdb-stub: true`, the loopback port and the guarded RM-807 profile. Use
**Run → Edit Configurations → Remote Debug**, select the ARM GDB executable,
set `target remote` arguments to `127.0.0.1:24689` (or the selected port) and
symbol file to `/Users/helena/dev/symbian/.symbian/gui-app/gui_app.elf`.
Map `/symbian-src/gui_app` to the local example and `/symbian-sdk/include` to
`.symbian/gui-sdk/include`. This configuration connects to an already running
stub; guest deployment is handled separately. See
[JetBrains' Remote Debug guide](https://www.jetbrains.com/help/clion/remote-debug.html).

After connecting while halted, apply the actual symbol slide in GDB's console
before setting breakpoints. For the digest-pinned current example/ROM fixture:

```gdb
symbol-file -o 0x6fff8000 /Users/helena/dev/symbian/.symbian/gui-app/gui_app.elf
set substitute-path /symbian-src/gui_app /Users/helena/dev/symbian/examples/gui_app
set substitute-path /symbian-sdk/include /Users/helena/dev/symbian/.symbian/gui-sdk/include
break GuiMain
continue
```

The slide is runtime code base minus ELF segment base. Recalculate it for a
changed mapping; do not assume the fixture's number applies to another build.
Clear stale breakpoints if CLion placed them before symbol relocation. Source
breakpoints, variables and instruction stepping pass the standalone ARM GDB
regression. CLion's debugger frontend and full stack unwinding remain untested.
The guest ELF is an ARM symbol file; a normal macOS Run configuration cannot
execute it. The macOS EKA2L1 process and the guest are separate debug targets.
