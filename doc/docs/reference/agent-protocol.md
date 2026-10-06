# Development-agent protocol

The resident agent is a manually started, read-only service. USB discovery
identifies a connected phone and can stage its SIS package; it does not expose
an agent socket. The emulator profile listens on `127.0.0.1:39101` with a
public test key. A phone-specific build embeds a separate 32-byte key, sends
keyed UDP discovery probes on the local network and connects to a responding
console at TCP port `39103`. No host or phone IP address is stored in its SIS.

The agent uses Mbed TLS **crypto primitives** for random challenges and
HMAC-SHA256, without a TLS connection. The SDK's `Symbian::Tls` target and
project-local CA bundle remain available to applications separately. This
agent protocol authenticates both peers but does not encrypt traffic; use it
on a trusted local network. Neither USB detection nor a completed ARM build
proves installation or execution on a Nokia 808.

## WebSocket transport

Agent 1.1 uses a binary WebSocket over HTTP/2 prior knowledge, negotiated
through RFC 8441 extended `CONNECT /symbian-agent`. Both endpoints use
nghttp2 1.70.0. The phone is the WebSocket client on an outbound connection;
the console accepts it as the WebSocket server. For emulator loopback, the
host connects as client and the guest accepts as server. The authentication
roles remain guest as challenge issuer and host as proof responder.

The SDK exposes `Symbian::WebSocket`, `WebSocketStream`, `WebSocketServer`,
and a socket-independent `symbian::websocket::WebSocket` codec. Host Python
uses `symbian.websocket.WebSocketStream` and `WebSocketServer`, with the same
native codec through `_native.WebSocketCodec`. Native bindings release the
GIL and retain no Python callbacks. The worker and its existing A11 thread
executor remain responsible for ownership and scheduling.

The framing parser, endian helpers, masking loop and frame writer come from
A11's `Http2WebSocketChannel`. The parser keeps A11's buffer adoption path.
The transport adapter replaces A11's unavailable libuv HTTP body stream with
nghttp2 memory callbacks and SDK TCP calls. Guest masking keys use the existing
SDK secure entropy provider; host keys use OpenSSL. The default message limit is 4100 bytes
(4 KiB control plus its prefix); each direction has a 64 KiB queue bound, and
receive queues hold at most 16 messages. Headers are capped at 2048 bytes and
16 fields. nghttp2 also bounds settings, acknowledgements and continuation
frames and disables dynamic HPACK tables. These are queue limits, not an
attestation of total process memory usage.

Authentication packets and complete control frames travel in binary messages.
Message boundaries do not replace the native control length prefix. Fragmented
binary messages and ping/pong use A11's framing code. Close drains already
received messages; transport/protocol errors abort the connection. Handshake
and request deadlines are distinct from stream lifetime.

This endpoint requires RFC 8441 support; HTTP/1.1 Upgrade clients and browser
WebSocket APIs cannot directly use this cleartext HTTP/2 endpoint. The old
raw-TCP agent and the new host require matching transport versions. Physical
1.0.8 observations do not establish compatibility of the new transport.

## Pairing and authentication

The console creates one private key for each serial-derived USB identity
anchor. It stores the key under `~/.symbian/agent-identities/`
with owner-only permissions, outside the repository. The key is embedded in
a phone-specific build. The handset panel displays an eight-character code
derived from the key. The owner must compare that code with the console's card
before checking live status. An authenticated reply proves possession of
the key; the visual comparison connects that key to the phone in hand.

During a status check, the console listens on UDP port `39104` and TCP port
`39103`. The phone broadcasts `SAGD1`, a fresh eight-byte nonce and
`HMAC-SHA256(key, "symbian-agent-discover-v1" || nonce)`. The console replies
only after checking the MAC; its reply is `SAGR1`, the same nonce and
`HMAC-SHA256(key, "symbian-agent-offer-v1" || nonce)`. The phone checks the
entire reply and connects to its IPv4 sender. The console accepts a TCP peer
only if it sent a valid discovery request during this check. Discovery and TCP
ports are protocol constants; addresses are found anew on each status check.

After the WebSocket handshake and before reading a control frame, the guest sends `SAG1` and a fresh 32-byte
nonce. The host replies with a fresh 32-byte nonce and
`HMAC-SHA256(key, "symbian-agent-client-v1" || server_nonce || client_nonce)`.
The guest checks the MAC, then returns
`HMAC-SHA256(key, "symbian-agent-server-v1" || server_nonce || client_nonce)`.
The host checks the final proof before it sends hello. A failed or incomplete
exchange closes the connection. A five-second deadline bounds the exchange.
The guest requires secure entropy for fresh nonces. The SDK supplies the shared
OS provider described in [TLS entropy contracts](tls-sdk.md). Unsupported OS
contracts fail closed. The RM-807 emulator still requires its matching secure
RNG implementation; physical-device validation remains a separate gate.

The checked-in `agent_service/test-agent.key` is public and **emulator-only**.
The build refuses to expose that key on Wi-Fi. A private build cannot use it.
No key is sent over USB discovery or the agent socket.

## Control session

Each control frame starts with a four-byte unsigned length in network byte
order and exactly that many MessagePack bytes. Zero and lengths above 4 KiB
are rejected before the payload is read. The first authenticated request must
be hello. Any other request sent first, or a repeated hello, closes the session.
The hello result declares protocol version 1, the 4 KiB limit, a cap of 16
requests per connection, and the available `status`, `logs` and
`workspace-list` operations.
Hello consumes one request slot.

The service applies one five-second Abseil deadline to each exchange's prefix,
payload and response. The host session uses one response deadline even if a
peer sends fragments slowly. An active-object listener passes accepted sockets
to a bounded SDK worker in the emulator profile. The phone profile uses that
worker for bounded UDP discovery and outbound TCP connection attempts. The
service does not offer file writes, command
execution, flashing or recovery operations.

The guest `Symbian::Agent` target owns the control codec in
`symbian/agent/guest_control.h`. It accepts version-one hello/status with an
empty body, or logs and workspace listing with exactly unsigned `after` and
`limit` fields; `limit` must be 1–8. Unknown top-level fields survive a
parse/encode cycle.
The codec validates MessagePack after the socket owner checks framing and
authentication. It does not own the listener, permission policy or scheduler.
The C++ declarations and return types are in the
[native reference](../cpp/index.html).

A status response names the service and `ready` state. A successful native
tick query adds `system.tick_count` and `system.tick_period_us`; a successful
primary display query adds `display.width_pixels` and
`display.height_pixels`. Missing observations stay absent. The tick counter
wraps and is an elapsed-time source; display geometry may differ from Window
Server layout.

### Service-local event log

`AgentLogRing` keeps 32 fixed-size process-local events. Logs reads return up
to eight records after a sequence cursor, a `next_cursor`, and `gap=true` if
older records were overwritten. Codes are 1 for authentication, 2 for a status
read, 3 for a rejected frame and 4 for session closure. Each record includes a
severity and clamped microseconds since this process created the ring. The
times are not UTC and cannot be compared across process restarts; clock
adjustments can affect elapsed intervals.

### Agent workspace

`workspace-list` enumerates only the agent's own
`C:\private\e0000a31\workspace\` directory. The request cannot supply a
path. Each page contains at most eight immediate child names, directory and
read-only flags, byte sizes, a `next_offset`, and a `more` flag. The listing
stops at 256 entries; offsets at or beyond that bound are rejected. A missing
workspace is empty. Pages are not a snapshot, so files
changed during pagination can shift their offsets. The agent does not read file
contents or expose arbitrary device paths.

## Host API and CLI

`ReadOnlyAgentSession` uses native bindings for MessagePack framing and typed
Python models for results. The direct connection API and CLI serve the emulator
listener. Pass its test key:

```python
from pathlib import Path
from symbian.agent import ReadOnlyAgentSession

with ReadOnlyAgentSession.connect(
    "127.0.0.1", 39101, key_file=Path("agent_service/test-agent.key")
) as agent:
    print(agent.status())
    page = agent.logs(after=0, limit=8)
    print(page.records, page.next_cursor, page.gap)
    workspace = agent.workspace_list(after=0, limit=8)
    print(workspace.entries, workspace.next_offset, workspace.more)
```

The equivalent CLI commands are:

```sh
symbian agent hello 127.0.0.1 39101 --key-file agent_service/test-agent.key
symbian agent status 127.0.0.1 39101 --key-file agent_service/test-agent.key
symbian agent logs 127.0.0.1 39101 --key-file agent_service/test-agent.key \
  --after 0 --limit 8
symbian agent files 127.0.0.1 39101 --key-file agent_service/test-agent.key \
  --after 0 --limit 8
```

For a phone, the desktop console's **Check live status** opens temporary
discovery and TCP listeners. Code using the host API directly can call
`ReadOnlyAgentSession.accept("0.0.0.0", 39103, key_file=key)` on a trusted local
network. The equivalent CLI command is `symbian agent listen --key-file
/private/agent.key`; it discovers the phone without an IP argument. The public
test key must never be used for a phone profile. The
[emulator guide](../guides/agent-emulator.md) gives the build and launch steps.

The console offers a separate **Read service events** action after a successful
status check. It reads the newest eight events from the agent's fixed ring
only when requested. The host API exposes `recent_logs(limit=8)` for this
bounded snapshot; `logs(after=cursor)` remains available for cursor-based
reads.

The phone-initiated CLI path can read the same log without an IP address:

```sh
symbian agent listen --key-file /private/agent.key --logs
symbian agent listen --key-file /private/agent.key --logs --after 12 --limit 8
symbian agent listen --key-file /private/agent.key --files
```

Each command opens one temporary authenticated listener. `--after` is a
process-local sequence cursor; it does not survive an agent restart.

## Device requirements

The emulator profile uses loopback and a public test key. For a physical device,
use a private key and package, complete the pairing-code comparison, and check
the device's network reachability and installation policy. The emulator key
and transport fixture are unsuitable for deployment.
