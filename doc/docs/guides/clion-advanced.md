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
uv run symbian build --project examples/gui_app --output .symbian/gui-app
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

## Index every root guest probe

If you open the repository root in CLion, use its `clion-guest-probes-armv6`
or `clion-guest-probes-armv5t` profile for native guest probes. The root
`symbian_probe_index` target compiles the declared probe sources with ARM
headers; it does not combine their different entry points into one program.
The connectivity probe is included through `symbian_index_connectivity`.

`CMakeUserPresets.json` is an ignored, machine-local file. If a profile says
the SDK lacks `Connectivity` or `Stackless`, check its
`SYMBIAN_SDK_PREFIX` value against the directory of the active `sdk.json`.
An old prefix can point to an earlier SDK even after you export a new one.
Update that local value, then reload CMake. `MBEDTLS_SOURCE` should point to
this repository's `third_party/mbedtls-symbian`, which contains the vendored
source and headers.

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
build. The SDK supervisor records mapping evidence in `.symbian/gui-runs`.
The checked example has passed source breakpoint and instruction stepping
controls; full IDE stack unwinding remains an open check. A macOS debugger
attached to the EKA2L1 process is a different target from ARM GDB attached to
the guest executable.
