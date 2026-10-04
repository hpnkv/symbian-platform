# Symbian platform

A modern development workspace for Symbian applications. Build ARMv5T and
ARMv6 executables and DLLs with CMake and Ninja, inspect E32 and SIS files,
and exercise supported behavior in disposable emulator instances. The SDK
includes C++ libraries, Python tooling, examples and the complete source of
its application-linked Mbed TLS port.

The project exists to make Symbian development inspectable and repeatable with
current host tools. It keeps firmware and private device data outside the
repository and records where emulator evidence ends. An ARM build or emulator
pass does **not** establish Nokia 808 compatibility.
Linux host builds are available with provisional application and emulator
instructions; interactive Linux guest runs and debugging remain to be checked.

[Explore the documentation](https://hpnkv.github.io/symbian-platform/) ·
[Create a project](doc/docs/guides/projects.md) ·
[Review current evidence](.dev/status.md)

## In this README

- [Capabilities](#capabilities)
- [How the build works](#how-the-build-works)
- [Start here](#start-here)
- [Examples and workflows](#examples-and-workflows)
- [Documentation map](#documentation-map)
- [Community foundations](#community-foundations)
- [Development status](#development-status)

## Capabilities

| Area | What is available | Scope |
| --- | --- | --- |
| Toolchain | CMake/Ninja builds, ARM ELF to E32 conversion, import proxies and inspection | ARMv5T and ARMv6 profiles |
| SDK | Native C++ libraries, architecture-specific CMake targets and source provenance | Link components explicitly |
| Runtime | Selected libc++, allocation, concurrency and device API paths | Bounded guest tests; [details](doc/docs/capabilities/index.md) |
| Packaging | Native SIS writer, metadata inspection and disposable emulator installation | Phone installation remains a separate gate |
| TLS | Vendored Mbed TLS 3.4.1 source, static targets and project-local CA bundle packaging | Authenticated TLS 1.2/1.3 passed in the emulator; physical-device trust and entropy remain open |
| Development agent | Manually started, authenticated read-only status and bounded service logs | Loopback-only emulator profile; [agent source](agent_service/) and [run guide](doc/docs/guides/agent-emulator.md) |
| Tools | Python CLI, desktop console, firmware onboarding and emulator controls | Firmware is supplied separately |

An **E32 image** is the executable or DLL format loaded by Symbian. A **SIS
package** is the installable container for an application and its resources.
The [GUI example](doc/docs/tutorials/gui-app.md) uses Symbian's **Window
Server**, the system service that manages windows, drawing and input events.

The [capability map](doc/docs/capabilities/index.md) distinguishes implemented
features from planned APIs. The [generated C++ reference](doc/docs/cpp.md)
lists native declarations; its companion guides explain support limits.

## How the build works

The application starts as C++ source and a CMake project. Clang produces ARM
objects, the platform tools produce an E32 image, and the packager can place
that image in a SIS file. [EKA2L1](https://github.com/EKA2L1/EKA2L1) is the
community Symbian emulator used for disposable runtime checks. It runs with
firmware and system files supplied separately; it is a testing environment,
not a substitute for a phone result.

The [build guide](doc/docs/guides/building.md) walks through each artifact.
The [emulator guide](doc/docs/guides/firmware.md) explains where firmware fits.

## Start here

Install the host dependencies using the [source preparation guide](doc/docs/guides/source-prerequisites.md)
or its [Linux path](doc/docs/guides/linux.md), then check the toolchain. `uv` creates the
Python environment and runs the `symbian` command. The first ARM object probe
does not require firmware or an installed target SDK.

```sh
uv sync
uv run symbian doctor
uv run symbian toolchain probe
```

From there, follow [getting started](doc/docs/getting-started.md) or open the
[desktop console](doc/docs/guides/console.md) with `uv run symbian console`.

## Examples and workflows

| If you want to… | Start with… |
| --- | --- |
| Create an IDE-ready application | [Standalone projects](doc/docs/guides/projects.md) |
| Build and inspect an E32 image | [Build an application](doc/docs/guides/building.md) |
| Build the source SDK and touch-counter GUI | [Source walkthrough](doc/docs/guides/from-source.md) and [GUI example](doc/docs/tutorials/gui-app.md) |
| Exercise guest C++ allocation and containers | [Runtime probe](doc/docs/tutorials/runtime-probe.md) |
| Connect a native Symbian TCP client | [Connectivity API](doc/docs/capabilities/apis/connectivity.md) and [example](examples/connectivity_probe/) |
| Package a private CA for one app | [TLS and CA bundles](doc/docs/guides/tls.md) |
| Start the read-only development agent | [Development agent in the emulator](doc/docs/guides/agent-emulator.md) |
| Prepare firmware for local emulator work | [Firmware guide](doc/docs/guides/firmware.md) |
| Build and test this repository | [Host build guide](doc/docs/guides/host-build.md) |
| Prepare a Linux host | [Provisional Linux guide](doc/docs/guides/linux.md) |

The runnable examples live in [`examples/`](examples/). Local builds,
firmware, upstream checkouts and emulator state stay outside version control.

## Documentation map

- [Guides](doc/docs/getting-started.md) cover setup, projects, CLion, packaging,
  TLS, the desktop console and device connection.
- [Capabilities](doc/docs/capabilities/index.md) explain the C++ runtime,
  concurrency and device APIs, including planned surfaces.
- [Reference](doc/docs/reference/sdk.md) describes SDK exports; Doxygen
  generates the [C++ API](doc/docs/cpp.md) from native source.
- [Engineering records](.dev/plan.md) hold plans, status and
  unresolved questions. They are public project records rather than API
  documentation.

## Community foundations

This repository builds on work by several projects. Their code and original
licenses remain attributed in the source and SDK provenance.

| Project | Contribution here |
| --- | --- |
| [EKA2L1](https://github.com/EKA2L1/EKA2L1) | Symbian emulator used for bounded loader, runtime and GUI tests |
| [mbedtls-symbian](https://github.com/shinovon/mbedtls-symbian) and [Mbed TLS](https://github.com/Mbed-TLS/mbedtls) | Symbian TLS port and upstream cryptographic library; the full port source is vendored and adapted for this SDK |
| [SymbianSource](https://github.com/SymbianSource) and [SymbianRevive](https://github.com/SymbianRevive) | Preserved platform sources, headers and build-tool references |
| [A11](https://github.com/hpnkv/a11), [Abseil](https://github.com/abseil/abseil-cpp) and [LLVM](https://github.com/llvm/llvm-project) | Concurrency reference, status/runtime libraries and modern ARM compilation |

The [credits and provenance guide](doc/docs/credits.md) gives the exact role
of each dependency and links to its source.

## Development status

The supplied RM-807 firmware has supported guarded emulator checks for GUI
rendering and input, runtime paths, native TCP, and authenticated guest TLS
1.2/1.3 handshakes. A manually started resident agent answers read-only
status and log requests over an authenticated, unencrypted socket on emulator
loopback. The SDK's Mbed TLS support remains available to applications.
Phone-specific agent packaging and a Wi-Fi status check are implemented, while
on-device installation, local pairing confirmation, physical-device entropy and
Nokia 808 compatibility remain open. See the
[status record](.dev/status.md) and [research log](.dev/research-log.md) for
specific results and unanswered questions.
