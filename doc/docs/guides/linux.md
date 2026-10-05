# Prepare a Linux development host

Use Linux for host tools, Python bindings and ARM cross-compilation. The
emulator frontend also needs Qt and compatible multimedia libraries.
Firmware is imported separately. PySide6 availability depends on the Python
version and host architecture; use the CLI if the Console's renderer is absent.

## 1. Install dependencies

Follow the **Linux** tab in [Prepare tools and source](source-prerequisites.md)
for Ubuntu dependency installation, static host libraries and the Python
source environment. Use CMake 3.28+, Ninja 1.12+ and matching LLVM tools.
SDK export discovers `clang++`, `clang`, `ld.lld`, `llvm-ar` and `llvm-ranlib`
from `PATH`; set `SYMBIAN_LLVM_BIN` to one LLVM bin directory when multiple
versions are installed. The pinned guest runtime needs Clang 23.

For host native and Python tests, from the checkout:

```sh
export SYMBIAN_DEPS_PREFIX="$PWD/.symbian/host-deps"
scripts/bootstrap_wheel_deps.sh
cmake --preset debug
cmake --build --preset debug --parallel 4
ctest --preset debug
uv run pytest -q
```

`build/debug/compile_commands.json` records compiler commands. See the
[host build guide](host-build.md) for wheel construction and installed checks.

## 2. Build an application

Prepare and install the source SDK, then follow
[Create a standalone application](projects.md). The CMake toolchain selects
an ARM target independently of the host. Its compilation database contains
`--target=armv6-none-eabi` or the selected ARMv5T triple.

[Build and inspect](building.md) describes the ELF and E32 outputs.
A build does not execute the application; use the emulator or target device
to check runtime behavior.

## 3. Build and run the emulator

Follow [Build the emulator](../reference/emulator-source-build.md) for the
Linux x86_64 dependencies and commands. Its frontend is
`build/eka2l1/bin/eka2l1_qt`. Linux aarch64 requires a separate compatible
FFmpeg build; the x86_64 script cannot be reused unchanged.

Import local firmware using the [firmware guide](firmware.md). SDK launch and
guest-debug wrappers accept the Linux executable and discover `gdb-multiarch`
when `arm-none-eabi-gdb` is absent. See [Run and Debug](clion-run-debug.md).

For unattended Qt sessions on a host without a display, use `xvfb-run -a`
with an owned disposable instance. Inspect the guest exit report as well as
the frontend log when diagnosing failures.
