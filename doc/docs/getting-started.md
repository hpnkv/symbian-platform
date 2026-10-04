# Getting started

Start with the host tools, then create an application. You need a local
firmware image only when you are ready to run it in an emulator.
On Linux, follow the [provisional host path](guides/linux.md) alongside these
steps; interactive emulator and guest-debugger checks remain open there.

## A few names you will see

| Name | Role in a Symbian application workflow |
| --- | --- |
| E32 | The executable or DLL image the Symbian loader opens |
| SIS | An installable package containing an app and its resources |
| ROM/Z | System software and read-only files supplied by a device firmware image |
| [EKA2L1](https://github.com/EKA2L1/EKA2L1) | A community emulator for testing selected Symbian behavior without a phone |
| Window Server | The system service a GUI app uses to create windows and receive redraw and input events |

The usual path is **C++ source → ARM build → E32 image → SIS package → emulator
check**. Each step has its own output and validation. A successful emulator
check is useful development evidence; physical compatibility is measured
separately.

## 1. Install the host tools

```sh
uv sync
uv run symbian doctor
uv run symbian toolchain probe
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
