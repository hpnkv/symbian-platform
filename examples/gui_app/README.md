# Native Window Server GUI experiment

Build a touch counter with original public Symbian headers, modern Clang/LLD,
and the native E32 converter. See the complete root
[WALKTHROUGH.md](../../WALKTHROUGH.md) for pinned source acquisition, SDK
preparation, build/test commands, emulator launch, editor setup and debugging.

The ELF/E32 build, model tests, image validation, SIS installation/registry
operations and offline debug symbols are tested. The supplied Delight RM-807
bundle imports into EKA2L1. With the guarded experimental executive profile and
CPU correction, live ARM GDB verifies heap setup, Window Server connection and
completion of the initial drawing function. SDK cleanup-stack initialization is
provided. Rendered pixels, pointer delivery and normal exit remain unverified.
See [the ABI experiment](../../docs/BELLE_ABI.md) for scope and replay. The example has a single-EXE SIS package
and no application registration.
