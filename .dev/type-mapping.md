# Owned C++ scalar types at external boundaries

Use standard fixed-width types for owned state, arithmetic, buffers and public
SDK C++ helpers. Keep foreign names only in declarations that must match a
Symbian, SDL, OpenGL, compiler or test-oracle signature. Convert at the call
site; never expose a foreign integer alias merely to shorten a declaration.

| External spelling | Owned spelling | Boundary retained |
| --- | --- | --- |
| `TUint8`, `Uint8` | `std::uint8_t` | descriptors, SDL C functions |
| `TUint16`, `Uint16` | `std::uint16_t` | descriptors, SDL C functions |
| `TUint32`, `Uint32` | `std::uint32_t` | native/SDL C functions, atomics ABI |
| `TUint64`, `Uint64` | `std::uint64_t` | native/SDL C functions, atomics ABI |
| `TInt8`, `Sint8` | `std::int8_t` | native/SDL C functions |
| `TInt16`, `Sint16` | `std::int16_t` | native/SDL C functions |
| `TInt32`, `Sint32` | `std::int32_t` | native/SDL C functions |
| `TInt64`, `Sint64` | `std::int64_t` | native/SDL C functions |
| `TInt`, `TUint` | `std::int32_t`, `std::uint32_t` when width matters | Symbian results, handles and callbacks |
| `TBool` | `bool` | Symbian callback arguments and output slots |
| `TReal32`, `TReal64` | `float`, `double` | Symbian callbacks |
| native time intervals | `std::chrono` durations | OS timer and MIDI calls |

The guest ABI checks `sizeof(TInt) == 4` and `sizeof(TUint) == 4`. The runtime
atomic bridge retains its `TUint*` signatures because it implements compiler
and EUSER entry points. Native `api/*/native*` files and original-style example
sources retain Symbian types around direct OS calls. SDL backends retain
`Uint*` only in SDL callback/function definitions. Public SDK owners, shared
game logic and the timing API use standard types. Historical validator
fixtures retain original type spellings to compare original behavior.

Do not replace external enum, handle, descriptor, or opaque pointer types with
integers. Such a substitution would discard their API contract.
