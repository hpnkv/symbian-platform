# Development-agent wire framing

The planned resident service will use one protocol over an authenticated TCP
session. Host native code owns framing and the full control envelope. A small
guest codec answers read-only hello/status requests. A one-connection research
DLL has exercised that exchange inside the emulator; a resident service is
still pending.

## Frame shape and limits

Each frame starts with a four-byte unsigned length in network byte order,
followed by exactly that many MessagePack bytes. A zero length is invalid.
The initial phone profile accepts at most 64 KiB per frame, four completed
frames in a queue and 256 KiB of queued encoded bytes. `InboundQueue`
enforces the latter two limits when the session owner uses it.

`symbian::agent::FrameDecoder` accepts a fragment of input at a time and stops
after one complete frame, reporting how many bytes it used. The caller can
feed the unused suffix again. It checks the complete length prefix before
allocating a payload buffer. On an invalid prefix it enters a failed state;
the session should close, or call `Reset()` before using the decoder for a
new trusted stream. `EncodeFrame` applies the same maximum to outgoing bytes.
The C++ declarations and return types are in the
[native reference](../cpp/index.html).

Control messages have a separate 4 KiB ceiling. `ParseControl` validates the
MessagePack map, version, nonzero request ID, operation kind and optional
deadline. `PackControl` emits the same fields; unknown top-level fields are
retained when a message is parsed and encoded again. Version one currently
defines hello, status, cancel, result and error envelopes. The `body` remains
an untrusted map until a specific operation validates it.

The guest `Symbian::Agent` target installs
`symbian/agent/guest_control.h`. `ParseGuestControl` accepts only version-one
hello/status, a nonzero request ID, an optional unsigned deadline and an empty
body. Its limit is 4 KiB and eight top-level fields; unknown top-level fields
are preserved in the response. `PackGuestResult` responds with service name,
`ready` state and the single `status` capability. These routines parse the
MessagePack payload after the four-byte frame prefix has been checked. They
require an authenticated TLS peer; they do not authenticate, authorize,
schedule or keep a listener alive.

The service must authenticate before interpreting payloads, validate each
operation body and permission grant, and give each request a deadline,
cancellation path and final status. In particular, the one-shot research DLL
does not implement a resident active-object listener, distinct peer identities
or a handset-visible pairing action.

## Host read-only session

`symbian.agent.ReadOnlyAgentSession` opens an explicitly addressed TLS socket.
It requires a CA PEM for the server and a client certificate/key; the server
name is checked separately from the address. It sends a status request using
the native control/frame bindings, rejects an oversized prefix before reading
the payload and returns a typed `AgentStatus`.

```python
from pathlib import Path
from symbian.agent import ReadOnlyAgentSession

with ReadOnlyAgentSession.connect(
    "127.0.0.1", 39098,
    server_name="my-development-phone",
    ca_bundle=Path("certs/phone-ca.pem"),
    client_certificate=Path("certs/host.pem"),
    client_key=Path("certs/host-key.pem"),
) as agent:
    print(agent.status())
```

This is an API example for a manually started listener; it is not a working
pairing recipe for a Nokia 808. The emulator research test currently uses one
self-signed fixture identity on both peers. Production use needs separate
identities, protected key provisioning and a handset-visible pairing action.
See the
[development-agent plan](https://github.com/hpnkv/symbian-platform/blob/main/.dev/development-agent.md)
for the intended service gates.
