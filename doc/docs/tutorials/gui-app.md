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

Build with the project's `symbian-pic` profile and use
[emulator controls](../guides/emulator-control.md) to capture the screen and
send taps. For IDE Run and guest breakpoints, follow
[CLion setup](../guides/clion.md). The SIS includes application registration,
menu captions and an icon; it does not supply an Avkon application lifecycle.
