# Runtime performance considerations

The host callback pool now uses A11's original 256-slot MPMC ring with a
mutex-protected overflow path and wakes an OS worker only when one is parked.
`Post` avoids a fiber stack; `PostAt` stores a steady-clock deadline after one
wall-time conversion. These are implementation facts, not measured speedups.
Benchmark callback throughput, ring overflow frequency, timer-queue insertion,
idle wakeups and shutdown latency against A11's full work-stealing pool.
The host custom scheduler caps idle parks at 50 ms and can drop CPython's GIL
for the park. Measure that cap's idle-wakeup cost and the Python/native handoff
latency before tuning it; the current tests cover progress and ordering, not
throughput. A11's hot-worker recruitment, context stealing and pooled-stack
optimizations are still missing from the host adaptation.

The first ARM/Thumb swap now executes from the SDK archive, but its tests
measure correctness and heap-cell balance only. Benchmark switch latency,
register-save cost, 16 KiB stack commitment, high-water use and debugger
visibility against raw callbacks and stackless continuations before exposing
fibers. The test stack is allocated and freed once; real fiber creation and
reaping costs are still unknown.

The guest Future/Task profile now transports original `absl::StatusOr<T>`
values. This removes a separate SDK result representation, but may change
small-result size, callback-copy cost and first-use code size. Measure these
against raw native `TRequestStatus` and the earlier bounded result profile;
the passing ARM execution controls establish correctness, not a speedup.
`TimerPump` rounds positive Abseil durations up to the next microsecond and
retains one steady-clock read per absolute deadline. Measure timer admission,
inline continuation cost and UI latency under saturation before increasing
its default 64 pending entries.
The ordinary generated starter admits one timer Task per tap, retains its
Future until ready and posts one event-mailbox callback for the delayed row.
The counter example uses the same request-semaphore loop for a 300-ms marker.
Both passed guest correctness controls, but neither measures scheduling cost.
Compare input-to-first-redraw latency and heap-cell/handle peaks against the
portable starter with the same taps, including rapid Clear/cancellation and
idle periods. The modern profile also imports `libpthread.dll`; quantify cold
load and image-size cost separately from timer wakeup cost.
The property-change request adds one OS handle and one Future state per
outstanding subscription; re-subscription and cancellation invoke selected
EUSER proxy calls. Measure allocations and wakeups against a raw
`RProperty::Subscribe` loop. The bounded event mailbox copies/moves
`std::function` targets and scans a short vector; benchmark throughput,
tail UI latency, saturation and reentrant turns before changing its default
128-entry admission limit. Both features have execution evidence, but no
comparative latency measurements yet. The future fiber backend must measure
stack commitment and context-switch cost separately; do not infer its
performance from this stackless path.

This is a measurement notebook for the SDK-supplied guest runtime, not a
performance guarantee. Keep execution correctness, memory limits and UI
responsiveness ahead of a microbenchmark win. Results from Dynarmic or Dyncom
do not establish physical-phone speed. Record target architecture, ROM/Z
profile, compiler and linker versions, emulator backend, warm/cold state and
input size with every result.

| Area | Established behavior | Worth testing; direction is not yet established |
| --- | --- | --- |
| Heap allocation | `new` uses the current Symbian `User::Alloc` heap; ordinary failure exits with -4 and nothrow returns null. The container probe checks balanced allocation cells after repeated destruction, not latency. | Allocation/free latency and fragmentation across small objects, strings, vectors, DLL boundaries and secondary-thread heaps. A bounded arena or mimalloc backend may help small allocations, but page sourcing, cross-thread free, peak committed memory, DLL unload and failure policy must work before replacing the default. |
| Native page source | The new bridge creates one process-owned `RChunk` handle and one metadata cell per page-multiple request. Same-thread alignment and cleanup execute on the guest. | Measure handle count, committed memory, fragmentation and create/close latency for Abseil's typical 64-KiB request size. Verify cross-thread release and memory-pressure behavior before adopting it as an allocator backend. |
| Abseil thread ID | A Symbian guard avoids unsupported ELF TLS in Abseil's thread-ID cache by using its existing `GetTID()` fallback (`pthread_self()` on the current guest target). | Measure repeated thread-ID reads in synchronization-heavy paths against a future validated TLS implementation. The fallback trades an unsupported relocation for an extra call. |
| Byte atomic exchange | The SDK exposes EUSER's ordered 8-bit swap through the compiler libcall; the guest checks old/new values. | Measure contention and ordering across real OS threads before claiming a throughput gain over a lock. |
| Native page source | A bounded `RChunk` probe queries native page size, then creates, writes, grows and closes 16 process-owned local chunks on both ARM targets and emulator backends. This is a correctness result for that path, not an allocator or throughput claim. | Verify commit/decommit, concurrent ownership, exhaustion and cleanup. Compare chunk creation cost and fragmentation with `User::Alloc`; Abseil's low-level allocator cannot assume OpenC's file-backed `mmap` provides anonymous pages. |
| Abseil LowLevelAlloc | A clean A11-pinned source replay executes a 130,000-byte allocation and explicit arena lifetime on RM-807/Dynarmic. It uses one process-owned `RChunk` owner per page request; the normal/changed result controls pass. | Compare small and large allocation latency, chunk count, fragmentation, handle pressure and memory footprint with `User::Alloc`. Measure cross-thread release and low-memory behavior before considering it as a default. |
| Guest Abseil Status and maps | Pinned Status/StatusOr, Cord payloads and `flat_hash_map` now execute in both architecture/backend matrices from source and a sealed SDK target. The hash seed's unsupported ELF TLS path uses a process-wide atomic sequence. EUSER byte and 32-bit bitwise atomics supply the required libcalls. These are correctness results. | Record payload and map allocation counts, rehash latency, first-use cost, archive/code size and import overhead. Compare process-wide seed contention with a future validated native TLS path, and EUSER atomic calls with any proven inline profile. Measure across threads before treating the tested single-thread map path as a concurrency guarantee. |
| C++ atomics | ARMv5T's observed `__atomic_*` and ARMv6's `__sync_*` 32-bit libcalls are backed by original EUSER operations. A parent and secondary thread complete 4,000 shared increments on both ARM profiles and emulator backends. The default 64-bit bridge uses one process-wide `RFastLock`; an alternate native profile calls EUSER's 64-bit operations directly. RM-807, RM-675 and RM-609 ROM entries use LDREXD/STREXD loops and the cross-thread native probe passes on both emulator backends. RM-243/RM-346 lack the required EUSER ordinals. Dyncom needed a STREXD fix. These are correctness results, not throughput results. | Measure call/import, retry and fence costs against the lock-backed profile under several contention levels. Inspect each selected ROM before calling its 64-bit path lock-free: original ARMv5/V6 source also has an interrupt-masked implementation. Targeting ARMv6 alone does not guarantee inline lock-free code: Clang emitted libcalls. |
| Locks | The original `RFastLock` create, uncontended poll, held poll and wait/signal path executes on the named ROM. | Measure uncontended and contended latency, kernel fallback, fairness and UI stalls. A11 `thread::Mutex` must park contended fibers through its backend; blocking an event OS thread would suspend all its runnable fibers. |
| Guest channels | `thread::Channel<T>` uses a bounded deque, `thread::Mutex` and `thread::CondVar`. A guest fiber can now park on an empty channel without blocking its OS thread; `EventMailbox` still uses only nonblocking operations. The guest condition-variable destructor comes from pinned libc++. | Compare channel queue allocation and enqueue/dequeue latency against the prior vector mailbox, including sustained cross-thread bursts, close/wakeup races and UI turn latency. Measure one 16 KiB stack per parked reader and avoid treating its reservation as committed free memory. |
| Host shared concurrency | The common channel/Future/Task headers compile against one SDK static archive bundling private Boost.Fiber/Context object code. Boost is needed to rebuild that archive, while consumers need no Boost headers or separate Boost link target. Bounded `thread::Fiber` joins on its creating OS thread. | Measure cross-thread completion and fiber-park latency, per-Future allocation, archive code size and startup cost against A11's full pool. Audit binary closure on Linux and every wheel architecture before shipping the host backend. |
| Integer helpers and architecture | Original compiler-rt signed/unsigned 32- and 64-bit division/remainder execute on ARMv5T and ARMv6. An ARMv6 `REV` instruction and the ARMv5T software sequence are independently checked. | Count software-helper calls in hot code, compare target profiles on the same workload, and profile code size. Avoid disabling correct helpers merely to reduce binary size. |
| Additional ARM EABI helpers | Original compiler-rt aligned memcpy aliases and soft-float `__aeabi_f2uiz` now pass bounded normal/changed-result controls on both ARM targets and emulator backends. They close two actual Abseil link dependencies but do not establish throughput. | Measure import/branch overhead for aligned copies and conversion latency against target-specific code generation, including small and large copy sizes. |
| Soft-double arithmetic | LLVM's original ARM add/subtract/multiply/divide and conversion helpers now execute on ARMv5T/ARMv6 and both emulator CPU backends after narrow ARMv5-safe instruction substitutions. The test establishes arithmetic results and NaN comparison behavior, not speed. | Measure helper call frequency and cost for Status/format/time paths on soft-float targets; compare code size and latency against hardware-assisted devices without changing target ABI blindly. |
| C formatting and errors | Clang-compatible guest varargs now call the named ROM's `vsnprintf`; original libc++ error categories call `strerror_r` and construct a `std::string` message. Correctness controls cover both ARM profiles and emulator CPU backends. | Measure imported C-call latency, formatting cost and message allocations before using status text or diagnostics on a UI hot path. Compare with direct code/message propagation without discarding useful error context. |
| Imported function pointers | A bounded global function pointer now resolves through a relocated local ARM PLT entry and an E32 data fixup. It executes on both ARM profiles and emulator backends. | Measure indirect PLT call overhead and data relocation cost against a direct import call; expect an extra indirection. |
| Interworking thunks | LLD's exact eight-byte ARM long thunk now gets an E32 code relocation and passes ARMv5T/ARMv6 execution controls. | Count generated thunks and their branch cost in mixed ARM/Thumb builds; compare with direct Thumb calls before choosing a code-generation profile. |
| C++ streams and classic locale | The separate installed `Symbian::Streams` archive executes bounded `ostringstream` formatting and C/POSIX locale on both ARM profiles and emulator backends. Its archive is about 734 KiB before dead-code elimination; this is not final image size. | Measure linked image size, first-use allocations, repeated formatting latency and startup/shutdown cost against direct `std::to_chars` or C formatting. Check both target architectures and firmware profiles before making streams a default dependency. |
| Containers and ownership | Real libc++ strings/vectors, repeated destruction and heap-cell accounting execute. Threaded libc++ now passes bounded shared/weak/unique ownership and cross-thread destruction of a moved `unique_ptr`. No timing data exist. | Compare allocation count and lifetime cost for A11 Future state, continuation capture and cancellation trees; test wider cross-thread destruction patterns before enabling workers. |
| Hash tables and soft-float | The real libc++ `std::unordered_set` probe grows past 300 elements, reserves 700 buckets, erases entries and returns to its initial heap-cell count. `__next_prime` needs software division on these soft-float ARM targets; load-factor growth imports `ceilf` when used. No timing result exists. | Measure rehash latency, helper/import counts, peak bucket memory and code size against `absl::flat_hash_map` after its guest port. Include old ROMs without `libm.dll` when evaluating an SDK-owned math path. |
| Guest clocks and deadlines | The RM-807 ROM's `clock_gettime(CLOCK_MONOTONIC)` returns `EINVAL`; libc++ `steady_clock` now reads `NTickCount` and its HAL period, or the measured ordinary tick when unavailable. The default 64-bit wrap state uses the existing lock-backed atomic bridge. EKA2L1's fast counter was corrected to match its advertised 32768 Hz rate. | Measure per-call HAL/import/atomic cost, concurrent readers, deadline jitter and long-idle wrap behavior on both CPU backends. Compare `NTickCount` and `FastCounter` only with each device's reported rate and direction; the original Symbian source warns that the latter can consume extra power and require activation. Do not use the coarse ordinary tick as a benchmark timer. |
| Native timer requests | Three simultaneous `RTimer` handles execute with cancellation and close-before-completion controls. `TimerPump` completes A11-derived Tasks, including one cross-thread cancellation that wakes a parked event thread within a bounded 500 ms control. The default generated starter and migrated counter share that wait with Window Server input; delayed/cancelled work passed on ARMv5T/ARMv6 and both RM-807 emulator backends. No timer worker is created; the native probe returns to its initial heap-cell count. | Measure creation/close and Future allocation cost, completion jitter, idle request-semaphore wakeups and input latency under mixed Window Server events. Cancellation and close may need to wait for native completion; quantify the effect on UI responsiveness and bound admitted pending tasks. |
| Startup, data and DLLs | Bounded writable EXE data/BSS, local GOT fixups, constructors/finalizers and dynamic DLL load/close have execution controls. | Measure cold/warm startup, import resolution, relocation count, constructor time and repeated DLL load/unload. Keep correctness controls for writable data and detach while optimizing. |
| OS threads, event loop and fibers | A bounded 16 KiB-stack secondary `RThread` starts with its own Symbian heap and joins through `Logon`/`WaitForRequest`; the shared atomic control passes. The A11-derived guest stackless profile executes cross-thread completion. The new guest fiber scheduler parks mutex/condition/future/channel waits cooperatively on one OS thread. Each fiber reserves a 16 KiB heap stack, and the explicit pump allocates a ready snapshot when selecting a turn. | Measure Future-state allocations, callback fan-out/reentry, mutex contention, fiber switch cost, stack high-water, scheduler snapshot cost, idle wakeups and input-to-redraw latency. Compare raw native requests, stackless continuations and pinned fibers for allocation count, stack commitment and latency. Avoid a competing native request-semaphore consumer. |
| Optional exceptions | An isolated no-throw profile retains ARM unwind tables and a Symbian exception descriptor and passes loader/cleanup controls. It does not execute a throw yet. | Measure code/image-size growth from exidx/extab, normal-path cost, throw/catch latency and destructor work once imported type-info data and real unwinding pass. Keep the no-exceptions default as a baseline, not a speed claim. |

The guest clock now passes a two-OS-thread correctness probe with 4,096
concurrent reads and 128 ordered handoffs, without a net Symbian heap-cell
increase. This is not a clock throughput or deadline-jitter measurement.
The process-wide 64-bit atomic uses the selected lock-backed runtime by
default, so contention is a plausible cost worth measuring before a timer
pump or A11 worker profile relies on frequent clock reads.

`TimerPump` now caps pending native timer requests at 64 by default. This
sets an upper bound on simultaneous timer handles and their Promise states,
but does not bound completed Task references retained by application code or
the allocation fan-out of arbitrary continuations. Measure saturation and
reuse cost alongside normal timer latency. The 64-entry policy is not based on
a measured OS limit; tune it only after recording memory and wakeup behavior
under a representative workload.

For allocator candidates, measure at least allocation latency distributions,
fragmentation, peak committed bytes, OOM behavior, cross-thread free and UI
latency under the same bounded workload. The current `User::Alloc` adapter is
the correctness baseline. [Mimalloc](https://github.com/microsoft/mimalloc)
is a candidate, not an integrated guest dependency or an established win.
An arena must obtain and release memory through a verified Symbian mechanism
and respect process/thread heap ownership.

The next concurrency measurements need a verified guest monotonic clock and a
bounded thread-start/join contract. Measure a cold run separately from warmed
runs, retain raw samples and failures, and compare both emulator CPU backends.
Once safe device execution is separately authorized and available, repeat on
identified hardware; emulator ratios alone cannot rank phone performance.
