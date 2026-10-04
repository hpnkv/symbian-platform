# Investigate the guest C++ runtime

For root-project IDE navigation in every platform-targeted probe,
enable the `clion-guest-probes-armv6` CMake profile (or the ARMv5T variant)
and reload CMake. Its `symbian_probe_index` aggregate builds one object target
per probe project, supplying actual ARM compiler commands, guest libc++/platform
headers and per-source flags to the editor. The local presets also index the
prepared Mbed TLS DLL probe; portable presets omit it when its source is absent.
The host Debug profile remains active for native SDK tooling. Building the
index checks compilation; use the probe integration tests for ELF/E32
publication and emulator execution.

The maintained [runtime probe](../tutorials/runtime-probe.md) now exercises real libc++
strings and vectors on Symbian's heap. Follow [guest runtime guide](../capabilities/runtime.md)
for the pinned LLVM sources, CMake configuration, proxy preparation, allocation
failure contract and both-backend execution tests. The SDK placement-new conflict
is isolated in an adapter translation unit. Local `.got` tables now receive E32
text fixups: direct global `std::nothrow`, a constant read and a Thumb callback
execute on both CPU backends. The changed-value control returns -113; malformed
or unclaimed GOT entries remain rejected. Generated starters use nothrow model
creation with native cleanup and -4 on failure. Run the maintained tests with:

```sh
SYMBIAN_RUNTIME_WORKSPACE="$PWD" \
SYMBIAN_RUNTIME_COMPILER=/opt/homebrew/opt/llvm/bin/clang++ \
SYMBIAN_RUNTIME_LINKER=/opt/homebrew/bin/ld.lld \
  uv run pytest -q symbian/tests/test_guest_runtime.py
SYMBIAN_APP_SDK=~/dev/symbian-sdk/sdk.json \
  uv run pytest -q symbian/tests/test_project_init.py
```

The visible SDK contains a deliberate export of these changes; editing the
repository alone does not update copied SDK tools/templates. The SDK from this
earlier checkpoint is retained at `~/dev/symbian-sdk-before-local-got-20261001`.
Existing owner apps
keep their sources, UIDs and saved IDE settings. Full hosted C++20, TLS,
local-static guards and general standard-library services remain future gates.

The current visible SDK additionally supplies a bounded 32-bit atomic bridge
through original EUSER operations. To run its normal and changed-result guest
controls against the installed proxy, set `SYMBIAN_RUNTIME_WORKSPACE` to this
checkout, `SYMBIAN_APP_SDK` to `~/dev/symbian-sdk/sdk.json`, and run
`pytest -q symbian/tests/test_guest_runtime.py -k atomic` with the ARM compiler
and linker variables shown above. Both ARM targets and emulator CPU backends
passed. A later threaded libc++ checkpoint adds bounded shared ownership and
`std::thread`; A11 tasks/fibers are still gated.
The newer parent/worker probe also passes eight normal/changed-result cases
across ARMv5T/ARMv6 and Dyncom/Dynarmic. Run it with the same environment and
`-k thread-atomic`. It uses a separate worker heap and waits for its exit
before releasing shared state. `performance guide` records costs to
measure; this correctness result is not a benchmark.

The latest runtime probe also covers `std::unique_ptr`, `std::shared_ptr`,
`std::weak_ptr` and `std::thread` using original libc++ and the selected ROM's
`libpthread.dll`. Use `-k 'ownership or std-thread'` with the same environment
to run normal/changed-result controls for both ARM targets and CPU backends.
The installed `Symbian::Threads` target links the verified runtime and pinned
libpthread/C++ ABI import proxies. A device without those imports needs a
different capability profile; the generated starter does not require threads.
The clock/thread closure adds original libc++ `steady_clock` with a Symbian
nanokernel-tick adapter and selected `libc.dll` `sched_yield` for
`std::this_thread::yield()`. With the same runtime-test environment, use
`-k 'clock-thread or std-thread or thread-error'` to run the concurrent clock,
existing thread and deliberate invalid-join controls. Both ARM targets and
emulator backends passed from source and the installed SDK. The invalid join
exits with -6 under the default no-exceptions policy. The E71 generated
starter still runs because its unused libc proxy is omitted from E32 imports.
The newest C-service probe uses the SDK's Clang-compatible OpenC varargs
header and the firmware's `libc.dll` `vsnprintf`; original LLVM libc++ error
categories provide `std::error_code` messages through `strerror_r`. With the
same environment, run `-k 'varargs or system-error'` for normal and
changed-result controls on both ARM profiles and emulator CPU backends. These
controls do not establish guest Abseil Status support.
The imported-function-pointer probe uses a global `memmove` pointer and a
separate translation unit to force an actual writable-data relocation. With
the same environment, run `-k 'import-pointer or imported_function_pointer'`
for both ARM profiles and emulator backends plus malformed ELF controls.
For bounded classic-C locale and `std::ostringstream` support, link
`Symbian::Streams` instead of `Symbian::Runtime` in a generated project's
CMake target. The separate archive and `__config_site` must be selected
together. With the same runtime-test inputs, run `-k locale-stream` for normal
and changed-result guest controls on ARMv5T/ARMv6 and both CPU backends; the
installed-SDK test uses `SYMBIAN_APP_SDK=~/dev/symbian-sdk/sdk.json`. This is
not an Abseil Status acceptance test, and file streams, arbitrary locales and
wide-character support are still open.
Use `-k 'long-thunk or generated_long_thunk'` to exercise LLD's generated ARM
interworking thunk across both ARM profiles/backends and reject an invalid
literal target before launch.
Application logic now lives in typed `model.h`/`model.cc`; its platform C ABI
bridge is SDK-owned `app_bridge.cc`. Guest exceptions remain off by default;
an opt-in profile needs ARM unwind tables, the E32 exception descriptor and
throw/catch execution tests. The visible SDK was refreshed to the tested
2,496-file payload, retaining the previous tree at
`~/dev/symbian-sdk-before-threads-20261001`.

The isolated exception metadata probe is built by setting
`SYMBIAN_RUNTIME_EXCEPTIONS=ON` and
`SYMBIAN_RUNTIME_EXCEPTION_METADATA_ONLY=ON` in the runtime-probe project.
Its maintained Pytest modes `exception-metadata` and
`changed-exception-metadata` run on both ARM targets and emulator backends.
They check descriptor-bearing E32 publication and a normal C++ cleanup path.
The `test_guest_typed_throw_requires_imported_typeinfo` negative control
records the current imported-data gate. These internal probe switches are not
an application SDK exception profile.
The visible SDK's converter/inspector was then refreshed from a verified
2,496-file export; its prior tree is preserved at
`~/dev/symbian-sdk-before-exception-metadata-20261001`.
The last parser check also rejects a renamed ARM exception index without its
descriptor; the immediately preceding SDK tree is retained at
`~/dev/symbian-sdk-before-exidx-validation-20261001`.


## Writable storage and A11 source checkpoint (2026-10-01)

New generated applications and the runtime probe use an independent RW mapping
for initialized data and BSS. Application code does not manually rebase globals:
the converter and loader apply typed code/data fixups. The original counter's
user-owned application source is preserved; its frozen-import contract remains
a separate example. `guest runtime guide` records the 44 ARM-profile/backend
execution cases and independent original validator controls. Run them with:

```sh
SYMBIAN_RUNTIME_WORKSPACE="$PWD" \
SYMBIAN_RUNTIME_COMPILER=/opt/homebrew/opt/llvm/bin/clang++ \
SYMBIAN_RUNTIME_LINKER=/opt/homebrew/bin/ld.lld \
SYMBIAN_EKA2L1_ORACLES_BUILD="$PWD/build/eka2l1" \
  .venv/bin/pytest -q symbian/tests/test_guest_runtime.py
cmake --build --preset debug --target symbian_a11_source_check
```

The second command validates the genuine staged A11 sources; it does not build
a concurrency backend. Future/Task/fiber/cancellation application examples will
be introduced after their guest execution and request-lifetime gates pass.
Bounded EXE global constructors/destructors and C++ DLL process-attach
constructors now execute on both ARM profiles and emulator backends. Run the
DLL attach and changed-constructor controls against a current installed SDK:

```sh
SYMBIAN_RUNTIME_WORKSPACE="$PWD" \
SYMBIAN_APP_SDK="$HOME/dev/symbian-sdk/sdk.json" \
  .venv/bin/pytest -q symbian/tests/test_guest_dll_lifecycle.py
```

The Belle profile's 0x10D library-entry-start and 0x10E load-preparation
mappings are maintained in the twelfth and thirteenth ordered emulator patches.
The SDK entry uses an explicit code relocation
for its ARM-to-Thumb target; an unrelocated linker thunk caused a real guest
KERN-EXEC fault in the diagnostic run. The dynamic-load probe imports the real
`RLibrary::Load`, `Lookup` and `Close` ordinals. It checks the DLL constructor,
hands it a pointer to client-owned memory and verifies that the destructor wrote
to that memory before `Close` returned; an absent DLL must report a load error.
TLS and general DLL lifetime remain gates. A bounded writable DLL with
per-process reset runs on
Dyncom and Dynarmic. Installed-SDK CMake builds C/C++ DLLs and selected ordinal
proxies for ARMv5T and ARMv6; the linked ELF retains debugger symbols.
See [PROJECTS.md](projects.md) for the helper and limits. Existing user
applications are not regenerated by an SDK refresh.

The optional Mbed TLS guest slice uses the prepared external source without
editing it. It builds the real C archive and SDK DLL, then dynamically loads
that DLL and checks SHA-256 of `abc` plus a changed-input failure on Dynarmic
and Dyncom:

```sh
SYMBIAN_MBEDTLS_SOURCE="$HOME/dev/mbedtls-symbian" \
SYMBIAN_APP_SDK="$HOME/dev/symbian-sdk/sdk.json" \
SYMBIAN_RUNTIME_WORKSPACE="$PWD" \
  .venv/bin/pytest -q symbian/tests/test_mbedtls_library.py
```

This verifies one function through the named emulator fixture. The broader
Mbed TLS modules and any device network service remain separate gates.

For the bounded A11-derived stackless profile, link `Symbian::Stackless` and
include `<symbian/concurrency/future.h>`,
`<symbian/concurrency/parallel.h>` and
`<symbian/concurrency/inline_pump.h>` and
`<symbian/concurrency/task_group.h>`.
The SDK-owned `Mutex` is in `symbian::concurrency` as well; A11's `a11::`
and `thread::` APIs are reserved for their compatible guest port.
An application callback may call
`Promise<T>::SetValue` or `SetError`; `Then` continues inline and `JoinAll`
publishes every input result in input order; `DriveInline` bounds recursive
reentry while preserving a deferred pass. `TaskGroup::Finish` publishes a
Task only after every child has settled. `Cancel` requests producer
cancellation; it does not free pending OS buffers. The profile has no blocking
`Await`, native request broker or fibers. Its maintained executable contract
can be reproduced with:

```sh
SYMBIAN_RUNTIME_WORKSPACE="$PWD" \
SYMBIAN_APP_SDK="$HOME/dev/symbian-sdk/sdk.json" \
SYMBIAN_RUNTIME_COMPILER=/opt/homebrew/opt/llvm/bin/clang++ \
SYMBIAN_RUNTIME_LINKER=/opt/homebrew/bin/ld.lld \
  .venv/bin/pytest -q symbian/tests/test_guest_runtime.py -k a11-stackless
```

The installed `<symbian/concurrency/native_timer.h>` exposes the first owned
native timer request. `NativeTimer` rejects negative/overlapping arms and
cancels and drains a pending request on close. Keep it on the creating OS
thread; a single event-loop owner must still dispatch Window Server events and
timer completions. `<symbian/concurrency/timer_pump.h>` now translates that
request into an A11-derived `Task`. With `Symbian::Stackless`, use
`ScheduleAfter(absl::Duration)` or `ScheduleAt(absl::Time)`, call
`DispatchReady` after native
wakeups, and call `Park` only after checking Window Server statuses.
`Task::Cancel` may be called from a worker; the event thread performs native
cancellation. New `symbian init` projects enable the combined loop by default.
It links `Symbian::Stackless`; the selected ROM must provide `libpthread`.
Tap logs immediately and schedules a delayed Task; Clear cancels pending
Tasks. The starter's bounded `EventMailbox` puts delayed UI work through an
explicit event-thread turn; it is not the A11 shared-pool `Post`. RM-807 GUI
execution passed on Dynarmic and Dyncom. `symbian init --firmware e71` selects
the portable profile when that ROM lacks `libpthread.dll`; use
`--portable-runtime` to choose it without selecting firmware. The original
`examples/gui_app` counter now also links the installed stackless profile:
an increment schedules a 300-ms Future that lights a small marker, Reset
cancels pending work, and the Window Server and timer share one wait. This is a bounded shared-loop
example, not a general native I/O or fiber backend. The current SDK uses
`absl::Duration` for relative timers and real-world `absl::Time` for absolute
deadlines, with monotonic waiting after registration.
The pump now caps pending timer requests at 64 by default. Its constructor
accepts a different limit, and saturation returns an already-ready Task with
`kResourceExhausted`. Check Task results even when scheduling returns at once.

The runtime probe also joins and cancels real timer Tasks through the
A11-derived `TaskGroup`. Its installed-SDK controls pass both ARM profiles and
emulator CPU backends; it does not create a timer worker thread. The same
runtime control exercises `PropertyWatch`, a typed Future over native
`RProperty::Subscribe`, including reentrant subscription and worker
cancellation through the one event-thread semaphore consumer.

The separate `fiber-context` runtime-probe mode now checks the first ARM/Thumb
context-switch primitive from an installed runtime archive. It retains C++
locals on a bounded 16 KiB stack and runs a changed-result control on both ARM
profiles and emulator backends. It does not enable `thread::` fibers or change
the GUI's stackless event loop.

The current SDK also carries LLVM's original ARM soft-double arithmetic and
conversion helpers. An ordered LLVM patch replaces ARMv6T2-only constant and
bit-clear instructions for ARMv5T/ARMv6. The compiler-rt probe runs actual
float/double operations and changed-result controls on both architectures and
CPU backends. This was a runtime prerequisite for guest Abseil. The later
pinned Status/StatusOr and `flat_hash_map` closure now executes from source
and from the installed `Symbian::AbseilStatusOr` target on both ARM profiles
and both CPU backends. Build `examples/abseil_status_probe` with its
`symbian-sdk` preset and `SYMBIAN_ABSEIL_USE_SDK=1` to inspect a standalone
project's compile commands; `symbian-pic` replays the clean pinned source
instead. The installed target supplies original Abseil headers and
per-architecture archives. Selected Abseil time and bounded cross-thread
page-owner release also execute; general TLS, allocation pressure and the
full A11 scheduler remain follow-on work.

Use a currently exported SDK manifest at `SYMBIAN_APP_SDK` on another machine.

