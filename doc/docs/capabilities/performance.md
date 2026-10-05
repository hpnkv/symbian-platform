# Runtime performance considerations

Keep event-thread work short, queues bounded and ownership explicit. Emulator
speed depends on its CPU backend and host; measure on the target device before
using emulator timings to choose a production configuration.

## Allocation and image size

The guest runtime defaults to mimalloc over process-owned `RChunk` pages. It
reserves an 8 MiB virtual arena and caps total chunk address space at 64 MiB by
default; `SYMBIAN_MIMALLOC_ADDRESS_BUDGET_MIB` accepts 16–512 MiB. Reserved
address space and committed physical memory are different costs. The alternative
`SYMBIAN_RUNTIME_MIMALLOC=OFF` profile uses the Symbian heap.

Ordinary allocation failure terminates with `KErrNoMemory`; nothrow allocation
returns null. Bound input and buffer sizes before constructing containers.
Reuse buffers for file copying, network bodies and other repeated work.
Link only required components: final image size depends on reachable functions,
imports and runtime profile, rather than static archive size alone.

## Choose an execution model

| Mechanism | Cost to consider | Application guidance |
| --- | --- | --- |
| Inline Future continuation | Work runs on the registering or completing thread | Keep callbacks short; marshal UI changes to the event owner |
| Worker continuation | Queue, result transfer and OS-thread wakeup | Use for blocking I/O and computation |
| Fiber | Stack memory and cooperative switches | Size the stack for the workload; blocking calls still block its OS thread |
| Native timer | OS handle and Promise state per pending timer | Reuse the event loop; keep an admission limit |
| Property watch | Handle and Future state per outstanding subscription | Rearm deliberately and drain cancellation before close |
| Event mailbox | Callback storage and dispatch turns | Bound admission and the work performed in one turn |

`TimerPump` admits 64 pending timers by default and `EventMailbox` admits 128
callbacks. These limits do not bound completed Futures retained by application
code or arbitrary continuation fan-out. Tune them for memory and responsiveness.
A worker fiber defaults to a 16 KiB stack; TLS handshakes can require a larger
stack. See the [concurrency guide](concurrency.md).

## Clocks, atomics and formatting

Use `absl::Time` and `absl::Duration` for deadlines. For short measurements,
check the reported frequency, direction and power cost before using a native
fast counter. The SDK steady clock uses a native tick source; its resolution
depends on the device.

Default 64-bit atomics serialize through a process-wide `RFastLock`. The optional
native atomic profile requires matching EUSER exports and can have different
contention costs. Use the standard runtime `is_lock_free()` query rather than
assuming an ARMv6 compiler target makes the linked implementation lock-free.

String streams, error messages and status payloads can allocate. Keep formatting
and diagnostics out of hot redraw or sample-processing loops when possible.
When comparing implementations, measure cold startup separately from warmed
runs and include peak memory, allocation failures, cancellation and UI latency.
