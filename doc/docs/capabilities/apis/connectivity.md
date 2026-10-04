# Native TCP sockets

The SDK's `Symbian::Connectivity` target provides a small IPv4 client and
listener. They use Symbian's `RSocketServ` and `RSocket` services to own socket
server sessions and streams. Link the target in an ARM application:

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
destruction on the same worker thread. Each call waits for a native request.
All three accept an absolute `absl::Time deadline`, defaulting to
`absl::InfiniteFuture()`. Choose a finite deadline at call sites where a
bounded wait matters, such as `absl::Now() + absl::Minutes(1)`. On expiry they
cancel and drain the native request before returning a deadline status. In the
emulator, a client reused its socket after a timed-out read and received the
host's later byte. A send can already be delivered before its deadline fires;
the application protocol must acknowledge work when delivery matters.

## Accept one host connection

```cpp
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "symbian/api/connectivity/tcp_listener.h"

auto listener = symbian::api::connectivity::TcpListener::ListenIpv4(
    {127, 0, 0, 1}, 39096);
if (listener.ok()) {
  auto client = listener->Accept(absl::Now() + absl::Seconds(5));
  // Check client.ok() before using the connected stream.
}
```

The listener binds only the supplied IPv4 address, with backlog one.
`Accept` accepts an absolute `absl::Time` deadline, cancels and drains a pending
native accept on expiry, and returns a deadline-exceeded status. The same
listener can accept again afterwards. The accepted client keeps the
socket-server session alive even if the listener closes. Both owners remain
synchronous and belong on one worker thread.

## Accept while the event thread is idle

`ActiveTcpListener` wraps one native `CActive` accept. Construct it on a thread
with an installed `CActiveScheduler`, implement `TcpAcceptObserver`, and call
`AcceptNext()` from the observer when ready for another connection. The
pending accept does not poll. `Stop()` cancels and drains it.

When the observer hands a client to an SDK worker, call
`EnableWorkerSharing()` **before** `ListenIpv4()`. This makes the Socket Server
session shareable before its sockets open. Keep `OnAccept()` short: post the
move-only client to `Symbian::Stackless`'s `WorkerExecutor`, then rearm. The
worker can call `Send`, `Receive`, or the `Symbian::Tls` owner. The
[development agent source](https://github.com/hpnkv/symbian-platform/tree/main/agent_service)
shows this pattern for its emulator profile. Its private phone profile uses
`BroadcastProbe` on a worker to find a console that answers a keyed UDP
probe, then `ConnectIpv4` with a deadline to open outbound TCP. The
probe accepts byte spans of at most 64 bytes and returns the IPv4 source of an
exact reply. The caller must authenticate that reply before trusting its
address; the agent does so with HMAC-SHA256.

The implementation keeps the original `RSocketServ`, `RSocket`, `TInetAddr`
and descriptor types in a native bridge. The public headers expose ordinary
C++ arrays, spans, deadlines and statuses. The SDK includes selected original ESOCK and
internet socket headers, plus proxies for their numbered DLL exports. See
the [original header reference](../../reference/native-symbian.md) when an
application needs an OS operation beyond this helper.

## Evidence and limits

An ARMv6 consumer linked through the exported SDK target and ran in a
disposable RM-807 emulator instance on Dynarmic and Dyncom. The host received
its request byte and the guest received the host reply. A wrong reply
produced a distinct failure exit. The test kept SHA-256-pinned ROM, EUSER,
ESOCK and INSOCK files unchanged. This establishes a bounded emulator path;
it does not prove Nokia 808 compatibility or radio behavior. A separate
inbound test connected from the host to a Dynarmic guest listener, exchanged
bytes after the listener closed, and verified two accept timeouts on a second
listener. This is a synchronous worker path, not a resident service loop.

The separate connection monitor discussed in the [development plan](https://github.com/hpnkv/symbian-platform/blob/main/.dev/plan.md)
remains planned. Opening this TCP client can request network connectivity;
it should not be used as a passive bearer observer.
