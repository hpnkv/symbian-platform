# Capability map

The SDK provides host tools and application-linked native libraries for ARMv5T
and ARMv6. Link only the components your application uses.

| Area | Available facilities | Read next |
| --- | --- | --- |
| Build tools | ARM ELF to E32 conversion, frozen-ordinal import proxies, executable and DLL inspection | [Building](../guides/building.md), [SDK](../reference/sdk.md) |
| C++ runtime | libc++ containers, ownership, clocks, compiler-rt helpers and runtime profiles | [Guest runtime](runtime.md), [C++ usage](cpp.md) |
| Concurrency | Futures, Tasks, channels, fibers, timers, property watches and event/worker executors | [Concurrency](concurrency.md) |
| Device APIs | Counters, power and display snapshots, file streaming, camera discovery and TCP sockets | [Device APIs](device-apis.md) |
| Application UI | Native Window Server applications; a guest Qt 4.8.1 widget example with separately supplied Qt DLLs | [GUI app](../guides/from-source.md), [Qt app](../guides/qt-app.md) |
| HTTP and WebSockets | Streaming HTTP/1.1 and HTTP/2 client/server; RFC 8441 WebSockets | [HTTP](../guides/http.md), [WebSockets](../guides/websocket.md) |
| TLS | Mbed TLS 1.2/1.3 sessions with explicit trust roots and peer verification | [TLS guide](../guides/tls.md) |
| Packaging | SIS generation, application resources, signatures and emulator installation | [Packaging](../guides/packaging.md) |
| EKA1 | Separate legacy executable startup and selected EUSER imports | [EKA1 profile](../guides/eka1.md) |

The modern runtime and device libraries require the EKA2 application ABI.
EKA1 has a smaller, separate profile. TLS uses the SDK-provided OS secure RNG where available and fails closed
where the native contract is unsupported. Sensors and media have no exported modern API yet.

Use the [C++ reference](../cpp.md) for symbol details and the linked guides
for each component's requirements and restrictions.
