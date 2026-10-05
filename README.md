# Symbian platform

Build Symbian ARMv5T and ARMv6 applications with modern C++20, CMake and Ninja
on macOS or Linux. The SDK provides native libraries, Python tools, E32/SIS
conversion, emulator controls and guest debugging.

[Documentation](https://hpnkv.github.io/symbian-platform/) ·
[Create a project](doc/docs/guides/projects.md) ·
[C++ API](https://hpnkv.github.io/symbian-platform/cpp/index.html) ·
[Original Symbian headers](https://hpnkv.github.io/symbian-platform/cpp/platform/index.html)

## What you can build

| Area | Available facilities |
| --- | --- |
| Toolchain | Clang/LLD ARM builds, native ELF-to-E32 conversion, frozen-ordinal import proxies, EXEs and DLLs |
| C++ runtime | libc++ containers, smart pointers, clocks, allocation and compiler-rt helpers; Abseil Status/StatusOr and maps |
| Concurrency | Futures, Tasks, channels, fibers, native timers, property watches and event/worker executors following A11 |
| Device APIs | Counters, power/display snapshots, streaming files and directories, camera discovery and native TCP |
| HTTP | Streaming HTTP/1.1 and HTTP/2 clients and servers with bounded buffers and absolute deadlines |
| WebSockets | RFC 8441 WebSockets using shared HTTP/2 and stream primitives |
| TLS | Application-linked Mbed TLS 3.4.1, TLS 1.2/1.3 streams, explicit CA roots and certificate/hostname verification |
| Packaging | SIS creation, registration, translated menu captions, SVG icons and RSA/X.509 signatures |
| Host tools | Python CLI, desktop console, USB inspection, package staging and disposable emulator sessions |
| EKA1 | Separate ARMv5T legacy process profile with selected original GNU2 EUSER imports |

The modern runtime, GUI and networking APIs require EKA2. EKA1 does not yet
provide those libraries or legacy SIS packaging. TLS requires a secure entropy
source for the actual target; the supplied emulator adapter is specific to its
patched firmware profile. Camera capture, modern sensor/media APIs, general C++
TLS and thrown C++ exceptions are unsupported. See the
[capability guide](doc/docs/capabilities/index.md) for component requirements.

Firmware and original OS DLL implementations are supplied separately and are
not redistributed. An import library describes a system DLL's exports; it does
not replace that DLL. Test your application's services and installation policy
on its target device before deployment.

## Start from source

Use the [macOS/Linux prerequisites](doc/docs/guides/source-prerequisites.md)
and [host build guide](doc/docs/guides/host-build.md) to prepare dependencies.
From the checkout:

```sh
uv sync
uv run symbian doctor
uv run symbian toolchain probe
uv run symbian sdk install ~/dev/symbian-sdk --workspace "$PWD"
uv run symbian init ~/dev/hello_time --name hello_time --non-interactive
uv run symbian app build --project ~/dev/hello_time
```

SDK export needs the prepared platform and runtime sources described in the
[source walkthrough](doc/docs/guides/from-source.md). Once the tools are installed
in your environment, invoke `symbian` directly. To run an application, first
[import local firmware](doc/docs/guides/firmware.md), then use
`symbian app run --project ~/dev/hello_time`.

The project version comes from [VERSION](VERSION). Release packaging is being
prepared for macOS/Linux on x86_64 and arm64, Python 3.11–3.14 wheels, a Python
sdist, a source archive and standalone SDK distributions. Full EKA1 library
coverage and complete native distribution packaging are still in progress;
they are not implied by the current host wheel build.

## Examples and guides

| Task | Guide |
| --- | --- |
| Create an IDE-ready application | [Standalone projects](doc/docs/guides/projects.md) |
| Build, inspect and package an application | [Building](doc/docs/guides/building.md), [packaging](doc/docs/guides/packaging.md) |
| Fetch a web page, set a header or serve a response | [Native HTTP client and server](doc/docs/guides/http.md) |
| Open a WebSocket connection | [Native WebSockets](doc/docs/guides/websocket.md) |
| Load CA roots and use TLS 1.2/1.3 | [TLS client/server examples](doc/docs/guides/tls.md) |
| Use native TCP | [Connectivity API](doc/docs/capabilities/apis/connectivity.md) |
| Understand the guest runtime and threading | [Runtime](doc/docs/capabilities/runtime.md), [concurrency](doc/docs/capabilities/concurrency.md) |
| Build a Window Server GUI and debug the guest | [Build a real GUI app](doc/docs/guides/from-source.md), [CLion](doc/docs/guides/clion.md) |
| Build a legacy EKA1 executable | [EKA1 process profile](doc/docs/guides/eka1.md) |
| Prepare a Linux development machine | [Linux host guide](doc/docs/guides/linux.md) |
| Inspect USB or stage a package on a phone | [Device guide](doc/docs/guides/device.md) |
| Run the authenticated read-only development agent | [Agent guide](doc/docs/guides/agent-emulator.md) |

Application examples live in [examples/](examples/); focused ABI, runtime and
network diagnostics live in [probes/](probes/). The probes are runnable examples
for checking specific platform contracts, rather than user-oriented apps. The native reference covers
SDK symbols and eleven original Symbian headers; symbol descriptions are being
expanded with links to hosted platform documentation. New generated descriptions use attribution markers in the reference. Local builds, firmware, private device data,
external source checkouts and emulator state stay outside version control.

## Community foundations and licensing

This project builds on [EKA2L1](https://github.com/EKA2L1/EKA2L1),
[SymbianSource](https://github.com/SymbianSource),
[SymbianRevive](https://github.com/SymbianRevive),
[A11](https://github.com/hpnkv/a11),
[LLVM](https://github.com/llvm/llvm-project),
[Abseil](https://github.com/abseil/abseil-cpp) and
[mbedtls-symbian](https://github.com/shinovon/mbedtls-symbian).
See [community credits](doc/docs/credits.md) for their contributions.

For separately supplied ROMs and firmware, see EKA2L1's
[Important Links](https://eka2l1.miraheze.org/wiki/Important_Links) and
[Delight](https://www.symwld.com/delight/), the community firmware project.
[Symbian World](https://www.symwld.com/) collects applications and resources;
[NNProject](https://nnproject.cc/) develops mobile applications and utilities;
the [Symbian World Telegram community](https://t.me/symbian_world) hosts
discussion and support. The curated
[hstsethi](https://github.com/hstsethi/awesome-symbian) and
[gauravssnl](https://github.com/gauravssnl/awesome-symbian) Awesome Symbian lists
link to software, tools and development resources.

Project code is licensed under [Apache 2.0](LICENSE). Third-party sources and
adaptations retain their original licenses and notices, including the separate
GPL-licensed emulator integration and EPL-licensed platform material.
