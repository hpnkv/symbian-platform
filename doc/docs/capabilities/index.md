# Capability map

The platform has a modern host toolchain and an intentionally bounded guest
runtime. Link only the application libraries you use; the generated SDK
provides architecture-specific targets for ARMv5T and ARMv6.

| Area | Current development evidence | Read next |
| --- | --- | --- |
| E32 builds and DLLs | Native conversion, format checks and selected emulator execution | [Building](../guides/building.md), [SDK](../reference/sdk.md) |
| C++ runtime | Selected libc++, allocation, global lifetime and concurrency paths | [Guest runtime](runtime.md), [C++ usage](cpp.md) |
| Device APIs | System, power, display, storage and camera archives with bounded probes | [Device APIs](device-apis.md) |
| TLS | Vendored Mbed TLS sources, project CA resource and selected guest crypto/X.509 calls | [TLS guide](../guides/tls.md) |
| Deployment | Native SIS writer and disposable emulator install checks | [Packaging](../guides/packaging.md) |

!!! warning "Open gates"

    Guest TLS 1.2/1.3 handshakes, secure entropy, socket transport and Nokia
    808 compatibility have not passed their development gates. Planned APIs
    are marked as such on their individual pages.

The [C++ reference](../cpp.md) is generated from native declarations. It
shows available symbols; the guides state which behavior has been exercised.
