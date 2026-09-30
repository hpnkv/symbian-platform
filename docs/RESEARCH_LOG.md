# Research log

## 2026-09-30: initial survey and engineering baseline

**Question:** What already exists, and can the host supply a native starting
point without reconstructing Nokia's build environment?

**Historical behavior:** GCCE/RVCT produced ARM ELF input for elf2e32; Symbian
loaded E32 images and resolved DLL contracts using target-specific metadata.
The historical build host did not define the phone's loader format.

**Evidence:** The initial directory contained only PLAN.md. Sources and immutable
revision links are listed in RESEARCH.md. The owner reports one Nokia 808, no
preserved assets and no second phone. Exact RM/product code and firmware remain
unknown. A11's native libraries disable exceptions and use Status/StatusOr;
its pybind11 boundary releases the GIL for blocking/native work and owns Python
references through dedicated holders when callbacks cross threads.

**Experiments:** Built the initial native library and Python extension with
the A11 Abseil revision `5650e9cf76d3be4318d5fa3af38ee483ddfd5e4a`. Built
GTest against the host's static GTest installation. The installed extension's
`otool -L` lists only CoreFoundation, libc++ and libSystem.
The default CMake fetch/build path also succeeded in a fresh tree and fetched
the exact pinned revision without using A11's local source cache. Verification
passed 41 Pytest cases and eight GTest cases, Black/Ruff, clang-format, and the
native typing declaration regeneration check. Compile commands confirm the
native core and native tests use `-fno-exceptions`.

**Conclusion:** The first slice needs only a synchronous native parser and Python
host orchestration, not A11's full runtime or a second concurrency library.

**Implementation decision:** Core format logic lives in cpp/symbian/analysis;
pybind11 is the exception-enabled boundary; errors retain canonical status
codes in Python and JSON. No callbacks, background Python references, native
scheduler, device executor or compatibility VM is introduced.

**Remaining uncertainty:** Target/runtime assets and a validated recovery path
are unavailable. Native object generation does not complete PLAN milestone 1.

## 2026-09-30: Apple Clang ARM code generation

**Question:** Can the installed native compiler produce usable ARM ELF input?

**Historical behavior:** GCCE used the `arm-none-symbianelf` target spelling and
target-specific libraries/startup before conversion to E32.

**Evidence:** Clang documents explicit cross-target selection. f32image.h defines
the downstream E32 loader contract independently of the compiler's target name.

**Experiments:** Apple Clang 21 rejected `--target=arm-none-symbianelf` with an
invalid-version diagnostic. It compiled the SDK-free integer probe with
`--target=armv5t-none-eabi -mthumb -mabi=aapcs -mfloat-abi=soft
-ffreestanding -fno-exceptions -fno-rtti -nostdinc`. The maintained CLI adds
`-O2 -std=c++20`. Native inspection reads ELF32 little-endian, type 1, machine
40, EABI version 5 and ten sections. Two builds with independent output
directories have identical bytes. The optimized example object SHA-256 is
`ce5601f39eca6a481489e83f87f0df62d2d44a51ecc285fc8523ffe7acc67c7b` on this
host/compiler. `clangd --check` using the emitted target compilation database
completed with zero errors. The host has no `ld.lld` in PATH or Xcode tools.

**Conclusion:** Native ARM object generation and target-aware source intelligence
work without GCCE. The old compiler triple cannot be reused verbatim.

**Implementation decision:** `symbian toolchain probe` and the experimental
`symbian build` report object reproducibility explicitly and always label
Symbian loader verification false. Flags are a conservative experimental
profile, not a verified specification for all 808 application code.

**Remaining uncertainty:** ARM ELF linking, ordinal DSOs, E32 conversion,
startup, target compiler builtins, C++ ABI/leave semantics and runtime acceptance.

## 2026-09-30: EKA2L1 automation contracts

**Question:** Which existing emulator capabilities should the facade reuse?

**Historical behavior:** The desktop frontend provides launch/install command
options but retains GUI initialization and host global state conventions.

**Evidence:** Pinned thread.cpp/cmdhandler.cpp, config/options.inl,
qt/src/main.cpp, scripting sources and gdbstub.cpp listed in RESEARCH.md.
The GUI requires Qt Widgets/LinguistTools/Svg/Network/OpenGL. Existing loader
tests share the emulator's broad native build/dependency graph.

**Experiments:** Source inspection only. No emulator executable or ROM/Z image
was run. The macOS main changes cwd to Qt's global generic-data location before
loading config; `--install` selects drive E; `--help` follows a failure-return
path. The GDB listener uses INADDR_ANY. `--listapp` is unfinished in this revision.

**Conclusion:** CLI flags alone do not provide an isolated automated test device.
A launcher's cwd and storage copy would leave shared configuration and assets.

**Implementation decision:** Before runtime orchestration, add/test a native
instance-root override before any writes and configuration loading. Reuse the
GDB stub and kernel hooks, with loopback binding and structured completion
results. Do not publish unverified snapshot/headless/screenshot CLI commands.

**Remaining uncertainty:** Native full build, actual Belle/FP2 runtime support,
Qt/application compatibility, debugger protocol interoperability and process exit
automation need measured tests with matching artifacts.

## 2026-09-30: reference archive semantics

**Question:** What can the host preserve before device access is established?

**Historical behavior:** Firmware inventories and device dumps are independent
of ordinary application builds and need variant/version/provenance records.

**Evidence:** PLAN.md requires preservation and a recovery path, and keeps
irreversible operations outside automated authority. No original artifacts exist
in the provided project.

**Experiments:** Tests use synthetic files only. Archive creation copies bytes,
hashes source and destination, detects input changes, records a deterministic
manifest, rejects links/special files, and removes write permission. Verification
detects missing/extra/modified files and unsafe manifest paths. A separately
retained manifest digest detects manifest substitution. No phone was modified.

**Conclusion:** The tools can preserve existing material but cannot establish a
known-good device/firmware baseline without original inputs. Owner-reversible
POSIX permissions are not immutable storage.

**Implementation decision:** `symbian preserve create/verify` and the private
inventory template support preservation; an offline reference and separately
trusted digest remain required. Policy classification has no execution API.

**Remaining uncertainty:** Actual device identity, original firmware acquisition,
ROM/Z dumps, backup method, and independently validated human recovery appliance.

## 2026-09-30: linked ELF and restricted E32 executable

**Question:** Can modern macOS tools produce an actual E32 container without a
historical compiler, SDK runtime or Windows tooling?

**Historical behavior:** EKA2 ARM startup begins with a version marker and
reserves a loader-owned word at entry+12. E32 V headers include alternating-byte
UID CRC16 and a CRC32 initialized with zero, without a final complement; the
header CRC field contains 0xc90fdaa2 during CRC calculation. These contracts are
visible in f32image.h, uc_exe.cia and the historical elf2e32 checksum source.

**Experiments:** Installed native LLD 23.1.2. Apple Clang 21 compiles ARM startup
and Thumb C++ using ARMv5T/AAPCS soft-float. LLD links ET_EXEC with retained
R_ARM_CALL and R_ARM_THM_CALL relocations. Disassembly confirms ARM-to-Thumb BLX,
a real C++ calculation/call/stack sequence, and SVC 0x73 with the ThreadKill
register contract. A volatile input and noinline probe prevent the calculation
from being optimized into a constant success result.

The converter in cpp/symbian/e32 emits an uncompressed 156-byte V header, UID3
and SID 0xe0000808, no capabilities/imports/exports/data, and 116 bytes of code.
It checks bounded segments/sections/symbols/relocations and the EKA2 entry.
Two independent output trees produce identical linked ELF and E32 bytes:

- ELF SHA-256: `4031252adc395d05bb7f3477262b4c00c0018aee1bc7eba40daa35feaa798e4e`
- E32 SHA-256: `997cd9c5ec35281f261a08cb3c2ca6a36c74be969b4a72cbbd8ede5ff5332395`

The independent EKA2L1 parser accepts the complete E32 and expected metadata,
rejects UID checksum damage and truncated code, and accepts a bad header CRC.
That last case deliberately documents a weaker oracle. A separate GTest binary
compiling Nokia's unchanged checksum.cpp confirms both checksums independently.
The host adapter supplies only TUid's four-byte size and KMaxCheckedUid=3.
The platform inspector rejects header CRC damage. Neither oracle is linked into
the maintained core or Python extension.

**Conclusion:** Clang/LLD plus a small native converter can produce a structurally
accepted import-free E32 experiment. This completes neither a general converter
nor PLAN milestone 1's runtime gate.

**Implementation decision:** Declare e32-pic-experiment explicitly in symbian.toml.
Require trusted ELF links retaining all relocations. Reject absolute/dynamic
relocations, external symbols, writable data, TLS and constructors rather than
discarding them. Hand-written addresses or missing relocation records cannot be
proved absent by format inspection. No native scheduler is needed. Boundaries
release the GIL and translate Status/StatusOr after reacquiring it. Preserve
LLVM tool symlink names in argv[0]: resolving ld.lld to generic lld breaks driver
selection, as demonstrated by the initial failed build.

**Remaining uncertainty:** The direct kernel-thread exit skips User::Exit cleanup
and is restricted to a no-resource probe. Belle's SVC mapping, actual process
creation, complete loader validation, imports, builtins and C++/leave ABI still
require matched runtime tests. Build reports retain loader/runtime flags false.

## 2026-09-30: native emulator build and disposable root smoke tests

**Question:** Can EKA2L1 build on the current macOS host and use private state
before a ROM/Z image is available?

**Experiments:** Initialized all submodules of EKA2L1 revision
2594edf4d6bf55d7bd3f0b46250fe2318d4dc2e8. Installed Qt base/tools/Svg 6.11.2.
Configuration first failed because bundled FFmpeg libraries were Intel-only.
Built the pinned FFmpeg submodule c2022fa4637301736ff7230e7cee5a7e8cd7d45a using
its macos_arm64-build.sh. Built the desktop bundle and ekatests with Ninja,
RelWithDebInfo, camera and LuaJIT disabled. File inspection reports an arm64
Mach-O executable. The final ad-hoc bundle signature verifies. Upstream ekatests
passes 28,172 assertions in 288 test cases. The upstream 11.0 deployment target
disagrees with FFmpeg's 12.0 and
host GTest's 26.0; older-host distribution is not verified.

The first offscreen smoke launch aborted because the bundle ships only Cocoa's
platform plugin. A native desktop --help launch then timed out after 20 seconds
with no devices available. It had written logs/assets/settings inside the
explicit root, and no global EKA2L1 application-data directory was created.
Inspection found a worker waiting on init_event while the CLI failure path
joined it after waking only graphics_event and kill_event.

The tracked instance-root.patch redirects macOS data and default Qt settings
through EKA2L1_DATA_ROOT, rejects invalid roots, makes standalone --help/-h return
zero before worker initialization, and wakes init_event on CLI failure. It also
changes the GDB bind address to IPv4 loopback. Patch SHA-256:
`9b353b0319c7247984165cfb1ced343f9a20cc6ba37b16a1aa91be61ee800652`.
Reverse-application checks match the compiled checkout. No patch was published
or sent upstream. Qt translation generation also changes ignored .ts files.

Four Pytest smoke cases pass: independent roots retain separate Qt settings and
assets, relative roots fail without creating the requested path, a file cannot
be an instance directory, and unknown CLI options exit normally within the bound
without installed device images. Full verification passes 51 Pytest cases,
16 platform GTest cases and five independent oracle GTest cases, Black/Ruff,
clang-format, regenerated stubs and clangd's target source check with zero errors.
CLI doctor still works without an installed native extension; native operations
then return FAILED_PRECONDITION.

**Conclusion:** The emulator builds and its desktop CLI can be smoke-tested with
private settings/assets on this host. This is not a guest boot or proof that all
runtime paths are isolated. No ROM, Z image or firmware was imported.

**Implementation decision:** Keep the emulator, licenses, runtime state and build
products outside Git. Track only replay instructions, the source patch and
independent tests. No emu install/launch/test facade is published before actual
guest completion and isolation can be measured.

**Remaining uncertainty:** Matched legally available ROM/Z assets, Belle/FP2
process startup and application support, all runtime storage paths, concurrent
instances, stopped golden-state restoration and debugger attachment. Device
identity/preservation/recovery remain open; the physical phone was untouched.

## 2026-09-30: historical validation and ROMless CPU execution

**Question:** Does the linked E32 satisfy more than the emulator parser, and
does the generated ARM/Thumb calculation execute with the intended result?

**Experiments:** Compiled Nokia's unchanged ValidateWholeImage implementation
from the pinned kernelhwsrv f32image.h. The separate host adapter supplies
fixed-width types, UID/security declarations, constants and the original
checksum implementation. Assertions check header sizes 124/128/156 and the
export-description offset 152. The fixture is bounded, uncompressed and trusted;
the historical pointer-based validator is not exposed as a general file API.
Seven GTest cases pass: acceptance, CRC damage, negative heap with correct CRC,
insufficient entry/CodeSegID space, missing imports, wrong ARM ABI and truncated
code. The original validator and checksum source were not edited or copied into
the maintained native library.

A synchronous EKA2L1 CPU harness parses the same E32, maps read-only code and
a writable private stack, and starts in ARM user mode. Eight cases pass across
dyncom and dynarmic, load addresses 0x8000 and 0x20000, and positive/negative
inputs. Within a 512-step bound, startup enters Thumb C++, restores the stack,
returns to ARM and reaches SVC 0x73. The callback observes r0=0xffff8001,
r1=0 and r3=0. The normal calculation yields exit reason zero; intercepting
the volatile stack input and changing 16 to 17 yields reason 42. The harness
does not handle the SVC in a Symbian kernel. Dynarmic has fallback paths, so
these are two backend configurations, not wholly independent CPU implementations.

The E32 artifact remains SHA-256
`997cd9c5ec35281f261a08cb3c2ca6a36c74be969b4a72cbbd8ede5ff5332395`.
`toolchain verify-probe` now runs the four separate oracle executables with a
private copy of that input. It clears inherited GTest filtering/sharding,
requires all 20 cases completed without failures/skips/disabled tests, bounds
each native process to 15 seconds, and retains JSON/logs and binary/input hashes.
Input and binary replacement checks reject changed bytes during verification.
Its machine-readable report keeps symbian_loader_verified and runtime_verified
false. Python tests cover missing evidence, reduced/skipped/failed/disabled
reports, isolation from inherited test settings and a real CLI invocation.

Full verification passes 59 Pytest cases with both optional dependency paths,
16 maintained native GTest cases, 20 independent oracle GTest cases and
288 upstream EKA2L1 cases. Black/Ruff, clang-format, generated stubs and
git diff whitespace checks pass. Replay instructions include all three pinned
upstream checkouts and the new native targets.

**Conclusion:** The maintained import-free image passes the historical whole-
image validator and executes its integer/interworking/stack probe under EKA2L1
CPU cores. This is useful partial ABI evidence, not completion of PLAN milestone
1. No Belle process was loaded or launched.

**Implementation decision:** Keep the GPL emulator harness and EPL historical
oracles separate from the platform core and wheel. Python orchestrates tests and
records evidence; native libraries retain format logic. The harness is
synchronous and needs no new scheduler, GIL callback or Python object holder.

**Remaining uncertainty:** Belle loader changes, actual process creation and
kernel dispatch, matched ROM/Z assets, imports, builtins, floating-point/class
ABI, constructors and leave/cleanup behavior. No phone operations occurred.

## 2026-09-30: import-free process through the emulator kernel

**Question:** Can the E32 advance beyond CPU callbacks to actual EKA2L1 process
creation and kernel exit before a matched ROM/Z image is available?

**Research finding:** EKA2 thread initialization enters the executable's own
startup. The euser.dll bootstrap requirement in libmanager.cpp belongs to EKA1.
The import-free EKA2 image can therefore be tested directly through EKA2L1's
virtual filesystem, library manager, process constructor, memory model and
guest scheduler. This does not supply Belle user libraries or system servers.

**Experiments:** Added a separate GPL research GTest executable. It copies the
unchanged E32 into a private temporary host directory, mounts that directory
as a write-protected virtual C drive, installs the flexible memory model and
selects the emulator's epoc10 profile. The original spawn_new_process path
loads C:\\sys\\bin\\probe.exe, creates its main thread and schedules its context.
Execution reaches the real ThreadKill handler via the kernel's original SVC
callback. No SVC implementation or loader algorithm is replaced. CPU faults
stop the test and fail it; no kernel exception-handling behavior is claimed.

Eight cases pass on dyncom/dynarmic: normal exit, changed-input failure,
another launch after failure exit and rejection of a missing executable.
The code maps at 0x70000000, rather than its ELF link address 0x8000. Both
backends execute ARM/Thumb interworking, return exit reason zero normally and
42 when input 16 changes to 17, and release the process address space. The
negative control flushes the TLB before each step so the MMU stack-write callback
can change the input. Each launch is limited to 512 CPU steps. Exit reasons,
backend/profile, code address, injection count and memory release are recorded
in GTest JSON. The test retains diagnostic process/thread objects until kernel
wipeout while checking their exit state and released memory model.

The first compile found a throwing inline UID helper, and the first link required
the upstream test-only platform.cpp UI hooks. The tracked runtime-probe.patch
initializes previously uninitialized ROM mapping fields and the VFS ID counter,
and changes invalid UID indexing from throwing to fail-fast abort. Its SHA-256
is `6b477e9581aa0b3c43e88fbfd38ebec20108ea83f1e69ed9ac98105940dc22a9`.
The emulator loader, scheduler, memory mapping and SVC algorithms remain
unchanged. The original timer lifecycle is reused; its existing worker stops
before kernel teardown. No platform scheduling library or Python callback was
added. The harness itself compiles with exceptions disabled.

Rebuilt the desktop emulator and upstream tests against both tracked patches.
The bundle signature verifies. All 59 Pytest cases, 16 platform GTest cases,
28 independent oracle GTest cases and 288 upstream EKA2L1 cases pass. Format,
stub and whitespace checks pass. The E32 digest remains
`997cd9c5ec35281f261a08cb3c2ca6a36c74be969b4a72cbbd8ede5ff5332395`.

**Conclusion:** This executable demonstrably loads, runs and exits as an EKA2L1
process in the tested ROMless epoc10 configuration. It is stronger evidence than
the CPU-only harness and permits further host-side toolchain work. It is not
an installed Belle/FP2 environment, a full SDK or a general application runner.

**Implementation decision:** Extend verify-probe to require all five research
binaries and 28 completed cases. Report eka2l1_process_verified and
kernel_exit_verified separately; keep symbian_loader_verified/runtime_verified
false until a matched target environment is tested. Keep the general emu and
application-test facade pending its install/launch/isolation gates.

**Remaining uncertainty:** Matched Belle ROM/Z, User::Exit cleanup, imported
DLLs, system services, complete target ABI, package installation, desktop boot,
debugger attachment and full runtime storage isolation. Physical identity,
preservation and recovery remain unverified; the phone was untouched.

## 2026-09-30: CMake/Ninja project integration and wheel replay

**Question:** Can the direct Clang/LLD experiment become a reusable modern
project build without changing its target contract or losing reproducibility?

**Experiments:** Added an ARMv5T Generic toolchain and SymbianPic CMake module
inside the Python package, plus CMakeLists/presets for e32_probe. The helper
accepts multiple local sources, tracks its linker script, compiles C++20 Thumb
PIC with exceptions/RTTI disabled, and links using ld.lld directly with retained
relocations. Python reads CMake's codemodel/cmakeFiles API, invokes the Ninja
build, and converts the ELF through the existing native boundary. No format
algorithm moved to Python and no new scheduler or callback binding was added.

The first CMake build tried to use unavailable clang-scan-deps for C++20 modules.
This import-free profile uses no C++ modules, so module scanning is explicitly
disabled. A cached/fresh comparison then exposed different built-in CMake
configuration input lists. Generated and built-in files are excluded from the
project-input comparison; CMake's version is recorded separately. Project and
platform module files remain hashed and checked.

A multi-source test with source/build paths containing spaces exposed a missing
header hash: Ninja's declared inputs list does not include compiler-discovered
headers stored in its deps log. The wrapper now obtains those through the
documented `ninja -t deps` tool, checks record counts/validity and records header
digests. It does not parse Ninja's binary database. Editing a local multiplier
header changes the E32, a fresh second build agrees, and the next cached build
performs no compilation. Three new integration cases also check missing-target
status and reject the obsolete TOML source fields. The existing source-escape
test now exercises the CMake helper's actual file check.

The baseline remains byte-for-byte identical:

- ELF SHA-256: `4031252adc395d05bb7f3477262b4c00c0018aee1bc7eba40daa35feaa798e4e`
- E32 SHA-256: `997cd9c5ec35281f261a08cb3c2ca6a36c74be969b4a72cbbd8ede5ff5332395`

That CMake-produced E32 passes all 28 historical/CPU/emulator-process oracle
cases. clangd consumes its CMake-generated database with zero errors. Built the
arm64 Python 3.12 wheel, confirmed both CMake files are packaged, installed it
into a separate environment under .symbian/wheel-check-gsu0lt0v, and rebuilt the
same E32 from outside the repository's Python import path. The recorded module
input paths point into that wheel installation. Host tools are CMake 4.4.3,
Ninja 1.13.2, Apple Clang 21 and LLD 23.1.2.

Full Pytest passes 62 cases with the optional emulator/oracle paths. Existing
native behavior remains covered by 16 platform GTest cases and the 28 oracles;
the upstream 288-case emulator suite passed at the preceding checkpoint.
Black/Ruff, generated stubs and whitespace checks pass. Project instructions
are in BUILDING.md, and build reports now use symbian.e32-pic-experiment/v2.

**Conclusion:** CMake can own the build graph and source intelligence while
the modern facade owns reproducibility evidence and native E32 conversion.
The first artifact's measured emulator behavior is retained across that change.

**Implementation decision:** Keep the primary Ninja tree for incremental builds
and real compilation-database paths; use a fresh temporary tree for the second
build. Declare sources/startup/linker script once in CMakeLists, and reserve
symbian.toml for project identity, preset and experimental UID. Package the
toolchain/module with the wheel. Keep object-only compiler research separate.

**Remaining uncertainty:** General SDK imports, writable data/constructors,
package generation/installation, matched Belle runtime and complete target ABI.
The build comparison is local repeatability, not a hermetic compiler/input
attestation. Physical preservation/recovery inputs are still unknown.

## 2026-09-30 — Native SISX and disposable install/launch/uninstall

**Question:** Can the macOS build path generate a package and use EKA2L1's
existing installer before kernel execution, without a historical Windows tool
or Belle ROM?

**Historical behaviour and evidence:** Cloned the public Nokia appinstall
repository to ignored research/upstream/appinstall, pin
`760927eba63e3324cceee974b9ed582da90cb9a3`, preserving its EPL-1.0 material.
Read secureswitools/swisistools/source/sisxlibrary, particularly siscontents.cpp,
header.cpp, sisarray.h, siscompressed.h, sisinfo.cpp, sisfiledescription.cpp,
sishash.cpp and sisdate.cpp. UID1 is 0x10201a7a; UID2 is reserved zero; UID3 is
package identity. Fields carry type/length and four-byte padding; arrays omit
element types from individual headers. Controller and data CRCs cover serialized
fields including headers/padding. File descriptions bind indexed data to targets
and carry SHA-1. The historical default epoch is 2004-01-01, with zero-based
month. EKA2L1's pinned package manager/interpreter already installs files and
registries using only its virtual filesystem/configuration. Its source stores
the file digest without checking certificate/capability policy or enforcing a
phone installer contract.

**Experiments:** Implemented an independent native C++ SIS writer/inspector for
one ordinary import-free E32 executable, no dependencies/scripts/signatures,
uncompressed streams, English ASCII metadata and experimental UIDs. Package
UID 0xe0000809 differs from executable UID/SID 0xe0000808. Fixed the date for
repeatability. Native logic reuses E32 inspection and a shared native UID CRC
helper; no binary format moved into Python. SHA-1 uses static OpenSSL 3.6.5
libcrypto.a, matching A11's linkage pattern. Only the pybind11 boundary enables
exceptions, and it releases the GIL around the stateless native work. The wheel
includes OpenSSL's Apache-2.0 license and has no Homebrew crypto dylib dependency.

Seven maintained SIS GTest cases cover round-trip identity, every truncation and
single-byte mutation, hostile lengths/trailing data, metadata/path/version
rejection, required E32 validation and payload size. A payload changed with a
recomputed data CRC still fails SHA-1; deflate/run operations with a recomputed
controller CRC remain unsupported. Reused the existing E32 fixture and moved the
process environment into a GPL test header so no second scheduler or kernel
implementation was introduced.

Six new GPL oracle cases install into a fresh private writable C filesystem,
compare installed bytes to the independently supplied E32, and inspect registry
identity/version/SID/hash. Both dyncom and dynarmic launch the installed image
through EKA2L1's unchanged loader, scheduler and kernel SVC dispatch. Registry
reload and uninstall remove the executable and prevent process creation.
Reinstall after a changed-input failure exit then runs normally, with exit reasons
42 and 0. The independent hashlib baseline for the unchanged E32 is
`f464490fdf04c80df2e778f65326189e9e2b38d8`; the registry's legacy digest agrees.
A seventh separate EPL case compiles Nokia's original checksum.cpp and agrees
with UID and controller/data CRCs. No EKA2L1 package algorithm was patched.

The baseline E32 remains
`997cd9c5ec35281f261a08cb3c2ca6a36c74be969b4a72cbbd8ede5ff5332395`.
The package is 908 bytes, SHA-256
`9065031847f1db3b1fae1779b478208c38ea3f61a2e5554c2bd293106903abb9`.
`toolchain verify-package` preserves 35 complete cases: the existing 28 oracles
and the seven new ones. Its report is .symbian/package-check/report.json with
retained private inputs, GTest JSON/logs and binary digests. Test filters/shards
are removed and full expected counts are required. Python exposes package,
SIS inspection and verification commands with canonical statuses and strict
TOML fields. The full-suite run exposed a project test that appended its legacy
source field beneath the new package table; corrected it to insert explicitly
into the project table, preserving the original check.

Validation completed: 73 Pytest cases with both optional dependency paths,
23 maintained native GTest cases, all 35 independent native cases, and the
unchanged upstream suite's 288 cases/28172 assertions. Black/Ruff, clang-format,
generated stubs and whitespace checks pass. Built the macOS arm64/Python 3.12
wheel, confirmed packaged crypto licensing and packaging modules, and installed
it into .symbian/sis-wheel-check-wbeb510b/venv. From outside the repository's
Python import path, that wheel generated the identical SIS and completed all
35 verification cases. wheel-result.json records the actual imported wheel path
and retained check report. Otool shows only system framework/libc++/libSystem
links for the native module, with no Homebrew crypto dylib.

**Conclusion:** A modern native writer and EKA2L1's existing installer provide
an observable build→package→install→kernel-exit loop for this maintained probe.
No VM, `.pkg` parser or general package scheduler was needed.

**Implementation decision:** Keep the production writer/inspector synchronous
and bounded, with Python handling policy and reports. Keep historical EPL CRC
and GPL emulator probes in separate research binaries, outside the wheel.
Publish a verification command for the maintained profile, not a general
untrusted SIS installer API. Document packaging in PACKAGING.md.

**Remaining uncertainty:** Matched Belle ROM/Z and installer/loader behaviour,
certificates/capabilities, SDK imports, writable data/constructors/full C++ ABI,
GUI resources and ordinary applications. The fixed timestamp and local repeat
are not a hermetic toolchain attestation. The sole 808's exact identity, preserved
firmware/ROM and recovery baseline remain unknown. No hardware operation ran.
