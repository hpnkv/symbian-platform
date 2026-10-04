# On-device development service

## Goal and boundary

Install one ordinary Symbian application/service on a development phone. It
starts after boot, remains available for authenticated SDK connections, and
sleeps while idle. The same service protocol works over Wi-Fi and a verified
USB transport. Wi-Fi must cover the normal application loop: inspect, transfer,
run, observe, deploy and debug where the OS grants the necessary capability.
USB may offer greater throughput or a more reliable connection; it is not a
prerequisite for normal development.

The service is not a privileged recovery agent. It has **no** flashing,
erasure, partition, bootloader, OTP, calibration or hardware recovery request
types. A possible future USB recovery/flashing tool belongs to a separate
human-governed broker and process, outside agent execution authority, as
required by [PLAN.md](plan.md). System file changes, reboot and system
component installation also require a separate, explicit human authorization
path. The development service initially handles only its own app data and
ordinary user-approved application deployment.

## Process and idle behavior

- Package as a signed SIS with an explicit start-at-boot entry supported by the
  target firmware. Verify the exact startup mechanism and capabilities on the
  Nokia 808 before selecting it. The server process owns one Symbian active
  scheduler and a small set of active objects; no polling worker is kept
  running while idle.
- Keep a local control IPC endpoint for app-side logging and SDK-launched
  process coordination. Create network listeners only when the corresponding
  bearer is available. A dormant process does not keep WLAN associated or wake
  the radio solely for discovery. Event subscriptions replace periodic scans.
- Allocate stream buffers, screen capture state, file handles, and debugger
  state only for an active session. Apply fixed session counts, queue lengths,
  frame sizes and memory ceilings; shed optional streams before risking the
  device's foreground application. Measure idle private bytes, wakeups and
  battery cost on hardware, then make those values release gates.
- On disconnect, cancel active requests, drain their native completions,
  close handles and return to the small listener state. A persistent bounded
  log ring may survive a session, but retention and disk use are explicit.
  Unexpected process termination should restart via the verified startup
  mechanism, with crash-loop backoff and a visible way to disable the service.

## Discovery and identity

| Transport | Discovery | Connection |
| --- | --- | --- |
| Wi-Fi | Advertise `_symbian-dev._tcp.local` with DNS-SD only while the WLAN interface has an address. Host browsing uses mDNS, plus manual IP/host entry for networks that suppress multicast. | Authenticate over the agent's keyed TCP protocol. It currently provides no confidentiality; keep it on a trusted local network. Reconnect after address changes using the paired device key, never a cached IP alone. |
| USB | Observe physical USB descriptors, mode and interfaces through the SDK's libusb inventory. Probe only a verified service endpoint or an IP-over-USB interface if the phone actually presents one. | Carry the same authenticated protocol over a suitable bulk/serial or IP transport. Do not claim that PC Suite MTP/OBEX interfaces are arbitrary application endpoints. |

Discovery may eventually announce protocol versions, a transient endpoint and
a short display name. It must not expose private device data or the shared key.
Host and phone agree on identity through the device-specific key and a
handset-visible code, with the USB serial and model as supporting evidence. A reconnecting
phone with the same key retains selection and trust; a matching model, USB
port, address or name alone does not. Key rotation and factory reset require
fresh user confirmation. The host displays transport, last-seen time and
current capability state separately from identity.

DNS-SD and mDNS follow [RFC 6763](https://www.rfc-editor.org/info/rfc6763/)
and [RFC 6762](https://www.rfc-editor.org/info/rfc6762/). A Symbian-side
advertiser is a proposed implementation, not an assumed Belle facility. USB
interface availability must be measured on the intended handset and mode.

## Pairing, trust and permissions

The first connection requires a handset-visible code comparison in the host
SDK. The initial read-only agent uses per-device secret-key HMAC challenges
over TCP. It does not claim encryption or protection against observation of
status data on the network. The default SDK still ships the application-linked
Mbed TLS 3.4.1 port for TLS 1.2 and 1.3; applications opt into that target and
their own project-local trust roots. The agent links only crypto primitives.
A version or authentication mismatch fails closed. USB is not implicitly
trusted.

Authorize by operation and scope: read diagnostics, read/write an approved
workspace, deploy ordinary apps, send input, and attach to an owned debug
target are distinct grants. The handset shows the paired host, current
session and sensitive active operations. Grants can be revoked locally;
unattended access is an explicit per-device setting, never implied by pairing.
No operation inherits more privilege merely because the transport is USB.
Phone-side platform capabilities and data-cage policy still apply.

## Shared protocol

Use one versioned protocol over both transports: a length-prefixed control
frame with typed request/response/event schemas and separate credited data
streams. Define schemas for device identity, capability, status, errors,
process, file entry, log record, screen frame and debug event; preserve
unknown fields and capability values. Use MessagePack only for serialization,
with purposed C++ structs and Python Pydantic models at the API boundary.
Negotiate version, frame limit, compression, stream count and features before
opening any data stream. Reject oversized or unauthenticated frames before
allocating their advertised payload.

Every request has an ID, deadline, cancellation path and final status. Data
streams have byte credits and bounded queues so a slow GUI cannot grow phone
memory. File and screen producers yield chunks/frames under backpressure;
the event thread only handles small parsing, ownership transfer and scheduling.
Compression, encoding, hashing and image conversion run on workers when their
cost is material. A disconnected consumer releases native buffers promptly.
Record exact transport loss, cancellation and partial completion separately.

### A11 reference and phone profile

Use A11's `cpp/a11/net/wire_stream.h` as the interface reference: nonblocking
send, start/accept, half-close, explicit drain, abort, deadline and terminal
status are useful lifecycle concepts. Its `WireMessage` in
`cpp/a11/data/types.h` demonstrates typed MessagePack validation and byte
accounting, but its node fragments and action graph are
not part of this device protocol. Define a small device message union for
control, data chunk, credit, cancellation and terminal status. A11's stream
does not promise global delivery order; file bytes, logs and debugger events
must carry a per-stream sequence and an explicit resume cursor.

The first phone profile should permit one authenticated session, at most four
queued inbound messages, a 64 KiB maximum frame, a 256 KiB aggregate inbound
budget, and 32 KiB file/screen chunks. Control messages get a smaller limit.
These are initial ceilings to test, not measured Nokia 808 optima. A11's
general `WireStreamOptions` defaults allow 1,000 queued messages and 32 MiB
for both a message and the buffered total; copying those defaults to a phone
would waste scarce memory. Reserve buffers lazily and return them on idle.
Permit a second session or larger windows only after memory, latency and
throughput tests justify it.

A11's `ChunkStoreReader` provides a pull cursor, cancellation and `NextMany`
batching; `ChunkStoreWriter` separates queue admission from backing-store
confirmation and can flush a cheap operation inline. Adapt those semantics for
file and log streams. Do not instantiate a general chunk store for every
transfer: a file cursor can read directly into a reusable bounded buffer, and
the writer can hold one or two pending chunks. Report **admitted**, **written**,
**flushed** and **verified** as distinct states; acknowledgement of a queue
slot is not proof that bytes reached the destination file.

The first device transport is framed, authenticated TCP over Wi-Fi or the
verified USB carrier. It does not require HTTP in the phone process. A11's
host networking uses an HTTP/1.1 codec, nghttp2 for HTTP/2, OpenSSL for TLS,
and libuv/uvw for event handling; some host components also use libcurl.
A11 also has WebSocket and HTTP SSE `WireStream` transports. Those are
references if an interoperable HTTP endpoint becomes necessary. Do not port
the HTTP/2, WebSocket or SSE stacks
to the phone merely to serve this SDK. The existing FastAPI/httpx transport
between `symbian console` and its host backend remains separate from the
phone wire protocol. If phone-side HTTP is later required, choose a maintained
small parser/library after checking code size, allocations, idle cost and
platform TLS support; do not write an ad hoc HTTP parser.

## Capability areas

| Area | Service contract and presentation |
| --- | --- |
| Feature discovery | Return actual firmware, OS, model, free space, transport, installed agent/protocol versions, permissions and per-feature availability with a reason. Unknown remains unknown. Refresh asynchronously and cache the last verified snapshot with its timestamp. |
| File transfer | Browse only authorized roots. Stream incremental listings and file chunks with cancellation checkpoints. Push to a temporary file in the destination filesystem, verify length and digest, then commit by an explicit replace/rename step. Pull with digest and resumable offsets. Show progress and partial-state cleanup. Do not expose raw `RFs`, `RFile` or `RDir` in the host API. |
| Logs | Structured records with source, process/thread, severity, monotonic timestamp and sequence. Read from a bounded ring by cursor, then follow live; indicate gaps when records were dropped. Offer filters and export. Native OS log collection is a separate capability because coverage and permissions may vary. |
| Remote shell/control | Start with a bounded command runner for SDK-owned apps/tests: launch, stop, arguments, working directory, stdout/stderr and exit status. Provide input/control as separately authorized operations. An interactive shell view may be added over that transport, but an unrestricted privileged OS command interpreter is outside the ordinary agent. |
| Screen | Produce cancellable still captures first, then a bounded frame stream with timestamps, dimensions and format. Prefer dirty-region/delta transport only after measuring its CPU cost. Drop old frames for a slow viewer rather than delaying input or growing memory. Screen capture and input injection each report their actual platform permission. |
| Deployment | Verify SIS metadata and digest on the host, transfer to a private staging area, ask for any handset installer confirmation, and report installer transaction/result. Preserve package provenance and distinguish transferred, installed and launched states. Rollback is only claimed where the native installer proves it. |
| Debugging | Tunnel a guest debugger protocol or an SDK-owned debug server for user processes, with attach, breakpoints, threads, registers, memory reads and stop events only where the OS permits. Correlate symbols and source on the host. Reject kernel debugging claims until a separate verified transport exists. |

USB-specific privileged recovery or firmware work is outside this service and
outside these protocol schemas. Wi-Fi can therefore remain sufficient for
most application and service development.

## SDK and console integration

The host has one session/transport layer under `symbian/device/`, shared by
CLI and `symbian console`. Native parsing or transport code, if needed, lives
in `cpp/symbian/<component>/` and binds through `cpp/python/`; Python owns
selection, policy and presentation. Use `absl::Status`/`StatusOr` at native
boundaries and `Status`/`StatusException` in Python. Long calls expose
iterative data and a cancellable `Future` where the existing async bindings
make that appropriate. Do not duplicate wire-format parsing in Python.

The CLI extends existing `symbian device` commands with `discover`, `pair`,
`connect`, `status`, `files ls/push/pull`, `logs read/follow`, `run/stop`,
`screen capture/watch`, `app deploy` and `debug attach`. Commands accept a
stable paired device ID and an optional transport preference; JSON/human
output uses the same typed models. `symbian device info` includes agent
capabilities when connected and clearly labels cached/offline data. A
`symbian agent` group can hold pairing management and diagnostics without
turning every feature into a separate device identity.

In `symbian console`, the Devices view discovers USB and Wi-Fi peers together,
shows trust/transport/connection state, and opens a device workspace. That
workspace has tabs for Overview, Files, Logs, Screen, Applications and Debug;
a terminal panel is available for the bounded command runner. Connection,
pairing and permissions are shown in a contextual sidebar. Each view renders
cached data immediately, refreshes in the background and shows progress or
pending device state in the status bar. Streaming views are cancellable and
clear stale status promptly on disconnect. The existing FastAPI/httpx
in-memory backend remains the frontend boundary; a new frontend must not
reimplement transport behavior.

## Project steps and gates

1. **Contracts and baseline.** Pin the target firmware and inspect its
   startup, socket, USB-client, installer, screen and debug interfaces. Measure
   current idle memory and battery cost. Write typed wire schemas, capability
   IDs, threat model and operation policy. Record unsupported paths rather
   than inferring them from headers.
2. **TLS runtime and transport.** Ship the port's headers, three static
   archives and architecture-specific CMake targets in every SDK installation.
   Keep its Apache-2.0 notices and source revision in the SDK provenance.
   Verify the exported targets in independent ARMv5T and ARMv6 consumers; a
   project without an Mbed TLS link must gain no TLS objects or imports.
   Provide a phone entropy source with health/failure handling, UTC conversion,
   certificate trust provisioning and nonblocking socket callbacks. Provide
   an optional per-project CMake CA bundle setting: validate the chosen PEM
   file, package only that bundle as an application resource, record its
   digest, and expose its packaged path to the TLS owner. An unset setting
   includes no roots; a project can use a private CA or explicit pinning
   without rebuilding the SDK. Add a
   small `Symbian::Connectivity` owner for TLS contexts and sessions so C++
   applications can use scoped lifetime, typed failures and cancellable
   reads/writes without managing raw Mbed TLS structures. Keep the C API
   available to callers needing its full configuration surface. Test
   authenticated TLS 1.2 and 1.3 handshakes, rejection of expired/wrong-peer
   certificates and cancellation under bounded memory on the emulator. Host
   TLS success and guest crypto execution alone do not satisfy this gate.
3. **Emulator-only preparation.** Build the minimal service, local status UI,
   active-object listener, a manually addressed authenticated read-only
   handshake, crash-loop backoff and idle/no-network state.
   Exercise start, stop, reconnect, malformed input, cleanup and idle behavior
   in disposable emulator instances. Verify the ARM image, imports, SIS
   metadata and installation path separately; emulator success does not prove
   Nokia 808 loader or service compatibility. Keep boot start disabled in the
   first phone package.
4. **Move to a development phone.** Start this gate only after step 3 passes,
   the target firmware/capabilities are recorded, the service has a local
   disable/uninstall path, and the development phone has a known-good recovery
   route and independently held offline backup with recorded digests. Install
   the ordinary signed SIS on the **development** phone through the verified
   user-approved installer path, initially launch it manually, and confirm
   process startup, shutdown, local UI, permissions and idle memory/wakeups on
   the handset. Verify guest entropy, the agent's authenticated handshake and
   idle-memory behavior on the phone before enabling regular Wi-Fi use. Pair through a handset-visible
   action, verify the read-only
   host handshake, and test cable removal, Wi-Fi loss, sleep/wake and a crashed
   service. Only then enable the verified boot-start mechanism; measure idle
   battery use and crash-loop recovery over
   a long run. Keep the preservation phone untouched. If a gate fails, remove
   or disable the app using the ordinary installer; no flashing or recovery
   operation is part of this migration.
5. **Wi-Fi discovery and trust management.** Add DNS-SD, manual address
   fallback, persistent key pinning and host CLI/console discovery. Test
   disconnect, address change, revoked keys, malformed frames and multiple
   hosts. This is the first discoverable end-to-end milestone.
6. **Files, logs and command runner.** Add scoped roots, transactional file
   transfer, bounded log cursor/follow and SDK-owned process launch/stop.
   Test cancellation, backpressure, digest mismatch and a full app build/run
   loop over Wi-Fi.
7. **Deployment and screen.** Integrate ordinary SIS installer outcomes,
   still capture and then measured frame streaming/input control. Keep
   installer confirmation and screen/input grants visible on the handset.
8. **USB transport.** Verify a non-conflicting phone-side endpoint in each
   relevant USB mode and carry the same session protocol. Test same-device
   handoff between Wi-Fi and USB without changing trust or losing progress.
9. **Debug transport.** Attach to an SDK-owned user process, prove stop,
   breakpoint, source mapping and detach on physical hardware, then expose
   CLI/console debugger views. Keep kernel-level work out of this agent.
10. **Hardening and release.** Run long idle, sleep/wake, low-storage,
   radio-loss, USB unplug, competing-client and crash recovery tests. Audit
   capabilities, data retention, pairing reset, and package provenance.

Each gate needs host tests, emulator tests where meaningful, and a separate
physical-device result. An ARM build or emulator success is not proof of
Nokia 808 service behavior. Record outcomes in `.dev/status.md` and open
questions in `.dev/research-log.md`.
