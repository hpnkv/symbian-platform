# Status

The initial technical survey is in RESEARCH.md. The owner has one Nokia 808;
exact identity, firmware, ROM/Z-drive assets and recovery method are unknown.

| PLAN milestone | Status | Required evidence |
| --- | --- | --- |
| 0 Preservation | Archive tools tested; physical baseline pending | Device inventory, original artifacts, offline archive, tested human recovery appliance |
| 1 Toolchain | Reproducible Clang/LLD ELF→E32 experiment; independent parser/checksum checks pass | Complete Belle loader validation and target runtime ABI tests |
| 2 Project model | Pending | Verified SDK subset, CMake application and installed SIS |
| 3 Emulator | Native arm64 build, upstream suite and CLI instance smoke tests pass | Matched ROM/Z image, guest boot/launch, complete runtime isolation |
| 4–9 | Pending | Gates in PLAN.md |

No physical-device executor, flashing capability or recovery automation exists.
Source inspection is not an emulator runtime test. Record implementation and
verification results in RESEARCH_LOG.md before changing milestone status.

Current working commands: `doctor`, `toolchain probe`, experimental `build`,
ELF/E32 `inspect`, `preserve create/verify`, and informational `device policy`.
The e32_probe example has no SDK/imports/data/constructors; its direct thread
exit is a no-resource experiment. Both the linked ELF and converted E32 repeat
byte-for-byte in two builds on this host. Parser acceptance does not prove
Symbian loader compatibility. Reports retain loader/runtime verification false.
Artifacts and reports live under .symbian/. clangd consumed the generated ARM
compilation database with zero errors. Native core code is compiled with
exceptions disabled; the pybind11 boundary retains canonical status codes.

Verification: 51 Pytest cases with the patched emulator supplied, 16 platform
GTest cases, five independent oracle GTest cases, 288 upstream EKA2L1 cases,
Black/Ruff, clang-format, generated stubs and clangd passed on Apple Silicon.
Without the optional emulator path, four smoke cases skip. EKA2L1 accepts our
image but omits header CRC verification; Nokia's separate original checksum
oracle validates the CRC. No Symbian guest executable or physical runtime ran.
Build and patch replay instructions are in research/eka2l1/README.md.
