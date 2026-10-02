# Native Window Server GUI experiment

Build a touch counter with original public Symbian headers, modern Clang/LLD,
the installed SDK's `Symbian::Stackless` runtime and the native E32 converter.
Each increment schedules a 300-ms timer Future that lights a small marker;
Reset cancels pending work. A narrow `async_bridge.cc` keeps the original
Window Server headers out of modern C++ code, while one event-thread loop
owns Window Server and timer request completion. The selected ROM needs
`libpthread.dll`. Select an installed SDK through the active global SDK,
`SYMBIAN_SDK_MANIFEST`, or an ignored local `sdk-location.json` containing
`{"sdk": "/path/to/sdk"}`. See the complete root
[WALKTHROUGH.md](../../WALKTHROUGH.md) for pinned source acquisition, SDK
preparation, build/test commands, emulator launch, editor setup and debugging.

The ELF/E32 build, model tests, image validation, SIS installation/registry
operations and offline debug symbols are tested. The supplied Delight RM-807
bundle imports into EKA2L1. With the guarded experimental executive profile and
CPU correction, live ARM GDB verifies heap setup, Window Server connection and
completion of the initial drawing function. SDK cleanup-stack initialization is
provided. Both backends now pass rendered PNG, increment/reset/outside pointer
and normal zero guest/frontend exit checks; see
[the control replay](../../docs/EMULATOR_CONTROL.md). CLion should load this
directory's `symbian-pic` profile; see [CLion setup](../../docs/CLION.md).
See [the ABI experiment](../../docs/BELLE_ABI.md) for scope and replay. The example has a single-EXE SIS package
and no application registration.
