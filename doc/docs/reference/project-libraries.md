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

A DLL is a normal linkable CMake target. The SDK supplies its entry, image
layout and complete ordinal import library:

```cmake
include(SymbianApp)
symbian_add_dynamic_library(my_library SOURCES api.cc)
target_include_directories(my_library PUBLIC include)
target_compile_definitions(my_library PRIVATE MY_LIBRARY_BUILD=1)
target_link_libraries(my_app PRIVATE my_library)
```

The project identity comes from `symbian.toml`; `UID3` can override it when a
project produces multiple DLLs. The helper retains `my_library_elf.elf` for
debugging, publishes `my_library.dll`, and exposes its generated import library
through the CMake target. The SDK discovers DLL dependencies from the linked
target graph. Applications do not maintain export symbol selections or proxies.

Visible function definitions form the initial export interface. Mark internal
helpers with hidden visibility. Automatic ordinals are deterministic for an
unchanged interface; adding or removing functions can change them. An independently
versioned DLL can optionally use `EXPORT_DEFINITION` to freeze its existing ABI.
Automatic data exports are not supported. Imported data from original OS and
Qt libraries is supported through their frozen import libraries.

SDK startup handles process-attach constructors and finalizers. The default
DLL runtime is `Symbian::Runtime`. Linking a streams-based SDK component
automatically selects its matching runtime and compile settings. Use ordinary
`target_link_libraries` for these dependencies; incompatible runtime profiles
produce a configuration error. Check the selected
firmware's imports and ABI; see the [SDK C++ reference](native-sdk.md).

## Mbed TLS

The SDK physically includes the Mbed TLS adaptation's headers and source.
`Symbian::Crypto` links the cryptographic primitives without a TLS stream;
`Symbian::Tls` adds the C++ server and TLS/X.509 archives. Applications link
these installed native targets explicitly and opt into a project-local
CA bundle. Follow the [TLS guide](../guides/tls.md) for client/server examples,
peer verification and the target-specific entropy requirement.
