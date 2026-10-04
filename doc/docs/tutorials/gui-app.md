# Native Window Server GUI application

Symbian's Window Server is the system service that owns visible windows and
delivers redraw and pointer events to applications. This example is a small
touch counter so you can follow the complete path from a C++ event handler to
an E32 app and a rendered emulator window. EKA2L1 supplies the emulator; the
firmware files used for the maintained checks are provided separately.

Build a touch counter with original public Symbian headers, modern Clang/LLD,
the installed SDK's `Symbian::Stackless` runtime and the native E32 converter.
Each increment schedules a 300-ms timer Future that lights a small marker;
Reset cancels pending work. `app.cc` visibly uses `std::make_shared` and
`std::thread` to own the Window Server event thread and carry its exit result
back to process startup. `window_server.cc` keeps original Symbian headers out
of that modern C++ translation unit. The event thread owns its own cleanup
stack, Window Server session and timer request completion. The selected ROM needs
`libpthread.dll`. Select an installed SDK through the active global SDK,
`SYMBIAN_SDK_MANIFEST`, or an ignored local `sdk-location.json` containing
`{"sdk": "/path/to/sdk"}`. See the complete root
[source walkthrough](../guides/from-source.md) for pinned source acquisition, SDK
preparation, build/test commands, emulator launch, editor setup and debugging.

The ELF/E32 build, model tests, image validation, SIS installation/registry
operations and offline debug symbols are tested. The supplied Delight RM-807
bundle imports into EKA2L1. With the guarded experimental executive profile and
CPU correction, live ARM GDB verifies heap setup, Window Server connection and
completion of the initial drawing function. SDK cleanup-stack initialization is
provided. Both backends now pass rendered PNG, increment/reset/outside pointer
and normal zero guest/frontend exit checks; see
[the control replay](../guides/emulator-control.md). CLion should load this
directory's `symbian-pic` profile; see [CLion setup](../guides/clion.md).
See [the ABI experiment](https://github.com/hpnkv/symbian-platform/blob/main/.dev/belle-abi.md) for scope and replay. The example has a single-EXE SIS package
and no application registration.
