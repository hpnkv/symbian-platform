# Symbian platform

Build Symbian ARM applications with CMake and Ninja, inspect E32 and SIS files,
and test supported behavior in disposable emulator instances. The SDK ships
native C++ libraries, Python tooling, example projects and the full source of
its application-linked Mbed TLS port.

<div class="grid cards" markdown>

-   :material-rocket-launch: **Start a project**

    Follow [getting started](getting-started.md), then create or open a
    [standalone application](guides/projects.md).

-   :material-tools: **Build and run**

    Use the [build guide](guides/building.md),
    [firmware guide](guides/firmware.md) and
    [emulator controls](guides/emulator-control.md).

-   :material-library: **Use the SDK**

    Review [C++ capabilities](capabilities/index.md),
    [device APIs](capabilities/device-apis.md) and the
    [generated C++ reference](cpp.md).

-   :material-shield-lock: **Add TLS**

    Link Mbed TLS explicitly and provision a
    [project-local CA bundle](guides/tls.md).

</div>

!!! note "Evidence boundary"

    Emulator results and ARM builds establish bounded development behavior.
    They do not prove compatibility with a Nokia 808. Guest TLS handshakes,
    secure entropy and physical-device validation remain open.

The [project status](https://github.com/hpnkv/symbian-platform/blob/main/.dev/status.md)
and [research log](https://github.com/hpnkv/symbian-platform/blob/main/.dev/research-log.md)
record exact evidence and unanswered questions.

This work uses [EKA2L1](https://github.com/EKA2L1/EKA2L1) for emulator
checks and vendors the full
[mbedtls-symbian](https://github.com/shinovon/mbedtls-symbian) port for optional
application TLS. See [community credits](credits.md) for the other projects
and preserved platform sources that make the SDK possible.
