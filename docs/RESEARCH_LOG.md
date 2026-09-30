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
