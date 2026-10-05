# Build a real GUI app

The native Window Server counter in `examples/gui_app` is a compact route from
C++ source to a visible Symbian application. Window Server is the OS service
that owns windows and delivers drawing and pointer events. The example draws
four digits and three touch controls, then exits through the guest runtime.

Follow the steps in order for a source checkout. If you already have an
installed SDK, start with [standalone projects](projects.md) and use this route
when you want to inspect the underlying compiler, E32 image, emulator or
runtime contracts.

| Step | Guide | You will have |
| --- | --- | --- |
| 1 | [Prepare the toolchain and public source](source-prerequisites.md) | Clang, LLD, Ninja and source inputs |
| 2 | [Understand the example](gui-architecture.md) | A map of startup, Window Server and packaging code |
| 3 | [Build and inspect](gui-build.md) | ARM ELF, E32 and host checks |
| 4 | [Run in the emulator](gui-emulator.md) | A disposable named-firmware session with real pixels and input |
| 5 | [Inspect symbols and debug](gui-debug.md) | Source navigation and ARM guest breakpoints |
| 6 | [Troubleshoot](gui-troubleshooting.md) | Common failures and diagnostics |
| 7 | [Investigate the guest C++ runtime](guest-runtime-source.md) | Runtime profile and native dependency context |

The [CLion guide](clion.md) explains IDE profiles and Run/Debug controls.
For device transfer and installer approval, use the [device guide](device.md).
Compiler or emulator success does not prove Nokia 808 compatibility.
