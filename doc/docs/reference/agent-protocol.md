# Development-agent wire framing

The planned resident service uses one protocol over an authenticated TCP
session. Host native code owns framing and the full control envelope. A small
guest codec answers read-only hello, status and recent-log requests. The manually started
[emulator service example](https://github.com/hpnkv/symbian-platform/tree/main/examples/agent_service)
combines the active listener, SDK worker and mutual TLS. Its fixed test key is
public; it must never be used as a device identity.

## Exercise the emulator service

The example binds **127.0.0.1:39101** inside a disposable RM-807 emulator
instance, selects TLS 1.3 and allows up to 16 control requests on one
connection. Its 4 KiB control limit is checked before reading a payload. The
first authenticated request must be hello. Status or logs sent first, and a
repeated hello, close the connection. The hello result declares version 1,
the 4 KiB control limit, the 16-request connection cap and available read-only
operations. The host validates these before using the session. Hello consumes
one request slot. The
service gives each control exchange one five-second monotonic deadline across
prefix, payload and response; a peer cannot keep a worker indefinitely by
dripping frame bytes. The host read-only session applies its requested timeout
to the entire status exchange, including fragmented responses. The
test constructs an ARMv6 E32 executable from the selected SDK, launches it
manually, sends status and log requests, tests an oversized prefix, reconnects and
stops the emulator. It runs both Dynarmic and Dyncom when the pinned firmware
fixture and emulator are prepared:

```sh
SYMBIAN_SDK_MANIFEST="$PWD/.symbian/sdk/sdk.json" \
SYMBIAN_AGENT_SERVICE_GUEST=1 \
  uv run pytest symbian/tests/test_agent_service_guest.py -q
```

Replace the manifest path with the SDK you installed. This opt-in test uses
public Mbed TLS fixtures for both peers. The service has no pairing screen,
private device identity, boot start or signed deployment policy; keep it in
the named research emulator instance. `WorkerExecutor::PostFiber` gives its
Mbed TLS job a 256 KiB stack because the default stack overflowed during an
emulator handshake. The outstanding-job cap is four, so a quick reconnect can
wait while an old TLS session drains.

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
defines hello, status, cancel, result, error and logs envelopes. The `body` remains
an untrusted map until a specific operation validates it.

The guest `Symbian::Agent` target installs
`symbian/agent/guest_control.h`. `ParseGuestControl` accepts only version-one
hello/status with an empty body, or logs with exactly two unsigned body fields:
`after` and `limit` (1–8). A nonzero request ID and optional unsigned deadline
are required. Its limit is 4 KiB and eight top-level fields; unknown top-level fields
are preserved in the response. `PackGuestResult` responds with service name,
`ready` state and the capabilities supplied by the caller. Its snapshot overload takes
`GuestStatusSnapshot`: a successful native tick query adds `system.tick_count`
and `system.tick_period_us`; a successful primary HAL display query adds
`display.width_pixels` and `display.height_pixels`. Missing observations stay
absent rather than becoming guessed values. These routines parse the
MessagePack payload after the four-byte frame prefix has been checked. They
require an authenticated TLS peer; they do not authenticate, authorize,
schedule or keep a listener alive.

`PackGuestHelloResult` emits the bounded service profile. The service applies
the hello-first rule; the codec alone has no session state. The host rejects
unsupported versions or limits and closes TLS before issuing other requests.

### Service-local event log

`AgentLogRing` retains 32 fixed-size event codes in memory on the service's
existing worker. It allocates no records while appending. A logs request
returns up to eight records after a sequence cursor, a `next_cursor` for the
next read, and `gap=true` if older records were overwritten. Cursor zero reads
from the oldest retained record. The service records successful authentication,
status reads, rejected frames and session closure. Codes are numeric so a host
can retain an unknown future value: 1 authentication, 2 status read, 3 frame
rejection and 4 session closed. The ring is cleared when the process exits.
It does not contain operating-system logs or private application file data.

Each new record also carries `severity` (1 debug, 2 information, 3 warning)
and `elapsed_us`, microseconds since this process created the ring. The ring
uses a steady clock and clamps each reading to the preceding one, so sequence
order and reported elapsed time do not run backwards. The time is not UTC,
does not survive a restart and should not be compared across processes. The
host accepts records without these fields from an older peer; it retains
unknown numeric codes and severities.

The guest parser validates the complete logs body before reading the ring;
`PackGuestLogResult` encodes one bounded page. The ring has one worker owner,
so the example needs no new scheduler or cross-thread log lock. A future
long-lived log stream needs its own credited flow control and cancellation.

The service must authenticate before interpreting payloads, validate each
operation body and permission grant, and give each request a deadline,
cancellation path and final status. The emulator example does not implement
distinct peer identities or a handset-visible pairing action.

The SDK also exports an active-object TCP accept owner. Before moving an
accepted socket to a worker, enable worker sharing before binding. The
emulator service accepts loopback connections while its event thread sleeps;
TLS and control work run on a bounded worker queue. This is a research process
launched manually, not a paired or boot-started phone service.

## Host read-only session

`symbian.agent.ReadOnlyAgentSession` opens an explicitly addressed TLS socket.
It requires a CA PEM for the server and a client certificate/key; the server
name is checked separately from the address. `connect()` first exchanges a
hello frame and exposes its typed result as `session.hello`. It sends status
requests using the native control/frame bindings, rejects an oversized prefix before reading
the payload and returns a typed `AgentStatus`. Its timeout covers the whole
request and response, rather than resetting for each `recv` fragment.
`AgentStatus.system` and `.display` are optional typed snapshots. The tick
counter wraps at 32 bits and is an elapsed-time source; HAL dimensions may
differ from Window Server layout.
`ReadOnlyAgentSession.logs(after=cursor, limit=8)` returns a typed `AgentLogPage`.
Use its `next_cursor` on the next call, and show the `gap` flag to the user.

```python
from pathlib import Path
from symbian.agent import ReadOnlyAgentSession

with ReadOnlyAgentSession.connect(
    "127.0.0.1", 39101,
    server_name="my-development-phone",
    ca_bundle=Path("certs/phone-ca.pem"),
    client_certificate=Path("certs/host.pem"),
    client_key=Path("certs/host-key.pem"),
) as agent:
    print(agent.status())
    page = agent.logs()
    print(page.records, page.next_cursor, page.gap)
```

This is an API example for a manually started listener; it is not a working
pairing recipe for a Nokia 808. The emulator research test currently uses one
self-signed fixture identity on both peers. Production use needs separate
identities, protected key provisioning and a handset-visible pairing action.
The CLI exposes the same read-only path:

```sh
symbian agent hello 127.0.0.1 39101 \
  --server-name my-development-phone \
  --ca-bundle certs/phone-ca.pem \
  --client-certificate certs/host.pem \
  --client-key certs/host-key.pem

symbian agent status 127.0.0.1 39101 \
  --server-name my-development-phone \
  --ca-bundle certs/phone-ca.pem \
  --client-certificate certs/host.pem \
  --client-key certs/host-key.pem

symbian agent logs 127.0.0.1 39101 \
  --server-name my-development-phone \
  --ca-bundle certs/phone-ca.pem \
  --client-certificate certs/host.pem \
  --client-key certs/host-key.pem --after 0 --limit 8
```

Both forms require a running agent and credentials you supplied. The
research example's public certificate and key are for emulator tests only.
See the
[development-agent plan](https://github.com/hpnkv/symbian-platform/blob/main/.dev/development-agent.md)
for the intended service gates.
