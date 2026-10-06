# Symbian GL cube

`gl_app` uses original Symbian GLES 2.0 and EGL 1.4 headers and DLL imports.
It spins a cube against pure black, with per-fragment Phong shading from a
warm diffuse point light and a cool directional light. Tap the silver pixel
**EXIT** button to quit; Escape also exits. The event loop uses one native
request wait for Window Server input, redraws and a 30 Hz frame timer.

The source layout follows those responsibilities:

- `app.cc`: standard application entry point.
- `application.h` / `application.cc`: native windows, input and frame timer.
- `renderer.h` / `renderer.cc`: EGL context/surface ownership and frame presentation.
- `cube.h` / `cube.cc`: cube geometry, transforms and shaded rendering.
- `exit_button.h` / `exit_button.cc`: layout, pointer gestures and button rendering.
- `shader.h` / `shader.cc`: shader compilation/linking and diagnostics.
- `shaders/`: cube and button vertex/fragment GLSL files. CMake embeds them
  through `shaders.h.in` and rebuilds when a shader changes. No loose shader
  files need to be installed on the device.

With a native SDK containing the graphics targets:

```sh
symbian build --project examples/gl_app --output .symbian/gl-app
symbian emu run --project examples/gl_app --firmware my-phone
symbian package --project examples/gl_app --artifact .symbian/gl-app/gl_app.exe
```

For CMake/IDE builds, select the installed SDK in `sdk-location.json`, then
use the `symbian-pic` preset. Source workspace builds expose `gl_app` in both
guest presets; `cmake --build build/guest-probes-armv6 --target gl_app` uses
repository-owned inputs. `compile_commands.json` retains the ARM compile context.

CLion **gl_app Standalone Run** and **gl_app Standalone Debug** configurations
are included in `.idea/runConfigurations/` when opening this directory as a project.
They use `sdk-run` / `sdk-debug`, the selected SDK and the project's firmware
selection. Debug discovers ARM GDB from the SDK or PATH; `SYMBIAN_GDB` overrides
it. Its endpoint is localhost:24701. Stop reaps the owned emulator. New
`symbian init --ide intellij` projects receive the same helpers automatically.

When opening the SDK repository root, use **GL App Run** or **GL App Debug**
from the root `.run/` directory. Run uses the host `debug` profile and its
`gl_app_run` executable. Both rebuild the repository's ARMv6 runtime and example
through the source CMake graph, independent of `sdk-location.json`.
Root Debug uses `.run/gl_app-debug`; symbols are published under
`.symbian/workspace-apps/gl_app`. Prepare the source dependencies and select
firmware for the example before running. ARM GDB must be on PATH or supplied
through `SYMBIAN_GDB`.

Applications link the APIs they use:

```cmake
include(SymbianApp)
symbian_add_executable(my_app app.cc)
target_link_libraries(my_app PRIVATE Symbian::GLES2 Symbian::EGL)
```

`Symbian::GLES1` provides GLES 1.1 Common fixed-function rendering;
`Symbian::GLES2` provides programmable GLES 2.0; `Symbian::EGL` provides EGL 1.4.
These are import libraries for firmware DLLs, rather than GPU implementations
distributed with the SDK. ARMv5T and ARMv6 use the same soft-float AAPCS imports.
CPU architecture alone does not determine whether a device has GLES2.

Configuration checks transitive dependencies, missing SDK files, EKA1 use,
unsupported architectures and incompatible ABI flags. Direct GLES1/GLES2
imports in one binary are rejected because common symbol names bind to only
one DLL; use separate libraries or EGL procedure lookup for multiple APIs.
Select conditional graphics links with CMake `if()`; unresolved link generator
expressions in a graphics graph produce a configuration error.

Set `SYMBIAN_GRAPHICS_AVAILABLE_APIS` to the semicolon-separated APIs supplied
by your deployment firmware, e.g. `EGL;GLES1` for a GLES1-only device. The default
lists SDK-supported APIs and does not certify unknown firmware.
Set `SYMBIAN_GRAPHICS_FIRMWARE_DIR` to a deployment Z-drive directory to check
required DLL presence during configuration as well. DLL presence establishes
availability of the library, rather than GPU/configuration or extension support.
Context/config creation, shader compilation and swap failures still require
runtime checks;
the example logs diagnostics and exits with an error instead of showing an
unlit fallback. Physical GPU/device behavior requires separate validation.

Original headers retain Nokia/EPL and Khronos/SGI notices. See
[native SDK reference](../../doc/docs/reference/native-sdk.md).
