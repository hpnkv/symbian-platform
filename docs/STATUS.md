# Status

The initial technical survey is in RESEARCH.md. The owner has one Nokia 808;
exact physical identity, installed firmware and recovery method remain unknown.
The supplied Delight RM-807 archive provides preserved emulator ROM/Z material;
a guarded disposable experiment now completes the initial GUI drawing function.

| PLAN milestone | Status | Required evidence |
| --- | --- | --- |
| 0 Preservation | Archive tools tested; physical baseline pending | Device inventory, original artifacts, offline archive, tested human recovery appliance |
| 1 Toolchain | Reproducible ELF→E32; historical validation and ROMless process/DLL import tests pass | Matched Belle runtime, SDK imports and complete target ABI tests |
| 2 Project model | CMake/Ninja, persistent database, native SIS and ROMless install/launch pass | Matched SDK/DLL imports, general application support and Belle installer |
| 3 Emulator | Native arm64 build, supplied ROM/Z import, guarded SDK startup/drawing calls and disposable tests pass | Rendered output/input, normal exit, OS boot and general runtime coverage |
| 4 Automated development | Native build/package/check loops pass; GUI execution is an opt-in research test | General unattended application execution, pixels/input and facade orchestration |
| 5 Modern debugging | Live ARM GDB source/ROM stops, stable stepping and model inspection pass | Full unwinding, crash/thread inspection and normal exit |
| 6–9 | Pending | Physical deployment/system/hardware/alternative OS gates in PLAN.md |

No physical-device executor, flashing capability or recovery automation exists.
Source inspection is not an emulator runtime test. Record implementation and
verification results in RESEARCH_LOG.md before changing milestone status.

Current working commands: `doctor`, `toolchain probe`, `toolchain verify-probe`, `toolchain verify-pointers`,
`toolchain verify-package`, `toolchain import-proxy`, experimental
`build`/`package`, ELF/E32/SIS/import-proxy `inspect`, `preserve create/verify`,
and informational `device policy`.
The e32_probe example has no SDK/imports/data/constructors; its direct thread
exit is a no-resource experiment. Both the linked ELF and converted E32 repeat
byte-for-byte in two builds on this host. Parser acceptance does not prove
Symbian loader compatibility. Reports retain loader/runtime verification false.
Artifacts and reports live under .symbian/. clangd consumed the generated ARM
compilation database with zero errors. Native core code is compiled with
exceptions disabled; the pybind11 boundary retains canonical status codes.

Verification: 112 Pytest cases pass with the patched emulator, native oracles
and public kernel source supplied. The 41 platform GTest cases and 48 independent
oracle GTest cases pass on Apple Silicon. The preceding emulator checkpoint
passed 288 upstream EKA2L1 cases; its sources are unchanged here. Black/Ruff,
clang-format and generated stubs pass; clangd target checks have zero errors.
Without optional dependency paths, four emulator smoke cases and two native
verification integration cases and one public-header research case skip.
One additional development DLL runtime case and three variable-header DLL
validation cases need the native oracles. Two additional pointer/combined-layout
cases need them.
EKA2L1's parser omits header CRC verification;
Nokia's original checksum and whole-image validator independently check it.
The unchanged historical validator accepts this image using host type adapters.
Both EKA2L1 CPU backends execute its ARM startup and Thumb C++ at two load
addresses; a changed input produces the expected failure exit. These are
ROMless CPU tests: the exit SVC is observed by their callback. Eight additional
cases use EKA2L1's real process loader, flexible memory model, scheduler and
kernel SVC dispatch under its epoc10 profile. Both backends return zero normally
and 42 for changed input; absent executables fail to create a process. Both
backends also launch another process after an earlier failure exit.
The process/thread exit states and address-space release are checked. This is
an import-free emulator process without Belle ROM/Z or system services.
Reports distinguish eka2l1_process_verified from Belle loader/runtime flags,
which remain false. No physical runtime ran.
Build and patch replay instructions are in research/eka2l1/README.md.

The CMake path preserves the baseline ELF/E32 bytes. Multi-source builds, header
changes, paths with spaces and cached no-op builds are tested. Its compilation
database points to existing build objects; clangd reports zero errors. The wheel
contains the CMake modules and builds the same E32 from an isolated installation.
Project instructions and the v2 build report are described in docs/BUILDING.md.

The unsigned native SISX package is reproducible and passes independent Nokia
UID/controller/data CRC checks. Six disposable EKA2L1 cases install the unchanged
probe, launch through the kernel on both CPU backends, reload its registry,
uninstall and reinstall. The registry's file hash agrees with an independent
hashlib baseline. That installer does not enforce phone signing/capability policy.
The production native inspector verifies checksums, SHA-1, E32 and the restricted
canonical profile before the trusted experiment. Reports retain Belle runtime
and phone installation flags false. See PACKAGING.md for profile limits and replay.
OpenSSL 3 is statically linked for SHA-1; its license is packaged with the wheel.

The native SDK component reads frozen EABI definitions and generates selected
function ordinal proxies using Clang/LLD. The public User::Exit slot remains 641,
and Nokia's unchanged ordinal lookup method agrees in a separate optional test.
An original e32std.h call compiles and links, typed layout assertions pass, and
clangd reports zero errors. The public request status is eight bytes with flags.
Proxy and link ELF builds repeat; SDK/source/header dependencies are recorded.
An isolated wheel installation includes the target probe resource and builds
the same proxy and linked ELF.
These are link contracts, not a verified 808 SDK. See SDK.md for replay.

The E32 import profile places function GOT slots in its code region and resolves
versioned symbols against original proxy ordinals. A compiled development DLL at
ordinal 7 executes on both EKA2L1 CPU backends; the patched slot, function PC,
changed-input failure and repeated launch are checked in six cases. Historical
validation/checksums accept both files. Two-DLL links and malformed controls pass.
An isolated installed wheel reproduces the same imported ELF/E32 and metadata.
General writable-data DLL support, matched SDK services, startup/cleanup and
Belle runtime remain open. See IMPORTS.md.


Native DLL conversion now resolves frozen function exports from retained ELF
symbols, preserves ordinal gaps/ABSENT entries, and emits the count prefix,
full absence bitmap and code relocations for all export pointers. No fixed
function address is required. Independent validation covers ordinals 7, 641 and
65,535, including complete variable headers and relocation pages. Both emulator
backends also verify the mapped table: present pointers agree with lookup,
absent pointers relocate to the entry, and the count word remains unchanged.
Combined import/export layout passes Nokia's checksum and whole-image validator;
its executable startup is not runnable DLL initialization evidence.

The earlier research DLL omitted the count prefix and export-pointer relocations.
Its successful structural validation and export lookup proved less than the
public ELF loader contract. It remains research material; maintained runtime
checks now use examples/dll_probe and the native converter. The installed wheel
reproduces the native DLL and ELF and their metadata. Evidence is in
.symbian/native-dll/report.json, verification-report.json and wheel-result.json.
General pointer relocations, writable data/BSS/TLS, constructors, SDK startup,
matched target DLLs and Belle runtime remain open. No device operation ran.


Retained internal ABS32 words now generate E32 text relocations, permitting
named RELRO tables within the RX mapping. A maintained multi-source C++ probe
executes an ARM callback, a Thumb callback and a virtual method, and reads a
constant-data pointer with an addend. Both emulator backends verify all four
mapped words and instruction state at the three indirect targets, changed-input
failure, repeated launch and address-space release. Its native SIS installs,
launches, reloads the registry, uninstalls and reinstalls in separate cases.
The new verify-pointers command retains 14 image/dispatch checks or 21 with the
package. An isolated wheel reproduces ELF/E32/SIS and repeats all 21 checks.
Evidence is in .symbian/pointer-probe, .symbian/pointer-package and
.symbian/pointer-check. See POINTERS.md for replay and the trusted-link contract.

Internal pointer relocations also coexist with eager imports and frozen exports
in independently validated layout cases. External absolute pointers, GOT_PREL,
writable data/BSS/TLS and global lifetime support remain open. Simple virtual
dispatch does not prove the full target C++ ABI. Matched Belle and physical
execution are still unverified; no device operation ran.


C++20 now has maintained language and named-module examples. Concepts/requires,
structural class arguments, consteval/constinit, designated initialization,
constrained lambdas, equality and small layout contracts compile with explicit
controls. The language probe passes 21 independent loader/installer cases. A
separately configured libc++ bit/concepts/span experiment produces distinct
machine code and passes the same loop without linking a hosted runtime.
The module example uses upstream Clang/CMake scanning, retains its BMI, checks
importer invalidation and passes the 35-case image/package loop. An isolated
installed wheel reproduces all three ELF/E32/SIS variants and all 77 checks.

All 124 Pytest cases pass with explicit optional compiler/header/oracle inputs;
the native platform and SDK ordinal checks pass. Evidence is under
.symbian/cxx20-probe, cxx20-check, cxx20-library, cxx20-library-check,
cxx20-module and cxx20-module-check. See CXX20.md. Complete standard library,
coroutine/thread/atomic runtime, global lifetime, writable data/BSS/TLS, SDK
services and matched Belle remain open. No physical-device operation ran.


The requested examples/gui_app and root WALKTHROUGH.md are now present. The
counter application directly uses Window Server; it has seven-segment drawing,
touch increment/reset/exit and explicit request cancellation/object cleanup.
Its primary-thread adapter attempts SDK heap/process setup and User::Exit,
while secondary-thread, exception-entry and global lifetime support remain
absent. The build report now correctly treats startup/cleanup as unverified
project behavior instead of assuming every executable uses direct ThreadKill.

The source profile stages 92 original header aliases and native frozen proxies
for nine EUSER and 28 WS32 functions from pinned ignored public trees. Preparation
checks file digests, preserves original files/licenses, rejects malformed
profiles and output redirection, and records SDK/runtime verification false.
Clang/LLD and independent CMake trees reproduce the ARM ELF and E32. Five model
GTests pass, including wide/tall layouts, arithmetic, input bounds and saturation.
Original checksum/validator sources accept the generated GUI in eight cases.
DWARF verifies and LLDB resolves functions and source lines; clangd has no
diagnostics with its limited check-mode refactoring selection.

All 139 Pytest cases passed with explicit optional inputs; all six root CTest
targets passed. The final drawing-layout adjustment also passed the five model
GTests and all 15 GUI Pytest cases. An isolated installed wheel runs the SDK
preparation/build/verification CLI, reproduces both artifacts and repeats all
eight independent image checks. Evidence lives in .symbian/gui-sdk,
.symbian/gui-app, .symbian/gui-validation and .symbian/gui-research.

Visible GUI execution, actual Belle EUSER/WS32 compatibility, heap/cleanup and
guest debugger attachment remain unverified because matched ROM/Z is absent.
The walkthrough labels these procedures as future experiments. The example
has no application registration or SIS package; the existing SIS writer's
import-free restriction remains. No emulator OS boot or physical-device action
is claimed or performed. Phone RM/product/firmware details remain unknown.


The GUI now has a native single-EXE unsigned SIS package, independently validated
in 17 image/checksum/installer cases. Actual installer/registry behavior verifies
exact payload bytes, UID/SID/version and an independent legacy digest, reload,
uninstall and reinstall in disposable ROMless filesystems. A control records
the unchanged upstream loader's missing-library defect: it creates a process
with all 37 imported words still unresolved. No guest instructions execute in
these installer cases. Reports explicitly keep GUI/SDK execution and matched
loader/runtime verification false. The native writer now accepts imported EXEs
while still rejecting DLL payloads, resources and broader package profiles.
All 142 Pytest cases and six root CTest targets pass; the installed wheel
reproduces ELF/E32/SIS and repeats the 17-case package check.

The supplied Delight v1.8 ZIP was checked and staged privately. Its VPL declares
RM-807, product 059M7Q4 and version 113.010.1508; seven required/present files
pass archive and declared CRC checks. One opt-in native GTest successfully uses
EKA2L1's VPL/FPSX/ROM/ROFS/FAT importer into a new isolated root. The result
identifies Nokia/808 PureView/RM-807/epoc100 and supplies a ROM and actual system
DLLs, with 13,438 imported files inventoried by SHA-256. This corrects the prior
missing-assets state. Custom archive authenticity, stock recovery baseline and
matching this physical phone are not established.

A copied instance maps the GUI at 0x70000000, EUSER at 0x804bcce8 and WS32 at
0x80a4c028. It logs unimplemented SVCs 0x51/0xF7 and a $HEAP lookup failure;
visual behavior, correct startup/cleanup and debugger attachment are not yet
verified. TERM did not finish the private emulator, so its confirmed process
was stopped with KILL after retaining logs. The original imported root remains
separate from runtime state. No physical phone operation ran. Evidence is in
.symbian/gui-package[-check], gui-research/delight-archive-check.json,
delight-import.json, delight-import-inventory.json and the private instance logs.

Live ARM GDB 17.2 attachment now works against a disposable RM-807 instance.
The actual startup source breakpoint receives reason=0 and info=0x40ffc0 at
PC=0x700009da. A local GPL guest-debug-step.patch fixes silent execution after
single stepping. A real frontend/GDB Pytest proves two successive Thumb stops,
a stable fresh register read after a delay, source display and ROM SVC stops.
The same test fails with a 30-second GDB timeout when that patch is removed;
restoring it passes. All 143 Pytest cases with explicit optional inputs and all
six root CTest targets pass. This is live debugging evidence, not GUI success.

Stable registers show heap initialization returns KErrNotFound (-1), before
GuiMain. The ROM requests kernel HAL page size using SVC 0x51 and chunk creation
using 0x6D; the pinned epoc10 table maps those operations to 0x4F and 0x6B and
dispatches 0x6D as object lookup. The real exit path reaches unimplemented
0xF7, while its handler is registered at 0xF6. These firmware executive ABI
discrepancies require a fuller independently checked Belle profile. No whole
table shift, SDK replacement, or loader workaround was introduced.

Visible GUI output/input, successful heap setup, normal SDK exit, stack unwinding
and OS boot remain unverified. The test-owned frontend still requires KILL
after TERM; reaching an exit wrapper does not establish guest cleanup. The
original ZIP and all 13,438 baseline file digests are rechecked unchanged.
Physical phone details remain unknown and no device operation ran. Replays and
current limitations are in WALKTHROUGH.md section 9; transcripts, negative
control, test logs and integrity evidence are in .symbian/gui-research/debugger*.


### 2026-09-30 — Guarded RM-807 ABI and initial drawing-function execution

The real ROM export probe and source-wrapper comparison now support a piecewise
experimental Symbian 101 executive map. It has 170 existing handlers, while the
original 172-handler epoc10 map remains intact. Profile selection is explicit
and exact-ROM-digest guarded; real frontend tests reject unknown profile names
and changed private ROM bytes. This is a research profile for the supplied
Delight image, not universal Belle support or authenticated stock firmware.

With that profile heap setup returns zero and GuiMain executes. A separate ARM
TPIDRURO register fixes the original Dynarmic coprocessor abort, with four real
instruction/context cases passing on both tested macOS backends. Window Server
connection returns zero. A subsequent E32USER-CBase/69 panic exposed missing
cleanup-stack setup; the example now creates the SDK CTrapCleanup before GuiMain
and deletes it on return. Its frozen EUSER import count is now ten (38 total).

Live GDB verifies the initial DrawGui entry, zero/running model, 360 by 640 layout
and return after its guest drawing calls. The default map still fails heap
startup, preserving a useful control. Full DLL initialization remains unproven:
0x10D is still unimplemented. Rendered pixels, pointer delivery, normal cleanup/
exit, full unwinding, OS boot and physical-phone compatibility remain unverified.
No screenshot or visible-GUI success is claimed from a drawing-function stop.

All 146 Pytest cases pass with explicit optional inputs, all six root CTest
targets pass, and the two new research CTest targets pass seven GTest cases.
The opt-in ROM export probe passes separately, rejects existing/nested output,
and executes no guest instructions. All six patches apply in documented order
against fresh pinned source files. An isolated installed wheel reproduces the
updated ELF/E32, packages it and passes all 17 historical/installer checks;
research tests/firmware remain excluded. Black/Ruff and C++ formatting pass.

The original ZIP and every path, size and SHA-256 of the 13,438-file unbooted
baseline are rechecked unchanged. Runtime work uses copied instances; the
baseline is not booted. Phone identity and independent offline preservation
remain unknown. No device operation ran. Replays and boundaries are recorded in
WALKTHROUGH.md and docs/BELLE_ABI.md; private evidence is under
.symbian/belle-abi-research. The platform mission remains active.
