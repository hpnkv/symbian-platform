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

The shared guest presets follow the active SDK on each CMake configure. After
exporting a new SDK, reload the profile. If an older CMake cache still selects
a host compiler or macOS `/usr/bin/ld`, run
`cmake --fresh --preset clion-guest-probes-armv6` once. The toolchain pins the
selected SDK's ARM compiler and LLD before CMake checks the compiler.
`CMakeUserPresets.json` remains an ignored place for local compiler paths.
`MBEDTLS_SOURCE` should point to this repository's
`third_party/mbedtls-symbian` source and headers.

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
