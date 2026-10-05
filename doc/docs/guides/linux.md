# Prepare a Linux development host

Use Linux for host tools, Python bindings and ARM cross-compilation. The
emulator frontend also needs Qt and compatible multimedia libraries.
Firmware is imported separately. PySide6 availability depends on the Python
version and host architecture; use the CLI if the Console's renderer is absent.

## 1. Install dependencies

For the installed host tools, use Python 3.11–3.14 and a virtual environment:

```sh
sudo apt update
sudo apt install python3 python3-venv
python3 -m venv ~/.venvs/symbian
source ~/.venvs/symbian/bin/activate
pip install symbian-platform
symbian doctor
```

The wheels support x86_64 and aarch64 with glibc 2.28 or later. The native host
libraries are included; building applications also requires the native target
SDK. Follow [Getting started](../getting-started.md) for SDK selection.

### Building from a source checkout

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

[Install the native SDK archive](native-distributions.md), then follow
[Create a standalone application](projects.md). Native archives need glibc 2.39
or later and include LLVM, CMake, Ninja and resource tools. You can also build
the SDK from the prepared source checkout. The CMake toolchain selects
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

## 4. USB devices

Use `symbian device list` to find the connected phone. If descriptor inspection
or MTP transfer reports insufficient USB permissions, grant access to Nokia USB
devices for the active desktop user with a udev rule:

```sh
sudo tee /etc/udev/rules.d/70-symbian-nokia.rules >/dev/null <<'EOF'
SUBSYSTEM=="usb", ATTR{idVendor}=="0421", TAG+="uaccess"
EOF
sudo udevadm control --reload-rules
```

Disconnect and reconnect the phone, then list it again. This rule uses Nokia's
USB vendor ID; adapt it for another manufacturer. A mounted mass-storage volume
also needs normal filesystem write permissions. Close a desktop MTP browser
before using the SDK's MTP session on the same phone. See
[Connect a device](device.md) and the [complete application installation
sequence](building.md#6-stage-the-signed-package-on-the-phone).
