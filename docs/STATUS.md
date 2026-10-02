# Status

The initial technical survey is in RESEARCH.md. The owner has one Nokia 808;
exact physical identity, installed firmware and recovery method remain unknown.
The supplied Delight RM-807 archive provides preserved emulator ROM/Z material;
guarded disposable tests now verify real GUI pixels, pointer-driven redraws,
reset and normal zero guest/frontend exit on both macOS CPU backends.

Latest 2026-10-02 development checkpoint:

* The first shared guest event owner now dispatches RTimer, real
  RProperty::Subscribe results, the bounded event mailbox and ready fibers
  before its sole native request wait. Fiber sleep arms an RTimer through that
  owner; event-affinity dispatch has an explicit operation. A structured
  timer/property owner forwards cancellation and an absolute deadline, then
  completes its asynchronous join after child and deadline-alarm drainage.
  The GUI and generated timer-task starter use the event owner. An
  installed-SDK candidate exported both headers, and both applications
  compiled against it. The GUI's rendered input/reset/normal-exit test passed
  on Dyncom and Dynarmic.
  A dedicated guest normal/changed-result probe passed 8/8 across ARMv5T/
  ARMv6 and both backends; it checked a real property value, timer, fiber
  Await/sleep, cancellation, repeated asynchronous TaskGroup Finish, the
  structured owner's success/close/timeout/error paths, cross-thread event
  dispatch and heap-cell balance. This remains a bounded event executor, not
  full A11 `thread::` parity: native request ownership is still split between
  the existing adapters, A11 Select/fiber trees/pools are open, and no phone
  result is claimed.

* The SDK exception policy now follows A11: native targets default to
  `-fno-exceptions`, with explicitly selected implementation translation units
  allowed to enable exceptions. The pinned, unmodified full A11 host
  `cpp/thread` compiled with that boundary, and its three original host test
  executables passed in an isolated build. The maintained
  `full_host_probe/CMakeLists.txt` reproduces this check. The staged SDK host
  archive still needs a deliberate replacement; this result does not claim
  that all A11 host APIs ship in the installed SDK.

* Host `thread::` now includes an A11-derived custom ready-fiber scheduler,
  idle-park guard, the original MPMC work queue, shared stackless `Post`/
  `PostAt`, and original `PermanentEvent`/`Select` with wait diagnostics still
  omitted. The Python boundary installs A11's CPython GIL park pair and
  deferred-reference discipline; a bounded Future-to-asyncio bridge resolves
  on the captured event loop. Native tests passed policy ordering, lock/GIL
  park balance, pool callback/timer and Select controls; an extracted macOS
  wheel passed concurrent Python progress, asyncio completion/cancellation and
  deferred-reference drainage. The installed SDK's Boost-free host consumer
  passed its expanded Post/Select test. A11 fiber trees, pool work stealing,
  Submit/Schedule, channel selection, introspection and complete shutdown
  remain open, so this is not a compatible full A11 host port.

* Repository and generated-project C++ formatting now uses clang-format 23's
  `InsertBraces` rule. The starter contains its own `.clang-format`; 170
  first-party C++ files passed a formatter idempotence check after 47 owned
  files were updated. The two user-owned edits were preserved. The selected
  visible SDK was refreshed with the generator, formatted starter and CMake
  target closure; 3,123 payload digests verify. Its initial-build, copied-SDK
  and moved-project test passed. Root CTest passed 10/10, and the built macOS
  wheel contains the template dotfile. The initial installed starter build
  exposed a missing guest fiber archive link from `Symbian::Stackless`; the
  target now supplies that archive when installed.

* A bounded guest `thread::Fiber` now runs on the verified ARM/Thumb switch,
  pinned to one OS thread and a 16 KiB stack. `thread::Mutex` contention and
  `thread::CondVar` signal/timeout waits park a fiber; `thread::SleepFor` and
  an unresolved `Future::Await` do likewise. Outside a fiber, unresolved
  guest Await fails clearly. `thread::SchedulerPolicy` provides custom
  ready ordering and a cross-thread wake callback outside internal locks.
  A normal/changed control covering move-only work, lock contention,
  cross-thread signal, timeout, LIFO policy, Future await and heap balance
  passed 8/8 on ARMv5T/ARMv6 × Dyncom/Dynarmic. A fresh SDK export builds
  a separate `Symbian::Fibers` archive for each architecture; installed-archive
  execution passed 8/8, and the updated visible SDK passed the same 8/8
  matrix against its existing Abseil archives. Event-owner integration,
  A11 Select/pool/tree,
  cancellation and joining remain open.

* Host and guest now share one portable A11-derived channel, Future/Task,
  TaskGroup and mailbox source layer. The host CMake target
  `symbian::concurrency` selects an opaque `thread::` primitive ABI; its
  Boost.Fiber/Context implementation is bundled inside one SDK static
  archive. An ordinary consumer has no Boost headers or separate Boost link
  dependency; Boost is needed only to rebuild that archive. The host's bounded
  `thread::Fiber` handles move-only
  work, same-thread join, cooperative cancellation and normal C++ cleanup;
  call sites use `thread::Fiber`, not Boost types. The SDK export includes the
  host archive and Boost-free headers. An installed host consumer compiled,
  linked and ran with Boost discovery disabled; its link command named no
  Boost library. Root CTest passed 10/10. An
  earlier no-Boost fallback test passed before that incomplete branch was
  removed. The updated installed-SDK
  guest stackless/channel/timer matrix passed 16/16 across ARMv5T/ARMv6 and
  Dyncom/Dynarmic. Guest `CondVar` now matches A11: true means timeout.
  A fresh workspace export passed four guest timer/Future cases and an SDK
  copy/build check. That earlier export had 3,113 verified payload digests.
  The new visible SDK verifies 3,122 payload digests; its prior tree is
  preserved at `~/dev/symbian-sdk-before-fibers-20261002`.
  A11 tree/Select/pool semantics and comprehensive guest fiber lifetime remain open.

* Guest synchronization and channel headers now keep A11's `thread::`
  namespace: `<thread/boost_primitives.h>` and `<thread/channel.h>`.
  The shared channel backend supports bounded FIFO `Channel<T>` with
  `reader()`/`writer()`, move-only values, blocking worker reads/writes,
  nonblocking Abseil-status operations, close wakeups and drainage.
  `EventMailbox` uses the nonblocking channel path. Original pinned libc++
  condition-variable destructor support was added to both runtime archives.
  A timed-wait control exposed an early `no_timeout` return without a signal;
  the adapter now validates signal generation and uses monotonic bounded
  waiting. The installed normal/changed guest stackless and timer/channel
  matrix passed 16/16 on ARMv5T/ARMv6 and Dyncom/Dynarmic. The fresh visible
  SDK had 3,113 verified payload files after the CLI refresh; the prior
  3,110-file SDK is preserved at
  `~/dev/symbian-sdk-before-channel-20261002`. Full A11 `Select`, fiber lifecycle,
  zero-capacity rendezvous and exception-throwing closed writes remain open.

* CLI inspection and workflow commands now default to human-readable
  summaries, with TTY-aware color and readable byte sizes. `--output-format=json`
  preserves the canonical schema for scripts before or after nested commands.
  Root and nested help describe every command and option. Long result lists
  use separate numbered lines. The live `device list` output showed the
  connected Nokia USB and mounted storage without its serial;
  44 CLI/device/control/SDK tests and the selected-SDK project regression
  passed. A rebuilt, separately installed macOS wheel passed its resource,
  native-dependency and CLI audit. The unrelated older package fixture still
  fails at the ARM exception-descriptor gate before CLI output is reached.

* Generated applications select an SDK through an ignored local
  `sdk-location.json` containing only a path, with no SDK digest lock.
  A two-way switch between the visible SDK and a second installation passed
  through `symbian app build`: compiler identity and
  `compile_commands.json` followed each selection. A direct CMake/Ninja
  control initially did no work after the path changed; generated
  `sdk.cmake` now declares the location and preference files as configure
  dependencies. Direct builds then reconfigured, rebuilt and selected the
  new compiler in both directions. The GUI example's 19 KiB source/digest
  manifest moved from its application folder to
  `research/gui_app/source-profile.json`, where SDK source preparation
  actually needs it. Synthetic staging checks passed 12/12; preparation
  against the pinned original source tree succeeded, and the current
  six-DLL GUI link test passed. The visible SDK template was refreshed and
  its digest manifest resealed. The generated-project SDK-switch regression
  passed with the prepared visible SDK (1/1). A full prepared-workspace
  export using the moved profile succeeded and all 3,109 payload digests
  verified; the visible SDK remained the global default.

* Localized menu resources and project SVG icons now build through ordinary
  `[application]` settings. The host policy maps BCP 47 tags to selected
  `TLanguage` IDs and invokes original `rcomp` with both Unicode output and
  UTF-8 source interpretation. Native SIS writing/inspection accepts a bounded,
  canonical resource set and converts an SVG to a same-host deterministic gzip-backed
  MIF. The maintained `gui_app` packages fallback/French/German/Japanese
  captions and one icon as seven files. On both CPU backends, EKA2L1's original
  installer installed/reloaded/removed them, AppArc selected all three
  translations and decoded `Zähler` and `カウンター`, and its original MIF
  reader recovered the SVG
  (8/8 headless tests). The separate native SIS suite passed 9/9 and the
  resource Pytests passed 2/2. These headless cases execute no guest
  instructions; physical Belle SVG-in-MIF rendering has not been observed.
  The physical `menu_v5` observation below predates this icon/locale change.
  A fresh 3,109-file SDK export passed digest audit and its own ARMv6
  init/build/package smoke. After promotion, the visible SDK itself completed
  ARMv6 and ARMv5T init/build/package checks; its ARMv6 sample included
  French and Japanese menu translations. The usual `~/dev/symbian-sdk` path
  was updated from that tested export and rebased; the previous tree remains
  at `~/dev/symbian-sdk-before-icon-locale-20261002`. The macOS wheel's
  installed dependency/resource audit also passed with the icon template
  and system zlib dependency.

* Application-menu packaging now uses the original EPL-licensed Symbian
  `rcomp` at pinned revision `d3c2eadd`, with a replayable 64-bit host patch
  and an SDK UID-checksum companion. A prepared-workspace SDK export contains
  both tools, genuine `AppInfo.rh`, the EPL notice and 3,108 digest-valid
  files. `[application]` gives `caption` and `short_caption` directly in
  `symbian.toml`; `symbian init` generates defaults, and `gui_app` specifies
  its own. Native SIS writing/inspection verifies the EXE, registration and
  caption resources, their hashes, UIDs and same-drive install paths.
  Eight native SIS tests, a real-resource Pytest, fresh ARMv5T and ARMv6
  init/build/package checks and `gui_app` packaging passed. An original
  EKA2L1 AppArc parser check exposed that `rcomp` needed its `-u` Unicode
  mode: the earlier byte-text SIS installed but did not decode its menu
  captions. The corrected resources parsed as `Counter` and `Symbian GUI
  Counter` on both backends. The older E32-probe packaging
  fixture still fails at its separate exception-descriptor gate.
  The 3,108-file export was rebased into `~/dev/symbian-sdk`, with the
  prior digest-valid tree retained at
  `~/dev/symbian-sdk-before-menu-registration-20261002`. The visible SDK
  repackaged `gui_app` with Unicode resources. EKA2L1's original installer
  accepted all three files, parsed their menu text, reloaded the package registry and removed and
  reinstalled the resources on Dyncom/Dynarmic (8/8 headless tests).
  The visible SDK's full GUI package verifier passed 17 image/checksum and
  installer cases with the current six-DLL/153-slot executable; it still
  records zero executed guest instructions. The separate on-phone result below
  is user-reported, not inferred from that verifier.

* Physical-device groundwork now has `symbian device list`, `info` and a
  build/package/USB-mass-storage staging flow. IOService associated the
  connected `808 PureView` USB descriptor with a writable mounted S60 disk;
  the Mac USB system profiler alone had returned no devices. The public
  selector hashes the serial, and RM code, firmware and phone runtime remain
  unknown. Eight synthetic discovery/transfer/policy/Linux/low-space controls
  passed. A real generated app in `~/dev/symbian-app-3` rebuilt reproducibly,
  and its unsigned SIS passed a host-only staging/hash check. After the owner
  confirmed the photos were copied, an ARMv5T portable generated starter
  with Unicode menu resources was copied to the phone's `Installs` folder.
  The copied SIS SHA-256 is
  `db4875221d6ecabbe02bfba76c4201c5da5f8f0df298ae50f7f5050356e70db1`;
  `disk4` ejected normally. The owner then reported that the handset installer
  succeeded, `menu_v5` appeared in the application menu, and the app opened
  and responded to a tap. This is direct user observation, not an SDK-read
  installer registry, captured device log or instrumented launch trace. A
  direct installer transport and real Linux-connected phone remain untested.
  `examples/gui_app`'s standalone reproducibility check still returns
  `DATA_LOSS` for differing CMake ELF/E32 builds, an independent open issue.

* `examples/gui_app` now links the installed `Symbian::Stackless`/guest Abseil
  profile through a narrow modern C++ bridge. A 300-ms timer Future lights a
  separate marker after an increment; Reset cancels pending work. The existing
  counter, drawing and owner's `app.cc` edits remain intact. The same event
  thread consumes Window Server and timer completions. The final candidate
  built reproducibly, both root and standalone CMake targets linked, and the
  expanded pixel/marker/cancellation/exit GUI control passed on ARMv5T and
  ARMv6 × Dynarmic and Dyncom (4/4). The root `gui_app_e32` publisher also
  rebuilt under Apple Clang, and its ARMv6 image passed both backends after
  SDK promotion. `symbian init` now defaults to Abseil Status/StatusOr and
  timer-backed Tasks, with a portable option and automatic portable selection
  when `--firmware` names a Z drive without `libpthread.dll`. A final-candidate
  CLI initial-build, relocated SDK/project and two-backend Task/cancellation/
  rapid-exit control passed on ARMv5T and ARMv6 × Dynarmic and Dyncom (4/4);
  the full generated-project suite passed 17/17 before the additional ARMv5T
  cases passed 2/2. A preceding candidate passed real starter
  builds and GUI execution on C7, E6, 6120 and E71 (4/4); the latter two used
  the portable profile. A deliberately modern executable against E71 now
  returns canonical `FAILED_PRECONDITION` before emulator startup. Root CTest
  passed 8/8. The digest-valid 3,100-file candidate was rebased into the
  visible `~/dev/symbian-sdk`; the prior 3,100-file tree remains at
  `~/dev/symbian-sdk-before-gui-init-migration-20261002`, also digest-valid.
  The promoted SDK completed a new default `symbian init` build. Its GUI
  publisher passed again on both ARM targets and both emulator backends.
  These are application-path results; A11 fibers and genuine
  `thread::` primitives remain open.

* The first C3 backend prerequisite now lives in the runtime archive, with an
  ARM/Thumb context swap preserving callee-saved registers across a bounded
  16 KiB heap stack. A C++ probe retains heap-backed string/unique ownership
  across two switches, destroys both normally and checks heap-cell balance.
  Installed normal/changed execution passed 8/8 on ARMv5T/ARMv6 ×
  Dyncom/Dynarmic. This is raw context switching only: stack guards,
  floating-point context, native TRAP safety, fiber scheduling, joining and
  genuine A11 `thread::Mutex`/`CondVar`/`PermanentEvent`/`SleepFor` remain
  unimplemented. The sealed candidate was promoted into the visible SDK;
  ARMv6/Dynarmic normal/changed controls passed again after promotion. The
  prior sealed installation is preserved at
  `~/dev/symbian-sdk-before-fiber-swap-20261002`.

* A second native request owner now runs `RProperty::Subscribe` through a
  typed `Future<int>` with SDK-owned status, result storage, handle, native
  cancellation and close-time drainage. It shares the timer pump's sole
  event-thread semaphore consumer. An `EventMailbox` bounds cross-thread
  dispatch and executes callbacks outside its lock; the generated timer GUI
  uses it. The installed candidate passed 8/8 timer/property/mailbox normal
  and changed controls on ARMv5T/ARMv6 × Dyncom/Dynarmic. The generated GUI
  timer and Status controls passed 5/5 against the candidate and 6/6 including
  a copied Abseil project against the 3,100-file promoted visible SDK.
  Absolute timer deadlines now take real-world `absl::Time`, convert once at
  registration and wait monotonically; a simulated post-registration wall
  jump, 24-hour admission/cancellation, infinity and native slice boundaries
  passed. Actual wall-clock adjustment and full-length rearm are not tested.
  A subsequent export adds an `Await` guard: ready results work and an
  unresolved `Await` returns `FailedPrecondition`; its stackless matrix passed
  8/8 and the visible SDK was updated. Its ARMv6/Dynarmic normal/changed
  cases passed again after promotion. A post-promotion timer/property matrix
  passed 8/8 with an added pending
  subscription-close/drain control. Full A11 `thread::` mutex,
  condition variable, permanent event and sleep semantics require the C3
  fiber backend; no OS-thread-only lookalike is published.

* A sealed development SDK candidate at
  `.symbian/abseil-direct-status-sdk-20261002` now publishes the pinned guest
  Abseil types directly through `symbian::concurrency`: Future/Promise/Task
  results are `absl::StatusOr<T>`, failures are `absl::Status`, and the earlier
  name-shortening aliases are gone. Installed-SDK stackless and timer controls
  each passed 8/8 on ARMv5T/ARMv6 × Dyncom/Dynarmic, including changed-result
  exits. At that checkpoint `TimerPump` used `absl::Duration` with a
  provisional monotonic-deadline wrapper; the later export above changed
  absolute deadlines to `absl::Time`. The
  Status-enabled generated GUI passed both backends, and its injected model
  failure reached the native exit path (3 tests). The pinned Abseil
  Status/StatusOr/Cord/map/time installed contract passed 9/9. Cross-thread
  closing of an SDK-owned page source and the original Abseil allocator passed
  normal/changed controls on the preceding candidate. Root ARM IDE CMake now
  has explicit targets for both Abseil probes; ARMv5T/ARMv6 configure and
  object compilation passed. General A11 scheduler/fibers, OS TLS and broader
  Abseil remain open. That candidate was promoted later as recorded above.

* Genuine A11-pinned guest Abseil `Status`, `StatusOr`, `Cord` payloads and
  `flat_hash_map<std::string, int>` now execute on RM-807. The normal and
  deliberately changed-result contract passed 8/8 from replayed source and
  8/8 through the installed `Symbian::AbseilStatusOr` target on
  ARMv5T/ARMv6 × Dyncom/Dynarmic. The visible SDK contains 384 Abseil
  headers and 43 compiled closure archives per architecture, plus exact source
  revision, three replayable patch digests, Apache-2.0 license and imported
  CMake target. A copied project built and converted to E32 using only the
  installed SDK for target inputs. The 3,097-file export is installed at
  `~/dev/symbian-sdk`; its verified 2,597-file predecessor is retained at
  `~/dev/symbian-sdk-before-guest-abseil-20261002`. The wider selected
  runtime regression passed 24 cases with one skip. The stream profile now
  supports the
  necessary 16-bit-wide libc++ surface and selected real OpenC wide/stdio
  functions; SDK adapters provide `nan`, `nanf` and `ldexpl` where no frozen
  export exists. Byte and bitwise atomics use real EUSER operations, while
  Abseil's table-seed TLS is adapted to a process-wide atomic sequence.
  This is a tested Status/StatusOr/map subset, not every Abseil component,
  general ELF TLS, cross-thread page release or A11 fibers.

* The pinned A11 Abseil `LowLevelAlloc` now links from only two ordered,
  replayable source patches and executes on the RM-807 Dynarmic guest. Its
  original allocator and arena code allocate a 130,000-byte block through an
  SDK-owned process `RChunk` page source; an arena with an outstanding block
  refuses deletion, then closes after that block is freed. A maintained
  opt-in guest test passed the normal and deliberately changed-result cases
  (2/2) against a fresh 2,597-file SDK export and again against the visible
  installed SDK. Both patches apply to the
  pristine A11 pin and reverse-check from the replay checkout. Runtime exits
  now use named internal reasons with compile-time `KErrNoMemory` and
  `KErrArgument` checks. The related installed-SDK runtime exit/clock/lifecycle
  selection passed 53 guest cases; root CTest passed 8/8. The 2,597-file SDK
  was promoted to `~/dev/symbian-sdk`, with the verified 2,596-file predecessor
  at `~/dev/symbian-sdk-before-closure-final-20261002`. A copied-project
  test passed before and after promotion. Cross-thread page release, memory
  pressure and A11 fibers remained open at that checkpoint; the newer
  Status/StatusOr result is recorded above.

* The runtime now has an owned `RChunk` page bridge with validated page size,
  page-aligned process-owned allocation, explicit handle release and a heap-cell
  balance check. ARMv5T/ARMv6 × Dyncom/Dynarmic normal/changed controls
  passed, as did
  a separate `pthread` TLS-key test for worker isolation and its exit
  destructor. EUSER byte exchange and OpenC `strtol`, `strcpy`, `strcmp`,
  `sysconf` execution controls passed on ARMv6/Dynarmic. This is page sourcing
  and selected services, not Abseil LowLevelAlloc or general ELF TLS. A
  replayable Abseil patch makes `GetCachedTID()` use its existing
  `pthread_self()` fallback on Symbian; the isolated relink no longer reports
  `__tls_get_addr`. Other allocator, synchronization and C-service undefined
  symbols remained at that checkpoint. The selected 2,597-file export
  passed its allocator test and was promoted as recorded above.

* LLVM's original ARM soft-double compiler-rt implementation now runs under
  the SDK's ARMv5T and ARMv6 profiles. An ordered, replayable LLVM patch
  changes four ARMv6T2-only `movw` constant loads to literal loads and one
  `bfc` to ARMv5-safe shifts; original helper sources complete its dependency
  closure. Actual guest double arithmetic, comparisons, NaN and conversions
  passed 8/8 normal/changed
  source-built and 8/8 installed-candidate ARMv5T/ARMv6 × Dyncom/Dynarmic
  controls. The candidate's copied-project test and root CTest 8/8 passed.
  The 2,596-file SDK was promoted; its verified predecessor is retained at
  `~/dev/symbian-sdk-before-softfloat-20261002`. This removes all floating-point
  compiler ABI undefined symbols from the isolated pinned Abseil link, but
  allocator, TLS, synchronization and C/POSIX services still block genuine
  guest Status/StatusOr execution.

* An integration control now uses A11-derived `TaskGroup` over real
  `TimerPump` requests: joining two timers succeeds only after native
  completion; cancelling an aggregate of three timers forwards cancellation
  to each request and settles after event-thread drainage. ARMv6/Dynarmic
  normal and changed-result source cases passed (2/2), followed by all 8/8
  ARMv5T/ARMv6 × Dyncom/Dynarmic source controls and 8/8 installed-SDK
  controls. This is stackless structured composition for timers, not a fiber
  backend.

* `TimerPump` now limits simultaneously pending native timer Tasks (64 by
  default, configurable). Saturation returns a ready Task with
  `kResourceExhausted` before allocating an OS timer; a slot is reused after
  event-thread completion. A two-slot saturation/reuse/close and heap-cell
  control passed 8/8 source-built and 8/8 installed-candidate
  ARMv5T/ARMv6 × Dyncom/Dynarmic normal and changed-result executions.
  The candidate contains 2,596 digest-verified files. This bounds one native
  request type, not all A11 continuations or native I/O queues. Guest Abseil
  Status/time migration remains open. The candidate's copied-project and
  two-backend GUI tests passed (3 tests), root CTest passed 8/8, and the
  candidate was promoted to `~/dev/symbian-sdk`. Its 2,596-file predecessor
  remains at `~/dev/symbian-sdk-before-timer-cap-20261002`; both verify by
  digest. Post-promotion ARMv6/Dynarmic normal and changed-result timer
  controls passed (2 tests), as did the canonical copied-project test.

* Generated applications now have an opt-in `SYMBIAN_ENABLE_TIMER_TASKS`
  profile. It links `Symbian::Stackless` and uses one event-thread wait for
  Window Server input/redraw and A11-derived timer Tasks. A tap logs
  immediately and schedules a delayed log; Clear cancels pending Tasks.
  Actual RM-807 GUI controls passed on Dynarmic and Dyncom, including delayed
  completion, cancellation and normal Exit. The default GUI controls passed
  on both backends, and a default E71 starter built and executed. The selected
  DRTAEABI proxy now includes the real `__cxa_pure_virtual` ordinal needed
  by this link. The 2,596-file candidate was installed at
  `~/dev/symbian-sdk`; the previous sealed version is retained at
  `~/dev/symbian-sdk-before-window-timer-20261002`. Both 2,596-file seals
  verify; canonical copied-project and two-backend opt-in GUI tests passed
  (3 tests), as did root CTest 8/8. This establishes bounded
  shared-loop integration, not complete C1/C2, guest Abseil or fibers.
  `std::chrono` is provisional in the public timer API: once guest Abseil
  time is executable, SDK public time utilities should use Abseil types.

Earlier 2026-10-01 checkpoints:

* The first native-to-A11 stackless completion path now executes through
  `symbian::concurrency::TimerPump`. One event OS thread owns the native
  `RTimer` requests and publishes `Task` results; a worker can request
  cancellation through a coalesced `RThread::RequestSignal()` wakeup without
  touching thread-relative timer handles. A bounded guest control verifies
  parked cross-thread cancellation, monotonic `ScheduleAt`, overlapping
  timers, continuation reentry, immediate `OnReady`, close with a pending
  timer and heap-cell balance. Normal/changed controls passed eight source
  cases before a final continuation check, and the formatted source smoke
  plus eight installed SDK cases passed after it on ARMv5T/ARMv6 ×
  Dyncom/Dynarmic. The final candidate and canonical copied-project tests
  passed, as did a canonical ARMv6/Dynarmic smoke, root CTest 8/8 and both
  ARM IDE index builds. The 2,596-file canonical SDK and its 2,595-file
  backup verify by digest. This is not yet a shared Window Server event pump,
  arbitrary native I/O adapter, full A11 scheduler, or fiber backend.

* A bounded native `RTimer` owner now lives behind a narrow original-SDK
  bridge and a modern `symbian::concurrency::NativeTimer` interface. It creates
  a thread-relative handle, rejects negative and overlapping arms before the
  OS can panic, cancels and drains a pending request on close, and retains its
  status storage until completion. A probe exercises three simultaneous
  timers, completion before waiting, cancellation, close during a pending
  request, rearm and heap-cell balance. Normal and deliberately changed-result
  cases passed eight source-built and eight installed-SDK ARMv5T/ARMv6 ×
  Dyncom/Dynarmic executions. The formatted final export passed another
  eight installed cases; its copied/relocated project test and canonical
  ARMv6/Dynarmic smoke test passed. Root CTest passed 8/8, both ARM IDE probe
  targets built, and the 2,595-file canonical SDK and its 2,594-file backup
  verify by digest. That checkpoint established the C1 request-owner slice;
  shared Window Server pumping and broader C1 controls remain open.

* The guest monotonic clock now has concurrent execution controls: two real
  `std::thread` readers take 2,048 samples each, then exchange 128 ordered
  timestamps, with no net Symbian heap-cell increase. Normal and changed
  controls for this clock and the existing thread path passed 16 source and
  16 installed-SDK ARMv5T/ARMv6 × Dyncom/Dynarmic cases. The combined link
  exposed an obsolete `__throw_system_error` fallback, now retired in favor
  of original libc++ `system_error.cpp`; `std::this_thread::yield()` required
  the real `sched_yield` libc import. The previous error-category probe passed
  eight source cases. An invalid `std::thread::join()` exited with -6 in four
  source and four installed negative controls, confirming the default fatal
  no-exceptions path. The new SDK candidate has 2,594 digest-valid files;
  an E71 generated starter built and executed, copied/relocated project tests
  passed before and after promotion, a canonical ARMv6/Dynarmic clock-thread
  smoke test passed, root CTest passed 8/8, and both ARM IDE probe builds
  include the new source. It was installed at `~/dev/symbian-sdk`; the prior
  sealed SDK remains at `~/dev/symbian-sdk-before-clock-thread-20261001`.
  A shared timer/Window Server pump, suspension and half-wrap clock
  continuity remain open C1 gates.

* The guest now executes original libc++ `steady_clock` and `system_clock`.
  RM-807's OpenC `CLOCK_MONOTONIC` returned `EINVAL`; a maintained Symbian-only
  LLVM patch reads `NTickCount` and its HAL period for monotonic time, with an
  ordinary-tick fallback. Original Symbian source recommends `FastCounter`
  for short profiling and `NTickCount` for production. Two ordered EKA2L1
  patches expose the nanokernel/fast-counter HAL rates and correct the
  emulator's FastCounter to its advertised 32,768 Hz. Positive and changed
  guest controls passed 16 ARMv5T/ARMv6 × Dyncom/Dynarmic cases from source
  and 16 against the sealed installed-SDK candidate. The generated E71
  starter also built and executed with the new optional proxy catalog, and
  the copied/relocated SDK project test passed both before and after
  promotion. The converter now accepts an
  actual needed-proxy subset while retaining identity and duplicate checks;
  its import suite passed 11 tests with one opt-in skip. Root CTest passed
  8/8; both ARM IDE probe targets build and index the two new clock sources.
  The 2,594-file sealed SDK was installed at `~/dev/symbian-sdk` with its prior
  2,591-file tree at `~/dev/symbian-sdk-before-clock-20261001`; both digest
  sets verify. Concurrent clock reads, suspension and a gap of half a 32-bit
  tick wrap (about 24.9 days at 1 ms) remain C1 gates.

* The prior root IDE checkpoint exposed all 48 then-known sources across
  platform-targeted probe projects through per-project ARM CMake targets,
  including the C++20 module and the locally prepared Mbed TLS probe.
  Separate ARMv6 and ARMv5T
  guest-index presets built successfully; both `compile_commands.json` files
  cover 48/48 sources with the correct target triple. The Mbed TLS source and
  SDK header paths are local opt-in settings, not shared repository paths.
  The local ARMv6 CLion preset uses LLVM 23 and
  was enabled alongside the existing host Debug profile, preserving a backup
  of the prior workspace settings. The running IDE log records a successful
  `clion-guest-probes-armv6` CMake generation (exit 0) after the profile was
  enabled. Actual editor diagnostics/header navigation remain a UI check;
  terminal compile database evidence is separate.
* A bounded classic-C locale and stream runtime is now a maintained,
  installable `Symbian::Streams` profile. Original LLVM libc++ locale/ios/
  ostream/iostream/strstream sources, SDK C-locale adapters and real selected
  OpenC imports produce `std::ostringstream` output in a guest. C and POSIX
  locale names work; invalid adapter inputs return `EINVAL`. Normal and
  changed-result controls passed eight ARMv5T/ARMv6 × Dyncom/Dynarmic cases
  from source and another eight from a fresh installed SDK. The visible
  SDK has 2,535 digest-valid files; its verified predecessor remains at
  `~/dev/symbian-sdk-before-streams-20261001`. This alternate archive uses
  its own libc++ configuration and replaces `Symbian::Runtime` in a target.
  File streams, wide strings, arbitrary locales and executable guest Abseil
  Status/StatusOr remain open. The pinned Abseil build is still an isolated
  diagnostic, not a shipped guest library. An isolated pinned
  `absl_statusor` archive now cross-compiles with experimental Symbian/Fuchsia
  platform guards and wide declarations, but guest linking exposes absent
  low-level mapped allocation, 64-bit atomics, TLS, soft-float builtins and
  further C/POSIX services. No Status execution or full A11 C1–C3 claim follows.
  A narrow native `RChunk` bridge then passed 16 source and installed-SDK
  normal/changed-result executions across both ARM targets and CPU backends:
  query native page size, create, write, grow, check retained bytes and close
  16 local chunks. This is a candidate page-source prerequisite, not an
  Abseil allocator or a native
  request/fiber backend.
* Imported **function** pointers now cross the ELF/E32 boundary in a bounded
  form. The converter validates ARM PLT identity, the dynamic `R_ARM_ABS32`
  record, zero-initialized writable storage and its retained relocation,
  then writes an E32-relocatable PLT pointer. A global EUSER `memmove`
  pointer executed on ARMv5T/ARMv6 × Dyncom/Dynarmic from source and against
  the existing installed SDK runtime/proxy (eight executions). Two mutated
  ELF controls reject a data-object relocation and false PLT symbol value.
  Imported **data objects** remain unsupported; typed guest exceptions still
  need that distinct ABI support. The earlier isolated LLVM libc++ classic
  locale/stream prototype ran on both backends before its promotion above.
  Its first image exposed an unrelocated LLD
  interworking thunk and failed in the guest. The converter now relocates
  LLD's exact named eight-byte ARM long-branch thunk; the original stream
  image then exited 0 on both CPU backends. A maintained thunk call passed
  all four architecture/backend cases, while a mutated out-of-range target
  was rejected. A fresh visible SDK passed the combined 11-case
  function-pointer/thunk matrix, was promoted after digest audits, and now
  has 2,503 sealed files. The previous 2,503-file SDK is retained at
  `~/dev/symbian-sdk-before-import-pointers-20261001` and also verifies. A
  post-promotion ARMv6/Dyncom imported-pointer execution also passed through
  the canonical SDK manifest.
  Other linker-generated absolute thunk forms remain open.
  An isolated pinned-Abseil `absl_status` build with the experimental stream
  profile progressed further but still fails on Linux `link.h` assumptions,
  disabled wide strings and unavailable file streams. No guest Abseil archive
  or A11 native request/fiber backend is claimed; the compiler log is retained
  under `.symbian/abseil-guest-probe/`.
* Clang-compatible guest varargs and original LLVM libc++ error categories
  now execute on the named firmware fixture. The SDK-owned `stdarg_e.h`
  bridges OpenC's header to Clang's ARM EABI `va_list`; a real `libc.dll`
  `vsnprintf` formats register, stacked and 64-bit arguments. The guest
  archive builds original `error_category.cpp` and `system_error.cpp`, and
  `std::error_code` category/message/condition checks use actual libc imports.
  Their first PIC image was correctly rejected for cross-mapping code/data
  references; compiling those originals without PIC uses the existing E32
  data relocation. Normal and changed-result controls passed 16 source-tree
  ARMv5T/ARMv6 × Dyncom/Dynarmic cases, and another 16 against a fresh
  installed SDK. A copied-SDK application build passed. The visible SDK was
  refreshed with 2,503 digest-valid files; the previous 2,502-file tree is
  retained at `~/dev/symbian-sdk-before-runtime-c-services-20261001`.
  The promoted canonical SDK passed four ARMv6/Dyncom normal and
  changed-result controls for both capabilities. Root CTest passed 8/8;
  Black, Ruff, clang-format and `git diff --check` passed.
  An isolated pinned-Abseil `raw_logging_internal` target compiles with the
  maintained varargs header; the full Status target still fails on streams.
  Abseil Status still needs the unbuilt locale/iostream closure; A11 native
  request ownership, `Await` and fibers remain open.
* The bounded guest completion profile now owns its API under
  `<symbian/concurrency/*.h>` and `symbian::concurrency`, including its
  OS-thread mutex adapter. It no longer exports incompatible headers under
  A11's `<a11/concurrency/*.h>` or `thread::` names. The unmodified A11 source
  pin still has its original namespace and remains the reference for a full
  guest port. Source and fresh installed-SDK execution each passed eight
  ARMv5T/ARMv6 × Dyncom/Dynarmic normal/changed-result controls; a copied-SDK
  generated application build passed. The visible SDK was refreshed from the
  prepared workspace and both its 2,502-file manifest and the retained
  `~/dev/symbian-sdk-before-namespace-20261001` tree verify by digest.
  The canonical SDK also passed the ARMv6/Dyncom normal and changed-result
  controls after promotion. Root CTest passed 8/8, and the A11 source
  integrity group passed 5/5.
  Guest Abseil Status/StatusOr, A11 Await and fibers remain open gates.
* A bounded A11-derived stackless guest profile now executes
  Promise/Future/Task, inline `OnReady`/`Then`, cooperative cancellation,
  abandoned-promise completion, ordered nonblocking `JoinAll` and bounded
  reentrant `DriveInline` and `TaskGroup::Finish` fan-in/cancellation. Its
  `symbian::concurrency::Mutex` adapter uses the tested guest libc++ OS-thread lock; no fiber
  enters it. The normal and changed-result execution matrix passed eight
  ARMv5T/ARMv6 × Dyncom/Dynarmic cases from source and another eight against
  a fresh exported SDK through `Symbian::Stackless`. The visible SDK was
  refreshed after a clean 2,496-file audit; its new 2,502 files verify by
  digest, and the previous tree is retained at
  `~/dev/symbian-sdk-before-stackless-20261001`. This is an explicit
  `symbian::concurrency::Result` adaptation, not
  the final Abseil Status ABI or a native-request/fiber backend. Directly
  cross-building pinned Abseil Status revealed missing streams, C++ ABI,
  signal APIs and lock-free atomic assumptions. A11 C1, remaining C2 and C3
  work remains open. Root CTest passed 8/8 and the refreshed-SDK
  generated-project/A11 source group passed 15/15. The debugger check now
  stops in both the C bridge and `model.cc`. After the final TaskGroup
  cancellation routing adjustment, two ARMv6/Dyncom source controls and two
  canonical-SDK controls passed; the whole architecture/backend matrix was
  last run immediately before that narrow adjustment.
* Exception-capable guest work advanced without changing the default profile.
  An isolated ARM probe retains `.ARM.extab`/`.ARM.exidx`, an original-layout
  four-word Symbian exception descriptor, and E32 header offset. The converter
  now validates those bounds and supports a real ARM branch to the pinned
  Symbian EHABI personality export. A no-throw landing-pad/cleanup probe and
  changed-result control execute on ARMv5T/ARMv6 with Dyncom/Dynarmic (eight
  passed); two installed-archive cases also pass. A typed throw still requires
  `_ZTIi` imported object data through `R_ARM_GLOB_DAT`, which the resolver
  explicitly rejects; a maintained negative test confirms that gate. There is
  no advertised exception-enabled application profile yet. All eight macOS
  root CTest targets pass after updating the starter bridge test source list.
  The final focused Pytest run passed nine cases (eight execution controls and
  the typed-throw negative gate). The refreshed 2,496-file visible SDK and its
  retained predecessor both verify by digest; the previous tree is at
  `~/dev/symbian-sdk-before-exception-metadata-20261001`. The exported SDK's
  own Python/native module converted and inspected a descriptor-bearing ELF.
  A final E32 regression rejects a renamed ARM exception-index section without
  a descriptor. The rebuilt visible SDK includes that check; its immediately
  preceding verified tree is retained at
  `~/dev/symbian-sdk-before-exidx-validation-20261001`.
* Original LLVM libc++ `memory.cpp`, `thread.cpp`, mutex, condition-variable
  and future-state sources now build with a threaded guest configuration.
  `unique_ptr`/`shared_ptr`/`weak_ptr` lifetime and a `std::thread` worker
  execute with changed-result controls on both ARM targets and both emulator
  backends (16 passed). The thread case moves a unique owner into the worker,
  copies/releases shared ownership, joins, and checks 4,000 atomic increments
  and balanced allocation cells. The same 16 cases pass against a fresh
  2,496-file digest-valid SDK export using its prebuilt archive and standard
  EUSER/pthread/C++ ABI proxies. The generated starter now puts the C ABI
  boundary in `app_bridge.cc`; `model.h`/`model.cc` expose typed application
  code without opaque pointers or C linkage. Its copied/relocated build and
  four live GUI/allocation-failure cases pass. Standard futures still need
  exception-pointer/error-category support. Guest exceptions are a required
  opt-in profile, with the SDK default off; ARM unwind tables, Symbian's
  exception descriptor and throw/catch controls are current gates. Host native
  libraries retain their no-exceptions Status policy. Actual A11
  Future/Task/fibers remain open.
  The final 2,496-file export and visible `~/dev/symbian-sdk` both pass their
  digest audits; the previous 2,428-file tree is retained at
  `~/dev/symbian-sdk-before-threads-20261001`. Installed `Symbian::Threads`
  execution passed two ARMv6/Dyncom controls, generated wizard/copy/build
  tests passed, and all eight macOS root CTest targets passed.
* A bounded secondary-thread prerequisite now executes on the preserved
  RM-807 fixture. Primary and worker `RThread`s each perform 2,000 shared
  32-bit atomic increments; normal and changed-result controls pass all eight
  ARMv5T/ARMv6 × Dyncom/Dynarmic cases. The worker uses its own Symbian heap,
  and the parent drains `Logon`, checks exit reason/type, then closes the handle.
  Generated starter startup now accepts the OS secondary-thread entry while
  keeping process-global initialization on the primary thread. The fourteenth
  ordered EKA2L1 patch maps an observed Belle `RThread::ExitReason` SVC 0x34;
  its applied/replay checks pass. The same eight cases pass against the freshly
  exported SDK's prebuilt runtime archive and standard EUSER proxy; a generated
  project builds after SDK copy and project relocation. The visible 2,428-file
  SDK and its retained predecessor are digest-valid. This does not establish A11 tasks/fibers,
  general thread/TLS cleanup or hardware speed. Measured versus hypothetical
  costs are tracked in [PERFORMANCE_CONSIDERATIONS.md](../PERFORMANCE_CONSIDERATIONS.md).
* Bounded global C++ lifetime now executes: SDK EXE startup runs `.init_array`
  after heap setup and `__cxa_finalize`/`.fini_array` before exit; real
  `std::string` globals pass both ARM profiles and emulator backends. The SDK's
  default C++ DLL entry similarly runs a constructor on the actual Belle
  process-attach call list. Eight maintained DLL execution cases pass, including
  a changed-constructor negative control. The process-attach path required an
  explicit relocatable ARM-to-Thumb entry target and an ordered EKA2L1 patch
  mapping the observed Belle 0x10D library-entry-start operation. DLL detach,
  TLS, local-static guards, hidden/internal data and broader C services remain
  open. The direct test launcher had omitted Qt's macOS foreground-transform
  flag; with both SDK launch flags, the foreground app stayed unchanged across
  the live DLL run. A shared helper now supplies both flags to launchers. The
  28-case guest runtime execution matrix passed; four original-validator cases
  passed after correcting an obsolete exact-BSS-size assertion for the added
  destructor registry. The 12 ordered emulator patches replay from the pinned
  revision. Fourteen generated-project/library tests and all eight root CTest
  targets pass. The visible `~/dev/symbian-sdk` has 2,427 verified payloads;
  its prior sealed tree is preserved at
  `~/dev/symbian-sdk-before-global-lifetime-20261001`.
* The dynamic-library gate advanced: native E32 conversion accepts bounded
  DLL data/BSS and typed relocations; an independent EKA2L1 loader executes a
  frozen-export DLL with initialized data, zeroed BSS and GOT fixups twice in
  fresh processes on Dyncom and Dynarmic. Four oracle cases and two reproducible
  Python build cases pass. The eleventh replayable EKA2L1 patch fixes a Dyncom
  post-exit fetch after the killed process is unmapped. The installed SDK now
  supplies an ARM C compiler, `symbian_add_dynamic_library`, exact frozen DEF
  conversion, optional import proxies and retained ELF symbols. ARMv5T/ARMv6 C
  DLL and consumer-import CMake tests pass. The user's Mbed TLS adaptation's
  full 80-object `mbedcrypto` C archive builds with the SDK compiler, and a
  SHA-256 subset links/converts to a DLL with selected EUSER imports. The full
  library has not executed in a guest; internal/hidden writable globals,
  DLL detach/TLS/lifetime and module debugging remain open. Direct GUI test
  launchers use the shared nonactivating environment and nonbundle symlink.
  The 19-test focused
  DLL/library group, optional actual Mbed TLS integration, eight root CTest
  targets, Black/Ruff and patch replay checks pass. The visible
  `~/dev/symbian-sdk` was deliberately rebuilt and all 2,422 sealed payload
  files verify; its previous unmodified 2,420-file tree is retained at
  `~/dev/symbian-sdk-before-dll-data-20261001`.

* The latest full optional-input run passed **270 tests**, with one inherited
  Starlette warning (`.symbian/arm-full-background-verified.log`, 428.58 s).
  The visible `~/dev/symbian-sdk` was deliberately refreshed to the tested
  ARMv5T/ARMv6, ROM/Z and static-archive payload (2,420 verified files); its
  previous unmodified 2,410-file tree is retained at
  `~/dev/symbian-sdk-before-arm-profiles-20261001`. Installed commands resolve
  E6, E71, 7610, C7 and 808 firmware IDs. An installed-SDK `symbian init`
  generated and initially built an E6 ARMv6 app, recording RM-609 selection
  without changing owner applications. The optional GUI run and all eight root
  CTest targets pass; full-screen Space placement remains unverified.
* SDK-owned macOS emulator sessions now start from a private symlink outside
  the `.app` bundle and use the tenth replayable EKA2L1 patch to show/order Qt
  and OpenGL windows without requesting focus. A real GUI launch rendered a
  720×1280 capture while iTerm2 remained frontmost; the directly owned child
  was reaped. The OpenGL window is assigned normal managed desktop-Space
  behavior, excluding full-screen auxiliary display and tiling. The same
  launch path is used by direct GUI/runtime/debug tests. After rebuilding the
  patch, a Dynarmic GUI test passed and Chrome PID 60698 stayed frontmost
  before, during and after it. Actual placement while another app is
  full-screen and Linux focus remain validation gates.
* ARMv6 is the default for new builds and `symbian init`; developers can select
  ARMv5T in the wizard or with `--architecture`. Generated projects keep one
  architecture setting, use its matching guest runtime archive and report an
  unsupported ELF/ABI before publishing or launching. Existing ARMv5T projects
  retain their selection. A real ARMv6 `REV` and the ARMv5T software sequence
  execute on Dynarmic and Dyncom; the full runtime matrix passes 28 cases
  including independent original E32 validation. The root CMake file API now
  supplies an explicit ARM target to CLion's guest compiler probe while host
  tools remain native arm64; both compiler probes and all eight CTest targets
  pass. The architecture/A11 source checks pass 14 tests.
* Bounded writable EXE data/BSS now runs on Dynarmic and Dyncom: initialized
  values, 64 zero-filled BSS words, data/BSS mutations, code/data pointers and
  a Thumb function pointer. A changed initial value exits -115. Twelve runtime
  execution cases plus two independent original checksum/whole-image-validator
  checks pass (14 tests, 57.97 seconds). Four new native data/fixup tests and all
  eight root CTest targets pass. New-project linker layouts include the verified
  RW mapping. Bounded global constructors/destructors now execute; TLS and full
  DLL lifetime remain unsupported. Artifacts: `.symbian/runtime-data-oracles*`.
* Actual A11 thread/concurrency adoption has begun: 46 original licensed files,
  92 local include edges, per-file digests, original tests, no adaptations yet.
  Original Git comparison, CMake source checking and five tamper/closure tests
  pass. This is source staging, **not a built guest concurrency backend**;
  Boost forced unwind/exception boundaries require the planned explicit
  adaptation. Guest Abseil, shared ownership/atomics, static lifetime and OS
  thread/TLS prerequisites remain gates. No scheduler lookalike or premature
  tasks/futures/fibers examples were added.

* Shared ROM/Z onboarding is implemented: native original EKA2L1 archive,
  ROM/RPKG, extracted Z and VPL readers; portable content IDs/bundles; XDG data
  storage and retained cache evidence; global -> SDK -> project -> command
  precedence with explicit origins/mappings. SDK/build/init require no firmware.
  Run/Debug no longer hard-code RM-807 paths, and select its workaround only for
  the exact known pair. Six other dump imports are tested; C7, E6, 6120 and E71
  generated starters pass initial build, greeting, clock input and native normal
  exit using the default profile. 7610/P900 import, with a specific missing EKA1
  startup/import ABI error before application launch. Default fonts, compact
  layout and logical capture dimensions remove 808-only display assumptions.
  Unknown FBS requests return KErrNotSupported instead of hanging; opcode 0x2C
  itself remains unimplemented. Both new upstream patches replay/reverse exactly.
  Firmware checkpoint full optional-input run: **234 passed**, one inherited Starlette warning,
  no skips, 335.11 seconds. Eight root CTest targets and five native firmware
  GTests pass. Logs/artifacts: `.symbian/firmware-full-verified*`; earlier failed
  captures, the 6120 teardown hang and import diagnostics remain retained.
  See [FIRMWARE.md](FIRMWARE.md) for commands, portability, limits and migration.

* Bounded local GOT conversion now emits E32 text fixups for up to 1,024
  defined object/function slots, preserving Thumb state and linked GOT_PREL
  words. Eight runtime cases pass on the preserved Delight fixture: ordinary
  success/OOM, global std::nothrow plus constant/function GOT success, and a
  changed-constant failure on each backend. Six new native GOT tests and seven
  original-validator checks pass. Generated apps now acquire their model with
  std::nothrow and return -4 after closing native resources on failure; all
  fifteen project-generation/build checks pass, including both CPU backends
  and terminal GDB. The visible SDK was deliberately refreshed with its previous
  tree retained; owner application sources/settings were not rewritten.
  GOT checkpoint full optional-input run: 208 passed, one inherited Starlette warning,
  no skips (181.74 seconds); eight root CTest targets pass. Black/Ruff, changed
  C++ formatting and whitespace checks pass. Integrity checks confirm the
  original archive, all 13,438 golden files and all 2,408 current SDK digests.
* `symbian sdk install` exports visible target headers, matching libc++ runtime,
  ordinal proxies, CMake package, Python utilities/native modules, notices and
  command shims. Projects keep one local SDK setting and relative shared files;
  commands dispatch to the selected SDK, including its project templates.
* `symbian init` asks for identity and IDE preferences, builds initially, and
  generates actual CMake workspace/C++ module registration, an enabled stable IDE profile,
  Run executable and remote-debug profile selection. The initially missing
  workspace registration caused IntelliJ to open a generic module despite valid
  terminal CMake/clangd results. After repair, the actual reopened owner project
  `symbian-app-3` configures in the IDE and reports three sources, zero unknown.
  Earlier owner projects were repaired without replacing application sources.
  Generated Run/Stop and GDB are execution-tested; IDE toolbar/debugger interaction
  and full unwind coverage remain distinct gates. See PROJECTS.md.
* The hello/time starter renders original W32 font text, uses real home-time
  services, std::string/std::vector, Clear/Exit, focus and pending-request cleanup.
  Both CPU backends pass rendered tap/log/clear/normal-exit checks. Real GDB stops
  in the modern model and exposes clock arguments/source. Project/SDK copies,
  relative SDK selection and selected-SDK template dispatch pass integration tests.
* The preceding full verification passed 202 Pytest cases with all optional inputs and
  one inherited Starlette deprecation warning. Following the IDE registration
  amendment, all thirteen project-generation/build cases pass; eight root CTest
  targets pass. The installed macOS wheel audit includes the new templates.
  Updated Linux aarch64 checks pass seven host CTest targets, installed wheel
  closure/resource audit and 62 installed Python cases; Linux emulator remains
  unverified. Earlier test totals below describe earlier checkpoints.

* Root CMake exposes the real ARM `gui_app`, its `gui_app_e32` publisher and
  a native `gui_app_run` executable. The saved root GUI Run configuration now
  explicitly selects that executable. Terminal launch/Stop/normal SDK exit and
  GDB batch/MI relocation pass; actual reloaded IDE UI interaction is unverified.
* A11's actual native/Python Status/StatusOr bridge is ported, with canonical
  caster modules, payloads, Pydantic hooks, GIL helpers, provenance and licenses.
  Native codecs/tests remain exception-free. Python initializers expose shortcuts
  and serializable metadata uses Pydantic. See A11_STATUS.md for adaptations and
  inherited mapping/compact-MessagePack limits.
* A real bounded libc++ runtime executes heap-backed strings/vectors and cleanup
  on both emulator CPU backends; actual heap exhaustion/nothrow/fatal failure
  controls pass. Over-aligned allocation and original ARM compiler-rt division/
  remainder execute in the maintained probe. LLVM sources are pinned with one
  replayable Symbian atomic-query patch. See RUNTIME.md and
  ../CXX_CAVEATS.md. Bounded GOT, writable data and initialization now execute;
  general TLS and full hosted C++20 remain unsupported. Guest Abseil/JSON is
  not provided by the host wheel.
* Linux aarch64 CPython 3.12 host native tests, wheel, installed closure/resource
  audit and 62 Pytest cases pass. macOS's installed wheel audit passes too.
  x86_64/other CPython and Linux emulator/guest/debugger are configured/planned,
  not locally verified. DISTRIBUTION.md describes the one-install payload design;
  compiler/emulator/header/debugger payload wheels are not yet distributed.
* Earlier baseline verification passed 152 Pytest cases (40 optional skips) and seven
  root CTest targets. Runtime tests pass four firmware-backed cases plus a
  rejected GOT control; original validator acceptance/failure checks pass three
  GTests. Black/Ruff, generated stubs and root target-aware clangd checks pass.
  The copied HTTP tests emit a visible Starlette/httpx deprecation warning.

Earlier verification totals below are historical checkpoints; current evidence
and precise scope are recorded in RESEARCH_LOG.md.

| PLAN milestone | Status | Required evidence |
| --- | --- | --- |
| 0 Preservation | Archive tools tested; physical baseline pending | Device inventory, original artifacts, offline archive, tested human recovery appliance |
| 1 Toolchain | Reproducible ELF→E32; historical validation and ROMless process/DLL import tests pass | Matched Belle runtime, SDK imports and complete target ABI tests |
| 2 Project model | CMake/Ninja, persistent database, native SIS and ROMless install/launch pass | Matched SDK/DLL imports, general application support and Belle installer |
| 3 Emulator | Native arm64 build, ROM/Z import and real GUI pixels/input/zero exit pass on both backends | Lifecycle/reset/snapshot facade, full OS boot and general runtime coverage |
| 4 Automated development | Native build/package/check loops and opt-in rendered GUI tests pass | General unattended install/run/artifact loop and symbian test |
| 5 Modern debugging | Live ARM GDB stops/stepping, model inspection and native process-exit records pass | Full unwinding, panic/thread/module inspection and CLion debugger validation |
| 6–9 | Pending | Physical deployment/system/hardware/alternative OS gates in PLAN.md |

No physical-device executor, flashing capability or recovery automation exists.
Source inspection is not an emulator runtime test. Record implementation and
verification results in RESEARCH_LOG.md before changing milestone status.

Current working commands: `doctor`, `toolchain probe`, `toolchain verify-probe`, `toolchain verify-pointers`,
`toolchain verify-package`, `toolchain import-proxy`, experimental
`build`/`package`, ELF/E32/SIS/import-proxy `inspect`, `preserve create/verify`,
`emu status`, `emu screenshot`, `emu pointer`, and informational `device policy`.
The emu commands require an explicitly started private research endpoint; they
are not a general lifecycle API or a physical transport.
The e32_probe example has no SDK/imports/data/constructors; its direct thread
exit is a no-resource experiment. Both the linked ELF and converted E32 repeat
byte-for-byte in two builds on this host. Parser acceptance does not prove
Symbian loader compatibility. Reports retain loader/runtime verification false.
Artifacts and reports live under .symbian/. clangd consumed the generated ARM
compilation database with zero errors. Native core code is compiled with
exceptions disabled; the pybind11 boundary retains canonical status codes.

Verification: 112 Pytest cases pass with the patched emulator, native oracles
and public kernel source supplied. The 41 platform GTest cases and 48 independent
oracle GTest cases pass on Apple Silicon. The preceding emulator checkpoint
passed 288 upstream EKA2L1 cases; its sources are unchanged here. Black/Ruff,
clang-format and generated stubs pass; clangd target checks have zero errors.
Without optional dependency paths, four emulator smoke cases and two native
verification integration cases and one public-header research case skip.
One additional development DLL runtime case and three variable-header DLL
validation cases need the native oracles. Two additional pointer/combined-layout
cases need them.
EKA2L1's parser omits header CRC verification;
Nokia's original checksum and whole-image validator independently check it.
The unchanged historical validator accepts this image using host type adapters.
Both EKA2L1 CPU backends execute its ARM startup and Thumb C++ at two load
addresses; a changed input produces the expected failure exit. These are
ROMless CPU tests: the exit SVC is observed by their callback. Eight additional
cases use EKA2L1's real process loader, flexible memory model, scheduler and
kernel SVC dispatch under its epoc10 profile. Both backends return zero normally
and 42 for changed input; absent executables fail to create a process. Both
backends also launch another process after an earlier failure exit.
The process/thread exit states and address-space release are checked. This is
an import-free emulator process without Belle ROM/Z or system services.
Reports distinguish eka2l1_process_verified from Belle loader/runtime flags,
which remain false. No physical runtime ran.
Build and patch replay instructions are in research/eka2l1/README.md.

The CMake path preserves the baseline ELF/E32 bytes. Multi-source builds, header
changes, paths with spaces and cached no-op builds are tested. Its compilation
database points to existing build objects; clangd reports zero errors. The wheel
contains the CMake modules and builds the same E32 from an isolated installation.
Project instructions and the v2 build report are described in docs/BUILDING.md.

The unsigned native SISX package is reproducible and passes independent Nokia
UID/controller/data CRC checks. Six disposable EKA2L1 cases install the unchanged
probe, launch through the kernel on both CPU backends, reload its registry,
uninstall and reinstall. The registry's file hash agrees with an independent
hashlib baseline. That installer does not enforce phone signing/capability policy.
The production native inspector verifies checksums, SHA-1, E32 and the restricted
canonical profile before the trusted experiment. Reports retain Belle runtime
and phone installation flags false. See PACKAGING.md for profile limits and replay.
OpenSSL 3 is statically linked for SHA-1; its license is packaged with the wheel.

The native SDK component reads frozen EABI definitions and generates selected
function ordinal proxies using Clang/LLD. The public User::Exit slot remains 641,
and Nokia's unchanged ordinal lookup method agrees in a separate optional test.
An original e32std.h call compiles and links, typed layout assertions pass, and
clangd reports zero errors. The public request status is eight bytes with flags.
Proxy and link ELF builds repeat; SDK/source/header dependencies are recorded.
An isolated wheel installation includes the target probe resource and builds
the same proxy and linked ELF.
These are link contracts, not a verified 808 SDK. See SDK.md for replay.

The E32 import profile places function GOT slots in its code region and resolves
versioned symbols against original proxy ordinals. A compiled development DLL at
ordinal 7 executes on both EKA2L1 CPU backends; the patched slot, function PC,
changed-input failure and repeated launch are checked in six cases. Historical
validation/checksums accept both files. Two-DLL links and malformed controls pass.
An isolated installed wheel reproduces the same imported ELF/E32 and metadata.
General writable-data DLL support, matched SDK services, startup/cleanup and
Belle runtime remain open. See IMPORTS.md.


Native DLL conversion now resolves frozen function exports from retained ELF
symbols, preserves ordinal gaps/ABSENT entries, and emits the count prefix,
full absence bitmap and code relocations for all export pointers. No fixed
function address is required. Independent validation covers ordinals 7, 641 and
65,535, including complete variable headers and relocation pages. Both emulator
backends also verify the mapped table: present pointers agree with lookup,
absent pointers relocate to the entry, and the count word remains unchanged.
Combined import/export layout passes Nokia's checksum and whole-image validator;
its executable startup is not runnable DLL initialization evidence.

The earlier research DLL omitted the count prefix and export-pointer relocations.
Its successful structural validation and export lookup proved less than the
public ELF loader contract. It remains research material; maintained runtime
checks now use examples/dll_probe and the native converter. The installed wheel
reproduces the native DLL and ELF and their metadata. Evidence is in
.symbian/native-dll/report.json, verification-report.json and wheel-result.json.
General pointer relocations, writable data/BSS/TLS, constructors, SDK startup,
matched target DLLs and Belle runtime remain open. No device operation ran.


Retained internal ABS32 words now generate E32 text relocations, permitting
named RELRO tables within the RX mapping. A maintained multi-source C++ probe
executes an ARM callback, a Thumb callback and a virtual method, and reads a
constant-data pointer with an addend. Both emulator backends verify all four
mapped words and instruction state at the three indirect targets, changed-input
failure, repeated launch and address-space release. Its native SIS installs,
launches, reloads the registry, uninstalls and reinstalls in separate cases.
The new verify-pointers command retains 14 image/dispatch checks or 21 with the
package. An isolated wheel reproduces ELF/E32/SIS and repeats all 21 checks.
Evidence is in .symbian/pointer-probe, .symbian/pointer-package and
.symbian/pointer-check. See POINTERS.md for replay and the trusted-link contract.

Internal pointer relocations also coexist with eager imports and frozen exports
in independently validated layout cases. External absolute pointers, GOT_PREL,
writable data/BSS/TLS and global lifetime support remain open. Simple virtual
dispatch does not prove the full target C++ ABI. Matched Belle and physical
execution are still unverified; no device operation ran.


C++20 now has maintained language and named-module examples. Concepts/requires,
structural class arguments, consteval/constinit, designated initialization,
constrained lambdas, equality and small layout contracts compile with explicit
controls. The language probe passes 21 independent loader/installer cases. A
separately configured libc++ bit/concepts/span experiment produces distinct
machine code and passes the same loop without linking a hosted runtime.
The module example uses upstream Clang/CMake scanning, retains its BMI, checks
importer invalidation and passes the 35-case image/package loop. An isolated
installed wheel reproduces all three ELF/E32/SIS variants and all 77 checks.

All 124 Pytest cases pass with explicit optional compiler/header/oracle inputs;
the native platform and SDK ordinal checks pass. Evidence is under
.symbian/cxx20-probe, cxx20-check, cxx20-library, cxx20-library-check,
cxx20-module and cxx20-module-check. See CXX20.md. Complete standard library,
coroutine/thread/atomic runtime, global lifetime, writable data/BSS/TLS, SDK
services and matched Belle remain open. No physical-device operation ran.


The requested examples/gui_app and root WALKTHROUGH.md are now present. The
counter application directly uses Window Server; it has seven-segment drawing,
touch increment/reset/exit and explicit request cancellation/object cleanup.
Its primary-thread adapter attempts SDK heap/process setup and User::Exit,
while secondary-thread, exception-entry and global lifetime support remain
absent. The build report now correctly treats startup/cleanup as unverified
project behavior instead of assuming every executable uses direct ThreadKill.

At that checkpoint, the source profile staged 92 original header aliases and
native frozen proxies
for nine EUSER and 28 WS32 functions from pinned ignored public trees. Preparation
checks file digests, preserves original files/licenses, rejects malformed
profiles and output redirection, and records SDK/runtime verification false.
Clang/LLD and independent CMake trees reproduce the ARM ELF and E32. Five model
GTests pass, including wide/tall layouts, arithmetic, input bounds and saturation.
Original checksum/validator sources accept the generated GUI in eight cases.
DWARF verifies and LLDB resolves functions and source lines; clangd has no
diagnostics with its limited check-mode refactoring selection.

All 139 Pytest cases passed with explicit optional inputs; all six root CTest
targets passed. The final drawing-layout adjustment also passed the five model
GTests and all 15 GUI Pytest cases. An isolated installed wheel runs the SDK
preparation/build/verification CLI, reproduces both artifacts and repeats all
eight independent image checks. Evidence lives in .symbian/gui-sdk,
.symbian/gui-app, .symbian/gui-validation and .symbian/gui-research.

Visible GUI execution, actual Belle EUSER/WS32 compatibility, heap/cleanup and
guest debugger attachment remain unverified because matched ROM/Z is absent.
The walkthrough labels these procedures as future experiments. The example
has no application registration or SIS package; the existing SIS writer's
import-free restriction remains. No emulator OS boot or physical-device action
is claimed or performed. Phone RM/product/firmware details remain unknown.


The GUI now has a native single-EXE unsigned SIS package, independently validated
in 17 image/checksum/installer cases. Actual installer/registry behavior verifies
exact payload bytes, UID/SID/version and an independent legacy digest, reload,
uninstall and reinstall in disposable ROMless filesystems. A control records
the unchanged upstream loader's missing-library defect: it creates a process
with all 37 imported words still unresolved. No guest instructions execute in
these installer cases. Reports explicitly keep GUI/SDK execution and matched
loader/runtime verification false. The native writer now accepts imported EXEs
while still rejecting DLL payloads, resources and broader package profiles.
All 142 Pytest cases and six root CTest targets pass; the installed wheel
reproduces ELF/E32/SIS and repeats the 17-case package check.

The supplied Delight v1.8 ZIP was checked and staged privately. Its VPL declares
RM-807, product 059M7Q4 and version 113.010.1508; seven required/present files
pass archive and declared CRC checks. One opt-in native GTest successfully uses
EKA2L1's VPL/FPSX/ROM/ROFS/FAT importer into a new isolated root. The result
identifies Nokia/808 PureView/RM-807/epoc100 and supplies a ROM and actual system
DLLs, with 13,438 imported files inventoried by SHA-256. This corrects the prior
missing-assets state. Custom archive authenticity, stock recovery baseline and
matching this physical phone are not established.

A copied instance maps the GUI at 0x70000000, EUSER at 0x804bcce8 and WS32 at
0x80a4c028. It logs unimplemented SVCs 0x51/0xF7 and a $HEAP lookup failure;
visual behavior, correct startup/cleanup and debugger attachment are not yet
verified. TERM did not finish the private emulator, so its confirmed process
was stopped with KILL after retaining logs. The original imported root remains
separate from runtime state. No physical phone operation ran. Evidence is in
.symbian/gui-package[-check], gui-research/delight-archive-check.json,
delight-import.json, delight-import-inventory.json and the private instance logs.

Live ARM GDB 17.2 attachment now works against a disposable RM-807 instance.
The actual startup source breakpoint receives reason=0 and info=0x40ffc0 at
PC=0x700009da. A local GPL guest-debug-step.patch fixes silent execution after
single stepping. A real frontend/GDB Pytest proves two successive Thumb stops,
a stable fresh register read after a delay, source display and ROM SVC stops.
The same test fails with a 30-second GDB timeout when that patch is removed;
restoring it passes. All 143 Pytest cases with explicit optional inputs and all
six root CTest targets pass. This is live debugging evidence, not GUI success.

Stable registers show heap initialization returns KErrNotFound (-1), before
GuiMain. The ROM requests kernel HAL page size using SVC 0x51 and chunk creation
using 0x6D; the pinned epoc10 table maps those operations to 0x4F and 0x6B and
dispatches 0x6D as object lookup. The real exit path reaches unimplemented
0xF7, while its handler is registered at 0xF6. These firmware executive ABI
discrepancies require a fuller independently checked Belle profile. No whole
table shift, SDK replacement, or loader workaround was introduced.

Visible GUI output/input, successful heap setup, normal SDK exit, stack unwinding
and OS boot remain unverified. The test-owned frontend still requires KILL
after TERM; reaching an exit wrapper does not establish guest cleanup. The
original ZIP and all 13,438 baseline file digests are rechecked unchanged.
Physical phone details remain unknown and no device operation ran. Replays and
current limitations are in WALKTHROUGH.md section 9; transcripts, negative
control, test logs and integrity evidence are in .symbian/gui-research/debugger*.


### 2026-09-30 — Guarded RM-807 ABI and initial drawing-function execution

The real ROM export probe and source-wrapper comparison now support a piecewise
experimental Symbian 101 executive map. It has 170 existing handlers, while the
original 172-handler epoc10 map remains intact. Profile selection is explicit
and exact-ROM-digest guarded; real frontend tests reject unknown profile names
and changed private ROM bytes. This is a research profile for the supplied
Delight image, not universal Belle support or authenticated stock firmware.

With that profile heap setup returns zero and GuiMain executes. A separate ARM
TPIDRURO register fixes the original Dynarmic coprocessor abort, with four real
instruction/context cases passing on both tested macOS backends. Window Server
connection returns zero. A subsequent E32USER-CBase/69 panic exposed missing
cleanup-stack setup; the example now creates the SDK CTrapCleanup before GuiMain
and deletes it on return. Its frozen EUSER import count is now ten (38 total).

Live GDB verifies the initial DrawGui entry, zero/running model, 360 by 640 layout
and return after its guest drawing calls. The default map still fails heap
startup, preserving a useful control. Full DLL initialization remains unproven:
0x10D is still unimplemented. Rendered pixels, pointer delivery, normal cleanup/
exit, full unwinding, OS boot and physical-phone compatibility remain unverified.
No screenshot or visible-GUI success is claimed from a drawing-function stop.

All 146 Pytest cases pass with explicit optional inputs, all six root CTest
targets pass, and the two new research CTest targets pass seven GTest cases.
The opt-in ROM export probe passes separately, rejects existing/nested output,
and executes no guest instructions. All six patches apply in documented order
against fresh pinned source files. An isolated installed wheel reproduces the
updated ELF/E32, packages it and passes all 17 historical/installer checks;
research tests/firmware remain excluded. Black/Ruff and C++ formatting pass.

The original ZIP and every path, size and SHA-256 of the 13,438-file unbooted
baseline are rechecked unchanged. Runtime work uses copied instances; the
baseline is not booted. Phone identity and independent offline preservation
remain unknown. No device operation ran. Replays and boundaries are recorded in
WALKTHROUGH.md and docs/BELLE_ABI.md; private evidence is under
.symbian/belle-abi-research. The platform mission remains active.


### 2026-10-01 — Rendered GUI, pointer input, normal exit and developer workflow

The native GPL research adapter reads the actual guest screen texture, routes
logical pointer events through Window Server and copies kernel process-exit
records. It runs on existing Qt/kernel loops, adds no scheduler/thread/Python
callback and compiles with exceptions disabled using Abseil Status/StatusOr.
The wheel contains only the synchronous Python policy client for this endpoint;
EKA2L1 and the native adapter remain separate research binaries.

Live tests on Dynarmic and Dyncom verify 720x1280 portrait PNGs (logical 360x640,
scale two), 0000 → 0001 → 0002, an outside tap leaving 0002, reset to 0000 and
exit type kill/0 with reason zero for UID 0xe0000811. The frontend also exits
zero. Four native GTests exercise disabled/invalid/private-path startup bounds;
live tests reject malformed/oversized commands, traversal, duplicate outputs and
invalid pointers. Two observed locking/lifecycle mistakes were corrected:
pointer delivery must not retain the kernel lock that Window Server acquires,
and callbacks must detach before the OS worker destroys the kernel. Final native
status survives socket closure; saved reports are explicitly selected.

The GUI preset now provides both SDK import proxies without CLI injection.
Standalone CMake configure/build and clangd parsing/indexing pass with zero
errors using the limited documented tweak selection. Canonical ELF/E32 rebuilds
retain their previous hashes. The running IntelliJ IDEA/CLion-plugin instance
now has persisted Symbian ARM toolchain settings and a local clion-arm preset.
The exact local preset configures/builds and clangd reports zero errors. The GUI
project now configures successfully in the actual IDE. Its generated CMake API
lists app.cc, startup.cc and startup.S; the initial model resolves two C++
sources and one assembly source with zero unknown sources. The live preset
selects existing Default with explicit ARM paths because the running IDE had
not loaded the saved application toolchain name. The generated native GDB
profile selects the ARM debugger independently and does not need that restart.
The debugger frontend is untested. Setup, E32 publication and ARM remote-debug steps are in docs/CLION.md.

The initial control checkpoint passed 161 Pytest cases with explicit optional
inputs; the launcher checkpoint now passes all 170. All six root CTest
targets pass, and the three control/routing/register research targets pass eleven
GTests. Seven patches replay against 15 fresh pinned source files. Black/Ruff,
clang-format and whitespace checks pass. An isolated installed wheel outside
the source tree reads the native final exit envelope and inspects the actual
GUI E32 through its native extension. Its module paths, wheel digest and reports
are in .symbian/clion-setup/wheel-result.json. The research native
adapter and tests are absent from the wheel; Pillow is a development dependency.
The original ZIP and all 13,438 baseline paths/sizes/SHA-256 values are rechecked
unchanged. No physical-device operation ran; identity and independent offline
preservation remain unknown.

PLAN.md now records the completed vertical slices, evidence boundaries and
ordered next gates: developer onboarding, disposable lifecycle/symbian test,
unresolved ABI/DLL lifetime, bounded runtime/C++ library support, diagnostics,
then broader application/system/device scope. Full initialization (including
0x10D), 0xFF interception, writable data/TLS/static lifetime, full unwinding,
OS boot and physical compatibility remain unverified. The broader mission
remains active. Replays are in WALKTHROUGH.md and docs/EMULATOR_CONTROL.md;
private evidence is under .symbian/belle-abi-research/control* and gui-control*.

A bounded foreground GUI launcher now publishes the current E32 before Run or
Debug, copies the named golden, owns/reaps its frontend and retains manifests,
logs and final native status. Generated local GUI Run/GUI Debug configurations
and a native Symbian GUI GDB profile are installed in the dedicated GUI project.
The supervisor starts a halted instance before GDB, then relocates symbols after
connection using the actual mapping. Source breakpoint/variable checks and the
IDE's GDB MI2 protocol pass, including Thumb-to-ARM instruction stepping. Normal
Run exit, Stop cleanup, occupied ports and launcher-path quoting are checked.
IDE toolbar interaction and full debugger frontend/stack unwinding remain
unverified. This is not yet the general lifecycle/symbian test API.

The final full launcher/control suite passes 170 tests in 112.82 seconds with
all optional inputs enabled. Four targeted installer/ownership tests pass after
saving GUI Run as the default selection. Research CTest again passes all three
targets/eleven GTests. The installed wheel is exercised from /tmp, including
GDB version discovery without startup and default configuration selection.
The ZIP and all 13,438 baseline files remain unchanged.

Earlier full runs exposed an Exit-up request after the app had already closed
and one frontend that did not finish shutdown within 15 seconds despite guest
ThreadKill reason zero. Exit tests now send only down; the native adapter drains
already queued replies for a bounded interval when its Qt loop stops. Added
teardown phase logs and an owned-process stack sample on a repeat timeout.
Twelve repeated GUI runs, five six-case debugger/GUI groups and the final full
suite then pass. No repeat stack is available; the isolated shutdown timeout's
cause remains open rather than being inferred from later passes. IDE Stop still
has tested bounded cleanup.

The reopened GUI project scans 535 files and resolves its three target sources.
Root-project exclusions for private runtime/upstream/build data are saved with
the staged SDK unexcluded; their live application is unverified. IDE toolbar
interaction remains unautomated. PLAN.md retains these developer-lifecycle gates
before broader ABI/runtime and device scope.

The current runtime and DLL follow-up adds actual LLVM compiler-rt ARM EABI
64-bit signed/unsigned quotient and remainder, its shared C division core and
the ARMv5T-safe leading-zero helper. The expanded guest matrix passes 32 cases
on ARMv5T/ARMv6 and Dyncom/Dynarmic, including a changed-wide-result failure
control. A separate run with the optional oracle build passed four original
checksum/whole-image validator cases on the new ARM images.

The named RM-807 fixture now passes a bounded dynamic DLL lifetime test. The
real ROM `RLibrary::Load` initially reached an unimplemented Belle SVC 0x10E
and returned its unchanged `this` pointer. The thirteenth ordered emulator
patch maps that observed slot to EKA2L1's existing v10 load-preparation hook;
the loader-server operation, two ordinal lookups and `RLibrary::Close` then run.
A DLL destructor writes to client-owned memory before `Close` returns. The
maintained constructor, dynamic-load and absent-DLL matrix passes 16 cases
across both ARM profiles and emulator backends; absent DLL returns -1 and the
client exits -121. All 13 emulator patches replay from the pinned base and the
last reverse-checks. This does not prove all DLL modes, TLS or thread lifetime.

A direct genuine-A11 prerequisite probe compiled `std::make_shared` but failed
to link without libc++ shared-ownership definitions. The current libc++ profile
also disables threads, so merely adding `memory.cpp` would not establish A11's
atomic cross-thread ownership. C0 still needs a real thread/atomic backend and
guest Abseil before Promise/Future can be exposed. The pinned 46 A11 sources
and 92 local include edges still verify unchanged. No guest A11 scheduler or
fiber capability is claimed.

The visible `~/dev/symbian-sdk` was deliberately refreshed from the prepared
checkout after its previous 2,427 sealed files all matched their digests. The
previous SDK is retained at `~/dev/symbian-sdk-before-wide-dll-20261001`;
the new canonical SDK also has 2,427 verified sealed files. Its ARMv6 archive
contains the new `__aeabi_ldivmod`, `__aeabi_uldivmod` and `__clzsi2` symbols.
A dynamic DLL run against the refreshed SDK passed; a fresh `symbian init`
build/copy/relocation test also passed. No owner application source or UID was
changed.

The external user-owned `~/dev/mbedtls-symbian` sources were built without
editing them. A selected SHA-256 subset links into the SDK E32 DLL and now
executes through a dynamic guest `RLibrary` client: SHA-256 of `abc` matches the
known 32-byte digest, while changing the input fails with -132. Static format
and four Dynarmic/Dyncom execution/control tests passed (five total). This is
one useful Mbed TLS function, not broad TLS/network integration.

The native-lock prerequisite now has a guest execution probe: an EUSER
`RFastLock` is created, its uncontended `Poll` succeeds, a second `Poll` while
held returns `KErrTimedOut`, and `Signal`/`Wait`/close complete. Four ARMv5T /
ARMv6 client × Dynarmic/Dyncom cases passed in 71.31 seconds. This runs the
RM-807 ROM implementation for both client architectures; it does not prove a
separate ARMv5 ROM implementation or cross-thread contention. A11's fiber-aware
`thread::Mutex` still needs a backend that parks fibers instead of blocking
their event OS thread. Fast ARM context switching remains unimplemented.

The final development SDK export adds the real ROM `RLibrary` and `RFastLock`
imports to its standard EUSER proxy. Its 2,427 sealed files passed digest
verification; the installed-proxy fast-lock matrix passed four cases and the
Mbed TLS SHA-256 DLL suite passed five. The export was promoted to
`~/dev/symbian-sdk` after checking the previous canonical SDK's digests; that
previous tree remains at `~/dev/symbian-sdk-before-fast-primitives-20261001`.
Both retained and current trees have 2,427 digest-valid files. A fresh
generated-project initial-build, SDK copy and project-relocation test against
the canonical SDK passed in 10.41 seconds.

A further C0 runtime gate now executes 32-bit `std::atomic` fetch-add, acquire
load, failed/successful CAS and exchange. Clang emitted `__atomic_*` libcalls
for ARMv5T and `__sync_*` libcalls for ARMv6; both are backed by real EUSER
ordered atomic imports through a narrow original-header bridge. The normal
and changed-value controls passed eight cases across both client targets and
both emulator CPU backends in 83.77 seconds. A fresh installed SDK with the
new standard proxy and `e32atomics.h` passed the same eight cases in 79.98
seconds; its 2,428 files are digest-valid. These are single-thread paths.
Cross-thread race behavior, 64-bit atomics, libc++ shared ownership and the
A11 task/fiber backend remain unverified.

The 2,428-file atomic-enabled export was promoted to `~/dev/symbian-sdk`
after the previous 2,427-file canonical tree passed its digest check. That
previous SDK is preserved at `~/dev/symbian-sdk-before-atomics-20261001`;
both current and backup trees pass their sealed-file checks. A fresh
generated-project initial-build, SDK copy and project-relocation test against
the new canonical SDK passed in 7.42 seconds. The source profile now stages
93 pinned header aliases, including original `e32atomics.h`.
The real-header GUI source-staging/link test passed with the updated count
in 10.75 seconds.
Finally, a stricter test copied the runtime probe into a temporary project,
linked the installed SDK's prebuilt `Symbian::Runtime` archive and standard
EUSER proxy, and used its installed `symbian/runtime.h`. All eight atomic
normal/changed-control guest executions passed in 85.85 seconds. The earlier
79.98-second selected-SDK run had used the installed proxy with a runtime
rebuilt from this checkout; the stricter run verifies the exported archive.
Four default string/vector runtime cases then passed on the same prebuilt
archive across both ARM profiles and emulator CPU backends in 49.80 seconds.

The 64-bit atomic C0 prerequisite now has two complete runtime archives. The
default `Symbian::Runtime` uses an original EUSER `RFastLock` to serialize
64-bit load/store/CAS/exchange/add across threads. The opt-in
`Symbian::NativeAtomics64` calls original EUSER 64-bit exports through a
matching proxy. The preserved RM-807 ROM executes LDREXD/STREXD for these
operations. Dyncom had assembled the STREXD register pair but written its
register index; the fifteenth ordered EKA2L1 patch fixes that argument. A
ROM-independent STREXD GTest passes on both backends. This was an emulator
fault, not evidence of bad ROM atomics.

Original Symbian source also has an ARM V5/V6 interrupt-masking implementation,
so native EUSER imports are not automatically lock-free on other firmware.
The opt-in native profile was initially verified on the named RM-807 fixture
and was later checked on RM-675/RM-609 as recorded below. Clang's
ARMv6 `is_lock_free` builtin falsely described the default lock-backed
archive; a maintained pinned LLVM header patch now directs `std::atomic` and
`std::atomic_ref` runtime queries to the selected archive. The 64-bit probe
checks actual parent/worker high-and-low-word updates, direct `__sync` calls,
store, CAS, `atomic_ref` and changed-result controls. Twelve source cases
passed before the query extension, its three-path smoke passed after, and all
16 selected installed-SDK cases passed across both ARM targets and emulator
backends in 80.40 seconds. Root CTest 8/8 and the independent STREXD CTest
pass. The final visible SDK has 2,564 digest-valid files; its prior 2,535-file
tree is preserved at `~/dev/symbian-sdk-before-native-atomic64-20261001` and
also verifies. A fresh generated-project initial-build, SDK copy and
relocation test passed against the promoted SDK. Wider ordering litmus tests,
other firmware, physical hardware, guest Abseil linkage, C1 request ownership
and A11 fibers remain open.

A subsequent non-808 check broadened the verified native-atomic profile to
C7-00/RM-675 and E6-00/RM-609 under EKA2L1. Their five 64-bit EUSER entry
points have byte-identical prefixes to the RM-807 ROM, including LDREXD/
STREXD add and CAS loops. Cross-thread `std::atomic<uint64_t>` then passed on
Dyncom and Dynarmic for both devices (four cases, 31.91 seconds). The initial
run isolated an unrelated emulator gap: their v10 ROM wrappers use SVC 0x32
for `RThread::ExitReason`; the sixteenth ordered patch maps it to the existing
handler. The older 6120c/RM-243 and E71/RM-346 ROMs expose only 2,228 EUSER
exports, below this profile's five EABI ordinals. The native profile must not
be selected for them. Physical phones, additional dumps and broad atomic
memory-order tests are still unverified.

The next runtime slice links original libc++ `hash.cpp` and genuine LLVM
compiler-rt soft-float, aligned-copy and multiply helpers. A bounded
`std::unordered_set<int>` growth/erase/cleanup probe and changed-result
control passed eight ARMv5T/ARMv6 × Dyncom/Dynarmic cases from source and
eight against a sealed installed-SDK candidate. The companion compiler-rt
normal/negative matrix also passed eight source and eight installed cases.
The candidate has 2,591 digest-valid files. Hash-table growth uses the ROM's
`ceilf` from `libm.dll`; this is not guest Abseil `flat_hash_map` yet.
An E71 generated starter initially failed because the CMake runtime target
added an unused `libm` needed proxy. Its CMake link now scopes LLD
`--as-needed` to that proxy. The E71 starter then built and executed on its
named firmware, and an installed hash-table smoke test still imports and
executes `ceilf`. ROMs without `libm.dll` still cannot execute that hash-table
path until the SDK supplies a math implementation.

The corrected candidate's full installed hash-table matrix passed all eight
cases in 44.76 seconds. A final re-export from the prepared checkout had
2,591 digest-valid files, and its initial-build/copy/relocation test passed.
It was promoted through the SDK installer's path rebasing to
`~/dev/symbian-sdk`; the previous 2,564-file SDK is retained at
`~/dev/symbian-sdk-before-hash-table-20261001`. Both trees verify all sealed
file digests. Root macOS CTest passed eight targets. The public PyPI payload
closure and broader guest Abseil/A11 execution remain open.
