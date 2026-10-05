# E32 build pipeline

This is the advanced path for an explicit CMake source graph. Start with
[building an application](../guides/building.md) if you are new to the SDK.

## Source graph and target

CMake owns source files, dependencies and `compile_commands.json`. A
`symbian.toml` project selects the CMake target and preset; source files belong
in `CMakeLists.txt`. An application needs only its own source files and ordinary library targets:

```cmake
cmake_minimum_required(VERSION 3.28)
project(my_app LANGUAGES CXX ASM)
include(SymbianApp)
symbian_add_executable(my_app app.cc algorithm.cc)
target_link_libraries(my_app PRIVATE Symbian::Runtime Symbian::EUser)
```

Write `int main()` or `int main(int argc, char** argv)` in `app.cc`.
The SDK supplies startup, image layout, platform headers, ABI settings and
relocations. It discovers import libraries through the evaluated CMake link
graph, including transitive dependencies. No per-application symbol list,
import proxy, assembly entry or linker script is needed. The current startup
passes one synthetic program name in `argv`; command-line decoding is not
provided yet.

The converter writes the process security header too. Declare
`capabilities = ["NetworkServices"]` in `[project]` only when the application
needs guest network sockets. See [project configuration](project-configuration.md).

## Configure and verify

A `symbian-pic` CMake configure preset uses Ninja and a build directory under
`.symbian/`. The CLI supplies the installed toolchain and output tree:

```sh
symbian build --project my_app --output .symbian/my-app
symbian inspect --format e32 .symbian/my-app/my_app.exe
```

The build publishes ELF/E32 plus a report with tool versions, inputs and logs.
It makes a second fresh build to compare output bytes on this host and toolchain.
That is a reproducibility check, not a hermetic build proof. The root
`compile_commands.json` supports editor and clangd analysis.

The bundled `probes/` projects are runnable diagnostic probes. Their specific
loader oracles do not replace testing your application against its selected
firmware. See the [runtime guide](../capabilities/runtime.md) for supported
runtime profiles and [library targets](project-libraries.md) for DLLs.
