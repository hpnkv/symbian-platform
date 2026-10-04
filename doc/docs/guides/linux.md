# Prepare a Linux development host

Linux support is **provisional**. Host native libraries, Python extensions and
an installed wheel have passed bounded Linux aarch64 checks. This project has
not yet completed an interactive Linux EKA2L1 GUI run, guest GDB session,
Console visual check or physical-device run. Use the evidence levels in the
[status record](https://github.com/hpnkv/symbian-platform/blob/main/.dev/status.md)
when interpreting a result.

On Linux aarch64, the host wheel does not bring a verified Console GUI
renderer: the CI environment could not resolve PySide6. Use the command-line
tools there until a renderer is tested on a real host.

## 1. Prepare the host

Follow the **Linux** tab in [Prepare the toolchain](source-prerequisites.md).
It lists an Ubuntu-style tool set, the isolated static-dependency bootstrap,
`uv sync` and the first ARM object probe. You need CMake 3.28+, Ninja and a
matching Clang/LLD toolchain. SDK export discovers `clang++`, its matching
`clang`, `ld.lld`, `llvm-ar` and `llvm-ranlib` from `PATH`; set
`SYMBIAN_LLVM_BIN` to a single LLVM bin directory when your distribution
installs several versions.

For host-only native checks:

```sh
export SYMBIAN_DEPS_PREFIX="$PWD/.symbian/host-deps"
scripts/bootstrap_wheel_deps.sh
cmake --preset debug
cmake --build --preset debug --parallel 4
ctest --preset debug
uv run pytest -q
```

These commands use the same CMake/Ninja and static-prefix pattern as
[A11](https://github.com/hpnkv/a11). `build/debug/compile_commands.json`
records host compiler commands. The [host build guide](host-build.md) explains
wheel construction and installed-wheel audit.

## 2. Build an application

Once a source SDK is prepared and installed, follow [Create a standalone
application](projects.md). CMake's `symbian-arm.cmake` selects a freestanding
ARM target independently of the Linux host. It retains
`compile_commands.json`; inspect the compile command for
`--target=armv6-none-eabi` or your selected ARMv5T target.

An ARM object, ELF or E32 image validates a different boundary from execution.
[Build and inspect](building.md) shows the artifact checks. Do not infer that
a Linux-built image will load on a phone or even in the Linux emulator until
those runs are recorded.

## 3. Prepare emulator work

The [research emulator build](../reference/emulator-source-build.md) has a
provisional Linux x86_64 tab. Its pinned CMake target emits
`build/eka2l1/bin/eka2l1_qt`; aarch64 Linux still needs a matching FFmpeg
build recipe. A local firmware image is imported separately through the
[firmware guide](firmware.md). The SDK's disposable launch and guest-debug
wrappers accept a Linux emulator path and can discover `gdb-multiarch` when
`arm-none-eabi-gdb` is unavailable, but their end-to-end Linux behavior is an
open validation gate.

Keep any first Linux run's compiler versions, SDK manifest, firmware identity,
frontend log, guest exit report and CPU backend. Record host failures as host
failures rather than attributing them to Symbian or the device. A Linux
emulator pass still does not prove Nokia 808 compatibility.
