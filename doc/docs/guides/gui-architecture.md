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
| `startup.S` | ARM entry marker and ARM-to-Thumb entry transition |
| `startup.cc` | Checked thread-create layout, SDK heap setup, process initialization and `User::Exit` |
| `image.ld` | Retained-relocation ELF transport with one code mapping and eager import tables |
| `CMakeLists.txt`, `CMakePresets.json` | Target compilation, SDK definitions, debug flags and source path maps |
| `symbian.toml` | E32 import profile, development UID `0xe0000811`, two explicit ordinal proxies |

SDK source preparation uses `research/gui_app/source-profile.json`; its input
digests are build data and are outside the application project.

The model is ordinary C++ with no SDK or host library dependency. It caps the
counter at 9999, treats hit regions as half-open rectangles, and ignores input
after exit. The host tests compile this same header, rather than implementing a
second model in Python. The model's division routine is tested against host
integer `/` and `%` at boundary values and 8,000 mixed inputs. It requires a
nonzero divisor; every application caller supplies a positive constant.
Initially normal division introduced unresolved ARM compiler-runtime helpers.
This bounded integer routine removes that dependency until compiler-rt is
ported. It is an example utility, not a general replacement for compiler-rt.

`GuiMain` explicitly constructs and closes an `RWsSession`, `CWsScreenDevice`,
`CWindowGc`, `RWindowGroup`, and `RWindow`. Group/client handles are 1 and 2.
The initial screen size determines the layout; supported dimensions are
120..8192 by 160..8192. Rotation and resizing after launch are not handled yet.
Digits and control marks are filled rectangles, so no font selection is needed.
Green has a plus; amber has a horizontal reset stroke; red has a boxed exit
mark. The painting implementation deliberately keeps its inputs on the stack.

The loop posts `EventReady` and `RedrawReady`, waits through
`User::WaitForRequest`, processes completed requests, and reposts them. Pointer
button-down events for window handle 2 change the model and invalidate the
window. Redraw requests for that handle bracket drawing with `BeginRedraw` and
`EndRedraw`. On exit it cancels and completes pending requests before their
stack statuses disappear, then closes GUI objects before the session.
This is Symbian's service protocol; no host scheduler or Python callback is
introduced. The surrounding Python staging/build policy uses existing native
format bindings, whose GIL and status handling follow A11. Host native libraries
retain their no-exception/Abseil status policy. At the target OS boundary the
example uses Symbian's required `TInt` result convention. Abseil and A11's host
thread library have not been ported into the guest.

The startup is a deliberately limited primary-thread adapter. It consumes the
entry reason and thread-create pointer, checks the source-derived struct sizes,
calls `UserHeap::SetupThreadHeap`, then `User::InitProcess`, creates an SDK
`CTrapCleanup`, calls `GuiMain`, deletes the cleanup object, and ends through
`User::Exit`. Failure to allocate the cleanup object returns `KErrNoMemory`.
Unexpected thread/exception entry calls
`User::Invariant`. It provides no secondary-thread or global constructor
support. Unlike the earlier resource-free integer probes, it attempts real SDK
initialization and cleanup. That attempt is still a **runtime experiment**:
successful linking cannot prove that the installed Belle EUSER ABI agrees.
The reference is the original
[ARM executable startup](https://github.com/SymbianSource/oss.FCL.sf.os.kernelhwsrv/blob/0c3208650587ac0230aed8a74e9bddb5288023eb/kernel/eka/euser/epoc/arm/uc_exe.cpp)
and its accompanying `uc_exe.cia`, together with the original
[Window Server header](https://github.com/SymbianSource/oss.FCL.sf.os.graphics/blob/ff133bc50e6158bfb08cc093b0f0055321dcde99/windowing/windowserver/inc/W32STD.H).

