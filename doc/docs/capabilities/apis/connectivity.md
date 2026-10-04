# Native TCP client

The SDK's `Symbian::Connectivity` target provides a small IPv4 TCP client.
It uses Symbian's `RSocketServ` and `RSocket` services, which own a socket
server session and a connected socket. Link the target in an ARM application:

```cmake
target_link_libraries(my_app PRIVATE Symbian::Connectivity)
```

For a generated project's `symbian.toml`, add these two entries to its
existing `[project].import_proxies` array so E32 conversion can map the
original DLL ordinals:

```toml
"${sdk}/proxies/esock/esock.dso",
"${sdk}/proxies/insock/insock.dso",
```

```cpp
#include <array>
#include <cstdint>
#include "symbian/api/connectivity/tcp_client.h"

auto opened = symbian::api::connectivity::TcpClient::ConnectIpv4(
    {127, 0, 0, 1}, 39094);
if (opened.ok()) {
  std::array<std::uint8_t, 1> request{'N'};
  absl::Status sent = opened->Send(request);
}
```

`ConnectIpv4` returns a move-only client. `Send` and `Receive` accept caller
buffers up to 32 KiB and return Abseil statuses for native errors. An empty
send succeeds; an empty receive returns zero. Keep the client, calls and
destruction on the same worker thread. Each call waits for a native request;
the current helper has no in-flight cancellation or deadline. It is useful
for bounded application work and diagnostics, while the resident development
agent still needs an asynchronous listener and cancellable TLS owner.

The implementation keeps the original `RSocketServ`, `RSocket`, `TInetAddr`
and descriptor types in a native bridge. The public header exposes ordinary
C++ arrays, spans and statuses. The SDK includes selected original ESOCK and
internet socket headers, plus proxies for their numbered DLL exports. See
the [original header reference](../../reference/native-symbian.md) when an
application needs an OS operation beyond this helper.

## Evidence and limits

An ARMv6 consumer linked through the exported SDK target and ran in a
disposable RM-807 emulator instance on Dynarmic and Dyncom. The host received
its request byte and the guest received the host reply. A wrong reply
produced a distinct failure exit. The test kept SHA-256-pinned ROM, EUSER,
ESOCK and INSOCK files unchanged. This establishes a bounded emulator path;
it does not prove Nokia 808 compatibility, TLS authentication, cancellation,
radio behavior or an application listener.

The separate connection monitor discussed in the [development plan](https://github.com/hpnkv/symbian-platform/blob/main/.dev/plan.md)
remains planned. Opening this TCP client can request network connectivity;
it should not be used as a passive bearer observer.
