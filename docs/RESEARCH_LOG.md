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
