# Guest CMake helpers

Include `SymbianApp` after `project()` in an ARMv5T or ARMv6 project using the
SDK's `symbian-arm.cmake` toolchain. These functions create ordinary CMake
targets, so use `target_link_libraries`, `target_include_directories` and
`target_compile_definitions` normally. The SDK supplies startup code, public
target headers, ABI flags, import interfaces and transitive libraries.

```cmake
include(SymbianApp)
symbian_add_executable(my_app main.cc)
target_link_libraries(my_app PRIVATE Symbian::Audio)
symbian_publish_executable(my_app UID3 0xe0000123)
```

## Application images

`symbian_add_executable(<target> <sources>...)` creates an ARM ELF application
target with the SDK startup and linker layout. `SOURCES <sources>...` is also
accepted. Write `main()` or `main(int, char**)` in the application. Link the
specific `Symbian::` facilities used; the helper discovers their ordinal
imports and adds the compatible runtime. The ELF keeps source symbols for
debugging. Creating this target alone does not produce an E32 executable.

`symbian_publish_executable(<target> UID3 <uid>
[CAPABILITIES <mask>] [PROJECT_DLLS BUNDLE|RUNTIME]
[IMPORT_PROXIES <paths>...])` converts that ELF to `e32/<target>.exe` and adds
the `<target>_e32` build target. The UID3 is required; the package UID in
`symbian.toml` is a separate identity. `CAPABILITIES` passes the image's
capability mask to the converter. Normal `Symbian::` links provide import
proxies automatically; `IMPORT_PROXIES` is an advanced escape hatch for an
independently supplied frozen interface. The SDK validates the selected
architecture, ABI flags and native API payloads at configuration or
generation, then validates E32 conversion at build time.

When the linked graph contains a project-built DLL, select one deployment
policy explicitly. `PROJECT_DLLS BUNDLE` places those DLLs in the SIS beside
the executable; `PROJECT_DLLS RUNTIME` keeps their imports but expects an
independent installation. An omitted or unknown value fails configuration.
Imported firmware DLLs provide interfaces only and are never bundled. See
[native libraries](project-libraries.md) and `examples/linking_app` in the
source tree.

In the root IDE's guest-index profile, the executable and library ELF targets
remain available for analysis; E32 publication is skipped when that profile
has no host converter. A normal source or installed-SDK build publishes E32.

## Project libraries

| Function | Required arguments | Output and use |
| --- | --- | --- |
| `symbian_add_static_library(<target> SOURCES <files>...)` | One or more sources | An ARM static archive; link the target into an executable or DLL. Its object code and source symbols become part of that image. |
| `symbian_add_dynamic_library(<target> SOURCES <files>...)` | Sources and a DLL UID3 from `symbian.toml` or `UID3 <uid>` | An ELF for debugging, an E32 `.dll`, and a generated ordinal import interface. Link the CMake DLL target directly from consumers. |

The dynamic helper also accepts `EXPORT_DEFINITION <def>` to preserve an
existing DLL's frozen ordinals, `RUNTIME_TARGET <target>` for a selected SDK
runtime, and `STARTUP <assembly>` or `LINKER_SCRIPT <script>` for researched
image profiles. Its `IMPORT_PROXIES <paths>...` option is the same advanced
escape hatch. Without `EXPORT_DEFINITION`, visible source functions receive
automatically generated ordinals; freeze a definition before publishing an
independently versioned ABI. The SDK rejects unsupported data exports rather
than fabricating a usable DLL.

## Native leaves

`symbian_enable_native_leaves(<target> SOURCES <files>...)` enables original
Symbian leave and `TRAP` semantics only in the named translation units. Call
it after creating the target and link the owning public native facility as
usual. It adds the SDK's C++ ABI and exception personality support, while the
rest of the target keeps the default exceptions-disabled policy.

```cmake
symbian_add_executable(my_app main.cc)
target_link_libraries(my_app PRIVATE Symbian::Bafl)
symbian_enable_native_leaves(my_app SOURCES main.cc)
symbian_publish_executable(my_app UID3 0xe0000123)
```

The named sources should be the files that invoke leaving functions or
`TRAP`. The helper requires at least one source. Runtime handling of every
possible native leave is still an empirical compatibility question for each
firmware; compiler acceptance and E32 conversion alone do not establish it.

## Lower-level and validation helpers

`symbian_add_import_executable(<target> SOURCES <files>...)` and
`symbian_add_pic_executable(<target> SOURCES <files>...
[STARTUP <assembly>] [LINKER_SCRIPT <script>])` are lower-level construction
helpers for researched image profiles. Prefer `symbian_add_executable` for
applications. `symbian_add_pic_dll` similarly creates only a low-level DLL
ELF; prefer `symbian_add_dynamic_library` for a published project DLL.

`symbian_sdk_header_canaries()` adds independent installed-header compilation
targets for supported guest and Python boundaries. It is used by SDK export
validation and can be called from a separate SDK validation project. The
public native inventory and payload validators provide additional checks;
header compilation does not prove firmware execution.
