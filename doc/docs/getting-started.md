# Getting started

Start with the host tools, then create an application. You need a local
firmware image only when you are ready to run it in an emulator.
On Linux, follow the [Linux host guide](guides/linux.md) alongside these steps.

## A few names you will see

| Name | Role in a Symbian application workflow |
| --- | --- |
| E32 | The executable or DLL image the Symbian loader opens |
| SIS | An installable package containing an app and its resources |
| ROM/Z | System software and read-only files supplied by a device firmware image |
| [EKA2L1](https://github.com/EKA2L1/EKA2L1) | A community emulator for testing selected Symbian behavior without a phone |
| Window Server | The system service a GUI app uses to create windows and receive redraw and input events |

The usual path is **C++ source → ARM build → E32 image → SIS package → emulator
check**. Test the required services and installation policy on your target
device before deployment.

## 1. Install the host tools

Use Python 3.11–3.14 on macOS 15 or later, or Linux with glibc 2.28 or later,
on x86_64 or arm64. Install the distribution in a virtual environment:

```sh
python3 -m venv ~/.venvs/symbian
source ~/.venvs/symbian/bin/activate
python -m pip install --upgrade pip
pip install symbian-platform
symbian doctor
```

The distribution provides the `symbian` command and native host libraries.
On Ubuntu, install `python3-venv` if `python3 -m venv` is unavailable.
For application builds, [install the native SDK archive](guides/native-distributions.md)
for your host. It includes the ARM compiler and build tools; no source checkout
is needed. Native Linux archives require glibc 2.39 or later. Firmware and the
emulator are configured separately for emulator execution.

If you want to compile the SDK itself, follow
[the source guide](guides/source-prerequisites.md).

### Working from a source checkout

In the repository root, install and activate the development environment:

```sh
uv sync
source .venv/bin/activate
symbian doctor
symbian toolchain probe
```

`doctor` reports available compilers and tools. The probe checks that the
toolchain can make an ARM object. Its output stays under ignored `.symbian/`
build state.

## 2. Create an application

Follow [Create a standalone application](guides/projects.md) to install the
SDK, make a small GUI project and build it. The guide introduces the files you
will edit. You can return here for the next task.

## 3. Choose a next task

| Goal | Guide |
| --- | --- |
| Build E32 and inspect it | [Building applications](guides/building.md) |
| Prepare the complete SDK from source | [Source walkthrough](guides/from-source.md) |
| Launch a supplied ROM/Z in the emulator | [Firmware and emulator](guides/firmware.md) |
| Use C++ libraries and device APIs | [Capabilities](capabilities/index.md) |
| Package a project CA bundle | [TLS guide](guides/tls.md) |

The [desktop console](guides/console.md) exposes the supported host workflows
and bounded device inspection from one window.
