# CLion source indexing and manual debugging

The dedicated `examples/gui_app` project is the best place to edit the counter:
its CMake model contains the guest sources without scanning retained firmware
and emulator copies. The root project also has host tools and research probes;
keep `.symbian`, `research/upstream`, `build`, `out` and `.venv` excluded from
ordinary IDE content indexing while retaining the staged SDK headers.

## Publish an E32 after manual edits

**GUI Run** and **GUI Debug** build and publish automatically. If you press
CLion's ordinary CMake **Build** action, it updates the ELF target; publish the
E32 separately when launching by hand:

```sh
cd /path/to/symbian-platform
symbian build --project examples/gui_app --output .symbian/gui-app
```

Point `/path/to/symbian-platform` to your checkout. If you prefer an IDE
external tool, run the same command from the repository root. Packaging must
be repeated after changing the executable if you use the SIS installer. The
[build guide](gui-build.md) explains the ELF/E32 distinction.

## Open a compilation database directly

CLion can open `.symbian/gui-app/compile_commands.json` as a project. Choose
**File → Open**, select the database and choose **Open as Project**. Set the
project root to the repository under **Tools → Compilation Database → Change
Project Root**. Reload it after rebuilding. This gives editor navigation the
actual ARM flags, although a compilation database alone does not define Run
and Debug actions. See [JetBrains' database guide](https://www.jetbrains.com/help/clion/compilation-database.html).

Do not introduce a macOS compiled dummy target for guest source files: it
would supply incorrect pointer sizes, calling conventions and platform macros.

## Index guest projects from the repository root

If you open the repository root in CLion, use its `clion-guest-probes-armv6`
or `clion-guest-probes-armv5t` profile. CMake discovers registered application
projects in the root and `examples/`, including `agent_service` and `gui_app`,
and exposes their real ARM targets. The SDK's device API, TLS and guest
concurrency libraries are loaded from their component CMake files in this
profile too, so implementation files such as `tls_server.cc` belong to
compilable targets. Any remaining project-owned native files under `cpp/symbian`,
`agent_service` and `examples` join an automatic indexing target. The host
profile indexes host-owned files under `cpp`, including `cpp/python` and
`cpp/tests`; guest-only sources stay with their ARM profile. A new source file
therefore appears in the IDE after CMake reload; its component target remains
the build authority. `symbian_probe_index` gives each probe a separate ARM
source target. New probe directories receive a source target automatically;
specialized probes retain their declared compiler settings.

Root guest presets use repository source inputs and ignore installed SDK
selection. Source preparation chooses LLVM through `SYMBIAN_LLVM_BIN` or
its normal host discovery; select matching Clang 23+, LLD and archive tools.
`CMakeUserPresets.json` remains an ignored place for local build settings.
Host and guest profiles have separate build directories and compiler caches.
Do not run two CMake configurations into the same build directory at once.

The root guest profile is currently validated with CMake 3.31.10 in the
IntelliJ IDEA 2026.2 CLion plugin. CMake 4.4.3 can finish configuration but
emit incomplete File API `abstractTargets` records for two Abseil header-only
targets; this plugin then reports `target-absl_non_temporal_memcpy-...json: no
backtrace` and refuses to load the model. Select CMake 3.31.10 under
**Settings → Build, Execution, Deployment → Toolchains** for the toolchain
named by the local guest preset, then reload CMake. The plugin used its
toolchain's CMake 4.4.3 even when `cmakeExecutable` named 3.31.10 in the
preset. Do not alternate CMake versions in one build directory during an IDE
reload. A successful configure line alone does not establish that CLion
accepted the File API model.
If CMake reports `Guest probe indexing requires the symbian-arm.cmake cross
toolchain`, inspect the command in the IDE CMake log. Host compiler overrides
such as `/usr/bin/clang` can leave an existing guest build cache identifying
Darwin. Select the **Symbian ARM** toolchain, restart the IDE so it loads that
saved toolchain setting, remove the ignored `build/guest-probes-armv6` cache,
and reload the guest preset. A fresh ARM cache records `Generic` as its system
and `armv6-none-eabi` as its C++ compiler target.

To check configuration, repeat reloads and actual source builds for both guest
architectures and the host GUI target:

```sh
python scripts/check_source_workspace.py --workspace "$PWD" \
  --output build/workspace-check --host
```

The check deliberately supplies unusable installed-SDK settings, so it also
checks that the source workspace remains independent of installation defaults.

Registered applications created by `symbian init` attach project-owned native
files anywhere in their source tree to the actual guest executable target.
Build output and separately managed dependencies are excluded. CMake watches
for new files, so an IDE reload updates target membership and compile commands.

## Attach ARM GDB manually

The [guest debug walkthrough](gui-debug.md) shows how to start a disposable
emulator with its loopback GDB stub. In CLion, choose **Run → Edit
Configurations → Remote Debug**, select an ARM capable GDB executable, set the
remote address to the stub (the prepared example uses `127.0.0.1:24689`),
and select the local `gui_app.elf` as its symbol file. Map
`/symbian-src/gui_app` to your local `examples/gui_app` and
`/symbian-sdk/include` to your staged SDK headers. The generated **GUI Debug**
configuration handles these details automatically; use this manual route to
inspect a different guest image or mapping. See [JetBrains' Remote Debug
reference](https://www.jetbrains.com/help/clion/remote-debug.html).

For a manual session, calculate the symbol slide from the guest's actual
runtime code base minus the ELF segment base. Apply it before expecting source
breakpoints to resolve. Do not copy a slide from another firmware, run or
build. The SDK supervisor records load mappings in `.symbian/gui-runs`.
Guest stack unwinding can be incomplete. A macOS debugger
attached to the EKA2L1 process is a different target from ARM GDB attached to
the guest executable.
