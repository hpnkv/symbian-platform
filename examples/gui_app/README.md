# Native Window Server GUI experiment

Build a touch counter with original public Symbian headers, modern Clang/LLD,
and the native E32 converter. See the complete root
[WALKTHROUGH.md](../../WALKTHROUGH.md) for pinned source acquisition, SDK
preparation, build/test commands, emulator launch, editor setup and debugging.

The ELF/E32 build, host model tests, historical validation and offline debug
symbols are tested. Visible GUI execution and guest debugging still require
compatible real ROM/Z and system DLLs and remain unverified. This example has
no SIS package or application registration.
