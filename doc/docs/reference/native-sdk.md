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
| `Symbian::Connectivity` | `symbian/api/connectivity/tcp_client.h`, `tcp_listener.h`, `active_tcp_listener.h` | Connect, listen, accept and exchange bounded IPv4 TCP data | Synchronous worker owners plus a single-request active-object listener; deadline cancellation for blocking accept, send and receive. |
| `Symbian::Crypto` | `mbedtls/md.h`, `mbedtls/entropy.h` | Use opt-in Mbed TLS cryptographic primitives without a TLS socket | Links only `libmbedcrypto`; SDK supplies OS secure entropy where supported; applications own key storage. |
| `Symbian::Http` | `symbian/api/connectivity/http.h` | Streaming HTTP/1.1 and HTTP/2 client/server | Worker-owned exchange, bounded body streams and absolute deadlines. |
| `Symbian::WebSocket` | `symbian/api/connectivity/websocket.h` | RFC 8441 WebSocket connections | Shared HTTP/2 transport with bounded messages and stream backpressure. |
| `Symbian::Tls` | `symbian/api/connectivity/tls_stream.h` | TLS 1.2/1.3 client/server streams | Caller supplies trust roots and peer identity; SDK supplies supported OS secure entropy. Synchronous worker only. |
| `Symbian::Agent` | `symbian/agent/guest_control.h`, `guest_log.h` | Parse bounded read-only control messages and retain a 32-record service log | Authenticate the peer before parsing; this codec does not own a service or grant permissions. |
| `Symbian::GLES1` | `GLES/gl.h`, `GLES/glext.h` | GLES 1.1 Common fixed-function rendering | Original `libglesv1_cm.dll` imports; firmware supplies the implementation. |
| `Symbian::GLES2` | `GLES2/gl2.h`, `GLES2/gl2ext.h` | GLES 2.0 shaders and programmable rendering | Original `libglesv2.dll` imports; firmware supplies the implementation. |
| `Symbian::EGL` | `EGL/egl.h`, `EGL/eglext.h` | EGL 1.4 contexts and native surfaces | Original `libegl.dll` imports; a window surface retains its native `RWindow` until destroyed. |
| `Symbian::PortableZlib` | `zlib.h`, `zconf.h` | Compression and decompression | Optional zlib 1.3.1 static archive built for both ARM profiles; Open C and guest runtime follow transitively. |

Graphics targets supply original platform headers and complete frozen ordinal
imports when linked. They support EKA2 ARMv5T and ARMv6 with soft-float AAPCS.
Configuration checks the consumer's transitive graph, SDK files and ABI flags.
Direct GLES1/GLES2 imports in one binary overlap; use one per binary or obtain
context-specific entry points through EGL. Separate DLLs can use different APIs.
Select conditional graphics links with CMake `if()` so the graph is concrete at
configuration time; unresolved link generator expressions are rejected.

`SYMBIAN_GRAPHICS_AVAILABLE_APIS` selects the APIs known to exist in your
deployment firmware (default: `EGL;GLES1;GLES2`, the SDK-supported set).
`SYMBIAN_GRAPHICS_FIRMWARE_DIR` optionally names its Z-drive directory and checks
required DLL presence during configuration. Unknown firmware and GPU details
remain unknown. Context configurations, shader limits and extensions still need
runtime queries. The [GL cube guide](../guides/gl-app.md) demonstrates per-fragment
Phong shading, animation and pointer input against a black background.

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

`PackGuestHelloResult` declares version-one control limits and read-only
operations. The service requires hello before other requests; the codec has
no connection state. `GuestStatusSnapshot` groups optional tick and display
readings for the agent
codec. Query `Symbian::System` and `Symbian::Display` on a worker after TLS
authentication, fill only successful readings and call the two-argument
`PackGuestResult`. The one-argument overload preserves the basic result.
Tick counts wrap; display geometry is a HAL observation rather than Window
Server layout. The [system](../capabilities/apis/system.md) and
[display](../capabilities/apis/display.md) guides describe those APIs.
`AgentLogRing` retains 32 fixed records on one worker and returns at most eight
after a sequence cursor. `AgentLogPage.gap` signals overwritten records;
`PackGuestLogResult` wraps that page in the same authenticated control
envelope. `AgentLogRecord` carries a process-relative steady-clock reading and
numeric severity; neither is a wall clock or OS log source. See the
[protocol guide](agent-protocol.md#service-local-event-log)
for code meanings and retention.

An `ActiveTcpListener` requires an installed original Symbian
`CActiveScheduler`. It holds one pending `RSocket::Accept` with no polling
timer. Its observer receives one `TcpClient` or a typed error and explicitly
calls `AcceptNext()` when ready for another connection. `Stop()` cancels and
drains the pending native request before freeing the socket and session.
Keep the observer brief; the synchronous `TcpClient` methods are intended for
a worker thread, not for long transfers or a TLS handshake in `RunL()`.
If that worker owns an accepted socket, call `EnableWorkerSharing()` before
`ListenIpv4()`; the Socket Server session must become shareable before its
sockets open.

## Guest concurrency and TLS

`Symbian::Stackless` supplies the bounded Future/Task path used by generated
starters. A guest event thread should perform short work and schedule blocking
queries or transfers on a worker. A cancellation request is complete only
when the producer publishes its terminal result and releases native buffers.
The [concurrency guide](../capabilities/concurrency.md) explains the available
profiles and their limits.

`MbedTLS::mbedtls` is an optional C archive target, with matching
`MbedTLS::mbedx509` and `MbedTLS::mbedcrypto` components. The SDK vendors the
full source and exposes its `symbian_mbedtls` platform and socket BIO headers.
Its default trust set is empty: an application chooses a project-local CA
bundle or explicit pinning. The [TLS guide](../guides/tls.md) shows CMake
configuration, peer verification and transport ownership.

`Symbian::Tls` adds the SDK's C++ `TlsStream` owner; `TlsServer` is an alias.
`Connect()` verifies a server using explicit roots, hostname, TLS version and ALPN.
`Create()` parses caller-supplied PEM server credentials and client CA roots.
Select `TlsVersion::kTls12` or `TlsVersion::kTls13` there: this Mbed TLS server
needs one version per listener. `Accept()` requires a verified client
certificate.
`Read()` and `Write()` hold a single 32 KiB-or-smaller operation under an
absolute deadline and drain an expired native socket request. The owner
handles one stream at a time and resets it with `CloseSession()`. It belongs
on a worker thread; a service must arrange cancellation, pairing and key
custody around it. The SDK supplies the supported OS secure RNG; absent or
unsupported native entropy contracts fail closed.
For a framed control request, pass one absolute request deadline into every
`Read()` and `Write()` call. Creating a new deadline per call lets a peer extend
the exchange by sending one byte before each expiry.

Deep native call chains can exhaust the worker's normal 16 KiB fiber stack.
`WorkerExecutor::PostFiber(work, stack_bytes)` accepts a word-aligned 4 KiB to
1 MiB stack, allocated for the live job. The emulator agent uses 256 KiB for
Mbed TLS. Set `max_outstanding` on the worker to cap queued plus active work;
check the returned task for admission errors. This size is an emulator
observation, not a measured Nokia 808 memory recommendation.

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
| `symbian::agent` | `cpp/symbian/agent/frame.h`, `control.h` | Bounded length framing, inbound queue accounting and typed MessagePack control envelopes for the device protocol. The host `symbian::agent_frame` target is available. The guest uses the smaller `Symbian::Agent` codec. |
| `symbian::device` | `cpp/symbian/device/usb.h` | Inspect serial-matched USB interfaces and stage one checked SIS through MTP. |

`symbian::device::StageMtpSis` is a host operation. It takes a serial-derived
anchor, checked host package path, safe content-addressed filename and expected
SHA-256. It selects a writable MTP `Installs` folder, uploads at most 16 MiB,
and reads the object back before returning its storage ID and object handle.
The result also counts pre-existing `Installs` children whose metadata the
phone refused to return. Those entries are skipped without deletion; a new
upload still requires a matching readback digest. MTP failures and skipped
handles are reported through Abseil `LOG()` in the host library.
The Python `symbian.device.mtp.stage_sis` wrapper exposes the same operation;
`symbian.device.installation.stage_package` chooses it when no mounted staging
volume is available. Neither API asks the handset to install the SIS.

The host Python binding exposes `pack_agent_read_request`,
`agent_control_payload_length` and `parse_agent_result_frame`. Each runs its
native validation with the GIL released. The
[read-only host session](agent-protocol.md#host-api-and-cli) wraps them
with a keyed challenge response and a typed result; application code does not need to
decode MessagePack itself.

These calls use `absl::Status` or `absl::StatusOr`; an unsupported format
profile returns an error instead of being guessed. `InspectImage` and
`InspectPackage` validate their documented subsets. Neither is a universal
oracle for every historical image. See [SDK exports](sdk.md) for the installed
artifacts and [E32/SIS capabilities](../capabilities/index.md) for available facilities.

## Original OS declarations

For a direct EUSER, Window Server, File Server, HAL or ECam call, open the
[original Symbian header guide](native-symbian.md) and its separate
[Doxygen index](../cpp/platform/index.html). Check that the selected firmware exports each required symbol and provides
the corresponding service.

## Owned C++ contracts and header checks

Mutable lvalue parameters use `T* absl_nonnull`; optional pointers use
`absl_nullable`. Original OS and upstream signatures retain their contracts.
Owned buffer helpers prefer writable `std::span`, and text helpers use
`std::string_view` with adaptations at native API boundaries. The SDK exports
Abseil annotation headers through each owning target's public dependencies.

The normal host and guest builds include `symbian_header_canaries`, grouped
by owning library and ABI. Every owned header compiles in a separate translation
unit with exceptions disabled; Python boundary headers explicitly enable them.
Owned C port headers also compile in C mode. SDK export runs installed-header
checks for ARMv5T and ARMv6. A consuming SDK validation project can enable them:

```cmake
include(SymbianSdkHeaderCanaries)
symbian_sdk_header_canaries()
```

Run `python scripts/check_cpp_style.py` for sources, examples, templates and
C++ documentation snippets. Run `python scripts/simplify_cpp_dereferences.py
--check` to check redundant dereference parentheses without changing required
forms such as `(*buffer)[index]`.
