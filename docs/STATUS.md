# Status

The initial technical survey is in RESEARCH.md. The owner has one Nokia 808;
exact identity, firmware, ROM/Z-drive assets and recovery method are unknown.

| PLAN milestone | Status | Required evidence |
| --- | --- | --- |
| 0 Preservation | Archive tools tested; physical baseline pending | Device inventory, original artifacts, offline archive, tested human recovery appliance |
| 1 Toolchain | Reproducible ELF→E32; historical validation, CPU and ROMless emulator process tests pass | Matched Belle runtime, imports and complete target ABI tests |
| 2 Project model | Experimental CMake/Ninja project path, persistent database and wheel replay pass | Verified SDK subset, general application support and installed SIS |
| 3 Emulator | Native arm64 build, upstream suite and CLI instance smoke tests pass | Matched ROM/Z image, guest boot/launch, complete runtime isolation |
| 4–9 | Pending | Gates in PLAN.md |

No physical-device executor, flashing capability or recovery automation exists.
Source inspection is not an emulator runtime test. Record implementation and
verification results in RESEARCH_LOG.md before changing milestone status.

Current working commands: `doctor`, `toolchain probe`, `toolchain verify-probe`,
experimental `build`, ELF/E32 `inspect`, `preserve create/verify`, and
informational `device policy`.
The e32_probe example has no SDK/imports/data/constructors; its direct thread
exit is a no-resource experiment. Both the linked ELF and converted E32 repeat
byte-for-byte in two builds on this host. Parser acceptance does not prove
Symbian loader compatibility. Reports retain loader/runtime verification false.
Artifacts and reports live under .symbian/. clangd consumed the generated ARM
compilation database with zero errors. Native core code is compiled with
exceptions disabled; the pybind11 boundary retains canonical status codes.

Verification: 62 Pytest cases with the patched emulator and native oracles
supplied, 16 platform GTest cases, 28 independent oracle GTest cases and
288 upstream EKA2L1 cases pass on Apple Silicon. Black/Ruff, clang-format and
generated stubs pass; the earlier clangd target check had zero errors.
Without optional dependency paths, four emulator smoke cases and one native
verification integration case skip. EKA2L1's parser omits header CRC verification;
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
