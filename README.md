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

[Explore the documentation](https://hpnkv.github.io/symbian-platform/) ·
[Create a project](doc/docs/guides/projects.md) ·
[Review current evidence](.dev/status.md)

## In this README

- [Capabilities](#capabilities)
- [Start here](#start-here)
- [Examples and workflows](#examples-and-workflows)
- [Documentation map](#documentation-map)
- [Development status](#development-status)

## Capabilities

| Area | What is available | Scope |
| --- | --- | --- |
| Toolchain | CMake/Ninja builds, ARM ELF to E32 conversion, import proxies and inspection | ARMv5T and ARMv6 profiles |
| SDK | Native C++ libraries, architecture-specific CMake targets and source provenance | Link components explicitly |
| Runtime | Selected libc++, allocation, concurrency and device API paths | Bounded guest tests; [details](doc/docs/capabilities/index.md) |
| Packaging | Native SIS writer, metadata inspection and disposable emulator installation | Phone installation remains a separate gate |
| TLS | Vendored Mbed TLS 3.4.1 source, static targets and project-local CA bundle packaging | Guest handshakes and secure entropy remain open |
| Tools | Python CLI, desktop console, firmware onboarding and emulator controls | Firmware is supplied separately |

The [capability map](doc/docs/capabilities/index.md) distinguishes implemented
features from planned APIs. The [generated C++ reference](doc/docs/cpp.md)
lists native declarations; its companion guides explain support limits.

## Start here

Install the host dependencies, then check the toolchain. The object probe does
not require firmware or an installed target SDK.

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
| Package a private CA for one app | [TLS and CA bundles](doc/docs/guides/tls.md) |
| Prepare firmware for local emulator work | [Firmware guide](doc/docs/guides/firmware.md) |
| Build and test this repository | [Host build guide](doc/docs/guides/host-build.md) |

The runnable examples live in [`examples/`](examples/). Local builds,
firmware, upstream checkouts and emulator state stay outside version control.

## Documentation map

- [Guides](doc/docs/getting-started.md) cover setup, projects, CLion, packaging,
  TLS, the desktop console and device connection.
- [Capabilities](doc/docs/capabilities/index.md) explain the C++ runtime,
  concurrency and device APIs, including planned surfaces.
- [Reference](doc/docs/reference/sdk.md) describes SDK exports; Doxygen
  generates the [C++ API](doc/docs/cpp.md) from native source.
- [Engineering records](.dev/plan.md) hold plans, experiments, status and
  unresolved questions. They are public project records rather than API
  documentation.

## Development status

The supplied RM-807 firmware has supported guarded emulator checks for GUI
rendering and input, runtime paths, and selected Mbed TLS crypto/X.509 calls.
Authenticated guest TLS 1.2/1.3 handshakes, secure entropy, emulator network
transport and Nokia 808 compatibility remain open. See the
[status record](.dev/status.md) and [research log](.dev/research-log.md) for
specific results and unanswered questions.
