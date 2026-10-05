# Actual A11 Status/StatusOr integration

The implementation is ported from the actual A11 working tree at
`fcccb8cb6e1e67d7ba0822ac14cece9ee4c7091b`, rather than a similar Python error
class. Original Apache notices and the complete license are retained. Source
identity is recorded here; standalone provenance manifests are not maintained.

* `cpp/symbian/status/` contains A11's status payload, HTTP/WebSocket, JSON,
  MessagePack and UTF-8 implementation, with namespace/include-path adaptation.
* `cpp/python/status_interop.*` copies NativeStatus, status/exception translation
  and the actual GIL-releasing StatusOr helpers from native_types/interop.
* `cpp/python/status_bindings.cc` copies A11's full NativeStatus binding and
  Python/JSON adapter. Native parser/converter bindings use ValueWithoutGil.
* `symbian/status.py` contains A11's full status policy: native-backed Status,
  Pydantic hooks, exception registry, HTTP/FastAPI/httpx and MessagePack support.
* The original pinned pybind11_abseil caster/runtime is built, including its
  canonical status and ok_status_singleton extension modules. Abseil headers
  come from one pinned revision and pybind11 headers use A11's isolated overlay.

Only Python boundary translation units enable exceptions. The copied A11 JSON
codec originally enabled exceptions for diagnostic reparsing/catching. Here it
uses the same nonthrow parse and iterative MessagePack decoder, omits diagnostic
exception reparsing, and preflights UTF-8 in object keys as well as values before
strict serialization. Native malformed-input tests compile without exceptions.
These adaptations satisfy this repository's explicit exception policy.

The payload key remains exactly `type.a11.dev/status-details+json`, allowing the
actual status bridge to retain structured details. Code/StatusError are aliases
to StatusCode/StatusException; compatibility accessors preserve the CLI/v1 error
envelope. Python package initializers only expose symbols; implementation lives
in named modules. Serializable target/session metadata uses Pydantic; live child
process handles are explicitly excluded from serialization.

This is synchronous interop. No Python object survives an asynchronous callback,
so no scheduler, deferred reference holder or event-loop facsimile was copied.
If asynchronous bindings are introduced, they must use A11's actual thread and
holder implementations with the same ownership/GIL contracts.

## Tests and inherited contracts

Native tests cover payload JSON/MessagePack round trips, malformed payloads,
invalid UTF-8 keys/values and bounded MessagePack decoding. Python tests cover
native Status/StatusOr errors, GIL release, Python callback exception conversion,
Pydantic schemas/round trips, canonical Abseil casters and A11's actual HTTP tests.
The GDB Python hook is kept in symbian/gdb_bridge.py so its embedded interpreter
can import it without loading a wheel for another CPython ABI.

Preserved upstream behaviors need explicit versioning before alteration:

* A11's WebSocket conversion is not an inverse for every private code: native
  UNAVAILABLE maps to 4013, while the reverse private table maps 4013 to
  UNIMPLEMENTED and 4014 to UNAVAILABLE. The port retains and tests that behavior;
  resolving it is an upstream protocol question.
* Python `Status.to_msgpack(packer)` writes the legacy two-field code/message
  representation and returns None. Structured details use the complete native
  payload/JSON bridge; the compact Python representation does not preserve them.
* The copied Python JSON policy does not normalize every JSONDecodeError or
  invalid-UTF-8 exception into a Status. Native format parsers remain status
  returning. This is not a claim that every policy helper handles untrusted input.

A public distribution must resolve A11/Symbian ownership of the same canonical
pybind11_abseil files and agree on the Abseil runtime ABI. Current isolated-wheel
checks pass; safe co-installation is not yet established. See DISTRIBUTION.md.
