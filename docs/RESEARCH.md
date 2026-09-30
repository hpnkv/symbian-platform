# Native Nokia 808 development: initial technical survey

This document answers PLAN.md section 26. Research date: 2026-09-30. The
implementation reference is the local A11 checkout at
`fcccb8cb6e1e67d7ba0822ac14cece9ee4c7091b`, including its working-tree guidance.

## 1. Available material

The initial project directory contained only PLAN.md and had no Git repository,
SDK, firmware, ROM image, emulator installation, source, or tests. The owner has
one Nokia 808; RM-807 is the intended target, but the actual product code,
firmware version, installed modifications, and recovery assets are unverified.
There is no second device. Do not invent a baseline from the model name.

This Apple Silicon host has Apple Clang 21, CMake, Ninja, Python 3.12, uv,
static GTest, and pybind11 headers. A11 supplies the native architecture reference
and a locally cached Abseil source tree. Its Thread library is available for
future native concurrency; its entire networking/runtime stack is unnecessary
for synchronous format inspection and host process orchestration.

Three research checkouts were initially obtained without submodules. The next
experiments built EKA2L1 with its recursive submodule pins and the tracked patch
in research/eka2l1. These remain separate research dependencies, outside the
platform library and Python wheel:

| Project | Inspected revision | Purpose |
| --- | --- | --- |
| [EKA2L1](https://github.com/EKA2L1/EKA2L1) | `2594edf4d6bf55d7bd3f0b46250fe2318d4dc2e8` | Emulator, loaders, command handlers, debugger |
| [Symbian kernel and user library](https://github.com/SymbianSource/oss.FCL.sf.os.kernelhwsrv) | `0c3208650587ac0230aed8a74e9bddb5288023eb` | E32 loader contract, headers, ABI references |
| [Symbian build tools](https://github.com/SymbianSource/oss.FCL.sf.os.buildtools) | `7b35cd328d3a5e8e0bc177d0169fd409c3273193` | ELF-to-E32 conversion reference |

## 2. Contracts that appear necessary

The phone consumes E32 executable images, target system APIs, and installation
packages. The host's historical IDE and build orchestration are incidental.

The authoritative
[f32image.h](https://github.com/SymbianSource/oss.FCL.sf.os.kernelhwsrv/blob/0c3208650587ac0230aed8a74e9bddb5288023eb/userlibandfileserver/fileserver/inc/f32image.h)
defines V-format E32 headers, ARM EABI and EKA2 entry-point flags, import formats,
relocations, UID checksums, header CRCs, and platform security fields. E32 code
is not simply an ELF file renamed `.exe`. Headers and DLL import stubs must match
the actual Belle runtime. DLL exports use stable ordinal contracts; arbitrary
ELF symbol resolution is insufficient.

Target C++ additionally needs descriptor layouts, calling conventions, class
layouts, virtual calls, runtime support, cleanup-stack/leaving semantics, and
the right startup path. Disabling host C++ exceptions does not establish how
Symbian leaves work. The [Arm ABI specifications](https://github.com/ARM-software/abi-aa)
are a starting point, not evidence of complete Symbian interoperability.

UI applications add resource files and application registration. Qt support
requires matching Qt headers/libraries and plugin contracts and should follow
a minimal native executable. A package builder must implement the applicable
SIS/SISX structure, integrity checks, file metadata, and signing preparation.
Physical installation also depends on device certificate and capability policy;
emulator installation cannot establish the phone's acceptance policy.

The public Symbian source predates some shipped Belle FP2 components. It is
research material; its headers and ordinal lists are not yet a verified 808 SDK.

## 3. Candidates for native replacement

| Historical facility | Modern implementation | Validation gate |
| --- | --- | --- |
| Carbide, SBS/Raptor, bldmake/abld | CLI plus CMake/Ninja | Repeatable builds, explicit target graph |
| GCCE code generation | Clang ARM backend | ABI probes, then loader and runtime tests |
| elf2e32 | Small native converter or isolated port | Structural validator and emulator loader |
| makesis/signsis | Native package library and crypto tooling | Official format, oracle packages, installer |
| rcomp and image utilities | Native resource/asset tools as needed | Byte comparisons and actual consumer |
| Historical source navigation | clangd compilation database | Real target headers and accurate flags |
| IDE debugging wrappers | EKA2L1 GDB remote plus symbols | Break, step, inspect real target process |

Begin with a stateless, bounded native binary inspector and an ARM compilation
probe. These expose the required contracts without adopting a legacy toolchain.
Do not implement complete resource processing or packaging before the first
executable is demonstrably loadable.

## 4. Possible compatibility requirements

GCCE or RVCT can serve as ABI oracles if preserved binaries expose behavior
Clang cannot yet reproduce. Investigate narrow differences before selecting a
compiler runtime. Source ports of elf2e32 are plausible; inspect the upstream
[converter](https://github.com/SymbianSource/oss.FCL.sf.os.buildtools/tree/7b35cd328d3a5e8e0bc177d0169fd409c3273193/toolsandutils/e32tools/elf2e32/source)
for import stubs, relocations, export holes, startup code, and compression.

Phoenix and Nokia USB drivers may need a sealed Windows recovery appliance.
Its architecture, Windows version, USB forwarding, and licensing must be chosen
from a recovery method verified for this physical phone. A Windows VM on an
Apple Silicon host is not automatically a working historical x86 USB-flashing
environment. There is currently no validated recovery path or recovery VM.

## 5. What EKA2L1 actually provides

The project implements the kernel and selected servers/libraries in host code;
it is not a complete RM-807 hardware/boot-chain simulator. Its
[README](https://github.com/EKA2L1/EKA2L1/blob/2594edf4d6bf55d7bd3f0b46250fe2318d4dc2e8/README.md)
describes partial application compatibility through Belle. That claim does not
prove Belle FP2, Qt, camera, or every 808 system component works.

Inspected facilities include:

* E32, SIS/SISX, resource, ROM and device loaders.
* Command options `--device`, `--install`, `--remove`, and `--run` in
  [thread.cpp](https://github.com/EKA2L1/EKA2L1/blob/2594edf4d6bf55d7bd3f0b46250fe2318d4dc2e8/src/emu/qt/src/thread.cpp).
* YAML settings for storage, IPC/SVC tracing and debugging in
  [options.inl](https://github.com/EKA2L1/EKA2L1/blob/2594edf4d6bf55d7bd3f0b46250fe2318d4dc2e8/src/emu/config/include/config/options.inl).
* Registers, memory, breakpoints, thread queries and process-related handling
  in the [GDB stub](https://github.com/EKA2L1/EKA2L1/blob/2594edf4d6bf55d7bd3f0b46250fe2318d4dc2e8/src/emu/gdbstub/src/gdbstub.cpp).
* Scripting hooks for process/thread/code-segment and IPC inspection.
* CMake builds with macOS code paths and ARM host interpreter/JIT infrastructure.

Important upstream constraints: `--install` currently installs to drive E;
`--help` returns through a failure path after initialization; `--listapp` has an
unfinished output implementation. Settings are read from `config.yml` relative
to the working directory, but every path lookup must be audited before claiming
complete instance isolation. The GDB listener binds all interfaces, so a local
debug interface needs an upstream loopback binding change or another enforced
network boundary. No reliable batch completion protocol, CLI screenshots,
full-machine snapshot interface, or headless application test loop has been
demonstrated. The local patch now fixes standalone help and CLI shutdown and
changes the debugger binding to loopback; debugger operation remains untested.

## 6. What to add around EKA2L1

The pinned emulator now builds natively and passes its upstream suite and
desktop CLI smoke tests. Next import a matched ROM and
Z-drive dump preserved from a legally available device; the upstream
[setup guide](https://github.com/EKA2L1/EKA2L1/wiki/Using-the-emulator)
requires both. Record the exact imported device/OS, emulator revision, build
options, and hashes. If a non-808 Belle image is used provisionally, label it.

Smoke tests verify two independent settings/assets roots and bounded CLI failure
shutdown. Full guest storage isolation and simultaneous instances remain to be
proved with a runtime. Golden state is a stopped-process filesystem baseline;
copying it does not capture running memory. Add install/launch completion and
application exit results inside the emulator rather than inferring success from
GUI logs. On macOS, `main.cpp` changes the working directory to Qt's global
application-data directory, overriding the launcher's working directory. A
per-instance root override is now available as a tested local patch. Audit all
runtime paths before using filesystem copies for concurrent guest runs.
Add framebuffer capture and
structured panic/IPC events through the
existing window/kernel facilities. Extend the upstream CLI/control surface where
possible. Keep deterministic execution as a separate measured capability.

## 7. Native macOS toolchain requirements

[Clang cross compilation](https://clang.llvm.org/docs/CrossCompilation.html)
supports target and CPU selection independently of the host. Start conservatively
with `armv5t-none-eabi`, Thumb, AAPCS and soft-float, without host system headers,
libraries, exceptions or RTTI. This is an experimental baseline, not a final
808 CPU/FPU specification. Inspect generated ELF metadata and compare repeated
builds before selecting target defaults.

A general executable additionally needs an ARM ELF linker (such as LLD), target
import DSOs, startup code, compiler builtins where emitted, ELF-to-E32 conversion,
and a loader test. Apple's Mach-O linker is not an ARM ELF linker. The initial
probe requires no linker or SDK and must report that narrower result clearly.
The compilation database can already use the real ARM compilation arguments.
LLD 23.1.2 now links the maintained E32 experiment. Native conversion accepts
one RX segment with retained internal PC-relative relocations and no SDK imports,
data, TLS or constructors. The independent EKA2L1 parser, Nokia's original
checksum source and unchanged whole-image validator accept it. A ROMless
harness executes its integer C++ probe on two EKA2L1 CPU backends at two load
addresses. A further ROMless harness loads a process through EKA2L1's filesystem,
loader, memory model and scheduler, and completes ThreadKill through its actual
epoc10 kernel dispatch. This does not establish matched Belle loader or runtime
compatibility.
Later tests must cover integer/float calling
convention, constructors, virtual dispatch, exports, imported API calls, and
leave/cleanup behavior independently.

## 8. Preservation and recovery inputs

Follow recovery/README.md to record product code, firmware, storage state,
modifications and physical condition without changing them. Keep serial/IMEI and
private backups out of Git. Preserve original firmware components and matched
ROM/Z dumps with provenance, hashes, variant and version. A digest proves byte
identity, not that firmware is authentic or will recover this unit.

The first archive tool should copy regular files into a new sealed directory,
reject symlinks/special files, verify copies, write a canonical manifest, and
provide independent verification. Make a second offline copy and retain the
manifest digest separately. Owner-controlled POSIX permissions alone cannot
provide immutable storage. Never gather these assets by flashing or privileged
phone modifications as an agent action.

## 9. Agent authority

Information/log/screenshot/application workflows are candidates for eventual
automation. Their transport and ordinary-package checks still need implementation.
Reboot, system installation and persistent-state mutation require a human-owned
authorization broker tied to exact device/action/artifact. A caller-provided
`approved=True` flag is not such a broker. No device executor is exposed initially.

Flashing, erasure, partitioning, bootloader, OTP, calibration and recovery are
absent from the automation API permanently. Agents can prepare documentation
and artifacts; a human operates recovery tooling independently. Emulator reset
does not grant physical-device reset authority.

## 10. Sequence of testable milestones

1. **Preservation foundation:** inventory template, sealed archive and tamper
   checks, explicit device policy. Physical preservation/recovery stays open.
2. **ARM code-generation proof:** repeatable SDK-free object, metadata inspector,
   compilation database and machine-readable report. Do not call it an app.
3. **First E32 executable:** obtain minimal verified imports/startup, link ELF,
   convert, validate, and run against the target runtime in EKA2L1.
4. **Project/package slice:** CMake executable, resource registration, minimal
   SIS and successful emulator installation/launch, including invalid-input tests.
5. **Disposable emulator tests:** fresh stopped baseline per test, verified
   isolation, bounded execution and panic/log/screenshot artifacts.
6. **Debugger slice:** breakpoint at source, registers/threads, correlated symbols,
   useful crash report; validate LLDB remote compatibility separately from GDB.
7. **Physical application deployment:** only after preservation and a validated
   recovery path, with a restricted transport and package classifier.
8. **Servers/DLLs/plugins:** ordinal and IPC tests in the emulator. Hardware and
   alternative-OS research remain later projects under PLAN.md's safety boundary.

## 11. First executable experiment evidence

The native converter derives its header contract from f32image.h. Startup uses
the marker and reserved code-segment word shown by
[uc_exe.cia](https://github.com/SymbianSource/oss.FCL.sf.os.kernelhwsrv/blob/0c3208650587ac0230aed8a74e9bddb5288023eb/kernel/eka/euser/epoc/arm/uc_exe.cia).
The no-resource exit experiment follows the register contract in
[uc_exec.cia](https://github.com/SymbianSource/oss.FCL.sf.os.kernelhwsrv/blob/0c3208650587ac0230aed8a74e9bddb5288023eb/kernel/eka/euser/epoc/arm/uc_exec.cia)
and the 0x73 ThreadKill mapping in EKA2L1's
[epoc94/epoc10 tables](https://github.com/EKA2L1/EKA2L1/blob/2594edf4d6bf55d7bd3f0b46250fe2318d4dc2e8/src/emu/kernel/src/svc.cpp).
It skips User::Exit cleanup and is not a replacement for application startup.

The separate checksum oracle compiles Nokia's original
[checksum.cpp](https://github.com/SymbianSource/oss.FCL.sf.os.buildtools/blob/7b35cd328d3a5e8e0bc177d0169fd409c3273193/toolsandutils/e32tools/elf2e32/source/checksum.cpp)
without changing its algorithms. EKA2L1's E32 parser currently omits header CRC
verification; a test deliberately demonstrates that blind spot. The platform's
native inspector checks it. Neither parser acceptance nor checksum agreement
proves that the real Belle loader will load, relocate and start the executable.
See RESEARCH_LOG.md for artifact hashes and research/eka2l1/README.md for replay.

The separate historical validator oracle calls ValidateWholeImage from the
unchanged f32image.h with fixed-width host declarations and asserted layouts.
Its seven cases include a valid image and six malformed controls. That public
source predates shipped Belle FP2 and is not proof of the phone's loader behavior.

Eight CPU cases cover dyncom/dynarmic, two load addresses and positive/negative
inputs. They observe ARM startup, Thumb C++ computation and the exit SVC's
register contract, including a restored stack. The SVC callback stops execution
without entering a Symbian kernel. No ROM, process, imports or system services
are present. `toolchain verify-probe` retains the independent results and hashes
while reporting matched Belle loader/runtime verification false.

Eight further cases create an import-free process using EKA2L1's actual loader
and flexible memory model under its epoc10 profile. Both CPU backends complete
normal and changed-input exits and repeated launches through the kernel;
missing files cannot create a process. Assertions check thread/process exit and
address-space release. The test profile supplies no ROM/Z image, target DLLs or
system services.
It is evidence that this image loads and runs in that emulator configuration,
not that the target Belle/FP2 system supports it. No kernel SVC algorithm was
replaced. The small runtime-probe.patch records host initialization fixes and
a UID-index invariant change needed for exception-free inclusion of headers.

## 12. CMake project path

The experimental E32 build now configures a declared CMake target using a Ninja
preset and packaged ARM toolchain/module files. The source graph is declared
in CMakeLists; symbian.toml selects the target/preset and UID. The retained
primary build tree supplies an actual compilation database and incremental
dependency tracking. A fresh second tree verifies identical linked/converted
bytes. The original ELF and E32 hashes remain unchanged for examples/e32_probe.

Project metadata comes from CMake's
[file API](https://cmake.org/cmake/help/latest/manual/cmake-file-api.7.html),
with generated and built-in CMake files excluded from the source-input comparison.
The tool version is recorded separately. Compiler-discovered headers are obtained
through Ninja's dependency tool; the declared `inputs` graph alone omits those
stored dependencies. Tests cover multiple translation units, paths with spaces,
local header changes, no-op rebuilds and missing targets. The installed wheel
contains the CMake files and reproduces the same E32 in a separate environment.
See BUILDING.md and RESEARCH_LOG.md for scope and experiment evidence.
