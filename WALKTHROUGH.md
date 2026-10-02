# Building and investigating the GUI example

For generated applications, visible SDK installation, relative project settings
and IDE Run/Debug integration, see [Standalone projects](docs/PROJECTS.md).
For current ROM/Z import, configuration precedence, device selection and offline
transfer, see [Firmware onboarding](docs/FIRMWARE.md). The named RM-807 paths
below remain evidence for the original counter experiment; current Run/Debug
resolve a shared firmware selection rather than requiring those paths.

`examples/gui_app` is a small native Window Server counter application. It
draws four seven-segment digits and three touch controls: increment, reset and
exit. The intended initial display is `0000`.

The source compiles on macOS with contemporary Clang, links with LLD, and
converts to a reproducible Symbian E32 executable. Original Nokia checksum
and whole-image validation pass. Its ARM ELF retains valid DWARF and LLDB
resolves its functions and source lines. The supplied Delight v1.8 firmware
imports as RM-807/808 PureView/epoc100. A guarded firmware-specific executive
profile and CPU corrections enable SDK startup. Both macOS CPU backends now
produce actual GUI screen captures, respond to increment/reset/outside taps and
complete normal zero guest exit and zero frontend exit. This does not establish
full Belle compatibility, OS boot or physical-phone behavior. Live guest
debugging is tested; see sections 8–9, [docs/BELLE_ABI.md](docs/BELLE_ABI.md) and
[docs/EMULATOR_CONTROL.md](docs/EMULATOR_CONTROL.md).

| Result | Current evidence |
| --- | --- |
| Public source headers and frozen function ordinals | 92 hashed header aliases; 10 EUSER and 28 WS32 imports |
| C++20 ARM compilation and E32 conversion | Two independent CMake/Ninja builds produce identical ELF/E32 bytes |
| Counter, layout, pointer boundaries, division | Five host GTests |
| SDK preparation, packaging and integration policy | 18 Pytest cases with optional public source/oracle inputs |
| Historical image checks | Eight passing original-source checksum/validator cases |
| GUI SIS package | 17 image/checksum/installer cases; registry reload/removal/reinstall verified |
| Supplied RM-807 firmware | VPL/ZIP checks and native emulator import pass; real DLLs mapped during launch |
| Debug information and editor input | DWARF verification, LLDB symbol/source lookup, real compilation database |
| Rendered GUI, pointer delivery and SDK exit | Real texture PNGs, count/reset/outside taps, guest reason zero and frontend exit zero on both backends |
| Guest debugger connection and breakpoints | Live ARM GDB source/ROM stops and stable single stepping verified |
| Nokia 808 / Belle compatibility | Unverified; actual phone identity/firmware remains unknown |

## 1. What was made and why

The example uses the public `w32std.h` interfaces directly. A raw Window Server
client is sufficient to create a window, draw rectangles, and receive pointer
and redraw events. This exposes the GUI service contract without first needing
Avkon, application registration resources, fonts, Qt, or a resource compiler.
Direct emulator launches use its executable path. Packaging now adds a
separate application-registration resource and caption resource so an
installed package can appear in the application menu; this does not introduce
an Avkon application lifecycle into the example.

The project is divided as follows:

| File | Responsibility |
| --- | --- |
| `model.h` | Pure counter, geometry, pointer hit testing and integer division |
| `app.cc` | Original SDK types, Window Server connection, drawing and request loop |
| `startup.S` | ARM entry marker and ARM-to-Thumb entry transition |
| `startup.cc` | Checked thread-create layout, SDK heap setup, process initialization and `User::Exit` |
| `image.ld` | Retained-relocation ELF transport with one code mapping and eager import tables |
| `CMakeLists.txt`, `CMakePresets.json` | Target compilation, SDK definitions, debug flags and source path maps |
| `symbian.toml` | E32 import profile, development UID `0xe0000811`, two explicit ordinal proxies |

SDK source preparation uses `research/gui_app/source-profile.json`; its input
digests are build data and are outside the application project.

The model is ordinary C++ with no SDK or host library dependency. It caps the
counter at 9999, treats hit regions as half-open rectangles, and ignores input
after exit. The host tests compile this same header, rather than implementing a
second model in Python. The model's division routine is tested against host
integer `/` and `%` at boundary values and 8,000 mixed inputs. It requires a
nonzero divisor; every application caller supplies a positive constant.
Initially normal division introduced unresolved ARM compiler-runtime helpers.
This bounded integer routine removes that dependency until compiler-rt is
ported. It is an example utility, not a general replacement for compiler-rt.

`GuiMain` explicitly constructs and closes an `RWsSession`, `CWsScreenDevice`,
`CWindowGc`, `RWindowGroup`, and `RWindow`. Group/client handles are 1 and 2.
The initial screen size determines the layout; supported dimensions are
120..8192 by 160..8192. Rotation and resizing after launch are not handled yet.
Digits and control marks are filled rectangles, so no font selection is needed.
Green has a plus; amber has a horizontal reset stroke; red has a boxed exit
mark. The painting implementation deliberately keeps its inputs on the stack.

The loop posts `EventReady` and `RedrawReady`, waits through
`User::WaitForRequest`, processes completed requests, and reposts them. Pointer
button-down events for window handle 2 change the model and invalidate the
window. Redraw requests for that handle bracket drawing with `BeginRedraw` and
`EndRedraw`. On exit it cancels and completes pending requests before their
stack statuses disappear, then closes GUI objects before the session.
This is Symbian's service protocol; no host scheduler or Python callback is
introduced. The surrounding Python staging/build policy uses existing native
format bindings, whose GIL and status handling follow A11. Host native libraries
retain their no-exception/Abseil status policy. At the target OS boundary the
example uses Symbian's required `TInt` result convention. Abseil and A11's host
thread library have not been ported into the guest.

The startup is a deliberately limited primary-thread adapter. It consumes the
entry reason and thread-create pointer, checks the source-derived struct sizes,
calls `UserHeap::SetupThreadHeap`, then `User::InitProcess`, creates an SDK
`CTrapCleanup`, calls `GuiMain`, deletes the cleanup object, and ends through
`User::Exit`. Failure to allocate the cleanup object returns `KErrNoMemory`.
Unexpected thread/exception entry calls
`User::Invariant`. It provides no secondary-thread or global constructor
support. Unlike the earlier resource-free integer probes, it attempts real SDK
initialization and cleanup. That attempt is still a **runtime experiment**:
successful linking cannot prove that the installed Belle EUSER ABI agrees.
The reference is the original
[ARM executable startup](https://github.com/SymbianSource/oss.FCL.sf.os.kernelhwsrv/blob/0c3208650587ac0230aed8a74e9bddb5288023eb/kernel/eka/euser/epoc/arm/uc_exe.cpp)
and its accompanying `uc_exe.cia`, together with the original
[Window Server header](https://github.com/SymbianSource/oss.FCL.sf.os.graphics/blob/ff133bc50e6158bfb08cc093b0f0055321dcde99/windowing/windowserver/inc/W32STD.H).

## 2. Enable the macOS toolchain

All commands in this document start in this repository's root unless indicated.
Use Apple Silicon macOS with Apple's command-line development tools installed.
Check the selected developer directory and compiler:

```sh
xcode-select -p
xcrun --find clang++
xcrun clang++ --version
brew install uv cmake ninja lld llvm googletest openssl@3
uv sync
uv run symbian doctor
```

Install Apple's command-line tools separately if `xcrun` cannot locate Clang.
`uv sync` builds the host native extension and installs the project CLI and
development tools. `doctor` reports tool availability and target readiness; a
working host toolchain does not make the Belle runtime ready.

The tested GUI build used Apple Clang 21 and LLD 23.1.2. Use the default
`clang++`/`ld.lld` discovered on PATH, or select them explicitly:

```sh
uv run symbian build --project examples/gui_app \
  --output .symbian/gui-app \
  --compiler "$(xcrun --find clang++)" \
  --linker "$(brew --prefix lld)/bin/ld.lld"
```

Run that build only after preparing the source headers in the next section and
activating an installed SDK. The migrated GUI links the installed SDK's
`Symbian::Stackless` target and selected import proxies; the earlier staged
EUSER/WS32 proxies remain provenance controls for the original narrow GUI.
Homebrew can install newer versions than this checkpoint; build reports record
actual paths and versions. Reproducibility currently means independent build
directories on the same host and toolchain, not identical output from every
compiler release.

Current new projects use the SDK's `symbian-arm.cmake` toolchain with
`SYMBIAN_TARGET_ARCH=armv6`. `symbian init --architecture armv5t` selects the
older target when needed. Existing ARMv5T projects retain their choice.
Effective C++ flags include `--target=armv6-none-eabi`, `-mthumb`,
`-mfloat-abi=soft`, `-mabi=aapcs`, `-ffreestanding`, `-std=c++20`,
`-fPIC`, `-fno-exceptions`, `-fno-rtti`, and `-nostdinc`.
The GUI adds `-g -gdwarf-4 -O1`; its final `-O1` overrides the generic `-O2`.
Target code cannot accidentally include macOS C++ headers. SDK includes are
supplied explicitly. `_UNICODE`, the GCC/EABI compatibility definitions,
`__EPOC32__`, and ARM platform definitions select the upstream headers' intended
declarations. The linker retains relocations and rejects unresolved imports.

C++20 language support is real, but a complete target standard library is not
available. This GUI uses neither a hosted libc++ nor exceptions. See
[docs/CXX20.md](docs/CXX20.md) for the separately tested language, modules and
selected header-only library experiments and their remaining runtime work.

## 3. Acquire the pinned public source profile

The checked-in manifest contains paths, hashes, revisions, and export names.
It does not contain an SDK distribution or upstream header contents.
For a fresh workspace acquire these exact public repositories, retaining their
licenses. These commands create ignored research checkouts:

```sh
mkdir -p research/upstream
git clone --filter=blob:none --no-checkout \
  https://github.com/SymbianSource/oss.FCL.sf.os.kernelhwsrv \
  research/upstream/kernelhwsrv
git -C research/upstream/kernelhwsrv checkout \
  0c3208650587ac0230aed8a74e9bddb5288023eb
git clone --filter=blob:none --no-checkout \
  https://github.com/SymbianSource/oss.FCL.sf.os.graphics \
  research/upstream/graphics
git -C research/upstream/graphics checkout \
  ff133bc50e6158bfb08cc093b0f0055321dcde99
git clone --filter=blob:none --no-checkout \
  https://github.com/SymbianSource/oss.FCL.sf.os.ossrv \
  research/upstream/ossrv
git -C research/upstream/ossrv checkout \
  1e9520caca186c601dd9768449b86bc72be39a22
git clone --filter=blob:none --no-checkout \
  https://github.com/SymbianSource/oss.FCL.sf.os.persistentdata \
  research/upstream/persistentdata
git -C research/upstream/persistentdata checkout \
  ef8baa21cee9cd1e214e1a7986595c60b3a63271
git clone --filter=blob:none --no-checkout \
  https://github.com/SymbianSource/oss.FCL.sf.os.textandloc \
  research/upstream/textandloc
git -C research/upstream/textandloc checkout \
  59666d6704fee305b0fdd74974f7b4f42659c6a6
```

If a checkout already exists, use `git -C <directory> rev-parse HEAD` and
`git -C <directory> status --short` to inspect it; skip the corresponding clone.
Do not reset local research changes to follow this recipe. A sparse checkout
must contain every file named by the research source profile, including differently cased
`INC`/`inc` directories and private header dependencies. A full checkout is
the straightforward starting point.

Prepare the source profile:

```sh
uv run symbian toolchain prepare-gui-sdk \
  --profile research/gui_app/source-profile.json \
  --sources-root research/upstream \
  --output .symbian/gui-sdk
```

Preparation verifies each actual input's SHA-256, creates explicit header aliases
as symlinks into the original checkouts, validates the original narrow import
selection in the native core, and builds two reproducible ordinal proxies. The
migrated GUI uses the installed SDK's broader proxies at link time. It refuses
path escapes, different occupied headers, and redirected output directories.
The 93 aliases flatten SDK includes and preserve necessary `graphics/...`
namespaces without modifying upstream files. Reported repository revisions are
manifest declarations; preparation verifies file digests, not Git provenance.
Keep original licenses beside the source trees and retain those trees for as
long as the aliases are used. Symlinks are not a preserved independent copy.

The EUSER definition is `kernel/eka/eabi/euseru.def`; WS32 is
`windowing/windowserver/eabi/WS322U.DEF`. The native parser selects the 38 frozen
function ordinals without renumbering. The generated `euser.dso` and `ws32.dso`
are **link-time ordinal proxies**, not executable implementations of those
libraries. Copying them into a guest cannot supply EUSER or Window Server.
The public source profile is pre-Belle evidence, not a verified Nokia 808 SDK.
The future matched target must supply compatible real system DLLs and services.

The preparation report is `.symbian/gui-sdk/sdk-report.json`; each proxy also
retains its own source, build trees, input digests and `report.json`.

## 4. Build, inspect, and iterate

```sh
uv run symbian build --project examples/gui_app --output .symbian/gui-app
uv run symbian inspect .symbian/gui-app/gui_app.elf --format elf32
uv run symbian inspect .symbian/gui-app/gui_app.exe --format e32
```

The CLI loads `symbian.toml`, resolves the selected SDK's proxy paths and
passes the target toolchain to the project's preset. The project builds with Ninja, converts with the native
E32 writer, and repeats the build in a separate directory. It compares the
ELF and E32 bytes and records actual dependencies. No Python implementation
of ELF/E32/DEF parsing is used.

| Output | Use |
| --- | --- |
| `.symbian/gui-app/gui_app.exe` | Guest E32 executable |
| `.symbian/gui-app/gui_app.elf` | Original ARM ELF with symbols and DWARF |
| `.symbian/gui-app/report.json` | Hashes, inputs, tool versions, build logs and native metadata |
| `.symbian/gui-app/compile_commands.json` | Real CMake compilation database for editors |
| `.symbian/gui-app/cmake/` | Persistent CMake/Ninja target tree |

Edit `app.cc` or `model.h` and rerun `symbian build`. The persistent tree provides
the incremental build; the second build rechecks reproducibility. You can also
use `cmake --build .symbian/gui-app/cmake` for quick compiler feedback, but that
alone does not reconvert or update the published `.exe` and report.

Changing the SDK path requires both a `SYMBIAN_GUI_SDK_INCLUDE` CMake cache
override and updated `import_proxies` in the project manifest. An ignored
`CMakeUserPresets.json` can inherit `symbian-pic`; set `cmake_preset` to its
name. The integration fixture in `symbian/tests/test_gui.py` demonstrates this
with paths containing spaces. Do not copy an arbitrary SDK onto the source
profile and assume the frozen ordinals remain valid.

The native package writer accepts imported executables and, when the manifest
has `[application]`, bundles genuine compiled registration/caption resources
on the executable's install drive. It still rejects DLL payloads. The project
declares package UID
`0xe0000812`, independently of executable UID `0xe0000811`:

```sh
uv run symbian package --project examples/gui_app \
  --artifact .symbian/gui-app/gui_app.exe --output .symbian/gui-package
uv run symbian inspect .symbian/gui-package/gui_app.sis --format sis
uv run symbian toolchain verify-gui-package .symbian/gui-package/gui_app.sis \
  --executable .symbian/gui-app/gui_app.exe \
  --oracles-build build/eka2l1 --output .symbian/gui-package-check
```

Edit menu text in `examples/gui_app/symbian.toml`:

```toml
[application]
caption = "Symbian GUI Counter"
short_caption = "Counter"
icon = "assets/icon.svg"

[application.localizations.de]
caption = "Symbian Zähler"
short_caption = "Zähler"
```

The package contains the unchanged EXE, fallback and translated menu
resources, and an SDK-compiled SVG-in-MIF icon on the same install drive;
target system libraries must already exist. Native inspection checks every
embedded hash, resource UID and install path. The original EKA2L1 AppArc
parser selects the French, German and Japanese resources, decodes `Zähler`
and `カウンター` as Unicode,
and its MIF reader extracts the SVG icon. Its installer accepted the seven
files on Dyncom and Dynarmic, reloaded
their registry entry, removed the resources on uninstall and reinstalled them
(eight headless tests). The full verifier passed 17 original image/checksum
and installer cases through the visible SDK. These cases execute no guest
instructions. The current GUI source has six DLL imports and 153 import
slots. Physical Belle icon rendering remains to be observed. The emulator's
process creation without system DLLs leaves those slots
unresolved, which is not an application
launch. Certificates and physical-phone installation remain separate gates.

## 5. Run checks that do not require a ROM

Build and run the host GTests and Python policy tests:

```sh
cmake --preset debug
cmake --build --preset debug -j 8
ctest --preset debug
SYMBIAN_GUI_SOURCE_ROOT="$PWD/research/upstream" uv run pytest \
  symbian/tests/test_gui.py -q
```

The source environment variable enables the real SDK/link fixture. Without it,
the two source-dependent cases skip; they are not silently counted as passing.
To enable historical validation also build the research oracles and supply their
path:

```sh
SYMBIAN_GUI_SOURCE_ROOT="$PWD/research/upstream" \
  SYMBIAN_EKA2L1_ORACLES_BUILD="$PWD/build/eka2l1" \
  uv run pytest symbian/tests/test_gui.py -q
uv run symbian toolchain verify-gui .symbian/gui-app/gui_app.exe \
  --oracles-build build/eka2l1 --output .symbian/gui-validation
```

The research build recipe below supplies the oracle executables.
`verify-gui` runs one checksum case and seven unchanged historical validator
cases against a private exact copy of this generated image. It retains binary
hashes, test JSON, logs and `report.json`. It does not execute GUI instructions,
resolve target system DLLs, or boot an OS. `verify-probe` and `verify-pointers`
are specific to other maintained examples; they cannot substitute for a GUI
execution test. All GUI runtime, import, loader and debugger flags stay false.

For formatting and the full Python suite:

```sh
uv run black --check symbian scripts
uv run ruff check symbian scripts
"$(brew --prefix llvm)/bin/clang-format" --dry-run --Werror \
  examples/gui_app/app.cc examples/gui_app/startup.cc \
  examples/gui_app/model.h cpp/tests/gui_model_test.cc
uv run pytest -q
```

The full suite also has optional emulator/header/module inputs described in
[research/eka2l1/README.md](research/eka2l1/README.md) and
[docs/CXX20.md](docs/CXX20.md). Missing optional inputs must remain visible as
skips. None of these tests issues a hardware operation.

## 6. Prepare the emulator research build

Use pinned EKA2L1 commit `2594edf4d6bf55d7bd3f0b46250fe2318d4dc2e8` and
the ordered local patches. `instance-root.patch` supplies isolated macOS data/settings roots,
bounded CLI failure shutdown, and a loopback-only GDB listener.
`runtime-probe.patch` supplies the documented research initialization fixes.
`guest-debug-step.patch` makes a remote single step stop after one instruction
and send its stop response. Without it, stepping continues silently through
the guest until another breakpoint, so register observations can be misleading.
`guest-debug-library-query.patch` removes an unsupported GDB capability.
`symbian101-experimental.patch` supplies explicitly selected, ROM-digest-guarded
executive routing; `guest-thread-register.patch` adds separate TPIDRURO state
and tested macOS backend context preservation. `guest-control.patch` links the
GPL native adapter for screen capture, logical pointer input and final exit
records; it is disabled unless an explicit private socket is supplied. These
are already applied in
the current workspace. For a fresh checkout:

```sh
brew install qtbase qttools qtsvg
git clone --no-checkout https://github.com/EKA2L1/EKA2L1 \
  research/upstream/EKA2L1
git -C research/upstream/EKA2L1 checkout \
  2594edf4d6bf55d7bd3f0b46250fe2318d4dc2e8
git -C research/upstream/EKA2L1 submodule update --init --recursive --depth 1
git -C research/upstream/EKA2L1 apply ../../../research/eka2l1/instance-root.patch
git -C research/upstream/EKA2L1 apply ../../../research/eka2l1/runtime-probe.patch
git -C research/upstream/EKA2L1 apply ../../../research/eka2l1/guest-debug-step.patch
git -C research/upstream/EKA2L1 apply ../../../research/eka2l1/guest-debug-library-query.patch
git -C research/upstream/EKA2L1 apply ../../../research/eka2l1/symbian101-experimental.patch
git -C research/upstream/EKA2L1 apply ../../../research/eka2l1/guest-thread-register.patch
git -C research/upstream/EKA2L1 apply ../../../research/eka2l1/guest-control.patch
git -C research/upstream/EKA2L1 apply ../../../research/eka2l1/firmware-import-bounds.patch
git -C research/upstream/EKA2L1 apply ../../../research/eka2l1/fbs-unsupported-request.patch
git -C research/upstream/EKA2L1 apply ../../../research/eka2l1/background-window.patch
git -C research/upstream/EKA2L1 apply ../../../research/eka2l1/dll-wsd-dyncom-exit.patch
git -C research/upstream/EKA2L1 apply ../../../research/eka2l1/belle-library-entry-start.patch
git -C research/upstream/EKA2L1 apply ../../../research/eka2l1/belle-library-load-prepare.patch
git -C research/upstream/EKA2L1 apply ../../../research/eka2l1/belle-thread-exit-reason.patch
git -C research/upstream/EKA2L1 apply ../../../research/eka2l1/dyncom-strexd-value.patch
git -C research/upstream/EKA2L1 apply ../../../research/eka2l1/v10-thread-exit-reason.patch
git -C research/upstream/EKA2L1 apply ../../../research/eka2l1/ntick-fast-counter-hal.patch
git -C research/upstream/EKA2L1 apply ../../../research/eka2l1/fast-counter-rate.patch
git clone --no-checkout \
  https://github.com/SymbianSource/oss.FCL.sf.os.buildtools \
  research/upstream/buildtools
git -C research/upstream/buildtools checkout \
  7b35cd328d3a5e8e0bc177d0169fd409c3273193
(cd research/upstream/EKA2L1/src/external/ffmpeg && sh macos_arm64-build.sh)
```

Skip acquisitions and patches already present; inspect before changing existing
research trees. The ARM64 FFmpeg step is needed because the pinned submodule's
bundled macOS libraries are Intel-only. Preserve EKA2L1's GPL and other licenses.
The injected hook fetches the same pinned Abseil revision as the host platform.
An existing matching checkout can be selected with `SYMBIAN_ABSEIL_SOURCE_DIR`
(or its CMake cache variable). The hook explicitly selects C++20 before
configuring Abseil because it runs before upstream's standard selection.
Configure the Apple Silicon build and separate historical oracle executables:

```sh
cmake -S research/upstream/EKA2L1 -B build/eka2l1 -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DCMAKE_PREFIX_PATH="$(brew --prefix)" -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DEKA2L1_BUILD_TESTS=ON -DEKA2L1_BUILD_TOOLS=OFF \
  -DEKA2L1_ENABLE_QT_CAMERA=OFF -DEKA2L1_SCRIPTING_LUA=OFF \
  -DCMAKE_PROJECT_EKA2L1_INCLUDE="$PWD/research/eka2l1/project-tests.cmake"
cmake --build build/eka2l1 -j 8 --target \
  eka2l1_qt symbian_checksum_oracle symbian_validator_oracle \
  symbian_sis_checksum_oracle symbian_gui_package_probe symbian_control_probe
```

`kernelhwsrv` is the same pinned tree already used for headers. The historical
checksum and validator remain separate oracle binaries; their algorithms are
not linked into the platform core. This is a local development app bundle,
not a signed redistributable application. The verified Qt was 6.11.2; the
research README records deployment-target/library warnings and the broader
process/installer test targets.

Select a disposable instance root, even for help:

```sh
export SYMBIAN_EMULATOR="$PWD/build/eka2l1/bin/EKA2L1.app/Contents/MacOS/EKA2L1"
export SYMBIAN_GUI_INSTANCE="$PWD/.symbian/instances/gui-development"
EKA2L1_DATA_ROOT="$SYMBIAN_GUI_INSTANCE" "$SYMBIAN_EMULATOR" --help
```

The two task-specific variables must be absolute paths. Help works without a
ROM. The patched frontend's working directory and `config.yml`, resources,
logs and default Qt settings are beneath the instance root. Default
`data-storage: data` then places guest storage in `$SYMBIAN_GUI_INSTANCE/data`.
If you change `data-storage`, subsequent drive paths must use that actual
setting. The Cocoa Qt plugin is bundled; a headless offscreen GUI runner has
not been established.

## 7. Boot a real test device and launch the GUI

The basic asset import and direct-launch attempt have now been executed with
the supplied firmware; the visual/input acceptance tests below remain open.
EKA2L1's
[installation instructions](https://github.com/EKA2L1/EKA2L1/wiki/Using-the-emulator)
require a ROM and a repackaged Z drive from the same device. Public headers and
ordinal proxies cannot replace those assets. Use legitimate, independently
preserved material and record its digests and actual device identity. A generic
epoc10 test profile does not identify a Belle firmware. RM-807 is still a
hypothesis for the physical phone in this project. The supplied ZIP declares
RM-807; that establishes the archive's identity, not the phone's current state.

### Supplied Delight v1.8 archive checkpoint

The provided Downloads ZIP has SHA-256
`88403c3a8ef5ed14a48712a70fd81020b2b33ae17d5487a595004f09b8c10d98`.
It contains seven files: core/ROFS2/ROFS3/UDA FPSX images, a VPL, a DCP and a
signature file. Its VPL declares product `059M7Q4`, version `113.010.1508`, and
a French/Euro variant. All required files are present; ZIP CRCs and all supplied
VPL CRC entries agree. Some optional files, including the eMMC image, are absent.
CRC agreement and a signature file do not establish authenticity. The filename
identifies custom Delight material; it is not an established factory recovery
baseline or a verified copy of this physical phone.

A working extraction is in `.symbian/assets/delight-v1.8/RM-807`. The separate
native research importer calls EKA2L1's actual VPL/FPSX/ROM/ROFS/FAT pipeline.
It refuses an existing output root and never calls a physical transport:

```sh
cmake --build build/eka2l1 --target symbian_firmware_import_probe -j 8
SYMBIAN_FIRMWARE_VPL="$PWD/.symbian/assets/delight-v1.8/RM-807/RM807_059M7Q4_113.010.1508_019.vpl" \
  SYMBIAN_EMULATOR_IMPORT_ROOT="$PWD/.symbian/instances/delight-import-new" \
  build/eka2l1/platform-tests/symbian_firmware_import_probe \
  --gtest_output=json:"$PWD/.symbian/delight-import-new.json"
```

The retained successful root is `.symbian/instances/delight-import-01`, with
13,438 inventoried files, a 31 MiB `data/roms/rm-807/SYM.ROM`, actual EUSER/WS32
in `data/drives/z/rm-807/sys/bin`, and isolated writable drives. Its
`data/devices.yml` identifies Nokia/808 PureView/RM-807/epoc100; machine UID was
initially recorded as zero and must not be inferred from this import alone.
Copy the complete stopped imported root before launch, wait for copying to
finish, and use the new copy as `SYMBIAN_GUI_INSTANCE`. The direct-launch attempt
in `delight-gui-01` mapped the GUI at `0x70000000` and both real system DLLs;
it logged unimplemented SVCs `0x51`/`0xF7` and a `$HEAP` lookup failure.
That is runtime diagnostic evidence, not proof of the counter rendering or
working. Guest symbols, heap startup and emulator service coverage need the
next debugging experiment. Original archive/extracted/imported digests and
logs stay in ignored `.symbian/gui-research`.

Launch the patched frontend:

```sh
EKA2L1_DATA_ROOT="$SYMBIAN_GUI_INSTANCE" "$SYMBIAN_EMULATOR"
```

That direct `.app` executable launch follows macOS's normal foreground-app
behavior. SDK `Run`/`Debug` and guarded GUI tests instead launch a private
symlink outside the bundle and set the maintained background-window profile.
They keep the current terminal or IDE active. The OpenGL display is a normal
managed desktop-Space window that cannot join or tile in another app's
full-screen Space; clicking it later may intentionally focus it. The policy
is built and a real desktop GUI run stayed behind the active app. Placement
while another app is already full-screen still needs a visual check.

Use **File/Install device** to select the preserved ROM and corresponding Z
package. Choose separate storage for this device if offered. Verify that the
installed device boots and the display accepts normal input before attempting
the new executable. Record firmware code, Symbian version and screen dimensions.
Select that device in the frontend. The CLI's `--device` option expects the
emulator's recorded firmware code; do not invent one from the phone model.

Quit the emulator before changing its virtual filesystem. With default storage,
the host C-drive mapping is one of:

| Device storage setting | Host C directory |
| --- | --- |
| Shared drives | `$SYMBIAN_GUI_INSTANCE/data/drives/c` |
| Separate drives | `$SYMBIAN_GUI_INSTANCE/data/drives/<lowercase-firmware-code>/c` |

These paths come from the pinned `device_drive_folder` implementation. Confirm
the selected device and mapping; do not create both paths and guess which is
mounted. For a shared-drive instance, copy only the generated executable:

```sh
export SYMBIAN_GUI_C_DRIVE="$SYMBIAN_GUI_INSTANCE/data/drives/c"
mkdir -p "$SYMBIAN_GUI_C_DRIVE/sys/bin"
cp .symbian/gui-app/gui_app.exe "$SYMBIAN_GUI_C_DRIVE/sys/bin/gui_app.exe"
EKA2L1_DATA_ROOT="$SYMBIAN_GUI_INSTANCE" "$SYMBIAN_EMULATOR" \
  --run 'C:\sys\bin\gui_app.exe'
```

For separate drives, set `SYMBIAN_GUI_C_DRIVE` to the confirmed per-device C
directory first. All paths in this step refer to disposable emulator storage.
The equivalent command `--app` is an alias of `--run` in this pinned frontend.
The absolute virtual EXE path is necessary because this example has no
application registration. Keep `gui_app.elf` on the host for debugging.

Alternatively, after confirming the selected disposable device and drive,
install `.symbian/gui-package/gui_app.sis` through the frontend's package
installer. The pinned CLI `--install` uses drive E, so a CLI installation must
subsequently launch `E:\sys\bin\gui_app.exe`, not the C-drive path above.
Ordinary installation still does not create an application-menu entry without
registration resources. The native research package tests explicitly select C.

Test and record the following in `docs/RESEARCH_LOG.md` with actual results:

1. On launch, the screen shows `0000` and all three controls, with no panic.
2. Left-control taps show `0001`, `0002`, and so on. The middle control resets to `0000`.
3. Taps in gaps and outside controls leave the count unchanged; a single
   button-down increments once. Saturation at 9999 is already model-tested;
   test it visually if automated pointer injection becomes available.
4. The right control exits. Launch again and verify the count starts at zero; record whether
   session/window resources and address space are released normally.
5. Obscure and restore the window and verify redraw. Launch in each intended
   fixed orientation; rotating while running is not supported yet.
6. With the emulator stopped, test separate copies with `cpu: dynarmic` and
   `cpu: dyncom` in `config.yml`, GDB disabled. Record differences and failures.

Preserve the matching build/validation reports, input asset digests, selected
device, actual code-load address, screenshot and instance `EKA2L1.log`.
`EKA2L1_TakeThis.log` holds a previous log when the frontend rotates it.
If imports or startup fail, preserve the failure: it identifies a real missing
runtime/ABI contract. Do not replace system libraries with link proxies.

For a baseline, quit the emulator and copy its complete stopped instance into
a separate golden directory; make a fresh copy for each experiment. This is
filesystem restoration, not a full-machine snapshot, and the snapshot workflow
has not been validated for every subsystem. Preserve firmware originals in an
independently held offline copy with recorded digests; read-only permissions
alone do not make the owner unable to change them. No physical-device recovery,
flashing, calibration, partition or bootloader operation is part of this recipe.

## 8. Enable editor navigation and verify debug information

For CLion, the repository root now includes `gui_app` with its actual ARM
compilation command alongside host tooling. Reload CMake, choose `gui_app` for
editing/building and `gui_app_run` for Run. The latter is a native launcher
executable: Run starts the owned Python/EKA2L1 supervisor. Building that target
only compiles the launcher. `gui_app_e32` independently publishes the matching
ELF/E32 pair; the Run supervisor also publishes before launch.

The standalone `examples/gui_app` project and `symbian-pic`/local `clion-arm`
profiles remain usable. Root **GUI Debug** requires the separate **Symbian GUI
GDB** debugger profile; restore the host profile for native tests. Toolchain,
SDK provenance and remote-debug details are in [docs/CLION.md](docs/CLION.md).

For clangd, the root `build/debug/compile_commands.json` now contains both host
and guest commands. The published standalone database at `.symbian/gui-app`
also contains the correct ARM macros/SDK includes. Use the database matching the
project being developed.

Check parsing/navigation inputs without booting an emulator:

```sh
"$(brew --prefix llvm)/bin/clangd" \
  --check=examples/gui_app/app.cc \
  --compile-commands-dir=.symbian/gui-app \
  --tweaks=ExpandAutoType
"$(brew --prefix llvm)/bin/llvm-dwarfdump" --verify \
  .symbian/gui-app/gui_app.elf
"$(brew --prefix llvm)/bin/llvm-dwarfdump" --debug-line \
  .symbian/gui-app/gui_app.elf
```

The pinned clangd parses and indexes this source with zero errors using that
limited tweak selection. An unrestricted `--check` also attempts every
refactoring at every token and reports two `ExtractFunction` failures at
loop-control statements; those are refactoring-check failures, not C++
diagnostics. Editor parsing does not require enabling that experimental check.

Debug prefix maps make the two independently built ELFs identical:

| Recorded source prefix | Local replacement |
| --- | --- |
| `/symbian-src/gui_app` | Absolute `examples/gui_app` directory |
| `/symbian-sdk/include` | Absolute `.symbian/gui-sdk/include` directory |
| `/symbian-build/gui_app` | Absolute `.symbian/gui-app/cmake` directory |

DWARF 4 keeps compatibility with older ARM debuggers. Its language tag can read
as C++14 even though actual compiler commands use C++20. The retained `.elf`
contains symbols and line tables; the converted `.exe` is the runtime format,
not the debugger symbol file. Keep each ELF beside the exact executable and
report it produced. `-O1` can optimize locals away and inline model methods;
`DrawGui` is explicitly noinline to give a useful drawing breakpoint.

Offline LLDB inspection has been tested:

```sh
lldb .symbian/gui-app/gui_app.elf
```

In LLDB, replace the `/absolute/repo` prefixes with this checkout's actual path:

```text
settings set target.source-map /symbian-src/gui_app /absolute/repo/examples/gui_app /symbian-sdk/include /absolute/repo/.symbian/gui-sdk/include
image lookup -n GuiMain
image lookup -r -n DrawGui
source list -n GuiMain
quit
```

LLDB identifies the object as ARM and shows its source. This does not attach to
the guest. Attaching LLDB to the macOS EKA2L1 process instead debugs the ARM64
host emulator, which is useful for emulator failures but a different target.

## 9. Debug guest startup and the GUI

**Guest attachment, startup source breakpoints and instruction stepping have
been verified against the supplied RM-807 image.** Heap initialization fails
before drawing; visual GUI behavior and a normal SDK exit remain unverified.
Debugging startup is useful even while those runtime contracts are incomplete.
The pinned emulator's
[GDB documentation](https://github.com/EKA2L1/EKA2L1/blob/2594edf4d6bf55d7bd3f0b46250fe2318d4dc2e8/src/emu/scripting/lua/eka2l1/topics/DebuggingWithGDB.md)
describes whole-guest debugging, Dynarmic and software breakpoints. Its stub is
not a per-process Symbian debug agent. Start with that supported documented CPU
path and a bootable device; no stub connection is available from ROMless help.

Homebrew has an
[ARM GDB formula](https://formulae.brew.sh/formula/arm-none-eabi-gdb).
Installation is a debugger prerequisite, not evidence of stub compatibility:

```sh
brew install arm-none-eabi-gdb
arm-none-eabi-gdb --version
```

First launch once with GDB disabled. Search the instance log for this image:

```sh
rg -i 'gui_app|runtime code|ordinal|panic' "$SYMBIAN_GUI_INSTANCE/EKA2L1.log"
```

The pinned kernel reports `gui_app ... runtime code: 0x...` on attachment.
Record the **actual** code address. The ELF/E32 link code base is `0x8000`.
The symbol slide is `actual_runtime_code_base - 0x8000`; do not assume a
particular guest address or reuse it after a different launch without checking.

Quit the emulator. Edit the existing instance `config.yml`, preserving its
device/storage settings and changing these keys:

```yaml
cpu: dynarmic
enable-gdb-stub: true
gdb-port: 24689
```

Useful optional trace keys in this pinned source are `log-svc`, `log-ipc`,
`log-read`, `log-write`, and `log-exports`. Start with the specific trace needed;
full service logging can be noisy and slow. Logs and settings are instance-local
only with the applied root patch. Restart using the same `--run` command.
The applied patch binds the GDB port to IPv4 loopback. Confirm its startup log;
an occupied port or an unbootable selected device must be fixed first.

Start `arm-none-eabi-gdb` in the repository root. In the following commands
replace `SLIDE` with the calculated hexadecimal slide and `/absolute/repo`
with the actual checkout path **before entering them**:

```text
set pagination off
set architecture arm
set remotetimeout 200
file /absolute/repo/.symbian/gui-app/gui_app.elf
symbol-file -o SLIDE /absolute/repo/.symbian/gui-app/gui_app.elf
set substitute-path /symbian-src/gui_app /absolute/repo/examples/gui_app
set substitute-path /symbian-sdk/include /absolute/repo/.symbian/gui-sdk/include
target remote 127.0.0.1:24689
break GuiRunThread
continue
```

[GDB's `symbol-file -o` contract](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Files.html)
adds the offset to section addresses. Verify the loaded symbols against the
new launch's mapping. A preliminary launch helps discover the address but does
not guarantee identical mapping next time. If it differs, interrupt and reload
symbols with the corrected slide and recreate breakpoints.

Once stopped in the correct image, use `info registers`, `bt`, `list`,
`step`, `next`, and `x/8i $pc`. Add a source-line breakpoint inside `DrawGui`
using its current line number from `app.cc`; inspect model/layout values where
optimization leaves them available. Startup assembly executes ARM instructions
and C++ executes Thumb. For raw-address breakpoints, upstream documents
`set arm fallback-mode thumb` for Thumb addresses; use ARM for startup instead.
For raw disassembly check CPSR bit `0x20`: use `set arm force-mode arm`
when clear and `set arm force-mode thumb` when set, returning to `auto`
for symbolized source. An incorrect fallback can misdecode ARM as Thumb.
Do not treat the Thumb state bit as a separate byte of code.

With the default executive profile, the launch stops at `GuiRunThread`,
PC `0x700009da`, with reason zero
and thread-create information at `0x40ffc0`. Two `stepi` commands stop at
`0x700009dc` and `0x700009de`. A fresh register read after a delay confirms the
first stop stays halted. Source path substitution displays the actual adapter.
A breakpoint at `startup.cc:19` observes heap result `-1` (`KErrNotFound`),
so `GuiMain` is never called in this experiment.

Live ROM breakpoints identify the failing executive-call boundary:

| Guest instruction address | Belle call | Pinned emulator behavior |
| --- | --- | --- |
| `0x804bf730` | SVC `0x51`, kernel HAL page-size query; arguments `0,7,&size,0` | Unimplemented; its epoc10 table uses `0x4F` for HAL |
| `0x804bf810` | SVC `0x6D`, chunk creation; owner `1`, `$HEAP` descriptor and chunk-create structure | Dispatches object lookup; chunk creation is registered at `0x6B` |
| `0x804bfc40` | SVC `0xF7`, called from the real `User::Exit(-1)` path | Unimplemented; its thread-exiting handler is registered at `0xF6` |

The HAL/chunk identifications combine real guest argument/instruction reads
with the original SDK implementations. The exit identification is consistent
with its caller and the original exit code. These discrepancies establish a
kernel executive ABI problem for this firmware; they do not establish a complete
Belle call table. Do not shift the whole table or substitute SDK implementations
based on these three calls. Subsequent source/export wrapper analysis supports
a piecewise experimental profile and retains default-profile controls. See
[docs/BELLE_ABI.md](docs/BELLE_ABI.md) for its derivation and remaining gaps.
To select it for a disposable launch, prefix the emulator command with
`EKA2L1_EXPERIMENTAL_SVC_PROFILE=rm807-113.010.1508`; it requires the exact
preserved ROM digest. The initial drawing function now completes with SDK
cleanup-stack setup and a separate ARM TPIDRURO register. This does not yet
establish displayed pixels, input delivery, complete DLL initialization or exit.

Replay the bounded live debugging regression with the exact preserved fixture:

```sh
SYMBIAN_GUI_DEBUG_GOLDEN_ROOT="$PWD/.symbian/instances/delight-import-01" \
SYMBIAN_GUI_DEBUG_BUILD="$PWD/.symbian/gui-app" \
SYMBIAN_EKA2L1_EXECUTABLE="$SYMBIAN_EMULATOR" \
uv run pytest -q symbian/tests/test_guest_debugger.py \
  --basetemp .symbian/gui-debug-check-01
```

Use a new disposable `--basetemp` directory: Pytest clears that directory.
The test checks ROM/EUSER and ELF/E32 digests before starting, copies the golden
state, uses an instance-local loopback port, checks real source and ROM stops,
retains logs, and verifies those input digests again. It explicitly expects the
default heap failure. The explicit guarded-profile case reaches and returns from
the initial `DrawGui` function, inspecting the zero counter and 360 by 640
layout. Two further cases check actual rejection of an unknown profile and
a one-byte-modified private ROM. No input means a visible skip. Its cleanup stops only its
own frontend process; the current frontend may require KILL after TERM. Neither
that forced stop nor reaching `User::Exit` proves normal guest cleanup.

The separate autonomous GUI test now verifies rendered redraws, pointer delivery
and normal exit on both backends. Enable the private native control socket and
replay its pixel/exit checks using [EMULATOR_CONTROL.md](docs/EMULATOR_CONTROL.md).
Keep those evidence flags separate from attachment and stepping. Source
symbols in a static LLDB session are not a substitute for the live test.
Guest stack unwinding, crash symbolication,
LLDB remote compatibility and full process inspection remain separate work.

## 10. Troubleshooting and the next platform steps

| Symptom | Check |
| --- | --- |
| Header SHA mismatch or missing file | Exact source pins, local edits, sparse checkout and literal-case paths |
| Duplicate SDK overload declarations | `_UNICODE` and the genuine target compilation command |
| CMake requests GUI SDK or import proxies | Run preparation; check both include cache and manifest proxy paths |
| Unresolved `__aeabi_*` or C++ runtime symbol | Newly added code may require compiler-rt, allocation or a library not yet ported |
| Converter rejects data, TLS or constructors | Current transport deliberately lacks those runtime contracts; inspect the real ELF |
| `.exe` unchanged after an incremental CMake build | Rerun `symbian build` to convert and publish it |
| Empty device list or failure before boot | Matching ROM/Z is missing or the selected instance/storage root is wrong |
| Executable not found | Correct mounted C path, exact virtual path and installed-device selection |
| DLL/ordinal rejection | Compare actual matched EUSER/WS32 exports with manifest proxies; do not fabricate system implementations |
| Panic before drawing | Heap/thread-create/process startup or SDK ABI mismatch; retain log and mapping |
| No redraw or pointer response | Window Server event/client handles, focus, pending request status and target service compatibility |
| No application-menu icon | Check the project-relative SVG path, package file list and AppArc icon path; physical Belle rendering still needs verification |
| SIS packaging fails | Check experimental UIDs, translated captions, SVG XML and bounded file sizes; DLL payloads/scripts/signatures are unsupported |
| Breakpoint never hits | Stub enabled on supported backend, correct current code slide, ARM/Thumb state and exact ELF/executable pair |
| Source files not found in debugger | Apply prefix substitutions rather than removing reproducibility maps |

Captured pixels, pointer-driven redraws, reset and normal SDK exit now have
concrete evidence in the guarded firmware experiment, alongside live debugging.
Next broaden firmware ABI coverage, rotation/focus handling, resource accounting
and application resources/registration.
Imported-app single-EXE SIS installation is already tested separately. A broad platform runtime still needs writable data/BSS/TLS,
static lifetime, compiler-rt/C-library support and a carefully configured C++
library. The evidence trail belongs in [docs/RESEARCH_LOG.md](docs/RESEARCH_LOG.md)
and [docs/STATUS.md](docs/STATUS.md); preserve failures as well as passes.

At this checkpoint the generated E32 SHA-256 is
`2ef4145fa9323837d3d16d8914652d69f0b703a747b4dbb52ab83db83b727792`
and the debug ELF SHA-256 is
`7b2918ba000c8faf039069a3571816a6203edde129a5cb9c2144e475d582d05f`.
Local evidence is under `.symbian/gui-sdk`, `.symbian/gui-app`,
`.symbian/gui-validation`, `.symbian/gui-research`, and
`.symbian/belle-abi-research`. Those directories and
upstream material remain outside version control.

## Installed IDE Run and Debug workflow

The prepared GUI project now has GUI Run and GUI Debug configurations. Open
examples/gui_app in its own IDE window and select clion-arm. GUI Run publishes
the current E32, copies the golden and launches the emulator. GUI Debug uses
CLion Remote Debug plus the separately selected native Symbian GUI GDB profile;
its supervisor publishes/starts a halted copy and automatically relocates
symbols after connection. Set source breakpoints before Resume. The GUI example
must run in the emulator; the CMake ELF is an ARM build product.

[docs/CLION.md](docs/CLION.md#installed-run-and-debug-buttons) gives the installed
settings, regeneration command, retained evidence, Stop behavior and toolchain
caching caveat. Inputs/binaries, logs and fresh instances live under
.symbian/gui-runs; source inputs remain unchanged. This is the bounded GUI
experiment, not a complete emulator instance manager or physical-device launch
path. Live launch and GDB/MI checks pass; IDE toolbar operation and complete
frontend stack unwinding are not claimed from those checks.

## 10. Guest standard-library runtime

For root-project IDE navigation in every platform-targeted probe,
enable the `clion-guest-probes-armv6` CMake profile (or the ARMv5T variant)
and reload CMake. Its `symbian_probe_index` aggregate builds one object target
per probe project, supplying actual ARM compiler commands, guest libc++/platform
headers and per-source flags to the editor. The local presets also index the
prepared Mbed TLS DLL probe; portable presets omit it when its source is absent.
The host Debug profile remains active for native SDK tooling. Building the
index checks compilation; use the probe integration tests for ELF/E32
publication and emulator execution.

The maintained [runtime probe](examples/runtime_probe) now exercises real libc++
strings and vectors on Symbian's heap. Follow [docs/RUNTIME.md](docs/RUNTIME.md)
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
before releasing shared state. `PERFORMANCE_CONSIDERATIONS.md` records costs to
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


### Writable storage and A11 source checkpoint (2026-10-01)

New generated applications and the runtime probe use an independent RW mapping
for initialized data and BSS. Application code does not manually rebase globals:
the converter and loader apply typed code/data fixups. The original counter's
user-owned application source is preserved; its frozen-import contract remains
a separate example. `docs/RUNTIME.md` records the 44 ARM-profile/backend
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
See [PROJECTS.md](docs/PROJECTS.md) for the helper and limits. Existing user
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

# Physical USB device work

For a connected handset, run `symbian device list` and `symbian device info`
to inspect the host-visible USB descriptor and mounted volumes. The Mac
adapter binds a volume to its actual USB ancestor and leaves the serial out of
CLI output. `symbian device install --project PATH --device SELECTOR` rebuilds
and packages through the selected SDK, then stages a checked SIS in the
phone's existing `Installs` directory when mass storage is available. The
result remains `awaiting-on-device-install`: finish other file transfers,
safely eject the phone, and open the SIS on the handset. See
[docs/DEVICE.md](docs/DEVICE.md) for the current transport, signing and Linux
limits. Do not infer RM code or firmware from the USB product name.
After photo copying finished, an ARMv5T portable starter with Unicode menu
resources was staged and the disk safely ejected. The owner reported that it
installed, appeared as `menu_v5` and responded to a tap. That is a physical
user observation; the SDK has not yet collected installer, process or device
logs automatically.
