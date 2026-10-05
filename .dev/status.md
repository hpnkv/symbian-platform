# Status

2026-10-04 resident agent workspace listing: the guest now offers authenticated,
read-only pagination of its own private workspace root through the existing
control session. Requests cannot name a path or read contents; each page has
at most eight immediate children and enumeration stops at 256 entries. A clean
ARMv5T/ARMv6 SDK export at `.symbian/agent-workspace-sdk-20261004` includes
the new public header and agent archive. The agent built from that exported
SDK after declaring the Storage `efsrv` import proxy. Host codec GTest passed
14 cases; 32 host CLI/session Pytests passed; strict MkDocs passed. The opt-in
RM-807 emulator test placed `hello.txt` in a disposable agent data cage and
read it over an authenticated session on Dynarmic and Dyncom (`2 passed, 3
deselected in 28.78s`). This is emulator behavior only. The installed 1.0.8
Nokia 808 agent does not contain this operation; the reported reboot remains
unexplained, so no new handset deployment was attempted.

2026-10-04 PC Suite agent staging and desktop window follow-up: the GUI's
generic staging failure was reproduced through `symbian device install` on the
connected 808. MTP `GetObjectInfo` for `Installs` child `0x010000af` returned
`0x2002`; the host now names failed MTP operations, logs through Abseil,
skips an unreadable pre-existing child without deleting it, and reports a
nonzero skip count. The console retains the specific failure in its activity
and work status. After the handset briefly left and returned to USB
discovery, the SDK uploaded
`Installs/agent_service-647bc74a5ba2.sis` to writable store `0x00020001`.
MTP readback matched SHA-256
`647bc74a5ba2de61ac08d49e6195ab22d570ff461ab258d804e7b50c7d26d8cd`;
a repeat returned the same object handle `0x010000b1` with `copied: false`.
This verified transfer, not installation or execution on the Nokia 808. The
skip branch was not observed on the successful call after reconnection.
Thirty-one focused host/frontend tests passed. On macOS, the maintained
EKA2L1 patch uses an accessory window for background launches. A live
disposable emulator remained visible after iTerm took focus and accepted a
mouse drag from `(474, 234)` to `(531, 270)` while iTerm stayed frontmost.
This is a host window behavior observation, not guest or phone compatibility.

2026-10-04 local status UI gate: an uncommitted Window Server panel was
compiled into the agent and exercised in the pinned Dynarmic emulator.
Screen-device and graphics-context construction did not prevent the existing
authenticated service test from passing. Adding window-group construction
caused a guest `KERN-EXEC 3` access violation inside `ws32.dll`, so the panel
was removed before packaging and pushing. The committed agent remains
headless; local stop/disable is unverified and development-agent gate 3 is
open. This failure says nothing about Nokia 808 behavior.

2026-10-04 public build terminology: generated applications and the agent
example now declare `e32-import`, while the builder also accepts `e32-pic`,
`e32-dll` and `arm-object`. Existing `*-experiment` declarations retain their
previous report schemas for compatibility. The agent's active-SDK build
returned `symbian.e32-import/v1` and `e32-executable` with E32 SHA-256
`27b5ce1df0f400780c48d883b33c48baba73555c24c9d414bb380b47c07cbdda`.
Console action names, README and agent guide describe an agent package rather
than a research SIS. Focused console/project-init tests passed (19 passed,
16 skipped), Black/Ruff and strict MkDocs passed. A broader legacy build test
run had 3 failures and 9 errors in the native converter because its current
ARM unwind check requires a Symbian exception descriptor; those old-kind
fixtures did not exercise the new aliases. This gate needs separate repair.

2026-10-04 desktop console device and agent navigation: connected USB
candidates now appear as child entries under Devices. Development Agents has
one card per candidate, a build/package control using the selected SDK, and a
SIS staging control enabled only for a volume with `stage-sis` capability.
The card says installation is unknown until an authenticated agent status
transport exists, and records staging separately from handset installation.
The agent project built and packaged with the active SDK; the package command
returned `OK` and SHA-256
`3f6dc1a5be2e1a343223076f5d0e1e2dc8d471994d40f2822c9c09d0ac2e9799`.
Frontend syntax and 14 web frontend Pytests passed. The package was not run
on a phone; the earlier emulator evidence applies only to the committed
agent service.

2026-10-04 structured resident event metadata: `AgentLogRing` now stamps
each fixed record with numeric debug/information/warning severity and a
nondecreasing, process-relative steady-clock microsecond count. The guest
codec includes both fields; the typed host model leaves them optional for
older peers. A clean `.symbian/resident-agent-event-sdk-20261004` export
installed the new public header/archive for ARMv5T and ARMv6. Native codec
GTest, 27 host session/CLI Pytests, Black/Ruff/Clang Format and strict docs
passed. The pinned RM-807 service passed Dynarmic and Dyncom (`2 passed in
38.99s`), including authenticated reads and timestamp order; a targeted
Dynarmic replay checked warning severity on rejected frames (`1 passed, 1
deselected in 22.70s`). The final ARMv6 E32 has 798,544 code, 1,540 data
and 69,316 BSS bytes with eight import DLLs. Its unsigned one-file research
SIS SHA-256 is
`b4c6f52e9908d6a745261c950374a08bf2b27f62ee0e9970d032ca3602527c69`.
Elapsed time is not UTC, survives neither process restart nor ring overwrite,
and does not make this OS log collection or a Nokia 808 result.

2026-10-04 CLion preset follow-up: the user's exact
`cmake --preset clion-guest-probes-armv6 -S ... -B ...` still failed after
the connectivity target was added because ignored `CMakeUserPresets.json`
selected an October 2 SDK at `/Users/helena/dev/symbian-sdk`. Its exported
targets did not include the current Connectivity/Stackless pair. The local
preset now selects the active SDK and the in-repository Mbed TLS source.
The exact ARMv6 configure command succeeds, as do the ARMv5T configure and
both `symbian_index_connectivity` builds. The committed CMake error now names
the stale prefix and the developer guide explains how to refresh a local
preset. This is configuration/indexing evidence only.

2026-10-04 connectivity IDE indexing repair: the root guest-probe CMake
profile now declares `connectivity_probe` explicitly and builds an object
indexing target against the active SDK's `Symbian::Connectivity` and
`Symbian::Stackless` targets. `cmake --preset guest-probes-armv6` and
`guest-probes-armv5t` configured with the current SDK prefix; both
`symbian_index_connectivity` targets compiled all seven source/assembly files
and ARMv6 `compile_commands.json` lists them. Two previously ignored Status
returns in the worker probe are checked. This is IDE indexing and compilation
evidence, not a new guest runtime or Nokia 808 result.

2026-10-04 ordinary emulator launch for the resident example: the research
project now resolves its installed SDK from the selected compiler when the
optional preset environment variable is absent. With the active negotiated
SDK and selected pinned RM-807 fixture, `symbian app run --project
examples/agent_service` built and launched a private instance. Separate
`symbian agent hello` and `status` CLI processes authenticated and returned
version/limits and a native tick/display snapshot. Ctrl-C caused the owned
launcher to reap the emulator; `launch.json` recorded
`inputs_unchanged=true` and the frontend process was gone. The emulator did
not exit within the launcher's two-second terminate grace and was killed
(`frontend_exit=-9`); this is host cleanup, not an in-guest graceful shutdown.
The new paced guide is at `doc/docs/guides/agent-emulator.md`. This workflow
still requires the manually selected fixture and public test identity.

2026-10-04 negotiated resident read-only profile: after mutual TLS, the
service now requires one hello before status or logs, advertises protocol
version 1, a 4 KiB control limit, a 16-request connection limit and available
operations, and closes on an out-of-order or repeated hello. The host validates
the profile before returning `ReadOnlyAgentSession`, enforces the advertised
request cap, and exposes `symbian agent hello`. A clean
`.symbian/resident-agent-negotiated-sdk-20261004` export installed the new
public hello codec for ARMv5T and ARMv6. Native codec GTest, 27 host
session/CLI Pytests, strict docs, Black, Ruff and Clang Format checks passed.
The opt-in pinned RM-807 service passed on Dynarmic and Dyncom (`2 passed in
41.06s`): status/logs after hello, CLI hello, status-before-hello rejection,
malformed and slow-frame cleanup, reconnect, log overwrite and the 16-request
cap. The final ARMv6 E32 has 798,288 code, 1,540 data and 69,316 BSS bytes
with eight import DLLs. Its unsigned one-file SIS SHA-256 is
`644cd5faf306a7f960201e2dc33591673c473d1f925ab70d9b74a644f397bcc3`;
it has no boot script or application registration. Hello validates this narrow
wire profile; it does not establish pairing, phone identity, grants, physical
Nokia 808 compatibility or broad transport negotiation.

2026-10-04 bounded resident service log: `Symbian::Agent` now installs the
documented `AgentLogRing` and log-result codec. One worker owns a fixed
32-record, process-local ring; authenticated clients read up to eight records
after a sequence cursor and receive a `gap` flag after overwrite. The host
native binding packs the typed logs request, `ReadOnlyAgentSession.logs()`
returns a typed page, and `symbian agent logs` exposes it through the CLI.
A clean `.symbian/resident-agent-log-sdk-20261004` export installed ARMv5T and
ARMv6 agent archives and both public headers. The native codec/ring GTest and
26 host session/CLI Pytests passed. Disposable pinned RM-807 Dynarmic and
Dyncom runs passed authenticated status and log reads, rejected-frame event
observation after reconnect, CLI log retrieval and the prior slow-prefix
control (`2 passed in 37.48s`). An additional Dynarmic run filled the ring
with 32 status requests across two capped connections and observed `gap=true`
with a later first sequence (`1 passed, 1 deselected in 20.08s`). The final
ARMv6 E32 has 796,688 code, 1,540 data and 69,316 BSS bytes with eight import
DLLs; its unsigned one-file SIS SHA-256 is
`d058ad064c947dc93c70cc24893b4a93eb61b8aacf95c92261fad09e61e2e226`.
The ring is not OS log collection, disk persistence, streaming follow or a
physical Nokia 808 result.

2026-10-04 authenticated native status snapshots: `GuestStatusSnapshot` now
adds only successful original-platform tick and primary HAL display readings
to the existing bounded read-only status result. The host maps them to typed,
optional `AgentTickSnapshot` and `AgentDisplaySnapshot` fields. A clean SDK
export at `.symbian/resident-agent-snapshot-sdk-20261004` built the updated
guest codec for both ARM profiles. Host codec GTest and mutual-TLS Python
tests passed. The resident ARMv6 consumer needed an explicit `hal.dso` import
proxy for display; after adding it, disposable RM-807 Dynarmic and Dyncom
instances returned nonempty tick periods and positive primary display sizes,
then passed repeat status, CLI, malformed-frame, slow-prefix and reconnect
controls (`2 passed in 37.39s`). Values are snapshots and do not establish
Window Server layout, physical-device behavior or a stable uptime clock. The
final ARMv6 E32 has 792,592 code, 1,540 data and 69,316 BSS bytes with eight
imports; its unsigned one-file SIS has SHA-256
`ac1ff54f86ec71401c709ddcae789cd295d99285d597d0c0f938fa9ab90aa4da`.

2026-10-04 bounded agent control deadline: each emulator service control
exchange now uses one five-second monotonic deadline across prefix, payload
and response, passing the remaining budget into the public TLS owner's
cancel/drain I/O. The host `ReadOnlyAgentSession.status()` similarly uses one
caller-selected deadline across send and fragmented receives. A local mutual
TLS drip-response control passed, and the opt-in resident test passed both
Dynarmic and Dyncom (`2 passed in 29.39s`), including a three-byte slow
prefix that the guest closed within the aggregate bound and a subsequent
successful authenticated reconnect. This does not establish cancellation of
arbitrary long-running operations or a Nokia 808 timing budget. The refreshed
ARMv6 E32 has 790,476 code, 1,540 data and 69,316 BSS bytes with seven import
DLLs; its unsigned one-file research SIS has SHA-256
`9b415621b3f28bfb08c91ab2c616d7f3b1362bffd3e2405bbb694e9ec9eb88c5`.

2026-10-04 resident emulator service: the manually started,
loopback-only `examples/agent_service` now combines an original
`CActive` accept owner, a bounded A11-derived worker fiber with a 256 KiB
stack, the public `Symbian::Tls` owner and the `Symbian::Agent` read-only
codec. The first worker TLS failure was an observed secondary-thread stack
overflow; the larger fiber fixed the two-backend status and reconnect test.
Against the cleaned `.symbian/resident-agent-verified-sdk-20261004` export,
disposable pinned RM-807 Dynarmic and Dyncom instances each answered two
status requests on one authenticated TLS 1.3 session, rejected a 4,097-byte
frame prefix, then answered on a fresh session. The host `symbian agent
status` CLI independently read the service result, and the process stayed
alive (`2 passed in 22.33s`). One-job queue admission dropped a rapid
reconnect; the tested service uses four bounded outstanding jobs. The public
worker handoff probe also passed on both backends against that export (`2
passed, 6 deselected in 16.53s`). A separate reproducible ARMv6 build
produced an E32 with 790,156 code, 1,540 data and 69,316 BSS bytes, importing
seven original DLLs. The unsigned one-file research SIS has SHA-256
`664f884d07eb5b138d1c051f28a07d521cf5288c7205351055f553ea5aa0eba2`;
its installer metadata has no boot script or application registration.
The public
fixture private key is emulator-only, and no handset pairing, status UI,
measured idle power, signed package or Nokia 808 result is claimed.

2026-10-04 public TLS owner: `Symbian::Tls` installs the documented
`TlsServer` C++ owner with explicit TLS 1.2 or TLS 1.3 selection, required
client certificate verification, caller-supplied PEM credentials and roots,
and cancellable bounded socket I/O on a worker. Its first hybrid-version
server configuration failed in the emulator with Mbed TLS `-0x7080`; the
vendored server requires a single selected version. A clean SDK export at
`.symbian/resident-agent-final-sdk-20261004` built ARMv5T and ARMv6 archives
and linked an independent consumer. The complete opt-in emulator matrix
passed against that export (`24 passed in 266.55 seconds`): existing outbound
controls, raw inbound controls, and the owner’s TLS 1.2/1.3 mutual-auth
handshakes, client-certificate rejection, framed status, and oversized-prefix
rejection. The test identity is a public fixture. This does not prove
production pairing, entropy quality on a physical device, or Nokia 808
compatibility.

2026-10-04 worker handoff: a clean
`.symbian/resident-agent-final-sdk-20261004` export includes opt-in
`ActiveTcpListener::EnableWorkerSharing()`, applied before listener socket
creation. An ARMv6 SDK consumer accepted two host connections, posted each
move-only client to the existing `Symbian::Stackless` worker and exchanged
bytes in both directions under Dynarmic and Dyncom (`2 passed, 6 deselected`).
The first version marked the session shareable after socket creation; its
worker executed but socket I/O timed out. Pre-open sharing resolved that
emulator failure. This is a selected RM-807 firmware/emulator result, not a
Nokia 808 device result.

2026-10-04 resident agent build checkpoint: the manually started,
loopback-only `examples/agent_service` links public `Symbian::Connectivity`,
`Symbian::Stackless`, `Symbian::Tls` and `Symbian::Agent` targets. Its active
accept sleeps while idle, a four-slot worker queue owns TLS 1.3 and at most
16 status requests per connection, and it rejects frames above 4 KiB before
reading their payload. A reproducible ARMv6 SDK consumer build produced an
E32 image with 789,772 code bytes, 69,316 BSS bytes and seven import DLLs.
An unsigned research SIS was generated with only `agent_service.exe` and no
boot-start script. The first emulator session completed the host's server
certificate check and TLS handshake, but the status response timed out; the
resident status/reconnect test is therefore open. The embedded certificate
and private key are public Mbed TLS fixtures and carry no device identity.

2026-10-04 active accept slice: `Symbian::Connectivity` now includes
`ActiveTcpListener`, an opaque public C++ owner for one original
`CActive`/`RSocket::Accept` request. Its observer rearms explicitly and
`Stop()` invokes native cancellation and completion drainage. The original
`CActive` ABI remains in the private native translation unit to avoid
collisions between original placement-new declarations and libc++ headers.
A clean `.symbian/active-listener-sdk-20261004` export built the new
connectivity archive for ARMv5T and ARMv6 and selected the needed original
EUSER ordinals. An ordinary SDK consumer ran in disposable pinned RM-807
instances under Dynarmic and Dyncom: two host connections each exchanged
`Q`/`A`, and shutdown cancelled the idle third accept (`2 passed, 4
deselected` targeted opt-in Pytest). The full clean-SDK connectivity run then
passed all six cases in 46.73 seconds, including existing outbound and
synchronous-listener controls. Strict MkDocs/two-Doxygen, Black, Ruff and
Clang Format checks passed. This proves a small active accept primitive in
the emulator; it is not yet integrated with TLS, a resident service, a local
status UI or a Nokia 808 device result.

2026-10-04 host read-only session: the native Python extension now binds
bounded request framing, prefix validation and result parsing, releasing the
GIL for native work. `symbian.agent.ReadOnlyAgentSession` provides explicit
CA/server-name verification and a client certificate over a synchronous TLS
socket; it returns a frozen typed status. The two local Pytests passed,
including a mutual-TLS loopback status exchange and oversized-prefix
rejection. This exercises host policy and wire compatibility, not production
pairing or a resident guest service. A clean SDK export at
`.symbian/agent-codec-sdk-20261004` installed the guest header and
`libsymbian_api_agent.a` for both ARM profiles. The opt-in research DLL now
links `Symbian::Agent` from that export rather than compiling its source
directly. The full opt-in emulator TLS matrix passed against this clean SDK
(`16 passed in 184.57 seconds`): TLS 1.2/1.3 outbound certificate controls,
inbound mutual authentication, framed read-only status and oversized-prefix
rejection. No default entropy or SDK-wide trust was enabled.

2026-10-04 read-only agent protocol slice: the native guest MessagePack codec
accepts bounded version-one hello/status requests, preserves unknown
top-level fields and emits a typed `ready`/`status` result. The host native
control codec round-trips its output in GTest. An opt-in RM-807 Dynarmic
research DLL answered a framed status request after TLS 1.2 mutual
authentication, including request ID and extension echo, and rejected a
4,097-byte advertised payload before reading it (`2 passed`, targeted guest
cases). An ARMv6 `symbian_api_agent` archive builds against the installed
SDK; the SDK exporter now includes it for both ARM profiles as
`Symbian::Agent`. The clean-export 16-case TLS matrix also passed the TLS 1.3
framed exchange and oversized-prefix control.
This is one synchronous connection in a research DLL, not a resident service,
paired production identity, active-object listener, or physical Nokia 808
result.

2026-10-04 inbound authenticated TLS research milestone: the opt-in E32
research DLL now has a manually addressed native RSocket listener that owns
one Mbed TLS server handshake. In a disposable pinned RM-807 Dynarmic
instance, local host clients completed TLS 1.2 and TLS 1.3 with server
certificate verification and a presented client certificate, then exchanged
`H`/`S`. A host that omitted its client certificate received a rejected/reset
connection for each protocol, while the guest exit reason confirmed the
handshake failed. Together with the outbound controls, the full opt-in TLS
matrix passed (`12 passed in 162.83 seconds`). The two peers use the same
self-signed local test certificate and key; this proves verification mechanics
for a fixture, not production identity, pairing or key custody. The DLL is a
one-connection research probe. Resident start/stop, active-object idle
listener, typed protocol, session grants, C++ TLS owner and physical Nokia
808 results remain open.

2026-10-04 connected native I/O deadline milestone: `TcpClient::SendFor` and
`ReceiveFor` now issue Symbian RSocket requests with an RTimer deadline,
cancel the pending native send/receive, and drain completion before releasing
caller buffers. They accept 0–60 second deadlines and return typed
deadline-exceeded statuses. A disposable RM-807 Dynarmic run held one accepted
stream open, timed out a 50 ms receive, then delivered and read the later
host byte on that same stream; two accept deadline cycles also completed
(`1 passed` targeted opt-in guest test). ESOCK `CancelRecv` and `CancelSend`
were added through preserved DEF ordinals. This proves a bounded synchronous
worker path in the emulator, not a nonblocking active-object event loop or
Nokia 808 device behavior. A fresh `.symbian/timed-tls-sdk-20261004` export
contains the new header and 15 selected ESOCK ordinals for both ARM profiles.
All four opt-in guest connectivity tests passed against it (29.00 seconds).
Strict MkDocs/two-Doxygen, Black, Ruff and Clang Format checks passed. The full
outbound/inbound TLS matrix also passed against this clean export (`12 passed
in 170.49 seconds`). The standard DLL link regression passed separately
(`1 passed, 7 opt-in skips`); no default TLS/entropy behavior was switched on.

2026-10-04 inbound native TCP milestone: `Symbian::Connectivity` now exports
move-only `TcpListener::ListenIpv4`, `Accept` and `AcceptFor` alongside
`TcpClient`. The listener binds an explicit IPv4 address with backlog one;
accepted clients share a reference-counted RSocketServ session and outlive the
listener safely on the same worker thread. `AcceptFor` uses a native RTimer,
calls `CancelAccept` on deadline, drains both requests, and maps `KErrTimedOut`
to a deadline status. An ordinary ARMv6 SDK consumer built against a staged
archive/proxy and ran in a disposable Dynarmic RM-807 instance: the host
connected to guest loopback, exchanged `Q`/`A`, and the guest then completed
two 50 ms accept timeouts on another listener (`1 passed`). Original EUSER,
ESOCK and INSOCK import ordinals are selected from preserved DEF files.
The clean `.symbian/listener-sdk-20261004` export contains the header and
13 selected ESOCK plus 79 selected EUSER proxy symbols. All four opt-in
connectivity emulator tests passed against that fresh SDK: two outbound CPU
backends, one wrong-reply control and the inbound/deadline test. The listener
is synchronous and does not yet provide an idle
active-object service or cancellable TLS reads/writes. No Nokia 808 device
compatibility follows from this emulator result.

2026-10-04 authenticated guest TLS research milestone: an opt-in E32 DLL
linked the public `Symbian::Connectivity` RSocket client to the vendored Mbed
TLS 3.4.1 archives, the checked guest UTC adapter, and a scoped RM-807 secure
random adapter. A disposable, pinned RM-807 EKA2L1 instance reached a local
Python/OpenSSL server. TLS 1.2 and TLS 1.3 each completed an authenticated
client handshake with required peer verification and exact hostname, then
exchanged one application byte. For each version, wrong-host, untrusted-chain
and expired-certificate controls failed with the expected X.509 flags:
`symbian/tests/test_mbedtls_tls_guest.py`, 8 passed in 92.75 seconds under
Dynarmic. The default SDK guest entropy callback still fails closed. This
does not establish secure entropy on a Nokia 808, a cancellable/nonblocking
TLS owner, inbound listener, resident service, or physical-device TLS.
Mbed TLS's TLS 1.3 session path directly references `time()`; the guest
archive now implements it through the same validated UTC adapter. The SDK
DLL helper also accepts `RUNTIME_TARGET` so a DLL using Abseil status targets
selects the streams runtime once rather than linking incompatible runtime
archives. A fresh `.symbian/tls-handshake-sdk-20261004` export completed for
ARMv5T and ARMv6; both installed Mbed crypto archives define the guest
`time()` adapter, and the installed CMake helper matches source. The default
Mbed DLL link regression passed (`1 passed, 7 opt-in skips`), Black/Ruff,
Clang Format and strict MkDocs/two-Doxygen builds passed.

2026-10-04 SDK native TCP client: `Symbian::Connectivity` now installs a
move-only `TcpClient` with bounded synchronous IPv4 connect/send/receive,
original ESOCK/INSOCK headers and frozen import proxies for both ARM profiles.
The fresh `.symbian/connectivity-sdk-20261004` export completed, and an
ordinary application linked the public target, converted to E32 and ran on
disposable RM-807 instances under Dynarmic and Dyncom. A local host listener
received `N`, the guest received `R`, and a changed host reply produced its
expected failure (`3 passed` opt-in Pytest). Pinned ROM/EUSER/ESOCK/INSOCK
digests remained intact. A separate EKA2L1 patch implements the missing
ESOCK `ESoRecvOneOrMoreNoLength` opcode 38 used by the original RSocket API.
This client has no in-flight cancellation, deadline or listener; it is not
the resident service transport. Guest TLS handshake and Nokia 808 device
gates remain open.

2026-10-04 outbound OpenC probe: a supplemental libc import proxy generated
from the original `libcu.def` selected `sendto` ordinal 304, then the
previously failing DLL linked afresh. A connected guest `sendto` to the
explicit loopback peer returned `ENOSYS` (78) and no host byte arrived.
`symbian/project/sdk.py` now includes that original export in future SDK
proxies, but the active SDK still needs a fresh export to contain it. The
existing guest `send`/`write` probe returned `EINVAL` (22). Outbound TCP,
authenticated TLS and development-agent gate 2 remain open.

2026-10-04 emulator TCP receive stability follow-up: a repeated guest
`WANT_READ` loop exposed duplicate `uv_read_start` submissions and one
emulator crash in the disposable diagnostic. The scoped receive patch now
arms one background read at a time. The opt-in connected receive/cancel
regression passed again (`1 passed, 7 deselected`). A proposed explicit-address
`sendto` diagnostic did not link: the active SDK's libc proxy lacks that
import, so later runs used the older DLL. No `sendto` runtime result is
claimed. Outbound I/O and authenticated TLS remain open.

2026-10-04 RM-807 emulator TCP receive experiment: a scoped EKA2L1 patch
implements `KSONonBlockingIO` for internet TCP sockets with a bounded 512 KiB
receive ring and `KErrWouldBlock` for an empty read. An opt-in E32 DLL test
using the SDK socket BIO passed on a disposable Dynarmic instance: connected
empty `recv` produced `MBEDTLS_ERR_SSL_WANT_READ`, a delayed host byte was
delivered, and a post-cancel call returned `MBEDTLS_ERR_NET_CONN_RESET`
(`1 passed, 7 deselected`). The pinned ROM and EUSER digests remained intact.
An outbound one-byte guest `send` or `write` still returned OpenC `EINVAL`
before the nonblocking option was enabled; no emulator service send request
was observed. This is a receive-only research result, not a supported TLS
transport or a guest TLS handshake. Development-agent gate 2 remains open.

2026-10-04 RM-807 emulator secure-random experiment: the SHA-256-pinned
`euser.dll` export at ordinal 2503 (`Math::RandomL(TDes8&)`) branches to its
ARM veneer for SVC `0x10A`. A scoped EKA2L1 patch now handles that service
only in the Belle v101 dispatch profile, using libuv's host OS CSPRNG and
returning `KErrNotReady` on source failure. An opt-in E32 DLL adapter invokes
the same call without EABI trap imports and clears output on failure. The real
DLL/`RLibrary` consumer passed on the disposable RM-807 instance under both
Dynarmic and Dyncom (`2 passed, 5 deselected`), checking two 32-byte outputs
and preserved ROM/EUSER digests. The default SDK archive still links its
fail-closed entropy callback. These tests do not establish phone entropy
quality, a Nokia 808 import/ABI result, connected socket behavior or an
authenticated TLS handshake. Agent gate 2 remains open.

2026-10-04 resident-agent protocol boundary: `cpp/symbian/agent` now has a native
incremental four-byte network-order length codec with a hard 64 KiB frame
ceiling. It rejects zero/oversized lengths before payload allocation, stops
at one completed frame and requires an explicit reset after malformed input.
The focused GTest passes for fragmented, adjacent, invalid and maximum-size
frames in the macOS host Debug build. A four-frame/256 KiB queue and a 4 KiB
typed MessagePack control envelope now reject over-budget and malformed input;
unknown top-level fields survive a round trip. This is host library evidence:
no guest SDK export, operation-body validation, authenticated channel, socket
listener or emulator service run has passed. Development-agent gates 2 and 3
remain open.

2026-10-04 provisional Linux host CI gate: [run `37202283905`](https://github.com/hpnkv/symbian-platform/actions/runs/37202283905) passed all four jobs on commit `23b2133`. Ubuntu 24.04 x86_64 and aarch64 built with CMake/Ninja and passed 10/10 CTests each. Separate manylinux_2_28 x86_64 and aarch64 jobs built and auditwheel-repaired CPython 3.11–3.14 wheels; all eight wheels installed in fresh environments and passed the installed-wheel audit (native import, CLI doctor, packaged resources and ELF dependency allowlist). The aarch64 wheels and Linux Python 3.14 wheels omit automatic pywebview/PySide6 because the tested manylinux baseline could not resolve that renderer. This is host core/package evidence. No interactive Linux source SDK export, EKA2L1 GUI, guest GDB, Console visual run, USB device test or Nokia 808 compatibility was established. Documentation workflow `37202283842` built and deployed successfully on the same commit.

2026-10-04 Linux aarch64 wheel gate: run `37201321281` built and auditwheel-repaired CPython 3.11, 3.12, 3.13 and 3.14 manylinux aarch64 wheels. Each installed in a fresh virtual environment and passed `python -m symbian.build_support.audit`, including native import, CLI doctor, resources and ELF dependency checks. The same run passed the Ubuntu 24.04 native build and 10/10 CTests on both aarch64 and x86_64. The aarch64 wheel excludes the unverified pywebview/PySide6 Console GUI dependency; an interactive Linux host, source SDK export, emulator and physical device remain separate gates.

The x86_64 wheel job in `37201321281` passed installed-wheel audits for CPython 3.11–3.13. Its CPython 3.14 wheel built and auditwheel repaired, but installation failed because no PySide6 wheel compatible with both Python 3.14 and manylinux_2_28 was available. The package metadata and lockfile now limit automatic Linux pywebview/PySide6 installation to x86_64 with Python 3.11–3.13; Python 3.14 retains the host CLI without a verified Console renderer. The full rerun remains pending.

2026-10-04 Linux CI dependency finding: run `37199385640` failed all four host jobs at CMake configure because neither the native isolated prefix nor the manylinux wheel prefix contained Boost's CMake package. OpenSSL and libusb bootstrap had passed. The bootstrap now hash-pins Boost 1.90.0, builds static Fiber/Context and supporting components with A11's explicit host architecture/ABI settings, and installs its license with the Python wheel. The manylinux_2_28 x86_64 image has GNU `ar` but no `llvm-ar`; a container probe confirmed its MRI `-M` mode merges an archive. The host bundle therefore uses GNU `ar` on Linux if LLVM's tool is absent. `bash -n`, `git diff --check`, local macOS CMake build and 11/11 CTests passed; the new CI run remains the Linux validation gate. No Linux native build, wheel, emulator, or Nokia 808 compatibility result is inferred from this setup fix.

Linux CI run `37199620567` built and installed the pinned Boost on Ubuntu aarch64, then CMake found Boost.Fiber but reported that its transitive Boost.Filesystem CMake package was absent from the selected bootstrap subset. The prefix now selects the same eight Boost components as A11. This run does not yet pass the native or wheel gates.

Linux CI run `37199819585` installed the complete Boost closure and configured CMake on x86_64. Its native compile then failed in three Boost-using translation units because the global native `-fno-exceptions` flag also disabled Boost.Context's required `forced_unwind` throw path. CMake now gives `-fexceptions` only to `boost_primitives.cc`, `executor.cc` and `fiber.cc`, following A11's explicit exception boundary. The local compile database confirms those three have `-fno-exceptions -fexceptions` in that order while `select.cc` retains only `-fno-exceptions`; the macOS build and 11/11 CTests passed. Linux retest is pending.

Linux CI run `37200048388` compiled the host C++ on Ubuntu aarch64 through 309 of 311 Ninja steps, then its concurrency test failed to link because `libsymbian_host_primitives.a` followed its required Abseil random archives. The imported host archive now declares those Abseil libraries as its CMake interface dependencies, which places them after the bundle in the generated link command. Local macOS build and 11/11 CTests pass; the Linux native and wheel link gates remain pending.

Linux CI run `37200345002` passed the Ubuntu 24.04 native CMake/Ninja builds and all 10 CTests on both x86_64 and aarch64. The aarch64 manylinux wheel then failed at its final `_native` shared-module link: static libusb contained a non-PIC relocation against `stderr`. The pinned libusb bootstrap now compiles with `-fPIC`, and its cache stamp was bumped. No installed-wheel audit has passed on this revision yet; host native test success is not an SDK export, emulator or device result.

Linux CI run `37200718178` again passed native CMake/Ninja and 10/10 CTests on both Ubuntu architectures. Its aarch64 CPython 3.11 wheel built and auditwheel repaired it, but installation for the wheel audit failed because PySide6 could not be resolved in the manylinux_2_28 aarch64 environment. The package now includes pywebview/PySide6 automatically only on Linux x86_64; aarch64 retains the command-line SDK without a verified Console GUI renderer. The installed-wheel audit remains pending after this packaging change.

2026-10-04 provisional Linux preparation: SDK export now discovers LLVM C/C++/LLD/archive tools from PATH or an explicit `SYMBIAN_LLVM_BIN`, preserving the selected driver filename and C/C++ toolchain pairing. The exported emulator path selects Linux `build/eka2l1/bin/eka2l1_qt`, with `gdb-multiarch` as a guest-debugger fallback. Source Run and IDE launch settings no longer inject Homebrew paths on Linux; macOS-only background-window variables are omitted there. Linux host instructions, source/emulator/debug tabs and a staged validation plan were added. Six focused host-tool/IDE tests passed locally on macOS; Black/Ruff and the strict MkDocs/two-Doxygen build passed. These checks do not establish a Linux SDK export, interactive emulator run, guest GDB session or Nokia 808 compatibility. The first local `cmake --preset debug` exposed a stale concurrency-target reference to the removed `cpp/symbian/concurrency/README.md`. Repointing its CMake `SOURCES` at `.dev/thread-a11.md` restored configure; the host native build then succeeded and all 11 CTest targets passed on macOS. Linux CI remains the next host build check.

2026-10-04 paced documentation revision: Getting started now leads through host tools, a small standalone application, build inspection and local firmware setup. The former long project, firmware, emulator, debug, runtime, Console, device and TLS guides were split into task pages and named capability/reference articles. Four cropped real CLion/IntelliJ screenshots illustrate project source, CMake profile, Run choices and debugger profiles; the stray unsaved IDE edit was excluded from the published crop and reverted in source. The strict MkDocs/two-Doxygen build and local link check pass. GitHub Actions documentation run `37198834084` completed both build and Pages deployment; the live getting-started, project, project-configuration and TLS-reference articles and CLion overview PNG each returned HTTP 200. This is a documentation result, not guest execution evidence.

2026-10-04 connected-socket prerequisite experiment: an extra guest DLL
export opened `socket(AF_INET, SOCK_STREAM, 0)` and tried to attach the
nonblocking Mbed TLS BIO. Both named RM-807 emulator backends exited `-146`:
socket creation succeeded, but `fcntl(F_SETFL, O_NONBLOCK)` failed. Their logs
reported unhandled EKA2L1 ESock base option family 1/id 4, the nonblocking
option. The temporary six-export run finished 3 passed/2 failed; the two
failures were the normal clients on Dynarmic and Dyncom. The experimental
export and test call were removed, preserving the maintained five-export
passing suite. Connected socket callbacks and TLS handshakes remain open;
no physical-device behavior is inferred from this emulator gap.

2026-10-04 IDE guide images: macOS screen capture is now permitted. Four
cropped, actual captures from the prepared `examples/gui_app` project in
IntelliJ IDEA with the CLion plugin show the indexed source/project tree, the
enabled local ARM CMake preset, saved GUI Run/GUI Debug choices and the separate
host LLDB/guest GDB profile selector. The terminal was restored to the
foreground after each capture. The CLion overview, profile and Run/Debug
articles now include these images with role-specific captions. The earlier
SVG remains available but is no longer used as a substitute for IDE captures.
These screenshots establish visible IDE configuration, not a guest run.

2026-10-04 EKA1 planning only: `.dev/eka1-plan.md` scopes a single opt-in,
no-UI process probe on one named EKA1 firmware fixture using current tools
where the ABI permits. It adds no dependency and changes no EKA1 Run behavior;
the existing EKA1 rejection remains the correct preflight result. Application
startup, emulator execution and device compatibility remain open.

2026-10-04 documentation publication: GitHub Actions run `37197271949`
passed its strict MkDocs/two-Doxygen build and Pages deployment. The published
Console and CLion articles, native SDK guide, original-platform Doxygen index
and Console image asset returned HTTP 200. MkDocs uses `.html` article URLs.
The CLion configuration image remains a labeled illustration because the owner
declined Screen Recording access for now.

2026-10-04 entropy link correction: a consumer that pulled the candidate
`mbedtls_hardware_poll` into an E32 DLL failed to link on
`TTrap::Trap(int&)` and `TTrap::UnTrap()`. The pinned EABI EUSER definition
does not export these functions. Enabling the source's exception-based trap
path would require a guest exception runtime and EHABI behavior that this SDK
does not yet verify. The SDK guest archive now links an explicit failure
callback; the `Math::RandomL` adapter remains in source as an unlinked
research candidate. A fresh source export at
`.symbian/mbedtls-safe-sdk-20261004` built and was selected as the active SDK.
Its provenance records 1,727 Mbed TLS source files and all ARM archive
digests; both ARMv5T and ARMv6 crypto archives define the failure callback.
The five-case dynamic E32 DLL suite passed on the fresh SDK, including the
failure callback through `RLibrary` on Dynarmic and Dyncom. The callback
reports `MBEDTLS_ERR_ENTROPY_SOURCE_FAILED` and zero produced bytes.
Secure guest entropy and authenticated guest TLS remain unverified, so
development-agent gate 2 remains open.

2026-10-04 documentation second pass: the former 1,142-line GUI source guide
is now an overview and seven step-oriented articles; the CLion guide is an
overview plus profile, Run/Debug and advanced articles. Two Console screenshots
were rendered from the current frontend in headless Chrome with sample paths
and no phone data; two 720×1280 counter frames came from the retained real
EKA2L1 input test. macOS denied desktop capture from this session, and the
owner chose not to enable Screen Recording now, so the CLion guide uses an
explicitly labeled configuration illustration rather than an IDE screenshot.
The SDK native API guide maps available CMake targets, headers, results and
lifetimes. Doxygen now builds separate SDK and curated original-platform
references; the latter contains nine unedited EPL-noticed SymbianSource header
snapshots and generated 827 HTML pages. The local strict MkDocs and both
Doxygen builds passed. Pages deployment of this pass is pending.

2026-10-04 guest entropy candidate: a new C++ Mbed TLS adapter calls original
`Math::RandomL(TDes8&)` under a Symbian `TRAP`, limits requests to 1024 bytes,
and clears output on a leave. The SDK exporter now includes original
`e32math.h`/`e32math.inl` and adds EUSER source ordinal 2503 to its proxy.
The first export failed because the header's `.inl` include was absent; a
retry at `.symbian/mbedtls-entropy-sdk-retry-20261004` built and was selected
temporarily. Both ARMv5T and ARMv6 crypto archives defined
`mbedtls_hardware_poll` and reference the Math import. No emulator execution,
guest entropy quality/health assessment, or Nokia 808 import check has passed;
the TLS gate remains open.

2026-10-04 documentation icon repair: the MkDocs Material emoji extension now
turns the four overview card icons into inline SVG instead of displaying their
`:material-...:` source text. The strict combined MkDocs/Doxygen build passed;
the generated overview contains four rendered icon spans and no literal
`material-rocket-launch` token. GitHub Actions run `37196505812` passed its
build and deploy jobs; the live overview contains four SVG card icons and no
literal Material icon shortcode.

2026-10-04 TLS transport follow-up: a fresh source export at
`.symbian/mbedtls-socket-sdk-20261004` is selected as the active SDK. It ships
`sdk_socket_bio.c` and its public header in the copied vendor tree and
both ARMv5T and ARMv6 `libmbedtls.a` archives define all four BIO functions.
The expanded dynamic DLL probe imports the BIO object from the three-archive
set and passed five tests on the fresh SDK, including Dynarmic and Dyncom runs
for normal and changed-digest clients. It confirms that a cancelled BIO
returns `MBEDTLS_ERR_NET_CONN_RESET` from send and receive before socket I/O.
It does not exercise a connected guest socket, in-flight cancellation, secure
guest entropy, or a guest TLS handshake. Development-agent gate 2 remains open.

2026-10-04 documentation transition: developer articles now live in lowercase
paths under `doc/docs/`, with MkDocs Material navigation and A11-matched
Noto Sans/JetBrains Mono fonts, indigo light/dark palettes, code highlighting
and navigation settings. Doxygen generates the C++ reference into the same
site. Plans, logs, research notes and other internal Markdown moved under
`.dev/`; the root README is a compact entry point. The local combined build
passed with `doc/build.sh --strict`, including link checks, MkDocs and
Doxygen/Graphviz. The public repository is
`https://github.com/hpnkv/symbian-platform`. GitHub Actions run
`37196098884` passed its build and deploy jobs. The published overview,
credits, source walkthrough and generated Doxygen index at
`https://hpnkv.github.io/symbian-platform/` each returned HTTP 200. The
README and introductory guides now explain E32, SIS, EKA2L1 and Window Server
and visibly credit the external projects. The TLS development-agent gate
remains open at step 2.

2026-10-04 Mbed TLS source ownership and trust packaging: the complete port
working tree (1,721 source/configuration/license files) now lives at
`third_party/mbedtls-symbian`, including its CMake project and required
third-party sources. Default SDK export builds that tree with no sibling
checkout dependency and installs an inspectable copy at
`source/mbedtls-symbian`. The derived, sealed active SDK is
`.symbian/mbedtls-ca-sdk-20261004` (5,302 digested files); its provenance
records the source copy SHA-256
`754a1c1101308adc572d75fa927f7fe9e854a57e940d65919083cdf835d75695`
and all six rebuilt archive hashes. Independent C consumers configured and
built on ARMv5T and ARMv6 against this SDK's architecture-matched
`MbedTLS::mbedtls` target; a plain target had no TLS link. The vendored host
suite passed 10/10, including authenticated TLS 1.2/1.3 application-data,
wrong-hostname rejection and an invalid-UTC adapter case, using host entropy
and time. The guest archive now defines `symbian_mbedtls_time` and
`mbedtls_platform_gmtime_r` through the SDK clock/libc imports. A dynamic DLL
probe on both emulator backends converted 2024-01-01 UTC, rejected a pre-2020
date and observed a clock above the plausibility floor; physical clock
correctness remains unverified.

`SYMBIAN_CA_BUNDLE` now selects only a project-local PEM file. CMake rejects
outside/missing or oversized files and exposes the packaged path; packaging
validates each certificate, rejects other PEM content, embeds the selected
bundle as an application resource, and records its SHA-256. An unset option
includes no roots. A rebuilt native boundary generated and re-inspected a
four-file SIS with the exact PEM bytes; the focused native SIS case and two
Python CA tests passed. SDK-wide trust was not changed. The historical
`rand()` guest entropy example was replaced by an explicit failure result.

The guest SHA-256 DLL test now passes five cases against the active SDK: the
declared export is retained while unused mimalloc POSIX helpers are discarded,
and a dynamic `RLibrary` client verifies the `abc` digest and changed-input
failure on Dynarmic and Dyncom. Its E32 import table lists EUSER, libc and
libpthread. The expanded guest DLL also parses an explicit test PEM trust
root, accepts its certificate for `sdk-test`, and rejects `wrong-name` and an
expired certificate on both CPU backends. A fresh active SDK at
`.symbian/mbedtls-transport-sdk-final-20261004` includes source-ordinal libc proxy
entries for these crypto dependencies and the selected socket functions.
Those socket symbols have not been exercised in the emulator. No guest TLS
handshake, verified guest entropy or physical UTC, cancellable socket owner,
emulator TLS memory
bound, or Nokia 808 connection is claimed. DEVELOPMENT_AGENT step 2 remains
open; steps 3 and later have not started. An ARM archive or host handshake
does not establish guest loader or Nokia 808 compatibility.

A new Mbed TLS socket BIO adapter makes an attached descriptor nonblocking,
returns WANT_READ/WANT_WRITE for retryable calls, and exposes atomic
cancellation that stops later callbacks. The host suite passed 12/12 with
read/write, empty-read and cancellation checks. This code was added after the
latest SDK source export; guest socket execution and SDK distribution of this
adapter require a fresh export and remain unverified.

2026-10-04 Mbed TLS SDK integration: the default exporter now builds the
local Mbed TLS 3.4.1 port for ARMv5T and ARMv6, installs architecture-specific
static CMake packages plus public headers and Apache-2.0 notice, and records
archive digests/source state in SDK provenance. A staged SDK copy was populated
from fresh Release builds: both architectures compiled all three archives and
configured/built a consumer that found the matching `MbedTLS::mbedtls` target.
The exporter helper itself subsequently rebuilt and installed both packages
in `.symbian/mbedtls-export-helper-test-20261004`; its SDK manifest loaded,
and an independent consumer compiled both targets on each architecture. The
consumer's `plain` target had no Mbed TLS dependency. This is compile
and packaging evidence; guest TLS 1.2/1.3 handshakes, entropy/time services,
certificate validation and physical-phone connections remain unverified.
The complete from-source install then passed at
`.symbian/mbedtls-default-sdk-20261004` and became the active SDK. Its
manifest, headers, license, provenance and six archive digests verified;
independent consumers compiled against both architecture packages.
An ordinary SDK copy/install also passed; a consumer found and compiled
against the relocated ARMv6 package. The from-source installation remains
selected as the active SDK.
An SDK-preparation simulation without compiler wrappers confirmed that the
exporter must pass both matching LLVM compilers; after that correction, all
three ARMv6 archives built with Clang 23.1.2 for C and C++.
Cloning a pre-TLS SDK now fails with a clear `FAILED_PRECONDITION`, requiring a
source export instead of silently producing another incomplete default bundle.

2026-10-04 emulator controls: SDK-owned macOS background emulator windows now
place the complete emulator menu bar inside the window, covering IDE Run/Debug
and automated GUI sessions. Foreground launches retain the macOS application
menu. The maintained EKA2L1 patch reverses cleanly, the patched `eka2l1_qt`
target rebuilt, and the live RM-807 GUI render/input/exit test passed on
Dynarmic and Dyncom (2/2). A live 896-by-754 window was observed through
CoreGraphics; macOS denied window image capture from this shell, so visual
confirmation of the menu remains open.

The initial technical survey is in RESEARCH.md. The owner has one Nokia 808;
its AT modem reports RM-807 and firmware revision 113.010.1508, while product
code, independent firmware verification and recovery method remain unknown.
The supplied Delight RM-807 archive provides preserved emulator ROM/Z material;
guarded disposable tests now verify real GUI pixels, pointer-driven redraws,
reset and normal zero guest/frontend exit on both macOS CPU backends.

Latest 2026-10-02 development checkpoint:

* `symbian console` now starts a wxWidgets frontend using native desktop
  controls on macOS and Windows. It has six purpose-based sidebar sections,
  rounded grouped content surfaces and higher-contrast guidance; guided views
  for all 38 public CLI workflows, USB inventory and bounded AT/MTP/PC Suite
  OBEX probes. Its FastAPI/httpx API remains in-process with no network
  listener. A live macOS wx event-loop check loaded all 38 actions and the
  connected 808; long forms measured a 903-pixel scroll extent inside a
  590-pixel viewport. The 57 focused console/CLI/device tests passed, as did
  a clean-installed macOS ARM64 wheel audit with wxPython 4.2.4. A live
  `symbian console` invocation returned in 0.12 seconds and left the wx GUI
  running. No new physical-device protocol run was claimed from this GUI
  check; see [CONSOLE.md](../doc/docs/guides/console.md).

* The native console status bar now shows a green bullet, phone name, USB IDs
  and interface profile only while a selected phone is observed. An idle
  disconnect clears the device field. Active device work and a saved USB mode
  baseline show amber pending status through temporary disconnection; a
  failed request is marked unverified until the next inventory result. Three
  state tests brought the focused console/CLI/device suite to 60 passing.

* `device info` now presents the connected 808's interfaces by USB-standard
  role and device-declared name, with endpoint counts, alternate settings,
  host driver and exact serial-port association. Its human view uses terminal
  color while redirected output remains plain; JSON retains the numeric
  descriptors. Live IOService names include MTP, PC Suite Services, SyncML,
  Haptics Bridge, UsbPnComm and LCIF. Interface 0 is still-imaging/PTP class
  labeled `MTP`; interfaces 1–2 are CDC ACM control/data, with the observed
  AT port under interface 2. The named PC Suite OBEX pair was later validated by a Connect/Disconnect
  exchange; other named channels remain descriptor evidence.
  Old mode tickets compare only stable numeric fields, so descriptive metadata
  does not manufacture a USB transition.

* `device info` now issues bounded read-only `AT`, `+GCAP`, `+CGMI`, `+CGMM`
  and `+CGMR` on the observed 808 PC Suite USB candidate's CDC ACM port;
  `--no-protocol` omits this probe. The physical phone returned `OK`,
  `+GCAP: +CGSM,+DS,+W`, Nokia 808 PureView, and revision
  `113.010.1508 2013-01-02 RM-807 (c) Nokia`. The CLI labels the parsed
  revision/date/RM as AT-reported identity. This establishes basic modem
  commands on that interface, not PC Suite protocol, OS build, phone logs or
  debugger access. The probe has fixed commands, one-second exchange limits,
  a 4 KiB response bound, and suppresses serial-like identity responses.

* Physical USB inspection now exposes exact interface descriptors, a
  serial-derived mode-stable anchor, and host serial-port paths. The owner
  switched the connected 808 from mass storage to PC Suite mode. IOService
  then showed `0421:05d1` at the same location, 18 interfaces, no mounted
  volume, and `/dev/cu.usbmodem141202`, versus prior `0421:05d0` and one
  `08/06/50` interface. The new `device mode begin/verify` workflow records a
  private baseline and checks same-serial re-enumeration; it cannot verify
  PC Suite protocol access or device debugging without a handshake. Ten
  device Pytests pass, including changed-product, impostor and Linux
  descriptor controls. A live begin/verify check correctly returned
  `unchanged`; a real ticketed transition was not captured because the
  handset was switched before the command existed.

* A fresh mimalloc-default ARMv6 `gui_app` compiled reproducibly and packaged
  with application registration and localized resources. The 17-case native
  GUI package verifier passed. The unsigned SIS SHA-256 is
  `be5ab91b933af743113dd1bba74c3edb626129cbcc04427ba3aec4a0d86ddcad`.
  One connected Nokia `808 PureView` USB descriptor had a USB-ancestor-linked
  writable FAT32 volume. The SIS copied and hash-verified at
  `Installs/gui_app-be5ab91b933a.sis`, then `disk4` ejected cleanly. The
  state is `awaiting-on-device-install`: no on-phone installer approval or
  application launch was observed. RM code, firmware and OS remain unknown.

* Mimalloc v3.5.3 is now the default guest allocator in both the standard and
  streams runtime archives of a fresh local SDK. Its pinned headers and MIT
  license ship with the SDK; the original allocator remains available by
  configuring `SYMBIAN_RUNTIME_MIMALLOC=OFF`. The separate native 64-bit
  atomic archive still uses that original allocator because the mimalloc port
  uses the portable atomic lock for generic operations and the native EUSER
  exports are not verified for every ROM. The mimalloc backend builds from pinned ignored
  sources with disconnected `RChunk` reserve/commit/decommit and pthread
  thread cleanup. The 8 MiB virtual arena option commits pages on demand; a
  cross-thread diagnostic held about 580 KiB after collection and two 512 KiB
  reuse bursts stayed within the probe's backing limits. A default 64 MiB
  total virtual reservation budget bounds this optional phone profile, and
  the guest checks its failure path. ARMv5T/ARMv6 ×
  Dyncom/Dynarmic functional guest runs passed 4/4. A warmed allocation burst
  was slower than the safe default heap on both CPU backends (Dyncom
  12,048 versus 1,595 fast-counter ticks; Dynarmic 12,058 versus 380).
  Follow-up instrumentation found only two `RChunk` reserves and three commits across
  the warmed burst, but 33,027 pthread TLS lookups. A single-thread cache
  ablation cut the mimalloc burst to 629 ticks; it was removed because it
  cannot serve multiple worker threads. Imported pthread TLS lookup is the
  measured bottleneck in this emulator profile. A fixed 256-slot native-ID
  cache now serves explicitly entered threads; collisions and unregistered
  threads use the original pthread keys. Writes always update pthread TLS.
  Main and guest worker threads have paired entry/exit; raw `RThread` stays
  uncached because this firmware skips its pthread exit destructor. The safe
  warmed burst measured 817 ticks on Dyncom and 116 on Dynarmic, versus the
  default heap's 1,595 and 380. The optional source profile passed the full
  ARMv5T/ARMv6 guest probe on both backends, including raw and managed thread
  exit, cross-thread free and bounded reuse. A fresh ignored installed SDK
  candidate linked the event executor and passed ARMv6/Dyncom and Dynarmic.
  A combined mimalloc stream-runtime and installed guest fiber image passed
  the full event, worker, native-thread and base allocation probe on both
  backends. That integration exposed and fixed tiny C++ `new` alignment.
  The new installed SDK passed the standard and event/worker guest probes on
  ARMv5T/ARMv6 and Dyncom/Dynarmic (8/8), including direct `<mimalloc.h>`
  calls, cross-thread free and managed cache cleanup. GUI rendering and input
  passed on both backends (2/2); copied-SDK and moved-project builds passed.
  Its 3,148 payload digests match. Emulator ticks establish neither phone nor
  application latency, and the memory-pressure comparison remains open.

* Cross-thread C++ destruction now frees through the allocating Symbian
  `RHeap`. Each allocation holds an `RAllocator::Open` lease until its direct
  `Free`/`Close`, so the producer may still be running or may already have
  exited. The process-owned `RChunk` page bridge likewise pins the heap that
  holds its handle metadata. An explicit private-heap `RThread` probe checks
  both directions, freeing while the producer remains alive, page close after
  producer exit, and heap-cell balance. Its normal/changed controls passed
  8/8 across ARMv5T/ARMv6 and Dyncom/Dynarmic; the event executor matrix
  passed 8/8 on the same candidate. The 3,135-file SDK digest audit, GUI
  link, generated-project SDK copy/relocation and root CTest 10/10 passed.
  The candidate remains local; no physical-device or allocator latency claim
  follows from these functional tests. The per-allocation lease and native
  heap lock still have a cost. This bridge remains the explicit
  `SYMBIAN_RUNTIME_MIMALLOC=OFF` compatibility profile.

* Guest callers can now opt into worker placement. `ThenOnWorker` posts a
  stackless transform and copies the ready result on the worker; inline
  `Then`/`OnReady` semantics remain unchanged. `PostFiber` creates a fiber
  on that worker's own scheduler. `EventExecutor::workers()` lazily provides
  one shared worker, and direct `WorkerExecutor` construction remains possible.
  The cap covers queued and active work; close is nonblocking and `Finish`
  reports drainage. Installed-SDK guest normal/changed controls passed 8/8
  across both ARM targets and emulator backends, including worker affinity,
  fiber sleep, queue saturation and asynchronous finish. The GUI linked and
  generated-project copy/relocation passed. No callback is preempted; a
  non-yielding worker task can delay other worker work, and an uncompleted
  fiber can keep `Finish` pending. This is an explicit guest placement API,
  not yet A11's shared `Post`/`PostAt` pool.
  The guest-only `future.ThenOnWorker(event_executor, transform)` member now
  selects the event owner's lazy worker; a closed executor returns a failed
  Future. The final installed-SDK candidate passed its 8/8 guest matrix,
  has 3,135 verified payload files, and passed GUI link, generated-project
  relocation, root CTest 10/10 and the pinned host source/test checks.

* Guest `thread::Case`, `PermanentEvent`, `Select` and `SelectUntil` now
  compile into `Symbian::Fibers`. A fresh installed-SDK candidate passed the
  event executor probe 8/8 across ARMv5T/ARMv6, Dyncom/Dynarmic and
  normal/changed controls. A further 8/8 run asserted event-OS-thread
  affinity for the property continuation, completion callback and waiting
  fiber. The probe includes immediate and timed selection,
  worker notification, competing events and repeated timeout cleanup. The
  GUI linked against the candidate, and the generated-project initial-build,
  copied-SDK and moved-project check passed. Continuations run on the event
  OS thread without a worker context switch; fiber ready snapshots now
  reuse storage across yields. There is no enforced short-work limit:
  compute-bound callbacks can indefinitely delay native service, despite the
  per-source count budget. Selectable channel cases, rendezvous, cancellation
  cases and full A11 tests remain open. The candidate is not yet the visible
  SDK, and these are emulator results only. Its 3,133 payload digests verify;
  root CTest passed 10/10 and the three original A11 host executables passed.

* The first shared guest event owner now dispatches RTimer, real
  RProperty::Subscribe results, the bounded event mailbox and ready fibers
  before its sole native request wait. Fiber sleep arms an RTimer through that
  owner; event-affinity dispatch has an explicit operation. A structured
  timer/property owner forwards cancellation and an absolute deadline, then
  completes its asynchronous join after child and deadline-alarm drainage.
  The GUI and generated timer-task starter use the event owner. An
  installed-SDK candidate exported both headers, and both applications
  compiled against it. The GUI's rendered input/reset/normal-exit test passed
  on Dyncom and Dynarmic.
  A dedicated guest normal/changed-result probe passed 8/8 across ARMv5T/
  ARMv6 and both backends; it checked a real property value, timer, fiber
  Await/sleep, cancellation, repeated asynchronous TaskGroup Finish, the
  structured owner's success/close/timeout/error and destruction-before-drain
  paths, cross-thread event dispatch and heap-cell balance. This remains a
  bounded event executor, not full A11 `thread::` parity: native request
  ownership is still split between the existing adapters, selectable channel
  cases, A11 fiber trees/pools are open, and no phone result is claimed.

* The SDK exception policy now follows A11: native targets default to
  `-fno-exceptions`, with explicitly selected implementation translation units
  allowed to enable exceptions. The pinned, unmodified full A11 host
  `cpp/thread` compiled with that boundary, and its three original host test
  executables passed in an isolated build. The maintained
  `full_host_probe/CMakeLists.txt` reproduces this check. The staged SDK host
  archive still needs a deliberate replacement; this result does not claim
  that all A11 host APIs ship in the installed SDK.

* Host `thread::` now includes an A11-derived custom ready-fiber scheduler,
  idle-park guard, the original MPMC work queue, shared stackless `Post`/
  `PostAt`, and original `PermanentEvent`/`Select` with wait diagnostics still
  omitted. The Python boundary installs A11's CPython GIL park pair and
  deferred-reference discipline; a bounded Future-to-asyncio bridge resolves
  on the captured event loop. Native tests passed policy ordering, lock/GIL
  park balance, pool callback/timer and Select controls; an extracted macOS
  wheel passed concurrent Python progress, asyncio completion/cancellation and
  deferred-reference drainage. The installed SDK's Boost-free host consumer
  passed its expanded Post/Select test. A11 fiber trees, pool work stealing,
  Submit/Schedule, channel selection, introspection and complete shutdown
  remain open, so this is not a compatible full A11 host port.

* Repository and generated-project C++ formatting now uses clang-format 23's
  `InsertBraces` rule. The starter contains its own `.clang-format`; 170
  first-party C++ files passed a formatter idempotence check after 47 owned
  files were updated. The two user-owned edits were preserved. The selected
  visible SDK was refreshed with the generator, formatted starter and CMake
  target closure; 3,123 payload digests verify. Its initial-build, copied-SDK
  and moved-project test passed. Root CTest passed 10/10, and the built macOS
  wheel contains the template dotfile. The initial installed starter build
  exposed a missing guest fiber archive link from `Symbian::Stackless`; the
  target now supplies that archive when installed.

* A bounded guest `thread::Fiber` now runs on the verified ARM/Thumb switch,
  pinned to one OS thread and a 16 KiB stack. `thread::Mutex` contention and
  `thread::CondVar` signal/timeout waits park a fiber; `thread::SleepFor` and
  an unresolved `Future::Await` do likewise. Outside a fiber, unresolved
  guest Await fails clearly. `thread::SchedulerPolicy` provides custom
  ready ordering and a cross-thread wake callback outside internal locks.
  A normal/changed control covering move-only work, lock contention,
  cross-thread signal, timeout, LIFO policy, Future await and heap balance
  passed 8/8 on ARMv5T/ARMv6 × Dyncom/Dynarmic. A fresh SDK export builds
  a separate `Symbian::Fibers` archive for each architecture; installed-archive
  execution passed 8/8, and the updated visible SDK passed the same 8/8
  matrix against its existing Abseil archives. Event-owner integration,
  A11 Select/pool/tree,
  cancellation and joining remain open.

* Host and guest now share one portable A11-derived channel, Future/Task,
  TaskGroup and mailbox source layer. The host CMake target
  `symbian::concurrency` selects an opaque `thread::` primitive ABI; its
  Boost.Fiber/Context implementation is bundled inside one SDK static
  archive. An ordinary consumer has no Boost headers or separate Boost link
  dependency; Boost is needed only to rebuild that archive. The host's bounded
  `thread::Fiber` handles move-only
  work, same-thread join, cooperative cancellation and normal C++ cleanup;
  call sites use `thread::Fiber`, not Boost types. The SDK export includes the
  host archive and Boost-free headers. An installed host consumer compiled,
  linked and ran with Boost discovery disabled; its link command named no
  Boost library. Root CTest passed 10/10. An
  earlier no-Boost fallback test passed before that incomplete branch was
  removed. The updated installed-SDK
  guest stackless/channel/timer matrix passed 16/16 across ARMv5T/ARMv6 and
  Dyncom/Dynarmic. Guest `CondVar` now matches A11: true means timeout.
  A fresh workspace export passed four guest timer/Future cases and an SDK
  copy/build check. That earlier export had 3,113 verified payload digests.
  The new visible SDK verifies 3,122 payload digests; its prior tree is
  preserved at `~/dev/symbian-sdk-before-fibers-20261002`.
  A11 tree/Select/pool semantics and comprehensive guest fiber lifetime remain open.

* Guest synchronization and channel headers now keep A11's `thread::`
  namespace: `<thread/boost_primitives.h>` and `<thread/channel.h>`.
  The shared channel backend supports bounded FIFO `Channel<T>` with
  `reader()`/`writer()`, move-only values, blocking worker reads/writes,
  nonblocking Abseil-status operations, close wakeups and drainage.
  `EventMailbox` uses the nonblocking channel path. Original pinned libc++
  condition-variable destructor support was added to both runtime archives.
  A timed-wait control exposed an early `no_timeout` return without a signal;
  the adapter now validates signal generation and uses monotonic bounded
  waiting. The installed normal/changed guest stackless and timer/channel
  matrix passed 16/16 on ARMv5T/ARMv6 and Dyncom/Dynarmic. The fresh visible
  SDK had 3,113 verified payload files after the CLI refresh; the prior
  3,110-file SDK is preserved at
  `~/dev/symbian-sdk-before-channel-20261002`. Full A11 `Select`, fiber lifecycle,
  zero-capacity rendezvous and exception-throwing closed writes remain open.

* CLI inspection and workflow commands now default to human-readable
  summaries, with TTY-aware color and readable byte sizes. `--output-format=json`
  preserves the canonical schema for scripts before or after nested commands.
  Root and nested help describe every command and option. Long result lists
  use separate numbered lines. The live `device list` output showed the
  connected Nokia USB and mounted storage without its serial;
  44 CLI/device/control/SDK tests and the selected-SDK project regression
  passed. A rebuilt, separately installed macOS wheel passed its resource,
  native-dependency and CLI audit. The unrelated older package fixture still
  fails at the ARM exception-descriptor gate before CLI output is reached.

* Generated applications select an SDK through an ignored local
  `sdk-location.json` containing only a path, with no SDK digest lock.
  A two-way switch between the visible SDK and a second installation passed
  through `symbian app build`: compiler identity and
  `compile_commands.json` followed each selection. A direct CMake/Ninja
  control initially did no work after the path changed; generated
  `sdk.cmake` now declares the location and preference files as configure
  dependencies. Direct builds then reconfigured, rebuilt and selected the
  new compiler in both directions. The GUI example's 19 KiB source/digest
  manifest moved from its application folder to
  `research/gui_app/source-profile.json`, where SDK source preparation
  actually needs it. Synthetic staging checks passed 12/12; preparation
  against the pinned original source tree succeeded, and the current
  six-DLL GUI link test passed. The visible SDK template was refreshed and
  its digest manifest resealed. The generated-project SDK-switch regression
  passed with the prepared visible SDK (1/1). A full prepared-workspace
  export using the moved profile succeeded and all 3,109 payload digests
  verified; the visible SDK remained the global default.

* Localized menu resources and project SVG icons now build through ordinary
  `[application]` settings. The host policy maps BCP 47 tags to selected
  `TLanguage` IDs and invokes original `rcomp` with both Unicode output and
  UTF-8 source interpretation. Native SIS writing/inspection accepts a bounded,
  canonical resource set and converts an SVG to a same-host deterministic gzip-backed
  MIF. The maintained `gui_app` packages fallback/French/German/Japanese
  captions and one icon as seven files. On both CPU backends, EKA2L1's original
  installer installed/reloaded/removed them, AppArc selected all three
  translations and decoded `Zähler` and `カウンター`, and its original MIF
  reader recovered the SVG
  (8/8 headless tests). The separate native SIS suite passed 9/9 and the
  resource Pytests passed 2/2. These headless cases execute no guest
  instructions; physical Belle SVG-in-MIF rendering has not been observed.
  The physical `menu_v5` observation below predates this icon/locale change.
  A fresh 3,109-file SDK export passed digest audit and its own ARMv6
  init/build/package smoke. After promotion, the visible SDK itself completed
  ARMv6 and ARMv5T init/build/package checks; its ARMv6 sample included
  French and Japanese menu translations. The usual `~/dev/symbian-sdk` path
  was updated from that tested export and rebased; the previous tree remains
  at `~/dev/symbian-sdk-before-icon-locale-20261002`. The macOS wheel's
  installed dependency/resource audit also passed with the icon template
  and system zlib dependency.

* Application-menu packaging now uses the original EPL-licensed Symbian
  `rcomp` at pinned revision `d3c2eadd`, with a replayable 64-bit host patch
  and an SDK UID-checksum companion. A prepared-workspace SDK export contains
  both tools, genuine `AppInfo.rh`, the EPL notice and 3,108 digest-valid
  files. `[application]` gives `caption` and `short_caption` directly in
  `symbian.toml`; `symbian init` generates defaults, and `gui_app` specifies
  its own. Native SIS writing/inspection verifies the EXE, registration and
  caption resources, their hashes, UIDs and same-drive install paths.
  Eight native SIS tests, a real-resource Pytest, fresh ARMv5T and ARMv6
  init/build/package checks and `gui_app` packaging passed. An original
  EKA2L1 AppArc parser check exposed that `rcomp` needed its `-u` Unicode
  mode: the earlier byte-text SIS installed but did not decode its menu
  captions. The corrected resources parsed as `Counter` and `Symbian GUI
  Counter` on both backends. The older E32-probe packaging
  fixture still fails at its separate exception-descriptor gate.
  The 3,108-file export was rebased into `~/dev/symbian-sdk`, with the
  prior digest-valid tree retained at
  `~/dev/symbian-sdk-before-menu-registration-20261002`. The visible SDK
  repackaged `gui_app` with Unicode resources. EKA2L1's original installer
  accepted all three files, parsed their menu text, reloaded the package registry and removed and
  reinstalled the resources on Dyncom/Dynarmic (8/8 headless tests).
  The visible SDK's full GUI package verifier passed 17 image/checksum and
  installer cases with the current six-DLL/153-slot executable; it still
  records zero executed guest instructions. The separate on-phone result below
  is user-reported, not inferred from that verifier.

* Physical-device groundwork now has `symbian device list`, `info` and a
  build/package/USB-mass-storage staging flow. IOService associated the
  connected `808 PureView` USB descriptor with a writable mounted S60 disk;
  the Mac USB system profiler alone had returned no devices. The public
  selector hashes the serial, and RM code, firmware and phone runtime remain
  unknown. Eight synthetic discovery/transfer/policy/Linux/low-space controls
  passed. A real generated app in `~/dev/symbian-app-3` rebuilt reproducibly,
  and its unsigned SIS passed a host-only staging/hash check. After the owner
  confirmed the photos were copied, an ARMv5T portable generated starter
  with Unicode menu resources was copied to the phone's `Installs` folder.
  The copied SIS SHA-256 is
  `db4875221d6ecabbe02bfba76c4201c5da5f8f0df298ae50f7f5050356e70db1`;
  `disk4` ejected normally. The owner then reported that the handset installer
  succeeded, `menu_v5` appeared in the application menu, and the app opened
  and responded to a tap. This is direct user observation, not an SDK-read
  installer registry, captured device log or instrumented launch trace. A
  direct installer transport and real Linux-connected phone remain untested.
  `examples/gui_app`'s standalone reproducibility check still returns
  `DATA_LOSS` for differing CMake ELF/E32 builds, an independent open issue.

* `examples/gui_app` now links the installed `Symbian::Stackless`/guest Abseil
  profile through a narrow modern C++ bridge. A 300-ms timer Future lights a
  separate marker after an increment; Reset cancels pending work. The existing
  counter, drawing and owner's `app.cc` edits remain intact. The same event
  thread consumes Window Server and timer completions. The final candidate
  built reproducibly, both root and standalone CMake targets linked, and the
  expanded pixel/marker/cancellation/exit GUI control passed on ARMv5T and
  ARMv6 × Dynarmic and Dyncom (4/4). The root `gui_app_e32` publisher also
  rebuilt under Apple Clang, and its ARMv6 image passed both backends after
  SDK promotion. `symbian init` now defaults to Abseil Status/StatusOr and
  timer-backed Tasks, with a portable option and automatic portable selection
  when `--firmware` names a Z drive without `libpthread.dll`. A final-candidate
  CLI initial-build, relocated SDK/project and two-backend Task/cancellation/
  rapid-exit control passed on ARMv5T and ARMv6 × Dynarmic and Dyncom (4/4);
  the full generated-project suite passed 17/17 before the additional ARMv5T
  cases passed 2/2. A preceding candidate passed real starter
  builds and GUI execution on C7, E6, 6120 and E71 (4/4); the latter two used
  the portable profile. A deliberately modern executable against E71 now
  returns canonical `FAILED_PRECONDITION` before emulator startup. Root CTest
  passed 8/8. The digest-valid 3,100-file candidate was rebased into the
  visible `~/dev/symbian-sdk`; the prior 3,100-file tree remains at
  `~/dev/symbian-sdk-before-gui-init-migration-20261002`, also digest-valid.
  The promoted SDK completed a new default `symbian init` build. Its GUI
  publisher passed again on both ARM targets and both emulator backends.
  These are application-path results; A11 fibers and genuine
  `thread::` primitives remain open.

* The first C3 backend prerequisite now lives in the runtime archive, with an
  ARM/Thumb context swap preserving callee-saved registers across a bounded
  16 KiB heap stack. A C++ probe retains heap-backed string/unique ownership
  across two switches, destroys both normally and checks heap-cell balance.
  Installed normal/changed execution passed 8/8 on ARMv5T/ARMv6 ×
  Dyncom/Dynarmic. This is raw context switching only: stack guards,
  floating-point context, native TRAP safety, fiber scheduling, joining and
  genuine A11 `thread::Mutex`/`CondVar`/`PermanentEvent`/`SleepFor` remain
  unimplemented. The sealed candidate was promoted into the visible SDK;
  ARMv6/Dynarmic normal/changed controls passed again after promotion. The
  prior sealed installation is preserved at
  `~/dev/symbian-sdk-before-fiber-swap-20261002`.

* A second native request owner now runs `RProperty::Subscribe` through a
  typed `Future<int>` with SDK-owned status, result storage, handle, native
  cancellation and close-time drainage. It shares the timer pump's sole
  event-thread semaphore consumer. An `EventMailbox` bounds cross-thread
  dispatch and executes callbacks outside its lock; the generated timer GUI
  uses it. The installed candidate passed 8/8 timer/property/mailbox normal
  and changed controls on ARMv5T/ARMv6 × Dyncom/Dynarmic. The generated GUI
  timer and Status controls passed 5/5 against the candidate and 6/6 including
  a copied Abseil project against the 3,100-file promoted visible SDK.
  Absolute timer deadlines now take real-world `absl::Time`, convert once at
  registration and wait monotonically; a simulated post-registration wall
  jump, 24-hour admission/cancellation, infinity and native slice boundaries
  passed. Actual wall-clock adjustment and full-length rearm are not tested.
  A subsequent export adds an `Await` guard: ready results work and an
  unresolved `Await` returns `FailedPrecondition`; its stackless matrix passed
  8/8 and the visible SDK was updated. Its ARMv6/Dynarmic normal/changed
  cases passed again after promotion. A post-promotion timer/property matrix
  passed 8/8 with an added pending
  subscription-close/drain control. Full A11 `thread::` mutex,
  condition variable, permanent event and sleep semantics require the C3
  fiber backend; no OS-thread-only lookalike is published.

* A sealed development SDK candidate at
  `.symbian/abseil-direct-status-sdk-20261002` now publishes the pinned guest
  Abseil types directly through `symbian::concurrency`: Future/Promise/Task
  results are `absl::StatusOr<T>`, failures are `absl::Status`, and the earlier
  name-shortening aliases are gone. Installed-SDK stackless and timer controls
  each passed 8/8 on ARMv5T/ARMv6 × Dyncom/Dynarmic, including changed-result
  exits. At that checkpoint `TimerPump` used `absl::Duration` with a
  provisional monotonic-deadline wrapper; the later export above changed
  absolute deadlines to `absl::Time`. The
  Status-enabled generated GUI passed both backends, and its injected model
  failure reached the native exit path (3 tests). The pinned Abseil
  Status/StatusOr/Cord/map/time installed contract passed 9/9. Cross-thread
  closing of an SDK-owned page source and the original Abseil allocator passed
  normal/changed controls on the preceding candidate. Root ARM IDE CMake now
  has explicit targets for both Abseil probes; ARMv5T/ARMv6 configure and
  object compilation passed. General A11 scheduler/fibers, OS TLS and broader
  Abseil remain open. That candidate was promoted later as recorded above.

* Genuine A11-pinned guest Abseil `Status`, `StatusOr`, `Cord` payloads and
  `flat_hash_map<std::string, int>` now execute on RM-807. The normal and
  deliberately changed-result contract passed 8/8 from replayed source and
  8/8 through the installed `Symbian::AbseilStatusOr` target on
  ARMv5T/ARMv6 × Dyncom/Dynarmic. The visible SDK contains 384 Abseil
  headers and 43 compiled closure archives per architecture, plus exact source
  revision, three replayable patch digests, Apache-2.0 license and imported
  CMake target. A copied project built and converted to E32 using only the
  installed SDK for target inputs. The 3,097-file export is installed at
  `~/dev/symbian-sdk`; its verified 2,597-file predecessor is retained at
  `~/dev/symbian-sdk-before-guest-abseil-20261002`. The wider selected
  runtime regression passed 24 cases with one skip. The stream profile now
  supports the
  necessary 16-bit-wide libc++ surface and selected real OpenC wide/stdio
  functions; SDK adapters provide `nan`, `nanf` and `ldexpl` where no frozen
  export exists. Byte and bitwise atomics use real EUSER operations, while
  Abseil's table-seed TLS is adapted to a process-wide atomic sequence.
  This is a tested Status/StatusOr/map subset, not every Abseil component,
  general ELF TLS, cross-thread page release or A11 fibers.

* The pinned A11 Abseil `LowLevelAlloc` now links from only two ordered,
  replayable source patches and executes on the RM-807 Dynarmic guest. Its
  original allocator and arena code allocate a 130,000-byte block through an
  SDK-owned process `RChunk` page source; an arena with an outstanding block
  refuses deletion, then closes after that block is freed. A maintained
  opt-in guest test passed the normal and deliberately changed-result cases
  (2/2) against a fresh 2,597-file SDK export and again against the visible
  installed SDK. Both patches apply to the
  pristine A11 pin and reverse-check from the replay checkout. Runtime exits
  now use named internal reasons with compile-time `KErrNoMemory` and
  `KErrArgument` checks. The related installed-SDK runtime exit/clock/lifecycle
  selection passed 53 guest cases; root CTest passed 8/8. The 2,597-file SDK
  was promoted to `~/dev/symbian-sdk`, with the verified 2,596-file predecessor
  at `~/dev/symbian-sdk-before-closure-final-20261002`. A copied-project
  test passed before and after promotion. Cross-thread page release, memory
  pressure and A11 fibers remained open at that checkpoint; the newer
  Status/StatusOr result is recorded above.

* The runtime now has an owned `RChunk` page bridge with validated page size,
  page-aligned process-owned allocation, explicit handle release and a heap-cell
  balance check. ARMv5T/ARMv6 × Dyncom/Dynarmic normal/changed controls
  passed, as did
  a separate `pthread` TLS-key test for worker isolation and its exit
  destructor. EUSER byte exchange and OpenC `strtol`, `strcpy`, `strcmp`,
  `sysconf` execution controls passed on ARMv6/Dynarmic. This is page sourcing
  and selected services, not Abseil LowLevelAlloc or general ELF TLS. A
  replayable Abseil patch makes `GetCachedTID()` use its existing
  `pthread_self()` fallback on Symbian; the isolated relink no longer reports
  `__tls_get_addr`. Other allocator, synchronization and C-service undefined
  symbols remained at that checkpoint. The selected 2,597-file export
  passed its allocator test and was promoted as recorded above.

* LLVM's original ARM soft-double compiler-rt implementation now runs under
  the SDK's ARMv5T and ARMv6 profiles. An ordered, replayable LLVM patch
  changes four ARMv6T2-only `movw` constant loads to literal loads and one
  `bfc` to ARMv5-safe shifts; original helper sources complete its dependency
  closure. Actual guest double arithmetic, comparisons, NaN and conversions
  passed 8/8 normal/changed
  source-built and 8/8 installed-candidate ARMv5T/ARMv6 × Dyncom/Dynarmic
  controls. The candidate's copied-project test and root CTest 8/8 passed.
  The 2,596-file SDK was promoted; its verified predecessor is retained at
  `~/dev/symbian-sdk-before-softfloat-20261002`. This removes all floating-point
  compiler ABI undefined symbols from the isolated pinned Abseil link, but
  allocator, TLS, synchronization and C/POSIX services still block genuine
  guest Status/StatusOr execution.

* An integration control now uses A11-derived `TaskGroup` over real
  `TimerPump` requests: joining two timers succeeds only after native
  completion; cancelling an aggregate of three timers forwards cancellation
  to each request and settles after event-thread drainage. ARMv6/Dynarmic
  normal and changed-result source cases passed (2/2), followed by all 8/8
  ARMv5T/ARMv6 × Dyncom/Dynarmic source controls and 8/8 installed-SDK
  controls. This is stackless structured composition for timers, not a fiber
  backend.

* `TimerPump` now limits simultaneously pending native timer Tasks (64 by
  default, configurable). Saturation returns a ready Task with
  `kResourceExhausted` before allocating an OS timer; a slot is reused after
  event-thread completion. A two-slot saturation/reuse/close and heap-cell
  control passed 8/8 source-built and 8/8 installed-candidate
  ARMv5T/ARMv6 × Dyncom/Dynarmic normal and changed-result executions.
  The candidate contains 2,596 digest-verified files. This bounds one native
  request type, not all A11 continuations or native I/O queues. Guest Abseil
  Status/time migration remains open. The candidate's copied-project and
  two-backend GUI tests passed (3 tests), root CTest passed 8/8, and the
  candidate was promoted to `~/dev/symbian-sdk`. Its 2,596-file predecessor
  remains at `~/dev/symbian-sdk-before-timer-cap-20261002`; both verify by
  digest. Post-promotion ARMv6/Dynarmic normal and changed-result timer
  controls passed (2 tests), as did the canonical copied-project test.

* Generated applications now have an opt-in `SYMBIAN_ENABLE_TIMER_TASKS`
  profile. It links `Symbian::Stackless` and uses one event-thread wait for
  Window Server input/redraw and A11-derived timer Tasks. A tap logs
  immediately and schedules a delayed log; Clear cancels pending Tasks.
  Actual RM-807 GUI controls passed on Dynarmic and Dyncom, including delayed
  completion, cancellation and normal Exit. The default GUI controls passed
  on both backends, and a default E71 starter built and executed. The selected
  DRTAEABI proxy now includes the real `__cxa_pure_virtual` ordinal needed
  by this link. The 2,596-file candidate was installed at
  `~/dev/symbian-sdk`; the previous sealed version is retained at
  `~/dev/symbian-sdk-before-window-timer-20261002`. Both 2,596-file seals
  verify; canonical copied-project and two-backend opt-in GUI tests passed
  (3 tests), as did root CTest 8/8. This establishes bounded
  shared-loop integration, not complete C1/C2, guest Abseil or fibers.
  `std::chrono` is provisional in the public timer API: once guest Abseil
  time is executable, SDK public time utilities should use Abseil types.

Earlier 2026-10-01 checkpoints:

* The first native-to-A11 stackless completion path now executes through
  `symbian::concurrency::TimerPump`. One event OS thread owns the native
  `RTimer` requests and publishes `Task` results; a worker can request
  cancellation through a coalesced `RThread::RequestSignal()` wakeup without
  touching thread-relative timer handles. A bounded guest control verifies
  parked cross-thread cancellation, monotonic `ScheduleAt`, overlapping
  timers, continuation reentry, immediate `OnReady`, close with a pending
  timer and heap-cell balance. Normal/changed controls passed eight source
  cases before a final continuation check, and the formatted source smoke
  plus eight installed SDK cases passed after it on ARMv5T/ARMv6 ×
  Dyncom/Dynarmic. The final candidate and canonical copied-project tests
  passed, as did a canonical ARMv6/Dynarmic smoke, root CTest 8/8 and both
  ARM IDE index builds. The 2,596-file canonical SDK and its 2,595-file
  backup verify by digest. This is not yet a shared Window Server event pump,
  arbitrary native I/O adapter, full A11 scheduler, or fiber backend.

* A bounded native `RTimer` owner now lives behind a narrow original-SDK
  bridge and a modern `symbian::concurrency::NativeTimer` interface. It creates
  a thread-relative handle, rejects negative and overlapping arms before the
  OS can panic, cancels and drains a pending request on close, and retains its
  status storage until completion. A probe exercises three simultaneous
  timers, completion before waiting, cancellation, close during a pending
  request, rearm and heap-cell balance. Normal and deliberately changed-result
  cases passed eight source-built and eight installed-SDK ARMv5T/ARMv6 ×
  Dyncom/Dynarmic executions. The formatted final export passed another
  eight installed cases; its copied/relocated project test and canonical
  ARMv6/Dynarmic smoke test passed. Root CTest passed 8/8, both ARM IDE probe
  targets built, and the 2,595-file canonical SDK and its 2,594-file backup
  verify by digest. That checkpoint established the C1 request-owner slice;
  shared Window Server pumping and broader C1 controls remain open.

* The guest monotonic clock now has concurrent execution controls: two real
  `std::thread` readers take 2,048 samples each, then exchange 128 ordered
  timestamps, with no net Symbian heap-cell increase. Normal and changed
  controls for this clock and the existing thread path passed 16 source and
  16 installed-SDK ARMv5T/ARMv6 × Dyncom/Dynarmic cases. The combined link
  exposed an obsolete `__throw_system_error` fallback, now retired in favor
  of original libc++ `system_error.cpp`; `std::this_thread::yield()` required
  the real `sched_yield` libc import. The previous error-category probe passed
  eight source cases. An invalid `std::thread::join()` exited with -6 in four
  source and four installed negative controls, confirming the default fatal
  no-exceptions path. The new SDK candidate has 2,594 digest-valid files;
  an E71 generated starter built and executed, copied/relocated project tests
  passed before and after promotion, a canonical ARMv6/Dynarmic clock-thread
  smoke test passed, root CTest passed 8/8, and both ARM IDE probe builds
  include the new source. It was installed at `~/dev/symbian-sdk`; the prior
  sealed SDK remains at `~/dev/symbian-sdk-before-clock-thread-20261001`.
  A shared timer/Window Server pump, suspension and half-wrap clock
  continuity remain open C1 gates.

* The guest now executes original libc++ `steady_clock` and `system_clock`.
  RM-807's OpenC `CLOCK_MONOTONIC` returned `EINVAL`; a maintained Symbian-only
  LLVM patch reads `NTickCount` and its HAL period for monotonic time, with an
  ordinary-tick fallback. Original Symbian source recommends `FastCounter`
  for short profiling and `NTickCount` for production. Two ordered EKA2L1
  patches expose the nanokernel/fast-counter HAL rates and correct the
  emulator's FastCounter to its advertised 32,768 Hz. Positive and changed
  guest controls passed 16 ARMv5T/ARMv6 × Dyncom/Dynarmic cases from source
  and 16 against the sealed installed-SDK candidate. The generated E71
  starter also built and executed with the new optional proxy catalog, and
  the copied/relocated SDK project test passed both before and after
  promotion. The converter now accepts an
  actual needed-proxy subset while retaining identity and duplicate checks;
  its import suite passed 11 tests with one opt-in skip. Root CTest passed
  8/8; both ARM IDE probe targets build and index the two new clock sources.
  The 2,594-file sealed SDK was installed at `~/dev/symbian-sdk` with its prior
  2,591-file tree at `~/dev/symbian-sdk-before-clock-20261001`; both digest
  sets verify. Concurrent clock reads, suspension and a gap of half a 32-bit
  tick wrap (about 24.9 days at 1 ms) remain C1 gates.

* The prior root IDE checkpoint exposed all 48 then-known sources across
  platform-targeted probe projects through per-project ARM CMake targets,
  including the C++20 module and the locally prepared Mbed TLS probe.
  Separate ARMv6 and ARMv5T
  guest-index presets built successfully; both `compile_commands.json` files
  cover 48/48 sources with the correct target triple. The Mbed TLS source and
  SDK header paths are local opt-in settings, not shared repository paths.
  The local ARMv6 CLion preset uses LLVM 23 and
  was enabled alongside the existing host Debug profile, preserving a backup
  of the prior workspace settings. The running IDE log records a successful
  `clion-guest-probes-armv6` CMake generation (exit 0) after the profile was
  enabled. Actual editor diagnostics/header navigation remain a UI check;
  terminal compile database evidence is separate.
* A bounded classic-C locale and stream runtime is now a maintained,
  installable `Symbian::Streams` profile. Original LLVM libc++ locale/ios/
  ostream/iostream/strstream sources, SDK C-locale adapters and real selected
  OpenC imports produce `std::ostringstream` output in a guest. C and POSIX
  locale names work; invalid adapter inputs return `EINVAL`. Normal and
  changed-result controls passed eight ARMv5T/ARMv6 × Dyncom/Dynarmic cases
  from source and another eight from a fresh installed SDK. The visible
  SDK has 2,535 digest-valid files; its verified predecessor remains at
  `~/dev/symbian-sdk-before-streams-20261001`. This alternate archive uses
  its own libc++ configuration and replaces `Symbian::Runtime` in a target.
  File streams, wide strings, arbitrary locales and executable guest Abseil
  Status/StatusOr remain open. The pinned Abseil build is still an isolated
  diagnostic, not a shipped guest library. An isolated pinned
  `absl_statusor` archive now cross-compiles with experimental Symbian/Fuchsia
  platform guards and wide declarations, but guest linking exposes absent
  low-level mapped allocation, 64-bit atomics, TLS, soft-float builtins and
  further C/POSIX services. No Status execution or full A11 C1–C3 claim follows.
  A narrow native `RChunk` bridge then passed 16 source and installed-SDK
  normal/changed-result executions across both ARM targets and CPU backends:
  query native page size, create, write, grow, check retained bytes and close
  16 local chunks. This is a candidate page-source prerequisite, not an
  Abseil allocator or a native
  request/fiber backend.
* Imported **function** pointers now cross the ELF/E32 boundary in a bounded
  form. The converter validates ARM PLT identity, the dynamic `R_ARM_ABS32`
  record, zero-initialized writable storage and its retained relocation,
  then writes an E32-relocatable PLT pointer. A global EUSER `memmove`
  pointer executed on ARMv5T/ARMv6 × Dyncom/Dynarmic from source and against
  the existing installed SDK runtime/proxy (eight executions). Two mutated
  ELF controls reject a data-object relocation and false PLT symbol value.
  Imported **data objects** remain unsupported; typed guest exceptions still
  need that distinct ABI support. The earlier isolated LLVM libc++ classic
  locale/stream prototype ran on both backends before its promotion above.
  Its first image exposed an unrelocated LLD
  interworking thunk and failed in the guest. The converter now relocates
  LLD's exact named eight-byte ARM long-branch thunk; the original stream
  image then exited 0 on both CPU backends. A maintained thunk call passed
  all four architecture/backend cases, while a mutated out-of-range target
  was rejected. A fresh visible SDK passed the combined 11-case
  function-pointer/thunk matrix, was promoted after digest audits, and now
  has 2,503 sealed files. The previous 2,503-file SDK is retained at
  `~/dev/symbian-sdk-before-import-pointers-20261001` and also verifies. A
  post-promotion ARMv6/Dyncom imported-pointer execution also passed through
  the canonical SDK manifest.
  Other linker-generated absolute thunk forms remain open.
  An isolated pinned-Abseil `absl_status` build with the experimental stream
  profile progressed further but still fails on Linux `link.h` assumptions,
  disabled wide strings and unavailable file streams. No guest Abseil archive
  or A11 native request/fiber backend is claimed; the compiler log is retained
  under `.symbian/abseil-guest-probe/`.
* Clang-compatible guest varargs and original LLVM libc++ error categories
  now execute on the named firmware fixture. The SDK-owned `stdarg_e.h`
  bridges OpenC's header to Clang's ARM EABI `va_list`; a real `libc.dll`
  `vsnprintf` formats register, stacked and 64-bit arguments. The guest
  archive builds original `error_category.cpp` and `system_error.cpp`, and
  `std::error_code` category/message/condition checks use actual libc imports.
  Their first PIC image was correctly rejected for cross-mapping code/data
  references; compiling those originals without PIC uses the existing E32
  data relocation. Normal and changed-result controls passed 16 source-tree
  ARMv5T/ARMv6 × Dyncom/Dynarmic cases, and another 16 against a fresh
  installed SDK. A copied-SDK application build passed. The visible SDK was
  refreshed with 2,503 digest-valid files; the previous 2,502-file tree is
  retained at `~/dev/symbian-sdk-before-runtime-c-services-20261001`.
  The promoted canonical SDK passed four ARMv6/Dyncom normal and
  changed-result controls for both capabilities. Root CTest passed 8/8;
  Black, Ruff, clang-format and `git diff --check` passed.
  An isolated pinned-Abseil `raw_logging_internal` target compiles with the
  maintained varargs header; the full Status target still fails on streams.
  Abseil Status still needs the unbuilt locale/iostream closure; A11 native
  request ownership, `Await` and fibers remain open.
* The bounded guest completion profile now owns its API under
  `<symbian/concurrency/*.h>` and `symbian::concurrency`, including its
  OS-thread mutex adapter. It no longer exports incompatible headers under
  A11's `<a11/concurrency/*.h>` or `thread::` names. The unmodified A11 source
  pin still has its original namespace and remains the reference for a full
  guest port. Source and fresh installed-SDK execution each passed eight
  ARMv5T/ARMv6 × Dyncom/Dynarmic normal/changed-result controls; a copied-SDK
  generated application build passed. The visible SDK was refreshed from the
  prepared workspace and both its 2,502-file manifest and the retained
  `~/dev/symbian-sdk-before-namespace-20261001` tree verify by digest.
  The canonical SDK also passed the ARMv6/Dyncom normal and changed-result
  controls after promotion. Root CTest passed 8/8, and the A11 source
  integrity group passed 5/5.
  Guest Abseil Status/StatusOr, A11 Await and fibers remain open gates.
* A bounded A11-derived stackless guest profile now executes
  Promise/Future/Task, inline `OnReady`/`Then`, cooperative cancellation,
  abandoned-promise completion, ordered nonblocking `JoinAll` and bounded
  reentrant `DriveInline` and `TaskGroup::Finish` fan-in/cancellation. Its
  `symbian::concurrency::Mutex` adapter uses the tested guest libc++ OS-thread lock; no fiber
  enters it. The normal and changed-result execution matrix passed eight
  ARMv5T/ARMv6 × Dyncom/Dynarmic cases from source and another eight against
  a fresh exported SDK through `Symbian::Stackless`. The visible SDK was
  refreshed after a clean 2,496-file audit; its new 2,502 files verify by
  digest, and the previous tree is retained at
  `~/dev/symbian-sdk-before-stackless-20261001`. This is an explicit
  `symbian::concurrency::Result` adaptation, not
  the final Abseil Status ABI or a native-request/fiber backend. Directly
  cross-building pinned Abseil Status revealed missing streams, C++ ABI,
  signal APIs and lock-free atomic assumptions. A11 C1, remaining C2 and C3
  work remains open. Root CTest passed 8/8 and the refreshed-SDK
  generated-project/A11 source group passed 15/15. The debugger check now
  stops in both the C bridge and `model.cc`. After the final TaskGroup
  cancellation routing adjustment, two ARMv6/Dyncom source controls and two
  canonical-SDK controls passed; the whole architecture/backend matrix was
  last run immediately before that narrow adjustment.
* Exception-capable guest work advanced without changing the default profile.
  An isolated ARM probe retains `.ARM.extab`/`.ARM.exidx`, an original-layout
  four-word Symbian exception descriptor, and E32 header offset. The converter
  now validates those bounds and supports a real ARM branch to the pinned
  Symbian EHABI personality export. A no-throw landing-pad/cleanup probe and
  changed-result control execute on ARMv5T/ARMv6 with Dyncom/Dynarmic (eight
  passed); two installed-archive cases also pass. A typed throw still requires
  `_ZTIi` imported object data through `R_ARM_GLOB_DAT`, which the resolver
  explicitly rejects; a maintained negative test confirms that gate. There is
  no advertised exception-enabled application profile yet. All eight macOS
  root CTest targets pass after updating the starter bridge test source list.
  The final focused Pytest run passed nine cases (eight execution controls and
  the typed-throw negative gate). The refreshed 2,496-file visible SDK and its
  retained predecessor both verify by digest; the previous tree is at
  `~/dev/symbian-sdk-before-exception-metadata-20261001`. The exported SDK's
  own Python/native module converted and inspected a descriptor-bearing ELF.
  A final E32 regression rejects a renamed ARM exception-index section without
  a descriptor. The rebuilt visible SDK includes that check; its immediately
  preceding verified tree is retained at
  `~/dev/symbian-sdk-before-exidx-validation-20261001`.
* Original LLVM libc++ `memory.cpp`, `thread.cpp`, mutex, condition-variable
  and future-state sources now build with a threaded guest configuration.
  `unique_ptr`/`shared_ptr`/`weak_ptr` lifetime and a `std::thread` worker
  execute with changed-result controls on both ARM targets and both emulator
  backends (16 passed). The thread case moves a unique owner into the worker,
  copies/releases shared ownership, joins, and checks 4,000 atomic increments
  and balanced allocation cells. The same 16 cases pass against a fresh
  2,496-file digest-valid SDK export using its prebuilt archive and standard
  EUSER/pthread/C++ ABI proxies. The generated starter now puts the C ABI
  boundary in `app_bridge.cc`; `model.h`/`model.cc` expose typed application
  code without opaque pointers or C linkage. Its copied/relocated build and
  four live GUI/allocation-failure cases pass. Standard futures still need
  exception-pointer/error-category support. Guest exceptions are a required
  opt-in profile, with the SDK default off; ARM unwind tables, Symbian's
  exception descriptor and throw/catch controls are current gates. Host native
  libraries retain their no-exceptions Status policy. Actual A11
  Future/Task/fibers remain open.
  The final 2,496-file export and visible `~/dev/symbian-sdk` both pass their
  digest audits; the previous 2,428-file tree is retained at
  `~/dev/symbian-sdk-before-threads-20261001`. Installed `Symbian::Threads`
  execution passed two ARMv6/Dyncom controls, generated wizard/copy/build
  tests passed, and all eight macOS root CTest targets passed.
* A bounded secondary-thread prerequisite now executes on the preserved
  RM-807 fixture. Primary and worker `RThread`s each perform 2,000 shared
  32-bit atomic increments; normal and changed-result controls pass all eight
  ARMv5T/ARMv6 × Dyncom/Dynarmic cases. The worker uses its own Symbian heap,
  and the parent drains `Logon`, checks exit reason/type, then closes the handle.
  Generated starter startup now accepts the OS secondary-thread entry while
  keeping process-global initialization on the primary thread. The fourteenth
  ordered EKA2L1 patch maps an observed Belle `RThread::ExitReason` SVC 0x34;
  its applied/replay checks pass. The same eight cases pass against the freshly
  exported SDK's prebuilt runtime archive and standard EUSER proxy; a generated
  project builds after SDK copy and project relocation. The visible 2,428-file
  SDK and its retained predecessor are digest-valid. This does not establish A11 tasks/fibers,
  general thread/TLS cleanup or hardware speed. Measured versus hypothetical
  costs are tracked in [PERFORMANCE_CONSIDERATIONS.md](../doc/docs/capabilities/performance.md).
* Bounded global C++ lifetime now executes: SDK EXE startup runs `.init_array`
  after heap setup and `__cxa_finalize`/`.fini_array` before exit; real
  `std::string` globals pass both ARM profiles and emulator backends. The SDK's
  default C++ DLL entry similarly runs a constructor on the actual Belle
  process-attach call list. Eight maintained DLL execution cases pass, including
  a changed-constructor negative control. The process-attach path required an
  explicit relocatable ARM-to-Thumb entry target and an ordered EKA2L1 patch
  mapping the observed Belle 0x10D library-entry-start operation. DLL detach,
  TLS, local-static guards, hidden/internal data and broader C services remain
  open. The direct test launcher had omitted Qt's macOS foreground-transform
  flag; with both SDK launch flags, the foreground app stayed unchanged across
  the live DLL run. A shared helper now supplies both flags to launchers. The
  28-case guest runtime execution matrix passed; four original-validator cases
  passed after correcting an obsolete exact-BSS-size assertion for the added
  destructor registry. The 12 ordered emulator patches replay from the pinned
  revision. Fourteen generated-project/library tests and all eight root CTest
  targets pass. The visible `~/dev/symbian-sdk` has 2,427 verified payloads;
  its prior sealed tree is preserved at
  `~/dev/symbian-sdk-before-global-lifetime-20261001`.
* The dynamic-library gate advanced: native E32 conversion accepts bounded
  DLL data/BSS and typed relocations; an independent EKA2L1 loader executes a
  frozen-export DLL with initialized data, zeroed BSS and GOT fixups twice in
  fresh processes on Dyncom and Dynarmic. Four oracle cases and two reproducible
  Python build cases pass. The eleventh replayable EKA2L1 patch fixes a Dyncom
  post-exit fetch after the killed process is unmapped. The installed SDK now
  supplies an ARM C compiler, `symbian_add_dynamic_library`, exact frozen DEF
  conversion, optional import proxies and retained ELF symbols. ARMv5T/ARMv6 C
  DLL and consumer-import CMake tests pass. The user's Mbed TLS adaptation's
  full 80-object `mbedcrypto` C archive builds with the SDK compiler, and a
  SHA-256 subset links/converts to a DLL with selected EUSER imports. The full
  library has not executed in a guest; internal/hidden writable globals,
  DLL detach/TLS/lifetime and module debugging remain open. Direct GUI test
  launchers use the shared nonactivating environment and nonbundle symlink.
  The 19-test focused
  DLL/library group, optional actual Mbed TLS integration, eight root CTest
  targets, Black/Ruff and patch replay checks pass. The visible
  `~/dev/symbian-sdk` was deliberately rebuilt and all 2,422 sealed payload
  files verify; its previous unmodified 2,420-file tree is retained at
  `~/dev/symbian-sdk-before-dll-data-20261001`.

* The latest full optional-input run passed **270 tests**, with one inherited
  Starlette warning (`.symbian/arm-full-background-verified.log`, 428.58 s).
  The visible `~/dev/symbian-sdk` was deliberately refreshed to the tested
  ARMv5T/ARMv6, ROM/Z and static-archive payload (2,420 verified files); its
  previous unmodified 2,410-file tree is retained at
  `~/dev/symbian-sdk-before-arm-profiles-20261001`. Installed commands resolve
  E6, E71, 7610, C7 and 808 firmware IDs. An installed-SDK `symbian init`
  generated and initially built an E6 ARMv6 app, recording RM-609 selection
  without changing owner applications. The optional GUI run and all eight root
  CTest targets pass; full-screen Space placement remains unverified.
* SDK-owned macOS emulator sessions now start from a private symlink outside
  the `.app` bundle and use the tenth replayable EKA2L1 patch to show/order Qt
  and OpenGL windows without requesting focus. A real GUI launch rendered a
  720×1280 capture while iTerm2 remained frontmost; the directly owned child
  was reaped. The OpenGL window is assigned normal managed desktop-Space
  behavior, excluding full-screen auxiliary display and tiling. The same
  launch path is used by direct GUI/runtime/debug tests. After rebuilding the
  patch, a Dynarmic GUI test passed and Chrome PID 60698 stayed frontmost
  before, during and after it. Actual placement while another app is
  full-screen and Linux focus remain validation gates.
* ARMv6 is the default for new builds and `symbian init`; developers can select
  ARMv5T in the wizard or with `--architecture`. Generated projects keep one
  architecture setting, use its matching guest runtime archive and report an
  unsupported ELF/ABI before publishing or launching. Existing ARMv5T projects
  retain their selection. A real ARMv6 `REV` and the ARMv5T software sequence
  execute on Dynarmic and Dyncom; the full runtime matrix passes 28 cases
  including independent original E32 validation. The root CMake file API now
  supplies an explicit ARM target to CLion's guest compiler probe while host
  tools remain native arm64; both compiler probes and all eight CTest targets
  pass. The architecture/A11 source checks pass 14 tests.
* Bounded writable EXE data/BSS now runs on Dynarmic and Dyncom: initialized
  values, 64 zero-filled BSS words, data/BSS mutations, code/data pointers and
  a Thumb function pointer. A changed initial value exits -115. Twelve runtime
  execution cases plus two independent original checksum/whole-image-validator
  checks pass (14 tests, 57.97 seconds). Four new native data/fixup tests and all
  eight root CTest targets pass. New-project linker layouts include the verified
  RW mapping. Bounded global constructors/destructors now execute; TLS and full
  DLL lifetime remain unsupported. Artifacts: `.symbian/runtime-data-oracles*`.
* Actual A11 thread/concurrency adoption has begun: 46 original licensed files,
  92 local include edges, per-file digests, original tests, no adaptations yet.
  Original Git comparison, CMake source checking and five tamper/closure tests
  pass. This is source staging, **not a built guest concurrency backend**;
  Boost forced unwind/exception boundaries require the planned explicit
  adaptation. Guest Abseil, shared ownership/atomics, static lifetime and OS
  thread/TLS prerequisites remain gates. No scheduler lookalike or premature
  tasks/futures/fibers examples were added.

* Shared ROM/Z onboarding is implemented: native original EKA2L1 archive,
  ROM/RPKG, extracted Z and VPL readers; portable content IDs/bundles; XDG data
  storage and retained cache evidence; global -> SDK -> project -> command
  precedence with explicit origins/mappings. SDK/build/init require no firmware.
  Run/Debug no longer hard-code RM-807 paths, and select its workaround only for
  the exact known pair. Six other dump imports are tested; C7, E6, 6120 and E71
  generated starters pass initial build, greeting, clock input and native normal
  exit using the default profile. 7610/P900 import, with a specific missing EKA1
  startup/import ABI error before application launch. Default fonts, compact
  layout and logical capture dimensions remove 808-only display assumptions.
  Unknown FBS requests return KErrNotSupported instead of hanging; opcode 0x2C
  itself remains unimplemented. Both new upstream patches replay/reverse exactly.
  Firmware checkpoint full optional-input run: **234 passed**, one inherited Starlette warning,
  no skips, 335.11 seconds. Eight root CTest targets and five native firmware
  GTests pass. Logs/artifacts: `.symbian/firmware-full-verified*`; earlier failed
  captures, the 6120 teardown hang and import diagnostics remain retained.
  See [FIRMWARE.md](../doc/docs/guides/firmware.md) for commands, portability, limits and migration.

* Bounded local GOT conversion now emits E32 text fixups for up to 1,024
  defined object/function slots, preserving Thumb state and linked GOT_PREL
  words. Eight runtime cases pass on the preserved Delight fixture: ordinary
  success/OOM, global std::nothrow plus constant/function GOT success, and a
  changed-constant failure on each backend. Six new native GOT tests and seven
  original-validator checks pass. Generated apps now acquire their model with
  std::nothrow and return -4 after closing native resources on failure; all
  fifteen project-generation/build checks pass, including both CPU backends
  and terminal GDB. The visible SDK was deliberately refreshed with its previous
  tree retained; owner application sources/settings were not rewritten.
  GOT checkpoint full optional-input run: 208 passed, one inherited Starlette warning,
  no skips (181.74 seconds); eight root CTest targets pass. Black/Ruff, changed
  C++ formatting and whitespace checks pass. Integrity checks confirm the
  original archive, all 13,438 golden files and all 2,408 current SDK digests.
* `symbian sdk install` exports visible target headers, matching libc++ runtime,
  ordinal proxies, CMake package, Python utilities/native modules, notices and
  command shims. Projects keep one local SDK setting and relative shared files;
  commands dispatch to the selected SDK, including its project templates.
* `symbian init` asks for identity and IDE preferences, builds initially, and
  generates actual CMake workspace/C++ module registration, an enabled stable IDE profile,
  Run executable and remote-debug profile selection. The initially missing
  workspace registration caused IntelliJ to open a generic module despite valid
  terminal CMake/clangd results. After repair, the actual reopened owner project
  `symbian-app-3` configures in the IDE and reports three sources, zero unknown.
  Earlier owner projects were repaired without replacing application sources.
  Generated Run/Stop and GDB are execution-tested; IDE toolbar/debugger interaction
  and full unwind coverage remain distinct gates. See PROJECTS.md.
* The hello/time starter renders original W32 font text, uses real home-time
  services, std::string/std::vector, Clear/Exit, focus and pending-request cleanup.
  Both CPU backends pass rendered tap/log/clear/normal-exit checks. Real GDB stops
  in the modern model and exposes clock arguments/source. Project/SDK copies,
  relative SDK selection and selected-SDK template dispatch pass integration tests.
* The preceding full verification passed 202 Pytest cases with all optional inputs and
  one inherited Starlette deprecation warning. Following the IDE registration
  amendment, all thirteen project-generation/build cases pass; eight root CTest
  targets pass. The installed macOS wheel audit includes the new templates.
  Updated Linux aarch64 checks pass seven host CTest targets, installed wheel
  closure/resource audit and 62 installed Python cases; Linux emulator remains
  unverified. Earlier test totals below describe earlier checkpoints.

* Root CMake exposes the real ARM `gui_app`, its `gui_app_e32` publisher and
  a native `gui_app_run` executable. The saved root GUI Run configuration now
  explicitly selects that executable. Terminal launch/Stop/normal SDK exit and
  GDB batch/MI relocation pass; actual reloaded IDE UI interaction is unverified.
* A11's actual native/Python Status/StatusOr bridge is ported, with canonical
  caster modules, payloads, Pydantic hooks, GIL helpers, provenance and licenses.
  Native codecs/tests remain exception-free. Python initializers expose shortcuts
  and serializable metadata uses Pydantic. See A11_STATUS.md for adaptations and
  inherited mapping/compact-MessagePack limits.
* A real bounded libc++ runtime executes heap-backed strings/vectors and cleanup
  on both emulator CPU backends; actual heap exhaustion/nothrow/fatal failure
  controls pass. Over-aligned allocation and original ARM compiler-rt division/
  remainder execute in the maintained probe. LLVM sources are pinned with one
  replayable Symbian atomic-query patch. See RUNTIME.md and
  ../CXX_CAVEATS.md. Bounded GOT, writable data and initialization now execute;
  general TLS and full hosted C++20 remain unsupported. Guest Abseil/JSON is
  not provided by the host wheel.
* Linux aarch64 CPython 3.12 host native tests, wheel, installed closure/resource
  audit and 62 Pytest cases pass. macOS's installed wheel audit passes too.
  x86_64/other CPython and Linux emulator/guest/debugger are configured/planned,
  not locally verified. DISTRIBUTION.md describes the one-install payload design;
  compiler/emulator/header/debugger payload wheels are not yet distributed.
* Earlier baseline verification passed 152 Pytest cases (40 optional skips) and seven
  root CTest targets. Runtime tests pass four firmware-backed cases plus a
  rejected GOT control; original validator acceptance/failure checks pass three
  GTests. Black/Ruff, generated stubs and root target-aware clangd checks pass.
  The copied HTTP tests emit a visible Starlette/httpx deprecation warning.

Earlier verification totals below are historical checkpoints; current evidence
and precise scope are recorded in RESEARCH_LOG.md.

| PLAN milestone | Status | Required evidence |
| --- | --- | --- |
| 0 Preservation | Archive tools tested; physical baseline pending | Device inventory, original artifacts, offline archive, tested human recovery appliance |
| 1 Toolchain | Reproducible ELF→E32; historical validation and ROMless process/DLL import tests pass | Matched Belle runtime, SDK imports and complete target ABI tests |
| 2 Project model | CMake/Ninja, persistent database, native SIS and ROMless install/launch pass | Matched SDK/DLL imports, general application support and Belle installer |
| 3 Emulator | Native arm64 build, ROM/Z import and real GUI pixels/input/zero exit pass on both backends | Lifecycle/reset/snapshot facade, full OS boot and general runtime coverage |
| 4 Automated development | Native build/package/check loops and opt-in rendered GUI tests pass | General unattended install/run/artifact loop and symbian test |
| 5 Modern debugging | Live ARM GDB stops/stepping, model inspection and native process-exit records pass | Full unwinding, panic/thread/module inspection and CLion debugger validation |
| 6–9 | Pending | Physical deployment/system/hardware/alternative OS gates in PLAN.md |

No physical-device executor, flashing capability or recovery automation exists.
Source inspection is not an emulator runtime test. Record implementation and
verification results in RESEARCH_LOG.md before changing milestone status.

Current working commands: `doctor`, `toolchain probe`, `toolchain verify-probe`, `toolchain verify-pointers`,
`toolchain verify-package`, `toolchain import-proxy`, experimental
`build`/`package`, ELF/E32/SIS/import-proxy `inspect`, `preserve create/verify`,
`emu status`, `emu screenshot`, `emu pointer`, and informational `device policy`.
The emu commands require an explicitly started private research endpoint; they
are not a general lifecycle API or a physical transport.
The e32_probe example has no SDK/imports/data/constructors; its direct thread
exit is a no-resource experiment. Both the linked ELF and converted E32 repeat
byte-for-byte in two builds on this host. Parser acceptance does not prove
Symbian loader compatibility. Reports retain loader/runtime verification false.
Artifacts and reports live under .symbian/. clangd consumed the generated ARM
compilation database with zero errors. Native core code is compiled with
exceptions disabled; the pybind11 boundary retains canonical status codes.

Verification: 112 Pytest cases pass with the patched emulator, native oracles
and public kernel source supplied. The 41 platform GTest cases and 48 independent
oracle GTest cases pass on Apple Silicon. The preceding emulator checkpoint
passed 288 upstream EKA2L1 cases; its sources are unchanged here. Black/Ruff,
clang-format and generated stubs pass; clangd target checks have zero errors.
Without optional dependency paths, four emulator smoke cases and two native
verification integration cases and one public-header research case skip.
One additional development DLL runtime case and three variable-header DLL
validation cases need the native oracles. Two additional pointer/combined-layout
cases need them.
EKA2L1's parser omits header CRC verification;
Nokia's original checksum and whole-image validator independently check it.
The unchanged historical validator accepts this image using host type adapters.
Both EKA2L1 CPU backends execute its ARM startup and Thumb C++ at two load
addresses; a changed input produces the expected failure exit. These are
ROMless CPU tests: the exit SVC is observed by their callback. Eight additional
cases use EKA2L1's real process loader, flexible memory model, scheduler and
kernel SVC dispatch under its epoc10 profile. Both backends return zero normally
and 42 for changed input; absent executables fail to create a process. Both
backends also launch another process after an earlier failure exit.
The process/thread exit states and address-space release are checked. This is
an import-free emulator process without Belle ROM/Z or system services.
Reports distinguish eka2l1_process_verified from Belle loader/runtime flags,
which remain false. No physical runtime ran.
Build and patch replay instructions are in research/eka2l1/README.md.

The CMake path preserves the baseline ELF/E32 bytes. Multi-source builds, header
changes, paths with spaces and cached no-op builds are tested. Its compilation
database points to existing build objects; clangd reports zero errors. The wheel
contains the CMake modules and builds the same E32 from an isolated installation.
Project instructions and the v2 build report are described in docs/BUILDING.md.

The unsigned native SISX package is reproducible and passes independent Nokia
UID/controller/data CRC checks. Six disposable EKA2L1 cases install the unchanged
probe, launch through the kernel on both CPU backends, reload its registry,
uninstall and reinstall. The registry's file hash agrees with an independent
hashlib baseline. That installer does not enforce phone signing/capability policy.
The production native inspector verifies checksums, SHA-1, E32 and the restricted
canonical profile before the trusted experiment. Reports retain Belle runtime
and phone installation flags false. See PACKAGING.md for profile limits and replay.
OpenSSL 3 is statically linked for SHA-1; its license is packaged with the wheel.

The native SDK component reads frozen EABI definitions and generates selected
function ordinal proxies using Clang/LLD. The public User::Exit slot remains 641,
and Nokia's unchanged ordinal lookup method agrees in a separate optional test.
An original e32std.h call compiles and links, typed layout assertions pass, and
clangd reports zero errors. The public request status is eight bytes with flags.
Proxy and link ELF builds repeat; SDK/source/header dependencies are recorded.
An isolated wheel installation includes the target probe resource and builds
the same proxy and linked ELF.
These are link contracts, not a verified 808 SDK. See SDK.md for replay.

The E32 import profile places function GOT slots in its code region and resolves
versioned symbols against original proxy ordinals. A compiled development DLL at
ordinal 7 executes on both EKA2L1 CPU backends; the patched slot, function PC,
changed-input failure and repeated launch are checked in six cases. Historical
validation/checksums accept both files. Two-DLL links and malformed controls pass.
An isolated installed wheel reproduces the same imported ELF/E32 and metadata.
General writable-data DLL support, matched SDK services, startup/cleanup and
Belle runtime remain open. See IMPORTS.md.


Native DLL conversion now resolves frozen function exports from retained ELF
symbols, preserves ordinal gaps/ABSENT entries, and emits the count prefix,
full absence bitmap and code relocations for all export pointers. No fixed
function address is required. Independent validation covers ordinals 7, 641 and
65,535, including complete variable headers and relocation pages. Both emulator
backends also verify the mapped table: present pointers agree with lookup,
absent pointers relocate to the entry, and the count word remains unchanged.
Combined import/export layout passes Nokia's checksum and whole-image validator;
its executable startup is not runnable DLL initialization evidence.

The earlier research DLL omitted the count prefix and export-pointer relocations.
Its successful structural validation and export lookup proved less than the
public ELF loader contract. It remains research material; maintained runtime
checks now use probes/dll_probe and the native converter. The installed wheel
reproduces the native DLL and ELF and their metadata. Evidence is in
.symbian/native-dll/report.json, verification-report.json and wheel-result.json.
General pointer relocations, writable data/BSS/TLS, constructors, SDK startup,
matched target DLLs and Belle runtime remain open. No device operation ran.


Retained internal ABS32 words now generate E32 text relocations, permitting
named RELRO tables within the RX mapping. A maintained multi-source C++ probe
executes an ARM callback, a Thumb callback and a virtual method, and reads a
constant-data pointer with an addend. Both emulator backends verify all four
mapped words and instruction state at the three indirect targets, changed-input
failure, repeated launch and address-space release. Its native SIS installs,
launches, reloads the registry, uninstalls and reinstalls in separate cases.
The new verify-pointers command retains 14 image/dispatch checks or 21 with the
package. An isolated wheel reproduces ELF/E32/SIS and repeats all 21 checks.
Evidence is in .symbian/pointer-probe, .symbian/pointer-package and
.symbian/pointer-check. See POINTERS.md for replay and the trusted-link contract.

Internal pointer relocations also coexist with eager imports and frozen exports
in independently validated layout cases. External absolute pointers, GOT_PREL,
writable data/BSS/TLS and global lifetime support remain open. Simple virtual
dispatch does not prove the full target C++ ABI. Matched Belle and physical
execution are still unverified; no device operation ran.


C++20 now has maintained language and named-module examples. Concepts/requires,
structural class arguments, consteval/constinit, designated initialization,
constrained lambdas, equality and small layout contracts compile with explicit
controls. The language probe passes 21 independent loader/installer cases. A
separately configured libc++ bit/concepts/span experiment produces distinct
machine code and passes the same loop without linking a hosted runtime.
The module example uses upstream Clang/CMake scanning, retains its BMI, checks
importer invalidation and passes the 35-case image/package loop. An isolated
installed wheel reproduces all three ELF/E32/SIS variants and all 77 checks.

All 124 Pytest cases pass with explicit optional compiler/header/oracle inputs;
the native platform and SDK ordinal checks pass. Evidence is under
.symbian/cxx20-probe, cxx20-check, cxx20-library, cxx20-library-check,
cxx20-module and cxx20-module-check. See CXX20.md. Complete standard library,
coroutine/thread/atomic runtime, global lifetime, writable data/BSS/TLS, SDK
services and matched Belle remain open. No physical-device operation ran.


The requested examples/gui_app and root WALKTHROUGH.md are now present. The
counter application directly uses Window Server; it has seven-segment drawing,
touch increment/reset/exit and explicit request cancellation/object cleanup.
Its primary-thread adapter attempts SDK heap/process setup and User::Exit,
while secondary-thread, exception-entry and global lifetime support remain
absent. The build report now correctly treats startup/cleanup as unverified
project behavior instead of assuming every executable uses direct ThreadKill.

At that checkpoint, the source profile staged 92 original header aliases and
native frozen proxies
for nine EUSER and 28 WS32 functions from pinned ignored public trees. Preparation
checks file digests, preserves original files/licenses, rejects malformed
profiles and output redirection, and records SDK/runtime verification false.
Clang/LLD and independent CMake trees reproduce the ARM ELF and E32. Five model
GTests pass, including wide/tall layouts, arithmetic, input bounds and saturation.
Original checksum/validator sources accept the generated GUI in eight cases.
DWARF verifies and LLDB resolves functions and source lines; clangd has no
diagnostics with its limited check-mode refactoring selection.

All 139 Pytest cases passed with explicit optional inputs; all six root CTest
targets passed. The final drawing-layout adjustment also passed the five model
GTests and all 15 GUI Pytest cases. An isolated installed wheel runs the SDK
preparation/build/verification CLI, reproduces both artifacts and repeats all
eight independent image checks. Evidence lives in .symbian/gui-sdk,
.symbian/gui-app, .symbian/gui-validation and .symbian/gui-research.

Visible GUI execution, actual Belle EUSER/WS32 compatibility, heap/cleanup and
guest debugger attachment remain unverified because matched ROM/Z is absent.
The walkthrough labels these procedures as future experiments. The example
has no application registration or SIS package; the existing SIS writer's
import-free restriction remains. No emulator OS boot or physical-device action
is claimed or performed. Phone RM/product/firmware details remain unknown.


The GUI now has a native single-EXE unsigned SIS package, independently validated
in 17 image/checksum/installer cases. Actual installer/registry behavior verifies
exact payload bytes, UID/SID/version and an independent legacy digest, reload,
uninstall and reinstall in disposable ROMless filesystems. A control records
the unchanged upstream loader's missing-library defect: it creates a process
with all 37 imported words still unresolved. No guest instructions execute in
these installer cases. Reports explicitly keep GUI/SDK execution and matched
loader/runtime verification false. The native writer now accepts imported EXEs
while still rejecting DLL payloads, resources and broader package profiles.
All 142 Pytest cases and six root CTest targets pass; the installed wheel
reproduces ELF/E32/SIS and repeats the 17-case package check.

The supplied Delight v1.8 ZIP was checked and staged privately. Its VPL declares
RM-807, product 059M7Q4 and version 113.010.1508; seven required/present files
pass archive and declared CRC checks. One opt-in native GTest successfully uses
EKA2L1's VPL/FPSX/ROM/ROFS/FAT importer into a new isolated root. The result
identifies Nokia/808 PureView/RM-807/epoc100 and supplies a ROM and actual system
DLLs, with 13,438 imported files inventoried by SHA-256. This corrects the prior
missing-assets state. Custom archive authenticity, stock recovery baseline and
matching this physical phone are not established.

A copied instance maps the GUI at 0x70000000, EUSER at 0x804bcce8 and WS32 at
0x80a4c028. It logs unimplemented SVCs 0x51/0xF7 and a $HEAP lookup failure;
visual behavior, correct startup/cleanup and debugger attachment are not yet
verified. TERM did not finish the private emulator, so its confirmed process
was stopped with KILL after retaining logs. The original imported root remains
separate from runtime state. No physical phone operation ran. Evidence is in
.symbian/gui-package[-check], gui-research/delight-archive-check.json,
delight-import.json, delight-import-inventory.json and the private instance logs.

Live ARM GDB 17.2 attachment now works against a disposable RM-807 instance.
The actual startup source breakpoint receives reason=0 and info=0x40ffc0 at
PC=0x700009da. A local GPL guest-debug-step.patch fixes silent execution after
single stepping. A real frontend/GDB Pytest proves two successive Thumb stops,
a stable fresh register read after a delay, source display and ROM SVC stops.
The same test fails with a 30-second GDB timeout when that patch is removed;
restoring it passes. All 143 Pytest cases with explicit optional inputs and all
six root CTest targets pass. This is live debugging evidence, not GUI success.

Stable registers show heap initialization returns KErrNotFound (-1), before
GuiMain. The ROM requests kernel HAL page size using SVC 0x51 and chunk creation
using 0x6D; the pinned epoc10 table maps those operations to 0x4F and 0x6B and
dispatches 0x6D as object lookup. The real exit path reaches unimplemented
0xF7, while its handler is registered at 0xF6. These firmware executive ABI
discrepancies require a fuller independently checked Belle profile. No whole
table shift, SDK replacement, or loader workaround was introduced.

Visible GUI output/input, successful heap setup, normal SDK exit, stack unwinding
and OS boot remain unverified. The test-owned frontend still requires KILL
after TERM; reaching an exit wrapper does not establish guest cleanup. The
original ZIP and all 13,438 baseline file digests are rechecked unchanged.
Physical phone details remain unknown and no device operation ran. Replays and
current limitations are in WALKTHROUGH.md section 9; transcripts, negative
control, test logs and integrity evidence are in .symbian/gui-research/debugger*.


### 2026-09-30 — Guarded RM-807 ABI and initial drawing-function execution

The real ROM export probe and source-wrapper comparison now support a piecewise
experimental Symbian 101 executive map. It has 170 existing handlers, while the
original 172-handler epoc10 map remains intact. Profile selection is explicit
and exact-ROM-digest guarded; real frontend tests reject unknown profile names
and changed private ROM bytes. This is a research profile for the supplied
Delight image, not universal Belle support or authenticated stock firmware.

With that profile heap setup returns zero and GuiMain executes. A separate ARM
TPIDRURO register fixes the original Dynarmic coprocessor abort, with four real
instruction/context cases passing on both tested macOS backends. Window Server
connection returns zero. A subsequent E32USER-CBase/69 panic exposed missing
cleanup-stack setup; the example now creates the SDK CTrapCleanup before GuiMain
and deletes it on return. Its frozen EUSER import count is now ten (38 total).

Live GDB verifies the initial DrawGui entry, zero/running model, 360 by 640 layout
and return after its guest drawing calls. The default map still fails heap
startup, preserving a useful control. Full DLL initialization remains unproven:
0x10D is still unimplemented. Rendered pixels, pointer delivery, normal cleanup/
exit, full unwinding, OS boot and physical-phone compatibility remain unverified.
No screenshot or visible-GUI success is claimed from a drawing-function stop.

All 146 Pytest cases pass with explicit optional inputs, all six root CTest
targets pass, and the two new research CTest targets pass seven GTest cases.
The opt-in ROM export probe passes separately, rejects existing/nested output,
and executes no guest instructions. All six patches apply in documented order
against fresh pinned source files. An isolated installed wheel reproduces the
updated ELF/E32, packages it and passes all 17 historical/installer checks;
research tests/firmware remain excluded. Black/Ruff and C++ formatting pass.

The original ZIP and every path, size and SHA-256 of the 13,438-file unbooted
baseline are rechecked unchanged. Runtime work uses copied instances; the
baseline is not booted. Phone identity and independent offline preservation
remain unknown. No device operation ran. Replays and boundaries are recorded in
WALKTHROUGH.md and docs/BELLE_ABI.md; private evidence is under
.symbian/belle-abi-research. The platform mission remains active.


### 2026-10-01 — Rendered GUI, pointer input, normal exit and developer workflow

The native GPL research adapter reads the actual guest screen texture, routes
logical pointer events through Window Server and copies kernel process-exit
records. It runs on existing Qt/kernel loops, adds no scheduler/thread/Python
callback and compiles with exceptions disabled using Abseil Status/StatusOr.
The wheel contains only the synchronous Python policy client for this endpoint;
EKA2L1 and the native adapter remain separate research binaries.

Live tests on Dynarmic and Dyncom verify 720x1280 portrait PNGs (logical 360x640,
scale two), 0000 → 0001 → 0002, an outside tap leaving 0002, reset to 0000 and
exit type kill/0 with reason zero for UID 0xe0000811. The frontend also exits
zero. Four native GTests exercise disabled/invalid/private-path startup bounds;
live tests reject malformed/oversized commands, traversal, duplicate outputs and
invalid pointers. Two observed locking/lifecycle mistakes were corrected:
pointer delivery must not retain the kernel lock that Window Server acquires,
and callbacks must detach before the OS worker destroys the kernel. Final native
status survives socket closure; saved reports are explicitly selected.

The GUI preset now provides both SDK import proxies without CLI injection.
Standalone CMake configure/build and clangd parsing/indexing pass with zero
errors using the limited documented tweak selection. Canonical ELF/E32 rebuilds
retain their previous hashes. The running IntelliJ IDEA/CLion-plugin instance
now has persisted Symbian ARM toolchain settings and a local clion-arm preset.
The exact local preset configures/builds and clangd reports zero errors. The GUI
project now configures successfully in the actual IDE. Its generated CMake API
lists app.cc, startup.cc and startup.S; the initial model resolves two C++
sources and one assembly source with zero unknown sources. The live preset
selects existing Default with explicit ARM paths because the running IDE had
not loaded the saved application toolchain name. The generated native GDB
profile selects the ARM debugger independently and does not need that restart.
The debugger frontend is untested. Setup, E32 publication and ARM remote-debug steps are in docs/CLION.md.

The initial control checkpoint passed 161 Pytest cases with explicit optional
inputs; the launcher checkpoint now passes all 170. All six root CTest
targets pass, and the three control/routing/register research targets pass eleven
GTests. Seven patches replay against 15 fresh pinned source files. Black/Ruff,
clang-format and whitespace checks pass. An isolated installed wheel outside
the source tree reads the native final exit envelope and inspects the actual
GUI E32 through its native extension. Its module paths, wheel digest and reports
are in .symbian/clion-setup/wheel-result.json. The research native
adapter and tests are absent from the wheel; Pillow is a development dependency.
The original ZIP and all 13,438 baseline paths/sizes/SHA-256 values are rechecked
unchanged. No physical-device operation ran; identity and independent offline
preservation remain unknown.

PLAN.md now records the completed vertical slices, evidence boundaries and
ordered next gates: developer onboarding, disposable lifecycle/symbian test,
unresolved ABI/DLL lifetime, bounded runtime/C++ library support, diagnostics,
then broader application/system/device scope. Full initialization (including
0x10D), 0xFF interception, writable data/TLS/static lifetime, full unwinding,
OS boot and physical compatibility remain unverified. The broader mission
remains active. Replays are in WALKTHROUGH.md and docs/EMULATOR_CONTROL.md;
private evidence is under .symbian/belle-abi-research/control* and gui-control*.

A bounded foreground GUI launcher now publishes the current E32 before Run or
Debug, copies the named golden, owns/reaps its frontend and retains manifests,
logs and final native status. Generated local GUI Run/GUI Debug configurations
and a native Symbian GUI GDB profile are installed in the dedicated GUI project.
The supervisor starts a halted instance before GDB, then relocates symbols after
connection using the actual mapping. Source breakpoint/variable checks and the
IDE's GDB MI2 protocol pass, including Thumb-to-ARM instruction stepping. Normal
Run exit, Stop cleanup, occupied ports and launcher-path quoting are checked.
IDE toolbar interaction and full debugger frontend/stack unwinding remain
unverified. This is not yet the general lifecycle/symbian test API.

The final full launcher/control suite passes 170 tests in 112.82 seconds with
all optional inputs enabled. Four targeted installer/ownership tests pass after
saving GUI Run as the default selection. Research CTest again passes all three
targets/eleven GTests. The installed wheel is exercised from /tmp, including
GDB version discovery without startup and default configuration selection.
The ZIP and all 13,438 baseline files remain unchanged.

Earlier full runs exposed an Exit-up request after the app had already closed
and one frontend that did not finish shutdown within 15 seconds despite guest
ThreadKill reason zero. Exit tests now send only down; the native adapter drains
already queued replies for a bounded interval when its Qt loop stops. Added
teardown phase logs and an owned-process stack sample on a repeat timeout.
Twelve repeated GUI runs, five six-case debugger/GUI groups and the final full
suite then pass. No repeat stack is available; the isolated shutdown timeout's
cause remains open rather than being inferred from later passes. IDE Stop still
has tested bounded cleanup.

The reopened GUI project scans 535 files and resolves its three target sources.
Root-project exclusions for private runtime/upstream/build data are saved with
the staged SDK unexcluded; their live application is unverified. IDE toolbar
interaction remains unautomated. PLAN.md retains these developer-lifecycle gates
before broader ABI/runtime and device scope.

The current runtime and DLL follow-up adds actual LLVM compiler-rt ARM EABI
64-bit signed/unsigned quotient and remainder, its shared C division core and
the ARMv5T-safe leading-zero helper. The expanded guest matrix passes 32 cases
on ARMv5T/ARMv6 and Dyncom/Dynarmic, including a changed-wide-result failure
control. A separate run with the optional oracle build passed four original
checksum/whole-image validator cases on the new ARM images.

The named RM-807 fixture now passes a bounded dynamic DLL lifetime test. The
real ROM `RLibrary::Load` initially reached an unimplemented Belle SVC 0x10E
and returned its unchanged `this` pointer. The thirteenth ordered emulator
patch maps that observed slot to EKA2L1's existing v10 load-preparation hook;
the loader-server operation, two ordinal lookups and `RLibrary::Close` then run.
A DLL destructor writes to client-owned memory before `Close` returns. The
maintained constructor, dynamic-load and absent-DLL matrix passes 16 cases
across both ARM profiles and emulator backends; absent DLL returns -1 and the
client exits -121. All 13 emulator patches replay from the pinned base and the
last reverse-checks. This does not prove all DLL modes, TLS or thread lifetime.

A direct genuine-A11 prerequisite probe compiled `std::make_shared` but failed
to link without libc++ shared-ownership definitions. The current libc++ profile
also disables threads, so merely adding `memory.cpp` would not establish A11's
atomic cross-thread ownership. C0 still needs a real thread/atomic backend and
guest Abseil before Promise/Future can be exposed. The pinned 46 A11 sources
and 92 local include edges still verify unchanged. No guest A11 scheduler or
fiber capability is claimed.

The visible `~/dev/symbian-sdk` was deliberately refreshed from the prepared
checkout after its previous 2,427 sealed files all matched their digests. The
previous SDK is retained at `~/dev/symbian-sdk-before-wide-dll-20261001`;
the new canonical SDK also has 2,427 verified sealed files. Its ARMv6 archive
contains the new `__aeabi_ldivmod`, `__aeabi_uldivmod` and `__clzsi2` symbols.
A dynamic DLL run against the refreshed SDK passed; a fresh `symbian init`
build/copy/relocation test also passed. No owner application source or UID was
changed.

The external user-owned `~/dev/mbedtls-symbian` sources were built without
editing them. A selected SHA-256 subset links into the SDK E32 DLL and now
executes through a dynamic guest `RLibrary` client: SHA-256 of `abc` matches the
known 32-byte digest, while changing the input fails with -132. Static format
and four Dynarmic/Dyncom execution/control tests passed (five total). This is
one useful Mbed TLS function, not broad TLS/network integration.

The native-lock prerequisite now has a guest execution probe: an EUSER
`RFastLock` is created, its uncontended `Poll` succeeds, a second `Poll` while
held returns `KErrTimedOut`, and `Signal`/`Wait`/close complete. Four ARMv5T /
ARMv6 client × Dynarmic/Dyncom cases passed in 71.31 seconds. This runs the
RM-807 ROM implementation for both client architectures; it does not prove a
separate ARMv5 ROM implementation or cross-thread contention. A11's fiber-aware
`thread::Mutex` still needs a backend that parks fibers instead of blocking
their event OS thread. Fast ARM context switching remains unimplemented.

The final development SDK export adds the real ROM `RLibrary` and `RFastLock`
imports to its standard EUSER proxy. Its 2,427 sealed files passed digest
verification; the installed-proxy fast-lock matrix passed four cases and the
Mbed TLS SHA-256 DLL suite passed five. The export was promoted to
`~/dev/symbian-sdk` after checking the previous canonical SDK's digests; that
previous tree remains at `~/dev/symbian-sdk-before-fast-primitives-20261001`.
Both retained and current trees have 2,427 digest-valid files. A fresh
generated-project initial-build, SDK copy and project-relocation test against
the canonical SDK passed in 10.41 seconds.

A further C0 runtime gate now executes 32-bit `std::atomic` fetch-add, acquire
load, failed/successful CAS and exchange. Clang emitted `__atomic_*` libcalls
for ARMv5T and `__sync_*` libcalls for ARMv6; both are backed by real EUSER
ordered atomic imports through a narrow original-header bridge. The normal
and changed-value controls passed eight cases across both client targets and
both emulator CPU backends in 83.77 seconds. A fresh installed SDK with the
new standard proxy and `e32atomics.h` passed the same eight cases in 79.98
seconds; its 2,428 files are digest-valid. These are single-thread paths.
Cross-thread race behavior, 64-bit atomics, libc++ shared ownership and the
A11 task/fiber backend remain unverified.

The 2,428-file atomic-enabled export was promoted to `~/dev/symbian-sdk`
after the previous 2,427-file canonical tree passed its digest check. That
previous SDK is preserved at `~/dev/symbian-sdk-before-atomics-20261001`;
both current and backup trees pass their sealed-file checks. A fresh
generated-project initial-build, SDK copy and project-relocation test against
the new canonical SDK passed in 7.42 seconds. The source profile now stages
93 pinned header aliases, including original `e32atomics.h`.
The real-header GUI source-staging/link test passed with the updated count
in 10.75 seconds.
Finally, a stricter test copied the runtime probe into a temporary project,
linked the installed SDK's prebuilt `Symbian::Runtime` archive and standard
EUSER proxy, and used its installed `symbian/runtime.h`. All eight atomic
normal/changed-control guest executions passed in 85.85 seconds. The earlier
79.98-second selected-SDK run had used the installed proxy with a runtime
rebuilt from this checkout; the stricter run verifies the exported archive.
Four default string/vector runtime cases then passed on the same prebuilt
archive across both ARM profiles and emulator CPU backends in 49.80 seconds.

The 64-bit atomic C0 prerequisite now has two complete runtime archives. The
default `Symbian::Runtime` uses an original EUSER `RFastLock` to serialize
64-bit load/store/CAS/exchange/add across threads. The opt-in
`Symbian::NativeAtomics64` calls original EUSER 64-bit exports through a
matching proxy. The preserved RM-807 ROM executes LDREXD/STREXD for these
operations. Dyncom had assembled the STREXD register pair but written its
register index; the fifteenth ordered EKA2L1 patch fixes that argument. A
ROM-independent STREXD GTest passes on both backends. This was an emulator
fault, not evidence of bad ROM atomics.

Original Symbian source also has an ARM V5/V6 interrupt-masking implementation,
so native EUSER imports are not automatically lock-free on other firmware.
The opt-in native profile was initially verified on the named RM-807 fixture
and was later checked on RM-675/RM-609 as recorded below. Clang's
ARMv6 `is_lock_free` builtin falsely described the default lock-backed
archive; a maintained pinned LLVM header patch now directs `std::atomic` and
`std::atomic_ref` runtime queries to the selected archive. The 64-bit probe
checks actual parent/worker high-and-low-word updates, direct `__sync` calls,
store, CAS, `atomic_ref` and changed-result controls. Twelve source cases
passed before the query extension, its three-path smoke passed after, and all
16 selected installed-SDK cases passed across both ARM targets and emulator
backends in 80.40 seconds. Root CTest 8/8 and the independent STREXD CTest
pass. The final visible SDK has 2,564 digest-valid files; its prior 2,535-file
tree is preserved at `~/dev/symbian-sdk-before-native-atomic64-20261001` and
also verifies. A fresh generated-project initial-build, SDK copy and
relocation test passed against the promoted SDK. Wider ordering litmus tests,
other firmware, physical hardware, guest Abseil linkage, C1 request ownership
and A11 fibers remain open.

A subsequent non-808 check broadened the verified native-atomic profile to
C7-00/RM-675 and E6-00/RM-609 under EKA2L1. Their five 64-bit EUSER entry
points have byte-identical prefixes to the RM-807 ROM, including LDREXD/
STREXD add and CAS loops. Cross-thread `std::atomic<uint64_t>` then passed on
Dyncom and Dynarmic for both devices (four cases, 31.91 seconds). The initial
run isolated an unrelated emulator gap: their v10 ROM wrappers use SVC 0x32
for `RThread::ExitReason`; the sixteenth ordered patch maps it to the existing
handler. The older 6120c/RM-243 and E71/RM-346 ROMs expose only 2,228 EUSER
exports, below this profile's five EABI ordinals. The native profile must not
be selected for them. Physical phones, additional dumps and broad atomic
memory-order tests are still unverified.

The next runtime slice links original libc++ `hash.cpp` and genuine LLVM
compiler-rt soft-float, aligned-copy and multiply helpers. A bounded
`std::unordered_set<int>` growth/erase/cleanup probe and changed-result
control passed eight ARMv5T/ARMv6 × Dyncom/Dynarmic cases from source and
eight against a sealed installed-SDK candidate. The companion compiler-rt
normal/negative matrix also passed eight source and eight installed cases.
The candidate has 2,591 digest-valid files. Hash-table growth uses the ROM's
`ceilf` from `libm.dll`; this is not guest Abseil `flat_hash_map` yet.
An E71 generated starter initially failed because the CMake runtime target
added an unused `libm` needed proxy. Its CMake link now scopes LLD
`--as-needed` to that proxy. The E71 starter then built and executed on its
named firmware, and an installed hash-table smoke test still imports and
executes `ceilf`. ROMs without `libm.dll` still cannot execute that hash-table
path until the SDK supplies a math implementation.

The corrected candidate's full installed hash-table matrix passed all eight
cases in 44.76 seconds. A final re-export from the prepared checkout had
2,591 digest-valid files, and its initial-build/copy/relocation test passed.
It was promoted through the SDK installer's path rebasing to
`~/dev/symbian-sdk`; the previous 2,564-file SDK is retained at
`~/dev/symbian-sdk-before-hash-table-20261001`. Both trees verify all sealed
file digests. Root macOS CTest passed eight targets. The public PyPI payload
closure and broader guest Abseil/A11 execution remain open.

### 2026-10-02 PC Suite host USB transport

The connected Nokia 808 in owner-selected PC Suite mode was inspected with a
statically linked libusb 1.0.30 host extension. The default `device info` map
now includes endpoint directions/types and CDC unions. Opt-in AT battery and
signal codes, a read-only MTP session with two reported storage records and
bounded root listing, and a PC Suite OBEX Connect/Disconnect handshake were
observed. Both OBEX responses were `0xA0` after Disconnect included the
server-issued Connection ID. A low-level Python binding exposes synchronous
and queued asynchronous control/bulk/interrupt transfers with an explicit event
pump; the Connect/Disconnect sequence was repeated through that binding.
`otool -L` showed no dynamic libusb dependency; the wheel also packages the
static archive, header, and LGPL text for the host SDK. The installed wheel
audit passed, and the matching source distribution includes the new native
transport source and the pinned libusb build recipe. These observations do
not establish file transfer, SyncML, PC Suite command semantics, or debugger
access. See `docs/DEVICE.md` for the opt-in commands and `docs/RESEARCH_LOG.md`
for the experiment record. Focused device and CLI tests passed (37); the
full Python suite remains red in toolchain/image and verification cases
(165 passed, 356 skipped, 7 failed, 39 errors).

The host USB binding now returns typed native records and exposes A11-backed
transfer Futures through `AsyncUsbSession`. Python inspection and transfer
results are Pydantic models with documented fields and omitted unobserved
defaults. On the connected 808, typed descriptor, MTP, AT, and OBEX probes
retained their prior observed results; an awaited control read returned two
bytes, a pending interrupt Future cancelled, and the Future-driven OBEX
handshake returned `0xA0` to Connect and Disconnect. The focused device, CLI,
and model suite passed 44 tests. Bare `symbian` now prints top-level help
and exits successfully. Terminal help uses colour and clear section/option
emphasis; redirected help remains plain text. The wheel built, a clean virtualenv loaded
the typed native list API and Pydantic models, and its installed audit passed.
The matching source distribution contains the new native and Python sources.

Application-facing CLI help, Python docstrings, native binding descriptions,
SIS comments, and build guides now call application builds and packages
"applications". Existing `*-experiment` manifest kinds and report schemas
remain unchanged. Focused CLI and device tests passed (40). The project-build
and toolchain tests had 8 passes and 3 existing ARM unwind descriptor failures;
the wording changes did not touch that conversion path.
The rebuilt wheel passed its installed audit in a clean virtualenv, including
the updated native binding descriptions; the matching source archive built.

### 2026-10-02 Console interaction and device reconciliation

The desktop console now uses a shared sidebar and status bar for resolved
workspace, application, SDK and phone state. Application and firmware wizards
can skip optional pages, while additional settings remain available on demand.
Technical text uses a borderless native frame and host-derived font sizes.
Supported-phone views refresh on a five-second inventory cycle; a redacted
serial identity preserves selection across reconnects and mode changes, while
port-only identity is cleared after an observed disconnect. Stale phone probe
results are removed. `symbian console` launches a detached GUI process and
returns to the terminal. Focused console and device tests passed (27); a
hidden Tk smoke check loaded all five tabs and observed one selected phone.
The USB inspector, Protocols and Activity now scroll inside fixed side/status
chrome. Phone map and protocol results are cached by selector and applied
immediately when revisiting a phone; read-only USB mapping refreshes behind
the cached view. Profiling on this host measured CLI import at 0.147 s,
catalog at 0.001 s, USB inventory at 0.008 s, and the initial phone context
at 0.977 s. A USB-signature discovery cache reduced repeated unchanged
context calls to 0.004–0.005 s, with full revalidation after 20 seconds.
Focused console/device tests passed (28); a hidden Tk smoke check confirmed
the Protocols scrollbar appears for overflow at a 1000×620 window.
The expanded console, CLI and device suite passed 57 tests. The macOS ARM64
wheel rebuilt with the console modules, passed an installed-wheel audit in a
clean Python 3.12 environment, and that installation loaded 38 tasks and one
connected phone through the in-memory client.
An actual `uv run symbian console` invocation returned in 0.12 seconds with
exit code zero; the detached GUI child remained running. The wheel was rebuilt
and re-audited after the final scroll and device-label changes.
The generic Home/Tasks browser was removed. Six main tabs now group the 38
actions by application, firmware, emulator, SDK, device and activity purpose;
SDK and Devices have focused inner sections. A hidden Tk check verified every
action appears exactly once, all tab sections mount, and cached tab switches
completed in 0.0002–0.001 seconds on this host. The bridge now cancels its
scheduled drain callback during shutdown.
Guided action sections were also placed in scrollable viewports, so a long
review or result can be reached without moving the fixed sidebar/status bar.
After this navigation change, the 57 focused console/CLI/device tests still
passed, and the rebuilt wheel passed the installed audit again.
Technical result viewers now request 14 lines and ask their enclosing viewport
to reveal them when expanded. A hidden Tk layout check measured a 211-pixel
technical text area and a scrolled outer viewport with the scrollbar visible.
The empty action-form canvas observed on the no-input `doctor` action was
removed. No-input actions now present a direct run button; other forms fit
their actual content up to a 280-pixel cap and scroll only when needed. Hidden
Tk checks confirmed the doctor form container is absent, application inputs
use the capped scroll area, and direct doctor execution produced its outcome.
After this form change, the 57 focused tests still passed, and the rebuilt
wheel passed its installed audit in the clean Python 3.12 environment.
The GUI now retains the Aqua control theme with native selected-content blue,
a contrasting sidebar, white content surfaces and alternating table/action
rows. Hidden Tk checks confirmed the Aqua theme, selected colour `#0064e1`,
row tags, and touchpad wheel routing from an input to its form and then the
outer view at the form's edge.

### 2026-10-02 — desktop console frontend revision

- `symbian console` now selects a pywebview desktop shell, with bundled local assets and the existing FastAPI/httpx in-memory service. macOS uses WKWebView; Windows uses WebView2; Linux receives the PySide6 renderer dependency. wxPython is no longer a package dependency or launcher choice. The CLI still detaches from the terminal.
- Six icon-led sidebar sections and compact tabs replace nested subnavigation. Action cards use task icons and omit repeated section labels. Guided forms, cached view state, background context refresh, status indication, dedicated USB/protocol views and technical details remain available. The USB table uses named class labels, and inspected phone interfaces appear as a separate interpreted table.
- A live Cocoa smoke check loaded 38 actions and one connected 808 and navigated Devices/SDK tabs without JavaScript errors. The focused console tests passed (21), JavaScript syntax and Python lint/format checks passed, and the built wheel contained all local frontend assets. No device protocol request was made for this UI revision; Windows/Linux visual testing remains open.
- The follow-up presentation pass replaced nested sidebar entries with three-tab strips inside SDK tools and Devices, added consistent action and protocol icons, removed repeated section labels and an empty no-input form hint, and tightened action-card spacing. A live Cocoa check exercised tab navigation and a host-only doctor action. The final built wheel passed a clean installed-wheel audit; 21 focused Pytests, Ruff, Black and JavaScript syntax checks passed. `uv run symbian console` returned in 0.12 seconds and opened the WebKit-backed desktop process.

### 2026-10-02 — integrated console inspection

- Firmware now displays its imported catalog immediately, with search, selected-record details and source settings in a contextual right sidebar. Import, Inspect, Export and source probing open while the catalog stays visible. Emulator settings, host readiness and connected-phone inventory likewise load as live views alongside their related actions. All other action results have a bounded structured presentation; large firmware manifests expose a searchable file index before optional raw JSON.
- Required inputs are ordered before optional source settings, and simple actions run without a redundant review page. The optional-settings control no longer skips required steps. A live macOS check showed seven imported identities, an Export form prefilled with `e6`, an inspected firmware file search, effective emulator cards and the connected 808 card. No new phone protocol transaction or export was run. Twenty-two focused tests, Black, Ruff and JavaScript syntax passed; the rebuilt wheel passed its clean installed audit.

### 2026-10-02 — inline optional application settings

- The Create application form now uses an inline **Optional settings** disclosure in place of the vague **Configure more…** action. SDK, build and emulator inputs remain in the form, and Review stays visible. Legacy Tk and wx fallback buttons use the same label. A catalog-driven JavaScript render check verified those groups and the Review action; 22 focused Pytests and JavaScript syntax passed. The rebuilt macOS wheel passed its installed-wheel audit.

### 2026-10-02 — selected firmware inspection pane

- Firmware inspection is now driven by catalog selection instead of a separate action card. The catalog occupies the left side and the selected identity's device details, searchable manifest, provenance and raw JSON occupy the right. Inspection is read-only, runs in the background and caches each identity's result. Source settings remain accessible beneath the catalog. The raw JSON viewer now highlights only JSON, with no unrelated language selector.
- A hidden WKWebView check loaded seven imported identities, selected E6-00 automatically, indexed 13,186 files and measured the two pane columns at 323 and 611 pixels. The old Inspect action and language selector were absent. Twenty-two console Pytests, JavaScript syntax and the rebuilt installed-wheel audit passed. No phone protocol operation was run.

### 2026-10-02 — live selection and faster device status

- The Current selection sidebar now offers folder pickers for the working directory, application and SDK, plus a connected-phone selector. It shows full effective paths and updates automatically. The chosen working directory is passed to isolated CLI children; project and SDK selections feed context-aware form defaults. Clearing a selected phone suppresses automatic re-selection.
- A serial-free native USB topology observation runs every 750 ms, independently of the SDK command worker. On a changed topology it requests authoritative context reconciliation; if the selected vendor/product disappears, it suppresses the idle green status immediately while keeping pending work amber. A simulated removal in hidden WKWebView cleared the green status and displayed “Checking USB connection…” before the slower context request returned. A second live check observed the status sequence Refreshing firmware → Reading firmware details → Ready. Fifty-nine console, CLI and device Pytests, Ruff, Black and JavaScript syntax passed. No physical disconnect was performed.

### 2026-10-02 — console application selection

- The application picker and context discovery now accept `symbian.toml` as well as `symbian-project.json`. This permits selecting the checked-in `examples/gui_app` source project and keeps generated/legacy JSON projects selectable. JSON-only SDK settings are read only when that file exists. A focused bridge test selected `gui_app` and a JSON-only fixture; 59 console, CLI and device tests passed with Black and Ruff checks. The rebuilt wheel passed its installed-wheel audit, and the packaged manifest helper recognized `gui_app`.

### 2026-10-02 — C++20 app code and compact device cards

- `gui_app/app.cc` now directly uses `std::thread` and `std::make_shared` to run its existing Window Server loop on one owned event thread and return its result. The historical SDK adapter moved to `window_server.cc` because its placement-new declarations conflict with libc++ headers. The event thread creates its own cleanup stack; visual behavior and the stackless timer stay the same. The published E32 build was reproducible, the host GUI model test passed, the two preserved RM-807 GUI emulator replays passed, and all four opt-in debugger cases passed.
- Console device cards now reserve a small image column beside the device name, USB identity and interfaces. An attributed transparent Nokia 808 photograph appears only for identified 808 PureViews; other supported models use the generic SVG. The image is bundled locally with the WebKit page. A JavaScript render check confirmed one 808 image and two generic fallbacks; 59 focused Python tests and the rebuilt installed-wheel audit passed.

### 2026-10-02 — selected application workspace and build repair

- `symbian console --workdir PATH` now starts the detached desktop process in that directory, selects a project there when it has `symbian.toml`, and prefers its local `.symbian/app-sdk/sdk.json`. The selected SDK manifest is also passed into isolated command children. The Applications sidebar shows a compact indented entry for the selected project, using a bounded, project-local manifest icon when available.
- The application view reads TOML identity, architecture, UID3, caption, package name and current executable. It offers Build, emulator Run with an imported-firmware selector, and Package after an executable exists. Standalone TOML projects now use the shared emulator launcher with their selected source directory and SDK. The launcher remains responsible for ownership and stopping the emulator process.
- A `gui_app` build from the application directory initially failed only the independent ELF equality check: the two converted E32 executables had the same SHA-256, but DWARF `DW_AT_comp_dir` contained different nested CMake paths. Stable compilation directories in the GUI and generated-project CMake flags repaired the check without relaxing it. Fresh `gui_app` and generated-project builds returned `reproducible: true`, packaging produced `gui_app.sis`, and a standalone `session(project=gui_app)` reached READY and closed cleanly on imported RM-807 firmware. Twenty-seven focused Console tests passed, including a JavaScript initial-render regression check; this session check proves emulator startup, not a full UI interaction replay.

### 2026-10-02 — application and device view refinements

- An explicitly supplied `symbian console --workdir` now opens the selected application view when its TOML identity validates; the first paint shows a small loading state until context and project details arrive. On macOS, the launcher finds the invoking terminal's window through its process ancestry and places the detached WebKit window on that display. A local ancestry probe selected display 1 for the current terminal.
- The application view now uses the declared caption as its heading, with identity facts in one compact card. The build control uses a spanner icon, live tool output is copied to a private bounded-read log, and CMake compile groups appear as one compact disclosure instead of nested cards. A real reproducible `gui_app` build returned JSON success and wrote 334,031 bytes of tool output to the live log. The log integration test observed its first line while the child process was still running.
- EKA1 firmware entries are disabled for an EKA2 application run. Console Run requests a foreground emulator window and passes the console display index; the maintained EKA2L1 patch positions and activates that window. The patched emulator target rebuilt successfully. A direct foreground `gui_app` run opened its EKA2L1 window on display 1 at CoreGraphics bounds `(-58, -1035, 902, 725)` and reported it on screen; the supervisor was deliberately stopped after observing the window. The USB interface result view now uses short expandable rows, while firmware browsing defaults to a selected connected phone's matching model and retains a manual firmware choice. The Nokia 808 portrait was replaced with a transparent crop of the image supplied by the user. Thirty focused Console tests, JavaScript syntax, Black and Ruff passed. Window stacking above an actual console and cross-platform monitor behavior still need a human visual check.
- While a build runs, its tool log is expanded. The same log becomes a collapsed disclosure as soon as the result arrives, leaving the concise Build complete card prominent; the full log remains available on demand. The final focused suite still passed 30 tests.

### 2026-10-02 — GUI Run host debugger repair

- The IDE's Debug action on GUI Run failed before launch because it selected a nonexistent bundled GDB for macOS. The local IDE generator now supplies a host LLDB debug profile and selects it when generating GUI Run settings; the separate ARM GDB profile remains for GUI Debug. Both ignored `.idea` project windows were regenerated with the new profile.
- `/usr/bin/lldb` launched the native `gui_app_run` launcher, stopped at `main` (`launcher.cc:10`) and showed a source backtrace; the process was killed before starting the emulator. The ARM GDB wrapper reported GNU GDB 17.2. Two focused IDE configuration tests, Ruff and Black passed. The still-open IDE retained its former GDB selection in memory after the XML update, as the owner's next screenshot confirmed. Select GUI Host LLDB in its debug-profile toolbar or reopen the project before using Debug. The ARM guest requires GUI Debug with Symbian GUI GDB selected.

### 2026-10-02 — live application Run output

- The selected application's Run action now expands a live output panel during build, launch and emulator execution. The console captures SDK tool and launcher diagnostics incrementally and reads a bounded tail of the owned emulator's `frontend.log`; the panel collapses after the run and remains available. The session log path is passed through a private sidecar and checked against the selected project's run directory before reading.
- A real `gui_app` emulator run showed tool output before completion and 11,807 bytes of frontend output while the process was still active; the combined console reader returned both. That verification run was stopped after the log check. Thirty-three console/frontend tests, Ruff, Black and JavaScript syntax checks passed.

### 2026-10-02 — IDE debugger selection state corrected

- The owner's next GUI Run Debug attempt still chose the nonexistent bundled GDB even after restarting the IDE. Inspection of the installed CLion 2026.2 plugin showed that its active debugger is persisted in `SelectedDebugProfileService`, keyed by run configuration and CMake profile. The prior generator had written an obsolete `CurrentDebugProfile` component while the actual GUI Run key still pointed to an empty non-shared GDB profile.
- The generator now replaces only GUI Run's active selection with the host LLDB profile ID in the actual service state, preserving other targets' choices. Both ignored IDE project workspaces were regenerated and show GUI Run mapped to the installed LLDB profile. Focused configuration tests cover an existing broken GDB mapping and unrelated debugger preservation. An already-open IDE can retain service state in memory; its live Debug action still needs a reload or toolbar selection to confirm the repaired mapping.
- A subsequent live Debug attempt still used `.symbian/clion-setup/gui-gdb` for `GUI Run`, then rejected the macOS `gui_app_run` executable as an unknown format. That confirms the still-open IDE kept its old debugger choice; it does not indicate a malformed host executable. `file` identifies the target as arm64 Mach-O, and `/usr/bin/lldb` launched it, stopped at `launcher.cc:10` and showed a source backtrace. The saved root and dedicated `SelectedDebugProfileService` entries both resolve `GUI Run` to `/usr/bin/lldb`; an IDE project reload or explicit GUI Host LLDB selection is required to replace the live cached choice.
- With **GUI Host LLDB** visibly selected, a live IDE Debug session started and paused at `_dyld_start` after the launcher `exec` into Python. The IDE log recorded its LLDB frontend, and the traced Python supervisor was present. The owner clicked **Resume Program** in the Debug tool window and confirmed that the emulator ran. This is LLDB's default stop-on-exec behavior, not a launch failure. Guest source debugging remains the separate **GUI Debug** / **Symbian GUI GDB** path.
- The owner confirmed the missed breakpoint was in Symbian application C++ while the selected configuration was **GUI Run** with host LLDB. The generated IDE state now also binds **GUI Debug** to the ARM GDB profile in both root and standalone projects. The backend was exercised on the current GUI build: `GuiMain` and `DrawGui` source breakpoints hit, and the GDB machine interface relocated a breakpoint set before remote connection, then stopped and stepped in guest code. Four focused configuration/live tests passed; their old literal instruction-address expectations were replaced with assertions against the current build's breakpoint addresses. The owner selected **GUI Debug** / **Symbian GUI GDB** and confirmed the live IDE stopped at the guest breakpoint. The IDE log recorded `GUI Debug` launching the generated GDB wrapper in MI mode; full stack unwinding remains unverified.
### 2026-10-04 — A11 channel interface correction

- Host `<thread/channel.h>` now uses the pinned A11 header and waiter state.
  The guest `<thread/channel.h>` has A11's public `Reader`, `Writer` and
  `Channel` signatures, including selectable read/write cases,
  `WriteUnlessCancelled`, `length()` and zero-capacity rendezvous.
  `EventMailbox` instead uses `symbian::concurrency::BoundedChannel` for its
  fallible, nonblocking enqueue and idempotent shutdown behavior.
- The focused host concurrency test passed, and the guest channel probe passed
  all eight ARMv5T/ARMv6 × Dyncom/Dynarmic normal/changed cases, covering
  buffered transfer, rendezvous, competing cases, timeout, cancellation and
  losing-case ownership. The A11 source pin check verified 46 sources and 92
  includes. A fresh `.symbian/a11-channel-sdk-final-20261004` export contains
  matching guest headers, both ARM fiber archives and verified digest entries;
  the full eight-case guest matrix passed against that export. The rebuilt
  host channel and fiber tests passed 2/2. Full A11 concurrency parity is still
  open: fiber trees, joining, shared pool scheduling, structured futures and
  original test mapping.

### 2026-10-04 — component device API foundation

- Added `cpp/symbian/api/` with separate source boundaries for system,
  connectivity, power, media, display, sensors, camera and storage. Each
  implemented component is an independent opt-in `Symbian::<Component>`
  archive; the SDK does not create targets for planned components without an
  archive. The API rules and verification sequence are in `docs/DEVICE_API.md`.
- The first implemented component, `Symbian::System`, reads native tick and
  fast counters with their reported period or frequency and returns typed
  `absl::StatusOr` results. A host GTest passed native-error and zero-metadata
  cases. The packaged API probe passed all eight ARMv5T/ARMv6 ×
  Dyncom/Dynarmic normal/changed emulator cases. The fresh SDK export at
  `.symbian/device-components-sdk-20261004` contains both archives, its public
  header and verified digest entries.
- At this checkpoint, connectivity, power, media, display, sensors, camera
  and storage have component directories and plans only. Their native
  contracts and device behavior remain unverified; no physical-device API
  test was performed.

### 2026-10-04 — HAL and File Server device components

- Added separate `Symbian::Power`, `Symbian::Display` and `Symbian::Storage`
  archives and modern public headers. Power reports independently optional
  power-good, external-supply and qualitative battery fields; it never guesses
  a percentage or charging state. Display reports positive primary HAL pixel
  geometry with optional twips. Both keep legacy HAL headers behind native
  bridge translation units.
- Storage provides move-only read-only file and directory owners. Reads write
  directly into a caller span and directory entries stream one at a time.
  Each owner closes its File Server session and subsession; its lifetime stays
  on the opening worker thread. File offsets beyond the initial 2 GiB profile,
  writes, subscriptions and cross-thread session ownership are not claimed.
- The fresh `.symbian/device-api-final-sdk-20261004` export packages the four
  component archives for ARMv5T and ARMv6, HAL/File Server proxies and public
  headers. Seventeen checked assets match SDK digests; all four public headers
  match source. The host device API GTest passed. Its packaged guest probe
  ran successfully in all eight ARM architecture × Dyncom/Dynarmic ×
  normal/changed event-executor cases; the changed control belongs to the
  event executor, not the new device APIs.
  On the RM-807 emulator fixture, a read of `Z:\sys\bin\euser.dll` was denied
  by the platform; a read of `Z:\resource\psui.r01` and a streamed resource
  directory entry succeeded. No physical Nokia 808 behavior is established.
- Connectivity, sensors and media remain planned rather than exported.
  Native service contracts, permissions, buffer ownership and cancellation
  still need their own evidence before exposing those APIs.

### 2026-10-04 — explicit storage writes and incremental copying

- `Symbian::Storage` now adds a move-only `WritableFile` with explicit
  create/open/replace modes, positional writes from caller memory, and File
  Server `Flush`. `CreateDirectories` is a separate operation. The public
  header does not require `RFs`, `RFile`, `RDir`, descriptors or `User::`.
  Original platform headers and import proxies remain available to an
  application that explicitly opts into native API translation units.
- `FileCopy` owns one reusable 32 KiB buffer. `Step()` performs at most one
  read and write and returns partial progress; `Cancel()` atomically requests
  a stop before a subsequent request. `DirectoryReader::Next()` yields one
  entry per call, and its `Cancel()` stops at the next checkpoint. This bounds
  work and memory *per step*, not the
  wall-clock latency of an in-flight synchronous File Server request.
  Verified native asynchronous cancellation and drainage remain open work.
- Eight host device API GTests passed, including explicit replacement,
  progressive copy and cancellation before another chunk. A packaged guest
  probe created and flushed a private `C:` file, read it back and copied it
  through `FileCopy`; all eight ARMv5T/ARMv6 × Dyncom/Dynarmic ×
  normal/changed executor cases passed in disposable emulator copies. This
  does not establish physical-device write behavior or hardware durability.
- The candidate connectivity monitor remains unexported: the prepared client
  header/import contract is missing, and the emulator server currently
  reports a fixed connection count and bearer rather than observed state.
- Final export `.symbian/device-api-final-stream-sdk-20261004` packages both
  storage archives with the allocation-before-replacement correction. All
  3,271 SDK asset digests matched; a fresh ARMv5T/Dyncom guest probe passed
  with that exact export. The full eight-case matrix passed immediately before
  the allocation-order-only correction.
- The subsequent `.symbian/device-api-cancel-sdk-20261004` export adds
  `DirectoryReader::Cancel()` to the packaged public header and archives.
  Its 3,271 asset digests matched, the packaged storage header matched source,
  and a fresh ARMv5T/Dyncom guest probe passed.

### 2026-10-04 — SD analysis and camera discovery

- Root `SD_STORAGE.md` records software opportunities for SD filesystem
  throughput: fewer larger I/O calls, drive-reported buffer hints, deliberate
  cache modes, pre-sizing and flush placement. These are hypotheses pending
  controlled measurements on a physical removable card; the current 32 KiB
  copy step stays as a responsive default.
- `Symbian::Camera` exports `DiscoverCameras` with a typed count and explicit
  native errors. It does not reserve, power on or capture from a camera.
- Nine host device API GTests passed. The camera SDK export at
  `.symbian/device-api-camera-sdk-20261004` packages ARMv5T and ARMv6
  archives, ECam headers and the `CamerasAvailable` import stub. All 3,307
  packaged asset digests matched. A packaged
  ARMv5T/Dyncom guest probe passed with runtime discovery. This first export
  does not establish physical camera or SD throughput behavior.
- A candidate `InspectCamera` was removed before export: its `CCamera::New2L`
  leave-catching path first produced unresolved `TTrap` symbols under the
  wrong leave mode. The exception-based ARM EABI mode linked after adding
  frozen `drtaeabi` imports, but ELF-to-E32 conversion then rejected its
  imported C++ type-info vtable because that path only accepts function
  imports. Opening without a proven converted leave boundary would be unsafe.

### 2026-10-04 — development service design

- Root `DEVELOPMENT_AGENT.md` defines a staged on-device service and host
  integration for USB/Wi-Fi discovery, authenticated sessions, scoped file
  transfer, logs, process control, screen, ordinary app deployment and user
  process debugging. It is a design, not a running service. Its resident idle
  behavior and actual USB/WLAN capability need on-phone measurement.
- Recovery and flashing are explicitly absent from the service protocol and
  remain under the separate human-governed broker required by `PLAN.md`.
- An A11 reference pass refined the protocol design: keep `WireStream`
  lifecycle and `ChunkStoreReader`/`ChunkStoreWriter` backpressure concepts,
  but start with a one-session, 64 KiB frame and 256 KiB inbound phone profile.
  HTTP remains a host-side console API rather than a phone-side dependency.
- The plan now has an explicit emulator-to-development-phone gate: a verified
  ordinary SIS and read-only authenticated handshake precede manual install,
  on-phone idle/recovery checks and later boot-start enablement. This gate has
  not been executed.

### 2026-10-04 — resident UI, movable emulator window and PC Suite staging

- The agent now registers a Development Agent menu entry and shows a local RUNNING panel with BACK and STOP controls. BACK hides the panel while the authenticated listener remains alive; STOP closes the listener and exits with guest reason zero. A clean SDK export included the required WS32/GDI imports and a fresh ARMv6 executable and SIS. Four opt-in guest checks passed in the disposable RM-807 emulator, including authenticated status after BACK and STOP exit. The guide includes a captured guest image. This is emulator behavior, not a Nokia 808 background or idle-power result.
- EKA2L1's macOS background-window patch now keeps the window movable and ordered in front without making it key. The patched frontend rebuilt. In a live launched emulator, macOS System Events reported its window at `(414, 198)`, accepted a move to `(444, 218)`, and reported that new position; `iTerm2` remained the frontmost process before and after launch. A literal pointer drag was not separately observed.
- The SDK now exposes reusable native `StageMtpSis` and Python `symbian.device.mtp.stage_sis`, used by both `device install` and the Development Agents console. The native transfer requires a serial-matched device, one MTP interface, required PTP operations, a writable store with a unique root `Installs` folder, sufficient free space and a nonconflicting content-addressed SIS name. It reads back the whole object and checks SHA-256; the device installer is never invoked. A 16 MiB transfer ceiling bounds memory use.
- On the connected 808 in PC Suite mode, MTP interface 0 reported writable Mass memory (`0x00020001`) and root Installs handle 4. The SDK staged `agent_service-fec19755d808.sis`, read back SHA-256 `fec19755d8083fa2a1d5a12246f86874d7fef54f96cb41d2f063055a56042933`, and a second call returned object handle `16777391` with `copied: false`. This is transfer evidence only. The package has a public emulator test certificate; on-phone install, execution, pairing and idle-power behavior are unverified.
- A later local rebuild changed the agent SIS digest to `ccaa4073eb257e2185fd37a6e8b32627d00e6875c2a800ace4266a04a6a52203`. A final PC Suite check found the matching object handle `16777392` with `copied: false`; the SDK did not overwrite the earlier content-addressed file. The provenance of that second on-phone object was not established by this check.
- Focused host device/console tests: 29 passed. Native extension rebuilt successfully with the MTP staging implementation.

### 2026-10-04 — agent project, IDE targets and phone status follow-up

- The resident agent is now a root project at `agent_service/`, with its own application manifest and package version 1.0.1. The final active SDK export at `.symbian/resident-agent-ide-sdk-20261004` includes the narrow WS32 `RWindowGroup::SetName` import. Its ARMv6 executable SHA-256 is `9e91b92674b065907ebc446608891e18c5530d271a917d13dfbe07ca5fc3029d`; the unsigned SIS SHA-256 is `70118e192e46dfa663ea10f5f682ea2ad4e22e231f43e9914a642f1383b6b34f`. Four opt-in guest agent tests passed, including BACK retaining the listener. The agent now gives its WindowServer group an AppArc-compatible name and signals an already-running instance to raise its panel when launched again. Reopening from the actual Nokia 808 application menu remains unverified because this revised SIS has not been installed there.
- The Development Agents console can record an owner's explicit “running” observation for a connected phone, keyed by a serial-derived identity digest, and displays it separately from a live authenticated status. The current phone build binds loopback only, so USB enumeration cannot prove installation or service health. The connected 808 was unavailable during the final check; the revised package was not staged or installed on it.
- The root ARMv6 IDE preset now selects the active SDK and discovers registered root and example applications through their manifests. The SDK's CMake executable helper attaches native files in an application's root and conventional `src/`, `cpp/` and `include/` trees to its real target. A fresh `clion-guest-probes-armv6` configure generated `compile_commands.json` entries for all seven agent and five GUI application translation units; both targets built. A newly added `extra.cc` in a generated `symbian init` project appeared in the compile database and its focused project test passed. Unknown `_probe` examples receive a generic object target automatically.
- The EKA2L1 patch now removes `WA_ShowWithoutActivating` after the first show and keeps a movable macOS window. A live CoreGraphics pointer drag moved the window from `(414, 198)` to `(474, 238)`; the patched frontend rebuilt. On this host the functional window activates the emulator at launch. Attempts to restore terminal focus made the emulator window fall behind full-screen terminal windows and prevented pointer selection, so launch focus still needs a separate fix. This is host UI evidence, not Nokia 808 compatibility.
- Final checks: 31 focused host tests passed, one live `symbian init` build test passed against the active SDK, four opt-in guest agent tests passed against that SDK, both root IDE application targets built, Black/Ruff passed on changed Python, `git diff --check` passed, and strict MkDocs built. USB discovery found no connected phone during the final check.

### 2026-10-04 — CLion linker and nested application sources

- The root ARM toolchain now forces compile-only CMake probes and pins `CMAKE_LINKER` to the selected SDK's `ld.lld` instead of accepting a cached macOS `/usr/bin/ld`. The ignored local CLion preset was changed to stop overriding the SDK's compiler/linker, while retaining its matching LLVM scanner path. The user's exact ARMv6 preset with a fresh cache configured successfully; its cache names the newly exported SDK's `clang++`, `ld.lld` and `STATIC_LIBRARY` probe mode. This is a host toolchain check, not a guest loader result.
- Application target discovery now follows native files recursively under the project source tree, excluding build output and separately managed dependency directories. A fresh source SDK export at `.symbian/resident-agent-index-v2-20261004` contained the new CMake helper; a moved `symbian init` app with `modules/widget/extra.cc` built and listed that file in `compile_commands.json`. The strict MkDocs build passed.

### 2026-10-04 — Abseil time and native IDE coverage

- Public TCP, TLS and tick-period interfaces now use `absl::Duration`; the resident agent uses `absl::Time` for request deadlines. Internal TLS transport bookkeeping retains a monotonic `steady_clock` boundary. A fresh source SDK at `.symbian/absl-time-sdk-20261004` exported successfully. The ARMv6 agent, GUI and connectivity index targets built; four opt-in agent guest tests and all 24 authenticated TLS guest handshake cases passed against that SDK. One host native device API test and the strict MkDocs build passed. These emulator results do not establish Nokia 808 behavior.
- The root guest IDE profile now loads the actual device API, TLS and guest concurrency component CMake targets. Automatic indexing covers remaining project-owned native files in `cpp/symbian`, `agent_service` and `examples`; the host profile covers host C++ bindings, tests and the tracked A11 source snapshot. Guest-only sources have no competing host placeholder, so CLion selects their ARM target and generated include directory. In the final compile databases, all 66 owned `cpp/symbian` guest translation units, 7 agent units and 84 example units were indexed; the 19 tracked A11 snapshot units were indexed by the host profile. `tls_server.cc` built from its real ARM component target, and its installed Mbed TLS headers exist at the compile command's include path. The generated `agent_certificates.h` exists under the ARM build directory.
- The first generic scans traversed multi-gigabyte local `.symbian` trees before filtering them, which made CMake reloads slow. Both the root indexer and exported application helper now skip generated directories before descent. Their isolated scan times were 0.18 and 0.10 seconds. A fresh active SDK at `.symbian/index-fast-sdk-20261004` exported the corrected helper. The exact `clion-guest-probes-armv6` preset configured from a fresh cache in 2.15 seconds; the host `debug` profile configured in 2.53 seconds. The ARM root declares ASM at `project()` time; the host profile leaves guest ASM to the GUI subdirectory. A separate ARM assembler target built, and both final profiles generated without `CMAKE_ASM_COMPILE_OBJECT` errors.

### 2026-10-04 — resident agent stop and package follow-up

- The agent's scheduler now subscribes to a local Symbian property for STOP instead of polling a timer every 100 ms. The UI thread signals the property on a local STOP or window failure; an accept rearm failure also stops the service instead of leaving a stale listener. The watcher uses fixed process storage and existing EUSER property imports. Four opt-in guest tests passed on the pinned emulator, including authenticated status after BACK and a zero-reason STOP exit. The event-driven design removes the periodic scheduler stop wakeup; physical idle power and sleep behavior remain unmeasured.
- Version 1.0.2 built reproducibly as E32 SHA-256 `bbb5969927ece056b1c1af25bbbacdd00e6f33a63aaa98803ea1efdcee188b69` and unsigned SIS SHA-256 `f24342b229e22de23120bd2164af66b3ba895570808bb0e0b5253a147f92da39`. SIS inspection found three files, a registered application, executable UID `0xe0000a31` and version `1.0.2`. The bundled certificate remains a public emulator fixture. Neither the new package nor panel reopening has been tested on the Nokia 808.

### 2026-10-04 — resident agent consumes SDK platform services

- The resident agent now uses `Symbian::System`'s `RunActiveService` and `RequestActiveServiceStop` for its native scheduler and property signal, and `Symbian::Display`'s `RunResidentPanel` and `RequestResidentPanelForeground` for Window Server presentation. The agent owns only service policy, protocol, and app-specific panel options. Its remaining original Symbian includes are the process startup and RM-807 entropy boundaries. The new public headers use `absl::Status` and ordinary C++ types; their native implementations are separate translation units. `Symbian::Display` now links its WS32 and GDI proxies for consumers.
- Both new component archives compiled for ARMv5T and ARMv6. A fresh source export at `.symbian/agent-refactor-final-20261004` packaged the new public headers and archives. The agent built and linked from that package. The first four-case RM-807 guest run passed three cases; the STOP case exposed a duplicate native scheduler Stop and a `KERN-EXEC 3` exit. After making the SDK watcher solely responsible for stopping on its property event, the focused STOP case passed with process exit reason zero. The combined evidence covers the four existing test cases, but the complete suite has not been rerun after that callback correction. Strict MkDocs, Doxygen and link checks passed; Doxygen still reports pre-existing upstream-header parsing warnings.
- The application keeps its TLS test certificate embedded. It performs no file I/O, so the SDK storage component was not added merely for this refactor. The UI still uses its own guest thread because its Window Server request wait is thread-affine; the TLS work uses the SDK `WorkerExecutor`. No Nokia 808 behavior was established by the emulator results.

### 2026-10-04 — agent status transport without TLS

- The resident agent now uses a 32-byte-key challenge response over bounded TCP before reading any MessagePack control frame. A public-key emulator build binds `127.0.0.1:39101`; a phone-specific build requires a private key file and an eight-character handset pairing code and binds IPv4 port 39101. Both peers prove the key with fresh nonces and HMAC-SHA256; the protocol **does not encrypt traffic**. The guest links `Symbian::Crypto` (`libmbedcrypto.a`) and no TLS/X.509 archive. `Symbian::Tls`, Mbed TLS source/headers and the project-local CA option remain in the SDK for applications. The host session and CLI now take `--key-file`. USB discovery and MTP SIS staging remain separate from the Wi-Fi status socket.
- The selected SDK at `.symbian/agent-refactor-final-20261004` built the public agent E32 (SHA-256 `f87ac41496f0283eef6093226eda0e106bfd4fcf52a6199d566e44ab0e79f073`). The pinned RM-807 emulator suite passed four guest cases across Dynarmic/Dyncom after the protocol change: panel BACK/STOP, authenticated status and logs, rejected control, deadline and reconnection. A separate phone-key profile passed the Dynarmic status/recovery case. Host session, console and private-key tests passed (23 tests). The root ARMv6 IDE CMake preset configured in 0.5 seconds and retained the real `agent_service.cc` compile target with its generated header. ARM/emulator results do not prove Nokia 808 compatibility.
- The connected USB candidate `usb:0421:05d1:83066b7e94dab4b0` supplied a serial-derived identity anchor. The SDK created a distinct private key outside the repository with mode `0600`, and protected the phone build tree with mode `0700`. The version 1.0.3 phone-specific E32 has SHA-256 `ea52ec218a55e38c9a18ceae06098aa6ba214125694dcc2216a8a14b006e3b13`. The unsigned SIS has SHA-256 `d2273dbc8f623e6b6fa4cf59e30c2adb9f1e874ee524fa4ad44532cac277582c`. PC Suite MTP staged it as `Installs/agent_service-d2273dbc8f62.sis` and verified readback; `on_device_verified` was false. The owner must install and open that exact SIS, compare the handset panel's pairing code with the console, and enter the phone's Wi-Fi IPv4 address before a live status can be established. The package content and pairing code are not proof of physical execution.
- The console now remembers the phone's local identity across restarts, builds/stages a phone-specific SIS, and distinguishes owner reports, staged packages, fresh authenticated status and stale prior readings. The status check rejects non-IPv4 addresses and requires the pairing-code confirmation. This GUI path has host tests; its live Nokia 808 outcome remains pending.

### 2026-10-04 — first phone-side agent observation

- The owner confirmed that the newly installed 1.0.3 agent panel on the Nokia 808 displays `PAIR OORGRKIN` and that the phone's Wi-Fi settings show `192.168.1.32`. This is an owner observation of installation, startup and panel rendering on that handset; it is not an authenticated host status response. The Mac's `en0` route and ARP entry reached that address, and two ICMP echoes returned. A direct `symbian agent status` attempt to TCP port 39101 timed out before the server challenge. The console therefore correctly remains at a reported/unchecked state. The host now distinguishes a TCP connect timeout from a challenge/proof timeout in its status error.
- A focused guest test with a wrong key passed after mapping native connection resets to `UNAVAILABLE`, and the current host session/identity/console suite passed 24 cases. The physical timeout is not explained by the emulator pass. A temporary Mac HTTP endpoint at `192.168.1.203:39102` is available for an owner-operated reverse-direction Wi-Fi check; its phone result is pending.

### 2026-10-04 — automatic phone-initiated agent transport in progress

- The owner confirmed the Nokia 808 browser displayed the Mac's one-line network check. The Mac HTTP server logged requests from `192.168.1.32`; the test server was stopped. Phone-to-Mac browser TCP works on this Wi-Fi session, while direct Mac-to-phone agent TCP 39101 still timed out. This is directional network evidence, not an authenticated agent status.
- The agent's proposed 1.0.4 private profile no longer stores host or phone IP addresses. It broadcasts a fresh-nonce HMAC discovery request, checks the console's HMAC offer, then dials its responder on TCP 39103. The console opens listeners only during a status check and authenticates the existing read-only session. The public emulator profile retains its loopback listener. A reusable SDK `BroadcastProbeFor` interface and cancellable `ConnectIpv4` path compile for ARMv6. One first private link exposed missing original ESOCK/INSOCK proxy imports; the SDK export now includes those imports. Final private linking, guest discovery and physical installation remain pending.
- Public TCP `ConnectIpv4`, `Send` and `Receive` now take absolute `absl::Time` deadlines with `InfiniteFuture` defaults. Their native timer waits in intervals for long deadlines without cancelling an in-flight request between intervals. Host session/identity/console tests passed 26 cases; the existing four public agent guest tests passed after the discovery branch was added. The long-deadline guest behavior and private reverse connection have not yet passed a guest run. Strict MkDocs built with the new protocol guide. No Nokia 808 compatibility claim follows from these host/ARM/emulator checks.

### 2026-10-04 — absolute deadlines, signed agent SIS and phone install gate

- The public TCP client/listener, UDP broadcast probe and TLS server read/write/accept APIs use absolute `absl::Time` deadlines with `InfiniteFuture` defaults. The native `*For` socket entry points were removed. The native deadline bridge keeps 64-bit epoch/remaining values where necessary and passes 32-bit intervals to `RTimer`. A focused guest receive/accept regression passed against `.symbian/agent-deadline-fixed-sdk-20261004`; the first implementation had allowed a short deadline to overrun, and the test caught it. The private keyed discovery/authenticated-status emulator test passed twice with the tracked EKA2L1 Belle receive opcode patch. These remain emulator observations.
- The 1.0.5 capability-enabled but unsigned SIS was **rejected by the Nokia 808 installer** with `Requested application access not granted`, as reported by the owner. This is a physical installation result; the precise installer policy behind it is not independently observed. The native SIS library now self-signs canonical packages with a matching RSA PEM identity, verifies the signature during inspection and rechecks the unsigned canonical contents. A package signing test passes for valid and truncated SIS and rejects an invalid key. The phone-specific agent build keeps the signing identity in a private per-phone directory, separate from the protocol key and Git.
- Agent version 1.0.6 was built against the selected SDK with E32 `NetworkServices` mask `0x2000`. Its signed SIS SHA-256 is `b0fe48f06c679ef94960aafaa2bc4cf0bc3e74c3832ac679d0e6c6616f6fd0fa`; native inspection reports a verified signature and three embedded files. PC Suite MTP copied `Installs/agent_service-b0fe48f06c67.sis` and verified readback. The owner has been asked to install it. `on_device_verified` remains false, and keyed discovery/live status have **not** passed on the phone.
- Native SIS tests passed 10/10; Python package tests passed 13 with 3 opt-in skips; host agent/device tests passed 25. The maintained ARM integer probe now emits a proper Symbian unwind descriptor for the compiler's `.ARM.exidx`, and its verification gate accepts exactly its four descriptor relocations. This fixes a host test setup failure without making a hardware claim.

### 2026-10-04 — reusable capability-aware SIS packaging

- The owner reported the **signed 1.0.6** SIS still failed installation with `Requested application access not granted`. Original Symbian installer source shows that it compares each SIS file description's capabilities with its executable header; the 1.0.6 SIS omitted the file description field even though its E32 requested `NetworkServices`. The native SIS writer now emits type 41 capabilities from the inspected E32 image for every executable package and the native inspector exposes the field. This is a concrete format correction; the 808's final installer policy remains unverified.
- Signing is available through the reusable native `SignPackage`/`InspectPackage` API, Python package policy and `symbian package --signing-certificate CERT --signing-key KEY`. The agent obtains its self-signed identity through a general private-identity module. The report distinguishes `signed` from `unsigned` without claiming all supplied certificates are self-signed or trusted. A native test covers `NetworkServices` in the SIS description. After this change, native SIS tests passed 11/11 and focused package/agent/device Python tests passed 38 with 3 opt-in skips.
- A phone-specific 1.0.7 SIS now has `0x2000` in both E32 and SIS file metadata, a natively verified signature, and SHA-256 `77391387488a6b19b9c7974f9d0531d4d1bbd5b7844be52a1e3f30e3cd3b36ac`. PC Suite MTP staged `Installs/agent_service-77391387488a.sis` with matching readback. The owner has been asked to install it. No 1.0.7 on-device result has been received yet; live status remains unverified.
- Two selected authenticated TLS guest handshakes passed against `.symbian/agent-deadline-fixed-sdk-20261004`, covering TLS 1.2 and TLS 1.3 after the deadline API migration. That emulator evidence does not establish Nokia 808 compatibility.

### 2026-10-04 — Nokia 808 agent installation and outbound handoff

- The owner confirmed **1.0.7 installed** on the Nokia 808 and its panel displayed the expected phone-specific pairing code. A 60-second host listener verified at least one keyed UDP discovery from that device but received no outbound TCP connection on port 39103. This is direct physical evidence of installation, UI startup and keyed UDP transmission, but not a verified agent session. The failed handoff could be an offer receive/validation problem or a TCP dial failure; the host alone cannot distinguish them.
- The selected SDK was refreshed at `.symbian/agent-diagnostic-sdk-20261004` with a reusable `ResidentPanelOptions.heading_provider`. It calls a static-label callback on the Window Server thread and checks for changes on the panel's existing wake timer. The agent uses atomic link phases so a phone panel can show `NO OFFER`, `DIAL ERR` or `AUTH` while preserving the pairing code. Version 1.0.8 built from that active SDK, retained the verified SIS/E32 `0x2000` capability match and signature, and was staged via PC Suite MTP at `Installs/agent_service-65b3856c5970.sis` (SHA-256 `65b3856c59701d46660d7afa0bcd167e958b3e3095a7f33bd20d78ccf1324380`). The owner has been asked to install it and report the displayed heading; no 1.0.8 physical result is claimed.
- The private keyed discovery/authenticated-status guest test passed against the diagnostic SDK with a separate ephemeral test key. It exercises the new panel-linked build but remains an emulator test. The diagnostic SDK export includes the reusable signing CLI and native package capability rule.
- The owner opened 1.0.8 and its diagnostic heading reported `NO OFFER` (rendered with missing F glyphs). A concurrent 60-second host listener received no keyed packet; two pings to the previously confirmed phone IP also timed out while the Mac remained on Wi-Fi. This differs from the earlier 1.0.7 keyed UDP receipt, so current phone WLAN state is being checked before changing discovery. Four missing diagnostic glyphs were added in source for a later SDK/UI export. No authenticated phone session exists yet.

### 2026-10-04 — authenticated Nokia 808 agent status

- The owner confirmed the 808 still used `192.168.1.32`; the Mac subsequently received two ICMP replies from that address after the earlier timeouts. With the installed 1.0.8 agent open, `symbian agent listen` on the Mac then completed in under five seconds: a keyed UDP discovery and offer, phone-initiated TCP connection, mutual HMAC authentication, Hello negotiation and status request all succeeded. The peer was `192.168.1.32`; the response reported `service=symbian-agent`, `state=ready`, capabilities `status` and `logs`, display `360×640`, tick count `13957484`, and period `15625` microseconds. This is a direct physical Nokia 808 result for this session and build. It does not establish reliability across WLAN reconnection, sleep or reboot, nor full device compatibility.
- The console bridge's `verify_agent_status` action was invoked against the USB-discovered `808 PureView` and returned `authenticated=true`, the same `ready` status, the expected pairing code `OORGRKIN`, and a fresh UTC check time. This directly exercises the GUI's backend action on the phone, although the rendered pywebview button was not observed in this run. The earlier missing discovery and ping timeouts remain evidence of intermittent reachability; the successful handoff rules out a persistent protocol or capability failure in this session.
- While preparing a further read-only log check, the owner reported that the phone rebooted. The pending host listener had received no agent connection and was interrupted; it issued no agent control request. The installed 1.0.8 protocol has no reboot operation. Timing and cause are undetermined, so physical agent checks are paused while the reboot circumstances are clarified. The host-side bounded log reader and console event display remain unverified on the handset.
- Host-only follow-up: `ReadOnlyAgentSession.recent_logs()` now walks at most five pages of the agent's fixed 32-record ring and returns the newest eight with a preserved gap flag. The console has a separate, explicit **Read service events** action after live status, so checking status does not trigger extra log requests. The SDK-owned guest log exposes Abseil time in its public header and clamps elapsed microseconds if the wall clock moves backward. Host agent/console tests passed 24 cases; the native agent-frame test passed; the ARMv6 guest agent archive compiled. No new phone package was installed or request sent for these changes.
- The owner supplied a screenshot of the actual Symbian Console **Development Agents** card showing `808 PureView`, `Verified live · ready`, Wi-Fi peer `192.168.1.32`, and an authenticated UTC timestamp of `2026-10-04T21:43:45.038471+00:00`. This closes the earlier unobserved rendered-GUI check for that session. The screenshot does not identify whether the app was relaunched after the reported reboot or establish reboot stability.
- The verified card now renders a local readable check time and a green verified badge; no protocol or phone package changed. JavaScript syntax, 24 focused host tests, strict MkDocs and whitespace checks passed. The new presentation itself has not been inspected in a live pywebview screenshot.
- The opt-in Dynarmic resident-agent lifecycle case passed against the selected diagnostic SDK in a disposable RM-807 emulator instance: it opened an authenticated read-only session, read status and logs, rejected a wrong key/bad frame, and recovered a new connection. The case now also checks the host's `recent_logs()` against the guest's log cursor. It passed in 21.55 seconds after the new assertion. This is emulator evidence only and does not explain or rule out the physical phone reboot.

### 2026-10-04 — transport library review and phone log CLI

- The worktree was clean and local `main` equalled `origin/main` at `0264821` when this stream began. An upstream and A11 review found libnghttp2 usable as an optional host HTTP/2 codec, with a plausible but untested guest lib-only port. ngtcp2 has no supported crypto helper for the SDK's Mbed TLS 3.4.1 profile, and HTTP/3 would also need nghttp3. No new transport dependency or protocol was added; the decision and evidence are in `.dev/research-log.md`.
- `symbian agent listen --logs` now returns the newest bounded service events from a phone-initiated authenticated connection; `--after` and `--limit` allow a cursor page. The same native MessagePack codec and host `ReadOnlyAgentSession` serve this CLI path. Focused CLI dispatch passed, Black/Ruff passed, and strict MkDocs passed. This is host behavior only; after the unexplained reboot, no new phone check or package installation was attempted.

### 2026-10-05 — reusable nghttp2 WebSocket runtime and live SDK workspace

- Agent 1.1.0 now carries its authenticated native length/MessagePack stream in
  binary RFC 6455 WebSockets over nghttp2 RFC 8441 extended CONNECT. The public
  emulator listens; the private profile connects after keyed UDP discovery.
  Native client/server codecs, TCP streams/listeners and Python transport
  classes are reusable. A11's parser body and framing helpers are copied
  verbatim, with recorded minimal WriteFrame boundary adaptations and licenses.
  The nghttp2 1.70.0 core has pinned upstream provenance and checked input hashes.
- The SDK exports Symbian::WebSocket on both ARM targets. Missing calloc and
  RSocket SetOpt imports were found at application linking and added using
  original ordinals. Steady-clock, endian and OpenC header adapters permit
  nghttp2 without a new scheduler. Default bounds are 4,100-byte messages,
  64 KiB queues, 16 received messages and 2 KiB/16 headers. This requires the
  updated host and agent together; HTTP/1 Upgrade, browsers and TLS are outside
  the implemented transport contract.
- Nine native tests passed; the final focused host selection passed 25 tests
  with one opt-in skip, including independent hyper-h2 interoperability in
  both directions. Public RM-807/Dynarmic guest tests passed 4/4. Private keyed
  discovery/authentication/status passed after moving its callback from the
  worker OS stack to the same bounded A11 fiber stack as public sessions; the
  original stack fault/reset is retained in the research log. No new package
  was installed or request sent to a physical phone. ARMv5T archive/ELF builds
  are not ARMv5T firmware or phone execution evidence.
- The owner's exact ARMv6 CLion configure command succeeds. Root guest profiles
  build and link current runtime, API, WebSocket, TLS wrapper and concurrency
  sources and prefer their live headers, so simultaneous SDK/application edits
  need no export. Pinned dependency archives/platform inputs still come from
  the active SDK; generated standalone applications use its exported snapshot.
  Both ARM profiles linked the agent from local archives. Fresh compiler-cache
  configuration was needed for the old ARMv5T build directory. Actual editor
  navigation and debugger observations remain separate gates.
- The host codec-only benchmark measured 1.77414 microseconds per 4 KiB round
  trip (4,403.55 MiB/s aggregate); it measures neither sockets nor guest/phone
  performance and makes no direct comparison with A11. The inherited console
  catalog/presentation test mismatch and physical lifecycle questions remain
  unresolved. See the research log for retained failures and provenance.
- Final SDK `.symbian/websocket-workspace-sdk-20261005` is installed and active;
  all recorded digests match. Both IDE caches now select its compiler. The
  exact ARMv6 configure command passes, and current API/runtime headers precede
  exported copies in the agent's ARM compilation command. A root configure
  against the older SDK without WebSocket also passes using the workspace's
  current target definitions. Final exported-SDK guest reruns passed all four
  public cases and the private case; both ARM workspace links, native tests,
  Black/Ruff, whitespace and strict documentation checks passed.

## 2026-10-05 — Standalone emulator and signing console

- `symbian emu run` and Console → Emulator → Run emulator open the selected
  verified firmware in fresh owned state without application compilation or
  injection. The real 808 fixture frontend/control launch passed; teardown
  reaped the child after escalation, removed its endpoint and verified all
  baseline hashes. Full OS boot and normal human window-close are unverified.
- Console → Signing lists certificate subjects/fingerprints/expiry, creates or
  imports private named RSA identities, archives identities while preserving
  keys, and signs existing SIS files through the native signer. Private key
  bytes stay out of results; existing identities/output files are protected.
  CLI parity is provided by `symbian signing list/create/import/archive/sign`.
  Phone trust and physical installation are not established by signing.
- Related regression run: 112 passed, 27 skipped; the previously recorded
  agent CLI/console presentation mismatch is the sole failure. Final standalone
  and web frontend follow-up: 22 passed. Changed Python formatting/lint,
  JavaScript syntax and strict documentation checks pass. Replay and the exact
  live firmware identity/session path are in the research log.

## 2026-10-05 — Context application staging for standalone launch

- Standalone `emu run` now builds and stages the CLI working-directory app or
  explicitly selected application. GUI Run emulator uses its resolved app
  context. It still omits `--run`; users launch the staged app themselves.
  With no application context, the earlier empty standalone behavior remains.
- App executables and SDK-compiled menu/icon/locale resources are staged in
  disposable C drives, with optional CA bundles. The real RM-807 frontend
  discovered Symbian GUI Counter (`0xE0000811`) in AppArc without automatic
  launch; all preserved baseline digests remained unchanged. App validation
  gates remain enforced. This is menu discovery, not new guest/phone execution.
- Focused tests: 70 passed, 9 skipped. Black/Ruff and strict docs pass. The
  upstream `--install` success-as-failure experiment and the successful direct
  resource staging evidence are retained and recorded in the research log.

## 2026-10-05 — Built-in application compatibility investigation

- Fresh RM-807 cases disprove the context-only launch limitation: Calculator
  renders and a real pointer tap changes 0 to 3; Settings renders its initial
  list. Both GUI app-list and UID activation use the same AppArc launch path.
- Both Gallery UIDs reproduce an entirely black screen. Gallery stays alive;
  its media Harvester exits with KErrGeneral (-2). Dynarmic and Dyncom agree,
  removing context injection does not help, and a 30-second Dynarmic wait
  remains black. Clock also stays black; its cause is unresolved.
- Gallery encounters missing executive calls (timer inactivity, process open
  by ID and process rename) and unsupported loader/file-server plugin requests.
  Original MDS source confirms those plugin dependencies. These are confirmed
  compatibility gaps; the single causal blocker for Gallery's first frame and
  Harvester's -2 exit have not been isolated. Missing MDS shutdown properties
  must not be misreported as proven readiness gates. No emulator workaround
  was implemented or claimed as a fix.
- Private scripts, native exit records, logs and actual texture captures are in
  `.symbian/app-launch-investigation-20261005/`; exact case paths and source
  provenance are in the research log. Every case preserved all 13,438 baseline
  hashes. These results establish bounded firmware execution, not full OS boot,
  all application subviews, normal human close or physical-device behavior.
- Retained texture/baseline checks, strict documentation build and whitespace
  validation pass. No production code changed in this investigation.

## 2026-10-05 — Native HTTP and shared secure transport

- Native HTTP client/server now support HTTP1.1 and single-exchange HTTP2,
  bounded pull bodies and Write/Finish response/request writers. Common byte
  transport and nghttp2 DATA streams also back the existing RFC8441 WebSockets.
  The exported SDK provides Symbian::Http and native IPv4 DNS.
- Verified Mbed TLS client/server streams support TLS1.2 and TLS1.3, hostname
  and trust validation, SNI and ALPN. Eleven exported-SDK RM-807/ARMv6/Dynarmic
  cases pass: actual example.com/Cloudflare clients, two wrong-host controls,
  HTTP1.1/h2c native servers and TLS1.2/HTTP1.1 + TLS1.3/h2 mTLS servers.
  Golden firmware hashes are preserved; documented emulator changes affect
  disposable copies only. ARMv5T builds do not prove loader/phone execution.
- Native CTest 14/14; host regressions 101 passed, 17 skipped. Complete page,
  header, WebSocket, HTTP server and TLS examples compile for ARM; strict docs
  and formatting checks pass. The prior CLI presentation mismatch is fixed.
- One exchange per connection, no pooling/redirect/decompression/multiplexing
  or HTTP1 WebSocket Upgrade. Details/replay/failures are in the research log
  and HTTP, WebSocket and TLS guides. Explicit Status discards use IgnoreError;
  bool-returning local concurrency APIs retain their appropriate handling.

## 2026-10-05 — Bounded EKA1 process support

- The first EKA1 slice is implemented: native legacy E32 publication and
  inspection, e32-eka1/ARMv5T CMake build profile, complete no-UI example and
  toolchain verify-eka1 CLI. Nokia 7610 RH-51/epoc80 runs and exits normally
  with result7610 on both Dynarmic and Dyncom; independent changed-result7611
  images run on both, and mismatched exit oracles reject them. All preserved
  fixture hashes remain unchanged.
- Existing EKA2L1 EKA1 heap/exit bootstrap is required; no emulator shim or new
  compiler/runtime dependency was added. Independent parser acceptance and
  UID/truncation controls pass. This establishes named-firmware emulator
  process execution, not general EKA1 ABI or physical-device acceptance.
- EKA1 acceptance11/11; native CTest14/14; E32 suite39/39. Existing host
  regression selection91 passed,
  15 skipped. Real CLI verification, strict docs and formatting checks pass.
  Artifacts/failures/replay are recorded in the research log.
- See [EKA1.md](EKA1.md) for the exact fixture/frontend identities and caveats.
  Imports, writable state/fixups, C++ runtime, HTTP/TLS, GUI generation/normal
  Console application Run/Debug, legacy SIS and P900 execution remain gated.
  The SISX builder explicitly rejects EKA1 rather than implying old-format
  installer compatibility.

## 2026-10-05 — EKA1 original EUSER imports

- Native EKA1 conversion and inspection now support a bounded function-only
  EUSER PE import table, sharing validated ELF/proxy/call primitives with EKA2.
  The complete ARMv5T example uses original allocation, length/count, copy and
  free calls; both CPU backends verify copied bytes and balanced heap cleanup.
- Normal7610, changed7611 and corrupted-copy41 results pass; checking controls
  against7610 fails. Ten import guest runs preserve every golden firmware hash.
  Combined EKA1 acceptance19/19; independent EKA2L1 legacy/PE parser tests2/2;
  native CTest14/14, E32 suite40 tests; host regression70 passed,2 skipped.
- This establishes five explicit pointer/integer legacy ABI calls against the
  named 7610 fixture. General C++ ABI, writable state/fixups/lifecycle, UI,
  HTTP/TLS, legacy SIS, P900 and physical-device behavior remain unverified.
  [EKA1.md](EKA1.md) and the user guide describe replay, provenance and caveats;
  the research log retains the initial Mem::Copy return-oracle failure.

### Linux x86_64 host and emulator build checkpoint — 2026-10-05

Ubuntu 24.04.4 at the requested `~/dev/symbian-platform` checkout: fresh host
CTest **13/13**, rebuilt-module Pytest **294 passed, 423 skipped**. The pinned
patched Qt 6 emulator frontend and all configured native oracle executables
build with Clang 20.1.8/Ninja 1.13.2. Original LLVM guest runtime requires
Clang 23+; the exported SDK built **121 target archives** before host resource
compiler installation, which now succeeds with the Linux limits-header fix.
SDK relocation, tool dependency closure, oracle-enabled integration and Linux
GUI/guest-debugger execution are still separate acceptance gates. See the
research log for exact private logs and failed controls.

The enabled Linux oracle/public-source/installed-host-archive suite subsequently
passed **310 tests, 407 skipped**. Both ARMv5T and ARMv6 root indexing targets
build; all **5,420 SDK digests** match. E32/CPU/import/pointer independent checks
now cover EHABI descriptor fixups, and the GUI mismatch control changes its
actual drawing source. Linux frontend `--help` passed under Xvfb using a fresh
private root. These checks retain separate firmware, interactive-GUI and device
gates; the private LLVM binary still needs ICU 70 outside its prefix.

## Public documentation, 2026-10-05

Public MkDocs pages describe usage, ownership and actual restrictions without
`.dev` links, development evidence matrices or detailed roadmap narration.
README covers native HTTP/WebSockets/TLS, concurrency, initial EKA1, Linux and
planned release payloads without presenting unfinished distribution work as
available. Complete networking examples remain. The standalone emulator source
recipe's 22 root patches replayed in order against clean pinned files. Local
links, strict MkDocs, both Doxygen indices and Markdown section links passed;
original-header warnings are unchanged. This documentation validation does not
add loader, emulator or device execution coverage.

## Reusable host SDK, 2026-10-05

The standalone static host closure installs and relocates on macOS arm64 and
Linux x86_64; consumer checks also exercise native CLI validation and input
preservation. Both host native suites passed 13/13. Binding-only CPython 3.12
wheels built from the installed archive pass fresh outside-checkout audits on
both hosts. The macOS OpenSSL-header omission was caught and fixed. This enables
reusing the core per host architecture, but does not establish the other host/
Python combinations or complete guest/no-Python release payloads. Fresh macOS
core minOS is 14.4; release floor/tag policy still needs resolution.

## Hands-on documentation, 2026-10-05

Capabilities and API-family pages now include short application helpers with
explicit failures and ownership. Fourteen examples and the C++20 layout helper
pass ARMv5T/ARMv6 syntax checks against guest headers; this adds no loader or
execution coverage. Ordinary commands use the activated/installed `symbian`
entry point, Tutorials is removed, and the runtime recipe remains in a guide.
Public Markdown formatting checks pass on macOS clang-format 23 and Linux
clang-format 18; documentation builds enforce braces and other root style rules.

Original-header Doxygen class briefs now describe functionality before API
metadata; detailed descriptions put publication/capability information last.
All eleven snapshots retain unchanged declarations and source-browser content.
Strict documentation builds and rendered class-list/detail-order checks passed.
The separate generated symbol-description coverage remains unfinished.

## Linux GUI and debugger, 2026-10-05

SDK-backed GUI/generated-project checks pass in the Linux reruns (35 initial
passes plus four repaired negative/debug cases). Original normal pixel controls
remain, with capture-scale normalization. The live GDB check now reaches both
source breakpoints after correcting the emulator's Thumb-2 breakpoint-kind
handling. Whole-suite and other host architecture release gates remain pending.

### 2026-10-05 — release matrix debugging

The static-archive merger repair reached native tests in the four-host CI matrix.
Linux arm64 exposed an idle worker-pool deadlock in old libstdc++'s condition-
variable clock fallback. A minimal Linux old-fallback reproducer timed out with
an infinite steady deadline and passed with A11's 50ms park cap. The repaired
host fiber test passes locally on macOS and Ubuntu; CI wheels are still pending.

The release workflow now builds the sdist and source archive once, reuses the
four-host core/wheel workflow, validates all 16 CPython/host combinations and
six host/source archives, then publishes tagged versions using PyPI OIDC in
`release.yml` / environment `release`. Manual dispatch builds and audits without
publishing. Five artifact-gate regressions and actionlint pass. Static dependency
prefixes are cached outside the manylinux container via its `/host` mount.
These archives are explicitly host components; full installed guest/tool SDK
payloads remain unfinished and are not represented by the host archive names.

### 2026-10-05 — complete enabled Linux suite after source sync

The Linux checkout at ~/dev/symbian-platform now follows eff8e9f; previous
uncommitted validation edits remain in a reversible Git stash. The full suite
with source GUI headers, the prepared SDK, historical oracle build root and
Xvfb passes: 330 passed, 387 optional skips, one upstream Starlette warning
(170.72s). The preceding run's missing display and doubled platform-tests path
were harness setup mistakes, not SDK failures. Two dynamic GUI-test references
to relocated probes were corrected. Root guest ARMv5T and ARMv6 presets both
configure/build with explicit SYMBIAN_SDK_MANIFEST; the toolchain now follows
that override before the user activation record. Native Linux CTest passes
13/13 (0.42s). Logs are /tmp/symbian-linux-full-sdk-tests-r6.log,
/tmp/symbian-linux-host-tests-r6.log and symbian-linux-probes-*-r6.log on Linux.

Release host caching now keys the tested installed core on native sources,
CMake/build scripts, dependency inputs and VERSION, while Python-only/docs
changes reuse it. A cache hit still builds/runs the relocated standalone
consumer before archiving and rebuilding interpreter-specific bindings.
The cache-hit script branch and archive creation passed on macOS; Bash syntax
and actionlint pass. The active dry run has all eight Linux and four macOS
arm64 wheels/audits complete, with Intel still running.

Release validation can now reuse completed host-sdk.yml artifacts for the exact
release commit. Reuse checks the source SHA, successful conclusion and workflow
path before downloading and re-uploading each architecture's already-audited
wheels/core archive. The release matrix/content gate still runs on the assembled
artifacts. This avoids rebuilding interpreter bindings when a tested main
commit is tagged; a cache miss retains the full four-host build path. Workflow
lint and the empty-candidate discovery query pass; hosted reuse acceptance is
pending the first completed matching host run. Host jobs have a 60-minute cap.

### 2026-10-05 — all host wheel combinations built

Dry release 37302095376 succeeded: 16 audited wheels, four standalone host
archives, the source archive and sdist. Host run 37304344750 succeeded for the
final artifact-reuse workflow and saved all four tested host-core caches.
An isolated pip-installed CI wheel exposed/fixed the installer's checkout-only
header-probe path and missing explicit workspace forwarding for resource tools.
The patched installed-wheel experiment compiles rcomp/uidcrc, builds hello_time
ARMv6 and creates/signs its SIS outside the checkout. PyPI publication, complete
native distributions and published clean-environment guide replay remain open.
PyPI attestations are disabled: OIDC publishing does not require additional
provenance documents. Expired/missing host artifacts now trigger a fresh build
rather than failing the release's artifact reuse path.

### 2026-10-05 — v0.1.0 actually published

Final host run 37306013453 succeeds on all four native runners, with 16 installed
wheel audits. Each architecture restored the tested c8c266b4 host-core cache;
Linux x86_64 logs confirm reuse plus the relocated consumer audit before all
four CPython wheels. Tag v0.1.0 points to f84519c. Release run 37306898498 succeeds:
all four host jobs reused exact-commit artifacts, source/sdist jobs and the full
matrix audit passed, and PyPI trusted OIDC publication and GitHub release both
completed. Public PyPI JSON has version 0.1.0, 16 wheels and one sdist. GitHub
has 22 attached assets: 16 wheels, four host SDK archives, source archive, sdist.
No optional provenance attestations were produced.

Fresh public-PyPI virtual environments outside the checkout pass pip install
and symbian doctor on macOS arm64 and Linux x86_64. The unchanged published
installer builds SDK resource tools using an explicit prepared source workspace.
With that source SDK, published CLI hello_time init/build, package and sign pass,
as does the real gui_app build. Public wheel smoke logs use the
/tmp/symbian-pypi-* prefix on macOS and /tmp/symbian-linux-pypi-* on Linux.
These checks reuse prepared source/toolchain inputs; they are not clean-machine
native SDK installation or emulator/device guide acceptance. Full native SDK
payload closure and full EKA1/runtime/library support remain open. The published
symbian-host archives are honest host components, not the requested complete
no-Python host-plus-native distributions. README now links actual publications
and describes that distinction.

### Native distribution assembly — 2026-10-05

The Python-free assembler now copies LLVM/build/resource tools and their dynamic
closure beside the guest headers/libraries and the standalone host SDK. A
relocated SDK with no Homebrew/LLVM/library overrides builds and converts
hello_time for both ARM targets on macOS arm64 and Linux x86_64. This verifies
compiler/linker/E32 build acceptance, not physical execution. Relative manifests
and `symbian sdk install --archive` are implemented with archive path-escape
rejection. Shipping all four host archives and full EKA1 API/runtime remains
outstanding; these prototypes must not be described as published distributions.

The fresh-wheel native archive path now passes hello_time creation/build/SIS
packaging/signing and a separately copied gui_app build/package on macOS.
Archive metadata, SDK wrapper assumptions, CMake scratch-build tool selection,
absolute GUI SDK paths and rcomp helper spawning were corrected. Release CI
will exercise this path on all four hosts before publication. These local
checks do not establish full EKA1, physical installation or firmware execution.

### Restricted EKA1 profile on Linux — 2026-10-05

The named Nokia 7610 EKA1 process and original-EUSER heap/copy profile now pass
on Linux x86_64 with both CPU backends: 19 tests, including changed-result and
corrupt-copy controls, in /tmp/symbian-linux-eka1-acceptance-r2.log. The private
fixture was transferred through the portable firmware format and imported with
its unchanged content identity. Broad EKA1 SDK/runtime/GUI/networking and legacy
packaging remain unsupported. Public guides include headless Linux execution.

### 0.1.1 native distributions published and public guides replayed — 2026-10-05

Audited manual release 37322572074 and tag release 37326077196 succeed at
25efb2b. PyPI has sixteen CPython 3.11–3.14 wheels for macOS/Linux x86_64/arm64
and one sdist. GitHub v0.1.1 has twenty-six assets, including all four relocatable
no-Python native SDK archives plus host archives and source. Shared ARMv5T/ARMv6
EKA2 payload compilation is reused across hosts. A tagged replay found/fixed the
30-second nested init build timeout before publication; an intentional
31-second delayed local initial build and corrected Intel acceptance pass.

Clean public-PyPI virtual environments and public native-archive downloads pass
Getting started and hello_time/gui_app build/package/sign/default IDE setup on
macOS arm64 and Linux x86_64. Actual installed-CLI emulator runs then pass
rendering, input, timer/Clear or counter/reset and normal exit, with preserved
RM-807 firmware unchanged; Linux uses Xvfb. Logs use the
/tmp/symbian-published-native-{macos,linux}-{guide,run}.log names on their hosts.
The Linux EKA1 profile also passes nineteen Pytests and two independent native
format/import GTests. Physical staging/installation remains unverified: USB
list is empty. Native archives provide the implemented EKA2 SDK, not full EKA1
or replacements for firmware OS DLL implementations. Complete original-symbol
documentation and broader OS/runtime coverage remain outstanding.

### Native startup export follow-up (2026-10-05)

Native assembly now preserves share/symbian/runtime, with an explicit release
completeness gate. SDK-owned runtime sources are accepted by the CMake input
helper. The relocated no-Python GUI and shared-startup E32 build checks pass
for both ARM targets on macOS arm64 and Linux x86_64; seven release-audit and
six E32 regressions pass. Guest/archive cache inputs now include probe and
Abseil patch inputs and exclude wheel-only CLI/acceptance changes. Actions
replay and a future release are still required to ship this change beyond the
already published 0.1.1 assets. No broader EKA1 or firmware-library support is
implied.

### Original-header documentation follow-up (2026-10-05)

Offline source-linked Doxygen supplements now add 3,200 briefs and 3,502
parameter names to the eleven-header index. Generated labels render in final
paragraphs outside individual parameter descriptions. The generator checks
const overloads, argument defaults/renames, fields/enums and ambiguous matches;
six regressions pass. Strict MkDocs and both Doxygen indexes build, with missing
parameter contracts and original-header warnings still visible. 826 semantic
descriptions remain unresolved; complete original-symbol coverage is still open.
The four-host/16-wheel build for 97a7f15 succeeds (37331396968); the new native
startup/archive workflow is running as 37333156879 and remains an open gate.

2026-10-05 original networking documentation: 48 additional original-source
contracts now supplement the hosted library import, preserving exact overloads,
EPL notices and final generated-source labels. Eight importer tests, Ruff,
strict MkDocs/both Doxygen builds and TInetAddr rendered-XML suffix/output-buffer
checks pass. Full original-symbol coverage remains incomplete.

### Independently installed emulator bundles — 2026-10-05

Versioned installation/rollback and protocol/capability discovery are committed
in cd37b95; 22 policy/CLI/owned-session tests pass with ten opt-in skips. SDK
release rehearsal 37336662479 passed all four host/native matrices and the
16-wheel/10-archive audit. The remaining inner CMake build timeout from the
Intel screenshot is fixed in bd28b56; exact-source replay 37340236532 is active.

Local source-SDL bundles pass relocation through paths with spaces, clean-home
Qt platform startup, native importer startup, resource deployment and isolated
data roots on macOS arm64 and Linux x86_64. The macOS app passes deep strict ad
hoc signature verification and has relative non-system dependency paths.
Both backends pass rendered counter/input/reset/timer/normal exit checks on
both hosts with the preserved RM-807 fixture. The capture oracle now accounts
for independently rounded physical dimensions and normalizes timer pixels.
Logs are /tmp/symbian-emulator-{archive-macos,archive-linux,delivered-gui}.log
on their respective hosts. Local Qt versions differ from pinned release Qt;
these are acceptance prototypes, not published release candidates.

Four-host delivered bundle/source CI, independent release auditing, exact-source
artifact reuse and corresponding runtime-source collection are implemented.
Initial CI compiled and tested macOS arm64/Linux x86_64; arm64 Linux acquisition
hit GitHub HTTP 504 and Intel macOS is still building. Pinned shallow Git fetches
now have bounded retries. Full four-host archive/source acceptance and actual
publication remain gates. The requested example is guest Symbian Qt, requiring
separately supplied Qt4 headers/imports/runtime; host Qt6 is not that runtime.
