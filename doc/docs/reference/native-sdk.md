# SDK native API guide

The native reference has two distinct audiences. **Guest** code becomes a
32-bit ARM Symbian application and calls the SDK's public C++ headers or
verified OS imports. **Host** code runs on the development computer and builds,
inspects or packages guest artifacts. Start with the
[generated SDK API index](../cpp/index.html) for declarations and use this
guide to choose the correct component, ownership model and result policy.

## Guest application components

An installed SDK publishes a CMake target only when its architecture-specific
archive is present. Link the component you use; the target brings in its
required Abseil status/runtime profile and, where needed, an OS import proxy.

| Target | Public header | Main operation | Result and lifetime |
| --- | --- | --- | --- |
| `Symbian::System` | `symbian/api/system/counters.h` | Read tick and fast counters | `StatusOr`; counts wrap and are not wall clock time. |
| `Symbian::Power` | `symbian/api/power/power.h` | Read a power snapshot | `StatusOr`; unsupported observations remain empty. |
| `Symbian::Display` | `symbian/api/display/display.h` | Read primary HAL geometry | `StatusOr`; one snapshot, separate from Window Server layout. |
| `Symbian::Storage` | `symbian/api/storage/storage.h` | Open, read, write or copy files | Move-only handles; use and destroy on the opening thread. |
| `Symbian::Camera` | `symbian/api/camera/camera.h` | Discover camera slots | `StatusOr`; discovery does not reserve a camera. |
| `Symbian::Connectivity` | `symbian/api/connectivity/tcp_client.h` | Connect and exchange bounded IPv4 TCP data | Move-only, synchronous worker owner; no in-flight cancellation. |

For example, a display query can live in a small adapter:

```cmake
target_link_libraries(my_app PRIVATE Symbian::Display)
```

```cpp
#include "symbian/api/display/display.h"

absl::StatusOr<symbian::api::display::DisplayGeometry> ReadLayoutInput() {
  return symbian::api::display::ReadPrimaryDisplayGeometry();
}
```

The caller checks the returned status before reading dimensions. For storage,
pass an absolute UTF-16 Symbian path; the native File Server still enforces
drive permissions and the application's data cage. `WritableFile::Open`
requires an explicit create, open or replace mode. `FileCopy::Step` transfers
one bounded chunk at a time and keeps its operation on the opening thread.
Read the [storage guide](../capabilities/apis/storage.md) before choosing its
file ownership pattern.

## Guest concurrency and TLS

`Symbian::Stackless` supplies the bounded Future/Task path used by generated
starters. A guest event thread should perform short work and schedule blocking
queries or transfers on a worker. A cancellation request is complete only
when the producer publishes its terminal result and releases native buffers.
The [concurrency guide](../capabilities/concurrency.md) explains the verified
profile and its limits.

`MbedTLS::mbedtls` is an optional C archive target, with matching
`MbedTLS::mbedx509` and `MbedTLS::mbedcrypto` components. The SDK vendors the
full source and exposes its `symbian_mbedtls` platform and socket BIO headers.
Its default trust set is empty: an application chooses a project-local CA
bundle or explicit pinning. The [TLS guide](../guides/tls.md) shows CMake
configuration and the current runtime acceptance boundary. A compiled TLS
archive is not evidence of a guest handshake.

## Host format libraries

The host native layer owns binary format parsing and publication; Python
bindings call it rather than duplicating format rules. In the
[Doxygen reference](../cpp/index.html), start with these namespaces:

| Namespace | Header | Use |
| --- | --- | --- |
| `symbian::e32` | `cpp/symbian/e32/e32.h` | Convert supported ARM ELF profiles and inspect supported E32 images. |
| `symbian::sis` | `cpp/symbian/sis/sis.h` | Build bounded unsigned SIS packages and inspect that profile. |
| `symbian::analysis` | `cpp/symbian/analysis/*.h` | Read bounded ELF, attributes and checksum inputs. |
| `symbian::emulator` | `cpp/symbian/emulator/*.h` | Native firmware/control parsing for owned emulator sessions. |
| `symbian::agent` | `cpp/symbian/agent/frame.h`, `control.h` | Bounded length framing, inbound queue accounting and typed MessagePack control envelopes for the planned device protocol. The host `symbian::agent_frame` target is available; a guest SDK export is pending. |

These calls use `absl::Status` or `absl::StatusOr`; an unsupported format
profile returns an error instead of being guessed. `InspectImage` and
`InspectPackage` validate their documented subsets. Neither is a universal
oracle for every historical image. See [SDK exports](sdk.md) for the installed
artifacts and [E32/SIS capabilities](../capabilities/index.md) for evidence.

## Original OS declarations

For a direct EUSER, Window Server, File Server, HAL or ECam call, open the
[original Symbian header guide](native-symbian.md) and its separate
[Doxygen index](../cpp/platform/index.html). A historical declaration is a
starting point for an import and runtime check. A function name in a header
does not establish that the selected firmware exports it or that the Nokia
808 has been validated.
