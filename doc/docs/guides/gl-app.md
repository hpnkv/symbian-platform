# Build a Symbian OpenGL ES app

The SDK supplies original Symbian GLES 1.1 Common, GLES 2.0 and EGL 1.4
headers and frozen import libraries. Link `Symbian::GLES1`, `Symbian::GLES2`
or `Symbian::EGL` to obtain their headers, compiler settings and imports.
Device firmware supplies the actual graphics implementations.

`examples/gl_app` is a complete GLES2 application with a spinning cube,
per-fragment Phong lighting from a warm diffuse point light and a cool
directional light, a black background and a silver pixel-lettered Exit button.
Its native Window Server input and frame timer share one request wait.

After [installing the native SDK](native-distributions.md) containing these
targets and [importing compatible firmware](firmware.md), copy the SDK's
`examples/gl_app` directory and select that SDK in `sdk-location.json`:

```json
{"sdk": "/absolute/path/to/native-sdk"}
```

From the application's directory:

```sh
symbian app build --project .
symbian app run --project . --firmware my-phone
```

Tap **EXIT** to close the application. Escape also exits. To package it:

```sh
symbian package --project . --artifact .symbian/build/gl_app.exe
```

Both ARMv5T and ARMv6 are supported. A CPU architecture does not imply GLES2
availability. For a GLES1-only deployment, configure with
`-DSYMBIAN_GRAPHICS_AVAILABLE_APIS='EGL;GLES1'`; attempting to link GLES2 then
fails at configuration time. Set `SYMBIAN_GRAPHICS_FIRMWARE_DIR` to the
deployment Z-drive directory to check required DLL presence too.

Missing SDK files, unsupported architectures, EKA1 use, conflicting ABI flags,
legacy EGL header selection and direct GLES1/GLES2 import collisions produce
configuration errors, including through static and interface dependencies.
Use CMake `if()` for conditional graphics dependencies. Separate shared DLLs
may privately use different GLES APIs. Multiple context APIs in one module
require EGL procedure lookup because common GLES import symbols overlap.

An EGL configuration or shader can still fail on a device whose library exists;
applications must check those runtime results. `gl_app` prints EGL/GL/shader
diagnostics and exits with an error. Its ARMv5T/ARMv6 rendering, rotation, black
background, input and normal shutdown have been checked against the named
preserved RM-807 Belle fixture on both EKA2L1 CPU backends on macOS arm64.
Physical GPU/device behavior and Linux rendering remain separate gates.

Source builds expose `gl_app` in the root guest CMake profiles. The application
uses standard `main`; the SDK owns startup, runtime, imports and E32 conversion.
`compile_commands.json` provides its real ARM compiler context for clangd/IDE use.
The original Khronos/SGI and Nokia/EPL notices remain with the exported headers.

CLion configurations are included for both ways of opening the code:

| Project | Run | Debug |
| --- | --- | --- |
| Standalone `examples/gl_app` | `gl_app Standalone Run`, `symbian-pic` profile | `gl_app Standalone Debug`, port 24701 |
| SDK repository root | `GL App Run`, host `debug` profile | `GL App Debug`, port 24701 |

Standalone configurations use the SDK selected in `sdk-location.json`.
Root configurations rebuild the ARMv6 example and SDK libraries from source;
prepare [source dependencies](source-prerequisites.md) first. Root symbols live
under `.symbian/workspace-apps/gl_app`. Select the example's firmware with
`symbian emu configure --scope project --project examples/gl_app --firmware my-phone`.
Debug requires ARM GDB on PATH or selected with `SYMBIAN_GDB`. Stop reaps the
owned emulator. Qt includes corresponding standalone and root configurations
on port 24702. `symbian init` generates **App Run** and **App Debug** when IDE
helpers are selected, including when ARM GDB will be installed later.
