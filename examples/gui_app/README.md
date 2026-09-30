# Native Window Server GUI experiment

Build a touch counter with original public Symbian headers, modern Clang/LLD,
and the native E32 converter. See the complete root
[WALKTHROUGH.md](../../WALKTHROUGH.md) for pinned source acquisition, SDK
preparation, build/test commands, emulator launch, editor setup and debugging.

The ELF/E32 build, model tests, image validation, SIS installation/registry
operations and offline debug symbols are tested. The supplied Delight RM-807
bundle imports into EKA2L1 and a launch maps real system DLLs; startup logs expose
unimplemented services and a heap lookup failure. Visible GUI behavior and
guest debugging remain unverified. The example has a single-EXE SIS package
and no application registration.
