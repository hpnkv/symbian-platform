# Native EKA2L1 research build

The pinned emulator is a separate research dependency, outside the Python wheel.
Its GPL-3.0-or-later license and dependency licenses remain in the checkout.
The EKA2L1 parser, CPU and process integration harnesses in cpp/tests/eka2l1 are
GPL-3.0-or-later. Nokia's original EPL-1.0 checksum source and whole-image
validator are compiled unchanged in separate oracle executables.
checksum_host_types.h supplies a four-byte UID and its count;
validator_host_types.h supplies fixed-width declarations and constants needed
by f32image.h, with asserted header sizes/offsets. These are host adapters, not
an SDK or target runtime. Neither upstream implementation is linked into the
platform's native library or Python extension.

Verified host: Apple Silicon macOS, Apple Clang 21, CMake/Ninja, Qt 6.11.2,
LLD 23.1.2 and host GTest 1.18.0. This is a local research build, not a supported
redistributable macOS bundle. The upstream deployment target is 11.0 while
FFmpeg is built for 12.0 and host GTest for 26.0; the linker reports this mismatch.
Older-host compatibility and distribution signing need a separate build profile.

Acquire the exact sources, keeping their licenses and all submodule pins:

```sh
brew install cmake ninja googletest lld qtbase qttools qtsvg
git clone --no-checkout https://github.com/EKA2L1/EKA2L1 research/upstream/EKA2L1
git -C research/upstream/EKA2L1 checkout 2594edf4d6bf55d7bd3f0b46250fe2318d4dc2e8
git -C research/upstream/EKA2L1 submodule update --init --recursive --depth 1
git clone --no-checkout https://github.com/SymbianSource/oss.FCL.sf.os.buildtools research/upstream/buildtools
git -C research/upstream/buildtools checkout 7b35cd328d3a5e8e0bc177d0169fd409c3273193
git clone --no-checkout https://github.com/SymbianSource/oss.FCL.sf.os.kernelhwsrv research/upstream/kernelhwsrv
git -C research/upstream/kernelhwsrv checkout 0c3208650587ac0230aed8a74e9bddb5288023eb
git -C research/upstream/EKA2L1 apply ../../../research/eka2l1/instance-root.patch
git -C research/upstream/EKA2L1 apply ../../../research/eka2l1/runtime-probe.patch
git -C research/upstream/EKA2L1 apply ../../../research/eka2l1/guest-debug-step.patch
```

If the checkout already exists, verify its revision and patch before building;
do not re-clone it or reapply an applied patch. The FFmpeg submodule revision is
`c2022fa4637301736ff7230e7cee5a7e8cd7d45a`. Its bundled macOS libraries are
Intel-only. Build native libraries with its maintained script:

```sh
(cd research/upstream/EKA2L1/src/external/ffmpeg && sh macos_arm64-build.sh)
```

From the platform repository root:

```sh
uv sync
uv run symbian build --project examples/e32_probe --output .symbian/e32-probe
cmake -S research/upstream/EKA2L1 -B build/eka2l1 -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DCMAKE_PREFIX_PATH=/opt/homebrew -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DEKA2L1_BUILD_TESTS=ON -DEKA2L1_BUILD_TOOLS=OFF \
  -DEKA2L1_ENABLE_QT_CAMERA=OFF -DEKA2L1_SCRIPTING_LUA=OFF \
  -DCMAKE_PROJECT_EKA2L1_INCLUDE="$PWD/research/eka2l1/project-tests.cmake"
cmake --build build/eka2l1 -j 8 \
  --target eka2l1_qt ekatests symbian_e32_oracle symbian_checksum_oracle \
    symbian_validator_oracle symbian_cpu_probe symbian_process_probe \
    symbian_sis_checksum_oracle symbian_package_probe
uv run symbian package --project examples/e32_probe \
  --artifact .symbian/e32-probe/e32_probe.exe --output .symbian/package
ctest --test-dir build/eka2l1 -R 'symbian_|^ekatests$' --output-on-failure
uv run symbian toolchain verify-probe .symbian/e32-probe/e32_probe.exe
SYMBIAN_EKA2L1_EXECUTABLE="$PWD/build/eka2l1/bin/EKA2L1.app/Contents/MacOS/EKA2L1" \
  SYMBIAN_EKA2L1_ORACLES_BUILD="$PWD/build/eka2l1" uv run pytest -q
```

The injection adds separate GTest oracles without vendoring the emulator or
altering its CMake files. SYMBIAN_E32_TEST_IMAGE can select another fixture, but
the current tests expect the maintained e32_probe identity/profile.
Upstream Qt translation generation writes .ts files in its source checkout;
these generated changes and all build products remain ignored.

instance-root.patch preserves upstream defaults when no override is supplied.
On macOS, EKA2L1_DATA_ROOT selects an absolute storage/config/resource root and
redirects default Qt settings beneath it. An invalid directory fails before
emulator writes. Standalone --help/-h prints options and returns zero without
starting a device worker. Failed CLI parsing wakes the initialization waiter
before joining the worker. The GDB listener is changed to IPv4 loopback; debugger
attachment and guest runtime behavior are not yet tested.

Use an explicit disposable directory even for smoke testing:

```sh
EKA2L1_DATA_ROOT="$PWD/.symbian/instances/smoke" \
  build/eka2l1/bin/EKA2L1.app/Contents/MacOS/EKA2L1 --help
```

The Cocoa Qt plugin is bundled; the offscreen plugin is not. These are native
desktop frontend tests, not a demonstrated headless guest runner. Smoke tests
prove independent settings/assets roots, root rejection and bounded CLI failure
shutdown. They do not prove isolation for every runtime subsystem, simultaneous
guest instances, stopped golden-state restoration, or full-machine snapshots.

The E32 oracle parses the complete converter output and checks its metadata;
it rejects damaged UID checksums and truncated code. It deliberately records
that this EKA2L1 revision accepts an incorrect header CRC. The separate historical
checksum oracle independently validates the converter's UID checksum and CRC.
The historical validator calls Nokia's unchanged ValidateWholeImage with a
bounded uncompressed V fixture and rejects CRC damage, negative heap size,
insufficient entry space, missing imports, wrong ARM ABI and truncated code.
It uses public pre-Belle source; acceptance is not a Belle loader verdict.

The synchronous CPU harness parses the same E32 using EKA2L1, maps read-only
code and a private stack, and executes up to 512 steps on each of dyncom and
dynarmic at 0x8000 and 0x20000. It checks ARM-to-Thumb interworking, restored
stack and exit SVC registers. Changing the volatile input from 16 to 17 produces
exit reason 42 instead of zero. Its callback only observes SVC 0x73; it never
dispatches to a Symbian kernel. There is no ROM, process creation, import or
service test. These backends are not claimed to be independent implementations
of every instruction; Dynarmic has fallback paths.

The process harness uses EKA2L1's actual virtual filesystem, executable loader,
flexible memory model, scheduler and epoc10 kernel SVC table. Each case has a
private temporary host directory mounted as a write-protected virtual C drive.
No ROM or Z drive is mounted, and no target DLLs/services are supplied. Eight cases
cover normal exit, a changed-input failure exit, another launch after exit and
missing-file rejection on both backends. The real ThreadKill handler completes
process/thread exit and releases the address space. Negative cases flush the
TLB before stepping so the stack-write callback can change input 16 to 17.
It never replaces the SVC
handler. JSON results include the actual code mapping and exit reason.

The harness reuses upstream's test-only platform.cpp UI hooks for linking; no
UI code is exercised. It uses the upstream timer lifecycle, stopping its worker
before kernel teardown. It does not introduce a platform scheduler. Probe source
compiles with exceptions disabled. runtime-probe.patch initializes ROM mapping
state and the VFS ID counter, and makes invalid UID indexing fail fast instead
of throwing from a header. The ELF/E32 loading and kernel algorithms are unchanged.

verify-probe runs all five oracle binaries with bounded execution and a private
input copy, preserving 28 completed GTest cases, logs, artifact and binary hashes.
Filtered, disabled, skipped or failing tests cannot produce a passing report.
It is specific to examples/e32_probe. It reports eka2l1_process_verified and
kernel_exit_verified true, while matched Belle loader/runtime verification
remains false. The epoc10 enum selects emulator behavior; it does not identify
an installed Belle/FP2 image. These tests do not cover DLL imports, User::Exit,
system services, full ABI validation, desktop boot or a package installation.
No firmware or ROM/Z image was imported.

The SIS checksum oracle remains a separate EPL binary using the same historical
CRC implementation. The GPL package harness reuses the process environment,
but mounts its private C filesystem writable and starts without an executable.
The unchanged EKA2L1 installer creates the payload and registry. Six cases cover
install/launch, registry reload/uninstall and uninstall/reinstall/launch on both
backends. Installed bytes must match the supplied E32; registry fields and an
independently recorded SHA-1 digest are checked. No ROM or target services are
present, and signing policy is not enforced by this emulator installer.
SYMBIAN_SIS_TEST_PACKAGE defaults to .symbian/package/probe.sis. Only pass the
maintained fixture after native inspection; these upstream parsers are not a
bounded public API for untrusted packages. See docs/PACKAGING.md for
`toolchain verify-package`, which retains 35 complete cases and input/binary
hashes while keeping Belle and phone flags false.

The eager import experiment adds `symbian_import_probe` with six development
DLL execution cases using the native frozen-export DLL. The GPL process harness
loads that trusted fixture without implementing an SDK system library. Build
and replay instructions, fixture scope and hashes are in
[docs/IMPORTS.md](../../docs/IMPORTS.md). The existing import-free verifier's
case count and scope stay unchanged.

The maintained import harness now requires the native DLL, including count word
and export-pointer relocations. Build it with:

```sh
uv run symbian build --project examples/dll_probe --output .symbian/native-dll
cmake --build build/eka2l1 --target symbian_import_probe
ctest --test-dir build/eka2l1 -R '^symbian_import_probe$' --output-on-failure
```

The previous fixed-address original-header DLL producer remains research material.
It does not satisfy the stronger mapped-export checks. See docs/IMPORTS.md and
docs/RESEARCH_LOG.md for the source-derived contract and evidence correction.
Checksum and validator adapters now retain complete bounded headers through the
maximum full export bitmap; upstream checksum/validator algorithms are unchanged.


The pointer experiment adds `symbian_pointer_probe` with six process cases.
They independently decode the four text relocations, inspect mapped pointer
values, and observe the ARM callback, Thumb callback and virtual method on both
CPU backends. Replay and optional SIS install/launch checks are in
[docs/POINTERS.md](../../docs/POINTERS.md). The package harness accepts the
verifier's independently computed `SYMBIAN_E32_TEST_HASH` reference file; the
original probe retains its recorded hash when that explicit input is absent.

The real RM-807 startup/debugger experiment is separate from these ROMless
tests. The GPL `guest-debug-step.patch` fixes a single-step flag that otherwise
keeps executing after a stop. `symbian/tests/test_guest_debugger.py` drives
the actual patched frontend and ARM GDB against digest-checked ROM/EUSER and
ELF/E32 inputs in a disposable copy. It expects the current heap startup
failure and checks source/ROM breakpoints and stable instruction stepping.
See [WALKTHROUGH.md](../../WALKTHROUGH.md#9-debug-guest-startup-and-the-gui)
for replay, concrete executive ABI discrepancies and remaining limitations.


The guarded RM-807 experiment additionally uses the GPL
`guest-debug-library-query.patch`, `symbian101-experimental.patch` and
`guest-thread-register.patch`. The native ROM/routing/register probes and live
Pytest cover actual preserved ROM exports, explicit profile rejection, separate
ARM register contexts and initial drawing-function execution. They are research
components excluded from the wheel. Patch order, exact digest guards, source
contracts and unresolved ABI/runtime limits are in
[docs/BELLE_ABI.md](../../docs/BELLE_ABI.md). The example now installs an SDK
cleanup stack; rendered pixels, pointer input and normal exit remain unverified.


The seventh GPL patch, `guest-control.patch`, links the separate native adapter
in `cpp/symbian/emulator` into the Qt research frontend. An explicit private Unix
socket enables actual screen-texture PNG capture, logical Window Server input
and kernel exit records. The adapter uses existing event loops and detaches
before kernel teardown. Four native path/startup tests and two real-backend GUI
render/input/normal-exit tests pass. It is never linked into the Python wheel.
See [the replay and bounds](../../docs/EMULATOR_CONTROL.md) and
[CLion configuration](../../docs/CLION.md).

## Change organization

The eighth ordered patch, `firmware-import-bounds.patch`, bounds RPKG names,
entries and reads, rejects traversal/truncation, and validates product/ROM dump
names before host writes. The ninth, `fbs-unsupported-request.patch`, completes
unknown synchronous FBS requests with KErrNotSupported; RM-243 otherwise blocks
during teardown on opcode 0x2C. Neither patch implements a missing font operation.
Both replay/reverse exactly against the pinned source, after the existing seven.
The tenth ordered patch, `background-window.patch`, lets SDK-owned macOS
sessions show the Qt window without activation and makes the OpenGL context
order it behind the current app. On macOS the OpenGL window uses managed
desktop-Space behavior and excludes full-screen auxiliary display and tiling.
Initial activation is suppressed without blocking later intentional focus.
Actual full-screen-Space placement awaits a nondisruptive visual check. The SDK
launcher also executes a private
per-session symlink outside the `.app` bundle; that prevents macOS bundle
activation before Qt creates a window while retaining a directly owned PID.
The patch has independent pinned-base replay and applied-state reverse checks.
The eleventh ordered patch, `dll-wsd-dyncom-exit.patch`, stops Dyncom fetching
another instruction after a guest SVC kills/unmaps the current process. The
writable-DLL fresh-process oracle exposed the stale fetch at the prior code
mapping's end; Dynarmic already stopped. It does not relax DLL validation or
hide a guest fault. The pinned-base apply and applied-state reverse checks pass.
The twelfth ordered patch, `belle-library-entry-start.patch`, maps the observed
RM-807 Belle 0x10D slot to the existing v10 library-entry-start hook. Original
EUSER's static-call-list contract and a live three-entry call list supplied the
mapping evidence. Eight DLL
constructor/changed-constructor execution controls pass across both ARM
profiles and emulator backends. The applied-state reverse check passes.
The thirteenth ordered patch, `belle-library-load-prepare.patch`, maps the
observed Belle 0x10E slot to EKA2L1's existing v10 load-preparation hook. The
ROM `RLibrary::Load` wrapper calls it before the loader-server request. The
subsequent actual load, lookup, attach, close and detach paths are checked by
the client-owned destructor sink and a missing-DLL error control. This mapping
does not implement new state in the existing preparation hook; the actual load
and failure are handled later by the loader path. It does not establish every
loader mode or DLL lifetime contract. The patch
replays after the first twelve and reverse-checks against the applied checkout.
The fourteenth ordered patch, `belle-thread-exit-reason.patch`, maps the
observed Belle ROM `RThread::ExitReason` call at SVC 0x34 to the existing
`thread_exit_reason` handler. The original v10 table omits the older slot;
the thread probe first failed with an unimplemented 0x34 after the worker had
exited normally. Eight parent/worker atomic guest cases now pass across both
ARM profiles and CPU backends, including changed-result controls. The patch
replays after the first thirteen and reverse-checks against the applied tree.
The fifteenth ordered patch, `dyncom-strexd-value.patch`, corrects Dyncom's
STREXD handler: it assembled the 64-bit register pair but passed only the
low-register number to the exclusive write. A direct original EUSER 64-bit
atomic probe failed on Dyncom and passed on Dynarmic before this correction;
both backends now pass. `symbian_cpu_strexd` checks an independent ARM
LDREXD/STREXD high-word update and a deliberately changed value on each
backend without firmware. The patch applies to the clean pinned source and
reverse-checks against the built checkout.
The sixteenth ordered patch, `v10-thread-exit-reason.patch`, adds the observed
v10 `RThread::ExitReason` SVC 0x32 to EKA2L1's existing handler. The E6 and
C7 ROM wrappers call 0x32 after their worker has exited normally; before the
patch the emulator returned an unimplemented-SVC result. Belle's 0x34 mapping
then follows automatically from the already verified two-slot v101 shift.
Four C7/E6 × Dyncom/Dynarmic cross-thread native-atomic controls pass after
this mapping. The patch reverse-checks against the applied source.
The seventeenth ordered patch, `ntick-fast-counter-hal.patch`, implements
the existing kernel HAL IDs for nanokernel tick period and fast-counter
frequency. EKA2L1 already generates both counters; before this patch it
could not report their rates through the ROM's HAL interface. The eighteenth,
`fast-counter-rate.patch`, corrects the emulated fast counter to produce
exactly `HIGH_RES_TIMER_HZ` counts per second instead of dividing by a
truncated integer microsecond period. A one-second guest interval comparison
checks the counter against the measured nanokernel clock. Both patches
reverse-check against the applied source; the rate patch also applies to
the pinned source index without the other local edits.
`symbian_firmware_tool` and `symbian_firmware_tests` build through the same CMake
hook, link original archive/ROM/RPKG/VPL implementations and stay outside the
Python wheel. Five GTests cover archive bounds/mappings and actual malformed
RPKG handling. Upstream diagnostics go to retained stderr, separate from JSON.
See [firmware UX and evidence](../../docs/FIRMWARE.md).

Upstream emulator changes are maintained as patch files in this directory,
against the pinned revision above. Apply the full ordered set in WALKTHROUGH.md.
The project-tests.cmake injection adds independent tests and the maintained GPL
control adapter from cpp/symbian/emulator; its Qt-specific calls stay at the
boundary and internal data uses standard C++ and nlohmann::json. Keep checkouts
and build/runtime state ignored. LLVM integration is separately documented in
../llvm/README.md; its two maintained libc++ patches are explicit there.
