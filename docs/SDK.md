# Public SDK export and header research

The native SDK component reads frozen EABI export definitions and generates
Clang/LLD sources for selected function import proxies. A proxy is an ELF link
artifact whose exported symbol points to an ordinal word. It is not executable
DLL implementation code. No historical SDK, DEF table or implementation is
bundled with the wheel.

The current profile accepts `EXPORTS`, `symbol @ ordinal NONAME`, optional
`ABSENT` and `DATA size` declarations, and selects 1..256 present functions.
It preserves original ordinals, including holes. Aliases, other directives,
data imports, absent selections and decorated DLL UID/version names remain
unsupported. Plain DLL/DSO names avoid guessing a Belle module version.
The parser, selection, source generation and bounded ELF metadata inspection
live in cpp/symbian/sdk. Python owns paths, CMake/Ninja, dependency hashes and
reports. Target probe C++ source also lives under cpp and is installed as a wheel
resource. Native work releases the GIL and uses Abseil statuses without exceptions.

For the pinned public kernel checkout described in RESEARCH.md:

```sh
uv run symbian toolchain import-proxy \
  research/upstream/kernelhwsrv/kernel/eka/eabi/euseru.def \
  --symbol _ZN4User4ExitEi --target-dll euser.dll \
  --headers research/upstream/kernelhwsrv/kernel/eka/include \
  --output .symbian/euser-proxy
uv run symbian inspect --format import-proxy .symbian/euser-proxy/euser.dso
```

`--headers` is optional. When supplied, the build compiles an original-header
User::Exit call and links a research ELF against the proxy. Both proxy and link
probe repeat byte-for-byte in separate CMake trees. The persistent primary tree
has a real compilation database, and Ninja dependencies include consumed SDK
headers. Inputs and tool versions are hashed/recorded in report.json. Keep the
DEF outside the generated output tree.

The public EABI table has 2546 exports and places `_ZN4User4ExitEi` at ordinal
641. Selecting it produces one ordinal slot without inventing the preceding
640 entries. Its ELF version identifies euser.dll and soname euser.dso.
The linker script puts the ordinal section first and makes dynamic pointers
file offsets, matching assumptions in the historical converter. LLD's ordinary
shared-library script does not provide that layout. Version-script DLL names
must be unquoted here; LLD retains quote characters in the resulting version
name when the name itself is quoted.

The header profile selects `__GCC32__`, `__GCCV3__`, `__EABI__`, `__EPOC32__`,
`__MARM__` and `__MARM_ARMV5__`. It checks the public source's sizes for integer,
UID, request status, time interval and descriptor types. In this EKA2 source
TRequestStatus has separate status and flags words and is eight bytes; it must
not be modeled as a single integer. TDesC16/TDes16/TPtrC16/TPtr16 sizes are
4/8/8/12. These checks establish this compiled source profile, not the full 808
C++ ABI. Clangd consumes the resulting target database with zero errors.

An optional GTest oracle extracts Nokia's nonthrowing GetSymbolOrdinal method
unchanged from the pinned buildtools source and compiles it with fixed-width host
declarations. It reads the generated User::Exit slot as 641 and rejects a changed
section index. It checks this method's contract, not the complete historical ELF
consumer or Belle loader. The original EPL source remains in its ignored checkout.

```sh
cmake --preset debug -DSYMBIAN_BUILD_SDK_ORDINAL_ORACLE=ON
cmake --build --preset debug
ctest --preset debug
```

SYMBIAN_SDK_PROXY_TEST_IMAGE defaults to .symbian/euser-proxy/euser.dso.
The option defaults off, so ordinary native tests do not require research
checkouts or a prebuilt fixture.

The native inspector bounds sections, strings, symbols and version records,
and checks ordinal words, DLL names and the generated dynamic-pointer profile.
It is not a general ELF authenticity/validity verdict. The research artifacts:

- euser.dso SHA-256: `c53aa0b81ec07f6d18c8eab0298a8237e7906909eaa5caac975e55d3659876fb`
- header_probe.elf SHA-256: `934eb1b3da3c3d7cde86388e797a61dfd251e1c32ed7608c65567ae8a42b272d`

The linked probe uses R_ARM_JUMP_SLOT in a writable GOT/PLT segment. The current
E32 converter supports an import-free PIC profile and correctly rejects it.
The separate eager import profile provides a code-region slot layout, ordinal
conversion and development DLL execution; see IMPORTS.md. Actual target DLLs
remain untested. SDK startup additionally
initializes heap/TLS and DLL/static entry points before E32Main; User::Exit performs
cleanup. Linking its symbol does not justify replacing those contracts with the
existing direct-thread-exit experiment.

Matched Belle DLL/ROM/Z material, complete ABI, initialization, resource processing
and physical installation remain unverified. Reports keep import execution and
Symbian loader verification false. See RESEARCH_LOG.md for experiment evidence.
