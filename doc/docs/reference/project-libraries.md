# Native libraries in an application

Use the installed CMake helpers to compile ARM libraries with the same target
headers and ABI flags as an application. Start with [a standalone
application](../guides/projects.md) before adding libraries.

## Static library

```cmake
symbian_add_static_library(my_logic SOURCES my_logic.cc)
target_link_libraries(my_app PRIVATE my_logic)
```

The library becomes part of the final application image. Its source symbols
remain in the linked ELF for debugging.

## Dynamic library

A DLL has an explicit export definition and identity. The installed
`SymbianPic` module supplies `symbian_add_dynamic_library` and companion import
targets. A minimal shape is:

```cmake
include(SymbianPic)
symbian_add_dynamic_library(my_library
  STARTUP startup.S
  LINKER_SCRIPT image.ld
  SOURCES api.c
  EXPORT_DEFINITION exports.def
  UID3 0xE0001234
  IMPORT_SYMBOLS MyPublicFunction)
target_link_libraries(my_app PRIVATE my_library_import)
```

The export definition freezes ordinals; the linker script separates executable
code and writable data. The helper retains an ELF for symbols and produces an
E32 DLL after link/conversion checks. DLL loading and C++ constructor behavior
have bounded emulator tests; other firmware needs its own checks. The
[SDK C++ reference](native-sdk.md) and [development status](https://github.com/hpnkv/symbian-platform/blob/main/.dev/status.md)
record the exact coverage.

## Mbed TLS

The SDK physically includes the Mbed TLS adaptation's headers and source.
Applications link its installed native targets and opt into a project-local
CA bundle. Follow the [TLS guide](../guides/tls.md) for the current build and
runtime boundary. A DLL probe or ARM build alone does not establish a TLS
handshake.
