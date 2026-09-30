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
    symbian_validator_oracle symbian_cpu_probe symbian_process_probe
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
