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

## 13. SISX packaging and disposable installation

The public appinstall checkout is pinned at
`760927eba63e3324cceee974b9ed582da90cb9a3`. Its
[sisxlibrary](https://github.com/SymbianSource/oss.FCL.sf.mw.appinstall/tree/760927eba63e3324cceee974b9ed582da90cb9a3/secureswitools/swisistools/source/sisxlibrary)
defines aligned typed fields, arrays with implicit element types, compressed
controller/file streams, checksums, file descriptions and data units. These
contracts permit a small independent native writer without rebuilding makesis
or importing the SDK. The current writer intentionally supports only one
uncompressed, unsigned ordinary executable package with a fixed timestamp.
It does not parse the historical `.pkg` language or implement signing.

EKA2L1's existing package manager can install into a host-backed filesystem and
registry without a ROM. A disposable harness uses its unchanged parser,
interpreter and registry before launching through the existing process kernel.
Installed bytes, UID/SID, package identity/version and registry reload are
checked. Uninstall and reinstall work on both CPU backends. The package hash is
checked against an independent hashlib baseline because this installer stores
it without enforcing phone signing/capability policy.

The separate original Nokia checksum implementation agrees with the SIS UID,
controller and data CRCs. The production core validates sizes, padding, CRCs,
SHA-1 and E32, and accepts only its canonical reconstruction. This bounded API
is separate from the trusted-fixture upstream harness. Python retains TOML,
filesystem and report policy. OpenSSL 3 libcrypto supplies legacy SHA-1 with
static linkage following A11; its license is included with the wheel.

The build/package/install/kernel-exit experiment now has 35 independent native
cases. This evidence supports an import-free ROMless loop, while matched Belle,
SDK services, complete ABI, GUI/resource processing, signing and physical
installation remain open. See PACKAGING.md and RESEARCH_LOG.md for replay and
artifact hashes.

## 14. Frozen exports and original-header compilation

A small native SDK component now reads frozen EABI definitions, preserves sparse
ordinals and generates selected function proxy sources. Modern Clang/LLD builds
these as ELF with a versioned target DLL identity. The linker script keeps ordinal
symbols in section one and dynamic addresses equal to file offsets, matching
assumptions in the public converter's
[ELF implementation](https://github.com/SymbianSource/oss.FCL.sf.os.buildtools/blob/7b35cd328d3a5e8e0bc177d0169fd409c3273193/toolsandutils/e32tools/elf2e32/source/pl_elfexecutable.cpp).
Nokia's unaltered ordinal lookup method independently reads the generated
User::Exit slot as 641. This is not the complete historical ELF consumer.

The original public e32std.h compiles as ARM C++20 with the GCC/EABI macros chosen
explicitly. A typed User::Exit call links against that proxy, and descriptor,
integer, UID and request-status size assertions pass for this source profile.
TRequestStatus is eight bytes with flags, also matching EKA2L1's EKA2 request
status representation. Clangd checks that translation unit with zero errors.
The SDK headers stay outside the wheel and version control.

Default LLD linking puts R_ARM_JUMP_SLOT imports into a writable GOT/PLT segment;
that research ELF remains outside the native converter's supported profile.
The separate eager import experiment below establishes code-region slot
conversion; execution against matched target DLLs remains open.
Startup and User::Exit cleanup still require target
heap/TLS/DLL initialization. See SDK.md for scoped CLI and oracle replay.

## 15. Eager E32 function imports

The converter resolves versioned undefined functions against bounded ordinal
proxies and emits canonical ELF-format E32 import blocks. LLD's PC-relative ARM
veneers reach GOT slots placed in the code region for eager loader patching.
Retained calls, DLL/version identities, GOT coverage and dynamic records are
checked in native code. Two-DLL tests verify LLD's packed version records and
separate original ordinals.

An independent development DLL supplies our compiled integer function at ordinal
7. EKA2L1 patches the import slot to its relocated export. Both CPU backends
execute the function, complete changed-input failure exits and launch again.
Nokia's unchanged validator/checksums accept the EXE and fixture DLL. These tests
supply no EUSER, Belle material or SDK startup/cleanup. The subsequent native
DLL experiment is described below; the original-header User::Exit experiment remains link evidence.
See IMPORTS.md and RESEARCH_LOG.md for scope, replay and hashes.

## 16. Frozen DLL tables and export-pointer relocation

The public [export-table producer](https://github.com/SymbianSource/oss.FCL.sf.os.buildtools/blob/7b35cd328d3a5e8e0bc177d0169fd409c3273193/toolsandutils/e32tools/elf2e32/source/e32exporttable.cpp)
prefixes the ordinal table with its count. Absent pointers use the entry address;
all slots, including absent ones, receive local code relocations. Bitmap padding
bits remain set. [Image generation](https://github.com/SymbianSource/oss.FCL.sf.os.buildtools/blob/7b35cd328d3a5e8e0bc177d0169fd409c3273193/toolsandutils/e32tools/elf2e32/source/e32imagefile.cpp)
includes the full variable header in CRC and appends the export table to code.
The [kernel loader](https://github.com/SymbianSource/oss.FCL.sf.os.kernelhwsrv/blob/0c3208650587ac0230aed8a74e9bddb5288023eb/userlibandfileserver/fileserver/sfile/sf_lepoc.cpp)
skips separate export adjustment for ELF images because those pointers must
already be covered by the code relocation section.

The earlier development fixture lacked those relocation records and the count
prefix. EKA2L1 resolves exports from its separately retained table, so import
execution alone did not prove mapped pointers were correct for that contract.
The original structural validator likewise does not establish all semantics.
The new native DLL converter resolves frozen named function symbols, emits
complete tables/bitmaps/text relocations and validates its canonical profile.
Maintained runtime tests also inspect the actual mapped table, including holes.
Nokia's original checksum and validator pass ordinals 7, 641 and 65,535 using
bounded test adapters that retain the complete variable header. This supports
the scoped no-resource emulator DLL experiment, not matched Belle or general
C++ startup. See IMPORTS.md and RESEARCH_LOG.md for replay and remaining limits.

## 17. Const tables and internal absolute-pointer fixups

A real Clang PIC callback table requires R_ARM_ABS32 plus `.data.rel.ro`.
Hidden internal table/class visibility changes its accesses from GOT_PREL to
local REL32; its linked pointer words still require a base adjustment. The
[Arm ELF specification](https://github.com/ARM-software/abi-aa/blob/main/aaelf32/aaelf32.rst)
defines the absolute relocation and instruction-state contract. The pinned
[Nokia local relocation implementation](https://github.com/SymbianSource/oss.FCL.sf.os.buildtools/blob/7b35cd328d3a5e8e0bc177d0169fd409c3273193/toolsandutils/e32tools/elf2e32/source/pl_elflocalrelocation.cpp)
selects text/data relocation kinds by the referenced segment. Its unresolved
ELF fixup must not be copied directly onto already resolved modern ET_EXEC words.

The converter now preserves bounded linked words and emits aligned text fixups,
including const-table entries inside named RELRO sections in its RX load.
It rejects unknown writable sections and unsupported external/GOT contracts.
Its inspector permits application and export fixups together, requires all
export slots, and forbids aliasing eager import slots. Real combined layout
cases pass the original checksum and structural validator.

The maintained C++ probe includes a const callback table with ARM/Thumb targets,
a pointer with a constant-text addend, and a single-inheritance virtual method.
Both emulator backends verify the actual four mapped pointer values, execute
all three indirect targets in their expected states, exit with correct results,
and reload after failure. Its SIS installs and runs in the original package
manager; a separately retained hashlib reference checks the registry hash.
The isolated installed wheel reproduces the artifacts and 21-case loop.
This adds ordinary C++ dispatch evidence without a hosted target runtime.
Full inheritance/RTTI/lifetime ABI, writable data/TLS, SDK initialization/cleanup
and matched Belle remain open. See POINTERS.md and RESEARCH_LOG.md.


## 18. C++20 language, modules and header-only library experiments

C++20 is feasible in the bounded native E32 path. A maintained C++20 probe
uses concepts/requires, a constrained generic lambda, a structural class
argument, designated initialization, consteval, constinit, defaulted equality,
char8_t and no_unique_address. Compiler negative controls fail for the expected
language/constraint/initialization reasons. Its optimized ELF/E32 matches the
already executed callback/virtual probe.

A separate named-module example uses upstream Clang and CMake's CXX_MODULES
scanning. It builds reproducibly, retains an actual BMI and rebuilds an unchanged
importer when its exported immediate function changes. Its 35-case image/package
loop passes on both emulator CPU backends. Exporting a constexpr object caused
an init_array and was correctly rejected; function-only modules do not establish
static lifetime support.

An isolated header configuration permits external libc++ bit/concepts/span
operations without a target library binary. A distinct callback executes
rotation and population count in the 21-case installer/loader loop. Unchanged
host configuration fails; coroutine/ranges/atomic expose missing target C-library
contracts in this experiment. LLVM lists no supported Symbian libc++ port.
Neither language support nor this header subset establishes a hosted standard
library, complete ABI, matched Belle or physical execution. See CXX20.md for
sources, compiler versions, replay and the remaining target runtime work.

## 19. Source SDK and Window Server GUI experiment

The maintained examples/gui_app uses original public Window Server types to
draw a four-digit counter and accept increment/reset/exit pointer events. Its
owned sdk.json pins 92 explicit header aliases and 37 function imports from
original EUSER/WS32 definitions. Native DEF selection and proxy generation
remain in the existing core; Python stages digest-checked aliases and build
policy. Source licenses stay in ignored upstream checkouts. Reported revisions
are manifest declarations; actual input bytes, rather than Git provenance,
are checked by preparation. This is not a matched Belle SDK distribution.

The primary-thread adapter derives R4/SP entry and heap/process initialization
from uc_exe.cia/uc_exe.cpp and ends through SDK User::Exit. No global lifetime,
secondary-thread or exception-entry support is claimed. A synchronous pair of
Window Server requests drives painting and input; cancellation completes
before request statuses leave the stack. This adds no host scheduler or Python
callback. The platform host bindings retain their A11 status/GIL policy.

Modern Clang/LLD produces identical ELF/E32 in independent build trees. Five
GTests cover the real model, extreme layout aspect ratios, half-open hit regions,
saturation and division against host arithmetic. Original historical checksum
and whole-image validator accept the GUI in eight cases. DWARF verifies, clangd
parses/indexes with a bounded tweak selection, and LLDB resolves symbols/source.
An installed wheel outside the source import path prepares both proxies,
reproduces the exact ELF/E32 and repeats all eight validation cases.

These checks do not execute SDK imports, boot Window Server, render a GUI or
attach a guest debugger. A matching ROM and same-device Z drive are still
missing. Imported-image SIS packaging, application registration, rotation and
full runtime support remain open. Root WALKTHROUGH.md contains creation details,
pinned source acquisition, verified build/check commands, and explicitly
unexecuted emulator/debugger procedures with acceptance criteria.

## 20. Imported GUI packaging and supplied RM-807 firmware

Single-executable SIS transport does not depend on the executable being
import-free. The native writer now preserves validated imported E32 payloads;
DLL payloads remain unsupported. Native inspection exposes the verified embedded
SHA-1, enabling an independent exact-input check before installer tests. The GUI
package adds no system DLL implementations or application registration.

Seventeen historical image/checksum/installer cases verify unchanged installed
bytes, complete registry metadata/SID/hash, reload, uninstall and reinstall.
The configured CPU backends execute no instructions in those package cases.
The absence-of-system-DLL case exposed an upstream defect: process creation
succeeds while all 37 import slots remain unresolved. Inspection of
buildup_import_fixup_table shows ignored failed fixups. This is now explicitly
observed and reported, not treated as launch success; a loader fix needs separate
rollback/cycle and compatibility tests. No upstream algorithm was changed here.

The owner supplied Nokia 808 PureView (Delight v1.8).zip. Its seven files include
core/ROFS2/ROFS3/UDA FPSX and a VPL declaring RM-807, product 059M7Q4, version
113.010.1508. Required files and all supplied CRC values agree. The actual
EKA2L1 firmware importer accepts it, producing a ROM, Z filesystem and isolated
writable drives, identified as Nokia/808 PureView/RM-807/epoc100. This establishes
an emulator research candidate; archive authenticity, physical-phone match and
factory recovery suitability are unverified.

A copied private instance loads gui_app.exe and real EUSER/WS32 at actual
runtime addresses. Logs expose unimplemented SVCs 0x51/0xF7 and a $HEAP lookup
failure. Visual behavior, guest debugging and correct SDK startup remain open.
The imported baseline is retained unbooted and inventoried separately from
mutable runtime state; all firmware and derived data stay ignored. The native
emulator-only importer refuses an existing output root and exposes no hardware
transport. WALKTHROUGH.md documents the concrete material and replay.

## 21. Live guest debugging and the Belle executive ABI boundary

ARM GDB 17.2 connects to the isolated Dynarmic frontend using the supplied
RM-807 ROM/Z. A relocated source breakpoint reaches GuiRunThread with reason
zero and the expected stack-provided thread-create information. Actual guest
register/memory reads and ROM instruction breakpoints are available before a
GUI can draw. Full stack unwinding is not established: the raw startup frame
lacks a proven unwind contract.

The pinned system loop leaves the GDB step flag set after a CPU step, causing
silent subsequent execution. The small GPL guest-debug-step.patch saves the
stepped context, clears that flag and sends one stop response while halted.
An opt-in Pytest drives the real frontend and ARM GDB; it checks two successive
Thumb instructions, a fresh register read after a delay, source substitution,
ROM SVC stops and the heap result. This changes emulator debugging control,
not target SDK code or the system-call map.

With stable stops, SetupThreadHeap returns KErrNotFound (-1), before GuiMain.
The real ROM calls SVC 0x51 with (0,7,&size,0), matching
[UserHal::PageSizeInBytes](https://github.com/SymbianSource/oss.FCL.sf.os.kernelhwsrv/blob/0c3208650587ac0230aed8a74e9bddb5288023eb/kernel/eka/euser/us_exec.cpp)
and the original kernel HAL enum. It then calls 0x6D with owner=1, the $HEAP
descriptor and a chunk-create structure. The
[pinned epoc10 SVC table](https://github.com/EKA2L1/EKA2L1/blob/2594edf4d6bf55d7bd3f0b46250fe2318d4dc2e8/src/emu/kernel/src/svc.cpp)
instead places HAL at 0x4F and chunk creation at 0x6B; 0x6D dispatches object
lookup and returns not-found. User::Exit(-1) subsequently reaches SVC 0xF7,
while the table registers its thread-exiting handler at 0xF6.

The HAL/chunk semantic mapping combines runtime arguments with original source;
the exit mapping is an inference from its caller and original cleanup code.
This is evidence of firmware executive ABI incompatibility, not a complete
Belle FP2 profile. Whole-table shifting would be unjustified. Next work needs
more exported-wrapper/call-site mappings, firmware-specific selection and
controls retaining older firmware compatibility. No synthetic SDK replacement
or permissive import workaround was added. Visual GUI execution, normal exit,
OS boot and physical-phone compatibility remain unverified.

## 22. Guarded Symbian 101 routing and initial GUI drawing calls

Native inspection of the real EUSER export table and source-wrapper comparison
now provides 212 old-call mappings with no conflicting constraints, spanning
both executive shift boundaries. This supports a separate piecewise experimental
map, rather than a uniform shift. The original epoc10 map is retained. Selection
requires an explicit profile name and exact full-ROM digest; actual frontend
tests reject an unknown name and a modified private ROM. Private operations,
new loader extensions and the existing SVC 0xFF interception remain open.

The corrected routing completes heap setup and reaches GuiMain. Real instruction
stepping identifies a Dynarmic abort on TPIDRURO, an independent ARM thread
register. A native patch adds its read and context preservation on both tested
macOS backends. Unequal-register and context-switch GTests reject TLS aliasing.
The subsequent guest panic E32USER-CBase/69 identifies a missing trap handler;
the example now installs the original SDK cleanup stack before calling GuiMain.

Live ARM GDB observes RWsSession::Connect returning zero, the initial DrawGui
entry with count zero and a 360 by 640 layout, and return from that function.
This is actual guest SDK execution, not a framebuffer or pointer-input proof.
Default-profile heap failure remains a control. Full DLL initialization, normal
cleanup/exit, rendered pixels and physical compatibility remain unverified.
The scoped evidence, source contracts, patch recipe and replay are in
[BELLE_ABI.md](BELLE_ABI.md); private code/data/logs stay outside Git.


## 23. Real texture/input/exit evidence and implementation review

A thin native GPL adapter adds an opt-in private Unix endpoint to the existing
Qt frontend. It uses the original graphics driver to read the screen texture,
Window Server's existing pointer path and real kernel process-exit callbacks.
No target model mutation, fake SDK drawing, host desktop capture or new native
scheduler is involved. Scalar/string exit records avoid lifetime issues with
process pointers. The synchronous Python client supplies policy and transport;
format/image work remains native.

Both Dynarmic and Dyncom produce real portrait counter frames and pass the
0→1→2→outside unchanged→reset→exit sequence. Normal SDK exit reaches User::Exit,
thread_user_exiting and ThreadKill with reason zero. A final kernel report and
frontend exit zero survive normal teardown. The initial render-only experiment
exposed a recursive kernel lock on pointer delivery; releasing the validation
lock before the existing delivery path fixes it. The first exit experiment
exposed a callback destructor after kernel destruction; detaching before worker
shutdown and persisting the final report fixes it. These failures remain in the
private evidence rather than being reclassified as successful runs.

CLion's missing target context comes from opening the host CMake project for a
guest source. The GUI's separate ARM preset now includes the two staged proxies
needed for standalone configuration, and configure/build plus clangd parsing
pass. CLI conversion still publishes E32 separately from the IDE's ELF build.
CLion UI/debugger integration itself remains a manual validation task. C++20
language/library evidence and firmware execution remain separate claims.

The review updates PLAN.md to consolidate workflows before expanding scope:
owned disposable emulator lifecycle/test artifacts, unresolved executive/DLL
contracts, bounded target runtime support, useful diagnostics, then broader
application/system work. Current UI success does not establish a full OS boot,
complete DLL initialization, TLS/static lifetime, hosted C++20, complete unwind
support or phone compatibility. See [EMULATOR_CONTROL.md](EMULATOR_CONTROL.md)
and [CLION.md](CLION.md) for the concrete replay and developer setup.
