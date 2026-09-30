# Native EKA2L1 research build

The pinned emulator is a separate research dependency, outside the Python wheel.
Its GPL-3.0-or-later license and dependency licenses remain in the checkout.
The EKA2L1 integration harness in cpp/tests/eka2l1/e32_oracle_test.cc is
GPL-3.0-or-later. Nokia's original EPL-1.0 checksum source is compiled unchanged
in a separate oracle executable; checksum_host_types.h supplies just a four-byte
UID type and the UID count needed by that translation unit. Neither upstream
implementation is linked into the platform's native library or Python extension.

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
git -C research/upstream/EKA2L1 apply ../../../research/eka2l1/instance-root.patch
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
  --target eka2l1_qt ekatests symbian_e32_oracle symbian_checksum_oracle
ctest --test-dir build/eka2l1 -R 'symbian_|^ekatests$' --output-on-failure
SYMBIAN_EKA2L1_EXECUTABLE="$PWD/build/eka2l1/bin/EKA2L1.app/Contents/MacOS/EKA2L1" \
  uv run pytest -q symbian/tests/test_emulator.py
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
These checks still do not replace Symbian's complete loader validation or an
actual executable launch. No firmware or ROM/Z image was imported.
