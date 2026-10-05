# SDK export and native headers

The SDK installs native headers, architecture-specific runtime and component
archives, frozen-ordinal OS import proxies and CMake helpers. See
[project configuration](project-configuration.md) for installation and selection,
and the [native API guide](native-sdk.md) for application targets.
Firmware and original system DLL implementations are supplied separately.

## OS library targets

Applications link installed CMake targets such as `Symbian::EUser` and
`Symbian::WindowServer`. The SDK supplies their complete frozen ABI and records
only the functions and data actually imported by the linked application.
Application authors do not create proxies or enumerate imported symbols.

```cmake
include(SymbianApp)
symbian_add_executable(my_app main.cc)
target_link_libraries(my_app PRIVATE Symbian::FileServer)
```

## Diagnostic proxy generation

A proxy is an ELF link artifact whose exported symbol points to an ordinal
word. It is not executable DLL implementation code. The native SDK component
reads frozen export definitions and generates Clang/LLD proxy sources.

The parser accepts `EXPORTS`, `symbol @ ordinal NONAME`, optional `ABSENT`
and `DATA size` declarations. Generation without a symbol selection includes
all present function and data exports, preserving original ordinals and holes.
Explicit selection supports up to 65535 exports. Aliases, absent selections
and decorated DLL UID/version names are unsupported; use plain DLL/DSO names.
The following command is for investigating an OS ABI, rather than building an
ordinary application.

From a prepared source checkout:

```sh
symbian toolchain import-proxy \
  research/upstream/kernelhwsrv/kernel/eka/eabi/euseru.def \
  --symbol _ZN4User4ExitEi --target-dll euser.dll \
  --headers research/upstream/kernelhwsrv/kernel/eka/include \
  --output .symbian/euser-proxy
symbian inspect --format import-proxy .symbian/euser-proxy/euser.dso
```

`--headers` is optional. When supplied, the command compiles an original-header
call and links it against the proxy. Keep the DEF outside the generated output
tree. The build retains a compilation database and records tool versions and
consumed inputs in `report.json`.

The public EABI definition places `_ZN4User4ExitEi` at ordinal 641. Selecting
it creates one ordinal slot rather than inventing preceding entries. The proxy
version names `euser.dll`; its soname is `euser.dso`. The SDK linker script
places ordinal data first and uses file offsets for dynamic pointers, as
required by the converter.

## Platform headers and ABI

The original EKA2 header profile defines `__GCC32__`, `__GCCV3__`, `__EABI__`,
`__EPOC32__`, `__MARM__` and `__MARM_ARMV5__`. In these headers,
`TRequestStatus` has status and flags words and is eight bytes;
`TDesC16`, `TDes16`, `TPtrC16` and `TPtr16` have sizes 4, 8, 8 and 12 bytes.
Use the actual target headers rather than host structures that imitate them.

An import proxy must match the selected firmware's export table. Linking
`User::Exit` does not supply heap setup, process initialization or runtime
cleanup: use SDK startup. EKA1 uses different GNU2 symbols and legacy import
layout; see the [EKA1 guide](../guides/eka1.md).

## Inspection

The native inspector bounds ELF sections, strings, symbols and version records
and checks ordinal words and DLL names. It validates the generated proxy
profile, rather than arbitrary ELF files. Binary parsing and source generation
live in the native library; Python handles paths and build orchestration.
