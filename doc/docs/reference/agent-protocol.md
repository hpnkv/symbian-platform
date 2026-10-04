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

## Pairing and authentication

The console creates one private key for each serial-derived USB identity
anchor. It stores the key under `~/.local/share/symbian/agent-identities/`
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

Before reading a control frame, the server sends `SAG1` and a fresh 32-byte
nonce. The client replies with a fresh 32-byte nonce and
`HMAC-SHA256(key, "symbian-agent-client-v1" || server_nonce || client_nonce)`.
The server checks the MAC, then returns
`HMAC-SHA256(key, "symbian-agent-server-v1" || server_nonce || client_nonce)`.
The client checks the final proof before it sends hello. A failed or incomplete
exchange closes the connection. A five-second deadline bounds the exchange.
The guest obtains its nonces through the selected RM-807 entropy adapter; its
behavior on a physical handset remains unverified.

The checked-in `agent_service/test-agent.key` is public and **emulator-only**.
The build refuses to expose that key on Wi-Fi. A private build cannot use it.
No key is sent over USB discovery or the agent socket.

## Control session

Each control frame starts with a four-byte unsigned length in network byte
order and exactly that many MessagePack bytes. Zero and lengths above 4 KiB
are rejected before the payload is read. The first authenticated request must
be hello. Status or logs sent first, or a repeated hello, closes the session.
The hello result declares protocol version 1, the 4 KiB limit, a cap of 16
requests per connection, and the available `status` and `logs` operations.
Hello consumes one request slot.

The service applies one five-second Abseil deadline to each exchange's prefix,
payload and response. The host session uses one response deadline even if a
peer sends fragments slowly. An active-object listener passes accepted sockets
to a bounded SDK worker in the emulator profile. The phone profile uses that
worker for bounded UDP discovery and outbound TCP connection attempts. The
service does not offer file writes, command
execution, flashing or recovery operations.

The guest `Symbian::Agent` target owns the control codec in
`symbian/agent/guest_control.h`. It accepts only version-one hello/status with
an empty body, or logs with exactly unsigned `after` and `limit` fields;
`limit` must be 1–8. Unknown top-level fields survive a parse/encode cycle.
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
```

The equivalent CLI commands are:

```sh
symbian agent hello 127.0.0.1 39101 --key-file agent_service/test-agent.key
symbian agent status 127.0.0.1 39101 --key-file agent_service/test-agent.key
symbian agent logs 127.0.0.1 39101 --key-file agent_service/test-agent.key \
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

## Verification boundary

The opt-in guest suite builds against the selected SDK, launches the agent in
the pinned RM-807 EKA2L1 profile, checks authentication, status, logs,
rejection, deadlines and local panel controls:

```sh
SYMBIAN_SDK_MANIFEST="$PWD/.symbian/sdk/sdk.json" \
SYMBIAN_AGENT_SERVICE_GUEST=1 \
  uv run pytest symbian/tests/test_agent_service_guest.py -q
```

Use an installed SDK manifest in place of the example path. Those tests show
bounded emulator behavior only. A private physical package still needs on-phone
installation, network reachability, the pairing-code comparison, entropy and
long-running idle behavior to be checked on the actual device.
