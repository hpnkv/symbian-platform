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

## 2026-09-30 — Frozen ordinal proxies and public SDK headers

**Question:** Which properties of a Symbian import library actually matter, and
can modern Clang/LLD link an original-header User::Exit call without recreating
a historical SDK/compiler environment?

**Historical behaviour and evidence:** Read the pinned kernel source's
kernel/eka/eabi/euseru.def, kernel/eka/include/e32def.h/e32cmn.h/e32std.h,
euser/us_func.cpp and euser/epoc/arm/uc_exe.cpp. The public table contains 2546
exports and assigns `_ZN4User4ExitEi` ordinal 641. Its library MMP links as
euser.dll and records public DLL UID3 0x100039e5; that does not identify the
physical phone's exact DLL/version. User::Exit notifies thread exit, closes
handles and invokes cleanup. Startup establishes the thread heap, TLS, DLL and
static initialization before E32Main. A direct ThreadKill experiment cannot
stand in for those contracts.

Read buildtools' pl_elfproducer.cpp, pl_elfexecutable.cpp and e32imagefile.cpp.
Proxy symbols name ordinal words rather than function implementation addresses.
The ordinal getter requires section index ESegmentRO=1. The historical dynamic
reader interprets its pointer fields as file offsets, and ELF version metadata
carries the target DLL name distinct from the proxy soname. Public producer
flags use BPABI version 4; modern Clang/LLD uses EABI5. The method oracle below
checks ordinal lookup only, not acceptance of all modern ELF flags by a complete
historical consumer.

**Experiments:** The first header compile with only __GCCE__ lacked Int64,
IMPORT_C and literal definitions. Selecting __GCC32__/__GCCV3__ enabled the
actual historical GCC branch. Adding __EABI__ supplied template specialization
syntax. No original header was edited. The first layout assertion incorrectly
expected TRequestStatus to occupy one word. The public class contains iStatus
and iFlags and is eight bytes; EKA2L1's EKA2 request status also has both fields.
Corrected the measured profile. Integer, UID, interval and descriptor assertions
now compile with ARM soft-float C++20 and exceptions/RTTI disabled.

A Clang assembly ordinal word plus an LLD version script can resolve the original
User::Exit declaration. Quoting the version-script node name caused LLD to
retain quote characters in ELF metadata; unquoted euser.dll produces the required
plain identity. A custom linker script makes the ordinal section first and
keeps dynamic metadata addresses equal to their file offsets. The proxy links
and repeats byte-for-byte without an independent hand-written ELF writer.

Implemented cpp/symbian/sdk: bounded ASCII frozen-DEF parsing, unique names and
ordinals, ABSENT/DATA metadata, selected function source generation and bounded
inspection of the generated proxy ELF contract. Sparse ordinals are retained,
not renumbered. Invalid/absent/data selections, aliases, unsupported directives,
unsafe basenames and decorated UID/version names are rejected. Native logic
returns Abseil statuses, disables exceptions and releases the GIL in its bindings.
No scheduler/callback/reference holder is introduced. Python runs CMake/Ninja,
hashes the DEF and consumed source/header dependencies, compares two build trees
and retains the actual database. Target C++ probe source lives under cpp and is
installed as a wheel resource; original SDK assets are not redistributed.

The public source builds euser.dso with one slot, ordinal 641. A User::Exit call
including e32std.h links to it and retains a version need for euser.dll. Both
artifacts repeat in separate CMake trees:

- Proxy SHA-256: `c53aa0b81ec07f6d18c8eab0298a8237e7906909eaa5caac975e55d3659876fb`
- Link probe SHA-256: `934eb1b3da3c3d7cde86388e797a61dfd251e1c32ed7608c65567ae8a42b272d`

Nokia's original GetSymbolOrdinal method is extracted unchanged at CMake
configuration from pl_elfexecutable.cpp and compiled in an optional separate
EPL research test using asserted 16/32-byte symbol/program declarations.
The complete source file's SHA-256 is
`5dacc5f9ef9d830e721548483cd9b6e7bb5ace458d189a0807cb89a098f69c6a`.
It independently reads 641 and returns UINT32_MAX after the section index is
changed. This is a trusted fixture method oracle, not an untrusted parser API
or whole-consumer compatibility result. C++ library/source tests and Python
integration cover parsing failures, no renumbering, actual multi-export proxy
builds with spaces in paths, every generated-ELF truncation, read-only bound
metadata and the original public header/link probe.

Evidence is retained beneath .symbian/euser-proxy: report.json, header_probe ELF,
readelf metadata/relocations, historical_ordinal.json and clangd.log. Clangd checks
the real header translation unit with zero errors. Default LLD places the import
R_ARM_JUMP_SLOT at 0x302dc in a writable GOT/PLT segment with virtual addresses
unlike file offsets. It is not convertible by the existing import-free E32 path;
reports retain import_execution_verified/symbian_loader_verified false.

**Validation:** All 78 Pytest cases pass with the optional emulator, native
verification binaries and public kernel source supplied. The 28 platform GTest
cases and the new historical ordinal method case pass. The existing 35
independent source/emulator checks run through the Python verification tests,
bringing the independent total to 36. Black/Ruff, clang-format, stub regeneration
and whitespace checks pass. Native core compile commands retain -fno-exceptions.
The preceding checkpoint's 288 upstream emulator cases remain evidence for its
unchanged sources; they were not rerun for this SDK-only change.

Built a new wheel and installed it in an isolated virtual environment outside
the source import path. The wheel includes the target header_probe.cc resource.
Its installed CLI compiles the public headers and reproduces both artifact
digests above. Evidence is in
.symbian/sdk-wheel-check-k067mfyh/wheel-result.json, which records the installed
Python module path, source/header hashes and both CMake build logs.

**Conclusion:** Frozen function contracts and original header declarations can
be exposed through modern native utilities and LLVM without carrying the full
SDK or writing another ELF emitter. Compiling and linking them is evidence for
those exact contracts, not for the runtime or complete target ABI.

**Implementation decision:** Provide `toolchain import-proxy` and native
inspection for the narrow generated profile. Keep public headers/definitions
as explicit external inputs and an isolated optional historical-method oracle.
Use CMake/Ninja for real objects, dependency tracking and clangd. Document in SDK.md.

**Remaining uncertainty:** E32 import/GOT conversion, decorated DLL version/UID
identity, actual target euser bytes, startup heap/TLS/static initialization,
leave/cleanup semantics, data exports, full C++ ABI and matched Belle runtime.
The phone's identity, preservation and recovery baseline remain unknown.

## 2026-09-30 — Eager E32 imports and compiled development DLL execution

**Question:** Can modern LLD function calls become E32 ordinal imports without
retaining an ELF dynamic loader, and can an independent emulator consumer execute
the imported compiled function after relocating both code segments?

**Historical behaviour and evidence:** Read the pinned f32image.h import block
and ValidateImports implementation, buildtools' E32ImageFile::ProcessImports,
and EKA2L1's libmanager.cpp/codeseg.cpp. ELF-derived E32 import lists contain
code-relative slot offsets. Slot low/high halves contain ordinal/addend. The
loader locates a dependency, looks up its ordinal in the attached process and
writes the relocated export address into the slot. The public converter rejects
imports into its writable data segment. ELF proxy soname and target LinkAs DLL
identity remain distinct. The original source/checkouts stay isolated research
dependencies; no historical dynamic loader is added to the platform.

**Experiments:** An LLD ET_EXEC custom script puts .plt, .got.plt and dynamic
metadata in one RX load, with a matching read-only PT_DYNAMIC. A retained Thumb
BLX reaches the ARM PLT veneer, which computes its slot address relative to PC.
That veneer needs no load-address relocation after the slot is eagerly patched.
The original-header shared ELF from the preceding experiment remains outside
this ET_EXEC layout.

Native conversion resolves undefined global function symbols against version
records and validated proxy metadata, then writes original ordinals and canonical
E32 import blocks. Retained static calls must reach their own PC-relative LLD
veneers. Dynamic tags, all GOT slots, identities, counts, bounds and string
tables are checked. Only zero-addend R_ARM_JUMP_SLOT function imports are
supported; data/BSS/TLS/constructors, RELA and decorated DLL identity remain open.
DLL case aliases, duplicate/ambiguous functions and unsupported references fail.
The import-free path preserves its previous ELF/E32 bytes and checks.

The first two-DLL test failed because I assumed interleaved 32-byte version
records. LLD packs 16-byte version headers followed by 16-byte auxiliary records.
Readelf exposed the offsets; corrected that layout and reran the test. The native
parser now checks this exact generated profile. Both DLL identities retain
independent ordinals, 7 and 641. This does not generalize to every ELF version
record producer or export type.

The modern CMake project path accepts explicit external proxy inputs, hashes
them with compiler-discovered dependencies and builds twice. Native bindings
copy Python data before releasing the GIL and use canonical Abseil statuses.
Core code keeps exceptions disabled. No scheduler, callback or reference holder
is added to production libraries. SIS packaging and the old maintained-probe
verifier explicitly retain their import-free scope.

Built our own integer function as Thumb C++ in an independent development DLL
fixture. An isolated research producer uses Nokia's original image declarations
and checksums, adds an ordinal-7 export and missing-export bitmap, and validates
the whole result. Its fixed function placement is asserted in the linker script.
This producer is outside the wheel and is not general DLL conversion.
It implements no EUSER function and claims no SDK startup/cleanup compatibility.

Six EKA2L1 cases cover dyncom/dynarmic, positive/changed input and repeated launch
after failure. They parse the import block independently, locate the loaded DLL,
compare the patched slot to its relocated export, observe the CPU at that
function, and check kernel exit reasons 0/42 and address-space release. No loader,
relocation, scheduler or kernel dispatch algorithm was replaced. Added passive
observation hooks to the existing research harness. The phone was not accessed.

The maintained artifacts are:

- ELF SHA-256: `5e7eb4c1662c975de9f0d45675ac8738679bc3a2d98160d67d6b4e44319f841b`
- E32 EXE SHA-256: `8f6cbed4ca3fe010be4d73b276d3671218e9cb3c3e4fbae29284048bced5e1a4`
- Development DLL SHA-256: `e90cbeb360f6ededc04b746f2827ff54ff09d33a06029b1a26b2542f0dc608b0`

**Validation:** All 87 Pytest cases pass with optional emulator, native oracle
and public-header paths supplied. All 32 platform GTests and 42 independent
oracle cases pass. The historical checksum and seven whole-image validator cases
also pass against each new EXE/DLL fixture. The missing-import negative control
now clears iImportOffset explicitly, so it remains a meaningful missing-section
check when testing an imported image. Core builds have no compiler warnings.
Black/Ruff, clang-format, generated stubs and whitespace checks pass. Clangd
checks examples/import_probe/probe.cc with its persistent database: zero errors.
The previous upstream 288-case suite concerns unchanged emulator sources and
was not rerun for this converter/harness change.

Retained evidence: .symbian/import-probe/report.json,
.symbian/import-layout/verification-report.json (private input copies, binary
and artifact digests, 14 EXE checks and eight DLL checks), pytest.log, clangd.log,
and the independent validator/runtime JSON and logs.

Built and installed the new wheel in an isolated environment outside the source
import path. Its CLI builds the same proxy, ELF and E32 using the packaged CMake
module and native extension; import metadata and both executable digests agree.
Evidence is in .symbian/import-wheel-check-mz171mjk/wheel-result.json, including
the installed module path, wheel digest, compiler graph and both build logs.

**Conclusion:** Modern LLD calls can be converted into eagerly patched E32
function imports. The unchanged emulator consumer resolves and executes the
relocated compiled export in this development DLL experiment. No ELF dynamic
runtime or historical compiler environment was required.

**Implementation decision:** Publish the native imported-executable converter,
canonical inspection and separate e32-import-experiment CMake project profile.
Keep the development DLL producer as a research oracle until general DLL
conversion has independent evidence. Document replay and limits in IMPORTS.md.

**Remaining uncertainty:** Production DLL/export/relocation generation, actual
target EUSER and decorated module identity, SDK heap/TLS/DLL/static startup,
User::Exit cleanup/leaves, data imports, full ABI and matched Belle runtime.
Preservation, exact phone identity and recovery baseline remain unknown.

## 2026-09-30 — Native frozen DLL exports and mapped pointer checks

**Question:** Can the modern native converter generate a frozen-export DLL
with the public Symbian ELF loader's export-pointer relocation contract,
without a fixed-address research producer?

**Sources:** Pinned buildtools 7b35cd328d3a5e8e0bc177d0169fd409c3273193,
e32exporttable.cpp and e32imagefile.cpp; pinned kernelhwsrv
0c3208650587ac0230aed8a74e9bddb5288023eb, f32image.h and sf_lepoc.cpp;
pinned EKA2L1 2594edf4d6bf55d7bd3f0b46250fe2318d4dc2e8,
loader/e32img.cpp and kernel/codeseg.cpp. Primary source links are in RESEARCH.md
section 16. Original algorithms were read locally and the public header was
checked through its pinned upstream page.

**Evidence correction:** The preceding research DLL omitted the ordinal-zero
count and code relocations for export pointers, and cleared the unused high
bitmap bit. Nokia's structural validator accepted it. EKA2L1 can resolve its
separately retained export table using a base delta, so successful import lookup
and execution did not establish actual mapped table pointer relocation. The
public ELF loader skips a separate export adjustment because those pointers
must already be covered by code relocation records. Earlier results remain
scoped emulator lookup/execution evidence; they do not prove that fuller
contract or physical compatibility. The original producer remains research
material outside the wheel and is replaced in maintained runtime tests.

**Experiment:** Added native frozen DEF resolution against retained visible
ELF function symbols, preserving original ordinals, gaps, ABSENT entries and
ARM/Thumb addresses. Generate DLL/library UIDs, count prefix, complete ordinal
table, no-hole or full-bitmap header, and text relocations for every export slot,
including absence entry markers. Unused bitmap bits stay set. Header CRC spans
the entire aligned variable header. Native inspection checks identities,
bounds, bitmap shape, absence/address agreement, count word, canonical page
blocks and the exact export-slot relocation list. Optional eager imports share
the existing native import converter. General application pointer relocations,
writable data, data exports, TLS and constructors remain unsupported.

The e32-dll-experiment CMake project uses symbian_add_pic_dll, with ET_EXEC as
its trusted intermediate transport. Python supplies the in-project frozen DEF
and optional proxy paths, records their hashes, and requires two matching
ELF/DLL builds. Metadata exposes DLL identity, header size, exports and code
relocations. The synchronous pybind11 boundary copies Python inputs under the
GIL, releases it for native work, then reacquires it for results/status errors.
No scheduler, callback, event loop or Python reference holder is needed.
All six core translation units compile with exceptions disabled.

examples/dll_probe provides our compiled integer transform at frozen ordinal 7.
Its linked Thumb address is 0x8021, resolved from the real static symbol table;
there is no fixed function location assertion. Its minimal ARM startup retains
the EKA2 marker/reserved word, makes an internal PC-relative call, preserves LR
and returns zero. It provides no SDK initialization or resources. The process
harness checks the patched import slot, CPU execution at that function, kernel
exit/address-space release and repeated launch on both dyncom and dynarmic.
It now independently reads export relocation records and inspects the actual
mapped export array: ordinal 7 agrees with lookup, all six absent pointers
relocate to the mapped entry, and the count prefix remains seven.
No upstream parsing, relocation, memory, scheduler or kernel dispatch algorithm
was altered. Historical checksum/validator test adapters now retain complete
bounded variable headers; their original algorithms are unchanged.

**Validation:** All 101 Pytest cases pass with optional dependency paths supplied;
all 38 platform GTests and 42 independent oracle GTests pass. Native tests cover
ARM/Thumb addresses, no-hole tables, sparse/ABSENT ordinals, maximum ordinal,
relocation page boundaries, truncation, corrupt pointers/counts/blocks and
missing/data export errors. Nokia's checksum plus seven whole-image validator
cases pass separately for ordinals 7, 641 and 65,535. These exercise 156-, 236-
and 8,348-byte headers. A combined import/export image also passes those
consumers, but its executable startup is layout evidence only, not a runnable
DLL initialization routine. All six maintained import execution cases pass
with the generated DLL and stronger mapped-pointer checks. Core compilation
has no warnings; research links retain the documented host deployment/library
warnings. Black/Ruff, clang-format, generated stubs and whitespace checks pass.
Clangd consumes the DLL CMake database with zero errors.

Retained evidence is under .symbian/native-dll: report.json,
verification-report.json (38 checks across importer and three DLL variants,
private input copies, binary and image digests), pytest.log, native-ctest.log,
independent-ctest.log, compile-policy.json and clangd.log. The wheel is installed
in an isolated environment outside the source import path; it builds identical
ELF/DLL bytes and metadata using its packaged module and extension. Installed
path, wheel digest and build evidence are retained in wheel-result.json.
The earlier upstream 288-case checkpoint concerns unchanged emulator source;
that upstream suite was not rerun for this converter/test-adapter change.

Maintained artifact SHA-256 values:

- DLL ELF: `0ecaad29f85a031a99098e837905403b3fd39c875f88bb9121bdc698bc56c76e`
- Native E32 DLL: `188cd1a6d8d40a4d00d21dc74fedfa57912dea67d0e2c25811cc178f6ebe9f9d`
- Unchanged importer EXE: `8f6cbed4ca3fe010be4d73b276d3671218e9cb3c3e4fbae29284048bced5e1a4`

**Conclusion and decision:** Publish the scoped native frozen-function DLL
converter, inspector metadata and CMake project profile. Maintained ROMless
execution now checks generated export-pointer relocation rather than only
lookup. Keep full-bitmap output canonical even where sparse encoding would be
smaller. Preserve experimental runtime/loader flags false in build reports.
SIS and verify-probe retain their import-free executable scope.

**Open questions:** General code-pointer/vtable relocation, writable data/BSS,
static construction/destruction, TLS and target DLL initialization, SDK heap
and User::Exit cleanup/leaves, actual EUSER/decorated module identities, complete
C++ ABI and matched Belle runtime. One physical phone is still the only declared
hardware; exact RM/product/firmware, preserved ROM/Z and recovery baseline remain
unknown. No phone or device operation ran.

## 2026-09-30 — Internal pointer tables, C++ virtual dispatch and installation

**Question:** Which additional modern compiler output is needed to run ordinary
const callback tables and simple virtual dispatch through the existing loader?

**Sources and survey:** Re-read PLAN sections 10, 20 and 21, current RESEARCH.md,
STATUS.md and converter sources. Reviewed A11 native status and Python boundary
conventions; this change remains synchronous and adds no scheduler. Read pinned
Nokia pl_elflocalrelocation.cpp/e32imagefile.cpp, the EKA2L1 relocation consumer,
and Arm's ELF specification. Primary upstream pages were checked; links are in
RESEARCH.md section 17. A first guessed historical filename returned 404; the
local checkout identified pl_elflocalrelocation.cpp and its pinned page was read.

**Experiment:** A real Clang PIC function-pointer table emits `.data.rel.ro`
with SHF_WRITE and R_ARM_ABS32. Default visible table access also requires
GOT_PREL, outside the supported profile. Hidden internal table/class visibility
emits local REL32 access, while the table pointers still require load-time
adjustment. A single-inheritance virtual class similarly needs a vtable
function pointer, without RTTI, allocation or target runtime imports.
The initial research ELF and source are retained under .symbian/pointer-research.

The historical producer chooses text/data relocation kinds according to the
referenced segment. Its unresolved ELF pointer fixup adds a symbol value; the
modern ET_EXEC word is already resolved. Copying that fixup would incorrectly
add the symbol twice. The native converter instead preserves the linked word
and emits a Symbian text relocation. It checks retained symbol/target bounds,
word alignment, instruction-state agreement for functions, duplicate fixups and
in-range targets. External absolute pointers and GOT/dynamic metadata fixups
remain unsupported. A bounded native name reader admits `.data.rel.ro` and its
dot-suffixed sections only as non-executable PROGBITS tables inside the RX load.
Ordinary writable data/BSS, TLS and construction arrays remain rejected.
This is a trusted compiler/link contract, not semantic validation of arbitrary
hand-authored code. One-past-the-mapping pointer values remain unsupported.

Native inspection now handles application and DLL export fixups together,
requires all export slots and rejects import-slot aliasing. The combined
relocation count is bounded to 65,535. Existing relocation-free artifacts
retain their bytes. The former blanket ABS32-negative controls now check
out-of-range and unaligned pointers; new positive controls establish the
supported behavior. No relocation producer/parser logic was added to Python.
All six core translation units still compile without exceptions and return
Abseil statuses; existing GIL release boundaries cover this native work.

**Maintained vertical slice:** examples/pointer_probe builds four source units
with its real CMake database and discovered header dependencies. It has two
const callbacks (Thumb C++ and ARM assembly), a constant-text pointer with
addend one, and a Thumb virtual method in a stack object. Its four E32 fixup
offsets are 240, 244, 248 and 260. The separate GPL research harness uses the
unchanged EKA2L1 parser and process loader, inspects all four mapped pointer
values against the actual base delta, and observes the CPU at all three
function targets in their correct ARM/Thumb states. Both backends exit zero
normally, exit 42 with changed input, and launch successfully after failure.
Kernel exit releases the address space. The label pointer retains its addend.
The compiler-generated stack vptr and indirect method call execute as emitted.
No upstream relocation, memory, process, scheduling or kernel algorithm changed.

The same native SIS writer packages this pointer-bearing image. The existing
installer harness loads it, checks unchanged installed bytes and registry
fields, launches it, reloads the registry, uninstalls and reinstalls it. A
separate retained Python hashlib SHA-1 reference replaces the fixed old hash
only when supplied explicitly; the earlier fixture keeps its historical
baseline. The common verifier now clears inherited Symbian fixture variables
as well as GTest filters/shards before passing private copies. Existing filter
and input isolation controls verify that unrelated hash paths do not leak in.

The new `toolchain verify-pointers` command performs 14 original checksum,
whole-image validator and mapped-pointer/dispatch cases. Supplying `--package`
adds seven checksum/installer cases for 21. It records binary/artifact hashes,
private copies and complete native reports. `verify-probe` rejects code-fixup
images explicitly, preserving its earlier relocation-free scope. Build/package
and verification reports keep matched Belle and physical flags false.

**Validation:** All 112 Pytest cases pass with optional emulator, oracle and
public kernel paths supplied. All 41 platform GTests and 48 independent oracle
cases pass on Apple Silicon. The new six process cases pass on both backends.
Real-link combined import/application-pointer and DLL export/application-pointer
layouts pass the unchanged Nokia checksum and seven whole-image validator cases;
the combined DLL has executable startup and is layout evidence only. Negative
controls cover pointer value/state/alignment, duplicates, section-name bounds,
unknown writable sections, import-slot aliasing, verifier scope and output/input
collisions that must preserve supplied artifacts. The earlier
import/DLL/package tests remain green. Core builds have no warnings. Research
links retain the previously documented deployment/library warnings. Black/Ruff,
clang-format, generated stubs and whitespace checks pass. Clangd checks the
pointer example through its persistent CMake database with zero errors.
The unchanged upstream emulator 288-case checkpoint was not rerun here.

Evidence is retained in .symbian/pointer-probe (build report, pytest.log,
independent-ctest.log, clangd.log, compile-policy.json and wheel-result.json),
.symbian/pointer-package/package-report.json, and
.symbian/pointer-check/report.json with the 21-case private test reports/logs.
An isolated installed wheel outside the source import path reproduces identical
ELF/E32/SIS bytes and runs the complete 21-case verification loop. Its installed
module path, wheel digest and nested build/package/verification reports are in
wheel-result.json.

Artifact SHA-256 values:

- ELF: `abc3c3c9e31d3ca9f325627af4ce86813d1b7c295e6be9ea294fe344a583a9ce`
- E32: `82cbc8e080efdc844733a273bdcfcf69914b71e2535367188a43348e637ec73a`
- SIS: `b8201005adf9908402d11e84de3e90aa9c235cef013cf211dcd57a144bc82c7d`

**Conclusion and decision:** Native macOS Clang can supply these ordinary C++
const-table/dispatch contracts with local visibility and retained pointer
fixups. Publish the bounded internal ABS32/RELRO extension, maintained probe
and verification command. Reuse the existing package writer and loader; no
compatibility runtime or additional scheduler was needed. Replay and exact
profile limits are documented in POINTERS.md.

**Remaining questions:** General GOT/preemptible data and external function
pointers, writable data/BSS/TLS, target DLL initialization, global lifetime,
multiple/virtual inheritance and RTTI, target heap and User::Exit cleanup/leaves,
full Symbian C++ ABI, actual system DLL identities, matched Belle runtime and
physical installation. Exact phone identity, firmware/ROM/Z preservation and
recovery baseline remain unknown. No device operation ran.


## 2026-09-30 — C++20 language, module and library boundaries

**Question:** Can modern C++20 programs be supported, beyond selecting a compiler
standard flag? What needs a target runtime rather than only compiler support?

**Experiments:** Added examples/cxx20_probe with actual concepts/requires,
structural class template arguments, consteval, designated initialization,
constrained generic lambdas, defaulted equality, constinit callback tables,
char8_t and a no_unique_address layout assertion. C++17, signed constrained
arguments and dynamic constinit initialization fail for the expected reason;
positive controls use the actual ARM compilation command. Apple Clang 21 builds
it without C/C++ system headers or hosted libraries. Optimization preserves the
earlier pointer ELF/E32 bytes. Both backends pass mapped pointer/state checks,
indirect/virtual dispatch, failure/relaunch and the complete 21-case SIS loop.

Homebrew libc++ 23.1.2's unmodified host configuration fails for ARM availability
and thread configuration. A separate research __config_site disables those and
unsupported host facilities, leaving upstream header bodies unchanged. Upstream
Clang 23.1.2 compiles bit/concepts/span with its freestanding resource headers;
coroutine fails for memcpy, ranges for memory/mbstate_t/stdio, atomic for memory
and time declarations. These failures identify missing port work, not language
impossibility. The maintained opt-in library callback uses span, rotate and
population count, with a separate expected arithmetic reference. Apple Clang 21
builds this distinct E32; all 21 loader/installer checks pass. Its 186 input
hashes include upstream headers and the isolated configuration. No target libc++
binary, C library or host SDK is linked. Matching runtime/library configuration
will be required for a real port, as LLVM's vendor documentation specifies.

Upstream Clang 23.1.2 builds a minimal named module; the installed Apple compiler
rejects it with the tested ARM flags. Added examples/cxx20_module_probe with a
CMake CXX_MODULES file set and explicit target scanning. Independent CMake trees
produce identical ELF/E32, retain a BMI and record actual compilation commands.
Changing its immediate function and arithmetic body rebuilds an unchanged
importer and changes the artifact. The earlier exported constexpr object
experiment generated init_array; conversion correctly rejected it. The
maintained module uses an immediate function, requiring no static startup.
Its original parser/checksum/validator, both CPU backends at two addresses,
real kernel and SIS installer loop passes all 35 cases.

Package verification now supplies an independent hashlib reference for the exact
caller-supplied executable, like the pointer verifier. It preserves original
hash baselines for direct native tests, checks package payload size before
execution, and rejects report/hash output collisions with either input. The
original package and new module package both pass. All inherited fixture/filter
isolation remains in the existing common verifier; no format logic moves into
Python and no scheduler/callback/holder is introduced.

**Validation:** All 124 Pytest cases pass with optional emulator, public headers,
module compiler, libc++ configuration and oracle paths supplied. Root native
platform GTests and optional SDK ordinal oracle pass. Existing independent
loader/installer binaries are reused without algorithm changes: language 21,
selected library 21, module 35 checks. An isolated installed wheel outside the
source import path reproduces all three ELF/E32/SIS variants and repeats all 77
checks. Its module path, wheel digest, nested reports and source/tool hashes are
in .symbian/cxx20-probe/wheel-result.json. Commands/header logs are in
.symbian/cxx20-research; full Pytest evidence is in cxx20-probe/pytest.log.
The unchanged upstream 288-case checkpoint was not repeated.

**Writable-data investigation:** Public kernel sf_lepoc.cpp and EKA2L1 apply
separate code/data deltas. A real two-load PIC link emits REL32 across those
segments, which cannot assume the linked distance survives loading. An explicit
non-PIC link emits ABS32 with code/data targets: five code-to-data words and
three initialized-data words, including a BSS pointer and Thumb callback.
Artifacts remain in .symbian/data-research. The incomplete typed relocation/API
sketch was removed before publication; no writable-data support is claimed.
A future implementation must distinguish source section from referenced segment
and reject cross-segment relative contracts, then validate initialization/BSS
and mapped words under differing deltas.

**Decision and remaining work:** Publish the scoped C++20 probes and CXX20.md.
A broad target standard library needs C library/compiler-rt, allocation/failure
policy, ABI configuration, SDK heap/TLS/DLL startup and cleanup, static lifetime
and synchronization/event-loop adapters. Modules do not expose historical DLL
interfaces automatically. Complete conformance, matched Belle and physical
execution remain unverified; hardware/firmware identity is still unknown.
The user's subsequent GUI example request is the next active slice.
