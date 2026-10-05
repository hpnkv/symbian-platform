# Build a real GUI app

The native Window Server counter in `examples/gui_app` is a compact route from
C++ source to a visible Symbian application. Window Server is the OS service
that owns windows and delivers drawing and pointer events. The example draws
four digits and three touch controls, then exits through the guest runtime.

## Build from an installed distribution

[Install the native SDK archive](native-distributions.md), then copy the
included counter application into your own project directory:

```sh
cp -R ~/dev/symbian-sdk/examples/gui_app ~/dev/gui_app
printf '%s\n' '{"sdk": "../symbian-sdk"}' > ~/dev/gui_app/sdk-location.json
symbian build --project ~/dev/gui_app --output ~/dev/gui_app/.symbian/build
symbian inspect ~/dev/gui_app/.symbian/build/gui_app.exe --format e32
symbian package --project ~/dev/gui_app \
  --artifact ~/dev/gui_app/.symbian/build/gui_app.exe \
  --output ~/dev/gui_app/.symbian/package
```

Edit `app.cc`, `window_server.cc`, `model.h` and `symbian.toml` in your copied
project. The selected SDK supplies the compiler, headers and libraries. Follow
[the application installation guide](building.md#5-sign-the-sis) to sign the SIS,
stage it and approve installation on a permissive device. Use the
[firmware guide](firmware.md) to configure an emulator separately.

## Build from a source checkout

The steps below also show how to prepare the SDK itself and inspect the
application's startup, compiler and emulator contracts. Run source-checkout
commands from the repository root.

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
