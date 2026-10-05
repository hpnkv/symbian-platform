# How the GUI example is built

The example uses the public `w32std.h` interfaces directly. A raw Window Server
client is sufficient to create a window, draw rectangles, and receive pointer
and redraw events. This exposes the GUI service contract without first needing
Avkon, application registration resources, fonts, Qt, or a resource compiler.
Direct emulator launches use its executable path. Packaging now adds a
separate application-registration resource and caption resource so an
installed package can appear in the application menu; this does not introduce
an Avkon application lifecycle into the example.

The project is divided as follows:

| File | Responsibility |
| --- | --- |
| `model.h` | Pure counter, geometry, pointer hit testing and integer division |
| `app.cc` | C++20 launcher using shared ownership and `std::thread` for the event loop |
| `window_server.cc` | Original SDK types, Window Server connection, drawing and request loop |
| `CMakeLists.txt`, `CMakePresets.json` | Application sources, library dependencies and build preset |
| `symbian.toml` | Development UID `0xe0000811` and package identity |

The SDK supplies startup, linker layout, platform headers, complete OS import
libraries and debug path mappings. Applications select libraries with
`target_link_libraries`; the SDK derives E32 imports from the link graph.

The model is ordinary C++ with no SDK or host library dependency. It caps the
counter at 9999, treats hit regions as half-open rectangles, and ignores input
after exit. The host tests compile this same header, rather than implementing a
second model in Python. The integer division routine requires a nonzero divisor;
every caller
supplies a positive constant. It is a small example utility, not a replacement
for the SDK's compiler-rt helpers.

`GuiWindowServerMain` explicitly constructs and closes an `RWsSession`, `CWsScreenDevice`,
`CWindowGc`, `RWindowGroup`, and `RWindow`. Group/client handles are 1 and 2.
The initial screen size determines the layout; supported dimensions are
120..8192 by 160..8192. Rotation and resizing after launch are not handled yet.
Digits and control marks are filled rectangles, so no font selection is needed.
The controls show increment, reset and exit marks. The painting code keeps
its inputs on the stack.

The event thread owns Window Server input/redraw statuses and timer Tasks.
It processes ready work before the shared request-semaphore wait, then reposts
native requests. Button-down events for window handle 2 update the model and
invalidate the window; redraw brackets painting with `BeginRedraw`/`EndRedraw`.
Each increment schedules a 300-ms marker, and Reset cancels pending marker work.
On exit, cancellation drains before statuses disappear and GUI objects close
before the session.

The SDK-owned process entry checks the thread-create layout and calls
`UserHeap::SetupThreadHeap`. For the primary thread it initializes the process,
runs global initializers, creates `CTrapCleanup`, calls standard `main`, runs finalizers
and exits through `User::Exit`. Secondary threads enter their callback on the
SDK-created thread heap. Failure to allocate cleanup state returns
`KErrNoMemory`; unexpected entry reasons call `User::Invariant`.

The reference is the original
[ARM executable startup](https://github.com/SymbianSource/oss.FCL.sf.os.kernelhwsrv/blob/0c3208650587ac0230aed8a74e9bddb5288023eb/kernel/eka/euser/epoc/arm/uc_exe.cpp)
and its accompanying `uc_exe.cia`, together with the original
[Window Server header](https://github.com/SymbianSource/oss.FCL.sf.os.graphics/blob/ff133bc50e6158bfb08cc093b0f0055321dcde99/windowing/windowserver/inc/W32STD.H).
