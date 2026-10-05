# Research log

2026-10-04: The connected 808's MTP `Installs` listing included handle
`0x010000af`, but `GetObjectInfo` returned `0x2002` for it. The handset then
briefly vanished from USB discovery and reappeared. A later upload and
digest-checked readback passed, and a repeat reused the verified object; no
unreadable entry was encountered on those successful calls. The host now
skips metadata-rejected existing children because the package filename is
content-addressed and upload is followed by exact readback. Open question:
whether the rejected handle was stale after phone-side file removal, and
whether this MTP implementation can retain a persistent inaccessible name
collision. Do not infer installer or agent execution state from staging.
The macOS background EKA2L1 window required accessory activation policy in
addition to all-Spaces, full-screen-auxiliary and floating behavior to stay
visible over a full-screen terminal without stealing focus. Its effect on
other macOS Space/window configurations remains unmeasured; Linux focus
behavior remains provisional.

2026-10-04: Public project declarations used `e32-*-experiment` as the only
accepted E32 kinds, so generated application manifests inherited a temporary
label. New short kinds produce short report schemas and artifact kinds; old
declarations remain accepted and keep their serialized identifiers. Open
question: decide whether a versioned migration should retire the old aliases
after external consumers have moved. The expanded legacy-build test run also
exposed an ARM unwind descriptor precondition in the native converter on old
fixtures; it is independent of the naming path and remains to be diagnosed.

2026-10-04: The console can enumerate USB candidates and safely stage a
digest-checked SIS on writable phone storage, but it has no authenticated
device-agent discovery or installation query. The new overview therefore
shows `installation unknown` even after staging. Open question: which paired
transport and guest identity mechanism will let the console associate a live
agent status with a USB device without conflating physical and emulator
profiles? A local UI attempt reached Window Server group construction, then
raised `KERN-EXEC 3` at ROM `ws32.dll` address `0x80a50fbc` in the pinned
Dynarmic emulator. Isolating screen and graphics-context creation passed the
existing agent service test; including group construction failed. The UI
change was removed from the package pending a verified implementation. No
physical-device status is inferred from the package build.

2026-10-04: The first bounded ring had sequence/code only, so events from one
live process lacked timing and severity context. A steady clock already used
for the service's control deadline now stamps append time as microseconds
since ring construction; the ring clamps each timestamp against its previous
one. Event code 2 is debug, code 3 warning, and authentication/close are
information. Host models accept absent fields from earlier peers and preserve
unknown numeric codes. Both emulator CPU backends read ordered stamps; the
Dynarmic replay verified warning severity after a rejected frame. Open
questions: future OS/application log sources need their own source identity,
permission and privacy policy; a process restart must be represented when
resuming a cursor; physical monotonic-clock quality and idle cost remain
unmeasured. These readings are neither UTC nor cross-process timestamps.

2026-10-04: A second CLion configure failure after adding the connectivity
indexing target was a different cause: the ignored user preset explicitly
overrode `SYMBIAN_SDK_PREFIX` with an older exported SDK, while the active
SDK manifest pointed at the new resident-agent export. A `-D` override made
the shared preset pass, but CLion's own named preset still selected the old
path. Refreshing the machine-local preset and replaying the exact CLion
command resolved it. Open question: add a supported generator for local IDE
presets so SDK updates do not leave absolute paths behind; do not silently
override an explicitly selected SDK in project CMake.

2026-10-04: Root `SYMBIAN_INDEX_GUEST_PROBES` intentionally rejects any
`examples/*_probe` directory without an explicit ARM target. The added
`connectivity_probe` directory had a standalone build but no root IDE target,
so configuration stopped with the reported CMake error. Adding an object
indexing target using the installed Connectivity/Stackless interfaces makes
both ARM preset compile databases complete. An existing ARMv6 build cache
still referenced an older SDK path; passing the active
`-DSYMBIAN_SDK_PREFIX=...` selected the current SDK. This target compiles
source for indexing; it does not link the probe variants into one executable.

2026-10-04: The resident service had only been started by a dedicated Pytest
harness. An attempt through the normal `symbian app run` path failed at CMake
because the example preset's optional `SYMBIAN_SDK_PREFIX` environment value
was empty, despite the launcher selecting an installed SDK compiler. The
example now derives the prefix from that compiler in this case. The normal
run then served separate authenticated CLI hello/status processes. Ctrl-C
reaped the owned emulator, but only after SIGTERM exceeded the two-second
grace and the launcher sent SIGKILL (`frontend_exit=-9`). Open question:
provide an in-guest local stop/disable path and measure shutdown cleanup;
reaping a disposable emulator is not evidence that the service stops cleanly
on a phone. No startup registration or persistent listener was added.

2026-10-04: The previous resident service accepted status or logs as the
first authenticated frame, leaving no explicit boundary for version and
limit agreement. A small hello result now gives version 1, the 4 KiB control
ceiling, 16 requests per connection and `status`/`logs` availability. The
worker requires hello first and rejects a repeated hello. The host checks
that profile before returning a session and counts hello against the request
cap. Dynarmic and Dyncom tests rejected a valid status frame sent before
hello, then completed normal and reconnect flows. Open questions: negotiate
stream counts/compression only when a credited stream exists; define stable
device identity and handset-visible pairing before any non-loopback listener;
measure memory and idle cost on a development phone. A successful emulator
hello is no evidence of those policies or of Nokia 808 compatibility.

2026-10-04: The first logs slice retains four service-local event codes after
TLS authentication: authenticated session, status read, rejected frame and
session closed. The ring is fixed at 32 records and lives on the existing
single worker; its sequence cursor reports an overwrite gap rather than
silently presenting a complete history. Native host tests covered cursor
rollover and malformed request bodies. An authenticated Dynarmic emulator
session filled the ring and received `gap=true`, while both CPU backends read
events after reconnect. The host model preserves an unknown numeric event
code in a synthetic mutual-TLS response. Open questions: add a local IPC
producer for SDK-owned applications, decide retention/privacy policy and
measure phone memory; any live-follow stream needs credits, cancellation and
gap events. This ring does not collect Symbian OS logs or justify broader
read permissions.

2026-10-04: Linking `Symbian::Display` into the resident agent added an eighth
original DLL dependency (`hal.dso`). The first app ELF had eight DT_NEEDED
entries but its project manifest listed seven import proxies, so E32
conversion correctly rejected the incomplete metadata. Adding the HAL proxy
let the normal installed-SDK consumer build. The native HAL display query and
system tick query both returned usable values under Dynarmic and Dyncom; the
agent omits either nested snapshot if its API fails on another profile.
Primary HAL size is not a Window Server layout claim. The tick wraps at 32
bits and is not a UTC clock. Physical-device availability remains unknown.

2026-10-04: Per-call `ReadFor(..., 5s)` allowed a peer to refresh the wait by
delivering another byte just before each deadline. The resident read-only
control loop now measures one monotonic five-second budget across a complete
request and response; the host uses one caller-selected budget for its status
exchange. An authenticated host sent three prefix bytes two seconds apart.
The guest closed before the final byte on both emulator CPU backends and then
accepted a new authenticated status session. The host drip-response test also
expired at its aggregate deadline. A future protocol needs per-request
cancellation and terminal outcomes for operations longer than status; this
test covers only bounded read-only control frames.

2026-10-04: The resident worker TLS timeout was a guest secondary-thread
stack overflow, observed as an access violation immediately below its stack
pointer and `KERN-EXEC 3` in the disposable emulator log. A standalone TLS
owner on the main thread had passed the same handshake. The existing
A11-derived `WorkerExecutor::PostFiber` now accepts an explicit 4 KiB–1 MiB
stack size; the service requests 256 KiB for its TLS job. The active listener
plus worker completed status, repeated requests, oversized-frame rejection and
reconnection under both Dynarmic and Dyncom. The first cleanup trial reduced
queue admission to one job and reset a rapid reconnect while the prior TLS
job was still draining; keeping four bounded outstanding jobs restored the
tested behavior. This is an emulator stack/admission result, not a measured
phone memory budget. The attempted `RSocket::Transfer` investigation was
discarded; the service uses the pre-open shareable Socket Server session that
had already passed bidirectional worker I/O.

2026-10-04: The vendored Mbed TLS 3.4.1 server returns
`MBEDTLS_ERR_SSL_FEATURE_UNAVAILABLE` (`-0x7080`) for a hybrid TLS 1.2/1.3
server configuration. The raw probe pins one version and succeeds. The public
`TlsServer::Create` now requires `TlsVersion::kTls12` or `kTls13`. A clean-SDK
24-case emulator matrix passed both owner versions, certificate rejection,
status framing and oversized-prefix rejection. The initial resident TLS 1.3
session reached a host-verified handshake but did not answer status. The
secondary-thread stack overflow described above explains that result.

2026-10-04: A Socket Server session marked shareable after the active listener
socket opened did not complete worker I/O in the emulator, although a worker
job did execute. Calling `RSocketServ::ShareAuto()` after Connect and before
socket Open produced two bidirectional worker connections on both CPU
backends. The public opt-in must therefore precede ListenIpv4. Cross-thread
behavior on a physical Nokia 808 is unknown. The service still needs a
handset-visible local status/disable UI, persistent per-device identity,
pairing, crash-loop backoff, idle/no-network state, signed packaging and a
verified on-device startup policy before its stated migration gate can pass.


### 2026-10-04 — active-object accept and event-thread I/O

The original `CActiveScheduler` and `CActive` EUSER ordinals are now selected
for an opaque `ActiveTcpListener`. Its native owner issues one asynchronous
`RSocket::Accept`, delivers the completed client to an observer, and drains
cancelled requests before closing. An ordinary SDK consumer completed two
accept/reconnect cycles and pending-accept cancellation under both RM-807
Dynarmic and Dyncom emulator backends. The event thread has no accept poll
timer while waiting for a client.

An initial probe rearmed accept before doing timed `TcpClient` I/O inside
`RunL()`. Dynarmic passed, but Dyncom panicked `E32USER-CBase 46` (a stray
scheduler signal). Moving rearm after the I/O alone did not clear it; removing
the timer-backed synchronous calls from that active callback did. The native
helper and documentation now say the accepted stream belongs on a worker for
substantial I/O. The probe's untimed one-byte callback is a narrow scheduler
contract test, not a model for a TLS server. Open question: establish safe
cross-thread socket ownership or implement nonblocking TLS I/O on this active
scheduler without nested synchronous waits. The emulator outcome does not
settle Nokia 808 behavior.

### 2026-10-04 — host control integration

The Python session delegates MessagePack and frame length rules to the native
agent library. It provides a manually addressed, mutually authenticated TLS
socket with a project-selected CA and explicit server name. A local server
requiring a client certificate accepted a status request and returned a typed
result; the native prefix check rejected an advertised payload above 4 KiB.
This proves the host API loopback path, not the phone pairing story.

Open questions: choose an on-phone key generation/storage path and separate
identity lifecycle before this can become a general device session. Add
deadline/cancellation integration, reconnect, discovery and an active-object
guest listener. Measure the actual phone transport and idle cost. No current
emulator or host result proves Nokia 808 compatibility.

### 2026-10-04 — guest read-only MessagePack control

The host control codec and small guest decoder now share version-one
hello/status/result envelopes. The guest caps its control payload at 4 KiB,
checks the frame prefix before payload allocation and retains unknown
top-level fields for the result. The TLS 1.2 and TLS 1.3 emulator cases authenticated the
client certificate, then returned a framed result with the same request ID
and unknown extension. A second case sent only a 4,097-byte prefix and the
guest rejected it before reading or allocating the payload. The clean SDK's
`Symbian::Agent` archive supplied the guest codec. The full opt-in TLS matrix
passed 16/16; the host GTest also passed round-trip and malformed/oversized
controls.

Open questions: the one-shot DLL still lacks an active-object idle listener,
separate provisioned peer identities and a handset-visible pairing flow.
The protocol needs explicit cancellation, deadline interpretation and grants
for every operation beyond read-only status. The guest decoder deliberately
handles only an empty body and a small MessagePack subset; future operations
need typed body validation before they are enabled. Neither emulator result
nor ARM archive generation demonstrates Nokia 808 compatibility.

### 2026-10-04 — inbound authenticated TLS probe

An opt-in DLL now accepts one native RSocket stream and runs the Mbed TLS
server path with `MBEDTLS_SSL_VERIFY_REQUIRED`. A local Python/OpenSSL client
verified the guest's fixture certificate, presented the same fixture as a
client certificate and exchanged one application byte in TLS 1.2 and 1.3.
When it omitted the client certificate, the guest observed handshake failure
and the host connection reset. All twelve outbound/inbound protocol and
certificate controls passed on the disposable emulator.

This fixture uses one self-signed certificate and private key on both peers
to isolate transport and verification mechanics. It does not establish
distinct host/device identities, secure key generation/storage, handset-visible
pairing, pin rotation or session authorization. The service should provision
separate identities under a visible pairing action and reject a repeated
fixture in any release configuration. The current server is a one-connection
research DLL; it does not meet the resident service or idle behavior gate.

### 2026-10-04 — connected request deadlines

Native `RSocket::Send` and `RecvOneOrMore` requests can share the same
`RTimer` wait/cancel/drain pattern as accept on one worker thread. After a
50 ms receive timeout and `CancelRecv`, the same accepted socket received a
later byte in the patched RM-807 emulator. This bounds the lifetime of the
caller-owned descriptor buffer. Send timeout is compiled but still needs an
emulator case that can hold the native send pending; a tiny loopback send may
finish before any deadline. A worker deadline does not replace the planned
active-object TLS transport: the session scheduler still needs cancellation
requests, terminal status publication and event-thread ownership without
polling while idle.

### 2026-10-04 — inbound RSocket and accept cancellation

The original ESOCK `RSocket::Bind`, `Listen`, blank `Open`, `Accept` and
`CancelAccept` exports and EUSER `RTimer::After` were absent from the selected
SDK import proxies. Their preserved DEF ordinals have now been selected; no
SDK-wide network or trust default changed. The EKA2L1 internet socket backend
already implements bind/listen/accept/cancel, and a disposable RM-807 guest
received a host loopback connection and completed two native accept deadline
cycles. The accepted socket survived destruction of its listener because both
retain their session until close.

The service still needs an active-object listener that has no polling worker
while idle, cancellation and deadlines for connected reads/writes and the TLS
BIO, bounded session/accounting state, and an authenticated read-only protocol
response. The synchronous helper is a proven transport primitive, not the
resident agent itself. A physical phone may expose different binding and
bearer behavior; measure that separately.

### 2026-10-04 — native guest TLS handshake boundary

The guest TLS DLL first crashed because its translation unit did not use the
Mbed TLS archive's `MBEDTLS_USER_CONFIG_FILE` and guest compile definitions.
Its `mbedtls_ssl_config` layout therefore differed from the archive. Sharing
those definitions removed the crash. Mbed TLS then returned
`MBEDTLS_ERR_PK_BAD_INPUT_DATA` until the probe initialized PSA crypto before
the handshake, matching its host test setup. TLS 1.3 delivered a session
ticket before application data, so a bounded read loop handles both
`WANT_READ` and `RECEIVED_NEW_SESSION_TICKET`. These are library integration
requirements for the planned C++ TLS owner.

Eight opt-in emulator cases passed against a local server: authenticated TLS
1.2 and 1.3 plus wrong-host, untrusted and expired certificate rejection for
each. The certificate fixtures are local test material, never SDK-wide roots.
The experiment uses original RSocket send/receive, the RM-807 random adapter
and local EKA2L1 patches. Open questions remain: how to implement a native
active-object socket owner with in-flight cancellation and deadlines; how to
bound TLS session memory and buffers; whether the target phone supplies
equivalent random quality, UTC, sockets and TLS latency; and how to host an
inbound authenticated listener while idle. No physical-phone claim follows
from this result.

## 2026-10-04: Original RSocket outbound and receive path

A diagnostic DLL compiled against original `ES_SOCK.H` and `in_sock.h` with
ordinal proxies selected from their EABI DEF files. `RSocket::Send` delivered
one byte to a host listener through the disposable RM-807 emulator even though
the guest OpenC `send` returned `EINVAL` and `sendto` returned `ENOSYS`. This
separates the original Socket Server path from the OpenC wrapper problem.

The public `TcpClient` uses an owning native bridge and a 32 KiB limit per
call. Its source header closure and DEF files are physically vendored from
SymbianSource `commsfw` revision
`bc8ac1a6d5273cbfa7852bbb8ce27d6ddc076984` and `networkingsrv`
revision `b283ce17f27f4a95f37cdb38c6ce79d38ae6ebf9`, retaining their
EPL notices. A fresh SDK export built armv5t and armv6 archives and installed
`esock`/`insock` proxies. The first ordinary consumer run hung after send:
EKA2L1 lacked opcode 38, identified as `ESoRecvOneOrMoreNoLength` in the
original `csock/SOCKMES.H` and `CS_CLI.CPP`. A scoped patch adds that dispatch
case to the existing `recv(..., one_or_more=true)` handler. The rebuilt
emulator passed public SDK send/receive tests on Dynarmic and Dyncom and a
wrong-response control. The original platform headers are now included in
the curated native Doxygen reference.

The helper waits synchronously for each native request. It is suitable for
bounded worker-side checks, but not for the resident agent's active-object
listener or cancellable TLS owner. The next transport slice needs a native
listener/accepted socket, read/write request lifetime, cancellation drainage,
deadlines and authenticated TLS. Confirm the source DEF ordinals and network
capability behavior on a physical Nokia 808 separately; the emulator result
does not establish device compatibility.

## 2026-10-04: Connected TCP receive, outbound OpenC blocker

With a supplemental proxy built from SymbianSource `libcu.def`, `sendto`
resolves to original libc export ordinal 304. The diagnostic DLL was linked
again and its timestamp/imports checked before rerun. An explicit-address
`sendto(fd, &byte, 1, 0, &peer, sizeof(peer))` returned `ENOSYS` (OpenC
errno 78); the local listener saw a connection but no byte. The active SDK
proxy lacked `sendto`; future SDK exports now select it. This does not fix
the Belle OpenC runtime or establish outbound delivery. The original
SymbianSource `ES_SOCK.H` is in `oss.FCL.sf.os.commsfw`, and `in_sock.h` in
`oss.FCL.sf.os.networkingsrv`; both research checkouts are ignored. A native
`RSocket::Send` experiment should use their ABI and a verified ESOCK import
proxy before selecting a connectivity implementation.

Follow-up: a tight sequence of guest `WANT_READ` retries posted many libuv
read-start tasks, produced repeated `UV_EALREADY` traces and crashed one
disposable emulator run. The patch now tracks whether its background read is
armed and posts only once until completion/stop. The opt-in receive regression
passed after rebuilding. A proposed explicit-address `sendto` diagnostic
failed at link time (`undefined symbol: sendto`): this SDK libc proxy selects
`send` and `recv` but omits `sendto`. The subsequent emulator runs used the
older DLL, so they cannot say anything about `sendto` delivery. The source of
the observed `send`/`write` `EINVAL` remains open. A verified `sendto` import
or native `RSocket` probe can distinguish the guest wrapper from ESOCK.

The earlier option-acceptance stub made `fcntl` succeed but left `recv`
blocked. `research/eka2l1/belle-nonblocking-tcp.patch` now gives the pinned
EKA2L1 internet TCP socket a background libuv read into its bounded ring,
returns ESOCK `KErrWouldBlock` (`-1000`) when no bytes are ready, and records
EOF/overflow as terminal errors. The opt-in guest probe in
`probes/mbedtls_dll_probe/socket_probe.c` passed on Dynarmic with a local
listener: empty read mapped to Mbed TLS `WANT_READ`, a delayed `R` arrived,
and later reads were cancelled. The test verified pinned ROM/EUSER digests.
The patch uses the existing EKA2L1 512 KiB ring; its memory cost and mode
switch behavior need further measurement. Dyncom, EOF, overflow and repeated
connect/disconnect cases remain untested.

A blocking `send(fd, &byte, 1, 0)` and `write(fd, &byte, 1)` both returned
`EINVAL` in the same connected guest before `fcntl(O_NONBLOCK)`. The host TCP
listener accepted the connection, but the emulator `socket_socket::send`
service handler was not entered. This suggests an OpenC or descriptor bridge
error before the emulator service; that diagnosis is an inference, not a
located cause. A native `RSocket::Send` probe can distinguish an OpenC wrapper
problem from a lower ESOCK path. No authenticated TLS handshake or resident
network service is possible until outbound I/O is verified. The default SDK
archive remains fail-closed for entropy and does not adopt this patch.

## 2026-10-04: RM-807 secure-random executive path

The original `genexec.pl` enum gave `EExecMathSecureRandom = 265`, but this
named Belle EUSER ROM uses an inserted dispatch slot: its `Math::RandomL`
export at ordinal 2503 reaches the ARM SVC `0x10A` veneer. The pinned EUSER
digest is `3cec7e1546f8ed0cf64a73fece9fdd8fe6e4976535ddffd18b7068c19c01357b`.
The EKA2L1 v101 map lacked a handler for that number. A scoped patch
(`research/eka2l1/belle-secure-random.patch`) adds one using libuv's existing
host OS CSPRNG, validates a writable descriptor and 1024-byte ceiling,
returns `KErrNotReady` and clears bytes on a host-random failure. libuv's
source contract guarantees an all-or-error fill; its synchronous call can
block if the host entropy source stalls, so latency remains to measure.

The opt-in DLL adapter in `probes/mbedtls_dll_probe` uses an ARM-state
veneer because Thumb SVC immediates cannot encode `0x10A`. It passes a
`TPtr8` to the original executive ABI, checks the native result, zeroizes on
failure and sets the Mbed TLS produced-byte count only after success. The
default archive still selects `sdk_entropy_fail.c`; no generic SDK behavior
was changed. Two disposable guest tests passed on Dynarmic/Dyncom via the
real EUSER/RLibrary path. The test compares two draws and nonzero bytes but
cannot itself certify cryptographic quality. Physical firmware, EUSER imports,
clock, network and TLS still require separate verification. A useful next
step is a failure-injection test for the emulator handler and bounded latency
measurement before proposing this as a supported SDK entropy path.

## 2026-10-04: Resident wire framing boundary

The frame codec enforces the 64 KiB per-frame budget before payload allocation;
`InboundQueue` enforces four messages and 256 KiB of completed payloads when a
session owns it. A fragmented frame can hold one additional bounded payload
in the decoder, so the complete session memory budget must count both. The
typed control envelope is limited to 4 KiB, but operation bodies remain
untrusted maps. The next service layer must authenticate before dispatch,
validate operation bodies and distinguish
malformed input, cancellation and transport loss. The current host GTest is
not evidence of a guest service or an authenticated emulator session.

The pinned emulator internet socket has asynchronous libuv send/receive, but
the base socket rejects `KSONonBlockingIO` and the selected OpenC `fcntl`
path fails before the TLS BIO attaches. Implementing that option requires an
observable nonblocking read/write contract, not just a success return. This
is still the transport prerequisite for an emulator TLS handshake.

The original `kernel/eka/kernel/execs.txt` passed through its own `genexec.pl`
generator assigns `EExecMathSecureRandom = 265`; the generated user stub
dispatches a one-argument slow executive call. The original kernel handler
returns `KErrNotReady` when its RNG cannot guarantee security. This source
number is a research lead, not yet a verified RM-807/Belle ROM wrapper ABI:
EKA2L1's v10 and v101 dispatch maps have no secure-random handler, and the
SDK's candidate adapter still has unresolved EABI trap imports. No guest
entropy source was enabled on the basis of the generated number alone.

## 2026-10-04: Linux host preparation questions

Final host CI run `37202283905` passed both native and both wheel jobs: 10/10 CTests on Ubuntu 24.04 x86_64 and aarch64, plus installed-wheel audit for CPython 3.11–3.14 on each manylinux_2_28 architecture. This closes the provisional host build/package gate, not the interactive development-machine gate. Remaining real-host checks are source SDK export, pinned EKA2L1 Qt/FFmpeg build and disposable GUI session, guest GDB/CLion relocation, Console visuals on the supported x86_64/Python combinations, a viable aarch64/Python 3.14 Console renderer and bounded USB discovery. None of this establishes a Nokia 808 guest ABI or physical-device result.

Linux CI run `37199385640` reached CMake after building the existing OpenSSL and libusb prefix, then failed on both architectures in native and wheel jobs because `BoostConfig.cmake` was absent. The host concurrency library already requires Boost.Fiber and Context; this is a bootstrap omission, not a new SDK dependency. The isolated prefix now builds hash-checked Boost 1.90.0 with explicit x86_64/aarch64 architecture and ABI selections adapted from A11. The follow-up run must verify the Boost install, CMake discovery, host compile and wheel audit. A future Linux machine check should confirm whether the chosen GCC/toolchain and libstdc++ baseline are suitable for distribution.

The local Docker manylinux_2_28 x86_64 image exposes GNU `ar` but no `llvm-ar`. Its GNU `ar -M` successfully merged a tiny C archive through the same MRI command shape used by `scripts/bundle_static.py`. CMake now selects LLVM `ar` when available and GNU `ar` on Linux otherwise. This only proves the archive command in that image, not the full host bundle or wheel audit.

The first Ubuntu aarch64 build with pinned Boost reached CMake and exposed an incomplete Boost component closure: Boost.Fiber's installed config calls `find_package(boost_filesystem 1.90.0)`. The archive had been compiled incidentally, but `bootstrap.sh --with-libraries` had not selected Filesystem for installation of its CMake config. The bootstrap now uses A11's exact eight-library list; CI must verify that this covers all imported targets.

With the complete closure, Ubuntu x86_64 CMake configure passed and native compilation reached the host concurrency layer. Boost.Context's `fiber_unwind` throws `forced_unwind`, and Boost.Fiber's condition-variable templates contain catch handlers, so GCC rejects these three source files under the project's default `-fno-exceptions`. The CMake fix adds a source-scoped `-fexceptions` only where direct Boost includes instantiate those paths, as the user's explicit exception-boundary policy permits. A Linux compile and CTest are still required to establish the boundary works on GCC.

Ubuntu aarch64 reached the final native test link after the exception fix. GNU ld reported unresolved Abseil Randen symbols originating in `libsymbian_host_primitives.a(util.cc.o)` because the static bundle was ordered after Abseil random libraries. The imported archive target now declares its Abseil link closure instead of leaving those libraries on the outer interface before the bundle. Local generated Ninja commands place the bundle before `libabsl_random_distributions.a`; Linux linkage and runtime tests must still confirm this order under GNU ld.

Run `37200345002` did confirm that link order with native builds and 10/10 CTests on Ubuntu x86_64 and aarch64. In a separate manylinux aarch64 wheel build, libusb's static `core.o` produced `R_AARCH64_ADR_PREL_PG_HI21` against external `stderr` when linked into `_native.so`, proving it needed PIC. The libusb configure step now prepends `-fPIC` to `CFLAGS`, as A11 does for its other static libraries used in shared modules. The installed wheel and audit are still open.

With PIC, the manylinux_2_28 aarch64 CPython 3.11 wheel built and auditwheel repaired it. The install-audit stage failed on the package's automatic `pywebview[pyside6]` dependency: PySide6 could not be resolved in that environment. [PySide6 6.11.2 on PyPI](https://pypi.org/project/PySide6/) targets glibc 2.39+ on aarch64; [6.7.1](https://pypi.org/project/PySide6/6.7.1/) targets 2.31+, still above the 2.28 wheel test baseline. We now restrict automatic pywebview/PySide6 installation to Linux x86_64 and leave aarch64 Console GUI support open for a real-host renderer choice. This changes only the host GUI dependency; it does not alter guest ABI or the native CLI wheel.

Run `37201321281` confirms that the revised aarch64 package can be installed and audited on CPython 3.11–3.14 in manylinux_2_28. The audit checks native import, CLI doctor, packaged resources and ELF library dependencies; it does not open a GUI. A real Linux aarch64 host still needs a separate Console renderer selection and visual validation.

The same run passed x86_64 wheel audits on Python 3.11–3.13. Its Python 3.14 wheel built and repaired but pip could not install the default PySide6 extra: releases compatible with the manylinux_2_28 baseline declared Python `<3.14`, while newer Python 3.14-compatible wheels need newer glibc. The package now excludes automatic pywebview/PySide6 on Linux Python 3.14 as well as aarch64. This keeps the native wheel installable and leaves GUI renderer selection open on those combinations. A final CI rerun must establish the full matrix.

A11 uses a pinned static dependency prefix, CMake/Ninja presets and an installed-wheel audit on Linux. This repository already has bounded Linux aarch64 host-native/wheel evidence, while its SDK exporter and Run/IDE wrappers still assumed Homebrew or a macOS `.app` path. The host code now selects LLVM through PATH or `SYMBIAN_LLVM_BIN`, chooses the Linux EKA2L1 CMake output path and can use `gdb-multiarch`. The pinned EKA2L1 checkout contains `linux_x64-build.sh` for FFmpeg but no maintained Linux aarch64 counterpart.

The host CMake smoke configure found `symbian_a11_source_check` still listed a deleted concurrency `README.md` after documentation reorganization. Its CMake source list now uses the tracked `.dev/thread-a11.md`; macOS configure, native build and 11/11 CTest cases passed after the correction. This was a host build-graph error, not a guest or Linux runtime result.

Open questions for a real Linux host: whether full source SDK export (including `rcomp`, Abseil, guest runtime and vendored Mbed TLS) passes on both host architectures; which Qt/FFmpeg package set the pinned patched emulator needs; whether disposable GUI and control sockets work under X11 and Wayland; whether the remote GDB frontend and source relocation work in CLion; and whether PySide6 Console and USB discovery/protocol results match the bounded host tests. Treat wheel and ARM build results separately from emulator execution and physical-device evidence.

## 2026-10-04: Transport and entropy boundary follow-up

The pinned EKA2L1 base socket returns false for `KSONonBlockingIO` (family 1/id 4), while its `KSOBlockingIO` case is itself a success stub. The internet socket uses asynchronous libuv send/receive requests, but the selected guest OpenC `fcntl(F_SETFL, O_NONBLOCK)` path still fails before the TLS BIO can attach. A one-line option-acceptance patch would not establish POSIX nonblocking `recv` or `send`; the next acceptance control must observe WANT_READ/WANT_WRITE and cancellation against a connected disposable guest socket. No emulator patch was applied in this pass.

The original EUSER source also defines non-leaving `Math::Random(TDes8&)`, but documents its output as possibly **not cryptographically secure** and discards the `Exec::MathSecureRandom` error. That route cannot replace the fail-closed Mbed TLS entropy callback. `Math::RandomL` preserves `KErrNotReady`, yet the EABI guest currently lacks the trap symbols needed by the adapter. `Exec::MathSecureRandom` is an internal executive interface explicitly marked subject to change; direct use would need an exact firmware ABI and emulator contract before consideration. A source search of the pinned EKA2L1 guest SVC bridge found only `math_rand`, which returns the emulator's common random value; it found no named `MathSecureRandom` handler. A direct executive-call adapter therefore would also need a verified emulator service path. Guest entropy, connected transport and authenticated TLS 1.2/1.3 handshakes remain open; development-agent gate 2 cannot advance.

## 2026-10-04: Guest POSIX socket nonblocking option

A temporary sixth DLL export called guest `socket(AF_INET, SOCK_STREAM, 0)`,
then `symbian_mbedtls_socket_bio_attach`. Normal RLibrary clients on both
Dynarmic and Dyncom exited `-146` (attach failure); changed-digest controls
still exited at their expected earlier SHA check. The EKA2L1 log on both
backends said `Unhandled base option family 1 (id 4)` followed by `Fail to set
value of socket option!`. The pinned emulator defines id 4 as
`SOCKET_OPTION_ID_NON_BLOCKING_IO`, while its base socket setter handles only
id 5 (`BLOCKING_IO`). This separates successful guest socket creation from
unverified nonblocking transport. The six-export temporary suite was 3
passed/2 failed; its source/test changes were removed after the diagnostic.
The [Nokia RSocket reference](https://cortex.p.gen.nz/nokia/symbian/Nokia%20Symbian%20Belle%20Developers'%20Library/GUID-C6E5F800-0637-419E-8FE5-1EBB40E725AA/GUID-D4F08503-F1EF-3531-9C3C-4AF24A6255F0.html)
documents `KSONonBlockingIO` and warns that a write-flowed-off send can still
complete with `KErrNone` for compatibility. That behavior needs an explicit
transport test; option acceptance alone is insufficient.

Open question: can EKA2L1 implement id 4 with the same observable nonblocking
receive/send semantics as the selected Symbian OpenC/EUSER path? Merely
returning success from `set_option` would conceal a blocking call and is not
an acceptable callback test. Inspect the original socket option contract and
the guest libc translation, then test WANT_READ/WANT_WRITE, in-flight
cancellation and connected I/O in a disposable emulator instance. This finding
does not establish an issue on a Nokia 808.

## 2026-10-04: Real IDE captures and minimal EKA1 planning

With macOS Screen Recording available, `screencapture` captured the prepared
`examples/gui_app` window in IntelliJ IDEA with its CLion plugin. The captures
show the source tree, active local ARM CMake preset, generated GUI Run/Debug
configuration selector and independent host LLDB versus guest GDB selector.
They were cropped to exclude the desktop dock and unrelated notifications;
the terminal regained focus after each capture. These are configuration
screenshots, not evidence of a successful debug session. The earlier
configuration SVG is no longer the guide's primary visual.

The EKA1 plan is intentionally narrower than general application support:
first establish one named firmware's entry/import ABI, then attempt a no-UI
process entry and recorded exit using the existing compiler, publisher and
emulator resolver. Firmware import for 7610/P900 already works; the EKA2
starter still rejects EKA1 before run output. Open questions are the exact
EKA1 executable/header and EUSER startup contracts, and whether the current
Clang/LLD/native converter can satisfy them without a new toolchain. No EKA1
code or dependency was added.

## 2026-10-04: Guest entropy EABI link boundary

The new `Math::RandomL(TDes8&)` callback compiled into both ARM archives, but
a real E32 DLL consumer linking `mbedtls_hardware_poll` failed on unresolved
`TTrap::Trap(int&)` and `TTrap::UnTrap()`. The pinned
`kernel/eka/eabi/euseru.def` exports `Math::RandomL` at ordinal 2503 but has
no EABI exports for these trap methods. Original ARM and x86 GCC definitions
do list trap exports, which is insufficient evidence for this EABI firmware.
An isolated exception-mode compilation instead referenced `__cxa_*`,
`__gxx_personality_v0`, and Symbian cleanup-stack functions. The SDK's guest
exception and EHABI support is still a separate unverified gate. Therefore
the SDK archive now uses a linkable failure callback; the leaving API adapter
stays as an unlinked source candidate. A rebuilt ARM archive linked into the
same E32 DLL with an entropy-probe export, confirming the import issue is
removed. A fresh 1,727-file source SDK export then passed the five-case dynamic
DLL suite, including the entropy failure check on both EKA2L1 CPU backends.
Open question: what measured, error-reporting secure-random route is
available on the named EABI firmware and emulator, and can it be exercised
without importing unverified trap or exception machinery?

## 2026-10-04: Guided native reference and visual documentation

The GUI source walkthrough was split at its actual stage boundaries into
preparation, architecture, build, emulator, debug, troubleshooting and runtime
articles. CLion now has shorter setup, Run/Debug and advanced articles; the
public instructions use project-relative examples instead of the maintainer's
machine path. The Console guide has two reproducible 2880×1800 captures of
its current frontend with a fixture context. The GUI emulator guide has two
retained 720×1280 guest framebuffer captures, showing a pointer-driven count
change. macOS `screencapture` returned `could not create image from display`;
the owner declined Screen Recording access for now. The CLion visual is an
explicitly labeled SVG configuration map, not a screenshot. Open question:
replace it with real IDE captures if screen capture access becomes available.

The SDK Doxygen landing page now explains host/guest surfaces, native error
rules and where to start. A separate Doxygen build indexes nine original
Symbian headers, kept apart from the SDK-owned APIs so historical declarations
do not appear to be implemented SDK features. The tracked snapshots retain
their EPL 1.0 notices and were copied from the pinned SymbianSource kernel,
graphics and camera checkouts recorded on that page. The snapshot filenames
were lowercased and contents retained. The combined strict documentation build
passed; the platform reference emitted 827 HTML pages. Upstream annotations
were mapped to Doxygen aliases, leaving 31 parser/documentation warnings from
the original headers. Open question: which additional original headers help
developers without implying unverified ROM support?

## 2026-10-04: Symbian secure random candidate

The pinned [EUSER source](https://github.com/SymbianSource/oss.FCL.sf.os.kernelhwsrv/blob/0c3208650587ac0230aed8a74e9bddb5288023eb/kernel/eka/euser/us_exec.cpp)
documents `Math::RandomL(TDes8&)` as filling a descriptor with cryptographic
random data and leaving with `KErrNotReady` when security cannot be assured.
The original EUSER `.def` maps it to ordinal 2503. A guest-only C++ adapter now
traps the leave, bounds the request to 1024 bytes and zeroizes output on
failure. The first fresh SDK export failed to compile because `e32math.inl`
had not been copied; the corrected export built both ARM profiles and records
the source ordinal in its EUSER proxy. This is an ABI candidate, not runtime
proof: the named ROM's ordinal behavior, emulator SVC implementation, random
service health and actual entropy quality remain unmeasured. Keep failures
closed and do not claim authenticated guest TLS from this build.

## 2026-10-04: Overview icons

The published overview showed raw `:material-...:` strings in its four cards.
MkDocs had the card Markdown but lacked `pymdownx.emoji`, which Material uses
to convert those shortcodes into SVG. Adding that extension with Material's
Twemoji index and SVG generator produced four SVG icon spans in the strict
local build. Run `37196505812` deployed the change; the live overview contains
four SVG icon spans and no literal shortcodes.

## 2026-10-04: Guest BIO cancellation probe

A fresh source SDK export includes the BIO source/header and ARMv5T/ARMv6
archive symbols. The test DLL now links all three Mbed TLS archives and exposes
a fourth ordinal that cancels a BIO before calling send and receive. Both
callbacks return `MBEDTLS_ERR_NET_CONN_RESET` without invoking guest socket
I/O. The five-case DLL suite passed on Dynarmic and Dyncom with the fresh SDK.
Open questions: does the named firmware support nonblocking libc socket calls
through these ordinals; can the callback be cancelled while a TLS operation is
in progress; which guest API supplies verified secure entropy; can the emulator
complete authenticated TLS 1.2 and TLS 1.3 handshakes with bounded memory?
The [upstream EUSER source](https://github.com/SymbianSource/oss.FCL.sf.os.kernelhwsrv/blob/0c3208650587ac0230aed8a74e9bddb5288023eb/kernel/eka/euser/us_exec.cpp)
describes `Math::RandomL(TDes8&)` as a cryptographic random request that leaves
with `KErrNotReady` if it cannot guarantee security, but
the imported firmware contract and emulator implementation are still unknown.

## 2026-10-04: Developer documentation site

The former root and `docs/` Markdown was mapped to MkDocs guides,
capabilities, tutorials and reference articles, or moved to `.dev/` when it
records planning or experiments. Relative links in moved pages were rewritten;
links from the public site to internal records now point to their public GitHub
locations. A local strict MkDocs build and Doxygen/Graphviz build pass. The
site uses A11's visual and code settings; copied Doxygen style assets retain
their upstream licenses. The first Pages run failed because the link checker
expected an ignored upstream research checkout in CI; the checker now treats
that path as optional while still checking published article links. Run
`37196098884` passed build and deployment. Four published routes, including
the generated C++ index, returned HTTP 200. The README and new credits guide
name EKA2L1, shinovon's mbedtls-symbian, Mbed TLS, SymbianSource,
SymbianRevive, A11, Abseil and LLVM with their distinct roles. Open question:
review guide terminology and onboarding flow with external developers.


## 2026-10-04: Vendored Mbed TLS and development-agent gate 2

The SDK source now comes from `third_party/mbedtls-symbian`, not the former
sibling checkout. The vendor tree contains the port's complete CMake project,
all copied source and headers, test fixtures, the Apache-2.0 license, and its
OpenSSL compatibility source/license. SDK export copies 1,721 files into a
sealed, inspectable source tree and independently builds all three archives on
both ARM profiles. The source tree digest, architecture archive digests,
source revision and explicit TLS gate state are in the active SDK provenance.
The project-local CA option packages a bounded PEM resource with a recorded
digest and no default roots. The new UTC adapter uses the SDK's
`clock_gettime`/`gmtime_r` imports and rejects values before 2020; the host
test passes and the ARM archive defines both required Mbed TLS symbols.
Host TLS 1.2/1.3 verification tests pass. The first guest SHA-256 DLL link
failed on unused mimalloc `realpath`/`pathconf` references. Retaining the
declared export and collecting unused sections removed those references; the
converted DLL explicitly imports EUSER, libc and libpthread. The maintained
five-case suite now passes, including dynamic `RLibrary` execution and a
changed-digest control on Dynarmic and Dyncom. This is guest SHA-256 execution,
not guest TLS or evidence of Nokia 808 loader compatibility.
An added DLL export exercises `symbian_mbedtls_utc_gmtime_r` with 2024-01-01
UTC, checks that 1970 is rejected, and checks that the emulated wall clock is
above the adapter's 2020 floor. The same five-case suite passes with this
probe on both backends; no physical clock/clock-change behavior was measured.
The next DLL export links `mbedx509` and parses an explicit self-signed test
root. Its guest verifier accepts `sdk-test`, rejects `wrong-name` with the
Mbed TLS name-mismatch flag, and rejects a leaf signed by that root whose
validity ended in 2011 with the expired flag. Five maintained cases pass on
the rebuilt SDK. The generated expired PEM fixture contains no private key;
the already vendored test key was used to sign it. The SDK exporter now selects
historical `libcu.def` ordinals for `strstr` and a bounded POSIX socket call
set; their presence in the proxy is link metadata, not an emulator transport
execution or a Nokia 808 ABI match.
The source port now includes `symbian_mbedtls_socket_bio` with nonblocking
send/receive callbacks and an atomic cancellation flag. Two host GTest cases
pass using `socketpair`; the full host suite passes 12/12. The descriptor
remains caller-owned and must outlive callbacks. This does not prove the
selected libc socket ordinals run under EKA2L1, nor that network access or
cancelled TLS handshakes work in the guest. The last SDK export predates this
source addition, so a fresh export is required before distributing it.

The former `library/symbian.c` entropy callback used C `rand()`; the vendored
copy now fails closed. [Historical Symbian cryptography documentation](https://docs.huihoo.com/symbian/s60-5th-edition-cpp-developers-library-v2.1/GUID-35228542-8C95-4849-A73F-2B4F082F0C44/sdk/doc_source/guide/Security-subsystem-guide/Crypto/cryptography.overview.html)
describes legacy random and CryptoSPI APIs, but the prepared SDK has no
verified random-service header, import or measured Nokia 808 entropy quality.
Open questions for gate 2: which callable guest source provides sufficient
entropy and fails visibly; how guest UTC maps across physical clock changes;
which socket API supports nonblocking, cancellable completion in the emulator;
how to bound TLS allocation and cancellation; and how to prove full
certificate rejection in guest TLS 1.2/1.3 handshakes. The SHA-256 link failure is resolved
for this bounded probe; broader TLS linking and guest runtime services remain
unverified. The
Nokia 808 firmware/import contract and independent recovery/backup gate remain
unverified; no phone installation or later development-agent gate was started.

## 2026-10-02: In-process desktop console and protocol interface

The new `symbian console` entry point launches a stock-Tk frontend. The
frontend's typed client reaches a FastAPI service through HTTPX ASGITransport
in the same process, without opening a host port. Its workflow catalog is
generated from the public CLI parser (38 executable leaves in this checkout).
Dedicated panels list libusb descriptors, inspect a supported device's
interpreted USB map and invoke the existing bounded AT, MTP and PC Suite OBEX
Connect/Disconnect probes. Output uses Pygments for language-aware highlighting.
The service sends the shared SDK `Status` across the transport and the client
raises `StatusException`; no console-specific error model is used. A device
command allowlist prevents future hardware commands from appearing as GUI
executors just because they were added to argparse.

Focused console tests passed 7/7, including status code/details preservation,
request-validation status, safe protocol parameter mapping and CLI launch.
CLI plus console tests passed 30/30; a combined CLI, console, status HTTP,
USB and device connection run passed 52/52 after the guided task view change.
A rebuilt wheel included all 15 current console modules and passed the
clean-installed wheel audit. A clean-wheel GUI smoke check built all five tabs,
loaded 38 tasks and enumerated 14 host USB devices plus one supported handset.
A real end-to-end device protocol operation was not initiated from the GUI
during this check. The revised UI uses native light
controls, drops its redundant banner and outer padding, presents a Home page,
and groups all 38 tasks by purpose. Multi-step application, firmware, emulator
and device forms validate required values and numeric formats; firmware import
shows only the selected source type. Open questions: what higher-level OBEX
operations the phone actually supports, and how to present large device
responses more selectively than a capped structured text view.

## 2026-10-02: Mimalloc-default GUI package staged on connected Nokia USB volume

The active local SDK was `.symbian/mimalloc-default-sdk-20261002`. A fresh
ARMv6 `gui_app` build compiled with `SYMBIAN_RUNTIME_MIMALLOC=1` and produced
a reproducible E32 executable with SHA-256
`63117a307ce8285c63d490c47ebe6d7334d49c401019e2a53d9f3af306b065ce`.
The native SIS writer packaged it with application registration, localized
captions and icon at `.symbian/gui-app-phone-package/gui_app.sis`, SHA-256
`be5ab91b933af743113dd1bba74c3edb626129cbcc04427ba3aec4a0d86ddcad`.
The 17-case GUI package verifier passed historical checksums, EKA2L1
installation, registry reload, removal and reinstall. The GUI's rendered
input/reset/normal-exit test had already passed both CPU backends against
the same SDK.

`symbian device list` observed one Nokia `808 PureView` USB descriptor
(`0421:05d0`) with one writable FAT32 `disk4` volume associated by USB
ancestry; RM code, installed firmware and OS version remained unknown.
`symbian device install --package` copied the SIS to
`/Volumes/NO NAME/Installs/gui_app-be5ab91b933a.sis` and verified the copied
SHA-256. `diskutil eject disk4` completed normally. The device operation
returned `awaiting-on-device-install`, with `on_device_verified=false`:
the agent had no installer control channel and did not observe handset
approval, installed-app registry state or an on-phone launch. The owner must
open the SIS in the handset's `Installs` folder and report the installer
result. The package is unsigned and phone policy may reject it.

## 2026-10-02: Mimalloc becomes the default guest runtime profile

The user explicitly selected mimalloc as the default after the bounded
per-thread cache and cross-thread allocation tests. Both standard and streams
runtime archives now compile pinned mimalloc v3.5.3
(`d4881d338125e1cb7c47ba4cfb398d6f7c0c8d45`) by default. SDK export
checks the source revision and clean checkout, then includes `mimalloc.h`,
the accompanying headers, the MIT license and both compiled ARM archives.
The original `RHeap` bridge remains a `SYMBIAN_RUNTIME_MIMALLOC=OFF` source
profile. The separately selected native 64-bit atomic archive continues to
use that bridge: mimalloc's generic atomic fallback currently calls a lock
helper supplied only by the portable atomic implementation, and the native
EUSER 64-bit imports are not verified across ROMs. There is no intrinsic
incompatibility between mimalloc and native atomics.

The exporter built each mimalloc archive twice and compared bytes. The
installed SDK's standard and streams archives contain `mi_malloc`, `mi_free`
and the native cache entry hook. A packaged-header probe included
`<mimalloc.h>` and called `mi_malloc`, `mi_realloc`, `mi_usable_size`, `mi_free`
and `mi_collect` from installed archives. Standard and event/worker probes
both exited 0 on ARMv5T/ARMv6 under Dyncom/Dynarmic (8/8), including
private-heap cross-thread free, raw and managed thread cache paths and
bounded memory reuse. The GUI's rendered input/reset/normal-exit test passed
both backends (2/2). SDK copy, generated app initial build and moved-project
rebuild passed. The completed local SDK has 3,148 matching payload digests.
These are firmware-backed emulator results, not physical-device performance
or compatibility proof. The earlier allocation microbenchmark favored the
cache-equipped mimalloc profile, but application latency, fragmentation and
peak committed backing on a phone have not been compared. The source build
still requires the pinned ignored upstream checkout; installed consumers need
only the SDK headers, archives and imported system libraries.

## 2026-10-02: Private-heap cross-thread free and RChunk owner lifetime

Original EUSER `User::Free` dispatches through the **calling** thread's heap
(`kernel/eka/euser/us_exec.cpp`), so the previous `new`/`delete` bridge was
unsafe when a future or worker object crossed private `RThread` heaps. The
original `RAllocator::Open`/`Close` implementation atomically counts heap
users and closes only on the last release (`kernel/eka/common/alloc.cpp`).
Original `CreateThreadHeap` uses a multithread-capable `RHybridHeap` by
default (`kernel/eka/common/heap_hybrid.cpp`). The runtime now stores the
exact creating `RHeap*` in an eight-byte allocation header and holds one
`Open` reference per allocation. A consumer directly calls that heap's
`Free`, then `Close`; it neither queues to nor waits for the creator thread.
The `RChunk` page bridge holds the creator heap open for its metadata too,
and closes it after the process-owned chunk handle and metadata are released.
This reuses the original heap synchronization and adds no global allocator
lock; its per-allocation atomic lease and heap-lock latency remain unmeasured.

The first installed-SDK probe failed 8/8 with -156 because it assumed
`std::thread` uses a different EUSER heap. A split diagnostic returned -158
on ARMv5T/Dyncom: both std::thread participants used the same heap in this
ROM. The replacement uses the existing explicit `RThread::Create` bridge,
which creates a private heap. It checks object and `RChunk` metadata release
after producer exit, reverse-direction object release, and a consumer free
while the producer is still alive. Its normal/changed controls passed 8/8
on ARMv5T/ARMv6 × Dyncom/Dynarmic (52.06 seconds). The changed control still
returns -154. The event executor matrix passed 8/8 (48.29 seconds), root
CTest passed 10/10, the GUI linked, and generated-project initial build,
SDK copy and relocation passed. `.symbian/cross-thread-heap-sdk` has 3,135
payload files with matching SHA-256 digests and no missing or extra files.
The local source-only std-thread harness initially lacked modern libc++
imports; the selected installed SDK supplied its complete closure. Its
ignored bootstrap EUSER proxy was regenerated to include the three original
heap symbols before the candidate export; no frozen DEF was changed.

Mimalloc v3.5.3 (`d4881d338125e1cb7c47ba4cfb398d6f7c0c8d45`) was
inspected in an ignored upstream checkout. Its own
documentation describes sharded concurrent-free lists and v3 heaps usable
from any thread, which could reduce contention. At this point its `prim.h`
still required OS reserve/commit/free, TLS and thread-exit hooks; the
experiment below implements them separately from the default page bridge.
The native heap lease remains the verified default.
The installed `global-nothrow` allocation-failure controls also passed 4/4
on the same candidate after the header and overflow checks were added.

## 2026-10-02: Optional mimalloc v3.5.3 guest allocator

The ignored source checkout is pinned to
`d4881d338125e1cb7c47ba4cfb398d6f7c0c8d45` (v3.5.3, MIT license).
`SYMBIAN_RUNTIME_MIMALLOC=ON` builds its original C sources with an explicit
Symbian `prim.h` adapter. A process-owned disconnected `RChunk` reserves
address space, then `Commit` and `Decommit` change physical backing as mimalloc
requests it. A fixed 64-slot handle table avoids metadata from a worker's
short-lived heap. OpenC pthread keys provide per-thread state and a destructor;
the original `RThread` ID supplies mimalloc's unique thread ID. The port uses
an 8 MiB virtual arena reserve option, down from upstream's much larger
default, with no physical commitment at reservation. The bridge also limits
total virtual chunk reservation to 64 MiB by default, configurable from 16
to 512 MiB. This is an address-space budget; other process memory is outside
it. The guest probe verifies that an allocation larger than this budget fails
without consuming more reservation. The selected EUSER import list gained the three
original disconnected-chunk exports. The candidate SDK was not regenerated.

The first exact-sized `RChunk` mapping left about 64 MiB committed after a
small cross-thread probe. Disabling arenas increased that to about 193 MiB
because aligned OS requests committed entire over-allocations. These mappings
were discarded. The disconnected mapping passed the same private-heap
cross-thread test on ARMv5T and ARMv6 under both Dyncom and Dynarmic. The
test also allocates and frees two 512 KiB bursts, asserting at most 2 MiB
additional committed backing during each burst and at most 512 KiB after
collection. One ARMv6 diagnostic found 580 KiB committed after collection.
The guest's default main-heap cell count remained balanced. A 4 MiB nothrow
allocation succeeds under this optional backend, unlike the default image's
1 MiB heap contract; the probe checks the selected policy explicitly.

A warmed 32,768-iteration burst of 64-byte allocations and frees took
12,048 native fast-counter ticks with mimalloc and 1,595 with the current
heap on Dyncom. The same binaries took 12,058 and 380 ticks on Dynarmic.
This is an emulator microbenchmark, not a physical-device or UI latency
result. The optional port is substantially slower on this workload, even
with `MI_DEBUG=0`; any cross-thread contention benefit remains open. It must
not replace the default allocator without a better latency,
memory pressure and application-level comparison. The source port and
`alloc_bench.cc` make that comparison reproducible without changing the
verified default.

A follow-up diagnosis isolated the cost. During the warmed 32,768-pair
allocation burst, the port made only two disconnected-chunk reservations and
three commits, with no decommit; `RChunk` operations are not repeated per
allocation. It made 33,027 `pthread_getspecific` calls, including the 256
warm-up pairs. A separate 65,536-call pthread TLS loop with checked results
took 22,897 ticks on Dyncom and 21,989 on Dynarmic. A 65,536-call 64-bit
atomic fetch-add loop
took 677 ticks on Dyncom. Replacing the thread ID with a constant for a
single-thread diagnostic barely changed the allocation burst (12,366 ticks).
A deliberately single-thread-only cache of mimalloc's TLS values reduced the
same burst to 629 ticks. Both ablations were removed immediately; they are
unsafe as a worker-thread implementation. The evidence points to imported
OpenC pthread TLS lookup on mimalloc's fast path, not the `RChunk` page
primitive or the portable 64-bit atomic lock. A production native TLS cache
needs per-thread ownership, exit cleanup and reuse tests before use.

The follow-up uses a process-static 256-slot direct-mapped table keyed by the
full native `RThread::Id`. Slots are claimed only by explicit
`SymbianRuntimeThreadCacheEnter`; a collision or unregistered thread falls
back to the real pthread key. The mimalloc setter mirrors every value to the
real key before updating the owner slot, so fallback and thread-exit cleanup
keep their original state. The process main thread enters after mimalloc
initialization; the guest `WorkerExecutor` pairs entry and exit around its
worker loop. A native owner can use the same pair. No slot stores a pointer
to a foreign thread's heap, and a remote free does not wait for that thread.
The fixed table has no allocation, explicit lock or eviction on the fast path;
it uses the existing 32-bit atomic bridge on ARMv5T.

The initial automatic registration failed the raw `RThread` lifecycle test:
this firmware did not invoke the OpenC pthread key destructor for a direct
`RThread::Create` callback. It left a cache entry after exit. Registration is
now explicit, so raw and ordinary unregistered threads retain the original
pthread lookup. Sixteen raw native threads and sixteen explicitly managed
native threads left the entry count at its baseline; at least one managed
thread held a live slot while allocating. The full optional allocator probe
passed on ARMv5T and ARMv6 under Dyncom and Dynarmic, including private-heap
cross-thread free and committed-backing limits. The same warmed 32,768-pair
burst measured 817 ticks on Dyncom and 116 on Dynarmic with the safe main
thread cache, versus 1,595 and 380 for the default heap. A fresh ignored SDK
candidate at `.symbian/mimalloc-cache-sdk-v2` links the worker entry/exit ABI;
its installed event executor probe exited 0 on ARMv6 under both backends.
An ARMv6 source-built mimalloc plus stream-runtime link against that SDK's
guest fiber archive also passed the combined event, worker, native-thread and
base allocation probe on Dyncom and Dynarmic. The first combined run exposed
that mimalloc could return a small size-class pointer with less than
`max_align_t` alignment for `new(0)`. The C++ bridge now requests at least
`alignof(max_align_t)` bytes in this optional profile; the full rerun passed.
These are emulator observations. Forced termination of a managed native thread
can skip paired cleanup; owners must not forcibly kill a cache-registered
thread. The allocator stays optional until application latency and memory
pressure have been compared on a device.

## 2026-10-02: Explicit stackless and fiber worker placement

Pinned A11 `Then` and `Future::OnReady` run inline on the completing thread;
`TreeOptions` describes fiber stack/name rather than continuation placement.
The guest adds an opt-in `WorkerExecutor`.
`future.ThenOnWorker(event_executor, transform)` obtains the event owner's
lazy worker; `ThenOn(future, worker, transform)` is the direct lower-level form.
Both forms register a cheap handle enqueue on the completing thread, then copy the
result and runs the transform stacklessly on one worker OS thread. A rejected
enqueue completes the returned Future with the queue error. `PostFiber` uses
the existing guest `thread::Scheduler` on that worker, keeping live fiber
stacks pinned. `EventExecutor::workers()` creates one worker lazily so app
components can share it. Queue admission counts both waiting jobs and live
fibers. `Close` requests drainage without joining on the event thread;
`Finish` returns an asynchronous Task. A permanently waiting fiber can keep
that task unresolved until cancellation trees are available.

The first worker-placement installed-SDK matrix passed 8/8 across ARMv5T,
ARMv6, Dyncom, Dynarmic and normal/changed controls (46.12 seconds). After
moving result copying to the worker, counting active fibers against the cap,
and adding the shared accessor, the new candidate passed the same 8/8 matrix
in 44.07 seconds. The probe checks stackless worker affinity, separate
worker-fiber affinity and sleep, a saturated one-work limit, rejected post
after finish, and an event-fiber Await of asynchronous worker drainage. The
GUI linked against this candidate; the generated-project copy/relocation
test passed. These tests establish routing and progress, not a bounded
latency guarantee under a non-yielding compute task or a complete A11 pool.

The guest-only member `future.ThenOnWorker(event_executor, transform)` now
selects that executor's lazy shared worker explicitly. A Future has no owner
identity, and completion may occur on another OS thread, so automatic
selection from the completing thread would be ambiguous. A closed executor
produces a failed Future. A fresh 3,135-file candidate at
`.symbian/then-on-worker-sdk` has no missing, modified or extra payload files.
Its normal/changed guest matrix passed 8/8 in 48.92 seconds; the GUI linked,
the generated-project copy/relocation test passed, root CTest passed 10/10,
the original three A11 host executables passed, and the source pin verified
46 files and 92 local include edges. No physical-device result is claimed.


## 2026-10-02: Guest event selection and dispatcher scheduling

The licensed guest adaptation of pinned A11 `cases.h`, `selectables.*` and
`select.*` now builds into the guest fiber archive. `PermanentEvent::Notify`
holds the event and selector locks while choosing and unlinking a waiter, but
defers the fiber wake until after both locks are released. The selector is
shared with the deferred wake so a racing timeout cannot destroy its
condition variable first. Timed selection converts the accepted absolute
wall deadline once and measures subsequent elapsed time with a monotonic
clock. Round-robin first-case rotation replaces A11's random choice to keep
the guest closure bounded. An empty case list and unrepresentably distant
finite deadline currently abort under A11's `int`-returning API; a checked
entry point is still needed for recoverable guest errors.

The final installed-SDK candidate `.symbian/select-sdk-candidate-final`
contains 3,133 digest-valid files with no unrecorded payload. It passed the
guest event probe 8/8: ARMv5T/ARMv6, Dyncom/Dynarmic, normal and changed
controls (46.17 seconds). A further probe with explicit event-OS-thread
affinity assertions for the property continuation, completion callback and
waiting fiber passed the same 8/8 matrix (43.46 seconds). The selection
probe exercises immediate readiness,
ready-over-expired selection, timed fiber expiry, 500 expired polls followed
by notification, worker-thread notification and simultaneous events. The GUI
linked against this exact candidate; the generated-project initial-build,
SDK-copy and relocation check passed (13.88 seconds). The source pin still
verified 46 files/92 include edges; the three original host executables and
root CTest 10/10 passed.
This maps part of pinned `ThreadSelectTest` behavior to guest execution, but
does not prove selectable channel read/write, zero-capacity rendezvous,
cancellation cases or original A11 test coverage.

On a single-core guest, the event executor runs callbacks and ready
fibers on its existing event OS thread. There is currently no enforced
short-work limit; a callback or fiber may be compute-bound. That path avoids
a worker handoff and
lets timer/property completion directly ready the continuation in the same
dispatch turn. Its bounded per-source turns and request-semaphore wake keep
pending native work visible. `Scheduler::RunReady` now reuses its ready
snapshot across fiber yields, avoiding repeated vector allocation; event
notification uses an inline-capacity wake list. Arbitrary C++ callbacks are
cooperative and cannot be preempted: a compute-heavy callback can delay
native service indefinitely. The per-source count budget cannot bound that
delay. The future pool should reserve worker dispatch for explicitly
compute-heavy tasks, batch such tasks, and keep event-affine callbacks short.
The one-core context switch for an actual worker handoff remains unavoidable.


## 2026-10-02: First shared guest timer/property/fiber event owner

The source-pin check still matches 46 A11 files and 92 local include edges.
The isolated original host tests passed 23 thread cases, 10 introspection
cases, and 7 of 8 affinity cases; the CPU-pinning case is skipped on macOS.
These are host reference tests, not guest parity evidence.

The new guest EventExecutor drives the existing TimerPump, PropertyWatch,
EventMailbox and OS-thread-pinned Scheduler in bounded turns. It offers an
explicit event-thread dispatch method and arms fiber deadlines through the
same timer request owner. Only TimerPump consumes the thread's native request
semaphore. PropertyWatch removes an entry before invoking inline completion;
TaskGroup::Finish now returns the same asynchronous join task on repeated
calls. NativeTaskOwner starts one timer and one property subscription,
forwards cancellation and an absolute deadline, and publishes its join after
both child results and deadline-alarm drainage. The GUI and generated
timer-task bridge use the event owner.

The first standalone probe link failed because it combined the source runtime
archive with the installed SDK's streams archive, producing duplicate libc++
symbols. Building the probe against the installed Stackless target and its
full EUSER proxy fixed the build configuration. Normal and changed-result
guest controls passed 8/8 over ARMv5T/ARMv6 × Dyncom/Dynarmic. The probe
observed an actual RProperty value of 17, a timer, a fiber Await and sleep,
property cancellation, reentrant resubscription, Await timeout without
overwriting the producer result, abandonment, cross-thread event dispatch,
repeated join, structured owner success/close/timeout/error, and balanced heap
cells. An owner can be destroyed before native cancellation completes; its
returned join then settles after the event owner drains both requests. A fresh
SDK candidate was exported under `.symbian/event-executor-sdk-owner` with
3,130 digest-valid files; the GUI and generated starter both linked with it.
The GUI's pixel/input/reset/normal-exit test passed on both emulator backends.
The physical phone was not accessed.

Open questions: timer/property statuses still have separate adapter state,
and Window Server statuses remain in the application; the executor has one
request-semaphore consumer but is not yet a general native status registry.
The TaskGroup and NativeTaskOwner probes start children before adding them
to the group, so they do not yet prove child registration before native
submission. Fiber stack guards, native TRAP/leave safety, A11 Select/channel
rendezvous, trees, worker pools, complete futures and PythonLoop parity remain
open. The candidate SDK has not been promoted to the visible installation.

## 2026-10-02: Selected exception boundary for full A11 host thread

The previous blanket interpretation of the SDK's no-exceptions policy kept
the host adaptation from using A11's actual Boost fiber teardown and pool.
The owner clarified that the SDK should follow A11: exceptions off by default,
with selected translation units allowed to enable them. An isolated build of
the pinned, unmodified A11 `cpp/thread` compiled with `-fno-exceptions` on
the library and `-fexceptions` on only `boost_primitives.cc` and
`thread_pool.cc`. Its original `thread_test`, `fiber_introspect_test` and
`thread_affinity_test` passed (3/3). A maintained standalone CMake probe now
records the exact dependency and flag setup. This is a host feasibility result;
the SDK-distributed staged host archive was not silently replaced, and the
guest still needs an ARM-specific fiber backend and lifecycle validation.

## 2026-10-02: Host A11 scheduler and Python boundary adaptation

The bounded host `thread::Fiber` previously ran on Boost's default scheduler,
which could park while retaining CPython's GIL. An A11-derived private Boost
algorithm now owns the ready queue, accepts a per-OS-thread policy, and wraps
each idle park in the `SchedulerParkGuard` release/acquire pair. A pybind
boundary installs A11's `PyEval_SaveThread`/`PyEval_RestoreThread` callbacks.
The installed wheel's fiber-park test let another Python thread run during the
native wait and returned with the GIL held. Its native test checked a custom
last-ready ordering policy and balanced park callbacks.

The host now builds a single shared stackless callback pool for `Post` and
`PostAt` using A11's original `WorkQueue` implementation. A publication
sequence plus an idle-worker count avoids lost wakeups and unnecessary
notifications. Accepted absolute deadlines are converted once to steady-clock
waits. The original A11 `cases.h`, Select and PermanentEvent implementations
were copied with licenses; Select's fiber-registry instrumentation was removed
because that registry is not yet adapted. Native tests covered 256 move-only
posts, absolute and infinite deadlines, a level-triggered event, fiber
parking and timeout. The host archive remained self-contained with no Boost
consumer include or separate link dependency.

The Python boundary adopts A11's raw-reference holder: a destructor retires
references without acquiring the GIL, and binding entry/atexit paths drain
them with the GIL held. A bounded Future-to-asyncio bridge captures the running
loop, releases the GIL around OnReady registration, reacquires it on completion
and dispatches across threads with `call_soon_threadsafe`. An extracted wheel
passed completion, cancellation and deferred-reference checks. Full A11
PythonLoop resolution, fiber-tree lifecycle, pooled fiber work stealing,
channel selection, introspection and finalization stress remain open; neither
the guest nor the host is advertised as full A11 parity.

## 2026-10-02: Automatic control-body braces and starter link closure

clang-format 23's `InsertBraces: true` enclosed one-statement `if`/`else`
and loop bodies in the first-party C++ sources and generated starter. The
root and template configurations are byte-identical. Formatting 47 owned
files left 170 inspected first-party files idempotent; the two pre-existing
user-owned edits were left untouched. An installed-SDK `symbian init` initial
build then failed with unresolved guest `thread::Fiber::Current` and
`thread::Scheduler` references from the now fiber-aware lock header. The
starter linked `Symbian::Stackless`, whose imported CMake target did not
include the guest fiber archive. The target now links `Symbian::Fibers` when
that archive is present, with an acyclic Abseil dependency. The same
initial-build, copied-SDK and moved-project test passed after the repair.
The selected SDK has 3,123 verified payload files; its changed originals
were copied to `.symbian/formatter-sdk-backup-20261002`. The macOS wheel
contains `symbian/project/templates/.clang-format`.

## 2026-10-02: Guest fiber-aware locks and custom scheduling slice

The host's incomplete no-Boost OS-thread fallback was removed at the owner's
request. Boost.Fiber/Context is now a required private host build dependency;
the opaque host SDK ABI does not expose Boost to consumers. The host build now
uses LLVM ar's MRI interface to merge the SDK primitive objects with static
Boost.Fiber/Context objects into one archive. Its CMake consumer link command
contains `libsymbian_host_primitives.a` and no Boost library path; both host
concurrency CTests pass, and `otool -L` on the fiber test names no Boost dylib.
A separate host-package CMake build from the pinned local Abseil checkout also
produced that archive. A previous forced-no-Boost test documents the discarded
branch, not current support.
The exported `Symbian::HostConcurrency` target compiled, linked and ran a
consumer while CMake Boost discovery was explicitly disabled. The final
visible SDK carries the host and both guest archives and verifies 3,122
payload hashes after a targeted copy that preserved the prior tree.
The bundled Boost license was copied from Boost's published
`https://www.boost.org/LICENSE_1_0.txt` (SHA-256
`c9bff75738922193e67fa726fa225535870d2aa1059f91452c411736284ad566`)
into `third_party/boost/` and the SDK payload.

The guest now builds `thread::Fiber` on the previously verified ARM/Thumb
`SymbianFiberSwap`, with an explicitly pumped, OS-thread-pinned
`thread::Scheduler`. `thread::SchedulerPolicy` can select a ready fiber and
receive a cross-thread wake notification outside the scheduler lock. Guest
`thread::Mutex` parks a contending fiber instead of blocking its OS thread;
`thread::CondVar` parks and releases its mutex, supports signal/broadcast and
monotonic timeouts; `thread::SleepFor` parks a fiber. Outside a fiber, the
primitives retain OS-thread behavior. An unresolved guest `Future::Await`
parks only inside a fiber and still fails clearly on the event thread outside
one. A normal/changed control executes C++ cleanup, contention, a future,
timeout, a custom LIFO policy and notification from a second OS thread. It
passed 8/8 on ARMv5T/ARMv6 × Dyncom/Dynarmic with an installed Abseil SDK.

The guest Boost.Fiber/Context headers are not currently a direct substitute:
an ARMv6 `-fno-exceptions` syntax probe against the installed target headers
and host Boost 1.90 headers fails in Boost's Symbian config
(`"Unsuppoted Symbian SDK"`) and in `boost/context/fiber_fcontext.hpp` at its
`throw forced_unwind`. Defining Boost's old `__S60_3X__` selector removes the
first error but not the forced-unwind error. No pinned Symbian ARM Boost binary
or matching OS/TLS/stack closure exists in the SDK. This does not prove Boost
could never be ported with an explicit alternate exception backend;
the bounded native ARM backend already executes and keeps Boost private to
hosts. The new scheduler is not A11's shared pool, fiber tree, `Select`,
`PermanentEvent`, cancellation/join contract or native request owner.
The event loop must pump `RunReady` and arm its single native wait for
`NextDeadline`; that integration is still open. The present 16 KiB heap
stacks have no guard/high-water accounting, and no native TRAP/leave boundary
or floating-point context test has been passed. These remain C3 gates.
## 2026-10-02: One host/guest concurrency layer and private Boost backend

The guest's buffered `thread::Channel<T>` and bounded
`symbian::concurrency` Future/Task, fan-in, TaskGroup, inline pump and mailbox
had no Symbian dependency above `thread::Mutex`/`CondVar`. They now have one
source under `cpp/symbian/concurrency/common/`. Guest export copies that
layer and then its OS-thread primitive backend into the same installed include
tree. The host `symbian::concurrency` CMake target selects a separate opaque
primitive header and private Boost.Fiber/Context implementation when Boost is
available, with an OS-thread fallback when absent. An ordinary host consumer
builds with exceptions disabled and neither includes nor directly links
Boost. The host SDK library statically contains Boost; a symbol export list
hides Boost and Abseil implementation symbols. `otool -L` and `nm -gU` checked
the no-Boost dependency/export boundary on macOS. A forced no-Boost CMake
build and its concurrency GTest passed.

The first host fiber test directly constructed `boost::fibers::fiber`; that
violated the intended public API and, when the test linked a second static
Boost copy, hung due to separate runtime scheduler state. The test was
replaced by a bounded `thread::Fiber` host API with private Boost construction,
same-OS-thread join, cooperative cancellation and normal C++ cleanup. Its
consumer tests now use only `thread::Fiber`, `Mutex`, `CondVar` and `SleepFor`.
Wrong-thread and repeated joins return `FailedPrecondition`; destruction of an
unjoined fiber terminates, matching the explicit-join ownership contract and
avoiding unverified forced unwind. This is not A11's tree/Select/pool backend
and is not yet a guest fiber implementation.

The earlier guest `CondVar` control had the reverse boolean convention from
A11. Both backends now return true on timeout. An updated installed-SDK
stackless/timer/channel matrix passed 16/16 on ARMv5T/ARMv6 and
Dyncom/Dynarmic. Root host CTest passed 10/10, including an actual
`thread::Fiber` Await/parking, same-thread join and C++ cleanup test. A fresh
workspace SDK export then passed 4/4 timer/Future guest executions and a
generated-project SDK copy/build check. Its 3,113 payload digests verified.
The visible SDK was updated only with the four functional changed files after
comparing the export; the previous visible tree remains at
`~/dev/symbian-sdk-before-shared-concurrency-20261002`. Both trees have
3,113 digest-valid files. The pinned 46-file A11 source and 92 include-edge
check still passes unchanged.

## 2026-10-02: Guest channel and readable CLI output

The pinned A11 `thread/channel.h` depends on `Select`, fiber cancellation,
fiber-aware primitives and exception-throwing writes to a closed channel. The
guest default disables exceptions and does not yet have fiber parking. An
explicit OS-thread adaptation now exposes `thread::Channel<T>`, `Reader<T>`,
`Writer<T>`, `Mutex`, `MutexLock` and `CondVar` under the original `thread::`
namespace and header paths. Blocking channel reads/writes are limited to OS
worker threads; nonblocking `TryRead`/`TryWrite` return Abseil status and are
used by `EventMailbox`. The queue is bounded and FIFO, closed reads drain,
blocked writers wake on close, and a failed move-only write retains its
payload. Zero-capacity rendezvous, `Select`, fiber parking and A11's throwing
closed-write signature remain open. The guest runtime lacked the original
libc++ `condition_variable_destructor.cpp`; the link failed until that pinned
source was added to both ARM runtime archives. A 2 ms no-signal control then
observed `wait_for` reporting `no_timeout` without a signal on the guest.
The adapted timed wait now checks an explicit signal generation and tracks
remaining time with the verified monotonic clock in bounded slices. The
normal/changed stackless and timer/channel controls passed 16/16 across
ARMv5T/ARMv6 and Dyncom/Dynarmic. A fresh SDK export was promoted to the
visible path; its 3,113 payload digests after the CLI refresh and the
preserved prior SDK's 3,110 digests verified.

The CLI previously emitted only canonical JSON, including for `device list`.
Human-readable summaries are now the default; `--output-format=json` preserves
the existing schema and works before or after nested commands. Terminal output
uses color only on a TTY without `NO_COLOR`, and nested help describes every
option. Root help also lists every command group, and long scalar lists now
print one item per line. The connected device control displayed the observed
Nokia USB and mounted storage without printing its serial. The output-mode
tests, 44-test CLI/device/control/SDK group and selected-SDK application
regression passed. The rebuilt macOS wheel passed its isolated installed-wheel
audit, including canonical JSON output from `doctor`.
The separate package fixture still encounters its preexisting ARM unwind-index
exception-descriptor gate before its CLI assertions run.

## 2026-10-02: SDK selection without application-level provenance

Generated projects contain only an ignored `sdk-location.json` path and
shared application preferences; the installed SDK's own short `sdk.json`
describes its tools. The detailed `examples/gui_app/sdk.json` was a research
source-selection manifest, not an application SDK lock. Its bytes were moved
to `research/gui_app/source-profile.json`; the explicit staging command and
SDK export now read it there. Against the pinned upstream source tree, source
preparation still succeeded. A relative SDK path switched a generated
application's CLI build to a second SDK installation and back, with both
compiler commands and compiler identity changing. A direct CMake/Ninja build
before the fix retained the old compiler and reported no work. Adding
`CMAKE_CONFIGURE_DEPENDS` to generated `sdk.cmake` made direct builds
reconfigure and rebuild on both switches. The real GUI source/link test
passed after correcting its stale two-DLL import expectation to the current
six-DLL profile. No hashes were removed from the reproducibility report or
from SDK source verification. The prepared-workspace export completed with
the moved profile and its 3,109 payload digests verified. The visible SDK's
Python utilities and template were refreshed and its 3,109 digests verified;
the global SDK selection was restored to that visible installation.

## 2026-10-02: Localized AppArc resources and SVG icon container

The original `e32lang.h` supplies the language-number mapping, while
EKA2L1's original AppArc `get_nearest_lang_file` selects `.rXX` when present
and `.rsc` otherwise. This lets the SDK expose BCP 47 tags and package all
translations without exposing RSS, RLS or SIS language groups to an author.
The first UTF-8 RSS trial with `rcomp -u` produced mojibake: independent
AppArc decoded `Zähler` incorrectly. Original `rcomp` defaults to CP1252
even in Unicode-output mode. Adding its supported `CHARACTER_SET UTF8`
directive fixed the independent parser. The original EKA2L1 MIF reader
recognizes a version-2 MIF containing a gzip-wrapped SVG entry; the SDK's
native writer emits that bounded, deterministic container. Its original
reader recovered the SVG after headless installation. The installed AppArc
selected French, German and Japanese resources, including correct Unicode
German and Japanese captions, on Dyncom and Dynarmic (8/8). No actual phone menu icon
rendering, language-switch UI, or in-app translation is claimed yet.

## 2026-10-02: Application-menu registration

The original `AppInfo.rh` defines `APP_REGISTRATION_INFO` and
`LOCALISABLE_APP_INFO`; the pinned EKA2L1 application list expects registration
under `private/10003a3f/import/apps` and local resources under
`resource/apps` on the EXE's drive. An EPL-licensed original `rcomp` from
`SymbianRevive/symbian-build` revision
`d3c2eadd3ff7826bdf9e1d92f447c357571af18b` compiled both RSS forms
after the tracked modern-host pointer-width/header patch. A UID helper
reproduced the native Symbian resource header checksum. Its patch
reverse-checks on the prepared source tree. The first compilation omitted
`rcomp -u`; the native SIS installer accepted those byte-text resources, but
EKA2L1's original AppArc parser decoded a corrupt localisable path and
rejected the caption. Enabling `-u` produced 97-byte registration and 80-byte
caption resources for `gui_app`; its original parser then read the expected
path and both captions on Dyncom and Dynarmic. The native SIS reader
verified three embedded file digests and canonical paths. A fresh
`symbian init` ARMv6 project built and packaged from the 3,108-file sealed SDK;
`gui_app` packaged the same way. EKA2L1's original headless SIS installer
accepted the registration and caption files on Dyncom and Dynarmic, reloaded
their registry entries, removed them and reinstalled them (8/8). Its import
negative control was updated for the current six-DLL executable instead of
assuming the old two-DLL/38-slot shape; it still checks every imported slot
remains unresolved without system DLLs. The final export added pin/patch
provenance and its 3,108 digests verified. It replaced the visible development
SDK after preserving the old 3,100-file installation at
`~/dev/symbian-sdk-before-menu-registration-20261002`; the visible SDK produced
corrected Unicode resources after a deliberate Python-tool update and
digest reseal; 3,108 files match and there are no extra files. Fresh ARMv5T
portable and ARMv6 default
starters both built and packaged through that visible SDK. After the owner
confirmed the photo copy was complete, the SDK staged the corrected ARMv5T
portable SIS in the phone's existing `Installs` directory, verified SHA-256
`db4875221d6ecabbe02bfba76c4201c5da5f8f0df298ae50f7f5050356e70db1`,
and ejected the USB disk. The installer result and handset menu are not yet
observed by SDK automation. The owner subsequently reported successful
on-phone installation and a visible `menu_v5` application-menu entry. This is
user-supplied observation. The owner then opened the app and reported that it
responded to a tap. No on-device registry API, launch trace or device log was
collected.
The updated visible-SDK `verify-gui-package` ran 17 original image/checksum
and installer cases against the registered GUI SIS. It counted 153 imported
slots in the current six-DLL image rather than retaining the obsolete fixed
38-slot report. No guest instructions ran in that headless package test.

## 2026-10-02: Read-only physical USB discovery and SIS staging preparation

The owner's connected Nokia 808 is visible as `0421:05d0` in macOS IOService,
although `system_profiler SPUSBDataType -json` returned no USB devices. The
IOService ancestry puts an `S60` block device under the same phone, and
`diskutil` reports a writable mounted volume with an existing `Installs`
directory. No serial is emitted in SDK output. USB product identification does
not establish RM code or firmware. No phone writes were made while the owner
copied photos. The new discovery/transfer/policy/Linux/low-space controls passed 8/8
with synthetic trees and temporary volumes. `~/dev/symbian-app-3` rebuilt
reproducibly via its selected SDK; the resulting unsigned SIS passed the native
reader and a real
SIS staging/hash check on a temporary host volume. The prior GUI example's
root build returned `DATA_LOSS` because independent CMake products differ;
this was not suppressed or used as a physical install candidate.
A separately exported SDK candidate at `~/dev/symbian-sdk-device-20261002`
completed installation with both target architectures. Its bundled CLI found
the connected phone and mounted S60 volume through read-only commands. The
current visible SDK remains in place until the device flow is finished. A
fresh portable `symbian init` project under `.symbian/device-install-smoke`
completed its initial ARMv6 build and native SIS package through the candidate
SDK. Its only E32 imports are EUSER, WS32 and GDI; no handset copy or install
was attempted while the owner transfers photos.

Gammu's [Symbian configuration documentation](https://docs.gammu.org/faq/config.html)
describes Bluetooth with an on-phone applet; its similarly named install
command installs that applet, not an arbitrary SDK SIS. The present USB mode
supports storage copy, not a verified remote installer. On-phone acceptance,
signing/import compatibility, installer state observation, and real Linux USB
device validation remain open. Future screenshot/debug adapters must report real
capabilities, while irreversible/recovery operations stay outside the agent
execution surface.

## 2026-10-02: Migrate both application paths onto the verified stackless profile

The original counter now uses the installed `Symbian::Stackless` target and a
separate modern C++ bridge. Its W32 source retains the owner's edits and the
same count behavior. A tap schedules a 300-ms timer Future; an event-mailbox
turn lights a marker, and Reset cancels it. One event thread checks Window
Server statuses and timer completions before its sole request-semaphore wait.
The bridge cancels and drains the pump before freeing callback state. The
source and published ELF/E32 bytes remained reproducible; root and standalone
CMake targets linked, and the expanded counter/marker/cancel/zero-exit control
passed on ARMv5T/ARMv6 × both RM-807 emulator backends (4/4) using the
background frontend. The root `gui_app_e32` publisher rebuilt with Apple
Clang and its ARMv6 image passed both backends after promotion.

`symbian init` now selects the Abseil Status/StatusOr and timer-Task profile by
default. If an explicitly selected firmware has no Z-drive `libpthread.dll`,
it emits the same source with portable CMake defaults; `--portable-runtime`
also selects that profile. Run checks the actual E32 import list against the
resolved firmware before launching. A modern executable aimed at E71 produced
`FAILED_PRECONDITION` with a rebuild instruction and no new emulator session.
The original nested CLI had obscured that status as `INTERNAL`; it now lets
the canonical `StatusError` cross the CLI boundary. Teardown explicitly
cancels and closes the TimerPump before destroying its mailbox/result state:
OnReady may execute inline during close. The final SDK candidate passed a
CLI initial-build/copied-SDK test and default-starter completion, cancellation
and rapid-exit on ARMv5T/ARMv6 × Dynarmic/Dyncom (4/4); the full
generated-project suite passed 17/17 before the two added ARMv5T cases passed.
A preceding candidate passed
real GUI starter execution on C7, E6, 6120 and E71 (4/4), selecting portable
defaults for the two dumps without `libpthread.dll`. The final 3,100-file
candidate and preceding visible SDK both verified against every recorded
digest. The candidate was copied and rebased into `~/dev/symbian-sdk`; the
preceding tree is retained at
`~/dev/symbian-sdk-before-gui-init-migration-20261002`. Its SDK selection
manifest was rebased and resealed so it remains selectable; the original
manifest and digest bytes are retained under
`.symbian/sdk-preservation-gui-init-20261002` for reconstruction. A new project also
completed its initial build after canonical SDK promotion. Neither profile exposes
A11 fibers, unresolved event-thread Await or genuine `thread::` primitives.
The canonical SDK's `gui_app` ELF/E32 publisher and GUI controls passed again
on ARMv5T/ARMv6 × Dynarmic/Dyncom after rebasing the installed target paths.
The wider `test_gui.py` selection returned 12 passed, five skipped and one
failure in the separate `e32_probe`: its ARM unwind index lacks the converter's
required Symbian exception descriptor. That test does not build either migrated
application; it remains an explicit converter/probe gate.

## 2026-10-02: First ARM/Thumb fiber context prerequisite

`cpp/symbian/concurrency/guest/arm_fiber_context.S` is an owned no-throw
backend adaptation, now compiled into each SDK runtime archive. It saves
ARM AAPCS `r4`–`r11` and return state, changes SP only within one OS thread,
and restores the next context. A bounded 16 KiB heap stack is constructed
with an 8-byte-aligned entry SP. The maintained C++ control keeps a
heap-backed `std::string` and `std::unique_ptr` live across the first switch,
resumes, destroys both normally and checks the original heap-cell count.
The independent changed-result control exits -302. Source-linked execution
passed 8/8 across ARMv5T/ARMv6 × Dyncom/Dynarmic; after moving the swap into
the installed runtime archive, the same matrix passed 8/8 against a sealed
SDK candidate. This is a context-switch proof, not an A11 fiber scheduler.
The sealed candidate was promoted to `~/dev/symbian-sdk`; ARMv6/Dynarmic
normal/changed controls passed again after promotion. The prior installation
is preserved at `~/dev/symbian-sdk-before-fiber-swap-20261002`.
There is no stack guard, floating-point context, native leave/TRAP scope
check, join/reaper or fiber-aware mutex yet. No real OS thread migration or
physical device operation was attempted.

## 2026-10-02: Property request, bounded event dispatch and Await guard

The original EUSER `RProperty` Define/Attach/Subscribe/Set/Get/Cancel/Delete
ordinals were added to the selected proxy. A narrow platform translation unit
owns an integer property, `TRequestStatus`, value storage and close-time
cancellation/drainage. The modern `PropertyWatch` returns `Future<int>` and
removes a completed entry before invoking its Promise callbacks. The guest
control subscribes, sets a value, registers the next subscription reentrantly
from `OnReady`, sets again, and cancels a third subscription from a worker.
The property and timer owners share one `WaitForAnyRequest` consumer.
The post-promotion timer/property matrix also passed 8/8 after adding a
close-while-subscription-pending control; that close drained native completion
and published a cancelled Future.

The same control drives an `EventMailbox` with one queued turn permitted:
worker enqueue, capacity rejection, event-thread affinity, reentrant enqueue,
bounded one-callback dispatch and close rejection. It deliberately does not
implement A11's pool `Post` or fiber `Submit`. ARMv5T/ARMv6 × Dyncom/Dynarmic
normal/changed tests passed 8/8 against a sealed installed SDK candidate;
the generated timer and Status GUI controls passed 5/5, then 6/6 including a
copied Abseil project after promotion to the 3,100-file visible SDK. The
preceding SDK, including four unsealed Finder files, is preserved at
`~/dev/symbian-sdk-before-status-concurrency-20261002`.

The absolute timer API now takes `absl::Time`, preserving `absl::Now()` as
wall time. Registration converts to a relative duration once; native timer
arms and elapsed waiting use the verified steady clock. A fake wall clock
jump after registration did not move an accepted 15-ms timer; 24-hour
admission/cancellation, infinite-future cancellation, negative-duration
rejection and native microsecond slice boundaries passed. A full 24-hour
expiry/rearm and an actual emulator clock adjustment remain untested.

A later candidate makes `Future::Await(absl::Time)` return ready values or
`FailedPrecondition` for unresolved waits until a fiber-aware Select backend
exists. Its installed stackless matrix passed 8/8; it was promoted to the
visible SDK, with the previous 3,100-file export preserved at
`~/dev/symbian-sdk-before-await-guard-20261002`. ARMv6/Dynarmic
normal/changed cases passed after promotion. A11's genuine
`thread::Mutex`, `CondVar`, `PermanentEvent`, `MutexLock` and `SleepFor` use
Boost fiber-aware behavior and cannot be represented honestly by the current
OS-thread-only mutex. The C3 backend remains open.

## 2026-10-02: Direct guest StatusOr concurrency and Abseil time

The installed guest Abseil profile now carries actual `absl::Status` and
`absl::StatusOr<T>` through the bounded A11-derived Future/Promise/Task,
JoinAll, TaskGroup and timer adapter. The temporary name-shortening aliases
were removed from the public headers and probes. The 8-case stackless and
8-case timer matrices each passed on ARMv5T/ARMv6 × Dyncom/Dynarmic against
`.symbian/abseil-direct-status-sdk-20261002`. A later maintained slice uses
`absl::Time` for public absolute deadlines and `absl::Duration` for relative
delays. It converts absolute time once at registration and retains monotonic
waiting internally, preserving the meaning of `absl::Now()`.

The first migration build failed because the probes still called the removed
`.error()` API. After correcting those call sites, E32 conversion rejected a
five-DLL import graph because the runtime test supplied only three proxies.
Its selected Abseil profile now declares the real `libm` and `libc` imports.
A subsequent timer link found an unresolved weak Cord constructor from the
payload shim; constructing from `absl::string_view` uses the original Cord
copying path and passed execution. These failures were retained and fixed at
their actual boundaries, not suppressed by the converter.

The Status-enabled generated project passed both emulator CPU backends and a
deliberate model error produced the native `KErrArgument` exit (3 tests).
The installed Abseil Status/StatusOr/Cord/map/time contract passed 9/9. The
root ARM IDE profile initially rejected the new Abseil probe directories;
explicit installed-Abseil object targets now configure and compile on both
ARM profiles. A cross-thread `RChunk` owner release on its creating process
heap and original pinned Abseil LowLevelAlloc normal/changed controls passed
on the preceding sealed candidate. General scheduler/fiber integration and
OS TLS are still unproven.

## 2026-10-02: Clean Abseil allocator replay and named runtime exits

A clean checkout at A11's pinned Abseil revision
`5650e9cf76d3be4318d5fa3af38ee483ddfd5e4a` accepted the ordered
`symbian-platform.patch` and `symbian-low-level-alloc.patch`; both reverse
checks passed on the replay checkout. The tracked allocator probe linked
against the fresh 2,597-file SDK export, converted to E32 with only EUSER,
OpenC and pthread imports, and ran on a disposable RM-807/Dynarmic instance.
The normal result was 0; a deliberately changed result was -286 (2/2 in the
maintained Pytest test). It exercises a 130,000-byte block, frees it, then
checks that arena deletion refuses an outstanding block and succeeds after
freeing it. The earlier scratch checkout had unmaintained log and thread
identity edits; this clean replay is evidence that neither is needed.

The first maintained Pytest run made the emulator frontend exit with host
SIGSEGV before a guest result; its temporary path was longer than macOS's
UNIX socket path limit. A short `/tmp` session path yielded both expected
guest results. This is a harness failure, not a ROM behavior. The runtime
exit bridge now accepts `SymbianRuntimeExitReason` instead of an unnamed
integer; the bridge statically checks its two process-exit categories against
`KErrNoMemory` and `KErrArgument`. Their numeric OS outcomes remain -4/-6.
Cross-thread page release and full Status/StatusOr closure are still unproven.
The installed SDK's selected exit/clock/lifecycle run passed 53 guest cases;
root CTest passed 8/8. Both 2,597-file promoted SDK and 2,596-file predecessor
verify by digest; the copied-project test passed before and after promotion.
The promoted SDK passed the allocator normal/changed controls again (2/2).

## 2026-10-02: Page sourcing, POSIX TLS and selected C/atomic closure

The pinned Abseil StatusOr link still lacks `LowLevelAlloc`, per-thread
semaphores, graph-cycle methods and numerous C services. Its page allocator
asks the OS for anonymous, page-aligned 64-KiB multiples and later unmaps
whole regions. OpenC's `mmap` does not establish anonymous-map behavior, so
enabling `ABSL_HAVE_MMAP` would hide a missing backend. The SDK now owns a
narrow original-header `RChunk` bridge that validates `UserHal` page size,
creates exact page-multiple process-owned chunks, checks alignment and
committed bounds, and returns an opaque owner for explicit close. The probe
checks invalid sizes, first/last bytes across 16 allocations and heap-cell
balance. ARMv6/Dynarmic normal and changed-result cases pass. Same-thread
close is the only verified ownership path; cross-thread release, exhaustion,
handle pressure and Abseil's map/unmap bookkeeping remain open.

The guest `pthread` key probe uses a real `std::thread` worker. It checks an
independent initial key value, sets/reads the worker value, joins, observes one
exit-destructor callback and keeps the parent's value. ARMv6/Dynarmic normal
and changed controls pass. This supports the POSIX key path, not ELF/C++ TLS.
The isolated Abseil relink had `__tls_get_addr` only in `GetCachedTID()`;
the replayable platform guard selects Abseil's existing `GetTID()` fallback,
and the relink no longer lists that symbol. The fallback currently calls
`pthread_self()` and needs performance measurement. The patch applies to the
clean A11-pinned source and reverses from the prepared scratch checkout.

The original EUSER `__e32_atomic_swp_ord8` ordinal now belongs to the selected
proxy and the guest checks old/new byte values twice. OpenC `strtol`,
`strcpy`, `strcmp` and `sysconf` have an execution control in the varargs
probe. The first run failed at link because its temporary proxy omitted
`strcmp`; adding the genuine ordinal made the ARMv6/Dynarmic normal/changed
tests pass. The Abseil relink still fails on missing allocation and
synchronization methods and further C services. Candidate SDK export and a
wider architecture/backend matrix are in progress.

## 2026-10-02: Original soft-double compiler-rt reaches ARMv5T and ARMv6

The isolated Abseil StatusOr guest link listed ARM double arithmetic,
conversion and comparison ABI entries. Adding LLVM's original ARM soft-double
assembly first failed at `movw`, which requires ARMv6T2 and is unavailable in
both SDK target profiles. The generic LLVM C implementation avoided `movw`
but introduced floating-environment services (`__fe_getround` and
`__fe_raise_inexact`) that this guest runtime has not verified. A maintained
third LLVM patch replaces four `movw` constant loads in the original ARM
assembly with literal loads. The next Abseil link exposed four single-precision
helpers; its original add/subtract source used ARMv6T2 `bfc`, so the same patch
expands that bit clear into two ARMv5-safe shifts. Algorithms and symbol
identities stay original.
The resulting link required LLVM's original `dnan2`, `dnorm2`, `dunder`,
`ashldi3` and `lshrdi3` helpers, now in the runtime closure.

A guest ABI control executes double add/subtract/multiply/divide, ordered and
NaN comparisons, signed/unsigned 64-bit conversions and float/double
conversions through actual compiler-generated calls. Normal and changed-result
controls passed 8/8 source builds on ARMv5T/ARMv6 × Dyncom/Dynarmic. This is
a compiler-rt/runtime result, not guest Abseil Status execution. The next
isolated StatusOr link remains a dependency gate; do not paper over allocator,
TLS or synchronization requirements.

Additional float subtraction and conversion controls passed an ARMv6/Dynarmic
normal/changed source pair after adding original `addsf3`, `fixsfsi`,
`fixunssfdi` and `floatundisf`, followed by 8/8 ARMv5T/ARMv6 ×
Dyncom/Dynarmic source controls and 8/8 installed-candidate controls. The
2,596-file candidate passed a copied-project test and root CTest 8/8, then
was promoted to `~/dev/symbian-sdk`; its verified predecessor remains at
`~/dev/symbian-sdk-before-softfloat-20261002`. Re-linking the pinned Abseil
StatusOr guest contract after these runtime changes removes all floating-point
compiler ABI undefined symbols. Even a reduced contract without
`Status::ToString()` has
the same unresolved allocator, TLS, synchronization, C/POSIX, wide-I/O and
exception-base link closure. Retained logs are
`.symbian/abseil-guest-probe/link-20261002-final.log` and
`link-minimal-20261002.log`. This is a diagnostic, not Status execution.

## 2026-10-02: Structured TaskGroup owns real timer completions

The A11-derived `TaskGroup` had only a producer-completed Promise control.
Its native integration probe now joins two short `TimerPump` Tasks, checks that
the joined Task is initially pending and succeeds after both native statuses
drain, then starts three longer timer Tasks and cancels the joined Task.
Cancellation traverses `Then` -> `JoinAll` -> child Task callbacks; only the
event thread calls native `RTimer::Cancel`, dispatches the completed statuses
and publishes the aggregate cancelled result. The block closes the pump and
returns to its initial heap-cell count. Initial ARMv6/Dynarmic normal and
changed-result source cases passed (2/2), followed by 8/8 ARMv5T/ARMv6 ×
Dyncom/Dynarmic source controls and 8/8 installed-SDK controls. This is actual
stackless structured composition over one native request type, not fibers or a
general native I/O adapter.

## 2026-10-02: Bounded admission for native timer Tasks

The first shared GUI loop made an unbounded `TimerPump` entry vector visible:
each admitted timer owns a native handle, Promise state and cancellation
callback until the event thread dispatches its completion. The pump now has a
configurable pending-request limit (64 by default). At capacity,
`ScheduleAfter`/`ScheduleAt` return an already-ready Task with an explicit
`kResourceExhausted` result before opening another native handle. The slot is
released only after the event thread drains the real completion, preserving
cancel/request ownership. An additional guest control admits two timers,
checks saturation, dispatches completion, admits a replacement, closes it and
checks heap-cell balance. Normal and changed-result controls passed 8/8 source
and 8/8 installed-candidate executions on ARMv5T/ARMv6 and Dynarmic/Dyncom.
The candidate contains 2,596 digest-verified files. This limits timer admission,
not all A11 continuation allocations or arbitrary native I/O queues. The
temporary guest Result code should map to Abseil `ResourceExhausted` when
guest Abseil Status is executable; public time types should likewise move to
Abseil. The candidate's copied-project and two-backend GUI controls passed
(3 tests), root CTest passed 8/8, and both candidate and previous canonical
trees verified before promotion. The visible SDK now selects the candidate;
both its seal and its predecessor's 2,596-file seal verify. The predecessor is
retained at `~/dev/symbian-sdk-before-timer-cap-20261002`. Post-promotion
ARMv6/Dynarmic normal and changed-result controls passed (2 tests), and the
canonical copied-project test passed.


## 2026-10-02: Generated GUI shares native wakeups with timer Tasks

The generated CMake project now offers `SYMBIAN_ENABLE_TIMER_TASKS` (off by
default because the selected ROM must provide `libpthread`). Its modern C++
bridge owns `TimerPump` and Tasks; the original-SDK GUI translation unit
continues to own the two Window Server statuses. The event thread dispatches
ready timer completions and WS events before parking on their one shared
request semaphore. The optional path logs on tap and on a delayed Task, and
Clear cancels pending work. This keeps native handles and their completion
storage inside the SDK bridge rather than in application model code.

The first link exposed the selected DRTAEABI proxy's missing
`__cxa_pure_virtual` ordinal. Adding its real ordinal, rather than a stub,
made the profile link. RM-807 Dynarmic and Dyncom GUI tests then verified
the immediate row, delayed row, cancellation after Clear and normal Exit.
The default GUI passed both backends; an E71 default generated starter built
and executed. Two E71 test attempts failed because relative SDK/importer
paths were resolved after the test changed working directory; absolute paths
passed. The verified 2,596-file candidate was promoted to the visible SDK,
with the previous sealed tree retained; both digest sets verify. Canonical
copied-project and two-backend opt-in GUI tests passed (3 tests), root CTest
passed 8/8, and Black/Ruff, native formatting and `git diff --check` passed.
The result does not establish full
native I/O registration, bounded pending-task admission, Abseil time, fibers
or arbitrary device support. Public `std::chrono` time types remain a
temporary guest-library boundary until executable guest Abseil time is ready.

## 2026-10-01: Timer requests complete A11-derived Tasks

Original `RThread::RequestSignal()` signals another thread's request
semaphore without changing a request status, provided the thread is in the
same process. The event-thread bridge opens a process-owned handle to its own
thread ID and keeps it alive while worker cancellation callbacks may signal.
A11's `Future::Cancel()` remains a request: the worker only marks an atomic
32-bit cancellation flag and coalesces a wake. The event OS thread alone calls
`RTimer::Cancel()`, observes its final status and publishes its Task result.
The wake handle is closed under the existing A11-derived mutex adapter, so a
concurrent callback cannot signal a closed handle. Ready entries are removed
before inline `OnReady` callbacks run; those callbacks can schedule another
timer without invalidating a dispatch iteration. The pump shares the current
thread's native request semaphore through an explicit `Park()` operation; it
does not start another waiter or a timer worker.

The first source link exposed an absent 8-bit `__atomic_store_1` helper, so
the cancellation flag uses the already verified 32-bit atomic path. Normal
and changed-result controls passed eight source cases before adding a
continuation check. The formatted final source passed an ARMv6/Dynarmic
smoke test, and the installed SDK candidate passed eight ARMv5T/ARMv6 ×
Dyncom/Dynarmic cases. The probe includes a worker's cancellation while the
event thread is parked behind a two-second timer and requires it to wake
within 500 ms; it also covers an inline continuation that schedules another
timer, multiple ready timers, close while pending and allocation balance.
The caller still owns Window Server status inspection, and that combined UI
loop needs actual execution tests before C1/C2 acceptance is complete.
The formatted candidate's copied/relocated project test passed, then its
2,596 digest-valid files were promoted to `~/dev/symbian-sdk`; the prior
2,595-file SDK is preserved at
`~/dev/symbian-sdk-before-timer-future-20261001`. The canonical path passed a
further ARMv6/Dynarmic timer-Future smoke and copied-project test. Root CTest
passed 8/8, and both ARM IDE index targets compiled the new probe.

## 2026-10-01: Owned native timer request slice

Original `RTimer::CreateLocal()` creates a thread-relative handle. Its
`HighRes()` writes `KRequestPending` before submission, panics on negative
intervals or an overlapping request, and completes through the owning
thread's request semaphore. `Cancel()` completes an outstanding status with
`KErrCancel`. `User::WaitForAnyRequest()` consumes that same semaphore for
all native requests, including Window Server events. These contracts rule out
a timer-specific background waiter or a separate competing request pump.

The SDK now keeps `RTimer` and its status in one allocated bridge state, checks
arm preconditions, and cancels/waits for completion before freeing a pending
state. The modern `NativeTimer` owner makes the C bridge private to the SDK
edge. A guest probe starts three overlapping timers, cancels one, closes one
while pending, waits for the surviving completion, rearms it, and verifies
heap-cell balance. It also rejects negative and overlapping arms and has a
changed-result control. Eight source and eight installed-SDK ARMv5T/ARMv6 ×
Dyncom/Dynarmic cases passed; the formatted final SDK export passed a second
eight-case installed matrix. Its copied/relocated project test, canonical
ARMv6/Dynarmic smoke test and root CTest 8/8 passed; both ARM IDE index
targets built the new probe. The canonical SDK has 2,595 digest-valid files;
its 2,594-file predecessor is preserved at
`~/dev/symbian-sdk-before-native-timer-20261001`. The bridge has no private
event loop. Before C1 is complete, one event-thread pump must own all status
registration/dispatch, Window Server events, idle parking and cross-thread
wakeups; close/cancel races and stale registrations need targeted controls.

## 2026-10-01: Concurrent guest clock and libc++ thread closure

The first `steady_clock` probe established single-thread progress but not
cross-thread ordering. A new real `std::thread` probe starts both readers
together, checks 2,048 samples per thread, then performs 128 release/acquire
handoffs and rejects a sample earlier than the other thread's published value.
It also checks Symbian heap-cell balance after join. Normal and deliberately
changed-result controls passed 16 ARMv5T/ARMv6 × Dyncom/Dynarmic cases from
source and 16 against a freshly exported installed SDK
(`.symbian/clock-thread-source-matrix.log`,
`.symbian/clock-thread-installed-matrix.log`). This is a bounded concurrent
reader check, not proof of timing resolution, suspension or half-wrap behavior.

Combining the clock and `std::thread` initially failed at link: an older
SDK-owned `__throw_system_error` fallback collided with original pinned
libc++ `system_error.cpp`. The fallback was retired from the archive and its
obsolete file removed; original libc++ now owns the no-exceptions thread
failure path. A second link failure exposed the real `std::this_thread::yield`
dependency on `sched_yield`, which is ordinal 296 in the preserved OpenC
`libcu.def`. The selected libc proxy now includes that entry. The concurrent
guest probe executes the real ROM import on RM-807. Eight source
`system_error` controls passed after the change
(`.symbian/clock-thread-system-error-source.log`). Joining an empty
`std::thread` deliberately exits the guest with -6 through original libc++'s
no-exceptions abort path; four source and four installed negative controls
passed (`.symbian/thread-error-source.log`,
`.symbian/thread-error-installed.log`). No recoverable `StatusOr` is inferred.

The new export has 2,594 digest-valid files. Its copied/relocated SDK project
test passed (`.symbian/clock-thread-project-copy.log`), and an E71 generated
starter built and executed on its named firmware
(`.symbian/clock-thread-e71.log`). The prior visible SDK had no altered or
unsealed files. The candidate was installed at `~/dev/symbian-sdk` through the
SDK's path-rebasing installer; its previous 2,594-file version remains at
`~/dev/symbian-sdk-before-clock-thread-20261001`, and both seals verify.
The canonical path's copied/relocated project test and one ARMv6/Dynarmic
clock-thread smoke test also passed after promotion
(`.symbian/clock-thread-canonical-copy.log`,
`.symbian/clock-thread-canonical-smoke.log`).
Native `RTimer` request ownership, cancellation/draining and
monotonic deadline scheduling remain C1 work; this test adds no scheduler or
competing request-semaphore consumer.

## 2026-10-01: Guest clocks and FastCounter rate

The pinned libc++ `chrono.cpp` could not use its POSIX monotonic branch on the
named RM-807 ROM: `clock_gettime(CLOCK_MONOTONIC)` returned `EINVAL`, including
after a successful `CLOCK_REALTIME` call. A maintained LLVM patch now routes
only Symbian `steady_clock` through a narrow runtime bridge. It reads
`User::NTickCount()` and the nanokernel HAL period, falling back to the
ordinary tick and its HAL period. A process-wide 64-bit atomic extends the
32-bit count and clamps out-of-order cross-thread samples. The adapter assumes
at least one sample within half of a counter wrap; long idle/suspend behavior
and concurrent clock readers remain separate gates. `system_clock` retains the
working original libc++ realtime path.

Original `euser/us_exec.cpp` explicitly recommends `FastCounter` for profiling
and testing, and `NTickCount` for production. It warns that fast-counter
frequency and activation vary by device and that it can consume extra power.
The screenshot's distinction from ordinary `TickCount` is useful for short
benchmarks, but does not establish a portable production deadline source.
EKA2L1 already generated NTick and FastCounter values without exposing their
period/frequency via the corresponding kernel HAL IDs. Ordered emulator
patches add those HAL responses and fix FastCounter's truncated integer period
calculation (the old 30 µs period implied about 33,333 Hz while the advertised
frequency was 32,768 Hz). A guest one-second interval probe checks the count
against the monotonic nanokernel clock. The original emulator build succeeded.

Source guest normal/changed controls passed 16 ARMv5T/ARMv6 ×
Dyncom/Dynarmic cases (`.symbian/clock-fast-source-matrix.log`). An isolated
SDK export first failed in the Streams profile because OpenC's `libm` alias
macros rewrote libc++ overload names; a C++-only header adapter now suppresses
that alias layer while retaining the original C declarations. The exported
Streams build then succeeded. A generated project exposed another issue:
its proxy list was also the direct linker input list, and CMake de-duplicated
two `--as-needed` scopes so an unused `libc.dso` became mandatory. The template
now uses one optional runtime scope, while import-image targets place
`--as-needed` before the complete declared proxy set. The converter itself
also assumed every declared proxy would appear in ELF `DT_NEEDED` and
`.gnu.version_r`; it now checks the actual needed subset against the declared
proxy catalog while retaining duplicate, identity, and unreferenced-import
checks. This keeps selected proxies available for conversion without making
every one a firmware dependency. The first exported clock and the corrected
FastCounter matrices each passed eight cases. After rebuilding the editable
native extension and re-exporting, the final installed matrix passed 16 cases
(`.symbian/clock-fast-installed-final-matrix.log`). A copied/relocated SDK
project and an E71 generated starter both built, and the latter executed on
its named firmware (`.symbian/clock-sdk-project-copy6.log`,
`.symbian/clock-e71-starter6.log`). A dedicated unused-proxy conversion control
and the full import suite passed. Root CTest passed 8/8. The 2,594-file
digest-valid candidate was promoted by path rebasing to `~/dev/symbian-sdk`;
its prior 2,591-file tree remains separately preserved and verifies.

## 2026-10-01: Root IDE indexing for guest probes

The host root CMake profile did not own `probes/runtime_probe/chunk_bridge.cc`;
CLion reported that the source belonged to no target and could not derive
platform compiler information. The root CMake project now has a separate ARM
analysis branch selected by `SYMBIAN_INDEX_GUEST_PROBES`, reached before host
FetchContent or native targets. It configures the original guest runtime
component and a compile-only `symbian_probe_index` aggregate with separate
object targets for each probe project. It covers ABI, C++20, C++20 modules,
DLL/data/lifecycle, E32, import, pointer and runtime probes. The local presets
reject any new `examples/*_probe` directory until its indexing target is
declared, so future probes cannot silently lose IDE ownership. They
also select a prepared Mbed TLS source and visible SDK headers for its C probe;
checkouts without it omit that independently supplied adaptation. A clean
configure with Apple's `/usr/bin/clang++` revealed no `clang-scan-deps`; the
profile now fails at configure with an explicit upstream-Clang requirement
instead of an opaque Ninja command-not-found failure. No ARM ELF Run
target is exposed to the host IDE. Special locale and exception
translation-unit settings are attached to the runtime target, and the C++20
module target uses CMake's module scanner. ARMv6 and ARMv5T local preset builds
succeeded; an audit of both compilation databases found all 48 probe
`.cc`/`.c`/`.cppm`/`.S` files using the
matching `--target=armv6-none-eabi` or `--target=armv5t-none-eabi` compiler
option. The ignored local CLion preset fixes LLVM/LLD/Ninja paths for this Mac;
the existing host profile remains enabled. The prior `.idea/workspace.xml`
was retained under `.symbian/clion-setup/backups/`. The live IDEA/CLion log
then recorded CMake exit 0 for the local ARMv6 guest profile at 22:04:38
after the expanded target graph was added.
Editor diagnostics after that reload have not been observed directly.

## 2026-10-01: Maintained classic-locale profile and Abseil link gate

The original LLVM libc++ 23.1.2 locale, ios, ostream, iostream and strstream
sources are now built in the opt-in `SYMBIAN_RUNTIME_LOCALE_STREAM` profile.
Owned C/POSIX locale, `isblank`, `std::uncaught_exceptions()` and ARM
`__aeabi_memmove4` adapters live in `cpp/symbian/runtime/`; the pinned LLVM
checkout is unchanged. The locale adapter rejects invalid names, masks and
categories with `EINVAL`, and deliberately does not claim arbitrary locales.
The guest checked real `std::ostringstream` output and `std::locale("C")` /
`std::locale("POSIX")`, including a changed-output control. Eight source-tree
and eight fresh installed-SDK ARMv5T/ARMv6 × Dyncom/Dynarmic cases passed.
The installed `Symbian::Streams` CMake target selects a separately built
archive and matching `__config_site`; it must replace, not accompany,
`Symbian::Runtime`. Two independent archive builds per architecture compared
byte-for-byte. Both the prior and new SDK trees passed complete digest audits
before promotion. The current tree has 2,535 sealed files; the 2,503-file
predecessor is retained at `~/dev/symbian-sdk-before-streams-20261001`.

An ignored pinned-Abseil `absl_statusor` cross-build now completes after
removing Symbian from Abseil's glibc-style in-memory ELF symbol path and
keeping Fuchsia-only zoneinfo file reading inside its platform guard. These
two adaptations are recorded as a replayable, checkable patch at
`research/abseil/symbian-platform.patch`; they are not shipped yet. Enabling
libc++ wide declarations let Abseil's formatting sources
compile, but does not establish working wide-character I/O. A guest Status/
StatusOr link then failed on a concrete dependency closure: Abseil's
`LowLevelAlloc` compiles empty when the platform lacks `ABSL_HAVE_MMAP`, while
its synchronization graph still refers to that allocator; compiler-rt soft
float helpers, 64-bit atomics, TLS, C/POSIX services and other original runtime
symbols are also missing. The full undefined-symbol record is in
`.symbian/abseil-guest-probe/link-locale-3d.log`. Do not supply a fake mmap
or no-op lock: OpenC declares file-backed `mmap` but its `sys/mman.h` does
not define `MAP_ANON`/`MAP_ANONYMOUS`, which Abseil's allocator requires.
Validate an actual anonymous page source, likely through the native `RChunk`
contract, and its synchronization path, then
execute Status/StatusOr before making A11's public guest API depend on them.

As a first page-source prerequisite, a narrow original-SDK `RChunk` bridge
queries `UserHal::PageSizeInBytes`, then creates, writes, enlarges and closes
16 process-owned local chunks, checking
the original bytes after growth. Normal and changed-result controls passed
eight ARMv5T/ARMv6 × Dyncom/Dynarmic executions from source and another eight
against the canonical SDK archive with a separately generated ordinal proxy.
This validates
that bounded native chunk path, not the page-size, commit/decommit,
cross-thread, asynchronous or low-level allocator contracts Abseil requires.

## 2026-10-01: Imported function pointers and locale/stream prerequisite

**Converter result:** A global pointer initialized to imported EUSER
`memmove` generated both a function PLT slot and `R_ARM_ABS32` in `.rel.dyn`.
The E32 converter now accepts only a bounded relocation to an imported
function with a unique, validated ARM PLT entry, a zero-initialized word in
file-backed writable data, and a matching retained `.rel.data` record. It
writes that PLT address into the E32 data image and emits a normal E32 data
relocation. An imported function address held in code is also accepted only
when its symbol value names the correct PLT entry. Imported data objects,
other `.rel.dyn` types, malformed PLT values and missing retained records
still fail conversion. The maintained global-pointer probe passed all four
ARMv5T/ARMv6 × Dyncom/Dynarmic executions from source, plus four using the
existing installed SDK runtime/proxy. Two mutated ELF controls reject a
`R_ARM_GLOB_DAT` data-object form and a false imported-symbol PLT address;
they passed in both source and installed-SDK test settings. A fresh 2,503-file
SDK export passed the combined 11 function-pointer/thunk execution and
malformed-input cases, then replaced the canonical visible SDK after both
trees passed their digest audits. The prior tree is retained at
`~/dev/symbian-sdk-before-import-pointers-20261001`; both 2,503-file trees
remain digest-valid and their manifests point to their actual locations.

**Locale experiment:** In an ignored isolated build, the original pinned
LLVM 23 libc++ locale/ios/ostream/iostream/strstream sources compiled with
an SDK-owned experimental classic-C-locale API adapter, a small `isblank`
bridge and libc++'s default rune table. After adding original source closure,
real C imports and a no-exceptions `std::uncaught_exceptions()` hook, a guest
`std::ostringstream` produced `"value 42"` and `std::locale("C")` named
`"C"`. That image exited 0 on both emulator CPU backends. This is a
prototype, not an installed runtime profile: its locale handling, imported
C-service closure, non-classic locale failures, exception-policy boundary and
cross-architecture tests still need promotion and negative controls before
Abseil Status can depend on it.

**Failed control:** The first stream image exited with a guest access
violation on both backends. Disassembly identified LLD's generated
`__ARMv5LongLdrPcThunk___aeabi_memmove`: its literal held the unrelocated
ELF address `0x9365` and no retained relocation. A Thumb-to-Thumb helper
removed that accidental interworking thunk and the stream probe passed.
The converter now recognizes LLD's exact named eight-byte ARM
`ldr pc, [pc, #-4]` thunk, validates its target in the code mapping and
emits an E32 code relocation for its literal. Rebuilding the original ARM
helper then made the same stream probe exit 0 on both CPU backends. A
maintained ARM-to-Thumb thunk probe passed all four architecture/backend
cases; a changed literal outside code was rejected before launch. Other
linker-generated absolute thunk forms remain a gate. A successful static
conversion alone had not proved the first image runnable. Experiment files
and logs remain under ignored `.symbian/libcxx-locale-probe/`.

**Next Abseil closure probe:** The isolated pinned Abseil
`5650e9cf76d3be4318d5fa3af38ee483ddfd5e4a` build was reconfigured
against the executing classic-locale/stream prototype. It progressed to
49/142 build steps before failing. OpenC's `libm_aliases.h` first rewrote
`nexttoward` to `nextafter`, conflicting with libc++ overloads; an ignored
test-only math wrapper after libc++ and before OpenC removed that macro for
the build. Remaining failures include Abseil debugging code expecting the
Linux `link.h` ELF loader interface, `str_format` requiring disabled
`std::wstring`/`wstring_view`, and time-zone code instantiating an unavailable
`std::ifstream`. These are real platform and library-profile dependencies.
No source patch or guest Abseil archive was promoted; the exact compiler log
is `.symbian/abseil-guest-probe/build-locale-2.log`. The next adaptation must
preserve actual Abseil behavior where used, resolve wide-character and file
services or narrow the dependency graph with justified source changes, then
execute Status/StatusOr on the guest before A11 can consume it.

## 2026-10-01: Guest varargs and libc++ error categories

**Question:** Can the guest use Clang's ARM EABI varargs and original libc++
error categories through real firmware C imports?

**Implementation:** An SDK-owned `stdarg_e.h` shadows OpenC's legacy header
at the guest compiler include boundary. It preserves Clang's builtin
`va_start`/`va_arg` and defines OpenC's reserved `__e32_va_list` name as the
builtin ARM `va_list` type. The installed SDK carries the identical header.
The runtime archive now builds original pinned LLVM libc++
`error_category.cpp` and `system_error.cpp`. Those files use local static
category storage and compile without PIC so the verified E32 data relocation
pass can rebase it. Their first PIC build was correctly rejected for a
PC-relative reference crossing code and data mappings; the failure was not
suppressed in the converter.

**Execution:** A guest calls the ROM's `libc.dll` `vsnprintf` with an integer,
string, then enough arguments to use the stack and a 64-bit integer. A second
guest exercises `std::make_error_code`, `generic_category`, `message` through
`strerror_r`, and default error condition. Each has a changed-result control.
Source-tree tests passed all 16 ARMv5T/ARMv6 × Dyncom/Dynarmic cases, and a
fresh materialized SDK passed the same 16 cases with its prebuilt archive and
headers. A copied-SDK application build passed. The actual C imports came
from selected frozen ordinals; no format stub was used. The prior visible SDK
audited clean before replacement. The new 2,503-file visible SDK and retained
2,502-file predecessor both verify by digest; the predecessor is
`~/dev/symbian-sdk-before-runtime-c-services-20261001`.
The promoted canonical SDK passed four ARMv6/Dyncom normal/changed-result
controls; root CTest passed 8/8, and formatting/lint/diff checks passed.
An isolated pinned-Abseil cross-build with LLVM 23.1.2 now compiles its
`absl_raw_logging_internal` target against the maintained SDK varargs header.
This is a compile result in an ignored adapted source copy, not a guest
Abseil Status build or execution result.

**Open:** This is a bounded C ABI and C++ system-error result. Abseil's pinned
Status closure still requires real iostream/locale support; A11's fiber,
request-owner and cancellation backend is not built. The scratch Abseil build
is not a passing guest library or a substitute for an execution test.

## 2026-10-01: Guest completion namespace and Abseil cross-build

The bounded stackless adaptation was initially exported under
`<a11/concurrency/*.h>` as `a11::guest`. This put an incompatible
`Result<T>` API on the same include path as A11's `absl::StatusOr<T>`
API. The owned adaptation now uses `<symbian/concurrency/*.h>` and
`symbian::concurrency`; the unmodified A11 snapshot retains the
`a11::` namespace. An SDK export must merge these headers into
`include/symbian` beside `runtime.h`, and must not publish the old
`include/a11` alias. The SDK-owned cross-thread mutex adapter likewise
moved from `thread::Mutex` in `<thread/boost_primitives.h>` to
`symbian::concurrency::Mutex` in `<symbian/concurrency/mutex.h>`; A11's
`thread::` namespace remains available for the eventual compatible backend.

An isolated copied Abseil source tree exposed further concrete guest
dependencies. The first exploratory build selected `/usr/bin/clang++`.
A second configured build used the pinned LLVM 23.1.2 Clang and LLVM archive
tools; after adding the real OpenC libm header directory, it still failed
in Abseil's `OStringStream` and `int128` stream formatting because this guest
libc++ profile has localization and iostreams disabled. OpenC's
`stdarg_e.h` replaces Clang's builtin `va_start`
macros with pointer-based macros even though Clang's ARM `va_list` is
a struct. Skipping that header in the experiment clears the immediate
compile error but does not establish variadic-call ABI safety. The
current libc++ build disables localization, so Abseil's stream-based
source does not have a complete `std::ostream` or stream buffer.
Enabling localization instead reaches absent OpenC `locale_t` and
locale functions. These scratch changes were not promoted to a
maintained Abseil patch or claimed as an executable Status closure.

**Evidence:** After moving both the completion headers and mutex shim, the
source-tree runtime probe passed all eight normal/changed-result cases across
ARMv5T/ARMv6 and Dyncom/Dynarmic (50.27 s). A new materialized SDK with no
`include/a11` or `include/thread` passed the same eight cases (53.84 s); a
copied-SDK initial application build and relocation test passed. The prior
visible SDK audited clean before replacement. The refreshed visible SDK and
retained predecessor each have 2,502 digest-valid files. After promotion,
the canonical SDK passed two ARMv6/Dyncom normal/changed-result controls;
root CTest passed 8/8 and the A11 source integrity checks 5/5. This establishes
namespace ownership for the bounded profile, not the full A11 ABI.

## 2026-10-01: A11 stackless guest completion and Abseil dependency gate

**Question:** Can the pinned A11 completion path run on the bounded guest
without a fiber scheduler or another native request consumer?

**Experiment:** A direct CMake cross-build of pinned Abseil
`5650e9cf76d3be4318d5fa3af38ee483ddfd5e4a` reached the Status closure
but failed on unavailable libc++ streams/C++ ABI headers, Symbian signal APIs,
platform `O_CLOEXEC` and an assumed always-lock-free 64-bit atomic. The
experiment's build files and patched source copy remain ignored; the pinned
upstream checkout was not edited. The
full A11 `Future` header also requires its Boost fiber/select backend.

An explicit licensed guest adaptation of the A11 pinned `future.h`,
`parallel.h`, `inline_pump.h` and `thread/boost_primitives.h` keeps shared completion, inline
callback/continuation semantics, cancellation request, promise abandonment
ordered all-result fan-in, bounded reentrant pump passes and an explicit
TaskGroup asynchronous join/cancellation. The bounded
`symbian::concurrency::Mutex` uses the verified
libc++ OS mutex; it cannot park fibers. This exposed missing libc++
`mutex_destructor.cpp` and ARM `__aeabi_memclr8` calls; the runtime now builds
the original destructor and supplies the ARM EABI aligned-clear adapter over
the existing real `memset` import.

**Evidence:** Eight normal/changed-result execution cases passed on the
preserved RM-807 fixture across ARMv5T/ARMv6 and Dyncom/Dynarmic (47.09 s
after the `thread::Mutex` adaptation). A fresh materialized SDK exported the
headers and `Symbian::Stackless`; the same eight cases passed with its
prebuilt runtime archives and selected proxies (49.14 s). That export was
promoted to `~/dev/symbian-sdk` only after the previous canonical tree's
2,496-file digest audit; the new 2,502-file tree and its retained predecessor
both verify. The refreshed SDK passed 15 generated-project/source-integrity
tests, including actual breakpoints in the bridge and model. The final
TaskGroup cancellation routing adjustment passed two ARMv6/Dyncom source
controls and two canonical-SDK controls after the full eight-case matrix.
Tests cover
cross-thread completion, immediate callbacks, cancellation before completion,
duplicate completion, abandonment, allocation-cell balance, reverse-order
fan-in, error retention, cancellation fan-out, empty fan-in, TaskGroup double
finish/rejection, group cancellation and abandonment. Neither these
tests nor the export prove general device compatibility, C1 native request
ownership, fiber parking or the final Abseil Status ABI.

## 2026-10-01: writable DLLs, C toolchain and Mbed TLS link

**Question:** Can an installed SDK publish a usable C DLL, preserve its
per-process data contract, and link actual Mbed TLS code without claiming a
general DLL runtime?

**Evidence:** The original `elf2e32` treats `EPOCALLOWDLLDATA` as a converter
opt-in. The pinned EKA2L1 loader allocates per-process DLL storage and copies
initialized data, clears BSS and applies data relocations. LLVM's ARM PIC mode
uses GOT entries for default-visible mutable globals but emits cross-mapping
PC-relative references for hidden/internal globals; the latter are rejected.

**Experiments:** A frozen-export C++ probe with 4-byte initialized data and
4-byte BSS converts, passes independent original E32 validation and executes
twice in each of two fresh processes on both CPU backends. A changed input
fails across the DLL import boundary. Dyncom initially fetched an unmapped
instruction after guest process exit; an ordered source patch stops its
interpreter when the syscall ends the current execution quantum. Both-backend
cases then pass. A C-only CMake target builds DLL and ordinal proxy for ARMv5T
and ARMv6, links a C consumer and converts its function import. The Mbed TLS
adaptation builds all 80 `mbedcrypto` C objects with SDK Clang 23.1.2. A
SHA-256 wrapper links that archive and the SDK runtime/EUSER proxy, then
converts to a DLL. Its import metadata contains seven selected EUSER slots.
Full Mbed TLS guest execution is not yet demonstrated. The SDK export had
omitted the proxy header-probe resource; materialization now includes it.
Copying an installed SDK now rebases the new C compiler path as well as the
existing tools; a copied 2,422-file SDK manifest loaded successfully, and the
active user SDK was restored to `~/dev/symbian-sdk` after that check.

**Focus regression:** Direct GUI/runtime/debug test launchers lacked the
background-window environment flag even though they used a nonbundle launch
path. All three now set the flag. No EKA2L1 process was running when the user
reported a focus steal during this turn, so its exact triggering process is
unconfirmed; the missing flag was a concrete reproducible launch-path defect.

**Remaining:** General hidden/internal writable data, import-heavy DLL
lifetime, constructors/destructors, TLS and useful dynamic-module debugging.
Test the Mbed TLS DLL and client with actual firmware EUSER imports before
claiming guest execution. Continue the A11 concurrency prerequisite work.

## 2026-09-30: initial survey and engineering baseline

**Question:** What already exists, and can the host supply a native starting
point without reconstructing Nokia's build environment?

**Historical behavior:** GCCE/RVCT produced ARM ELF input for elf2e32; Symbian
loaded E32 images and resolved DLL contracts using target-specific metadata.
The historical build host did not define the phone's loader format.

**Evidence:** The initial directory contained only PLAN.md. Sources and immutable
revision links are listed in RESEARCH.md. The owner reports one Nokia 808, no
preserved assets and no second phone. Exact RM/product code and firmware remain
unknown. A11's native libraries disable exceptions and use Status/StatusOr;
its pybind11 boundary releases the GIL for blocking/native work and owns Python
references through dedicated holders when callbacks cross threads.

**Experiments:** Built the initial native library and Python extension with
the A11 Abseil revision `5650e9cf76d3be4318d5fa3af38ee483ddfd5e4a`. Built
GTest against the host's static GTest installation. The installed extension's
`otool -L` lists only CoreFoundation, libc++ and libSystem.
The default CMake fetch/build path also succeeded in a fresh tree and fetched
the exact pinned revision without using A11's local source cache. Verification
passed 41 Pytest cases and eight GTest cases, Black/Ruff, clang-format, and the
native typing declaration regeneration check. Compile commands confirm the
native core and native tests use `-fno-exceptions`.

**Conclusion:** The first slice needs only a synchronous native parser and Python
host orchestration, not A11's full runtime or a second concurrency library.

**Implementation decision:** Core format logic lives in cpp/symbian/analysis;
pybind11 is the exception-enabled boundary; errors retain canonical status
codes in Python and JSON. No callbacks, background Python references, native
scheduler, device executor or compatibility VM is introduced.

**Remaining uncertainty:** Target/runtime assets and a validated recovery path
are unavailable. Native object generation does not complete PLAN milestone 1.

## 2026-09-30: Apple Clang ARM code generation

**Question:** Can the installed native compiler produce usable ARM ELF input?

**Historical behavior:** GCCE used the `arm-none-symbianelf` target spelling and
target-specific libraries/startup before conversion to E32.

**Evidence:** Clang documents explicit cross-target selection. f32image.h defines
the downstream E32 loader contract independently of the compiler's target name.

**Experiments:** Apple Clang 21 rejected `--target=arm-none-symbianelf` with an
invalid-version diagnostic. It compiled the SDK-free integer probe with
`--target=armv5t-none-eabi -mthumb -mabi=aapcs -mfloat-abi=soft
-ffreestanding -fno-exceptions -fno-rtti -nostdinc`. The maintained CLI adds
`-O2 -std=c++20`. Native inspection reads ELF32 little-endian, type 1, machine
40, EABI version 5 and ten sections. Two builds with independent output
directories have identical bytes. The optimized example object SHA-256 is
`ce5601f39eca6a481489e83f87f0df62d2d44a51ecc285fc8523ffe7acc67c7b` on this
host/compiler. `clangd --check` using the emitted target compilation database
completed with zero errors. The host has no `ld.lld` in PATH or Xcode tools.

**Conclusion:** Native ARM object generation and target-aware source intelligence
work without GCCE. The old compiler triple cannot be reused verbatim.

**Implementation decision:** `symbian toolchain probe` and the experimental
`symbian build` report object reproducibility explicitly and always label
Symbian loader verification false. Flags are a conservative experimental
profile, not a verified specification for all 808 application code.

**Remaining uncertainty:** ARM ELF linking, ordinal DSOs, E32 conversion,
startup, target compiler builtins, C++ ABI/leave semantics and runtime acceptance.

## 2026-09-30: EKA2L1 automation contracts

**Question:** Which existing emulator capabilities should the facade reuse?

**Historical behavior:** The desktop frontend provides launch/install command
options but retains GUI initialization and host global state conventions.

**Evidence:** Pinned thread.cpp/cmdhandler.cpp, config/options.inl,
qt/src/main.cpp, scripting sources and gdbstub.cpp listed in RESEARCH.md.
The GUI requires Qt Widgets/LinguistTools/Svg/Network/OpenGL. Existing loader
tests share the emulator's broad native build/dependency graph.

**Experiments:** Source inspection only. No emulator executable or ROM/Z image
was run. The macOS main changes cwd to Qt's global generic-data location before
loading config; `--install` selects drive E; `--help` follows a failure-return
path. The GDB listener uses INADDR_ANY. `--listapp` is unfinished in this revision.

**Conclusion:** CLI flags alone do not provide an isolated automated test device.
A launcher's cwd and storage copy would leave shared configuration and assets.

**Implementation decision:** Before runtime orchestration, add/test a native
instance-root override before any writes and configuration loading. Reuse the
GDB stub and kernel hooks, with loopback binding and structured completion
results. Do not publish unverified snapshot/headless/screenshot CLI commands.

**Remaining uncertainty:** Native full build, actual Belle/FP2 runtime support,
Qt/application compatibility, debugger protocol interoperability and process exit
automation need measured tests with matching artifacts.

## 2026-09-30: reference archive semantics

**Question:** What can the host preserve before device access is established?

**Historical behavior:** Firmware inventories and device dumps are independent
of ordinary application builds and need variant/version/provenance records.

**Evidence:** PLAN.md requires preservation and a recovery path, and keeps
irreversible operations outside automated authority. No original artifacts exist
in the provided project.

**Experiments:** Tests use synthetic files only. Archive creation copies bytes,
hashes source and destination, detects input changes, records a deterministic
manifest, rejects links/special files, and removes write permission. Verification
detects missing/extra/modified files and unsafe manifest paths. A separately
retained manifest digest detects manifest substitution. No phone was modified.

**Conclusion:** The tools can preserve existing material but cannot establish a
known-good device/firmware baseline without original inputs. Owner-reversible
POSIX permissions are not immutable storage.

**Implementation decision:** `symbian preserve create/verify` and the private
inventory template support preservation; an offline reference and separately
trusted digest remain required. Policy classification has no execution API.

**Remaining uncertainty:** Actual device identity, original firmware acquisition,
ROM/Z dumps, backup method, and independently validated human recovery appliance.

## 2026-09-30: linked ELF and restricted E32 executable

**Question:** Can modern macOS tools produce an actual E32 container without a
historical compiler, SDK runtime or Windows tooling?

**Historical behavior:** EKA2 ARM startup begins with a version marker and
reserves a loader-owned word at entry+12. E32 V headers include alternating-byte
UID CRC16 and a CRC32 initialized with zero, without a final complement; the
header CRC field contains 0xc90fdaa2 during CRC calculation. These contracts are
visible in f32image.h, uc_exe.cia and the historical elf2e32 checksum source.

**Experiments:** Installed native LLD 23.1.2. Apple Clang 21 compiles ARM startup
and Thumb C++ using ARMv5T/AAPCS soft-float. LLD links ET_EXEC with retained
R_ARM_CALL and R_ARM_THM_CALL relocations. Disassembly confirms ARM-to-Thumb BLX,
a real C++ calculation/call/stack sequence, and SVC 0x73 with the ThreadKill
register contract. A volatile input and noinline probe prevent the calculation
from being optimized into a constant success result.

The converter in cpp/symbian/e32 emits an uncompressed 156-byte V header, UID3
and SID 0xe0000808, no capabilities/imports/exports/data, and 116 bytes of code.
It checks bounded segments/sections/symbols/relocations and the EKA2 entry.
Two independent output trees produce identical linked ELF and E32 bytes:

- ELF SHA-256: `4031252adc395d05bb7f3477262b4c00c0018aee1bc7eba40daa35feaa798e4e`
- E32 SHA-256: `997cd9c5ec35281f261a08cb3c2ca6a36c74be969b4a72cbbd8ede5ff5332395`

The independent EKA2L1 parser accepts the complete E32 and expected metadata,
rejects UID checksum damage and truncated code, and accepts a bad header CRC.
That last case deliberately documents a weaker oracle. A separate GTest binary
compiling Nokia's unchanged checksum.cpp confirms both checksums independently.
The host adapter supplies only TUid's four-byte size and KMaxCheckedUid=3.
The platform inspector rejects header CRC damage. Neither oracle is linked into
the maintained core or Python extension.

**Conclusion:** Clang/LLD plus a small native converter can produce a structurally
accepted import-free E32 experiment. This completes neither a general converter
nor PLAN milestone 1's runtime gate.

**Implementation decision:** Declare e32-pic-experiment explicitly in symbian.toml.
Require trusted ELF links retaining all relocations. Reject absolute/dynamic
relocations, external symbols, writable data, TLS and constructors rather than
discarding them. Hand-written addresses or missing relocation records cannot be
proved absent by format inspection. No native scheduler is needed. Boundaries
release the GIL and translate Status/StatusOr after reacquiring it. Preserve
LLVM tool symlink names in argv[0]: resolving ld.lld to generic lld breaks driver
selection, as demonstrated by the initial failed build.

**Remaining uncertainty:** The direct kernel-thread exit skips User::Exit cleanup
and is restricted to a no-resource probe. Belle's SVC mapping, actual process
creation, complete loader validation, imports, builtins and C++/leave ABI still
require matched runtime tests. Build reports retain loader/runtime flags false.

## 2026-09-30: native emulator build and disposable root smoke tests

**Question:** Can EKA2L1 build on the current macOS host and use private state
before a ROM/Z image is available?

**Experiments:** Initialized all submodules of EKA2L1 revision
2594edf4d6bf55d7bd3f0b46250fe2318d4dc2e8. Installed Qt base/tools/Svg 6.11.2.
Configuration first failed because bundled FFmpeg libraries were Intel-only.
Built the pinned FFmpeg submodule c2022fa4637301736ff7230e7cee5a7e8cd7d45a using
its macos_arm64-build.sh. Built the desktop bundle and ekatests with Ninja,
RelWithDebInfo, camera and LuaJIT disabled. File inspection reports an arm64
Mach-O executable. The final ad-hoc bundle signature verifies. Upstream ekatests
passes 28,172 assertions in 288 test cases. The upstream 11.0 deployment target
disagrees with FFmpeg's 12.0 and
host GTest's 26.0; older-host distribution is not verified.

The first offscreen smoke launch aborted because the bundle ships only Cocoa's
platform plugin. A native desktop --help launch then timed out after 20 seconds
with no devices available. It had written logs/assets/settings inside the
explicit root, and no global EKA2L1 application-data directory was created.
Inspection found a worker waiting on init_event while the CLI failure path
joined it after waking only graphics_event and kill_event.

The tracked instance-root.patch redirects macOS data and default Qt settings
through EKA2L1_DATA_ROOT, rejects invalid roots, makes standalone --help/-h return
zero before worker initialization, and wakes init_event on CLI failure. It also
changes the GDB bind address to IPv4 loopback. Patch SHA-256:
`9b353b0319c7247984165cfb1ced343f9a20cc6ba37b16a1aa91be61ee800652`.
Reverse-application checks match the compiled checkout. No patch was published
or sent upstream. Qt translation generation also changes ignored .ts files.

Four Pytest smoke cases pass: independent roots retain separate Qt settings and
assets, relative roots fail without creating the requested path, a file cannot
be an instance directory, and unknown CLI options exit normally within the bound
without installed device images. Full verification passes 51 Pytest cases,
16 platform GTest cases and five independent oracle GTest cases, Black/Ruff,
clang-format, regenerated stubs and clangd's target source check with zero errors.
CLI doctor still works without an installed native extension; native operations
then return FAILED_PRECONDITION.

**Conclusion:** The emulator builds and its desktop CLI can be smoke-tested with
private settings/assets on this host. This is not a guest boot or proof that all
runtime paths are isolated. No ROM, Z image or firmware was imported.

**Implementation decision:** Keep the emulator, licenses, runtime state and build
products outside Git. Track only replay instructions, the source patch and
independent tests. No emu install/launch/test facade is published before actual
guest completion and isolation can be measured.

**Remaining uncertainty:** Matched legally available ROM/Z assets, Belle/FP2
process startup and application support, all runtime storage paths, concurrent
instances, stopped golden-state restoration and debugger attachment. Device
identity/preservation/recovery remain open; the physical phone was untouched.

## 2026-09-30: historical validation and ROMless CPU execution

**Question:** Does the linked E32 satisfy more than the emulator parser, and
does the generated ARM/Thumb calculation execute with the intended result?

**Experiments:** Compiled Nokia's unchanged ValidateWholeImage implementation
from the pinned kernelhwsrv f32image.h. The separate host adapter supplies
fixed-width types, UID/security declarations, constants and the original
checksum implementation. Assertions check header sizes 124/128/156 and the
export-description offset 152. The fixture is bounded, uncompressed and trusted;
the historical pointer-based validator is not exposed as a general file API.
Seven GTest cases pass: acceptance, CRC damage, negative heap with correct CRC,
insufficient entry/CodeSegID space, missing imports, wrong ARM ABI and truncated
code. The original validator and checksum source were not edited or copied into
the maintained native library.

A synchronous EKA2L1 CPU harness parses the same E32, maps read-only code and
a writable private stack, and starts in ARM user mode. Eight cases pass across
dyncom and dynarmic, load addresses 0x8000 and 0x20000, and positive/negative
inputs. Within a 512-step bound, startup enters Thumb C++, restores the stack,
returns to ARM and reaches SVC 0x73. The callback observes r0=0xffff8001,
r1=0 and r3=0. The normal calculation yields exit reason zero; intercepting
the volatile stack input and changing 16 to 17 yields reason 42. The harness
does not handle the SVC in a Symbian kernel. Dynarmic has fallback paths, so
these are two backend configurations, not wholly independent CPU implementations.

The E32 artifact remains SHA-256
`997cd9c5ec35281f261a08cb3c2ca6a36c74be969b4a72cbbd8ede5ff5332395`.
`toolchain verify-probe` now runs the four separate oracle executables with a
private copy of that input. It clears inherited GTest filtering/sharding,
requires all 20 cases completed without failures/skips/disabled tests, bounds
each native process to 15 seconds, and retains JSON/logs and binary/input hashes.
Input and binary replacement checks reject changed bytes during verification.
Its machine-readable report keeps symbian_loader_verified and runtime_verified
false. Python tests cover missing evidence, reduced/skipped/failed/disabled
reports, isolation from inherited test settings and a real CLI invocation.

Full verification passes 59 Pytest cases with both optional dependency paths,
16 maintained native GTest cases, 20 independent oracle GTest cases and
288 upstream EKA2L1 cases. Black/Ruff, clang-format, generated stubs and
git diff whitespace checks pass. Replay instructions include all three pinned
upstream checkouts and the new native targets.

**Conclusion:** The maintained import-free image passes the historical whole-
image validator and executes its integer/interworking/stack probe under EKA2L1
CPU cores. This is useful partial ABI evidence, not completion of PLAN milestone
1. No Belle process was loaded or launched.

**Implementation decision:** Keep the GPL emulator harness and EPL historical
oracles separate from the platform core and wheel. Python orchestrates tests and
records evidence; native libraries retain format logic. The harness is
synchronous and needs no new scheduler, GIL callback or Python object holder.

**Remaining uncertainty:** Belle loader changes, actual process creation and
kernel dispatch, matched ROM/Z assets, imports, builtins, floating-point/class
ABI, constructors and leave/cleanup behavior. No phone operations occurred.

## 2026-09-30: import-free process through the emulator kernel

**Question:** Can the E32 advance beyond CPU callbacks to actual EKA2L1 process
creation and kernel exit before a matched ROM/Z image is available?

**Research finding:** EKA2 thread initialization enters the executable's own
startup. The euser.dll bootstrap requirement in libmanager.cpp belongs to EKA1.
The import-free EKA2 image can therefore be tested directly through EKA2L1's
virtual filesystem, library manager, process constructor, memory model and
guest scheduler. This does not supply Belle user libraries or system servers.

**Experiments:** Added a separate GPL research GTest executable. It copies the
unchanged E32 into a private temporary host directory, mounts that directory
as a write-protected virtual C drive, installs the flexible memory model and
selects the emulator's epoc10 profile. The original spawn_new_process path
loads C:\\sys\\bin\\probe.exe, creates its main thread and schedules its context.
Execution reaches the real ThreadKill handler via the kernel's original SVC
callback. No SVC implementation or loader algorithm is replaced. CPU faults
stop the test and fail it; no kernel exception-handling behavior is claimed.

Eight cases pass on dyncom/dynarmic: normal exit, changed-input failure,
another launch after failure exit and rejection of a missing executable.
The code maps at 0x70000000, rather than its ELF link address 0x8000. Both
backends execute ARM/Thumb interworking, return exit reason zero normally and
42 when input 16 changes to 17, and release the process address space. The
negative control flushes the TLB before each step so the MMU stack-write callback
can change the input. Each launch is limited to 512 CPU steps. Exit reasons,
backend/profile, code address, injection count and memory release are recorded
in GTest JSON. The test retains diagnostic process/thread objects until kernel
wipeout while checking their exit state and released memory model.

The first compile found a throwing inline UID helper, and the first link required
the upstream test-only platform.cpp UI hooks. The tracked runtime-probe.patch
initializes previously uninitialized ROM mapping fields and the VFS ID counter,
and changes invalid UID indexing from throwing to fail-fast abort. Its SHA-256
is `6b477e9581aa0b3c43e88fbfd38ebec20108ea83f1e69ed9ac98105940dc22a9`.
The emulator loader, scheduler, memory mapping and SVC algorithms remain
unchanged. The original timer lifecycle is reused; its existing worker stops
before kernel teardown. No platform scheduling library or Python callback was
added. The harness itself compiles with exceptions disabled.

Rebuilt the desktop emulator and upstream tests against both tracked patches.
The bundle signature verifies. All 59 Pytest cases, 16 platform GTest cases,
28 independent oracle GTest cases and 288 upstream EKA2L1 cases pass. Format,
stub and whitespace checks pass. The E32 digest remains
`997cd9c5ec35281f261a08cb3c2ca6a36c74be969b4a72cbbd8ede5ff5332395`.

**Conclusion:** This executable demonstrably loads, runs and exits as an EKA2L1
process in the tested ROMless epoc10 configuration. It is stronger evidence than
the CPU-only harness and permits further host-side toolchain work. It is not
an installed Belle/FP2 environment, a full SDK or a general application runner.

**Implementation decision:** Extend verify-probe to require all five research
binaries and 28 completed cases. Report eka2l1_process_verified and
kernel_exit_verified separately; keep symbian_loader_verified/runtime_verified
false until a matched target environment is tested. Keep the general emu and
application-test facade pending its install/launch/isolation gates.

**Remaining uncertainty:** Matched Belle ROM/Z, User::Exit cleanup, imported
DLLs, system services, complete target ABI, package installation, desktop boot,
debugger attachment and full runtime storage isolation. Physical identity,
preservation and recovery remain unverified; the phone was untouched.

## 2026-09-30: CMake/Ninja project integration and wheel replay

**Question:** Can the direct Clang/LLD experiment become a reusable modern
project build without changing its target contract or losing reproducibility?

**Experiments:** Added an ARMv5T Generic toolchain and SymbianPic CMake module
inside the Python package, plus CMakeLists/presets for e32_probe. The helper
accepts multiple local sources, tracks its linker script, compiles C++20 Thumb
PIC with exceptions/RTTI disabled, and links using ld.lld directly with retained
relocations. Python reads CMake's codemodel/cmakeFiles API, invokes the Ninja
build, and converts the ELF through the existing native boundary. No format
algorithm moved to Python and no new scheduler or callback binding was added.

The first CMake build tried to use unavailable clang-scan-deps for C++20 modules.
This import-free profile uses no C++ modules, so module scanning is explicitly
disabled. A cached/fresh comparison then exposed different built-in CMake
configuration input lists. Generated and built-in files are excluded from the
project-input comparison; CMake's version is recorded separately. Project and
platform module files remain hashed and checked.

A multi-source test with source/build paths containing spaces exposed a missing
header hash: Ninja's declared inputs list does not include compiler-discovered
headers stored in its deps log. The wrapper now obtains those through the
documented `ninja -t deps` tool, checks record counts/validity and records header
digests. It does not parse Ninja's binary database. Editing a local multiplier
header changes the E32, a fresh second build agrees, and the next cached build
performs no compilation. Three new integration cases also check missing-target
status and reject the obsolete TOML source fields. The existing source-escape
test now exercises the CMake helper's actual file check.

The baseline remains byte-for-byte identical:

- ELF SHA-256: `4031252adc395d05bb7f3477262b4c00c0018aee1bc7eba40daa35feaa798e4e`
- E32 SHA-256: `997cd9c5ec35281f261a08cb3c2ca6a36c74be969b4a72cbbd8ede5ff5332395`

That CMake-produced E32 passes all 28 historical/CPU/emulator-process oracle
cases. clangd consumes its CMake-generated database with zero errors. Built the
arm64 Python 3.12 wheel, confirmed both CMake files are packaged, installed it
into a separate environment under .symbian/wheel-check-gsu0lt0v, and rebuilt the
same E32 from outside the repository's Python import path. The recorded module
input paths point into that wheel installation. Host tools are CMake 4.4.3,
Ninja 1.13.2, Apple Clang 21 and LLD 23.1.2.

Full Pytest passes 62 cases with the optional emulator/oracle paths. Existing
native behavior remains covered by 16 platform GTest cases and the 28 oracles;
the upstream 288-case emulator suite passed at the preceding checkpoint.
Black/Ruff, generated stubs and whitespace checks pass. Project instructions
are in BUILDING.md, and build reports now use symbian.e32-pic-experiment/v2.

**Conclusion:** CMake can own the build graph and source intelligence while
the modern facade owns reproducibility evidence and native E32 conversion.
The first artifact's measured emulator behavior is retained across that change.

**Implementation decision:** Keep the primary Ninja tree for incremental builds
and real compilation-database paths; use a fresh temporary tree for the second
build. Declare sources/startup/linker script once in CMakeLists, and reserve
symbian.toml for project identity, preset and experimental UID. Package the
toolchain/module with the wheel. Keep object-only compiler research separate.

**Remaining uncertainty:** General SDK imports, writable data/constructors,
package generation/installation, matched Belle runtime and complete target ABI.
The build comparison is local repeatability, not a hermetic compiler/input
attestation. Physical preservation/recovery inputs are still unknown.

## 2026-09-30 — Native SISX and disposable install/launch/uninstall

**Question:** Can the macOS build path generate a package and use EKA2L1's
existing installer before kernel execution, without a historical Windows tool
or Belle ROM?

**Historical behaviour and evidence:** Cloned the public Nokia appinstall
repository to ignored research/upstream/appinstall, pin
`760927eba63e3324cceee974b9ed582da90cb9a3`, preserving its EPL-1.0 material.
Read secureswitools/swisistools/source/sisxlibrary, particularly siscontents.cpp,
header.cpp, sisarray.h, siscompressed.h, sisinfo.cpp, sisfiledescription.cpp,
sishash.cpp and sisdate.cpp. UID1 is 0x10201a7a; UID2 is reserved zero; UID3 is
package identity. Fields carry type/length and four-byte padding; arrays omit
element types from individual headers. Controller and data CRCs cover serialized
fields including headers/padding. File descriptions bind indexed data to targets
and carry SHA-1. The historical default epoch is 2004-01-01, with zero-based
month. EKA2L1's pinned package manager/interpreter already installs files and
registries using only its virtual filesystem/configuration. Its source stores
the file digest without checking certificate/capability policy or enforcing a
phone installer contract.

**Experiments:** Implemented an independent native C++ SIS writer/inspector for
one ordinary import-free E32 executable, no dependencies/scripts/signatures,
uncompressed streams, English ASCII metadata and experimental UIDs. Package
UID 0xe0000809 differs from executable UID/SID 0xe0000808. Fixed the date for
repeatability. Native logic reuses E32 inspection and a shared native UID CRC
helper; no binary format moved into Python. SHA-1 uses static OpenSSL 3.6.5
libcrypto.a, matching A11's linkage pattern. Only the pybind11 boundary enables
exceptions, and it releases the GIL around the stateless native work. The wheel
includes OpenSSL's Apache-2.0 license and has no Homebrew crypto dylib dependency.

Seven maintained SIS GTest cases cover round-trip identity, every truncation and
single-byte mutation, hostile lengths/trailing data, metadata/path/version
rejection, required E32 validation and payload size. A payload changed with a
recomputed data CRC still fails SHA-1; deflate/run operations with a recomputed
controller CRC remain unsupported. Reused the existing E32 fixture and moved the
process environment into a GPL test header so no second scheduler or kernel
implementation was introduced.

Six new GPL oracle cases install into a fresh private writable C filesystem,
compare installed bytes to the independently supplied E32, and inspect registry
identity/version/SID/hash. Both dyncom and dynarmic launch the installed image
through EKA2L1's unchanged loader, scheduler and kernel SVC dispatch. Registry
reload and uninstall remove the executable and prevent process creation.
Reinstall after a changed-input failure exit then runs normally, with exit reasons
42 and 0. The independent hashlib baseline for the unchanged E32 is
`f464490fdf04c80df2e778f65326189e9e2b38d8`; the registry's legacy digest agrees.
A seventh separate EPL case compiles Nokia's original checksum.cpp and agrees
with UID and controller/data CRCs. No EKA2L1 package algorithm was patched.

The baseline E32 remains
`997cd9c5ec35281f261a08cb3c2ca6a36c74be969b4a72cbbd8ede5ff5332395`.
The package is 908 bytes, SHA-256
`9065031847f1db3b1fae1779b478208c38ea3f61a2e5554c2bd293106903abb9`.
`toolchain verify-package` preserves 35 complete cases: the existing 28 oracles
and the seven new ones. Its report is .symbian/package-check/report.json with
retained private inputs, GTest JSON/logs and binary digests. Test filters/shards
are removed and full expected counts are required. Python exposes package,
SIS inspection and verification commands with canonical statuses and strict
TOML fields. The full-suite run exposed a project test that appended its legacy
source field beneath the new package table; corrected it to insert explicitly
into the project table, preserving the original check.

Validation completed: 73 Pytest cases with both optional dependency paths,
23 maintained native GTest cases, all 35 independent native cases, and the
unchanged upstream suite's 288 cases/28172 assertions. Black/Ruff, clang-format,
generated stubs and whitespace checks pass. Built the macOS arm64/Python 3.12
wheel, confirmed packaged crypto licensing and packaging modules, and installed
it into .symbian/sis-wheel-check-wbeb510b/venv. From outside the repository's
Python import path, that wheel generated the identical SIS and completed all
35 verification cases. wheel-result.json records the actual imported wheel path
and retained check report. Otool shows only system framework/libc++/libSystem
links for the native module, with no Homebrew crypto dylib.

**Conclusion:** A modern native writer and EKA2L1's existing installer provide
an observable build→package→install→kernel-exit loop for this maintained probe.
No VM, `.pkg` parser or general package scheduler was needed.

**Implementation decision:** Keep the production writer/inspector synchronous
and bounded, with Python handling policy and reports. Keep historical EPL CRC
and GPL emulator probes in separate research binaries, outside the wheel.
Publish a verification command for the maintained profile, not a general
untrusted SIS installer API. Document packaging in PACKAGING.md.

**Remaining uncertainty:** Matched Belle ROM/Z and installer/loader behaviour,
certificates/capabilities, SDK imports, writable data/constructors/full C++ ABI,
GUI resources and ordinary applications. The fixed timestamp and local repeat
are not a hermetic toolchain attestation. The sole 808's exact identity, preserved
firmware/ROM and recovery baseline remain unknown. No hardware operation ran.

## 2026-09-30 — Frozen ordinal proxies and public SDK headers

**Question:** Which properties of a Symbian import library actually matter, and
can modern Clang/LLD link an original-header User::Exit call without recreating
a historical SDK/compiler environment?

**Historical behaviour and evidence:** Read the pinned kernel source's
kernel/eka/eabi/euseru.def, kernel/eka/include/e32def.h/e32cmn.h/e32std.h,
euser/us_func.cpp and euser/epoc/arm/uc_exe.cpp. The public table contains 2546
exports and assigns `_ZN4User4ExitEi` ordinal 641. Its library MMP links as
euser.dll and records public DLL UID3 0x100039e5; that does not identify the
physical phone's exact DLL/version. User::Exit notifies thread exit, closes
handles and invokes cleanup. Startup establishes the thread heap, TLS, DLL and
static initialization before E32Main. A direct ThreadKill experiment cannot
stand in for those contracts.

Read buildtools' pl_elfproducer.cpp, pl_elfexecutable.cpp and e32imagefile.cpp.
Proxy symbols name ordinal words rather than function implementation addresses.
The ordinal getter requires section index ESegmentRO=1. The historical dynamic
reader interprets its pointer fields as file offsets, and ELF version metadata
carries the target DLL name distinct from the proxy soname. Public producer
flags use BPABI version 4; modern Clang/LLD uses EABI5. The method oracle below
checks ordinal lookup only, not acceptance of all modern ELF flags by a complete
historical consumer.

**Experiments:** The first header compile with only __GCCE__ lacked Int64,
IMPORT_C and literal definitions. Selecting __GCC32__/__GCCV3__ enabled the
actual historical GCC branch. Adding __EABI__ supplied template specialization
syntax. No original header was edited. The first layout assertion incorrectly
expected TRequestStatus to occupy one word. The public class contains iStatus
and iFlags and is eight bytes; EKA2L1's EKA2 request status also has both fields.
Corrected the measured profile. Integer, UID, interval and descriptor assertions
now compile with ARM soft-float C++20 and exceptions/RTTI disabled.

A Clang assembly ordinal word plus an LLD version script can resolve the original
User::Exit declaration. Quoting the version-script node name caused LLD to
retain quote characters in ELF metadata; unquoted euser.dll produces the required
plain identity. A custom linker script makes the ordinal section first and
keeps dynamic metadata addresses equal to their file offsets. The proxy links
and repeats byte-for-byte without an independent hand-written ELF writer.

Implemented cpp/symbian/sdk: bounded ASCII frozen-DEF parsing, unique names and
ordinals, ABSENT/DATA metadata, selected function source generation and bounded
inspection of the generated proxy ELF contract. Sparse ordinals are retained,
not renumbered. Invalid/absent/data selections, aliases, unsupported directives,
unsafe basenames and decorated UID/version names are rejected. Native logic
returns Abseil statuses, disables exceptions and releases the GIL in its bindings.
No scheduler/callback/reference holder is introduced. Python runs CMake/Ninja,
hashes the DEF and consumed source/header dependencies, compares two build trees
and retains the actual database. Target C++ probe source lives under cpp and is
installed as a wheel resource; original SDK assets are not redistributed.

The public source builds euser.dso with one slot, ordinal 641. A User::Exit call
including e32std.h links to it and retains a version need for euser.dll. Both
artifacts repeat in separate CMake trees:

- Proxy SHA-256: `c53aa0b81ec07f6d18c8eab0298a8237e7906909eaa5caac975e55d3659876fb`
- Link probe SHA-256: `934eb1b3da3c3d7cde86388e797a61dfd251e1c32ed7608c65567ae8a42b272d`

Nokia's original GetSymbolOrdinal method is extracted unchanged at CMake
configuration from pl_elfexecutable.cpp and compiled in an optional separate
EPL research test using asserted 16/32-byte symbol/program declarations.
The complete source file's SHA-256 is
`5dacc5f9ef9d830e721548483cd9b6e7bb5ace458d189a0807cb89a098f69c6a`.
It independently reads 641 and returns UINT32_MAX after the section index is
changed. This is a trusted fixture method oracle, not an untrusted parser API
or whole-consumer compatibility result. C++ library/source tests and Python
integration cover parsing failures, no renumbering, actual multi-export proxy
builds with spaces in paths, every generated-ELF truncation, read-only bound
metadata and the original public header/link probe.

Evidence is retained beneath .symbian/euser-proxy: report.json, header_probe ELF,
readelf metadata/relocations, historical_ordinal.json and clangd.log. Clangd checks
the real header translation unit with zero errors. Default LLD places the import
R_ARM_JUMP_SLOT at 0x302dc in a writable GOT/PLT segment with virtual addresses
unlike file offsets. It is not convertible by the existing import-free E32 path;
reports retain import_execution_verified/symbian_loader_verified false.

**Validation:** All 78 Pytest cases pass with the optional emulator, native
verification binaries and public kernel source supplied. The 28 platform GTest
cases and the new historical ordinal method case pass. The existing 35
independent source/emulator checks run through the Python verification tests,
bringing the independent total to 36. Black/Ruff, clang-format, stub regeneration
and whitespace checks pass. Native core compile commands retain -fno-exceptions.
The preceding checkpoint's 288 upstream emulator cases remain evidence for its
unchanged sources; they were not rerun for this SDK-only change.

Built a new wheel and installed it in an isolated virtual environment outside
the source import path. The wheel includes the target header_probe.cc resource.
Its installed CLI compiles the public headers and reproduces both artifact
digests above. Evidence is in
.symbian/sdk-wheel-check-k067mfyh/wheel-result.json, which records the installed
Python module path, source/header hashes and both CMake build logs.

**Conclusion:** Frozen function contracts and original header declarations can
be exposed through modern native utilities and LLVM without carrying the full
SDK or writing another ELF emitter. Compiling and linking them is evidence for
those exact contracts, not for the runtime or complete target ABI.

**Implementation decision:** Provide `toolchain import-proxy` and native
inspection for the narrow generated profile. Keep public headers/definitions
as explicit external inputs and an isolated optional historical-method oracle.
Use CMake/Ninja for real objects, dependency tracking and clangd. Document in SDK.md.

**Remaining uncertainty:** E32 import/GOT conversion, decorated DLL version/UID
identity, actual target euser bytes, startup heap/TLS/static initialization,
leave/cleanup semantics, data exports, full C++ ABI and matched Belle runtime.
The phone's identity, preservation and recovery baseline remain unknown.

## 2026-09-30 — Eager E32 imports and compiled development DLL execution

**Question:** Can modern LLD function calls become E32 ordinal imports without
retaining an ELF dynamic loader, and can an independent emulator consumer execute
the imported compiled function after relocating both code segments?

**Historical behaviour and evidence:** Read the pinned f32image.h import block
and ValidateImports implementation, buildtools' E32ImageFile::ProcessImports,
and EKA2L1's libmanager.cpp/codeseg.cpp. ELF-derived E32 import lists contain
code-relative slot offsets. Slot low/high halves contain ordinal/addend. The
loader locates a dependency, looks up its ordinal in the attached process and
writes the relocated export address into the slot. The public converter rejects
imports into its writable data segment. ELF proxy soname and target LinkAs DLL
identity remain distinct. The original source/checkouts stay isolated research
dependencies; no historical dynamic loader is added to the platform.

**Experiments:** An LLD ET_EXEC custom script puts .plt, .got.plt and dynamic
metadata in one RX load, with a matching read-only PT_DYNAMIC. A retained Thumb
BLX reaches the ARM PLT veneer, which computes its slot address relative to PC.
That veneer needs no load-address relocation after the slot is eagerly patched.
The original-header shared ELF from the preceding experiment remains outside
this ET_EXEC layout.

Native conversion resolves undefined global function symbols against version
records and validated proxy metadata, then writes original ordinals and canonical
E32 import blocks. Retained static calls must reach their own PC-relative LLD
veneers. Dynamic tags, all GOT slots, identities, counts, bounds and string
tables are checked. Only zero-addend R_ARM_JUMP_SLOT function imports are
supported; data/BSS/TLS/constructors, RELA and decorated DLL identity remain open.
DLL case aliases, duplicate/ambiguous functions and unsupported references fail.
The import-free path preserves its previous ELF/E32 bytes and checks.

The first two-DLL test failed because I assumed interleaved 32-byte version
records. LLD packs 16-byte version headers followed by 16-byte auxiliary records.
Readelf exposed the offsets; corrected that layout and reran the test. The native
parser now checks this exact generated profile. Both DLL identities retain
independent ordinals, 7 and 641. This does not generalize to every ELF version
record producer or export type.

The modern CMake project path accepts explicit external proxy inputs, hashes
them with compiler-discovered dependencies and builds twice. Native bindings
copy Python data before releasing the GIL and use canonical Abseil statuses.
Core code keeps exceptions disabled. No scheduler, callback or reference holder
is added to production libraries. SIS packaging and the old maintained-probe
verifier explicitly retain their import-free scope.

Built our own integer function as Thumb C++ in an independent development DLL
fixture. An isolated research producer uses Nokia's original image declarations
and checksums, adds an ordinal-7 export and missing-export bitmap, and validates
the whole result. Its fixed function placement is asserted in the linker script.
This producer is outside the wheel and is not general DLL conversion.
It implements no EUSER function and claims no SDK startup/cleanup compatibility.

Six EKA2L1 cases cover dyncom/dynarmic, positive/changed input and repeated launch
after failure. They parse the import block independently, locate the loaded DLL,
compare the patched slot to its relocated export, observe the CPU at that
function, and check kernel exit reasons 0/42 and address-space release. No loader,
relocation, scheduler or kernel dispatch algorithm was replaced. Added passive
observation hooks to the existing research harness. The phone was not accessed.

The maintained artifacts are:

- ELF SHA-256: `5e7eb4c1662c975de9f0d45675ac8738679bc3a2d98160d67d6b4e44319f841b`
- E32 EXE SHA-256: `8f6cbed4ca3fe010be4d73b276d3671218e9cb3c3e4fbae29284048bced5e1a4`
- Development DLL SHA-256: `e90cbeb360f6ededc04b746f2827ff54ff09d33a06029b1a26b2542f0dc608b0`

**Validation:** All 87 Pytest cases pass with optional emulator, native oracle
and public-header paths supplied. All 32 platform GTests and 42 independent
oracle cases pass. The historical checksum and seven whole-image validator cases
also pass against each new EXE/DLL fixture. The missing-import negative control
now clears iImportOffset explicitly, so it remains a meaningful missing-section
check when testing an imported image. Core builds have no compiler warnings.
Black/Ruff, clang-format, generated stubs and whitespace checks pass. Clangd
checks probes/import_probe/probe.cc with its persistent database: zero errors.
The previous upstream 288-case suite concerns unchanged emulator sources and
was not rerun for this converter/harness change.

Retained evidence: .symbian/import-probe/report.json,
.symbian/import-layout/verification-report.json (private input copies, binary
and artifact digests, 14 EXE checks and eight DLL checks), pytest.log, clangd.log,
and the independent validator/runtime JSON and logs.

Built and installed the new wheel in an isolated environment outside the source
import path. Its CLI builds the same proxy, ELF and E32 using the packaged CMake
module and native extension; import metadata and both executable digests agree.
Evidence is in .symbian/import-wheel-check-mz171mjk/wheel-result.json, including
the installed module path, wheel digest, compiler graph and both build logs.

**Conclusion:** Modern LLD calls can be converted into eagerly patched E32
function imports. The unchanged emulator consumer resolves and executes the
relocated compiled export in this development DLL experiment. No ELF dynamic
runtime or historical compiler environment was required.

**Implementation decision:** Publish the native imported-executable converter,
canonical inspection and separate e32-import-experiment CMake project profile.
Keep the development DLL producer as a research oracle until general DLL
conversion has independent evidence. Document replay and limits in IMPORTS.md.

**Remaining uncertainty:** Production DLL/export/relocation generation, actual
target EUSER and decorated module identity, SDK heap/TLS/DLL/static startup,
User::Exit cleanup/leaves, data imports, full ABI and matched Belle runtime.
Preservation, exact phone identity and recovery baseline remain unknown.

## 2026-09-30 — Native frozen DLL exports and mapped pointer checks

**Question:** Can the modern native converter generate a frozen-export DLL
with the public Symbian ELF loader's export-pointer relocation contract,
without a fixed-address research producer?

**Sources:** Pinned buildtools 7b35cd328d3a5e8e0bc177d0169fd409c3273193,
e32exporttable.cpp and e32imagefile.cpp; pinned kernelhwsrv
0c3208650587ac0230aed8a74e9bddb5288023eb, f32image.h and sf_lepoc.cpp;
pinned EKA2L1 2594edf4d6bf55d7bd3f0b46250fe2318d4dc2e8,
loader/e32img.cpp and kernel/codeseg.cpp. Primary source links are in RESEARCH.md
section 16. Original algorithms were read locally and the public header was
checked through its pinned upstream page.

**Evidence correction:** The preceding research DLL omitted the ordinal-zero
count and code relocations for export pointers, and cleared the unused high
bitmap bit. Nokia's structural validator accepted it. EKA2L1 can resolve its
separately retained export table using a base delta, so successful import lookup
and execution did not establish actual mapped table pointer relocation. The
public ELF loader skips a separate export adjustment because those pointers
must already be covered by code relocation records. Earlier results remain
scoped emulator lookup/execution evidence; they do not prove that fuller
contract or physical compatibility. The original producer remains research
material outside the wheel and is replaced in maintained runtime tests.

**Experiment:** Added native frozen DEF resolution against retained visible
ELF function symbols, preserving original ordinals, gaps, ABSENT entries and
ARM/Thumb addresses. Generate DLL/library UIDs, count prefix, complete ordinal
table, no-hole or full-bitmap header, and text relocations for every export slot,
including absence entry markers. Unused bitmap bits stay set. Header CRC spans
the entire aligned variable header. Native inspection checks identities,
bounds, bitmap shape, absence/address agreement, count word, canonical page
blocks and the exact export-slot relocation list. Optional eager imports share
the existing native import converter. General application pointer relocations,
writable data, data exports, TLS and constructors remain unsupported.

The e32-dll-experiment CMake project uses symbian_add_pic_dll, with ET_EXEC as
its trusted intermediate transport. Python supplies the in-project frozen DEF
and optional proxy paths, records their hashes, and requires two matching
ELF/DLL builds. Metadata exposes DLL identity, header size, exports and code
relocations. The synchronous pybind11 boundary copies Python inputs under the
GIL, releases it for native work, then reacquires it for results/status errors.
No scheduler, callback, event loop or Python reference holder is needed.
All six core translation units compile with exceptions disabled.

probes/dll_probe provides our compiled integer transform at frozen ordinal 7.
Its linked Thumb address is 0x8021, resolved from the real static symbol table;
there is no fixed function location assertion. Its minimal ARM startup retains
the EKA2 marker/reserved word, makes an internal PC-relative call, preserves LR
and returns zero. It provides no SDK initialization or resources. The process
harness checks the patched import slot, CPU execution at that function, kernel
exit/address-space release and repeated launch on both dyncom and dynarmic.
It now independently reads export relocation records and inspects the actual
mapped export array: ordinal 7 agrees with lookup, all six absent pointers
relocate to the mapped entry, and the count prefix remains seven.
No upstream parsing, relocation, memory, scheduler or kernel dispatch algorithm
was altered. Historical checksum/validator test adapters now retain complete
bounded variable headers; their original algorithms are unchanged.

**Validation:** All 101 Pytest cases pass with optional dependency paths supplied;
all 38 platform GTests and 42 independent oracle GTests pass. Native tests cover
ARM/Thumb addresses, no-hole tables, sparse/ABSENT ordinals, maximum ordinal,
relocation page boundaries, truncation, corrupt pointers/counts/blocks and
missing/data export errors. Nokia's checksum plus seven whole-image validator
cases pass separately for ordinals 7, 641 and 65,535. These exercise 156-, 236-
and 8,348-byte headers. A combined import/export image also passes those
consumers, but its executable startup is layout evidence only, not a runnable
DLL initialization routine. All six maintained import execution cases pass
with the generated DLL and stronger mapped-pointer checks. Core compilation
has no warnings; research links retain the documented host deployment/library
warnings. Black/Ruff, clang-format, generated stubs and whitespace checks pass.
Clangd consumes the DLL CMake database with zero errors.

Retained evidence is under .symbian/native-dll: report.json,
verification-report.json (38 checks across importer and three DLL variants,
private input copies, binary and image digests), pytest.log, native-ctest.log,
independent-ctest.log, compile-policy.json and clangd.log. The wheel is installed
in an isolated environment outside the source import path; it builds identical
ELF/DLL bytes and metadata using its packaged module and extension. Installed
path, wheel digest and build evidence are retained in wheel-result.json.
The earlier upstream 288-case checkpoint concerns unchanged emulator source;
that upstream suite was not rerun for this converter/test-adapter change.

Maintained artifact SHA-256 values:

- DLL ELF: `0ecaad29f85a031a99098e837905403b3fd39c875f88bb9121bdc698bc56c76e`
- Native E32 DLL: `188cd1a6d8d40a4d00d21dc74fedfa57912dea67d0e2c25811cc178f6ebe9f9d`
- Unchanged importer EXE: `8f6cbed4ca3fe010be4d73b276d3671218e9cb3c3e4fbae29284048bced5e1a4`

**Conclusion and decision:** Publish the scoped native frozen-function DLL
converter, inspector metadata and CMake project profile. Maintained ROMless
execution now checks generated export-pointer relocation rather than only
lookup. Keep full-bitmap output canonical even where sparse encoding would be
smaller. Preserve experimental runtime/loader flags false in build reports.
SIS and verify-probe retain their import-free executable scope.

**Open questions:** General code-pointer/vtable relocation, writable data/BSS,
static construction/destruction, TLS and target DLL initialization, SDK heap
and User::Exit cleanup/leaves, actual EUSER/decorated module identities, complete
C++ ABI and matched Belle runtime. One physical phone is still the only declared
hardware; exact RM/product/firmware, preserved ROM/Z and recovery baseline remain
unknown. No phone or device operation ran.

## 2026-09-30 — Internal pointer tables, C++ virtual dispatch and installation

**Question:** Which additional modern compiler output is needed to run ordinary
const callback tables and simple virtual dispatch through the existing loader?

**Sources and survey:** Re-read PLAN sections 10, 20 and 21, current RESEARCH.md,
STATUS.md and converter sources. Reviewed A11 native status and Python boundary
conventions; this change remains synchronous and adds no scheduler. Read pinned
Nokia pl_elflocalrelocation.cpp/e32imagefile.cpp, the EKA2L1 relocation consumer,
and Arm's ELF specification. Primary upstream pages were checked; links are in
RESEARCH.md section 17. A first guessed historical filename returned 404; the
local checkout identified pl_elflocalrelocation.cpp and its pinned page was read.

**Experiment:** A real Clang PIC function-pointer table emits `.data.rel.ro`
with SHF_WRITE and R_ARM_ABS32. Default visible table access also requires
GOT_PREL, outside the supported profile. Hidden internal table/class visibility
emits local REL32 access, while the table pointers still require load-time
adjustment. A single-inheritance virtual class similarly needs a vtable
function pointer, without RTTI, allocation or target runtime imports.
The initial research ELF and source are retained under .symbian/pointer-research.

The historical producer chooses text/data relocation kinds according to the
referenced segment. Its unresolved ELF pointer fixup adds a symbol value; the
modern ET_EXEC word is already resolved. Copying that fixup would incorrectly
add the symbol twice. The native converter instead preserves the linked word
and emits a Symbian text relocation. It checks retained symbol/target bounds,
word alignment, instruction-state agreement for functions, duplicate fixups and
in-range targets. External absolute pointers and GOT/dynamic metadata fixups
remain unsupported. A bounded native name reader admits `.data.rel.ro` and its
dot-suffixed sections only as non-executable PROGBITS tables inside the RX load.
Ordinary writable data/BSS, TLS and construction arrays remain rejected.
This is a trusted compiler/link contract, not semantic validation of arbitrary
hand-authored code. One-past-the-mapping pointer values remain unsupported.

Native inspection now handles application and DLL export fixups together,
requires all export slots and rejects import-slot aliasing. The combined
relocation count is bounded to 65,535. Existing relocation-free artifacts
retain their bytes. The former blanket ABS32-negative controls now check
out-of-range and unaligned pointers; new positive controls establish the
supported behavior. No relocation producer/parser logic was added to Python.
All six core translation units still compile without exceptions and return
Abseil statuses; existing GIL release boundaries cover this native work.

**Maintained vertical slice:** probes/pointer_probe builds four source units
with its real CMake database and discovered header dependencies. It has two
const callbacks (Thumb C++ and ARM assembly), a constant-text pointer with
addend one, and a Thumb virtual method in a stack object. Its four E32 fixup
offsets are 240, 244, 248 and 260. The separate GPL research harness uses the
unchanged EKA2L1 parser and process loader, inspects all four mapped pointer
values against the actual base delta, and observes the CPU at all three
function targets in their correct ARM/Thumb states. Both backends exit zero
normally, exit 42 with changed input, and launch successfully after failure.
Kernel exit releases the address space. The label pointer retains its addend.
The compiler-generated stack vptr and indirect method call execute as emitted.
No upstream relocation, memory, process, scheduling or kernel algorithm changed.

The same native SIS writer packages this pointer-bearing image. The existing
installer harness loads it, checks unchanged installed bytes and registry
fields, launches it, reloads the registry, uninstalls and reinstalls it. A
separate retained Python hashlib SHA-1 reference replaces the fixed old hash
only when supplied explicitly; the earlier fixture keeps its historical
baseline. The common verifier now clears inherited Symbian fixture variables
as well as GTest filters/shards before passing private copies. Existing filter
and input isolation controls verify that unrelated hash paths do not leak in.

The new `toolchain verify-pointers` command performs 14 original checksum,
whole-image validator and mapped-pointer/dispatch cases. Supplying `--package`
adds seven checksum/installer cases for 21. It records binary/artifact hashes,
private copies and complete native reports. `verify-probe` rejects code-fixup
images explicitly, preserving its earlier relocation-free scope. Build/package
and verification reports keep matched Belle and physical flags false.

**Validation:** All 112 Pytest cases pass with optional emulator, oracle and
public kernel paths supplied. All 41 platform GTests and 48 independent oracle
cases pass on Apple Silicon. The new six process cases pass on both backends.
Real-link combined import/application-pointer and DLL export/application-pointer
layouts pass the unchanged Nokia checksum and seven whole-image validator cases;
the combined DLL has executable startup and is layout evidence only. Negative
controls cover pointer value/state/alignment, duplicates, section-name bounds,
unknown writable sections, import-slot aliasing, verifier scope and output/input
collisions that must preserve supplied artifacts. The earlier
import/DLL/package tests remain green. Core builds have no warnings. Research
links retain the previously documented deployment/library warnings. Black/Ruff,
clang-format, generated stubs and whitespace checks pass. Clangd checks the
pointer example through its persistent CMake database with zero errors.
The unchanged upstream emulator 288-case checkpoint was not rerun here.

Evidence is retained in .symbian/pointer-probe (build report, pytest.log,
independent-ctest.log, clangd.log, compile-policy.json and wheel-result.json),
.symbian/pointer-package/package-report.json, and
.symbian/pointer-check/report.json with the 21-case private test reports/logs.
An isolated installed wheel outside the source import path reproduces identical
ELF/E32/SIS bytes and runs the complete 21-case verification loop. Its installed
module path, wheel digest and nested build/package/verification reports are in
wheel-result.json.

Artifact SHA-256 values:

- ELF: `abc3c3c9e31d3ca9f325627af4ce86813d1b7c295e6be9ea294fe344a583a9ce`
- E32: `82cbc8e080efdc844733a273bdcfcf69914b71e2535367188a43348e637ec73a`
- SIS: `b8201005adf9908402d11e84de3e90aa9c235cef013cf211dcd57a144bc82c7d`

**Conclusion and decision:** Native macOS Clang can supply these ordinary C++
const-table/dispatch contracts with local visibility and retained pointer
fixups. Publish the bounded internal ABS32/RELRO extension, maintained probe
and verification command. Reuse the existing package writer and loader; no
compatibility runtime or additional scheduler was needed. Replay and exact
profile limits are documented in POINTERS.md.

**Remaining questions:** General GOT/preemptible data and external function
pointers, writable data/BSS/TLS, target DLL initialization, global lifetime,
multiple/virtual inheritance and RTTI, target heap and User::Exit cleanup/leaves,
full Symbian C++ ABI, actual system DLL identities, matched Belle runtime and
physical installation. Exact phone identity, firmware/ROM/Z preservation and
recovery baseline remain unknown. No device operation ran.


## 2026-09-30 — C++20 language, module and library boundaries

**Question:** Can modern C++20 programs be supported, beyond selecting a compiler
standard flag? What needs a target runtime rather than only compiler support?

**Experiments:** Added probes/cxx20_probe with actual concepts/requires,
structural class template arguments, consteval, designated initialization,
constrained generic lambdas, defaulted equality, constinit callback tables,
char8_t and a no_unique_address layout assertion. C++17, signed constrained
arguments and dynamic constinit initialization fail for the expected reason;
positive controls use the actual ARM compilation command. Apple Clang 21 builds
it without C/C++ system headers or hosted libraries. Optimization preserves the
earlier pointer ELF/E32 bytes. Both backends pass mapped pointer/state checks,
indirect/virtual dispatch, failure/relaunch and the complete 21-case SIS loop.

Homebrew libc++ 23.1.2's unmodified host configuration fails for ARM availability
and thread configuration. A separate research __config_site disables those and
unsupported host facilities, leaving upstream header bodies unchanged. Upstream
Clang 23.1.2 compiles bit/concepts/span with its freestanding resource headers;
coroutine fails for memcpy, ranges for memory/mbstate_t/stdio, atomic for memory
and time declarations. These failures identify missing port work, not language
impossibility. The maintained opt-in library callback uses span, rotate and
population count, with a separate expected arithmetic reference. Apple Clang 21
builds this distinct E32; all 21 loader/installer checks pass. Its 186 input
hashes include upstream headers and the isolated configuration. No target libc++
binary, C library or host SDK is linked. Matching runtime/library configuration
will be required for a real port, as LLVM's vendor documentation specifies.

Upstream Clang 23.1.2 builds a minimal named module; the installed Apple compiler
rejects it with the tested ARM flags. Added probes/cxx20_module_probe with a
CMake CXX_MODULES file set and explicit target scanning. Independent CMake trees
produce identical ELF/E32, retain a BMI and record actual compilation commands.
Changing its immediate function and arithmetic body rebuilds an unchanged
importer and changes the artifact. The earlier exported constexpr object
experiment generated init_array; conversion correctly rejected it. The
maintained module uses an immediate function, requiring no static startup.
Its original parser/checksum/validator, both CPU backends at two addresses,
real kernel and SIS installer loop passes all 35 cases.

Package verification now supplies an independent hashlib reference for the exact
caller-supplied executable, like the pointer verifier. It preserves original
hash baselines for direct native tests, checks package payload size before
execution, and rejects report/hash output collisions with either input. The
original package and new module package both pass. All inherited fixture/filter
isolation remains in the existing common verifier; no format logic moves into
Python and no scheduler/callback/holder is introduced.

**Validation:** All 124 Pytest cases pass with optional emulator, public headers,
module compiler, libc++ configuration and oracle paths supplied. Root native
platform GTests and optional SDK ordinal oracle pass. Existing independent
loader/installer binaries are reused without algorithm changes: language 21,
selected library 21, module 35 checks. An isolated installed wheel outside the
source import path reproduces all three ELF/E32/SIS variants and repeats all 77
checks. Its module path, wheel digest, nested reports and source/tool hashes are
in .symbian/cxx20-probe/wheel-result.json. Commands/header logs are in
.symbian/cxx20-research; full Pytest evidence is in cxx20-probe/pytest.log.
The unchanged upstream 288-case checkpoint was not repeated.

**Writable-data investigation:** Public kernel sf_lepoc.cpp and EKA2L1 apply
separate code/data deltas. A real two-load PIC link emits REL32 across those
segments, which cannot assume the linked distance survives loading. An explicit
non-PIC link emits ABS32 with code/data targets: five code-to-data words and
three initialized-data words, including a BSS pointer and Thumb callback.
Artifacts remain in .symbian/data-research. The incomplete typed relocation/API
sketch was removed before publication; no writable-data support is claimed.
A future implementation must distinguish source section from referenced segment
and reject cross-segment relative contracts, then validate initialization/BSS
and mapped words under differing deltas.

**Decision and remaining work:** Publish the scoped C++20 probes and CXX20.md.
A broad target standard library needs C library/compiler-rt, allocation/failure
policy, ABI configuration, SDK heap/TLS/DLL startup and cleanup, static lifetime
and synchronization/event-loop adapters. Modules do not expose historical DLL
interfaces automatically. Complete conformance, matched Belle and physical
execution remain unverified; hardware/firmware identity is still unknown.
The user's subsequent GUI example request is the next active slice.


## 2026-09-30: Native GUI example and complete walkthrough

**Request:** Create examples/gui_app and root WALKTHROUGH.md describing its
construction, toolchain, emulator testing and debugging. Continue without
permission prompts. The only known hardware remains one Nokia 808; actual
RM/product/firmware identity and matched ROM/Z material are not known.

**Source investigation:** Read original W32STD.H and WS322U.DEF at graphics
ff133bc50e6158bfb08cc093b0f0055321dcde99, and original ARM uc_exe.cpp/uc_exe.cia startup,
euseru.def and kernel headers at kernelhwsrv
0c3208650587ac0230aed8a74e9bddb5288023eb. Header dependencies additionally use
ossrv 1e9520caca186c601dd9768449b86bc72be39a22, persistentdata
ef8baa21cee9cd1e214e1a7986595c60b3a63271 and textandloc
59666d6704fee305b0fdd74974f7b4f42659c6a6. Original header case and the SDK's
flattened include names require explicit aliases; a graphics/gdi header cannot
be chosen merely by ambiguous basename. The maintained owned manifest lists
92 aliases and per-file hashes. No upstream header contents, firmware or runtime
state are added to Git. Licenses remain alongside original ignored checkouts.

**Implementation:** Added a raw Window Server counter rather than assuming an
unimplemented Avkon/resource/Qt stack. Pure model and target drawing adapter
are separated. The four digits and controls use rectangles, requiring no fonts.
The primary entry preserves the EKA2 marker and code-segment unique-ID word at
entry+12, passes R4/SP to the adapter, aligns the stack, performs SDK heap setup
and process initialization, and exits through User::Exit. Compile-time checks
record source-derived thread/status layout sizes. Secondary-thread and exception
entry fail through User::Invariant; global constructors are unsupported.

Window Server uses real EventReady/RedrawReady requests and User::WaitForRequest.
Only pointer button-down and redraw events for window handle 2 are handled.
Pending requests are canceled/completed before stack statuses disappear;
window/group, graphics objects and session are closed in dependency order.
An original compiler attempt exposed ARM division builtins; a small bounded
unsigned routine now handles digit extraction and layout, tested against host
integer arithmetic. Digit scale is bounded by both width and available height;
the final tests include extreme wide and tall allowed displays. Rotation after
launch is not yet supported. No second host scheduler, Python callback, holder
or format parser is introduced. Native host code retains its A11 reference
style/status/no-exception policy, and target ABI calls use required SDK results.

prepare-gui-sdk validates manifest types, confined paths and SHA-256, preflights
both selections in the native core, stages links, and uses the existing native
ordinal proxy generator. It refuses occupied/redirected destinations and changed
inputs, preserves unknown provenance as unverified, and retains exact input
digests. The two proxies contain 9 EUSER and 28 WS32 functions. They provide
ordinal link contracts, not runtime implementations. Python policy tests cover
bad nested types/selections, changed input, traversal, malformed CLI input and
destination redirection; actual CMake/proxy/build integration uses source paths
and output paths containing spaces.

**Build and validation:** Apple Clang 21.0.0 and LLD 23.1.2 produce reproducible
ARM ELF/E32 in independent CMake/Ninja trees. Target code uses C++20, no exceptions
or RTTI, explicit source SDK macros and no host standard includes. Debug flags
are -g -gdwarf-4 -O1 with stable source/build/SDK prefix maps. Both system libraries
are eager function imports, no writable data/TLS/constructors. The final E32
SHA-256 is 93428ab91029784854db74f36c87d32d441c305572a9476eaeda7d3068a03641;
ELF SHA-256 is 1d02b5449ac1b765a77749ee61889efd3fcc3d550888f31910b13784321d38ee.

verify-gui retains one original checksum and seven unchanged whole-image
validator cases. It records all execution/loader/debugger flags false. It
does not misuse the integer/pointer execution harnesses for system imports.
The existing generic build report's claim that every EXE directly kills its
thread was corrected: startup and cleanup are project behavior not proven by
static conversion. The original import-free SIS writer is unchanged and this
project has no package declaration or application registration.

The full suite passed all 139 Pytest cases with optional emulator, public headers,
module compiler, libc++ experiment, source SDK and oracle paths supplied. All
six root CTest entries pass, including the five GUI model GTests and the optional
original SDK ordinal check. After the final aspect-ratio improvement, model
and all 15 GUI Python cases pass again. An isolated installed wheel outside
the checkout import path runs prepare/build/verify through its CLI, stages both
proxies, reproduces exact final ELF/E32 and repeats the eight historical checks.
Wheel/module paths and all nested evidence are in gui-research/wheel-result.json.
Black/Ruff, C++ formatting, stub checks and diff whitespace checks pass.

llvm-dwarfdump verifies the final DWARF. LLDB identifies ARM and resolves
GuiMain/DrawGui and mapped source lines. clangd parses both C++ files with no
errors under --tweaks=ExpandAutoType. Unrestricted check-mode feature probing
reports two ExtractFunction failures at break statements; there are no compiler
diagnostics, and the limited refactoring selection is recorded explicitly.
This does not claim full clangd refactoring correctness.

**Emulator/debugger research:** Reconfirmed pinned EKA2L1 CLI help in a private
root without ROM. --run accepts absolute virtual EXE paths; --device requires
the recorded firmware code. Source inspection establishes default storage
data/drives/c or per-device data/drives/<lowercase-code>/c, configuration cpu,
enable-gdb-stub/gdb-port and trace keys, and runtime-code mapping logs. The local
patch confines GDB to IPv4 loopback. Upstream docs require same-device ROM/Z and
describe whole-guest Dynarmic software breakpoints. GDB's own documentation
specifies symbol-file -o relocation; actual mapping minus 0x8000 is required.
Offline symbols are tested; guest connection, breakpoints and GUI visuals are
not. WALKTHROUGH.md provides exact source/toolchain/build commands and explicitly
unexecuted emulator/debugger steps, acceptance criteria, logging and diagnosis.

**Open questions:** Does real matched Belle EUSER agree with the public thread
layout and startup/cleanup calls? Do target WS32 exports/versions and service
semantics match the selected ordinals? Does pointer event routing/redraw work
in the intended booted device? Does heap and GUI teardown survive repeated
launch? Can the pinned GDB stub stop/step this relocated Thumb image accurately?
ROM/Z is still missing; these questions remain open rather than manufacturing
service implementations or declaring loader compatibility from ELF generation.
No physical-device operation, OS boot or visible GUI test occurred.


## 2026-09-30: GUI SIS transport and user-supplied Delight firmware

**Continuation and steering:** Prior GUI work was a progress turn: source,
toolchain, tests, walkthrough and commit abdc655 are authoritative. The next
platform slice extends GUI packaging independently of matched execution. During
this work the user supplied /Users/helena/Downloads/Nokia 808 PureView
(Delight v1.8).zip and asked to check it. Asset inspection/import became the
next independent experiment. No permission prompt or physical operation ran.

**Packaging evidence and implementation:** Read EKA2L1's SIS interpreter and
registry behavior. It extracts E32 as opaque bytes and reads SID from its header;
imports do not alter the SIS representation. Removed the writer's artificial
import-free restriction, preserving its validated single-EXE/unsigned/ASCII
profile and DLL rejection. No package-format serialization changes were needed.
Native inspection now exposes the verified embedded SHA-1 string. Python uses
independent hashlib to reject a mismatched equal-size valid executable before
writing or invoking installer checks; no native format logic moves to Python.
The immutable binding and generated stub expose this field, with existing
GIL/status policy intact. Host native code remains no-exception Abseil style.

Added the GUI package declaration (package UID e0000812 versus executable/SID
e0000811). Package bytes are 7,036, SHA-256
7c7f2a402c4f22175c2e120fdc6dd39f37508168908e28fe07ee50828b6f4ebf.
The E32 and ELF hashes remain those of the GUI checkpoint. The new verifier runs
eight historical image cases, one original SIS checksum case and eight real
installer/registry cases. Each configured backend gets a private filesystem,
complete installed byte/metadata/SID/hash checks, registry reload, removal and
reinstallation. These cases deliberately execute zero guest instructions.
All reports distinguish installer evidence from GUI/SDK/Belle execution.

**Unexpected loader behavior:** The initial control expected absent EUSER/WS32
to reject process creation. Six installer cases passed; both rejection cases
failed because the loader returned a process. Failure JSON/CLI transcript is
retained under gui-package-check/run-uehyvdul. Source inspection identifies
buildup_import_fixup_table discarding bool failure from elf_fix_up_import_dir.
An independent upstream parser/memory inspection now records the actual
failure: process creation succeeds and all 37 import slots retain their original
ordinals, with no guest instructions run. The verifier reports rejected=false,
unresolved_import_slots_observed=37 and GUI/runtime flags false. This observation
does not resolve the loader defect. A correct future fix needs code-object and
dependency rollback, including cycles and previously loaded dependencies;
forcing deletion without those tests could leave dangling references. Upstream
loader/installer algorithms were not changed to turn this control green.

**User archive checks:** The ZIP is 202,675,963 bytes, SHA-256
88403c3a8ef5ed14a48712a70fd81020b2b33ae17d5487a595004f09b8c10d98.
It contains seven files plus the RM-807 directory: core, ROFS2, ROFS3 and UDA
FPSX files, VPL, DCP and a signature file. VPL declares RM-807, product 059M7Q4,
version 113.010.1508 and the French/Euro variant. All non-optional VPL inputs
are present; supplied ZIP CRCs validate and supplied VPL CRC entries agree.
Optional eMMC/templates are absent. The signature file is opaque; no authenticity
or physical-device matching verdict is inferred from the filename, CRC or hash.
The existing Downloads ZIP is unchanged. Working extraction is private under
.symbian/assets/delight-v1.8, with per-file hashes and reports under gui-research.

**Actual emulator import:** Added a separate GPL no-exception research GTest
calling EKA2L1's actual VPL/FPSX/ROM/ROFS/FAT installation path, without any
hardware transport. It requires explicit inputs and a new absolute root, refuses
existing state, requires exactly one chosen variant, and is not a default CTest
job. Supplied material imports successfully in 1.8 seconds into
.symbian/instances/delight-import-01. Device metadata identifies Nokia,
808 PureView, RM-807 and epoc100, initially machine UID zero. Imported data is
329 MiB/13,438 inventoried files, with a 31 MiB SYM.ROM, Z/system DLLs and
isolated C storage. The immutable-offline preservation requirement is not
fulfilled by these owner-controlled copies; original/working/imported artifacts
and recorded digests are only local research preservation evidence.

**Runtime probe:** A separate copied root delight-gui-01 contains the unchanged
GUI EXE on per-device C. The first frontend attempt occurred before the filesystem
copy finished and failed device discovery; the copy handle was then awaited and
the completed device metadata checked before retry. Record this sequencing
mistake rather than interpreting the early failure as an asset defect. The
second attempt has actual runtime mappings: GUI 70000000, EUSER 804bcce8 and
WS32 80a4c028. It initializes a screen buffer and logs unimplemented SVCs 51/F7
and a $HEAP lookup failure. No framebuffer inspection, pointer test, healthy
exit or debugger attachment has been performed, so visual/SDK runtime flags
remain false. TERM did not stop the process; after confirming the exact private
argv/PID and continued liveness, KILL stopped it. The retained imported baseline
was not booted or modified by this runtime test. Logs and mutable state remain
ignored. Archive/phone match, correct startup and emulator service coverage
remain separate questions; no phone API or operation is involved.

**Verification:** All 142 Pytest cases passed with explicit optional paths;
all 18 GUI Python cases passed, including native imported packaging, DLL
rejection, equal-size mismatched payload rejection and installer/registry checks.
All six root CTest targets pass. The new eight-case GPL package harness and
one-case firmware importer pass against actual supplied/generated inputs.
An existing-root negative replay fails before import; all 13,438 baseline file
digests still agree afterward. Final archive/baseline integrity and importer
binary hash are retained in delight-final-integrity.json.
The installed wheel, outside the source import path, prepares the SDK, reproduces
ELF/E32/SIS and passes eight image cases plus the 17-case package check. Native
format logic and existing scheduler architecture are unchanged. Black/Ruff,
ClangFormat and stub/whitespace checks pass. Updated WALKTHROUGH.md and package
docs replace the earlier absent-assets/no-GUI-package claims with measured
results. Goal remains active; asset availability now enables SDK startup and
guest-debugging work rather than repeated missing-ROM status reports.

### 2026-09-30 — Live GDB startup diagnosis using the imported RM-807 image

Installed Homebrew ARM GDB 17.2. Fresh copies of the unbooted imported baseline
were completed before starting their frontends. A loopback GDB connection and
relocated source breakpoint reach GuiRunThread with reason=0, info=0x40ffc0,
PC=0x700009da, SP=0x40ffb8 and LR=0x70000020. EUSER/WS32 mappings agree with
the prior real import/launch. Inputs remain exact ELF/E32 from the GUI build.

The first unpatched debugging run was inconsistent: a stop at startup.cc:19
was followed by registers already at the startup return loop and result=-2.
Those stale/moving observations are not the heap result. Inspection found that
system_impl::loop never clears the remote step flag or sends a completion stop
after its CPU step. GDB's breakpoint continuation steps therefore keep running.
The GPL guest-debug-step.patch clears the flag, saves the stepped context and
sends a stop before further scheduling. No SDK or SVC table was altered.

With this patch, two stepi commands produce PCs 0x700009dc and 0x700009de.
A delayed fresh register read remains at the first stop. SetupThreadHeap returns
-1, with PC still at 0x700009e8, LR=0x804cd55f and SP=0x40ffb8. Real SDK heap
startup fails before GuiMain; the source adapter does not invent another heap.

ROM instruction stops retain these executive-call arguments and addresses:

* At 0x804bf730, SVC 0x51 receives 0,7,0x40ff80,0; LR=0x804cadab. Original
  UserHal::PageSizeInBytes and u32hal.h identify kernel-group/page-size query.
  The emulator table registers HAL at 0x4F and has no 0x51 implementation.
* At 0x804bf810, SVC 0x6D receives owner=1, name=0x40febc ($HEAP),
  create-info=0x40ff14; LR=0x804cd55f. The create-info begins 0x82,0,0,0,
  0x00220000 and the original heap implementation calls RChunk::Create.
  The emulator instead dispatches handle_open_object and returns not-found;
  chunk_new is registered at 0x6B.
* User::Exit(-1) reaches SVC 0xF7 at 0x804bfc40 with LR=0x804cc18b. Its caller
  agrees with the original exit path, supporting the thread-exiting inference.
  The emulator registers thread_user_exiting at 0xF6. We stop before dispatching
  this missing call; this establishes neither a clean exit nor cleanup.

The new opt-in Pytest drives actual frontend/GDB processes, checks exact
ROM/EUSER/ELF/E32 digests, uses a copied instance and loopback port, and retains
its logs. The first source-display assertion failed because `list GuiRunThread`
did not include the requested heap line; explicitly listing startup.cc:18 fixes
that test assertion. The subsequent mapped-source run passes in 6.71 seconds.
Removing only the step patch, rebuilding, and running the identical test fails
with a bounded 30-second GDB timeout (37.07 seconds total). The patch is restored
in a finally block and the frontend rebuilt; the complete 143-case Pytest suite
passes in 46.51 seconds. All six root CTest targets pass in 0.17 seconds;
Black/Ruff and whitespace checks pass. The frontend binary hashes differ across
rebuilds and are recorded; no reproducible emulator binary claim is made.

Evidence: delight-gdb.log and delight-gdb-step-fixed.log retain manual runs;
debugger-pytest-mapped retains the positive replay; debugger-negative retains
the negative run and its instance; debugger-control.json records binary hashes;
debugger-full-pytest retains the restored full-suite instances and GDB transcript;
debugger-full-pytest.log records the full suite. Each confirmed private frontend was
stopped; TERM did not complete shutdown, so only its owned process was killed.
No normal guest termination is claimed. The original archive SHA-256 and all
13,438 baseline files are rechecked unchanged in debugger-input-integrity.json.

Open work: map a complete firmware-specific executive ABI independently;
identify a reliable profile selector; retain older firmware compatibility;
verify heap/init/Window Server services, visible drawing/input and SDK cleanup;
then broaden debugging to unwind/crash/thread inspection. Current debugging
proofs do not change GUI-runtime, OS-boot, physical-match or device-operation
claims. The project goal remains active.


### 2026-09-30 — Firmware-specific routing, ARM thread state and SDK cleanup

Continued from the live heap failure and revalidated the modified worktree.
The original imported platform.txt declares SymbianOSMajorVersion=101, while
EKA2L1's version detector groups 100/101 under epoc10. The preserved device/ROM
identity is a research fixture; it does not identify the physical phone.

Added a trusted opt-in native ROM probe using unchanged upstream load_rom and
parse_romimg. It copies ROM into its temporary environment before mapping it,
retains actual EUSER code/export data, and records zero guest instructions.
Observed code address 0x804bcce8, size 301100, exports 2566 and version 2.4.0.
Output path guards reject an existing directory and a child of the preserved
input; those negative controls leave the child absent. The probe remains
excluded from the production wheel and default research CTest invocation.

The original genexec.pl generates old executive numbers from execs.txt. Its
obsolete cpp flags fail on current macOS. A private shim invokes current Clang
as a C++ preprocessor while retaining the original Perl generator. Native DEF
parsing joins original frozen ordinals to mapped real ROM exports. LLVM builds
an analysis-only ELF with raw code and symbol labels; GDB explicitly selects
ARM for the stub region and Thumb for exported wrapper code. The output contains
307 decoded stub addresses and 446 exported wrapper call records. Comparing
source functions and Exec call sets yields 313 constraints and 212 distinct
old-call mappings without conflicts. Ambiguous wrappers are omitted. A first
research regex stalled; its identified Python process was stopped and a linear
block parser replaced it. That was an analysis-script defect, not guest behavior.

The observed shifts are zero through 0x10, +2 through old 0xD5, and +1 from old
0xD8 onward. The two intervening private source operations are not independently
identified. Fast calls in the observed subset retain their numbers. Added a
separate map of existing handlers, excluding the two unverified newer loader
extensions. Metadata GTests confirm 172 original and 170 experimental handlers.
Unknown new calls and the SVC 0xFF/HLE trampoline collision remain unresolved;
this is not a complete firmware executive specification.

Selection requires EKA2L1_EXPERIMENTAL_SVC_PROFILE=rm807-113.010.1508, epoc10 and
full-ROM digest b5c1ea63cb6359270c5b7cfb1bb453594e208a01b8aeb5b5e020f37d546f7086.
Both unknown selection and a one-byte-modified private ROM are rejected by real
frontend tests. The exact digest does not authenticate other firmware files.
Default routing remains unchanged. The first full-file GDB attempt failed because
the stub advertises qXfer:libraries:read but does not implement it. Removing that
advertisement permits normal file/symbol-file loading; no library introspection
support is claimed. The six local patches replay successfully on fresh pinned
source files in the documented order.

The profile first returns heap result zero at 0x700009e8 and reaches GuiMain at
0x7000002a, then aborts on actual ARM instruction 0xee1d0f70 at 0x804c26f0.
The raw fallback initially misdecoded it as Thumb; forcing ARM identifies
TPIDRURO. Arm's architecture manual and the original Symbian SMP scheduler show
that it is separate from TPIDRURW. Dynarmic lacks the read; dyncom already reads
it but omits context save/restore. Added independent state/read/preservation on
both tested macOS backends and zero initialization for a new HLE thread. Writes
to TPIDRURO and ARM32/12l1r remain outside this patch's verified scope.

Four native cases execute unequal-register reads, context switching and saved
context restoration on both backends. The initial two-word memory fixture caused
an interpreter read-ahead exception; mapping a whole code page fixes the test
fixture. Initial SVC-name assertions also used a std::string where GTest expects
a C string; the helper now uses c_str without throwing map::at calls. A build
attempt selected nonexistent target eka2l1; the actual frontend target is
eka2l1_qt. Corrected builds and native tests pass with exceptions disabled for
our research translation units.

Live execution then completes RWsSession::Connect with zero, but screen-device
construction panics E32USER-CBase/69. The original panic definition identifies
missing CTrapCleanup. Added SDK CTrapCleanup::New before GuiMain, allocation
failure handling and deletion on return. This uses frozen ordinal 196 and adds
one EUSER import. No target C++ exception runtime was introduced. The new build
has ten EUSER and 28 WS32 imports; the validator/package checks now require that
maintained profile. The old 37-import artifacts are retained privately.

A cleanup-enabled run reaches DrawGui at 0x700003ae. The maintained real GDB test
inspects count=0, running=true and 360x640 layout, then returns from the function
at 0x700002f0. Initially retained HAL/chunk breakpoints stopped again inside heap
setup and produced misleading marker PCs; deleting each completed breakpoint
before the next stage fixes the script, with PCs asserted at every claimed stop.
The new startup preserves more registers, so heap argument stack addresses move
by eight bytes; digest-pinned expectations are updated from the actual trace.
The default-profile heap failure and real profile-rejection cases still pass.
Partial source backtraces reach startup, but GDB warns at the raw ARM frame;
full unwind support remains unverified.

All 146 Pytest cases pass in 73.44 seconds with explicit optional inputs. All six
root CTest targets and the two new research targets pass. Original image checking
accepts the updated GUI in eight cases. An isolated installed wheel imports its
own extension, reproduces ELF/E32, packages the GUI and passes 17 historical/
installer cases with 38 unresolved slots in the ROMless missing-library control.
Firmware-specific execution is a separate opt-in frontend test, not a stronger
claim attached to that static/package verifier. Formatting and whitespace checks
pass. The final four live checks with model/layout assertions pass in 19.51
seconds. Logs, GTest JSON, disassembly, fresh copied instances, patch replay and
wheel evidence are retained under .symbian/belle-abi-research.

Original ZIP SHA-256 and all 13438 baseline paths/sizes/digests are unchanged.
No frontend or analysis process is intentionally left running. Captured pixels,
pointer/redraw delivery, normal SDK cleanup/exit, complete DLL initialization,
new private executive operations and broader firmware selection remain next work.
The existing 0x10D warning persists even though the drawing function runs. This
turn advances real GUI execution without declaring full platform compatibility
or completing the broader mission. No physical phone operation was performed.


## 2026-10-01 — Native captures/input/exit, CLion setup and plan review

Question: do the guest drawing calls create real visible output, can normal
Window Server pointer requests operate the model, and does the SDK cleanup/exit
complete? The original Android screenshot path supplied the graphics contract:
read_bitmap(screen_texture), 32-bit RGBA output. Qt encodes PNG atomically in a
private endpoint directory. Four-kilobyte native command bounds, logical input
validation, no-overwrite outputs and explicit final status cover accidental
misuse; only capture/pointer/status operations are exposed, with no hardware API.

The first build failed because the injected project() hook ran before upstream
selected C++20. Selecting/restoring the standard around pinned Abseil/native
configuration fixes it; direct emulator library dependencies supply required
headers. Existing CMake feature-check cache entries were cleared once for this
failed configuration. The final adapter and native tests compile with
-fno-exceptions; no Python bindings, GIL holders or second scheduler are needed.

control-live-01 captures the actual initial portrait frame, then pointer delivery
times out from holding the kernel lock across Window Server's own grab-window
lock. Releasing the validation lock before the existing frontend delivery path
allows count=1. control-live-02 additionally gets a real kernel-busy response;
the maintained test retries UNAVAILABLE only, never ambiguous timeouts.

The first full GUI tests render all expected frames on both backends and reach
ThreadKill reason zero, but frontend teardown crashes with SIGSEGV. The local
EKA2L1 diagnostic stack points to ControlServer::Impl::~Impl accessing the kernel
after the OS worker reset symsys. Stop now unregisters before kill_emulator and
saves native final status before the endpoint disappears. The next run exits
zero but the oracle expected an undecorated process name. The actual kernel
name is gui_app[e0000811]0001; the oracle now checks that exact record. No guest
startup, drawing or input code was replaced to satisfy these checks.

The final two-backend test passes in 8.83 seconds. It reads actual PNG segment
centers and control/background colors, checks 0→1→2→outside unchanged→0, then
normal process UID/type/reason and host exit zero. It additionally checks native
malformed/oversized input, unknown operations, invalid coordinates, traversal
and duplicate capture refusal. Native path/startup controls pass four GTests;
control/routing/register targets pass eleven total. Saved-status policy tests
require an explicit saved request and reject malformed/oversized envelopes.

The GUI's standalone preset lacked SYMBIAN_IMPORT_PROXIES because the CLI used
to supply them. Added the two staged proxies to the preset. CMake configure and
Ninja build pass from examples/gui_app; documented clangd check with
--tweaks=ExpandAutoType reports zero errors. An unrestricted check reports two
ExtractFunction refactoring failures at loop-control statements, with no target
parsing diagnostics. Canonical rebuild retains ELF 7b2918ba... and E32
2ef4145f..., matching the previous artifact digests. CLion menu/toolchain,
compilation-database fallback, E32 publication and remote ARM debugging are now
in docs/CLION.md. The CLion UI itself remains untested.

All 161 Pytest cases pass with explicit optional inputs in 73.69 seconds; all
six root CTest targets pass. The seventh guest-control patch replays after the
six existing patches against 15 fresh pinned upstream source files. Native
control is GPL-3.0-or-later with its license retained and excluded from the
production Python extension/wheel. An isolated installed wheel outside the
source import path reads the retained native final exit envelope and inspects
the GUI E32 with its native extension. Module paths, wheel digest and reports
are in control-wheel-result.json; development tests and the research native
adapter are absent from the wheel. Input integrity
rechecks all 13,438 baseline paths/sizes/digests and the supplied ZIP unchanged.
No physical operation ran; the golden is never booted.

Review: the vertical slices successfully replaced Windows build/package needs
for their limited subset, but private scripts/copies are not yet a complete
platform workflow. PLAN.md now preserves the broad mission while prioritizing
IDE onboarding, owned emulator lifecycle and symbian test, unresolved 0x10D/0xFF
and DLL lifetime, bounded writable data/TLS/runtime, then diagnostics and broader
application/system scope. Hosted C++20, OS boot and device compatibility are not
claimed. Private logs, PNGs, copied instances, final status and replay evidence
remain under .symbian/belle-abi-research.

Actual IDE setup: the running application is IntelliJ IDEA 2026.2.1 with the
CLion plugin, rather than standalone CLion. The application's normal MCP
endpoint returns an authorization error, and desktop Accessibility is not
available. No authorization mechanism was changed or bypassed. Saved a named
Symbian ARM toolchain in the plugin's primary mac/cpp.toolchains.xml storage,
retaining Default and backing up its old migration file. An ignored GUI
CMakeUserPresets.json specifies CMake/Ninja/Clang/LLD paths, Homebrew PATH, ARM
GDB through the named toolchain and its own build directory. That exact preset
configures/builds and clangd reports zero errors.

The normal application launcher opens the GUI project. Its first profile names
copied from A11 triggered a migration warning: this plugin version names build
profiles directly. Corrected the enabled profile to clion-arm and retained the
plugin's new-format marker. The project opens and CMake workspace reloads, but
logs still report zero resolved sources. Global toolchain/UI loading needs an
IDE restart and verification; no restart was forced into the user's open
session. docs/CLION.md records this remaining step rather than claiming the
live IDE analysis or remote debugger was validated. Settings and log evidence
are retained under .symbian/clion-setup; machine-specific files remain ignored.

The user confirms the live error “Toolchain Symbian ARM is not found”. This
matches the application-settings caching limit. Changed the local clion-arm
vendor selection to existing Default while preserving explicit ARM compiler,
linker, Ninja and SDK paths; terminal configure/build still passes. The saved
named toolchain remains available after restart for ARM GDB. Preset reload and
live target-model verification remain required; a terminal build cannot clear
this evidence gate by itself.

Live IDE follow-up: opening the changed local preset triggers its reload.
At 10:58:48 the IDE runs cmake --preset clion-arm with the correct GUI source
and build directory and gets exit zero. Its generated file-API gui_app target
lists startup.S, app.cc and startup.cc. The initial GUI model reports two
resolved C++ sources, one assembly source and zero unknown sources; at 10:58:56
it invokes the gui_app build. This resolves the missing-toolchain/target gate
for the running session without restart. Private ide-model.log and target.json
record the actual IDE evidence. Default remains the live frontend toolchain;
explicit CMake paths provide the ARM cross-build. The saved named toolchain
needs restart for application-settings loading; ARM remote frontend debugging
and editor-inspection behavior are still untested.

## 2026-10-01 — Owned IDE Run/Debug launches

The user additionally requests working Run/Debug buttons. Added synchronous
Python policy in symbian/emulator/launch.py, gdb.py and ide.py. Run publishes
current source through the existing native build/converter, fingerprints the
named fixture, copies the unbooted golden, launches one owned frontend and
retains a manifest/logs/instance. No new scheduler, native parser or hardware
API is introduced. Stop/interrupt reaps only children of this launcher with
bounded terminate/kill waits; native endpoint directories are removed. Normal
guest exit preserves the final native report.

Debug supervises a real ARM GDB child and a fresh halted frontend. Version/
configuration probes exec GDB directly without building/launching. A busy port
is refused before a new instance is created. The first experiment tried to
read runtime mapping before GDB connection, but the listener opens before the
GUI is loaded. The corrected hook runs after target remote and uses the actual
kernel mapping plus native-inspected link-time base. Source mappings and symbol
relocation then precede IDE breakpoint insertion. Batch source breakpoints reach
GuiMain at 0x7000002a and DrawGui at 0x700003ae; model inspection and instruction
step pass. A real MI2 conversation connects, inserts/hits the relocated source
breakpoint, reads PC and steps the Thumb BLX into the ARM constructor veneer.
The initial test incorrectly assumed linear stepping at that source breakpoint;
independent llvm-objdump shows BLX 0x8a50, matching runtime 0x70000a50 and cleared
CPSR Thumb bit. This is a test correction, not an emulator stepping change.

The installed IDEA/CLion plugin enables its new Debug Profiles feature by
default. Old Remote Debug debugger fields alone are ignored in that mode.
The generated local shared Symbian GUI GDB profile selects the supervisor and
the dedicated GUI project selects it. The root project's host debugger remains
unchanged. Run uses the native CMake run configuration with explicit Python
RUN_PATH, avoiding an invalid unregistered Python SDK. Generated GUI Run/GUI
Debug settings can be recreated with symbian emu configure-ide. The actual
IDE target model is verified; toolbar clicks, frontend inspections and complete
stack unwinding remain unautomated checks. Private launch/MI test evidence is
in .symbian/clion-setup; retained owned instances are in .symbian/gui-runs.

### Launcher shutdown and final checks

First full launcher suite: 168 passed, two failed in 125.46 seconds. Both
failures requested release after Exit-down had already removed the guest window
and initiated frontend teardown; the native guest and host exit evidence was
zero. Corrected the tests to issue down only. After application.exec() returns,
the control adapter also drains accepted pending output with a bounded 100 ms
wait per socket before destruction, without starting another event loop.

Second full suite: 169 passed, one failed in 140.63 seconds. Dynarmic's guest
ThreadKill reason was zero, but the frontend exceeded the 15-second wait and
cleanup killed only that owned process. No final native report was published.
This is distinct from the release-after-exit mistake. Added three native Stop
phase messages and a bounded /usr/bin/sample of the exact frontend before
cleanup on a repeat timeout. Twelve single GUI repetitions pass; then five
groups of all four guest-debugger and both GUI-backend tests pass (30 cases).
No repeated hang/stack appears. The timeout cause remains an open question;
do not infer it was fixed merely from subsequent successful runs.

Final full suite, with every optional input enabled: 170 passed in 112.82
seconds. The control/routing/register CTest targets again pass all eleven GTests.
After selecting GUI Run as the generated default, four focused installer/path/
ownership tests pass. The rebuilt wheel is installed in the isolated environment
and checked from /tmp: native E32 inspection, saved native exit envelope, GDB
version discovery without fixture startup and the default GUI selection.
.symbian/clion-setup/wheel-result.json records the current digest and module
paths. .symbian/clion-setup/input-integrity.json confirms all 13,438 baseline
paths/sizes/digests and the original ZIP unchanged; physical operations remain
zero. No additional native scheduler, Python holder or device API was added.

IDE indexing: the root content model exceeded two million files after retained
firmware copies accumulated. A11-style contentRoot alone did not exclude those
files and was removed. Saved CidrRootsConfiguration exclusions for .symbian,
research/upstream, build, out and .venv, with the staged gui-sdk unexcluded.
Live root exclusion loading is unverified. The dedicated GUI project reopened
at 11:39:10, scanned only 535 files, and still reports two resolved C++ sources,
one assembly/weak source and zero unknown sources. Keep that guest project as
the normal editing/Run/Debug context. Its RunManager lists GUI Run and GUI Debug
and its saved native profile is Symbian GUI GDB. Saved default selection now
chooses GUI Run; the automatic gui_app ELF configuration is not the launcher.
Actual toolbar interaction and the full debugger frontend remain unautomated.

## 2026-10-01 — Actual A11 status port, package structure and Linux host gates

**Question:** Can the tooling use A11's actual cross-language contracts and
support a one-install SDK without making every developer rebuild a host stack?

**Implementation:** Ported the actual A11 working-tree Status/StatusOr wrappers,
interop/GIL helpers, NativeStatus binder, payload/JSON/MessagePack/UTF-8 code,
Python status policy and HTTP tests. Recorded source hashes and A11 HEAD in
third_party/a11/provenance.json; retained Apache notices/licenses and the exact
payload URL. Built the original pinned pybind11_abseil canonical runtime modules,
isolated pybind11 headers from competing Abseil installations, and supplied
three missing direct Abseil link dependencies rather than relying on A11's
larger core to mask them. All core/codec GTests compile without exceptions; only
Python translation boundaries enable exceptions. JSON diagnostic reparsing was
removed and object-key UTF-8 preflight added to satisfy this policy. Actual
compact MessagePack and non-inverse WebSocket mapping contracts are documented
in A11_STATUS.md, including what the port does not silently fix.

Moved implementation from package __init__.py files into named modules, retaining
public shortcuts. Serializable CMake target/session metadata uses Pydantic;
process handles are excluded. The GDB hook moved to a dependency-free top-level
module: importing the emulator package from GDB had otherwise loaded a native
extension for the venv's different CPython ABI. A python -S regression and actual
GDB batch/MI checks cover that boundary.

**Linux experiment:** Adapted A11's isolated static-OpenSSL dependency bootstrap,
loader closure audit, CMake presets and separate native/wheel CI gates. Actual
Linux aarch64 manylinux_2_28 CPython 3.12 container built seven native CTest targets
and an installed wheel, passing closure/resource/status checks and 62 Pytest
cases. The bootstrap initially exposed a missing Perl Time::Piece prerequisite;
that is now checked/installed. Testing from /src initially shadowed the installed
package; copied tests outside the source tree establish installed imports.
The final macOS arm64/macOS 26.0 wheel also passes its installed audit from /tmp.
Wheel notices include A11, Abseil, JSON, pybind11, pybind11_abseil and OpenSSL.
The Starlette/httpx deprecation warning remains visible in the copied HTTP tests.

**Distribution design:** docs/DISTRIBUTION.md separates the native Python core
from exact-version platform payload wheels containing materialized headers,
compiler/linker, EKA2L1 and a complete debugger closure. Measured local emulator
archive is approximately 41 MB and has no non-system absolute Mach-O dependency;
its actual maximum minimum-OS requirement is 26.0. Copying Homebrew LLVM/GDB
executables alone would omit large required closures. No firmware is bundled.
Canonical pybind11_abseil co-installation/ABI ownership with A11 is a release gate.
Only the host tooling wheel is built today; payload wheels and public publication
are not implemented. Linux x86_64/other CPython, Qt/display/guest/GDB and a clean
host end-to-end SDK install remain distinct gates.

## 2026-10-01 — Combined root CMake, executable Run target and Qt boundaries

**Experiment:** Added the real ARM gui_app target to the host CMake project with
directory-scoped target flags/link rules. CMake expanded <CMAKE_LINKER> using the
root native driver despite a local variable override; spelling the discovered
ELF LLD path in the guest directory's link rule fixed the real failed links.
ARM ELF and native Mach-O targets build in one graph; the root compilation
commands contain the appropriate target triples independently. Root clangd
parses/indexes the GUI with zero compiler diagnostics. Its default ExtractFunction
tweak self-test failed on break/continue; restricting the check to ExpandAutoType
completes with zero errors, matching the maintained walkthrough check.

**Run fix:** A custom gui_app_run build target was not a CLion application.
Replaced it with a native executable that execs the owned Python supervisor.
The root target writes a stable build/debug/gui_app_run artifact. Saved root GUI
Run explicitly selects it (empty application arguments), because the IDE retained
an empty executable field for the former custom target. Root/standalone run and
remote-debug settings are generated, preserving the root's existing host debugger
selection. Actual launcher rendering/input/normal exit and GDB batch/MI relocation
checks pass. UI reload/Run/debugger interaction remains distinct from terminal
verification. No IDE authentication or accessibility settings were bypassed.

The full regression initially exposed four stale hard-coded GUI artifact digests
following legitimate owner edits. Tests now verify the publisher's current source
hashes and actual ELF/E32 conversion, while retaining exact ROM/EUSER guards,
independent pixel/input behavior and firmware call controls. Compiler veneer
addresses are read from the actual disassembly; ephemeral caller locations are
checked against mapped code bounds rather than the older app's layout.

**Types:** Removed QJsonObject/QJsonDocument and internal QString state from the
maintained GPL control adapter. Requests/records use nlohmann::json, paths and
messages use std::string/std::filesystem; QString/QByteArray conversions occur
at Qt APIs. The real frontend/control probe rebuild and native control tests pass.
Ordered native serialization/export tables remain ordered for reproducibility;
future lookup structures follow A11's Abseil choices. No new scheduler was added.

## 2026-10-01 — First executable guest C++ runtime

**Question:** Can real modern strings/containers use Symbian's own heap while
preserving a strict no-exceptions application profile?

**Sources:** Unmodified LLVM llvmorg-23.1.2, commit
85ac560262434c9ccfc0c183ec22d4138ed647fb, plus already-preserved OpenC C declarations
and selected EUSER frozen exports. The ignored LLVM checkout is pinned and clean;
our adapters/configuration live in cpp/symbian/runtime. Future LLVM changes must
be explicit patches under research/llvm, as EKA2L1 changes already are.

**Findings:** OpenC's C99/long-long feature macros are needed by original
libc++ string.cpp. Combining preserved SDK e32cmn placement-new declarations/
inline definitions with libc++ <new> produces real incompatible specifications
and duplicate definitions. Isolated SDK translation units behind a small C ABI
solve this without modifying upstream headers. A global std::nothrow reference
produces R_ARM_GOT_PREL and a .got table: native conversion correctly rejects it.
A maintained rejected-global variant preserves that gate. The allocation probe
uses a genuine local nothrow_t tag; no standard classes or discarded sections
substitute for the required runtime machinery.

**Implementation:** Generated target configuration from LLVM's original template,
private std::__symbian ABI namespace, original assertion handler, original
string.cpp/new_helpers.cpp, and a static Symbian heap/ABI bridge. Ordinary new
uses User::Alloc and exits the guest with -4 on failure; nothrow returns nullptr.
Zero sizes request a byte; TInt overflow is rejected before the SDK call. Default
alignment is checked. Minimal reached C/compiler memory entry points are supplied,
with memory copying routed through actual EUSER. No OpenC DLL or host libc++ is
linked; unimplemented services are not claimed.

**Execution:** Four maintained Pytest cases execute success and fatal allocation
variants on dyncom and dynarmic using fresh digest-guarded firmware copies.
Success checks 20 rounds of heap-backed strings and 1,024-element vector growth,
independent arithmetic and unchanged heap allocation-cell counts after destruction.
Oversized nothrow fails at the bridge range guard. A representable 4 MiB request
exceeds the image's 1 MiB heap maximum and exercises actual SDK allocation failure;
nothrow returns nullptr and ordinary new exits with -4. Native exit records and
logs are retained; ROM/EUSER digests remain unchanged. The original Symbian
validator accepts the published runtime image and rejects bad-CRC/negative-heap
controls (three GTests).

**Limits/next steps:** GOT, writable data/BSS, constructors/destructors, TLS,
complete compiler-rt/C services, over-aligned allocation and new-handler behavior
need explicit implementation/execution gates. Guest Abseil/StatusOr, maps and JSON
need ports; the full host bridge does not supply guest runtime support. The GUI
has not yet been converted to use this runtime. No physical-device execution
has occurred. PLAN.md makes runtime a required next priority; docs/RUNTIME.md and
CXX_CAVEATS.md explain the developer-facing contract and distinguish OS principles
from work the SDK should absorb.


## 2026-10-01: Standalone project wizard, visible SDK and actual IDE import

**Question:** Can a project created outside the checkout build, resolve native
and modern C++ headers, and launch/debug through its own selected SDK?

**Implementation:** Added Pydantic project preferences/location models and
`symbian init`, `sdk install`, `app configure/build/run`. Target SDK exports
materialize original headers, pinned configured libc++, compiler resource/C
headers, runtime archive and selected EUSER/WS32/GDI proxies. The visible prefix
also carries actual Python utilities and native modules, all host-native
notices, provenance/digests and tool wrappers. Host interpreter/packages,
LLVM/LLD, ARM GDB, EKA2L1 and guarded private firmware remain declared external
prerequisites. This is not the finished one-install host payload distribution.

Shared project files stay relative; sdk-location.json is the ignored local
setting. CMake includes it before project() and links Symbian::Runtime for the
actual header/ABI configuration. Init builds initially. Package entry points
redirect to selected SDK commands/templates. A Python bootstrap prevents an
editable import finder from silently overriding the visible SDK package.
SDK installation clones/rebases an active export; a direct directory move still
requires manifest rebasing. Configure refreshes generated build/IDE files and
preserves app sources, but custom generated-file edits need preserving first.

**Runtime findings:** Real TFontSpec/DrawText needed the platform's 16-bit
wchar_t ABI; incorrect literal length caused a native typeface panic. Original
SDK placement-new declarations still conflict with modern libc++ <new>, so the
model crosses a narrow C bridge into the W32 adapter. LLVM sources stay clean.
Original compiler-rt ARM division/remainder sources and checked over-aligned
allocation now link the -O0 model and execute in the runtime probes. Ordinary
OOM exits -4 and nothrow returns null; no recoverable guest StatusOr claim.
Global std::nothrow/GOT remains a rejected control. PLAN records the actual A11
completion/stackless/pinned-fiber/worker delivery gates and the exception/GIL/
deferred-reference conflicts that require real backend work.

**Execution evidence:** Both emulator backends render original font greeting
and one/two time rows, clear, then exit guest/frontend zero. The saved Run
executable itself is launched from a project outside the checkout. Real ARM GDB
stops in AppLogTime and reads date/source/backtrace. A copied visible SDK, moved
project, relative SDK setting and initial CLI build pass; a modified copied SDK
template proves init dispatches to that selected SDK. Modern model GTest retains
the latest twelve rows, clears, reuses and destroys the model. Golden digests
remain checked; no physical operation ran.

**Failed controls and corrections:** A screenshot ink threshold incorrectly
rejected antialiased font pixels and prolonged tests; use real-ink row checks
with retained images. One dyncom Exit press was lost while an unnecessary app
pointer-down latch was present; removing that latch restores the raw native
per-down handling. Later success does not establish the earlier event loss's
cause. Regression checks also rejected a cached root ELF built by an older
Apple Clang behind the same /usr/bin/clang++ path. DWARF producer strings show
clang-2100.0.123.102 versus clang-2100.1.1.101. Cache identity now includes actual
compiler/linker versions; a same-path compiler-update failing control and the
root Run/Stop/batch/MI tests pass. No reproducibility check was weakened.

**Actual IDE correction:** Terminal builds and zero-error clangd checks alone
were insufficient: the owner's new screenshot still showed an invalid Run
configuration. IDEA logs reported JAVA_MODULE and no CMake workspace; native
Debug chose a nonexistent bundled GDB. Generate .idea/misc.xml CMakeWorkspace,
a CPP_MODULE descriptor/module entry and per-run SelectedDebugProfileService
mapping, as well as the enabled preset and saved host executable. Preserve
existing module/workspace components. Repair app, app-2 and app-3 without
replacing user source or UID. Reopening app-3 produces actual successful CMake
configure and RAD model 3 sources/1 weak/0 unknown (13:41:12 host log). This
establishes IDE import, not an automated toolbar click or full debugger frontend.

**Checks:** Full optional-input Pytest run: 202 pass, one inherited Starlette
warning; subsequent project/IDE/build group: 13 pass. Root CTest: eight targets
pass. Linux aarch64 current host build: seven CTest targets, installed wheel
resource/closure audit and 62 installed Python cases pass. Linux guest/emulator
and broader hosted C++20 support remain separate gates. Mac installed wheel
includes starter/CMake/bootstrap resources and passes audit. Black/Ruff, native
formatting excluding owner edits, stub regeneration and whitespace checks pass.
Private logs are under .symbian/distribution-research; no generated firmware or
runtime instances are versioned. Replay is in docs/PROJECTS.md.

The current IDE also combines configure/build preset names (symbian-pic -
symbian-pic), invalidating a Run configuration keyed only to symbian-pic.
Use a stable ordinary `Symbian App` IDE profile with the same CMake target
configuration, while retaining CLI presets. The saved executable from the
actual owner app-3 also launches its emulator and exits zero with unchanged
golden inputs. Existing open IDE state must reload the new profile metadata.

The actual open IDE reload at 13:46:49 also configures the stable Symbian App
profile successfully. The Run configuration now references that exact profile;
SDK-debug version discovery uses the declared ARM GDB without starting a VM.

## 2026-10-01: Local GOT execution and generated model allocation failure

**Question:** Can the rejected global std::nothrow control become an actual
supported image contract without dropping its GOT, relaxing failures or
introducing Python format logic?

**Baseline:** Replayed the prior four runtime cases with the prepared inputs:
four passed, with global-nothrow correctly failing publication. Retained the
linked failing ELF under `.symbian/runtime-got-baseline`. LLD emitted a writable
four-byte `.got`, one retained R_ARM_GOT_PREL (0x60), and no ABS32 record for
the generated word. The existing eager-import GOT/PLT is a separate table.

**Implementation:** Native conversion accepts one aligned local `.got` up to
4,096 bytes in the existing RX mapping. Every word must exactly equal a
defined object/function value named by retained GOT_PREL, and each referenced
value must have a slot. Symbol values must belong to their declared section.
The converter emits text fixups for every slot, preserves Thumb state and
the linked PC-relative reference words, and rejects missing/unclaimed words,
undefined objects, unsupported symbol kinds, duplicate/malformed tables and
retained records targeting the table itself. General writable data/BSS/TLS,
external data imports and other GOT/dynamic contracts stay rejected.

**Execution:** A new separately compiled probe constant and Thumb callback
ensure the test really dereferences/dispatches through the GOT; the empty
nothrow tag alone would not observe a bad argument address. Ordinary success,
ordinary allocation failure, global-nothrow/GOT success and changed-GOT-value
failure all execute on Dyncom and Dynarmic: eight pass, respectively reasons
0/-4/0/-113. Logs, exits and two-build input-hashed ELF/E32 reports are retained
under `.symbian/runtime-got-execution`. Six new native GOT GTests cover layout,
coverage, state and rejection cases. All eight root CTest targets pass. The
unchanged original whole-image validator accepts the new image and passes six
corruption/contract controls (seven GTests).

**Application integration:** Generated AppCreate uses `new (std::nothrow)`.
The W32 adapter checks null, closes the window/group, releases the font and
returns KErrNoMemory; startup then exits -4. Both CPU backends pass a real
4 MiB request against the image's 1 MiB heap limit. The fifteen-case project
generation/build group also passes normal pixels/input/clock/clear/exit, saved
host launcher, SDK copy/relocation and terminal ARM GDB. Logs are under
`.symbian/runtime-got-projects`. Later string/vector growth remains fatal on
allocation failure. This is not a guest StatusOr allocator or IDE-button proof.

**Visible SDK preservation:** All 2,408 prior SDK file digests matched before
refresh. Prepared a fresh complete export, retained the original tree at
`~/dev/symbian-sdk-before-local-got-20261001`, rebased/sealed/activated the new
`~/dev/symbian-sdk`, and verified its native converter reproduces the executed
GOT E32 bytes. Final converter hardening was synchronized with a checked native
module replacement and resealed digests. No owner application source, UID,
generated configuration or IDE module was rewritten. The first smoke command
used stdin `-`, which the SDK Python bootstrap currently does not implement;
the supported `-c` replay passes. SDK update/integrity records are retained.

**Regression-input correction:** The first full optional-input run supplied
the raw LLVM source include directory to the older standalone header-only
experiment. That experiment's recorded input is the installed
`/opt/homebrew/opt/llvm/include/c++/v1` plus its separate baremetal configuration;
source headers need generated vendor files supplied by the runtime build. The
ten-case C++20 replay with the recorded installed-header input passes. The
failed full-run artifacts/log remain in `.symbian/runtime-got-full*`.
That run had 206 passes, one failure and one fixture error, both for the
header experiment's missing generated assertion handler. The corrected final
full run passes all 208 cases with every optional input enabled and no skips
in 181.74 seconds; one inherited Starlette deprecation warning remains visible.
Its log is `.symbian/runtime-got-final-pytest.log`, with artifacts under
`.symbian/runtime-got-final`. Black/Ruff, changed C++ formatting and
`git diff --check` pass. No reproducibility or rejection check was weakened.

**Integrity and limits:** Rechecked all 13,438 baseline paths/sizes/SHA-256
values and the original archive SHA-256 unchanged, plus all 2,408 current SDK
file digests. LLVM sources and emulator patches are unchanged. No physical
operation ran. The archive does not identify the owner's phone, and an
independently held offline preservation copy remains unresolved. Next runtime
gates are writable data/BSS, global lifetime/TLS and the remaining service/library
closure; A11 concurrency still depends on those prerequisites.

## 2026-10-01: shared firmware onboarding and multiple generated-app devices

**Implementation:** ROM/Z objects have portable identities derived from native
metadata and full relative-path/hash inventories. Storage defaults to XDG data,
with retained import/failure evidence in XDG cache. Global, SDK, project and
command keys merge with defining-file path resolution and per-key origins.
Explicit null clears selection; unset resumes inheritance. SDK exports no longer
bind firmware to a workspace, and build/init do not require emulator inputs.
Projects pin exact IDs; local aliases and offline verified bundles are supported.
One resolver drives Run and Debug, strips ambient executive overrides and records
actual ROM/Z/C mappings/profile. Firmware parsers remain original pinned native
EKA2L1 implementations, in a separate GPL no-exceptions executable, not Python.

**Execution evidence:** Native imports accepted C7 (RM-675), E6 (RM-609), 6120
(RM-243), E71 (RM-346), 7610 (RH-51) and Sony Ericsson P900 across archive ROM/Z,
ROM/RPKG and self-contained ROM forms. C7/E6/6120/E71 each pass actual CLI init,
initial build, rendered greeting, time-row input and reason-zero native/frontend
exit with the default executive map. The modern string/vector model runs on
these devices without historical feature gating. EKA1 startup/import ABI is
still absent; 7610/P900 import successfully but reject the current application
ABI before creating run output. Existing 808 workflows still pass; its guarded
profile is selected only for the exact verified ROM/EUSER pair.

**Failures retained and corrected:** The first E71 preflight counted an empty
QuickOffice data file named registration.rom within Z as another firmware ROM.
The native candidate resolver now distinguishes Z data from image candidates,
with a GTest. Initial multi-device capture loops reused one basename (correctly
rejected ALREADY_EXISTS), then assumed scale 2. Native capture now reports logical
width/height and actual scale, and tests wait for greeting/time ink independently
of endpoint readiness. RM-243 rendered and logged time but stalled in DLL teardown:
FBS opcode 0x2C had no handler and left a synchronous request pending. The separate
replayable fbs-unsupported-request.patch returns KErrNotSupported, preserving the
error/log instead of inventing an operation; the caller now completes teardown.
An initial VPL attempt omitted the native installer's required roms staging
folder; after creating it, the fresh Delight VPL import passes. Both failed VPL
and original 6120 hang/captures remain retained. Upstream warnings/errors now go
to retained stderr rather than disappearing behind an uninitialized logger.

**Validation:** Five native GTests exercise traversal/case collision/multiple
candidates/expansion and original RPKG reader malformed-name/truncation controls.
The firmware acceptance group passes 26 cases. Final full optional-input Pytest:
234 passed, one inherited Starlette warning, no skips in 335.11 seconds, artifacts
`.symbian/firmware-full-verified`, log `.symbian/firmware-full-verified.log`.
Eight root CTest targets pass. Explicit missing-Z returns FAILED_PRECONDITION;
profile mismatch, tampered/extra files, alias replacement, bundle transfer,
configuration precedence and reaped timeout are maintained controls. Original
archive inputs were checked before/after native imports. New upstream bounds and
FBS patches replay/reverse exactly against the pinned base; the replay record is
`.symbian/firmware-patch-check.json`. The existing original SDK digest inventory
has no modified or extra files before its deliberate refresh.

**Limits:** Default-backend execution above is Dynarmic; the existing 808 CPU
backend and debugger controls remain independent evidence. Import is not OS boot,
all-device ABI support, IDE toolbar interaction or physical compatibility. Missing
services remain real errors. Full supported headers/proxies, DLL lifetime, TLS,
A11 concurrency and host payload closure still need their acceptance gates.
Hardware identity and independent offline preservation remain unresolved.


### 2026-10-01 — bounded writable EXE data/BSS and genuine A11 source staging

Continued after the ROM/Z onboarding gate. Added an independent RW PT_LOAD
transport capped at 1 MiB, initialized data/BSS header fields and typed 0x1000 /
0x2000 fixups from either source mapping to code/data. Local code GOT slots can
point into writable storage; PC-relative references across independently
relocated mappings remain errors. DLL data, TLS and constructor arrays remain
unsupported. No LLVM source changes or sections dropped to obtain acceptance.
The new-project and runtime-probe linker scripts now emit the separate layout.

The real guest probe checks 64 BSS words begin zero, initialized value 2026,
pointers to initialized data/BSS/Thumb code, mutation and subsequent observation.
Both CPUs exit zero; altering initialized data to 2025 exits -115 on each.
The original success/-4/global-nothrow/-113 controls still pass. Twelve execution
cases plus two original checksum/whole-image-validator tests pass: 14 tests in
57.97 seconds, `.symbian/runtime-data-oracles.log` and adjacent retained builds,
frontend logs and native exits. Four new native format/typed-fixup acceptance
and negative tests pass; the eight root CTest targets pass. Pure-BSS transport
has native format coverage only, separately from the real mixed-storage guest.

Pinned actual A11 sources from `fcccb8cb6e1e67d7ba0822ac14cece9ee4c7091b` under
`cpp/symbian/concurrency/upstream`, retaining exact paths/license and all 46
source digests. Their inspected working-tree bytes matched Git. Closed 92 local
include edges, inventory external headers, and preserve original Future/thread
semantic tests and build declarations. The standard-library-only checker
verifies inventory/digests/edges; five tests detect altered/missing/extra sources
and altered graph. CMake source-check and comparison against the original Git
pin pass (`.symbian/a11-concurrency-source-check.json`). The staged Boost backend
is deliberately absent from the SDK link graph; this is not a built backend or
a completed C0 gate. Upstream primitives/pool forced-unwind and boundary
exceptions require a genuine no-exceptions backend adaptation. Guest Abseil,
shared ownership/atomics, static lifetime, thread/TLS and native completion owner
remain prerequisites; application async examples wait for actual execution.

Visible SDK firmware refresh verified all 2,410 payload digests and preserved the
previous tree at `~/dev/symbian-sdk-before-firmware-20261001`. Existing app-3
resolution now reports the shared exact 808 content ID, defining layers and
ROM/Z/C paths; no application/UID/IDE settings were regenerated. Firmware data
is outside the SDK. `.symbian/firmware-sdk-update.json` records publication;
`.symbian/firmware-owner-project-resolution.json` records resolution evidence.

### 2026-10-01 — ARMv6 profiles, IDE compiler probe and DLL data directive

New-project target selection defaults to ARMv6 and records one preference in
`symbian-project.json`. The CMake toolchain reads it before `project()`; the
CLI passes the same selection. Each architecture has a separately built runtime
archive. Existing generated projects without the setting retain ARMv5T. The
ELF attribute parser and E32 publisher validate CPU, floating-point ABI,
Thumb generation and SIMD declarations rather than trusting requested flags.
An unsupported image is rejected before a guest session is created. The root
mixed host/guest CMake file API supplies `--target=armv6-none-eabi` to CLion's
guest compiler probe; the native tooling probe remains Apple arm64. Both
probes preprocess successfully with the saved CMake fragments, the root GUI
build publishes an ARMv6 E32, and eight root CTest targets pass. The 28-case
runtime matrix executes real ARMv6 `REV` and ARMv5T software variants on
Dynarmic/Dyncom, including original format/validator controls. Retained logs:
`.symbian/runtime-arm-matrix-verified.log`,
`.symbian/root-architecture-compiler-probes.json`,
`.symbian/arm-architecture-last-tests.log`, `.symbian/arm-last-ctest.log`.
None of these proves the unidentified physical Nokia 808's CPU profile.

Checked `EPOCALLOWDLLDATA` in the pinned original build tools. The MMP parser
sets `AllowDllData`; original `elf2e32`'s `FinalizeE32Image()` rejects a DLL
with nonzero `iDataSize` or `iBssSize` unless that option is set. Sources:
`research/upstream/buildtools/sbsv1_os/e32toolp/makmake/mmp.pm:987` and
`research/upstream/buildtools/toolsandutils/e32tools/elf2e32/source/e32imagefile.cpp:925`.
The EKA2L1 process creates writable DLL static-data memory, but loader fixups,
per-process separation and initialization/destruction still need direct
execution tests for SDK-produced DLLs. The directive is a converter permission,
not a substitute for those mechanisms or for EXE data relocation. Our bounded
EXE data/BSS work requires no historical opt-in. The current converter keeps
writable DLL data unsupported pending an actual lifecycle implementation.

### 2026-10-01 — macOS emulator focus and first static-library target

Direct `.app/Contents/MacOS/EKA2L1` launches took focus even after Qt
`WA_ShowWithoutActivating`, AppKit `orderBack`, an accessory activation policy,
`open -g`, and Qt's foreground-transform override were tried. Those failed
trials and disposable instances are retained in `.symbian/gui-runs` and
`.symbian/open-background-*`. Running the same pinned binary through a private
symlink outside its `.app` bundle kept iTerm2 PID 757 frontmost during a real
owned GUI session. The 720×1280 native screen capture completed and the child
was reaped. `symbian.emulator.background` now creates that per-session link on
macOS. The ordered GPL `background-window.patch` also suppresses explicit
Qt/OpenGL focus calls for controlled sessions; pinned-base replay and
applied-state reverse checks pass. Direct test launchers use the same path.
The bundle trial changed only ignored build-product Info.plist, and its trial
key was removed afterward.

The user also needs new emulator windows on a desktop Space rather than over
their full-screen IDE. The OpenGL patch now assigns a normal managed window
with `FullScreenNone` and `FullScreenDisallowsTiling`; Apple documents
`FullScreenAuxiliary` as joining a full-screen window's Space. Qt's show path
suppresses initial activation but does not forbid deliberate later focus.
This policy is implemented and replayable; visual placement with another app
already full-screen remains unverified because switching the owner's active
Space would disrupt their work. The 720×1280/focus check was on a desktop
Space, not a full-screen-placement proof.
After removing the permanent `WindowDoesNotAcceptFocus` flag to retain
deliberate click-to-focus, the emulator rebuilt. The Dynarmic counter GUI
render/input/exit integration passed in `.symbian/background-space-gui.log`
(1 passed, 1 deselected, 5.06 s); `NSWorkspace.frontmostApplication` reported
Chrome PID 60698 before, during and after that exact run.

An SDK-owned background emulator window could show the guest display while
its controls remained in the unavailable macOS application menu. The Qt
`QMenuBar` was present in `mainwindow.ui`; only its native placement was
wrong for background sessions. `background-window.patch` now calls
`setNativeMenuBar(false)` before showing that window, keeping its six menus in
the window. The pinned patch reverses cleanly against the applied checkout,
the `eka2l1_qt` target rebuilt, and the real RM-807 counter GUI regression
passed Dynarmic and Dyncom (2/2, 13.62 s). CoreGraphics listed the live
896-by-754 emulator window, but `screencapture -l` returned “could not create
image from window” in this shell session, so a captured visual menu check
remains open. Foreground console launches still use the native macOS menu.

The first installed static-library helper builds ARM archives with matching
runtime headers/ABI flags and stable source DWARF paths. Initial integration
found Apple's host `ar`/`ranlib` produced an archive LLD could not resolve.
The SDK now declares and wraps LLVM `ar`/`ranlib` and includes their versions in
toolchain identity. A second failure exposed build-tree paths in archive DWARF;
the helper maps those paths consistently. Both ARMv5T and ARMv6 tests build a
generated application, link an archive, convert to E32 and locate its function
in ELF/DWARF; two tests pass in `.symbian/static-library-target-tests.log`.
General DLL loading, writable storage, module symbols and standalone library
project UX remain separate gates.

The complete optional-input suite then passed 270 tests with one inherited
Starlette warning in 428.58 seconds (`.symbian/arm-full-background-verified.log`).
The tested SDK export's source files matched the checkout; its 2,420 recorded
payload digests and the old visible SDK's 2,410 digests matched before the
visible path was replaced. The previous tree is retained at
`~/dev/symbian-sdk-before-arm-profiles-20261001`. The installed CLI resolves
E6, E71, 7610, C7 and 808 aliases to portable content IDs. An installed-SDK
`symbian init` for E6/RM-609 on ARMv6 created a standalone application and
completed its initial ELF/E32 build. The generated project records the
portable firmware ID and visibly selects `~/dev/symbian-sdk`; resolving it
without overrides reports the project emulator.json as firmware origin.
The disposable project and output are retained at
`.symbian/installed-e6-smoke*`. This is a build/selection result for E6, not
an E6 emulator-run result; separate earlier copied-instance tests cover the
actual guest launch.

### 2026-10-01 — Bounded C++ global lifetime and Belle DLL attach

The SDK runtime now retains linker constructor/finalizer arrays and supplies a
bounded 256-entry `__cxa_atexit` registry with per-module `__dso_handle`.
EXE startup calls initializers after `User::InitProcess` and the thread heap,
then finalizers before exiting. The real `std::string` global probe checks two
constructor and reverse-destructor events plus heap-cell balance. The four
ARMv5T/ARMv6 × Dynarmic/Dyncom named-firmware runs exit zero. Allocation failure
still exits -4; no recoverable guest StatusOr allocation API is claimed.

The installed-SDK C++ DLL default uses an SDK ARM entry, linker layout,
matching runtime archive and EUSER proxy. The first live DLL client returned
-122: EKA's static call list contained EUSER, the new DLL entry at 0x70001000,
and the EXE entry, but Belle SVC 0x10D was unimplemented. Mapping the existing
`library_entry_call_start` handler at Belle's shifted 0x10D slot advanced to
the DLL entry and exposed a real guest KERN-EXEC 3 at address 0x81C8. LLVM's
ARM-to-Thumb branch thunk contained the unrelocated absolute target 0x81C9.
The SDK entry now uses an explicit `R_ARM_ABS32` target word; the native E32
image contains the code relocation at offset 24. The same guest returned
constructor state 12 after the loader applied that fixup. The ordered GPL
`belle-library-entry-start.patch` records the guarded emulator mapping; the
remaining loader extension stays unimplemented.

`symbian/tests/test_guest_dll_lifecycle.py` builds SDK C++ DLLs and import
clients for both target architectures and runs them on both emulator backends.
All eight cases passed in 42.10 seconds: normal constructor value 12 exits
zero, and changing only the constructor to 13 exits -122. Each copied instance
uses unchanged digest-checked RM-807 ROM/EUSER inputs. DLL detach/destruction,
local-static guards, TLS, hidden writable globals and Mbed TLS execution remain
unverified. Artifacts of the failure diagnosis are retained under
`.symbian/dll-lifecycle-exec-dynarmic`; Pytest retains its copied instances.

The user's renewed focus report was reproduced by ad hoc launches: a private
nonbundle symlink plus `EKA2L1_RESEARCH_BACKGROUND_WINDOW=1` alone still changed
the frontmost macOS application. The normal SDK launcher already also set
`QT_MAC_DISABLE_FOREGROUND_APPLICATION_TRANSFORM=1`; the ad hoc probe had omitted
it. With both settings, `lsappinfo front` remained the same throughout a live
DLL execution. The shared `background_environment()` now supplies both settings
to SDK and direct test launchers. Temporary AppKit/Qt activation experiments
were removed from the upstream checkout; the existing ordered window patch is
unchanged. This is a desktop-Space focus check, not a full-screen-Space proof.

Follow-up acceptance: 28 guest runtime execution cases passed. Four original
validator cases initially failed at a test-only exact BSS-size assumption:
the C++ destructor registry adds 3,076 bytes to the prior 256-byte probe array.
The test now checks that the probe's required minimum exists; all four original
validator cases pass. Fourteen generated-project/static-and-dynamic-library
tests and eight root CTest targets also pass. The 12 ordered GPL emulator patches
apply from the pinned base in a fresh worktree and the last reverse-checks.
The visible SDK was deliberately re-exported: its 2,427 sealed payloads and
the prior 2,422-file backup verify without changes. A fresh constructor run
against that visible SDK passed. No owner application source or UID changed.

### 2026-10-01 — ARM wide arithmetic and dynamic DLL close

The original LLVM compiler-rt ARM `__aeabi_ldivmod`/`__aeabi_uldivmod` wrappers
were added to the guest archive alongside original `divmoddi4.c`,
`udivmoddi4.c` and `clzsi2.c`. The first build failed on undefined `__clzsi2`;
including that real transitive source fixed the link without an owned arithmetic
stub. Volatile signed and unsigned 64-bit division/remainder and a changed
expected-remainder control execute in the runtime probe. The maintained suite
passed 32 named-firmware cases in 225.63 seconds on both ARM profiles and both
CPU backends. Four optional original-validator cases were skipped in this run.

A dynamic `RLibrary` client first exited with reason 0x410F28. ARM GDB showed
the `Load` ordinal resolving to real ROM EUSER Thumb code at 0x804E395D, but
returning the unchanged client `this` pointer. Retained EKA logs identified
unimplemented Belle executive call 0x10E before the loader-server request.
Mapping that observed slot to the emulator's existing v10 load-preparation
hook allowed the real subsequent load, lookup, attach and detach path to run.
The DLL publishes a setter for a client-owned destructor sink; after `Close`,
the client reads 34 from that sink. An absent-DLL control gets KErrNotFound
(-1) and exits -121, so the mapping is not treated as a universal load-success
stub. The maintained 16-case constructor/dynamic-load/absent-DLL matrix passed
in 94.21 seconds. The thirteenth GPL patch replays after the first twelve from
the pinned EKA2L1 base and reverse-checks against the applied checkout. Local
diagnostic logs and GDB scripts remain ignored under
`.symbian/dll-lifecycle-dynamic-20261001/`.

The four optional original checksum/whole-image validators subsequently passed
on the new wide-arithmetic runtime images. A fresh installed SDK dynamic-DLL
case and a fresh generated-project initial-build/copy/move case passed. Both
old and new visible SDK trees have 2,427 verified digest-sealed files; the old
tree is retained as `~/dev/symbian-sdk-before-wide-dll-20261001`. The new ARMv6
archive defines all three added EABI/helper entry points.

The optional Mbed TLS adaptation from the user-owned external project compiled
its real C archive against the refreshed SDK. The maintained test built the
E32 DLL, imported real EUSER `RLibrary` ordinals into a client, loaded the DLL
at runtime, called `MbedSha256` through frozen ordinal 1 and checked the full
SHA-256 digest of `abc`. Replacing `c` with `d` returned the changed-digest
control -132. The static/guest suite passed five cases in 37.12 seconds, with
fresh digest-checked RM-807 instances on Dynarmic and Dyncom. This does not
claim the full Mbed TLS feature set or physical-device operation.

Mimalloc was considered as a future guest allocator backend. Its documented
per-thread heaps, concurrent frees and OS page reserve/purge behavior require
a Symbian memory-source and thread-lifetime adapter. The 2026-10-02 experiment
below adds that optional adapter; the existing allocator remains the default.

### 2026-10-01 — Native fast-lock prerequisite for A11

The original Symbian `RFastLock` is a semaphore-backed fast path. Its original
ARM source uses SWP for ARMv5 and LDREX/STREX when supported, with a kernel
semaphore fallback on contention. A11's actual `thread::Mutex` instead wraps a
Boost.Fiber mutex and records the owning fiber for diagnostics. Replacing that
fiber-aware wait with blocking `RFastLock::Wait` on the UI/event OS thread would
halt all fibers and native completions. The backend therefore needs an OS-thread
lock and a fiber-aware park/wakeup path under A11's interface.

A narrow SDK-header C++ bridge now imports the original `RFastLock` create,
poll, wait and signal functions plus handle close. The guest checks success,
`KErrTimedOut` while held, release/reacquisition and close. Four named-firmware
ARM-client/backend cases passed in 71.31 seconds. The RM-807 ROM supplies the
lock implementation in every case, so this does not independently execute an
ARMv5 ROM's SWP implementation or a contended two-thread path. No A11 mutex or
fiber switch is claimed yet.

A direct `std::make_shared<std::string>` A11 prerequisite build failed at link
on missing libc++ `__shared_weak_count` definitions. Original LLVM
`libcxx/src/memory.cpp` contains these definitions, but its refcounting path
depends on `_LIBCPP_HAS_THREADS`; the SDK's current generated config disables
threads. Adding that translation unit alone would silently give the genuine
A11 Future/Promise shared state the wrong thread-safety contract. The C0 gate
therefore remains open. The staged A11 46-source/92-edge pin still verifies.

The standard EUSER proxy in a fresh development SDK export now carries the
real ROM `RLibrary` and `RFastLock` ordinals. All 2,427 exported file digests
verified. The installed-proxy fast-lock matrix passed four ARM/backend cases
in 68.70 seconds, and the Mbed TLS SHA-256 dynamic DLL suite passed five cases
in 43.80 seconds. After digest-checking the previous canonical SDK, the new
export was rebased into `~/dev/symbian-sdk`; the old tree remains under
`~/dev/symbian-sdk-before-fast-primitives-20261001`. Both trees are digest
valid, and a fresh generated-project build/copy/relocation test against the
canonical SDK passed in 10.41 seconds.

### 2026-10-01 — C0 32-bit compiler atomics

A `std::atomic<int>` probe exposed missing `__atomic_fetch_add_4`,
`__atomic_load_4`, `__atomic_compare_exchange_4` and `__atomic_exchange_4`
on ARMv5T. After bridging those to the original EUSER atomic API, ARMv6 then
exposed `__sync_fetch_and_add_4`, `__sync_val_compare_and_swap_4` and
`__sync_lock_test_and_set_4`. The latter names are compiler builtins, so the
bridge exports them using separate C identifiers with assembler symbol labels.
The original `e32atomics.h` was added to the digest-pinned staged header
manifest. The bridge uses ordered ROM add/CAS/exchange and acquire load;
the changed-value control detects an incorrect final result. Eight guest
execution cases passed on both ARM profiles and emulator backends in 83.77
seconds. A fresh 2,428-file digest-valid SDK export, with all five required
EUSER ordinals in its standard proxy, passed all eight again in 79.98 seconds.
No parallel thread race or full libc++ thread mode was tested; A11 Future/Task
remains gated.

The 2,428-file export was promoted to the visible `~/dev/symbian-sdk` only
after both it and the previous 2,427-file canonical tree passed digest checks.
The previous tree remains at `~/dev/symbian-sdk-before-atomics-20261001`.
A fresh generated-project initial-build, SDK copy and project-relocation
test passed against the new canonical SDK in 7.42 seconds.
The real-header GUI source-staging/link test passed with 93 aliases in
10.75 seconds.
An additional stricter installed-SDK run substituted the selected SDK's
prebuilt `Symbian::Runtime` archive and installed `symbian/runtime.h` into a
temporary runtime-probe project. All eight atomic normal/changed controls
passed again in 85.85 seconds on both ARM profiles and emulator backends.
The earlier 79.98-second run had selected only the installed EUSER proxy;
this run proves the exported runtime archive also executes the bridge.
Four default string/vector cleanup cases passed against that archive across
both ARM targets and CPU backends in 49.80 seconds.

### 2026-10-01 — Secondary-thread atomic prerequisite

A first `RThread`/atomic guest probe returned -139. Retained worker logs showed
USER 0 panic before its callback; the EXE startup had rejected secondary reason
1. Original `kernelhwsrv/kernel/eka/euser/epoc/arm/uc_exe.cpp` instead calls
`UserHeap::SetupThreadHeap(ETrue, info)` and the `iFunction/iPtr` callback on
that path. Adapting the probe startup let the worker exit normally but exposed
an unimplemented Belle SVC 0x34 in `RThread::ExitReason`. The pinned emulator
already has a `thread_exit_reason` handler, so ordered patch 14 maps this
observed slot to that handler. The applied-state reverse check and full
14-patch replay from the pinned revision pass. The rebuilt emulator passes the
ARMv5T/Dyncom control and the full eight-case ARM/profile/backend matrix
(`8 passed, 48 deselected`, 92.65 seconds). Parent and worker each increment a
shared atomic 2,000 times; expected 4,000 and deliberately changed 3,999
produce distinct exits. The parent joins and checks completion, reason and
type before releasing the stack-owned state. Generated starter startup now
follows the same primary/secondary distinction. Thread heap teardown, TLS,
64-bit atomic coverage and A11 scheduling remain open. The same eight cases
passed against a fresh exported SDK's prebuilt runtime archive and standard
38-export EUSER proxy in 65.17 seconds. A generated project built after SDK
copy and relocation; the canonical `~/dev/symbian-sdk` was refreshed through
the installer. Its 2,428 files and the retained prior tree at
`~/dev/symbian-sdk-before-thread-primitives-20261001` both verify by digest.
See the root `PERFORMANCE_CONSIDERATIONS.md` for testable cost hypotheses;
none are measured.

### 2026-10-01 — Threaded libc++ and guest exception boundary

The pinned LLVM libc++ 23.1.2 `memory.cpp`, `thread.cpp`, `mutex.cpp`,
`condition_variable.cpp` and `future.cpp` now build in a pthread-backed guest
configuration. Bounded ownership tests exercise `unique_ptr`, `shared_ptr`,
`weak_ptr`, expiry and allocation-cell cleanup. A `std::thread` worker joins
after 4,000 shared atomic updates, copies/releases shared ownership and
destroys a moved `unique_ptr`. Normal and deliberately changed-result controls
pass on ARMv5T and ARMv6 with Dyncom and Dynarmic: 16 cases in 83.34 seconds,
with the changed worker-ownership probe rerun as eight cases in 44.29 seconds.
A fresh 2,496-file SDK export using its prebuilt runtime and standard
EUSER/libpthread/drtaeabi proxies passed the same 16 cases in 74.31 seconds.
Two installed-SDK `Symbian::Threads` target cases passed in 26.88 seconds.
Generated-project wizard/build/copy and actual GUI clock/OOM tests also passed
against that export. These are bounded emulator results, not a general
pthread, TLS or cross-ROM compatibility claim.

The first ownership probe emitted `R_ARM_REL32` code-to-data references to
libc++ vtables when built as PIC. E32 code and data relocate independently, so
the converter correctly rejected them. Compiling that source and original
`thread.cpp` with `-fno-pic` emitted typed absolute data fixups and passed the
controls. The installed application runtime target now supplies this option;
the separate local-GOT control remains intact. ARMv6 also exposed an 8-bit
compiler CAS libcall and ARMv5T a 32-bit store libcall; both now route through
original EUSER atomic operations.

A genuine `std::promise`/`std::future` link attempt failed on
`std::exception_ptr`, `std::logic_error`, error-category and related symbols;
its compiler/linker log is retained at `.symbian/std-future-test.log`.
The original Symbian `elf2e32` sets E32 `iExceptionDescriptor` from the
`Symbian$$CPP$$Exception$$Descriptor` symbol in the read-only image, with its
low bit marking presence. `TExceptionDescriptor` contains exidx base/limit and
read-only segment base/limit. Current SDK linker scripts discard
`.ARM.exidx`/`.ARM.extab` and the converter leaves that header field zero.
Enabling `-fexceptions` alone would produce a misleading, unsafe profile.
Guest exceptions are therefore a required future opt-in capability, default
off; host Abseil Status libraries remain exception-free. A throw/catch probe
must prove destructor unwinding, allocation failure and both emulator CPU
backends before any advertised support.

### 2026-10-01 — Bounded EHABI metadata and the imported-data gate

A Clang 23 ARMv6 typed throw emits `.ARM.extab` and `.ARM.exidx`, calls the
original `drtaeabi` exception allocation/catch exports, and references
`__gxx_personality_v0` plus `_ZTIi`. The pinned Symbian source exports the
three-argument ARM EHABI `__aeabi_unwind_cpp_pr1` personality. An isolated ARM
veneer tail-calls that genuine export; its throw semantics remain unverified.
The veneer needed explicit four-byte alignment: the changed-result build first
placed its ARM instruction at an unaligned address and the converter correctly
rejected it. The ARM function symbol is now aligned and converted.

The original `elf2e32` and `TExceptionDescriptor` sources guided an isolated
linker profile retaining `.ARM.extab`/`.ARM.exidx` and a four-word descriptor
(index base/limit, read-only segment base/limit). Native E32 conversion now
requires the descriptor when an index exists, validates its symbol and bounds,
sets the low-bit-marked header offset, and inspects the resulting structure.
The read-only end sentinel receives a bounded E32 code relocation. The
existing `R_ARM_JUMP24` import path now validates an ARM `B` to the PLT as well
as ARM `BL`/`BLX`; this is needed by the personality veneer.

A no-throw C++ catch/cleanup probe converted, passed native inspection and
exited 0 on the preserved RM-807 emulator. The maintained normal and
deliberately changed-result matrix passed all eight ARMv5T/ARMv6 ×
Dyncom/Dynarmic cases in 47.61 seconds. Two ARMv6/Dyncom cases also passed
with the installed SDK's prebuilt runtime archive. These controls establish
unwind metadata, loader acceptance and ordinary cleanup only. A typed throw
still produces `.rel.dyn` with `R_ARM_GLOB_DAT` for the imported `_ZTIi` data
object; the current import resolver supports only function PLT slots. A
maintained build-negative test now expects a specific rejection rather than
claiming a runnable exception profile. Original LLVM libc++abi
`cxa_personality.cpp` compiled in an experiment but required its own exception
object/unwind ABI closure; mixing it with Symbian's `__cxa` objects would be
unsafe without execution evidence, so it was not included. Logs and source
objects remain under ignored `.symbian/exception-experiment/`.
The final E32 parser also rejects an ARM_EXIDX section renamed to hide its
required descriptor; a native GTest covers the malformed input. The last
visible SDK export and its retained predecessor each contain 2,496
digest-verified files. The preceding visible tree is at
`~/dev/symbian-sdk-before-exidx-validation-20261001`.

### 2026-10-01 — 64-bit atomics, native ROM operations and Dyncom STREXD

A two-thread `std::atomic<std::uint64_t>` probe linked only after adding the
observed Clang `__atomic_*_8` and `__sync_*_8` entry points. The initial
runtime serializes 64-bit operations with one process-owned original EUSER
`RFastLock`; 32-bit EUSER CAS initializes it. Across ARMv5T/ARMv6 clients and
Dyncom/Dynarmic, 4,000 high-and-low-word increments, failed/successful CAS,
exchange and the direct `__sync` controls passed eight normal/changed cases.
The same eight cases passed against a fresh installed candidate SDK archive.

Direct calls into the preserved RM-807 EUSER's 64-bit add, CAS, load and swap
exports passed on Dynarmic but failed on Dyncom before the emulator fix. A
changed check showed the stored high word was wrong, not just the returned
value. In pinned EKA2L1 Dyncom, STREXD assembled the 64-bit register pair in
`value` then passed `RM` (a register index) to `exclusive_write64`. The
replayable GPL `dyncom-strexd-value.patch` passes `value`. A ROM-independent
LDREXD/STREXD GTest now checks both emulator backends with a changed high-word
control; CTest passes. After rebuilding EKA2L1, direct original-EUSER 64-bit
probes pass on both client architectures and CPU backends. The ROM was not at
fault for this observed failure. Pre-fix and post-fix traces remain under
`.symbian/native-atomic64-diagnostic*.log`.

The original Symbian ARM V6K 64-bit atomic source uses LDREXD/STREXD retry
loops. Its ARM V5/V6 source instead masks interrupts around a read/modify/
write; an EUSER export name alone therefore does not establish lock freedom
on every device. A separate native-atomic runtime build delegates 64-bit
operations to EUSER and passes a 12-case source matrix on the named RM-807
fixture, including direct-ROM and `std::atomic` changed-result controls.
This is evidence for that named ROM under EKA2L1, not other firmware or
physical hardware.

A further negative control found that Clang's ARMv6 `is_lock_free` builtin
reports true even when the selected runtime uses `RFastLock`. libc++'s
`__atomic/support.h` now has a narrowly guarded Symbian query to the actual
runtime profile; the pinned LLVM edit is maintained as
`research/llvm/symbian-libcxx-lock-free.patch`. The first attempted guard in
`__atomic/support/gcc.h` did not affect Clang, which uses libc++'s C11
atomic backend; this failed control led to the shared-header adaptation.
After that correction, a source smoke passed the lock-backed, direct-EUSER
and native-runtime paths; the final installed candidate passed 16 normal and
changed-result executions on both client architectures and emulator CPU
backends in 80.40 seconds. Its 2,564 sealed files all match their digests.
The candidate was promoted to `~/dev/symbian-sdk`; the previous 2,535-file
SDK was preserved at
`~/dev/symbian-sdk-before-native-atomic64-20261001`. Both trees are
digest-valid. The LLVM patch applies to clean pinned header copies and
reverse-checks against the current checkout; the emulator patch likewise
replays/reverses. No physical device was used.

### 2026-10-01 — Non-808 EUSER atomic capability check

The read-only `symbian_rom_atomic_oracle` uses EKA2L1's original ROM-image
parser on imported, digest-pinned EUSER files from the supplied dump
collection. ROM-image DLLs are not ordinary compressed E32 images: the first
attempt with `parse_e32img` rejected them. The original `parse_romimg` needs
the image mapped for the epoc10 path; an unmapped dump uses its epoc91 file
fallback, which shares the EKA2 120-byte header layout. A diagnostic attempt
with an unmapped epoc10 path hit the parser's null export-pointer branch; no
upstream parser change was made. The final read-only oracle checks the export
table and captures the first 64 bytes of each relevant function.

C7-00/RM-675, E6-00/RM-609 and 808/RM-807 each have the EABI EUSER 64-bit
atomic ordinals 2281/2329/2357/2361/2373. Their first 64 bytes for all five
entry points are identical (concatenated SHA256
`a424db7ad3c3947a92204bdfca244eb8d8839ada6436e7a866510905d0080422`).
The add and CAS entries disassemble to LDREXD/STREXD retry loops, rather than
an interrupt-masked read/modify/write. The 6120c/RM-243 and E71/RM-346 EUSER
images have 2,228 exports; the relevant EABI ordinals are absent. This does
not rule out every other atomic mechanism on those devices, but the SDK's
current native EUSER profile cannot link against those ROMs.

A cross-thread `std::atomic<std::uint64_t>` probe using the installed native
archive first failed with -142 on C7 and E6 under both CPU backends. The worker
actually exited with code 0; EKA2L1 logged unimplemented SVC 0x32 when the
parent asked `RThread::ExitReason`. The sixteenth ordered GPL patch adds the
observed v10 0x32 mapping to EKA2L1's existing handler. Belle's 0x34 call
then follows from the already proven two-slot v101 shift. With this patch,
all four C7/E6 × Dyncom/Dynarmic guest controls pass in 31.91 seconds. The
patch reverse-checks against the applied checkout and replays after the
previous fifteen. These are emulator results, not physical-device results.
Raw evidence is retained at `.symbian/rom-atomic-oracle-five.log`,
`.symbian/native64-other-rom-test*.log` and the Pytest frontend logs.

### 2026-10-01 — Compiler-rt closure and libc++ hash-table execution

The default guest archive now builds original LLVM compiler-rt aligned-copy,
float-to-unsigned, soft-float arithmetic/comparison/division and 64-bit
multiplication sources. Its bounded compiler-rt probe checks aligned copying
and positive, negative, overflow and NaN float conversions with a changed
result control. Eight ARMv5T/ARMv6 × Dyncom/Dynarmic source and eight
installed-SDK cases passed; raw logs are
`.symbian/compiler-rt-helper-source2.log` and
`.symbian/compiler-rt-sdk-test.log`. The pinned ARM `divsf3.S` requires an
ARMv6T2 `MLS` instruction, so the archive uses the original generic
`divsf3.c` instead. The latter also required original `muldi3.c`; missing
`__aeabi_lmul` was caught by the executable link, not hidden with a stub.

The next original libc++ dependency was `hash.cpp` for `__next_prime`.
`std::unordered_set<int>` now inserts 300 elements, reserves 700 buckets,
finds and erases elements, and returns to its starting Symbian heap-cell
count. A changed-result variant fails with -231. The source and selected
installed-SDK matrices each passed eight architecture/backend/positive-negative
cases (`.symbian/hash-table-source-matrix.log`,
`.symbian/hash-table-installed-matrix.log`). The path needs the selected
ROM's `libm.dll` `ceilf` import. Linking that proxy unconditionally caused
an E71 generated starter to fail image conversion with an unwanted needed
proxy. The installed CMake target now encloses only that proxy with LLD's
`--as-needed`/`--no-as-needed`; ordinary EUSER imports retain their existing
link behavior. The first attempt used compiler-driver `-Wl,` syntax, which
was rejected because the project invokes `ld.lld` directly. The corrected
template lets an E71 starter build and execute (one real-firmware test,
`.symbian/hash-table-e71-starter3.log`). An application that actually uses
the hash-table path still needs `libm.dll`; the SDK has not supplied a
replacement math service on ROMs without it.

After the scoped-link change, the full selected installed hash-table matrix
passed eight cases in 44.76 seconds, retained at
`.symbian/hash-table-installed-final-matrix.log`. A fresh export contained
2,591 digest-valid files and passed initial build/copy/project relocation.
The final export was installed at `~/dev/symbian-sdk` with a rebased manifest;
its previous 2,564-file version remains at
`~/dev/symbian-sdk-before-hash-table-20261001`, with both seals verified.
Root macOS CTest passed eight targets. No physical-device result is inferred.

### 2026-10-02 — Pinned guest Abseil Status/StatusOr and map closure

Replayed clean Abseil `5650e9cf76d3be4318d5fa3af38ee483ddfd5e4a`
with three ordered patches under `research/abseil/`. The new patch adapts
`raw_hash_set`'s static TLS seed/counter to a process-wide atomic sequence:
E32 does not supply ELF TLS merely because intermediate objects are ELF.
The platform patch also narrows Abseil's compile-time byte-lock-free assertion
for ARMv5T, where Clang emits a libcall and the SDK implements that libcall
using original EUSER ordered operations. The actual guest atomics now include
byte store/exchange and 32-bit sub/and/or paths. Source normal and
changed-result atomic controls passed four selected cases. General TLS and
concurrent hash-table behavior are still untested.

The Status/StatusOr link exposed actual dependency gaps rather than optional
stubs. A 16-bit-wide libc++ streams profile and selected frozen OpenC wide,
stdio and math imports close the required path. Using libc++'s `exception.cpp`
was rejected: it pulled fallback exception-pointer abort machinery rather
than the required base exception implementation. The original pinned
libc++abi `stdlib_exception.cpp` supplied the correct dependency. `ldexpl`,
`nan` and `nanf` use narrow SDK ABI adapters where the frozen libm table has
no corresponding exports; NaN bits, payload parsing and long-double equality
were checked in the guest hash/stream contract. Source hash/stream normal and
changed-result controls passed; the wider runtime selection passed 24 cases
with one documented skip (200 deselected).

The tracked `probes/abseil_status_probe` exercises original Abseil Status
code/message, Cord payload set/get, StatusOr failure and success with move,
and `flat_hash_map<std::string, int>` insertion, lookup and erase. A changed
result returns a distinct failure. Source-replay and sealed-SDK matrices each
passed 8/8 on RM-807: ARMv5T/ARMv6 × Dyncom/Dynarmic × normal/changed.
The SDK candidate sealed 3,097 digest-valid files: 384 original Abseil
headers and 43 closure archives for each architecture. It carries the source
revision, patch digests, Apache-2.0 notice, and
`Symbian::AbseilStatusOr` CMake target. A copied project built and converted
with only the SDK's target headers and archives; the separate original
`LowLevelAlloc` and generated-project relocation checks passed three cases.
The result is a tested library subset, not Abseil time, general TLS,
cross-thread page release, complete synchronization or A11 fibers.

The candidate and previous visible SDK were audited before promotion with
3,097/3,097 and 2,597/2,597 digest matches and no unrecorded files. The
previous tree is retained at
`~/dev/symbian-sdk-before-guest-abseil-20261002`; a copied, rebased
3,097-file SDK is now at `~/dev/symbian-sdk`. All eight installed guest
Status/StatusOr/map cases and the copied-project check passed again after
promotion (9/9). The pristine Abseil checkout has no tracked edits; the
platform patch apply-checks against it and the map/TLS patch reverse-checks
against the replayed checkout. No application sources were refreshed.

### 2026-10-02 — Nokia 808 USB mode observation

With the owner's 808 attached, filtered read-only IOService inventory showed
`0421:05d0`, location 1315328, one `08/06/50` mass-storage interface and a
mounted `disk4`. The owner selected PC Suite / Nokia Suite mode on the
handset. A second inventory showed `0421:05d1` at location 1315328, 18
interfaces, no mounted disk, and an `IOSerialBSDClient` under interface 2
providing `/dev/cu.usbmodem141202`. The USB descriptor includes a serial,
but its raw value was not retained. A serial-derived hash of the current
enumeration is available for future before/after comparisons; no baseline
hash was captured before the owner changed mode. Thus the host change and
owner report agree, while serial-bound verification of this
particular transition and a PC Suite protocol handshake remain open.

The CLI now supports redacted USB descriptor inspection and a two-phase
human-selected mode check. Ten device Pytests pass, including a synthetic
same-serial product-ID change and rejection of a different serial. Linux
sysfs descriptor parsing is synthetic-tested only. The macOS serial port was
observed but not opened; its protocol and ability to carry phone logs,
process inspection or debugging remain unknown.
Live `device mode begin` and `verify` in the current PC Suite enumeration
returned `unchanged` with `same_device_verified=true`, matching the saved
serial-derived anchor. The local ticket is ignored build/runtime state.

### 2026-10-02 — 808 CDC ACM AT identity probe

The `0421:05d1` configuration exposes one CDC ACM callout port beneath its
USB interface 2. A bounded `AT` handshake on that port returned `OK`. The
new `device info` probe then received `OK` for `AT+GCAP`, `AT+CGMI`,
`AT+CGMM` and `AT+CGMR`: capability line `+GCAP: +CGSM,+DS,+W`, manufacturer
`Nokia`, model `Nokia 808 PureView`, and revision
`113.010.1508 2013-01-02 RM-807 (c) Nokia`. These values come from the
handset's modem AT interpreter and are not an independently checked firmware
manifest. No serial/subscriber identity query or setting command was sent.
The actual PC Suite channel, phone logs, process inspection, screenshot and
debugger transport remain open. The probe uses fixed commands, short per-
exchange deadlines and bounded responses; `--no-protocol` leaves the port
closed.

### 2026-10-02 — 808 USB interface function inventory

Filtered read-only IOService inspection of `0421:05d1` exposed interface
names, endpoint counts, alternate settings and driver children. Interface 0
declares `MTP` and has `06/01/01` still-imaging/PTP class with three
endpoints. Interfaces 1–2 are CDC ACM control/data (`02/02/01`, `0a/00/00`);
macOS binds `AppleUSBACMControl`/`AppleUSBACMData` and the observed AT port
under interface 2. Interfaces 3–4 are another CDC ACM control/data layout
with vendor protocol `ff` on control; no host serial binding was observed
for it. Interface 5 is CDC wireless-handset control. Interfaces 6, 8, 10
and 12 are CDC OBEX-class control interfaces labeled `SYNCML-SYNC`,
`PC Suite Services`, `SYNCML-DM` and `Haptics Bridge`. Their adjacent CDC
data interfaces have zero endpoints in alternate setting 0. Interfaces 14
and 16 are unrecognized CDC subclasses `fe` and `fd` labeled `UsbPnComm`
and `LCIF_Alt0`. No MTP, OBEX, SyncML, Phonet or LCIF session was opened;
names and classes advertise roles only. USB-IF class codes and Linux's CDC
definitions support the standard names. Linux's sysfs source formats
alternate setting as decimal, while endpoint count and class codes are hex.

### 2026-10-02 — native libusb PC Suite inspection

- Pinned libusb 1.0.30 static archive in the host extension build and wheel SDK assets. `otool -L` on the built `_native` listed system frameworks and libraries only, with no libusb dylib. The native map matched the serial-derived anchor of the connected Nokia 808 (`0421:05d1`) without printing the serial.
- Active configuration 1 exposed 25 interface alternate-setting descriptors. Interface 0 has MTP/PTP bulk IN `0x81`, bulk OUT `0x01`, interrupt IN `0x82`; CDC union 8→9 names `PC Suite Services`, with bulk IN `0x88` and OUT `0x05` on data alternate 1. No interface association descriptor was present in the active configuration.
- `AT+CBC` returned charge 100% and connection status code 1. `AT+CSQ` returned RSSI/BER codes 99/99 (measurement unavailable). These are modem replies, not independent battery or radio readings.
- Native PTP/MTP GetDeviceInfo, OpenSession, GetStorageIDs, GetStorageInfo, GetObjectHandles and a bounded GetObjectInfo root listing succeeded. The session closed successfully. Two storages were reported: Mass memory and Phone memory. Root handle counts were 19 and 18; three root object infos per storage were fetched without recording private names here.
- On CDC data interface 9 alternate 1, OBEX Connect with the PC Suite FTP target returned `0xA0` and a Connection ID. The first Disconnect lacking that ID did not confirm success. A subsequent Connect and Disconnect with the returned ID both returned `0xA0`; alternate 0 was restored and the interface released (both confirmed by libusb success returns). No file operation or debugger command was sent.
- A native `UsbSession` binding then repeated the same Connect/Disconnect sequence using queued asynchronous bulk transfers; both write and read completions were `completed`, and both responses were `0xA0`. Synchronous and asynchronous standard `GET_STATUS` control IN each returned two bytes. A final low-level check confirmed the poll-fd binding returns both `fd` and `events` fields on macOS. The Python binding performs no protocol parsing in libusb callbacks.
- The pinned libusb 1.0.30 source tarball matched SHA-256 `fea36f34f9156400209595e300840767ab1a385ede1dc7ee893015aea9c6dbaf` and built a static archive and header with the wheel bootstrap configure flags on macOS. The final wheel audit passed after installation, and the matching source distribution contains the native transport source and the libusb 1.0.30 version gate. Open question: verify static source/relink materials and poll-fd behavior on Linux CI and another device. The SDK packages the LGPL notice and archive; distribution procedure should include the pinned source and build materials.
- Focused USB/AT/connection/CLI tests: 37 passed. The full `symbian/tests` run had 165 passed, 356 skipped, 7 failed, and 39 errors in unrelated toolchain/image and verification cases; no device-focused test failed. This run is not a complete repository green gate.

### 2026-10-02 — typed USB results and A11 Future bridge

- Native USB descriptor, MTP, OBEX, inventory, poll-descriptor, and completion results now use purpose-built C++ structs. The Python USB and AT APIs validate them into Pydantic models; CLI JSON is emitted at the output boundary. Optional unobserved fields use `exclude_if`, while explicit verification states remain present.
- A11's native `Future<UsbCompletion>` and `FutureToPython` bridge now back the six asynchronous bulk, interrupt, and control transfer methods. `AsyncUsbSession` watches libusb poll descriptors and its timeout from asyncio. The libusb callback records completion, and promises settle after the event handler returns. Python Future cancellation requests native transfer cancellation.
- On the connected 808, an awaited standard control IN returned a typed `UsbCompletion` with two bytes. A pending interrupt read Future cancelled successfully. A PC Suite OBEX Connect/Disconnect over the Future path returned `0xA0` for both responses. After typed probe conversion, the live descriptor, MTP, and OBEX states remained `observed`, `connected`, and `connected`; the CLI still rendered the rich interface and protocol summary. No private object names were recorded.
- Focused device, CLI, and model tests passed: 41. Linux poll integration and repeated cross-device USB Futures remain open; these tests exercise the macOS connected handset and a fake event pump only.
- The wheel built and passed the installed audit in a clean Python 3.12 virtualenv. That environment loaded `list_usb_devices_native()` as a list of typed records and `list_devices()` as Pydantic models. The matching source archive includes `usb_models.py`, the native USB headers and source, the binding source, and the pinned dependency bootstrap. An old editable-install path in the development virtualenv initially obscured this check; the clean environment resolved it.
- A bare `symbian` invocation prints top-level help and exits 0. The focused CLI/device/model suite passed 43 tests after this change.
- CLI help now colours usage and sections cyan, command/option names green, and descriptions with emphasis when the output terminal supports colour. `NO_COLOR` and `TERM=dumb` keep help plain; `CLICOLOR_FORCE=1` supports explicit colour testing. The formatter suppresses unhelpful `None` and `False` defaults. Nested `device info --help` and the bare command were tested in both plain and forced-colour modes; the focused suite passed 44 tests.

### 2026-10-02 — application wording in help and guides

- Replaced "experiment" with "application" in application build/package help, binding descriptions, comments, and introductory guides. Compatibility identifiers such as `e32-pic-experiment`, report schema names, experimental UID range terminology, and actual research experiment records were retained.
- Native Python extension rebuilt after binding-description and SIS status-string edits. Focused CLI/device tests passed 40. The project-build/toolchain run had 8 passes and 3 ARM unwind descriptor failures in E32 conversion, matching the previously recorded broader-suite failure area; these are not help-text failures.
- The rebuilt wheel passed the installed audit in a clean Python 3.12 environment; the installed `symbian` help and native `build_sis` description use application wording. The matching source archive built successfully.

### 2026-10-02 — console device identity and desktop behavior

- The connected phone's discovery record includes a serial-derived hashed anchor without exposing the serial. A GUI selection can use this anchor to follow a phone across a PC Suite/mass-storage product-ID change. Port-location anchors cannot prove sameness after a disconnect, so the GUI clears the selection instead. A swap between two polling observations at the same port remains undetectable without a serial-backed anchor.
- The Tk request bridge keeps discovery off the GUI thread. A five-second context refresh updates all device views, clears disconnected probe results, and preserves a unique USB table row by vendor/product/physical port. Hidden GUI smoke on this host loaded five tabs, resolved context and selected one phone; the technical text widget reported border width zero.
- CLI application creation may replace its process with an SDK tool, so the console executes CLI tasks in a child with standard input closed and supplies noninteractive mode. `symbian console` itself now spawns a detached GUI child so its terminal becomes available immediately. Focused console and device tests passed (27). Open question: test the detached process lifetime and identity reconciliation on Windows/Linux desktops and with multiple live handsets.
- A profile of console startup on this host measured CLI module import 0.147 s, service import 0.013 s, FastAPI app creation 0.019 s, catalog request 0.001 s, and libusb USB inventory 0.008 s. Full phone context discovery took 0.977 s because host discovery invokes system profiling. The console service now compares a cheap libusb signature (VID, PID, bus, address and ports) before rerunning that scan, with a 20-second upper bound on reuse; unchanged context reads measured 0.004–0.005 s. A USB change forces discovery immediately. This optimization retains detailed host driver/volume data from the authoritative full scan.
- The USB inspector and protocol panels retain per-phone results in memory. Changing tabs presents cached widgets immediately, then schedules read-only USB refresh in the worker. Protocol operations remain explicit because they open handset sessions. Scrollable USB, Protocol and Activity tab viewports keep the sidebar and status bar visible; a hidden Tk check at 1000×620 showed the Protocol scrollbar and a scroll region taller than the window. Focused console/device tests passed (28).
- Expanded console, CLI, USB, AT and connection tests passed (57). The rebuilt macOS ARM64 wheel passed its installed audit in a clean Python 3.12 environment; the installed console client loaded all 38 catalog tasks and one connected handset from outside the source tree.
- A live `uv run symbian console` invocation returned to its terminal in 0.12 seconds with exit code zero, and the new `symbian.console.gui` child remained running. The wheel was rebuilt and re-audited after the final scroll and short device-label edits.
- The generic Home/Tasks browser was replaced with six purpose-based tabs. All 38 catalog actions were accounted for once across persistent application, firmware, emulator, SDK Setup/Inspection/Preservation, and Devices Actions panels; USB and Protocols remain specialised sections under Devices. Hidden Tk tab-switch timings on this host were 0.0002–0.001 seconds with no synchronous device request on navigation. The bridge shutdown now cancels its scheduled Tk drain callback, which avoided a Tcl warning in a repeated hidden-window close check.
- Guided action sections also gained outer scrollable viewports for long reviews/results. After the navigation change, 57 focused console/CLI/device tests passed and the rebuilt wheel passed its installed audit in the clean Python 3.12 environment.
- The screenshot of expanded OBEX details showed only the first line because the technical widget began near the bottom of its outer viewport. Enlarging the text area to 14 lines and revealing it through the enclosing canvas after layout resolved this in a hidden Tk check: the text measured 211 pixels high, the outer canvas had a nonzero vertical offset, and its scrollbar was packed.
- The SDK Setup screenshot exposed a form canvas that expanded vertically despite containing no fields. The form now sizes to requested content, capped at 280 pixels for longer inputs, with its own scrollbar only on overflow. No-input actions skip the otherwise redundant review step and hide the form; a hidden Tk check confirmed `doctor` showed a direct Check computer button and returned its result.
- The 57 focused console/CLI/device tests remained green after the form change. The final rebuilt wheel passed the installed audit in the clean Python 3.12 environment.
- On this macOS Tk build, `systemSelectedContentBackgroundColor` resolves to `#0064e1` and `systemAlternatingContentBackgroundColor` to `#f4f5f5`. The GUI keeps Aqua controls, adopts the selected-content blue, and tags tree rows alternately. A hidden Tk check confirmed theme `aqua`, the selected row colour and alternating action tags. Sending a synthetic Mac `<MouseWheel>` event to a form entry scrolled the inner form first, then the outer viewport when the inner form reached its edge.

### 2026-10-02 — native-control console frontend

- wxPython 4.2.4 installed from its macOS ARM64 wheel and reported `osx-cocoa` / wxWidgets 3.2.8. The `symbian console` launcher now selects `symbian.console.wx_frontend.app` when wxPython is installed, retaining the Tk frontend as a Linux fallback if it is not. wxPython is a macOS/Windows package dependency. The frontend reuses the FastAPI/httpx in-memory client, typed catalog, device selection policy and Status facilities; no protocol executor was added.
- A live wx event-loop check opened six tabs, loaded all 38 public actions, and resolved one connected 808. Every action form rendered with advanced settings. A long form measured 903 pixels of virtual height inside a 590-pixel scroll viewport, confirming that the enclosing native panel scrolls. The SDK worker remained outside the wx event loop. The USB inventory keeps a uniquely matched vendor/product/bus/port selection across refreshes and clears phone-specific results when selection changes.
- The 57 focused console, CLI and device Pytests passed. `uv build --wheel` succeeded; the resulting macOS ARM64 wheel contained the wx frontend and was installed with wxPython in a clean Python 3.12 environment. Its installed-wheel audit passed from `/tmp`. `uv run symbian console` returned in 0.12 seconds and left a `symbian.console.wx_frontend.app` child running. Open question: exercise native appearance and device hotplug behavior on Windows/Linux desktops; the connected 808 was observed for context only and no new AT/MTP/OBEX transaction was run during this frontend change.

### 2026-10-02 — device-aware console status

- The status bar now reserves its right field for a selected live phone: a green bullet plus product, VID:PID and descriptor-derived interface profile. An idle unplug clears the field after the next inventory observation. Active phone requests carry a snapshot of the selected phone so their amber status survives temporary disconnect or lack of response; the request result and a fresh context observation resolve it. A failed request is labeled connection-unverified until discovery completes. A successful `device mode begin` retains an amber verification-pending hint through unplug/replug; `device mode verify` success or an explicit View-menu dismissal clears the local hint without modifying the saved ticket.
- Three focused state tests cover the connected/idle-unplug, active-disconnect/failed-result and mode-pending/verification paths; the combined console, CLI and device suite passed 60 tests. A live macOS wx check rendered the green indicator for the connected 808 as `808 PureView · 0421:05d1 · composite`. No physical protocol operation was initiated to test the new status display.

### 2026-10-02 — current macOS console appearance

- The owner's screenshots showed that the prior top segmented tabs, stretched white action list, faint gray help text and separate right context column still looked unlike current macOS Settings. The wx frontend now uses a tinted left navigation sidebar with a rounded blue selected row, expandable SDK/Devices sections and current selection below navigation. A `wx.Simplebook` switches ten cached content pages without visible nested tabs. Action lists fit their contents instead of stretching to the window bottom; forms and protocol groups use rounded white surfaces on a contrasting light content background, and secondary text uses a darker muted color.
- A live wx event-loop check switched all ten pages, loaded 38 catalog actions and the connected 808, and opened Protocols without widget errors. An expanded emulator form reported a 955-pixel virtual scroll extent in a 592-pixel viewport. The new sidebar has not yet been compared against a fresh human screenshot on other platforms; macOS settings colors are intentionally light, as requested.

### 2026-10-02 — WebKit-backed console presentation

- The owner compared the wxPython window against current macOS Settings and found its native widgets visually inconsistent despite the earlier sidebar revision. A pywebview 6.2.1 shell now embeds local HTML/CSS/JavaScript in WKWebView on macOS and WebView2 on Windows; the Linux dependency includes PySide6. The frontend calls the same typed FastAPI/httpx in-memory service through a narrow Pydantic bridge, with no network listener. This is a presentation change; no new handset protocol or device write was added.
- A live Cocoa window loaded all 38 catalogued actions, the connected 808's redacted selector and green status indicator. A follow-up live window switched among Devices → Protocols/USB inspector and SDK tools → Inspection using the new main-content tabs. The layout has six icon-led primary sections, compact icon-led action cards and no redundant workspace/action headings. These are renderer and interaction checks, not a human visual review of every page or a Windows/Linux renderer run.
- Generic USB inventory now carries a named class beside the exact native code, while the supported-phone inspector shows interpreted interface roles, declared names, host drivers, endpoint counts and serial ports before raw technical details. The new frontend did not initiate an AT, MTP or OBEX transaction during smoke checks. Open question: verify appearance and touchpad behavior on Windows/Linux and with multiple phones.
- After the hierarchy refinement, a live Cocoa window showed six primary navigation icons, three Devices tabs, three SDK tabs and three protocol icons; tab clicks reached Protocols, USB inspector and Artifact inspection. A separate live window ran the host-only Check this computer action to completion and returned its status to Ready. JavaScript syntax, Ruff, Black and 21 focused Pytests passed. The final macOS wheel rebuilt and passed the installed-wheel audit from a clean Python 3.12 environment; its package includes the HTML, CSS, JavaScript and bridge. `uv run symbian console` returned in 0.12 seconds and left a WebKit-backed child running.

### 2026-10-02 — integrated console data views

- Firmware catalog browsing previously opened a generated form despite every input being optional. The new local WebKit view loads the catalog on entry, renders seven identities on this host as a searchable list, selects an identity, and keeps that list visible while Import, Inspect, Export or source probing is opened. A contextual right sidebar shows selected model/code/Symbian identity and the effective store, with optional source overrides. An Export check showed the selected alias `e6` in the required reference field and the destination field on the first page; no export was executed.
- Host readiness, effective emulator settings and connected-phone inventory now refresh as read-only views on entry. Related actions remain in the same workspace. Results from other public actions get bounded structured fields, lists and a searchable file index when a manifest contains many files; raw JSON is a disclosed exact-evidence view. A live firmware Inspect check found the file search and returned to Ready without replacing the library list. This inspection read an imported host object only; no phone protocol transaction occurred.
- Required argument groups now precede optional source groups in generated steps. Optional groups following required input are disclosed rather than forced into an intermediate page. The former Configure more action skipped the first required step for some workflows; it now expands later settings without advancing. Consequential operations keep explicit review, while simpler actions execute after their last required input. Focused console tests passed (22), JavaScript syntax, Black and Ruff passed; a rebuilt macOS wheel passed the installed-wheel audit. A direct installed import attempted from the source checkout shadowed the wheel and failed to find its extension; repeating from `/tmp` confirmed the packaged frontend assets render correctly.
- A final live Cocoa check typed `SYM.ROM` into the inspected firmware file index and received `1 matching files`, confirming the client-side filter updates the structured view. Source overrides from the active library result are carried into related Inspect/Export actions; a later UI refinement restricted automatic base refreshes to actions that can change the workspace. The final rebuilt wheel was reinstalled outside the checkout and its audit passed.

### 2026-10-02 — optional settings wording and flow

- The application form's Configure more button obscured that it merely exposed optional inputs. Replaced it with an inline Optional settings disclosure, grouped by the catalog's SDK/build and emulator headings. The Tk and wx fallback buttons received the same wording. A catalog-driven JavaScript render check found both groups and the persistent Review action. Twenty-two focused Pytests, JavaScript syntax, Ruff, Black and the installed-wheel audit passed. Open question: verify the disclosure's visual spacing in the next human macOS review.

### 2026-10-02 — firmware selection and evidence display

- The separate Inspect imported firmware action placed its result below a full-width catalog and required an extra click. The WebKit view now pairs a narrow catalog with a selected-record inspection pane and starts the existing read-only inspect command when selection changes. Results are cached by content identity; stale results cannot replace another selected record after a rapid switch. A hidden WKWebView run loaded seven identities and the selected E6-00's 13,186-file index in 323/611-pixel columns, with no Inspect action card.
- Raw result content is serialized as JSON. The previous language picker could apply XML, Python or other grammars to those same bytes; the viewer now always requests JSON highlighting. The bridge still supports other languages for future views that display actual source text. Open question: human visual review of the new pane on smaller screens and other operating systems.

### 2026-10-02 — live context selection and connection observation

- Full context resolution includes project/SDK settings and can take about 950 ms on a cold macOS scan; repeat calls measured 5–19 ms. Native libusb inventory measured 4–8 ms. The GUI now polls a serial-free native topology snapshot every 750 ms on a separate bridge path, using it as a change signal and immediate reason to suppress a stale green presence indication. The full context flow still decides identity and selection. A simulated topology removal in hidden WKWebView cleared the status before delayed context reconciliation, without disconnecting hardware.
- The sidebar now chooses a workspace, application, SDK and phone. Workspace selection is carried to the CLI subprocess as its cwd, and the in-memory context API accepts explicit paths so forms show effective values. A hidden WKWebView check showed three path controls, the selected 808 and no manual Refresh action. Another live check observed background firmware load status until completion. Open question: measure unplug/replug latency with physical hardware and verify the path picker layout on Windows/Linux.

### 2026-10-02 — application manifest recognition

- The console application selector assumed every project had `symbian-project.json`. `examples/gui_app` has only `symbian.toml`, while the current `symbian init` generator writes both files. Selection and context discovery now recognize either manifest, and JSON-specific SDK lookup runs only when JSON exists. A focused bridge test selected the real `gui_app` directory and a JSON-only compatibility fixture; 59 console, CLI and device tests passed. The rebuilt wheel passed its installed-wheel audit and its packaged helper recognized `gui_app`. Open question: check other standalone source project layouts as they are introduced.

### 2026-10-02 — console monitor, build output and device presentation

- The connected-terminal ancestry on this macOS host ended at iTerm PID 757. Its CoreGraphics window center mapped to NSScreen index 1, which matches pywebview's second screen. The detached console now carries this index; the foreground emulator receives it and the maintained Qt patch moves and activates the emulator window on that screen. The modified EKA2L1 target compiled. A direct `gui_app` foreground run produced an on-screen window at `(-58, -1035, 902, 725)`, within display 1, before the supervisor was deliberately terminated. Open question: visually verify stacking above a live console window when the two apps occupy different macOS Spaces.
- A real `gui_app` build with `SYMBIAN_CONSOLE_BUILD_LOG` returned its normal JSON result and a 334,031-byte streaming log; an integration test saw a first line while the tool remained alive. The UI reads only the last 64 KiB for responsiveness and puts compile groups behind a disclosure. The exact build result stays available as JSON.
- The user supplied a sharper Nokia 808 front/back image. Its front portrait was cropped, the connected white background removed, and the result checked on a light blue backdrop. USB interface rows now lead with the interpreted function and keep numeric descriptors expandable. Firmware selection uses an exact normalized model match to the connected selected phone only when the user has not manually chosen another identity. Open question: verify model-name matching on other phone descriptions and operating systems.

### 2026-10-02 — C++20 GUI launcher and device portrait

- Including both historical `w32std.h` and libc++ `memory`/`thread` in one translation unit fails on conflicting placement-new declarations. The GUI now has a small C++20 `app.cc` that uses `std::make_shared` to carry the run result into a `std::thread` owning the original event loop in `window_server.cc`. That thread installs its own Symbian cleanup stack. The counter and timer behavior are unchanged. The reproducible ARMv6 E32 build passed, the real GUI replay passed on Dynarmic and Dyncom (2 tests, 13.70 s), and all four opt-in live debugger cases passed after their source breakpoint and variable-stack expectations were updated. This is emulator evidence against the preserved RM-807 fixture; physical-device behavior remains unverified.
- For the Console's supported-device card, the Nokia 808 portrait was cropped and background-masked from Vlado.grv's CC BY-SA 3.0 Wikimedia Commons photograph. The bundled 226×382 transparent PNG is selected only when vendor `0x0421` and product name `808 PureView` match; other models retain a generic SVG. A JavaScript render check produced one photograph and two generic fallbacks from three device fixtures, 59 focused console/CLI/device tests passed, and the installed wheel audit found the bundled asset and passed. Open question: visually verify the portrait and row density on the next live Console review.

### 2026-10-02 — project-local Console and reproducible debug builds

- A detached `symbian console` process can use `--workdir` for initial project and SDK discovery. The selected SDK is propagated to CLI children via `SYMBIAN_SDK_MANIFEST`, keeping Build/Run/Package aligned with the sidebar selection. The project icon comes from `symbian.toml` only when it is a small project-local PNG/JPEG or static SVG with allowed shape elements and attributes.
- The application view reads source-tree declarations rather than requiring a separate inspect action. Its firmware menu uses the existing imported-firmware catalog, and its Run action invokes the shared emulator supervisor. A standalone TOML project previously failed at `ProjectConfiguration.load` because it had no generated JSON preferences; the launcher now resolves its SDK directly and uses the TOML target name/UID.
- The owner's Console build failed with `DATA_LOSS: Independent CMake ELF/E32 builds differ`. Captured diagnostic copies had identical E32 SHA-256 `e84062953e8bd86614651f6ef7517d7d3402e58ffaabdeea56f1cc9d52b0a36c`, while ELF DWARF compilation directories differed (`/symbian-src/gui_app/.symbian/build/cmake` versus a `repro-*` directory). Adding `-fdebug-compilation-dir` to GUI and generated-project CMake flags produced an ELF SHA-256 `dc30747cf114abf7300019c2481c1c4af924633dab67e631a78ee7dd8ff83338` in two independent trees. A new generated project also built reproducibly. The package command then returned OK with `gui_app.sis`; a bounded session entered READY and closed. An initial-render JavaScript check caught and fixed a null project-icon dereference before the next GUI launch. Open question: a full Run-button UI replay and cross-platform sidebar appearance still need human visual review.

### 2026-10-02 — IDE GUI Run debugger selection

- The active IDEA/CLion 2026.2 log recorded a failed Debug of `GUI Run`: it attempted to launch a nonexistent bundled macOS GDB at `Contents/bin/gdb/mac/aarch64/bin/gdb`. This project had only a guest ARM GDB profile and no selected host debugger. The GUI Run target is a macOS launcher; the separate GUI Debug configuration is the ARM remote-debug path.
- The local IDE generator installs a host LLDB profile pointing at `/usr/bin/lldb`; the corrected per-target selection logic is recorded below. The ARM GDB profile remains available for GUI Debug. LLDB launched `build/debug/gui_app_run`, stopped on `main` at `launcher.cc:10`, and showed a source backtrace; the process was then killed before the emulator started. The ARM GDB wrapper returned GNU GDB 17.2. Two focused IDE configuration tests, Ruff and Black passed. The owner's subsequent screenshot still showed the nonexistent GDB path in the already-open IDE. macOS Accessibility denied scripted IDE menu access (`osascript` error -1719), so a live toolbar click was not automated.

### 2026-10-02 — console Run log stream

- Run previously held CLI stderr until the emulator exited; its build tools wrote only when the Build action had set the private log environment. The Run bridge now supplies the same bounded tool log, routes CLI stderr to it while the child remains active, and receives the owned session directory through a private sidecar. The view combines recent tool output with a bounded tail of that session's `frontend.log`, after verifying the directory is beneath the selected application's `.symbian/runs`. It polls while running and collapses after completion, retaining the output for inspection.
- A live `gui_app` run against the imported RM-807 firmware produced tool output while the CLI process was active, then 11,807 frontend-log bytes before the process completed. The combined reader returned 28,208 characters including emulator output. The verification supervisor was terminated after that observation; this was a log-transport check, not a normal-exit replay. Thirty-three focused console/frontend tests passed; Ruff, Black and JavaScript syntax checks passed.

### 2026-10-02 — actual CLion debugger selection key

- A second GUI Run Debug screenshot after an IDE restart still showed an attempt to launch `Contents/bin/gdb/mac/aarch64/bin/gdb`. The generated `CurrentDebugProfile` selection therefore was not authoritative. Decompiling the installed `intellij.cidr.debugger.profiles.clion.jar` showed `SelectedDebugProfileService` as the active `CidrCurrentDebugProfileService`, with persistent state in `$WORKSPACE_FILE$` and a cache keyed by `##RUN_CONFIGURATION##` plus the applicable CMake profile. The root workspace already contained `CMake Application.GUI Run` + `CMakeBuildProfile:Debug` mapped to non-shared GDB ID `4ea605ef-...`, which had empty settings; the dedicated GUI workspace had a similar mapping.
- The IDE generator now parses that JSON state with typed Pydantic models and changes only the GUI Run stamp to the shared GUI Host LLDB profile ID, keeping other mappings. Regeneration produced the expected root `Debug` and dedicated `clion-arm` mappings. Focused tests replace a deliberately broken GDB mapping and verify an unrelated debugger mapping survives. Open question: confirm the running IDE consumes the corrected selection after project reload; it may retain previously loaded service state until then.
- A live attempt made while the IDE remained open launched `.symbian/clion-setup/gui-gdb` for GUI Run, then rejected `build/debug/gui_app_run` as an unrecognized executable format. The file is a macOS arm64 Mach-O executable; direct `/usr/bin/lldb` launch reached the source breakpoint and backtrace. Both saved `SelectedDebugProfileService` stamps now point to the LLDB profile, so the remaining live mismatch is the IDE's cached choice. A project reload or direct GUI Host LLDB selection is needed before confirming an IDE Debug run.
- The owner then selected GUI Host LLDB and retried GUI Run Debug. The IDE log showed its LLDB frontend starting; a traced Python supervisor process existed, and the editor displayed `_dyld_start` disassembly. LLDB documents `target.process.stop-on-exec` as true by default, matching the C++ launcher's `execv` transition to Python. Clicking the IDE's **Resume Program** control advanced the session and the owner confirmed the emulator ran. The earlier F9 key attempt did not visibly advance it on this macOS keyboard. This verifies the host Debug route but not Symbian guest breakpoints; those still use GUI Debug and ARM GDB.
- The owner confirmed the missed breakpoint was in Symbian application C++ while **GUI Run** and host LLDB were selected. This pairing cannot debug ARM guest instructions. The generator now also records `Remote Debug.GUI Debug` -> `Symbian GUI GDB` in `SelectedDebugProfileService` for root and standalone projects, without changing other selections. The current guest ELF relocated `GuiMain` to `0x70000028` and `DrawGui` to `0x700005b0`; both source stops were observed through ARM GDB. A machine-interface test placed its `GuiMain` breakpoint before remote attach and confirmed that the post-connection symbol relocation moved it into the runtime code range, then received `breakpoint-hit` and an instruction-step stop. Four focused tests passed. The owner then selected GUI Debug and confirmed that the live IDE stopped at the guest source breakpoint; the IDE log showed the generated GDB wrapper launched in MI mode for GUI Debug. Full IDE stack unwinding and broader variable inspection remain open.

### 2026-10-04 — A11 channel interface correction

The earlier fallible `thread::Channel` surface conflicted with A11's pinned
`Writer::Write` (`void`), selectable cases and zero-capacity rendezvous. The
host now uses the pinned channel header and waiter state; the guest implements
the same public Reader/Writer/Channel signatures with its own selector backend.
The SDK's nonblocking mailbox behavior moved to
`symbian::concurrency::BoundedChannel`, which keeps Status returns,
idempotent Close and Discard without changing A11's API. The guest probe
exercised buffered and rendezvous transfers, losing-case value preservation,
competing cases, timeout and cancellation on ARMv5T/ARMv6 under
Dyncom/Dynarmic, with normal and changed controls: 8/8 passed. The focused
host concurrency test passed. Original A11 test coverage, fiber trees and
shared scheduling remain open.
The final exported SDK at `.symbian/a11-channel-sdk-final-20261004` carries
source-matching channel and case headers and digest-verified ARMv5T/ARMv6
fiber archives. Its own eight-case guest matrix passed. Rebuilt host channel
and fiber tests passed 2/2, including selectable cancellation.

### 2026-10-04 — first component device API

The existing runtime already bridges `User::TickCount`/`UserHal::TickPeriod`
and `User::FastCounter`/kernel HAL frequency. I used those verified native
calls for a small `system` component under `cpp/symbian/api/`, with typed
counter readings and status mapping. This avoids introducing a second native
counter bridge or pretending that counter frequency is fixed across devices.
The host mapping test passed, and a probe linked from the freshly exported
SDK passed eight ARM architecture/emulator backend cases. The SDK manifest's
header and both archives matched their SHA-256 entries.

The next components each need their own native header/ordinal and permission
audit before a public API is exposed. In particular, power state must not be
inferred from an unverified HAL field, and camera and storage requests need
explicit buffer ownership, cancellation and capability behavior. Emulator
results do not establish any of those contracts on the connected Nokia 808.

### 2026-10-04 — read-only HAL and File Server slices

The preserved HAL headers define pixel dimensions, physical twips, external
power, power-good and four qualitative battery states. Their frozen EABI export
maps `HAL::Get` to `hal.dll` ordinal 1. The new power and display components
keep that header in native translation units, returning independent optional
states or typed geometry to C++ applications. Power values can be unavailable;
the guest probe accepts an explicit unsupported status rather than fabricating
a charge level. Display requires positive pixel dimensions. Both components
linked and executed in all eight packaged ARM/backend controls.

File Server source headers and its EABI definition provide the observed
`RFs`/`RFile`/`RDir` read path. The first link exposed one missing frozen
EUSER `TDesC16::Ptr` ordinal; adding exactly that proxy export resolved it.
The storage component owns a session plus one subsession per handle, uses
caller-owned memory for file reads, and advances a directory cursor one entry
at a time. A guest read of `Z:\sys\bin\euser.dll` returned PermissionDenied;
the same packaged probe then read `Z:\resource\psui.r01` and one entry from
`Z:\resource` successfully across both ARM targets and CPU backends. This
shows the platform security boundary should be represented by status, not
bypassed. The host mapping/ownership test passed, and 17 SDK assets matched
their recorded digests. Open questions: physical-device HAL support, dynamic
orientation, File Server session affinity across threads, large-file behavior,
and asynchronous notifications. None is inferred from the emulator result.

### 2026-10-04 — connectivity monitor audit and explicit File Server writes

The prepared SDK lacks a connection-monitor client header and frozen import
contract. More importantly, the emulator's connection-monitor server currently
returns a fixed connection count of one and a fixed GPRS bearer. Exposing that
as a modern connectivity snapshot would misstate the emulator's actual state,
so connectivity remains a planned component rather than a public library.

The preserved `f32file.h` and `efsrvu.def` provide `RFs::MkDirAll`,
`RFile::Create`, `RFile::Open`, `RFile::Replace`, positional `RFile::Write`, and
`RFile::Flush`; `euseru.def` provides the `TPtrC8` constructor needed to refer
to caller memory without copying. These support a separate move-only writer
with explicit create/open/replace mode and a separately requested flush. The
File Server still controls data-cage and drive permissions. Guest validation
uses only a disposable emulator instance and an app-private `C:` path; it
does not test phone storage or establish power-loss durability.

The writable probe passed all eight ARMv5T/ARMv6 × Dyncom/Dynarmic ×
normal/changed event-executor cases after the new imports were packaged. It
created and flushed a private `C:` file, read the bytes back and used the
incremental `FileCopy` owner to make a second copy. Host GTests exercise a
32 KiB progress step and cancellation before the next chunk. The native
File Server calls used by a step are synchronous, so their worst-case latency
and cancellation completion time remain unmeasured and unbounded by this API.
The final SDK export moved the one-time 32 KiB buffer reservation before any
destination create/replace call, avoiding a destination side effect on buffer
allocation failure. Its 3,271 asset digests matched, and a fresh ARMv5T/Dyncom
guest probe passed with that exact export; the preceding full eight-case matrix
passed before this allocation-order-only change.
Directory iteration now has a cross-thread atomic `Cancel()` request checked
before and after its single native read, so no later entry is delivered once
the request is observed. This does not change the unresolved latency of the
native read itself.
The final cancellation export packaged the updated header and both ARM
archives; all 3,271 asset digests matched and a fresh ARMv5T/Dyncom guest
probe passed with the directory cancellation check enabled.

### 2026-10-04 — SD throughput and initial ECam contract

The preserved File Server `f32file.h` exposes `RFs::VolumeIOParam` with
physical block, filesystem cluster and suggested read/write buffer sizes.
It also exposes buffered/direct/read-ahead file modes, positional I/O,
`RFile::SetSize` and `Flush`. These make software-level SD throughput
improvements plausible, but no physical-card throughput has been measured.
The emulator's host-backed storage cannot establish a Nokia 808 gain. The
root `SD_STORAGE.md` records candidate levers and a controlled on-device
benchmark, retaining the current 32 KiB copy step until measurements justify
another default. Open question: which I/O parameters and cache modes does
the 808 firmware actually report for its removable drive?

The preserved Symbian multimedia `ECam.h` and `ecamU.def` identify
`CCamera::CamerasAvailable` and `CCamera::New2L` imports. The latter uses an
`MCameraObserver2`, may leave, and requires UserEnvironment capability. The
new `Symbian::Camera` boundary exposes only typed inventory discovery. Nine
host device API GTests passed, including native error mapping. The package at
`.symbian/device-api-camera-sdk-20261004` contains both ARM archives, ECam
headers and the `CamerasAvailable` import stub. A packaged ARMv5T/Dyncom guest
probe passed with camera discovery.

A candidate inspection API using `CCamera::New2L` compiled, but a guest probe
that referenced it initially failed to link: `TTrap::Trap` and `TTrap::UnTrap`
were absent from the ARM EABI exports. Selecting the exception-based leave
mode and adding frozen `drtaeabi` exception imports linked successfully. The
ELF-to-E32 converter then rejected the resulting imported C++ type-info
vtable as a non-function import. The candidate was removed from the public
library. Open questions: how to support and verify this data import and leave
boundary in the converted image; whether the pre-Belle ECam contract behaves
correctly on RM-807, how Belle orders callback
delivery and buffer release, and how to own a cancellable reserve/power/capture
session without blocking the event executor.

### 2026-10-04 — proposed resident development service

`DEVELOPMENT_AGENT.md` proposes an always-available but sleeping service,
with DNS-SD on active WLAN and a separately verified USB endpoint carrying
the same authenticated protocol. The startup hook, network advertising,
phone-side crypto library, USB client endpoint and platform permissions are
not yet proven for RM-807. Measure each before presenting it as a supported
capability. The host must retain paired identity across transport changes,
while never treating a USB port or IP address as identity. Per-stream credits
and bounded queues are proposed to avoid unbounded device memory or event
thread work. Firmware/recovery remains outside the service entirely.

The A11 reference has `WireStream` send/start/accept/half-close/drain/abort
semantics, MessagePack `WireMessage`, and `ChunkStoreReader`/`Writer` cursors
with cancellation, batching and admission versus persistence confirmation.
Those interfaces inform the design, while A11's 1,000-message/32 MiB default
stream buffer and its general node/action graph are too broad as initial
phone assumptions. A11's host HTTP stack uses an HTTP/1.1 codec, nghttp2,
OpenSSL and libuv/uvw; there is no verified reason to package that full stack
in the on-device service. The initial phone ceilings in `DEVELOPMENT_AGENT.md`
are proposed test values, not measured hardware limits.

The service's transition to hardware is now a separate plan gate after
emulator smoke tests. Open questions for that gate include the actual startup
registration API, installer signing/capabilities, physical idle cost and
whether the chosen phone transport stays available across sleep and USB mode
changes. Only a development phone with a separately held offline baseline is
in scope for the initial installation; no on-phone result is claimed yet.

### 2026-10-04 — default SDK Mbed TLS packages

The local `~/dev/mbedtls-symbian` port has a 3.4.1 CMake package with
`MbedTLS::mbedtls`, `MbedTLS::mbedx509` and `MbedTLS::mbedcrypto`, public
headers, and an Apache-2.0 notice. It compiles TLS 1.2 and TLS 1.3 client and
server sources, but configuration and compilation do not establish a working
guest connection. Fresh Release builds of all three archives passed on ARMv5T
and ARMv6 against the current SDK. Installing each export under a separate
architecture prefix allowed a tiny consumer to resolve the matching package
and compile a TLS-using static target on both architectures; an unrelated
target had no Mbed TLS link. The exporter helper then rebuilt and installed
both packages in a staged SDK copy. Its SDK manifest loaded and the independent
consumer built against that package on each architecture. A complete
from-source SDK install subsequently passed, including target runtime,
Abseil, device APIs, Mbed TLS and host tools. The installed manifest and
all six Mbed TLS archive digests matched the provenance and digest records;
ARMv5T and ARMv6 consumers compiled against that final installation.
Copy/install from the new active SDK preserved a relocatable package: an
independent ARMv6 consumer found its imported archive under the copied prefix
and compiled. The from-source SDK was reselected afterwards.

A configure check with the SDK's compiler wrappers deliberately absent (the
state during `prepare`) initially selected Apple's `/usr/bin/clang` for C
while C++ used LLVM 23.1.2. The exporter now passes the matching LLVM C
compiler explicitly. With both wrappers absent, C and C++ identified as
Clang 23.1.2, and a fresh ARMv6 build of all three archives completed.

The port currently requires application-supplied entropy, UTC conversion and
transport callbacks. Open questions: which Nokia 808 source yields sufficient
randomness; how to verify UTC and trusted roots across clock changes; whether
an authenticated TLS 1.3 handshake fits the phone's latency and memory budget;
and how to cancel stalled socket callbacks without blocking the event thread.

### 2026-10-04 — PC Suite MTP staging and resident UI follow-up

The connected Nokia 808 exposes a standard still-imaging MTP interface even in PC Suite mode. Native SendObjectInfo/SendObject followed by GetObjectInfo/GetObject successfully staged and read back one checked 806 KiB agent SIS in Mass memory/Installs. A repeat call found the same digest and made no new copy. This does not show that the phone's installer will accept the package or that the agent can run, remain resident or maintain TLS through sleep. The agent still carries a public emulator test key and loopback binding; physical identity provisioning and host reachability are open. A bounded physical development-phone gate remains required before any claim of compatibility.

The emulator's local BACK control leaves the service running while the UI is hidden; STOP exits cleanly in the opt-in guest test. Need separate measurement of actual phone background scheduling and idle cost. The macOS EKA2L1 movable-window patch builds; a live window accepted a System Events position change while the terminal remained frontmost. A literal pointer drag has not been separately observed.

### 2026-10-04 — resident agent and host UI questions

The agent's AppArc-compatible WindowServer group name and property-based panel raise compile and pass current guest startup tests. Does a second application-menu launch on the 808 deliver the expected `KErrInUse` startup path and raise the hidden panel? Install the revised 1.0.1 SIS on the development phone and verify this, along with BACK residency and STOP exit, before claiming phone behavior.

USB discovery remains unable to authenticate a live resident service while the phone listener is loopback-only. The console's owner-reported state is deliberately labeled as an observation, not a health probe. A bounded phone-reachable transport and identity provisioning are required for verified status.

On macOS, a normal EKA2L1 window accepts pointer dragging after its initial Qt show. Restoring terminal focus from the host launcher puts the regular window behind full-screen terminal windows; the older background-app transform leaves a visible but unselectable window. Find a launch method that preserves a selectable normal window while avoiding initial focus transfer. The current patch favors a usable draggable window and may briefly take focus.

The public TCP, TLS and system-counter APIs now use `absl::Duration`, and the agent uses `absl::Time` for request deadlines. `absl::Now()` follows guest wall time, which can change if the handset clock is corrected; the older service-local `steady_clock` calculation was monotonic. Verify clock-adjustment behavior in the emulator and introduce an internal monotonic adapter if a clock change can extend an aggregate request beyond its intended bound. Individual native socket operations remain capped at 60 seconds.

The agent's version 1.0.2 removes the 100 ms scheduler STOP poll in favor of a property subscription. This eliminates one known periodic idle wakeup, but the UI still has a five-second wake timer and the worker's idle cost is unmeasured. Measure wakeups, idle memory and battery effect on a development phone before treating background residency as suitable for daily use. The four emulator lifecycle tests do not exercise sleep/wake or the phone's application menu.

Symbian Foundation's [APGWGNAM.CPP](https://github.com/SymbianSource/oss.FCL.sf.mw.appsupport/blob/master/appfw/apparchitecture/apgrfx/APGWGNAM.CPP) confirms that AppArc names encode a two-digit status, UID, caption and document with NUL separators. The agent's manually encoded ready-state name follows that shape; the source alone does not prove that the Nokia 808 menu will reopen a lowered group. Install version 1.0.2 on the development phone and test BACK, menu relaunch, STOP and a second launch after STOP. The console cannot turn USB presence into authenticated running status while this agent is bound to loopback.

### 2026-10-04 — SDK resident-service abstraction

The native `CActiveScheduler` remains necessary for `ActiveTcpListener` and Window Server requests. The SDK's A11-derived `EventExecutor` owns a different request-semaphore loop and is not substituted into this active-listener path. `RunActiveService` packages the existing native active scheduler and stop property; the agent uses `WorkerExecutor` for TLS work. A separate Window Server thread is still needed for the panel's thread-affine request statuses. Verify on a development phone whether the panel's five-second wake timer and the service's worker thread meet idle-power and sleep/resume requirements. The public panel currently has a fixed layout and limited uppercase glyphs; broader UI should be designed separately if applications need it.

### 2026-10-04 — phone agent status after removal of TLS

The user chose an authenticated agent protocol without TLS. SDK TLS remains a separate opt-in application capability. The current agent's HMAC challenge response verifies key possession, but gives no traffic confidentiality. A phone-specific SIS contains the key in its executable; the locally stored SIS, generated build files and the on-phone staged copy must be treated as sensitive and must not be shared. This is a deliberately narrow read-only profile, not a basis for future write or debug permissions. The public emulator test key is rejected for Wi-Fi builds.

The revised 1.0.3 SIS has been staged and read back over PC Suite MTP, but the handset installer, guest network listener, entropy adapter, UI pairing label, actual Wi-Fi address and remote status have not been observed on the Nokia 808. The owner has been asked to install and open the exact staged file, compare the code, and provide the phone's Wi-Fi IPv4 address. A failed connection could mean installation, WLAN reachability, guest random generation, application capabilities or listener startup failed; it should not be flattened into an "agent absent" verdict. The current E32 converter has no nonzero capability support, while Symbian `NetworkServices` may be required for socket access on a real phone. Check the actual installer and socket error on the device before changing the converter or package policy. The unsigned SIS and emulator acceptance do not establish a signed, policy-compliant physical release.

The active-object listener and worker are bounded, but the phone's background behavior, sleep/wake, battery draw and panel reopen after BACK remain unmeasured. The five-second panel timer and the use of wall-clock `absl::Now()` for aggregate guest deadlines also remain candidates for a monotonic/idle follow-up. Do not enable boot start or broaden agent permissions from the present evidence. USB discovery remains inventory/staging; PC Suite MTP/OBEX is not a proven application socket transport.

### 2026-10-04 — phone Wi-Fi listener timeout

The owner reports the 1.0.3 agent panel and exact pairing code on the Nokia 808. The host can ping the phone's confirmed `192.168.1.32` Wi-Fi address, but a TCP connect to `39101` times out before authentication. The private build's generated header selects the all-interface listener; the equivalent profile passed the emulator guest check. The panel is started only after the agent's `RSocket::Open`, `Bind`, `Listen` and accept-arm path returns success, so an immediate listener-start error is unlikely. Incoming packet filtering, actual interface routing, per-connection policy and phone firewall behavior remain open.

The [Nokia Belle socket reference](https://cortex.p.gen.nz/nokia/symbian/Nokia%20Symbian%20Belle%20Developers%27%20Library/GUID-C6E5F800-0637-419E-8FE5-1EBB40E725AA/GUID-D4F08503-F1EF-3531-9C3C-4AF24A6255F0.html) says an implicit TCP socket accepts packets routed through any connection; the [Symbian platform-security migration guide](https://cortex.p.gen.nz/nokia/symbian/S60%205th%20Editiom%20C%2B%2B%20Developer%27s%20Library%20v2.1/GUID-35228542-8C95-4849-A73F-2B4F082F0C44/sdk/doc_source/guide/platsecsdk/migration.html) lists `NetworkServices` for opening TCP sockets. The current E32 converter emits zero capability bits, yet the owner-visible panel implies the socket setup returned success on this device. Do not treat capability omission alone as the proven cause. Check phone-to-Mac TCP reachability and the actual interface attached to the listener before changing image capabilities or adding an outbound relay. A direct packet capture on this Mac was unavailable because the session cannot open its BPF device.

### 2026-10-04 — phone-initiated discovery and deadline follow-up

The owner opened `http://192.168.1.203:39102/` in the Nokia 808 browser and saw `Symbian network check OK`. The temporary Mac server independently logged two HTTP GETs from `192.168.1.32`, the phone Wi-Fi address shown by the owner. This proves phone-to-Mac TCP for the browser in the current Wi-Fi session; it does not prove that the resident agent can send UDP broadcast or establish an outbound TCP session. The temporary server was stopped after the check. Direct Mac-to-phone TCP on 39101 still times out.

The next private agent profile sends a keyed UDP broadcast probe, verifies the keyed response and initiates TCP to that responder. No IP address is embedded in the package or typed into the console. Fixed UDP/TCP service ports remain protocol constants. The host listener responds only to a valid per-phone discovery MAC and still requires the existing fresh-nonce HMAC session proof before serving read-only status. Host behavior tests pass, and the new connectivity bridge compiled for ARM; emulator and physical UDP behavior remain under test. A failed status check should distinguish no discovery from discovery followed by failed TCP. If the handset routes broadcast on a different bearer or blocks UDP, inspect that specific failure before introducing a broader network manager or fallback.

The public TCP client API now uses `absl::Time` deadlines with `InfiniteFuture` defaults. The native timer has a 32-bit microsecond range, so a long deadline is implemented by waiting in multiple intervals without cancelling the pending socket request. A prompt guest receive with a two-hour deadline is the next behavioral check; source compilation alone does not establish timer behavior. Other native accept and UDP probe APIs still have their earlier bounded duration contracts and are separate follow-ups if applications need unbounded deadlines there.

### 2026-10-04 — phone discovery and process capability follow-up

The owner reports that version 1.0.4 is installed on the Nokia 808, yet the console returns `No keyed phone discovery arrived within 20s`. A separate host UDP socket bound to `0.0.0.0:39104` received **no raw datagram** during a 25-second window while the phone was connected. That observation cannot distinguish a guest send failure from WLAN bearer selection or broadcast filtering. The emulator's first private-profile run received the keyed broadcast but stalled on its response because EKA2L1 did not dispatch Belle socket opcode 0x2C (`socket_reform_so_recv_from_no_len`). The owned `belle-recv-from-no-length.patch` adds that dispatch; after rebuilding EKA2L1, the private guest discovery and authenticated status test passed. Emulator behavior is not Nokia 808 evidence.

Inspection of the version 1.0.4 build path found that the E32 converter always emitted a zero process-capability mask. Belle's original `e32capability.h` assigns `NetworkServices` bit 13, and Symbian's published network migration notes identify it for socket access. This is a plausible cause of an on-phone send failure, but the socket's actual return code was not captured and the phone's installer policy is still unknown. The converter now supports **only** an explicit `NetworkServices` opt-in; agent version 1.0.5 requests it. Recheck raw UDP arrival and authenticated status after the owner installs that package. If discovery is still absent, inspect the phone's selected IAP and whether limited broadcast reaches the Mac before adding another discovery transport.

### 2026-10-04 — signed agent package after installer rejection

The owner tried the capability-enabled **unsigned** 1.0.5 SIS on the Nokia 808 and the installer said `Requested application access not granted`. The E32 capability bit alone does not authorize installation. Symbian's published CreateSIS/SignSIS documentation describes signing and device-dependent acceptance of self-signed packages. The SDK now self-signs the phone-specific agent with a separate per-phone RSA identity stored outside Git; its native SIS inspector verifies the signature and reconstructs the canonical package before accepting its files. This establishes package integrity on the host, not the phone's grant decision.

The signed 1.0.6 SIS has `NetworkServices` bit 13 in its inspected E32 image and SHA-256 `b0fe48f06c679ef94960aafaa2bc4cf0bc3e74c3832ac679d0e6c6616f6fd0fa`. PC Suite MTP staged it at `Installs/agent_service-b0fe48f06c67.sis` and verified readback. The owner has been asked to try that exact file. Open questions: will Belle accept this self-signed signature and grant NetworkServices; if so, will the agent send keyed UDP discovery on the WLAN bearer and connect back to the Mac; and how does the resident process behave across BACK, lock and sleep? Do not infer any of these from host inspection or emulator execution.

The native absolute deadline bridge retains 64-bit Unix microseconds only where an arbitrary absolute deadline and remaining interval require them; individual `RTimer` slices remain 32-bit. A guest regression caught a stale-clock calculation that allowed a 50 ms receive to succeed after 150 ms; computing the remaining interval once and subtracting completed slices restored the timeout test. Verify clock-adjustment and sleep behavior separately on a handset. The no-suffix public TCP, listener, broadcast and TLS methods now take `absl::Time deadline` with an infinite default; call sites set bounded operational deadlines.

The owner also tried the signed 1.0.6 SIS and received the same access-grant error. Inspection of the original Symbian installer source then identified a concrete package mismatch: `CSISFileDescription` has an optional `ESISCapabilities` field, and `SecurityCheckUtil::CompareHeaderAndExeCapabilities` requires it to match the executable's E32 capability set. The SDK's SIS builder had omitted that field even when the E32 declared `NetworkServices`; signing alone could not correct this. The native package writer now derives `ESISCapabilities` from the inspected executable and the inspector verifies the canonical field. This is a reusable package rule for every application, not an agent-specific byte patch.

Version 1.0.7 was rebuilt with `0x2000` in both E32 and SIS file description and a verified RSA signature. Its SHA-256 is `77391387488a6b19b9c7974f9d0531d4d1bbd5b7844be52a1e3f30e3cd3b36ac`; MTP staged `Installs/agent_service-77391387488a.sis` with matching readback. The owner has been asked to test installation. Whether the 808 accepts this corrected self-signed package is still open. The general package CLI now accepts a certificate/key pair; identity generation is a reusable host policy module.

The owner confirmed 1.0.7 **did install** and its panel showed `PAIR OORGRKIN`. A Mac listener then validated a keyed discovery packet from the phone, but no TCP connection followed within 60 seconds. Thus the previous installer and UDP egress questions are resolved for this handset/session, while UDP offer reception/validation and outbound TCP dial remain open. The owner has not yet reported whether BACK relaunch, STOP, sleep or idle behavior work on the phone. A generic resident-panel heading callback and atomic agent link phases were added to show the failing stage without exposing the private protocol key. Version 1.0.8 was built against a newly exported active SDK and staged with MTP readback; its on-phone diagnostic result is pending. The emulator's private discovery/authentication case passed on that SDK but cannot answer the phone-side transport question.

The owner installed/opened 1.0.8 and reported the heading rendered as `NO O  ER`, the bitmap font's omission of `F` in `NO OFFER`. During a concurrent 60-second host listener, **no keyed UDP packet arrived**, whereas the 1.0.7 attempt had yielded at least one. A ping to the phone's earlier `192.168.1.32` address also timed out twice while the Mac remained on `192.168.1.203/24`; ARP still had an entry, which can be stale. The present evidence points to an intermittent bearer/reachability problem, not proof of a persistent protocol error. The owner has been asked to check the phone's current WLAN state/address. The missing F/D/H/L glyphs were added in source; the installed 1.0.8 SDK/UI image does not yet contain them.

The owner confirmed that the phone still showed `192.168.1.32`. A repeat ping returned two replies (about 593 ms and 201 ms), and a fresh 45-second `symbian agent listen` completed in under five seconds with a verified phone-initiated session from that address. The response contained authenticated status (`ready`), `status` and `logs` capabilities, display dimensions and a live tick counter. Thus the installed 1.0.8 profile can perform keyed discovery, receive the UDP offer, dial TCP, authenticate and serve a request on this Nokia 808. A second check through `ConsoleWebBridge.verify_agent_status` using the USB-discovered phone and the console's normal 20-second listener also returned `authenticated=true`, `ready`, live status and pairing code `OORGRKIN`. This checks the GUI backend, while an actual pywebview button run remains unobserved. The earlier failure appears transient, but its cause is still unknown. Follow-up questions: does it recover after WLAN reconnect, lock/sleep and reboot; and do BACK/relaunch/STOP behave as intended on device?

The owner then reported a phone reboot while a later host-only listener waited for another session. That listener received no connection and was cancelled; no request was sent during that attempt. No reboot command exists in the agent control protocol. The owner is unsure what preceded the restart. This does not establish that the agent caused the reboot, nor does it exonerate its ongoing background activity or earlier sessions. Physical testing is paused. Investigate whether the process survives normal idle and whether the reboot is reproducible only after agent startup; avoid inferring a platform cause from the host trace alone. The later host-only log reader and console UI changes have no Nokia 808 validation yet.

A subsequent owner screenshot shows the actual desktop console card rendering `Verified live · ready` for the 808 at `21:43:45.038471Z`, with Wi-Fi peer `192.168.1.32`. This confirms that the pywebview frontend displayed an authenticated result, beyond the earlier direct bridge call. It does not reveal whether the agent was restarted manually after the phone reboot; that lifecycle question remains open.

A repeat of the opt-in Dynarmic resident-agent lifecycle test passed after adding a `recent_logs()` assertion to its authenticated status/log session. It exercises emulator status, wrong-key rejection, malformed-frame recovery and reconnect, but has no phone power or kernel instrumentation. The physical reboot remains unexplained and should not be interpreted through this emulator pass.

## 2026-10-04 — workspace listing gate

The next agent increment adds only a fixed-root, read-only workspace listing
to the existing authenticated session. A missing directory is intended to
read as empty; that branch has not yet been exercised in the emulator and
native `KErrPathNotFound` mapping needs checking. Pagination is offset-based,
so concurrent directory changes are not a snapshot. The 256-entry scan bound
limits guest work per request. No Nokia 808 build of this increment has been
installed while the earlier phone reboot is unexplained.

## 2026-10-04 — nghttp2 and ngtcp2 for the resident agent

The owner asked whether these libraries can serve the development-agent transport. The worktree is clean at pushed commit `0264821` before this review. A11's host HTTP stack already links static `libnghttp2` through `pkg-config` and wraps its byte-oriented callbacks in its own HTTP/2 connection and event loop. This Mac has libnghttp2 1.70.0 and a static archive. [nghttp2's upstream requirements](https://github.com/nghttp2/nghttp2#requirements) say its C library can be built alone without the application programs' TLS, libev, zlib and c-ares dependencies. Its [programmer's guide](https://nghttp2.org/documentation/programmers-guide.html) says the library performs no socket I/O; the caller supplies transport and lifecycle. Thus a future optional guest HTTP/2 client or server is plausible, but guest ARM compilation, ALPN over the SDK's Mbed TLS port, binary size, heap use, callbacks and cancellation are **not yet tested**. The resident agent presently speaks a small authenticated control protocol and has no HTTP interoperability need, so adding nghttp2 now would expand its protocol and memory surface without satisfying the next files/logs gate.

[ngtcp2 upstream](https://github.com/ngtcp2/ngtcp2/blob/main/README.rst) describes a dependency-free C11 QUIC transport core, but its usable crypto helpers require a supported QUIC TLS backend. Its current list covers GnuTLS, BoringSSL/AWS-LC, Picotls, wolfSSL, LibreSSL and experimental OpenSSL 3.5; it does not list Mbed TLS. The [ngtcp2 programmer's guide](https://nghttp2.org/ngtcp2/programmers-guide.html#tls-integration) recommends those helpers and details the work needed for custom TLS integration. The SDK's Mbed TLS 3.4.1 tree has no QUIC-specific public integration API; [Mbed TLS's open QUIC API request](https://github.com/Mbed-TLS/mbedtls/issues/4731) is consistent with that local inspection, though absence of a supported backend is the decisive current limit. HTTP/3 would also require [nghttp3](https://github.com/ngtcp2/nghttp3/blob/main/README.rst), not nghttp2. No guest QUIC/TLS handshake, size or battery result exists. Do not add ngtcp2 to the SDK or agent until there is a concrete QUIC use case and a maintained compatible TLS backend that passes guest and physical gates.

## 2026-10-05 — A11 WebSocket framing over guest nghttp2

The owner requested WebSocket transport for the resident agent and verbatim
A11 reuse wherever the runtime permits it. The codec now uses RFC 8441
extended CONNECT over HTTP/2 prior knowledge, with RFC 6455 binary messages.
A11 revision `fcccb8cb6e1e67d7ba0822ac14cece9ee4c7091b` supplies the
opcode/endian/masking helpers, ParsedActions and ParseFrames body verbatim.
The recorded parser-body comparison passed after formatting. WriteFrame keeps
the A11 implementation with explicit single-owner/entropy/output adaptations;
its unavailable asynchronous HTTP body stream is replaced by encoded bytes.
Provenance and licenses are shipped. nghttp2 v1.70.0 is pinned to
`85e300c79fb6dbcfa9c1013215c8710c1c2cd3d2`. The owner-supplied archive has no
.git directory: a Git query there initially inherited this repository's HEAD.
An independent upstream checkout at that release and SHA-256 comparison of
all declared core inputs corrected the provenance; builds check the manifest.

Native socket-independent client/server endpoints and worker-owned TCP stream
and listener classes are reusable. Python owns socket/deadline policy and uses
the native codec with the GIL released; it contains no duplicated format
implementation. Existing agent HMAC authentication, native MessagePack and
length framing remain inside binary messages. The phone profile is a WebSocket
client after keyed UDP discovery; the public emulator profile is a server.
Agent 1.1.0 and the updated host must be deployed together. There is no browser
HTTP/1 Upgrade compatibility or TLS claim for this cleartext transport.

Guest runtime additions include the steady clock adapter for nghttp2's rate
limiter, OpenC C-declaration compatibility in inttypes.h, byte-order adapters,
calloc's original libc export (ordinal 47), and the original RSocket SetOpt
integer overload (ESOCK ordinal 231) for TCP_NODELAY. Two earlier exported
SDKs linked nghttp2 but failed at the actual application link because those
imports were absent; neither ARM archive generation nor E32 conversion alone
proved their availability. The verified export includes both imports and
builds the nghttp2 core, codec and TCP wrapper on ARMv5T and ARMv6. Its exported
file digests were independently checked with no mismatches. Defaults bound
messages to 4,100 bytes, queues to 64 KiB and 16 received messages, and headers
to 2 KiB/16 fields; configured messages are capped at 32 KiB. HPACK dynamic
tables and server push are disabled, with bounded SETTINGS/ACK/continuations.

Nine native GTests passed. Thirteen focused WebSocket/agent-session Pytests
passed, including independent hyper-h2 peers in both directions. The final
SDK/build/identity/transport host selection passed 25 tests with one opt-in
skip. A wider console run encountered the inherited catalog/presentation
mismatch for agent commands; that broader suite is not claimed passing.
A host RelWithDebInfo in-memory codec benchmark completed 100,000 round trips
of 4,096 bytes with OpenSSL mask entropy: 1.77414 microseconds per round trip,
4,403.55 MiB/s aggregate payload. This excludes TCP, guest execution and Python
policy; no direct A11 performance comparison or phone energy result exists.

The named RM-807 preserved-firmware Dynarmic public guest suite passed all four
cases. The first private client run reset the host connection and recorded
KERN-EXEC 3 with a stack write fault. Its long-running discovery/connection
callback used stackless WorkerExecutor::Post, unlike public sessions' 256 KiB
fiber stack. Moving it to the same A11 PostFiber stack passed the private
keyed-discovery/authentication/status test (11.01 seconds). A compile gate
caught PostFiber's Task return type during that change; startup now inspects
an immediately ready result for admission failure. No alternative scheduler
was introduced. These are emulator results; physical installation, reconnect,
sleep/reboot behavior and the earlier unexplained phone restart remain open.

The IDE initially loaded an active SDK without Symbian::WebSocket. Activating
the verified SDK made the owner's exact CLion configure command succeed.
The owner then clarified that SDK development must consume current sources.
The root guest graph now loads the current capability template and redirects
runtime/API/WebSocket/TLS/guest-fiber capability archives to their real local
CMake targets, with build dependencies and current public headers first.
Installed platform headers/proxies and pinned Abseil/Mbed TLS remain external
inputs. ARMv6 and ARMv5T agent ELF links passed using the local libraries;
ARMv5T's TLS wrapper also built. Enabling the current stream runtime exposed
a missing rune-table compile definition on the Runtime facade; propagating
the source runtime definitions fixed it. An old ARMv5T compiler cache switched
to host detection during CMake's automatic reset; explicit --fresh restored
the ARM profile. This is compiler/index/build evidence, not a new guest loader
or direct observation of IDE editor navigation.

Final export/activation: `.symbian/websocket-workspace-sdk-20261005` packages
the current capability template and both architecture archives. All recorded
file digests match after tooling installation. ARMv5T core/codec/wrapper
archives are 313,096/63,324/17,248 bytes; ARMv6 are
312,660/63,124/17,200 bytes. These are archive file sizes, not resident heap
measurements. Both IDE compiler caches were refreshed against that export;
the owner's exact ARMv6 command then configured successfully. Both profiles
linked the agent and built the TLS wrapper from workspace archives. A separate
root configuration against the old SDK without WebSocket also succeeded,
confirming that capability discovery uses the current workspace template.
Compilation database checks locate real ARM commands for the agent, wrapper,
codec and worker implementation, with current API/runtime headers ahead of
exported copies. Native tests passed again, Black/Ruff and whitespace checks
passed, and strict MkDocs succeeded. Against the final exported SDK, the public
RM-807/Dynarmic suite passed 4/4 in 46.73 seconds and the private case passed
in 14.91 seconds. No physical-device test was performed.

## 2026-10-05 — Standalone emulator launch and console signing

- Added `symbian emu run` and Console → Emulator → Run emulator. This uses the
  existing resolved/verified firmware copy, owned frontend, private control
  socket, retained launch/log evidence and teardown. It omits `--run`, builds
  no application, injects no executable and requires no guest application exit
  report. Project/SDK arguments select settings only. The EKA2 application ABI
  gate remains on application launch; standalone EKA1/EKA2 policy branches have
  controlled-child coverage, not new EKA1 runtime evidence.
- Live experiment used the selected 808 PureView baseline
  `sha256:e4502051aee69bb1b7060f47c4e5d5dbf63db3277f8ea918a81076b43ab47e8d`.
  The real patched frontend reached its control endpoint with
  `{"process_exits": []}` and no injected E32/ELF digest. Evidence is retained
  privately under `.symbian/emulator-runs/run-05b_r461/`. Context teardown
  escalated from terminate to kill (`frontend_exit: -9`); the child was reaped,
  endpoint removed and all 13,438 baseline files retained their recorded hashes.
  This proves frontend/control launch and ownership, not full OS boot or a
  normal human window-close path.
- Console → Signing has a live public certificate listing and guided creation,
  PEM RSA pair import, archival and signing of existing SIS applications.
  Identity picker entries come from that store; imported subject text is escaped.
  Default storage follows XDG data paths outside projects. Private directories
  and key files use 0700/0600; duplicate identities are refused. Archival retains
  keys in `.archive/`. OpenSSL checks pair matching and public metadata; the
  existing native `sign_sis` and `inspect_sis` own SIS/signature processing.
  Existing output files and inputs are preserved, and invalid imports/packages
  publish no identity/signed output. No private key bytes enter GUI results.
- Regression command covered signing, standalone/IDE launch, CLI, console,
  web frontend, packaging, emulator/control and firmware tests: 112 passed,
  27 optional tests skipped, one inherited failure in
  `test_catalog_tracks_cli_and_excludes_frontend` (agent hello/status/files/
  listen/logs have CLI leaves without console presentations). Follow-up after
  adding the supervisor no-guest-exit check and final frontend changes:
  standalone + web frontend, 22 passed. Black/Ruff, Node syntax and strict
  documentation build pass for the changed code.
- Open questions: normal user window-close acceptance, interactive desktop
  placement and complete firmware OS boot remain separate checks. Encrypted
  private keys and broader SIS profiles are unsupported by this workflow;
  self-signed certificates do not establish phone trust or capability rights.

## 2026-10-05 — Stage context applications for standalone emulator launch

- Follow-up changes standalone launch to build and inject an application when
  `--project` names an application or the CLI workspace/current directory
  declares one. GUI Run emulator already carries its resolved application
  folder; a regression now verifies that default. Without an application
  context, standalone launch still performs no application build. Automatic
  execution remains disabled and no guest application-exit report is required.
- Application validation, architecture/import compatibility and the EKA2 ABI
  gate apply whenever an app is staged. An executable is copied to the private
  C drive. For registered apps, the existing SDK resource compiler supplies
  registration, translated captions and MIF assets; the selected CA bundle is
  also staged when configured. Host path checks apply to these generated asset
  targets. Resource compilation accepts the already selected application SDK.
  No SIS/resource format parser, scheduler or native implementation was added.
  Launch evidence records the staged app identity, assets, E32/ELF hashes and
  `auto_run: false`.
- Initial experiment built a registered SIS and used upstream `--install`.
  The real installer logged success but the CLI handler converted its enum
  success value (zero) to false and exited 255. Failure evidence remains in
  `.symbian/emulator-runs/run-jinr6h2y/`. The maintained workflow instead stages
  compiled resources directly; upstream code and the native installer were
  not changed, and no failed status is treated as success.
- Successful real RM-807 frontend experiment is retained at
  `.symbian/emulator-runs/run-av0gq61g/`, using the previously recorded
  `sha256:e4502051aee69bb1b7060f47c4e5d5dbf63db3277f8ea918a81076b43ab47e8d`
  baseline. The frontend command contained only `--device RM-807`;
  control reported no guest exits. All seven app files were present in the
  copied C drive. AppArc logged `Found app: Symbian GUI Counter, uid:
  0xE0000811`, independently establishing menu discovery before manual launch.
  Teardown reaped the owned child and all baseline hashes remained unchanged.
  This establishes staging/frontend menu discovery, not new guest execution,
  full OS boot, normal human window-close or physical installation evidence.
- Focused checks: 70 passed, 9 optional checks skipped across standalone/IDE
  launch, CLI, web console, packaging and CA bundle tests. Controlled-child
  cases cover explicit/implicit app context, registered/raw apps, absent app
  context on EKA1/EKA2, no automatic `--run` and retained baseline integrity.
  Changed Python passes Black/Ruff; strict documentation build passes.

## 2026-10-05 — Built-in RM-807 applications and Gallery black screen

The owner reported Gallery opening to a black screen after standalone emulator
launch. Fresh-process UID controls establish that execution is not restricted
to the injected context application. Both the upstream `--run UID` handler and
Qt app-list activation use `applist_server::launch_app`. No launch selection,
signing, staging or emulator implementation was changed in this investigation.

Fixture: content identity
`e4502051aee69bb1b7060f47c4e5d5dbf63db3277f8ea918a81076b43ab47e8d`,
808 PureView/RM-807, epoc100 detector result, explicit
`rm807-113.010.1508` executive profile, ROM digest
`b5c1ea63cb6359270c5b7cfb1bb453594e208a01b8aeb5b5e020f37d546f7086`.
The real local patched macOS frontend was used. Its source checkout has other
existing local changes; these results characterize that binary, not pristine
upstream. Every case copied the verified baseline into disposable writable
state. All 13,438 baseline file hashes agreed after every case.

Private replay scripts, individual `report.json`, frontend logs and real screen
texture PNGs are under `.symbian/app-launch-investigation-20261005/`.
`probe.py` runs the six-app comparison, `controls.py` the longer wait/backend
controls, `isolation.py` the no-injection and Calculator input controls, and
`dyncom.py` the successful short Dyncom reproduction. They use the native
control socket and stop/reap only their owned children. Each report includes
fixture identity, profile, command, exit records and baseline verification.

| App / UID | Evidence directory | Observed result |
| --- | --- | --- |
| Context counter / `0xE0000811` | `context-bh5b49ax` | Nonblack 720×1280 frame, no process exits |
| Gallery / `0x200009EE` | `gallery-sug7fqf0` | Entire RGB texture zero, frontend and Gallery remain alive |
| Gallery/Photos / `0x200104E7` | `gallery-photos-pkzs28zo` | Same black texture and service exit pattern |
| Calculator / `0x10005902` | `calculator-05b4tr5y` | Actual calculator initial UI |
| Clock / `0x10005903` | `clock-l7__ig_m` | Black texture, DBMS server exits zero; cause unresolved |
| Settings / `0x100058EC` | `settings-akrlaxs7` | Actual initial Profiles/Themes/Phone/etc. list; subviews untested |

Additional controls:

- `gallery-long-sd9iq_xr`: still entirely black after 30 seconds on Dynarmic.
- `gallery-no-context-d7ai6lfv`: no injected SDK executable/resources; same
  black texture and media service failure after eight seconds.
- `gallery-dyncom-short-4qwntkgn`: no context injection, Dyncom; same black
  texture and Harvester exit after eight seconds.
- `calculator-input-4ryjjtrq`: no context injection. Real logical pointer
  press/release at (225,482) changes the displayed digit from 0 to 3. Both
  before/after texture captures retained. No Calculator process exit.
- Initial long Dyncom case `gallery-dyncom-wamjccti` lost its endpoint during
  the wait and the frontend ended with zero; no texture was captured. Treat
  that attempt as inconclusive, not a successful frame or a backend crash.
  The fresh short Dyncom case supplies the actual reproduction.

Confirmed runtime observations and source contracts:

- Both Gallery registrations start substantial real guest code and services.
  Native status records `harvesterserver[200009f5]0001` exiting normally with
  reason `-2` (`KErrGeneral`), cryptospisetup and BlacklistServer exiting zero.
  No Gallery process exit is reported. This is a live guest with a failed media
  dependency, not an absent executable or an emulator-wide rendering failure.
- Gallery logs unsupported `!Loader` IPC `0xB` (`ELoadFSPlugin`), file-server
  108 (`MountPlugin`), 111 (`PluginOpen`) and unidentified opcode 149. The
  existing frontend completes these unsupported requests with -5; it does not
  implement the file-system plugins. Original MDS file monitor initialization
  calls AddPlugin/MountPlugin and opens its plugin engine; these are real media
  stack dependencies. This proves missing support, not that one request alone
  causes Gallery's first-frame failure. Do not fake success for those requests.
- Missing SVCs in Gallery are `0x55`, `0x74` and `0x7B`. The previously retained
  native-ROM/source-wrapper mapping identifies them as RTimer::Inactivity,
  RProcess::Open(TProcessId, TOwnerType) and User::RenameProcess respectively.
  Source executive numbers 0x53/0x72/0x79 map to these Belle numbers. They are
  absent from the v10 handlers used to derive the experimental profile;
  process open/rename handlers exist in the older v9 map. No permissive map or
  synthetic inactivity completion was added. Their direct causal role in the
  black screen remains unproven.
- Repeated missing property `0x20022E94`, key 2 is the MDS **shutdown** property,
  not a proven readiness signal. Original MDS definitions name it
  `KMdSPSShutdown` / `KShutdown`. Do not infer an initialization gate solely
  from that warning. Trapped leaves, missing optional settings and skin errors
  likewise need causal controls before being called fatal.
- Original MDS source was read from ignored upstream checkout
  `research/upstream/mds`, repository
  https://github.com/SymbianSource/oss.FCL.sf.mw.mds, commit
  `6a336727266557c351987111e793a270f86a6291`. EPL notices remain untouched.
  Relevant files: `inc/mdscommoninternal.h`,
  `harvester/monitorplugins/fileplugin/src/filemonitorao.cpp`, and
  `harvester/server/src/harvesterserver.cpp`. The server's outer trap returns
  a startup/runtime leave code from E32Main, consistent with a normal -2 exit;
  this is source comparison, not matched firmware debug symbols.

Conclusion: launch and staging work for applications beyond the context app.
Gallery exposes incomplete Belle/media/file-system/executive support, while
Clock is another unresolved compatibility case. Calculator has real frame/input
acceptance, Settings initial-frame acceptance only. This is neither full OS boot
nor general built-in-app or physical-phone compatibility. Teardown usually
escalated to kill (-9); children were reaped, but this is not normal human
window-close evidence.

Open questions: locate Gallery's exact blocked thread/request and Harvester's
original -2 leave using bounded debugger/service tracing; independently verify
file-system plugin contracts and timer-inactivity semantics before implementing
fixes; diagnose Clock separately. Rendering Gallery requires a successful
before/after guest oracle, not removal of log warnings. Updated the emulator
user guide with the measured compatibility boundary. No firmware/private
screenshots, runtime state or upstream checkout was added to version control.

Verification: five retained Gallery texture captures independently checked as
all-zero RGB; Calculator before/after captures differ and visual inspection
confirms 0→3. All per-case baseline-verification fields are true. Strict MkDocs
build and `git diff --check` pass. No production code changed in this
investigation, so no additional unit-test suite was required.

## 2026-10-05 — Shared native HTTP, WebSocket and TLS streams

Implemented protocol-neutral byte streams, ordered HTTP heads, A11-derived
HTTP/1 codecs, bounded pull bodies and explicit Write/Finish writers. HTTP/2
DATA/flow control is shared with RFC8441 WebSockets, using pinned nghttp2;
there is no second scheduler. Native TCP DNS resolves an ASCII hostname to its
first IPv4 address. Mbed TLS client and server share the same stream/BIO with
mandatory outbound chain, date and hostname verification, SNI and explicit
TLS1.2/TLS1.3 plus ALPN. EOF without close_notify is a TLS truncation error.

Final exported-SDK acceptance: 11/11 passed in 76.47 seconds, replay with
SYMBIAN_HTTP_LIVE_GUEST=1 SYMBIAN_HTTP_EXPORTED_SDK=1 and
SYMBIAN_SDK_MANIFEST=.symbian/http-final-sdk-r2-20261005/sdk.json,
pytest symbian/tests/test_http_guest.py. Log:
/tmp/symbian-http-exported-r2c.log; artifacts:
.symbian/http-exported-r2c-20261005. Guest DNS connected to example.com over
HTTP and verified TLS1.2/HTTP1.1 and TLS1.3/HTTP1.1; www.cloudflare.com over
verified TLS1.2/h2 and TLS1.3/h2, including streamed megabyte-sized bodies.
Wrong-hostname controls reject both TLS versions. Independent host clients
exercise native HTTP1.1/h2c servers and mutual-authenticated
TLS1.2/HTTP1.1 and TLS1.3/h2 servers. Test roots come from the host trust store;
server/client test identities are the existing public fixture certificates.
This is RM-807 Delight 113.010.1508, ARMv6, Dynarmic emulator evidence;
ARMv5T compilation and physical-phone compatibility are separate gates.

The preserved baseline ROM/Z hashes remain unchanged. EKA2L1 mutates only
disposable Z: avkonfep.dll/goommonitor.dll are renamed to hash-identical .bak
files, slpgw.dll removed and stubcached created. The test records these changes
and verifies unchanged content; the first overstrict copy-equality assertion
failed and is retained rather than described as preservation failure.

Retained integration failures: mixing the two complete Runtime/Streams
archives caused duplicate std::to_string definitions, fixed by using Streams
consistently for networking/TLS. DNS revealed absent SDK proxy imports for
RHostResolver and TSockAddr/TBufBase constructors. Added original esockU
ordinals 120/122/125/127/262, EUSER C2 ordinals 1473/85; C1 ordinal1469 alone
was insufficient. The final export was supplemented then resealed; exporter
source includes these symbols for subsequent clean prepares. Supplement replay
is /tmp/symbian-fix-dns-proxy.py and /tmp/symbian-http-dns-proxy.log. Earlier
passed tests used host-resolved addresses; final 11 cases use guest DNS.

Final host regressions: 101 passed, 17 opt-in skipped, in
/tmp/symbian-outstanding-final-regressions.log. Fixed the inherited five agent
CLI presentation omissions. Native CTest: 14/14 passed, including eight HTTP
and nine WebSocket tests (/tmp/symbian-http-final-native-tests.log). Final
ARMv6 HTTP probe and connectivity archives built; ARMv5T had earlier compiled.
Complete HTTP/TLS/WebSocket documentation examples compile as one ARM TU at
.symbian/http-docs-20261005/examples.cc. Strict MkDocs, Black/Ruff and whitespace
checks pass. Status discards now use IgnoreError including the pinned A11
continuation cancellation (adaptation/hash recorded); local bool-returning
Future cancellation/value setters retain ordinary casts. An attempted blanket
replacement exposed that bool distinction at compile time and was corrected.

Restrictions: one exchange/connection, bounded buffering, no redirects,
connection pooling, decompression, HTTP1 Upgrade or HTTP2 multiplexing/push.
Streaming exposes independent read/write halves and caller deadlines; native
synchronous work belongs on the existing SDK executor. Internet acceptance
requires live DNS/network and the selected public roots; it is not hermetic.

## 2026-10-05 — First executable EKA1 profile

After committing/pushing the HTTP and outstanding work, implemented the first
slice from eka1-plan.md using the existing Clang/LLD, CMake/Ninja and native
E32 converter. There is no new compiler/runtime/scheduler or EKA1 emulator
patch. The profile is e32-eka1, ARMv5T, one RX PIC mapping, callable ARM entry
with integer return, legacy 124-byte header/CPU0x2000 and no security extension.
Native inspection now identifies the kernel. Imports, writable data/BSS,
absolute pointer fixups, exports and lifecycle/unwind metadata are rejected.
The example explicitly excludes cantunwind metadata. SISX now rejects EKA1
instead of accidentally accepting its newly inspectable header.

Selected preserved Nokia 7610 RH-51, epoc80, firmware identity
80c85c43e74cd6f6bc2a071e32d2efdebe1fa0a3c5352c08217327b6415bfbe3.
ROM SHA256 a5a2b1fb499410ced638ad590edc7dfbfcd491839919d1a11cf58f76c64aee9a;
EUSER SHA256 7ae4317ffb1fc4f29506439bc6a21392fe9dd9bfdfd75c07a9727ed677cabd51.
Original instrec.exe is a ROM image (not disk E32), SHA256
bedf1cbbe7c032a45d13ffb4d68d82bb74e2f617e4e78c243f85330bb52e3422:
UID1 0x1000007a, priority350, flags2, Thumb entry0x50adc225,
code address0x50adc224, no data/BSS, ROM DLL reference table. Header and entry
observations remain private at
.symbian/eka1-20261005/original-rom-contract.json. ROM import references cannot
be treated as EKA2 eager-import or legacy disk PE tables.

Pinned EKA2L1 2594edf4d6bf55d7bd3f0b46250fe2318d4dc2e8 supplies an existing
EKA1 thread bootstrap in kernel/src/libmanager.cpp and thread.cpp. It sets up
the heap with original EUSER ordinals166/1127, invokes DLL attach entries,
calls the image entry and forwards r0 to ordinal397 Thread::Exit. The callable
ARM entry returns through that bootstrap; it does not use an EKA2 marker or
Belle SVC0x73. This is a dependency on the tested emulator's EKA1 bootstrap,
not proof of a physical phone's entry contract. Executed local frontend
SHA256 c32f93f1b67921ca8c7dd1933fc099eb860d876f68eddaeda281a133d3e97982
includes maintained patches and preexisting research changes; no pristine
upstream binary claim is made.

Normal image SHA256
64238a00231c929bf395e1534476d0b615fa2e51dbedc92df460df4236bf9589;
changed-return image SHA256
d3b619814060b4651fd875a037bd093dac9ad0d6c10e8752877eb5340bc60dea.
Both images reproduce in two separate CMake trees. Both Dynarmic/Dyncom
record normal process exit type0 with reasons7610 and7611 respectively,
frontend exit0. Both changed-return checks against expected7610 fail with
FAILED_PRECONDITION and retain accepted=false. All golden manifest hashes
match before/after every run. Only disposable instance contents are writable.

Replay: SYMBIAN_EKA1_GUEST=1 pytest symbian/tests/test_eka1.py
symbian/tests/test_eka1_guest.py: 11 passed in 12.98 seconds, including
four preflight policy cases and four both-backend execution cases (six
actual guest runs with mismatched controls). Final log
/tmp/symbian-eka1-acceptance-r3.log and evidence
.symbian/eka1-final-r3-20261005. Independent EKA2L1 parser test:
SYMBIAN_EKA1_TEST_IMAGE=.symbian/eka1-20261005/build/eka1_probe.exe,
platform-tests/symbian_e32_oracle --gtest_filter=Eka1OracleTest.*;
/tmp/symbian-eka1-oracle-tests.log. It accepts original-format CPU/header,
entry, absent imports/security/data/relocations and rejects damaged UID and
truncated code. It does not verify historical code checksum semantics;
our native checker validates the emitted profile's additive word checksum.
EKA2/V original-loader validation is not an EKA1 oracle.

The actual CLI toolchain verify-eka1 passed on Dyncom:
/tmp/symbian-eka1-cli.log, .symbian/eka1-20261005/cli-dyncom/report.json.
It uses resolved firmware/frontend/backend settings, requires a new output
outside preservation, rejects wrong image/fixture/exec map, bounds execution,
reaps only its child, checks the native process exit rather than frontend
exit alone, and retains firmware/image/frontend identity and logs.

Retained failures: first execution reached reason7610, but reporting a Path
inside the command list failed JSON serialization; string conversion fixed
report publication, and fresh runs retained the full native exit. The initial
SISX rejection test loaded the older installed native module and failed;
a rebuilt, atomically replaced module passes it. Initial EKA2 regression
selection found two stale test assertions: code size assumed no relocation
section despite the probe's existing EHABI descriptor, and expected obsolete
CMake input-error wording. Updated those assertions to the current preserved
behavior, with the same loader/profile checks; final selection passes.

Native tests: all14 CTest targets pass; E32/ARM-attribute suite39 tests pass,
including EKA1 truncation, identity, ISA, entry, unsupported data and pointer
fixup controls. Host EKA2/firmware/CLI/console/signing selection91 passed,
15 opt-in skipped (/tmp/symbian-eka1-regressions-r2.log). Strict MkDocs,
Black/Ruff and whitespace checks pass. Full restrictions and replay are in
EKA1.md and the user guide. P900, C++/import ABI, modern libraries, networking,
GUI, legacy SIS, debugging and physical-device compatibility remain gates.

## 2026-10-05 — Original EKA1 PE imports and balanced heap calls

Continued the accepted no-UI Nokia 7610 slice with native legacy PE imports.
The native converter reuses retained ELF/proxy/PLT validation, keeps the final
GOT function slots as a contiguous IAT at text_size, appends its zero terminator
and emits ordinals in the import section. Inspection checks canonical section
encoding, ordinal/IAT equality and bounds. One function-only euser.dll block,
ARMv5T and no writable data/fixups/lifecycle remain the supported boundary.
Python copies proxy bytes under the GIL and delegates all format work natively.

Symbol/ordinal facts come from pinned EKA2L1 bridge/epoc6_n.def; provenance and
its hash are in probes/eka1_import_probe/euser-source.json. Original firmware
EUSER remains unchanged (digest in EKA1.md). Explicit GNU2 symbol declarations
exercise User::AllocLen39, AllocSize41, Alloc45, Mem::Copy243 and Free476.
No legacy compiler, C++ runtime, emulator shim or new scheduler was introduced.
The probe allocates64, copies and checks every byte, checks live cell/byte counts,
frees, and requires exact restoration. The corruption control still frees.

Retained initial failure: .symbian/eka1-import-20261005/dynarmic/report.json
records reason40 for image238a92ed07f0c549db80ef7f4ae89f8b30c876029a8ce2484d67fed2d873e4f0.
The oracle incorrectly expected Mem::Copy to return dest. Original e32std.inl
specifies dest+length; correcting that expectation passed both backends in
{dynarmic-r2,dyncom-r2}. Final code always performs the copy before checking
length/end-pointer, so a failed length oracle cannot lead to uninitialized reads.

Final acceptance: SYMBIAN_EKA1_GUEST=1 pytest test_eka1_import.py test_eka1.py
 test_eka1_guest.py: 19 passed in31.56s. Log /tmp/eka1-import-acceptance-final.log;
retained outputs .symbian/eka1-import-final-r2-20261005. The import matrix uses
normal7610, changed7611 and corrupted41 on Dynarmic/Dyncom, plus four reruns
requiring rejection against expected7610: ten import guest runs. Native exits
are type0, frontend exits0 and all preserved baseline hashes match.
Two-tree-reproducible imported images:
- normal: 8accf68f01299e6eed6f4da5c8cb819e503b63fb8e215e6e498f0edcf63b85fc
- changed: a48730a5dbbcca8174eb3d203fddb27d36227aa5867ccf3b355459814f86de26
- corrupt: 6808cc443cf67689c018b15eb92779e604565620454b0df5f695d3caa123be6e

Independent EKA2L1 parser checks original header/CPU/entry, one EUSER block,
section ordinals41/45/39/243/476, matching IAT, absent writable state/security
and fixups, damaged UID and truncated code. Both EKA1 oracle tests pass;
/tmp/eka1-import-oracle-final.log. This is not historical checksum validation.
Native CTest14/14 (/tmp/eka1-import-native-final.log); E32 suite40 tests.
Host E32/import/project/CLI/Console/SDK regression selection70 passed,2 skipped
(/tmp/eka1-import-host-final.log). See EKA1.md and the public guide for complete
build/verification commands and restrictions. Broader ABI, writable state,
lifecycle, networking, UI, legacy packaging and phone acceptance remain gates.

## 2026-10-05 — SDK manifest policy

At the owner's request, removed standalone A11, HTTP/WebSocket, network-header,
EKA1 ordinal and Mbed TLS provenance JSON files, SDK provenance.json generation,
and the source-inventory checker/tests/custom target. Retained upstream licenses
and source references in research notes. The A11 upstream source snapshot stays
because the full host concurrency implementation probe compiles it.
Build-consumed GUI header selection and nghttp2 source-integrity contracts stay;
sdk.json, payload digests and firmware preservation manifests are required by
actual SDK/distribution/preservation APIs. They are not archaeology manifests.
Guest SDK export still builds both architectures, validates archive closure,
copies licenses and seals installed files. Source identity no longer adds an
unused per-file inventory to installed SDKs. EKA1's five ordinal facts came from
EKA2L1 revision2594edf4d6bf55d7bd3f0b46250fe2318d4dc2e8,
src/emu/bridge/include/bridge/epoc6_n.def, hash
047ffa38a86e2a590f7488429e397ece5c58b361a2d1b0c2d9285bf0f3ddcb1c.

### Linux x86_64 portability verification — 2026-10-05

On `helena@192.168.1.209`, Ubuntu 24.04.4, the pinned patched EKA2L1
frontend now builds with Clang 20.1.8, Qt 6 and Ninja 1.13.2. The FFmpeg
compatibility patch constrains x86 shift immediates and disables its obsolete
Vulkan video acceleration; it does not disable emulator graphics. Dynarmic's
MCL template needs an explicit `std::integer_sequence` specialization with
contemporary libstdc++. Both patches apply-check against clean pinned source
files. The updated background-window patch replays after reversing its previous
version and guards the Cocoa-only hook on Linux.

Original emulator headers contain throwing templates, so the owned integration
translation units and three emulator adapter boundaries explicitly enable
exceptions. General SDK libraries retain their Status/no-exceptions policy.
The initial emulator CTest run failed because experiment artifacts were absent;
it also exposed an obsolete assertion that the previously verified Belle DLL
entry/load-preparation hooks were absent. The assertion now checks their actual
handler names and retains the unverified-slot controls. Preserve this failed run
at `/tmp/symbian-linux-emulator-tests-r1.log` rather than reporting it as a pass.

Fresh host native CTest passed 13/13 (`/tmp/symbian-linux-native-tests-final.log`);
a rebuilt Python module passed 294 tests with 423 explicitly skipped opt-in
cases (`/tmp/symbian-linux-pytest-final-r1.log`). An oracle-enabled run is in
progress. The frontend build does not prove Linux GUI interaction or firmware
execution; neither result establishes physical-device compatibility.

### Linux enabled-oracle and ARM indexing checkpoint — 2026-10-05

The broad opt-in run exposed old assumptions beyond the host-only tests:
E32 parser/CPU probes assumed no EHABI descriptor, pointer verification assumed
four rather than eight fixups, and the import loader assumed only seven export
fixups rather than seven exports plus four descriptor fields. The independent
harnesses now verify/relocate those fields and continue checking all dispatch
and export destinations. EKA1's two fixture-specific cases are in their own
`symbian_eka1_oracle` executable, keeping EKA2's
complete four-case verification contract free of skipped EKA1 cases. The GUI
negative control now changes the drawing source where its color actually lives
and asserts the replacement exists; the old app.cc replacement changed nothing.
Current runtime import counts are checked explicitly.

The root Mbed TLS ARM indexing target now uses the installed runtime's actual
include/definition contract and generates its certificate fixture header.
Both guest-probes presets build successfully with LLVM 23.1.2 on Linux:
`/tmp/symbian-linux-index-v5-r3.log`, `/tmp/symbian-linux-index-v6-r3.log`.
With oracle, public GUI-source and installed host concurrency variables enabled,
Pytest passed **310 tests, 407 skipped** in 50.88s:
`/tmp/symbian-linux-pytest-oracles-r3.log`. Failed earlier runs remain retained.
The SDK's **5,420 declared digests** match their actual installed files.
The Linux frontend's `--help` exits zero under Xvfb with a fresh private root
(`/tmp/symbian-linux-frontend-help-r1.log`). Interactive GUI behavior and a
firmware-backed Linux run are not established by that smoke test. The LLVM
binary's private ICU 70 dependency remains a release-tool closure gap.

## 2026-10-05 — public developer documentation cleanup

Reviewed the MkDocs guides, capability pages and references, replacing development
journals, old experiment matrices, retained-artifact hashes and engineering-plan
links with available API contracts, requirements and usage. No `.dev/` reference
remains in `doc/docs`. Preserved complete HTTP/TLS/WebSocket examples and concrete
restrictions: EKA1's separate GNU2 profile, unsupported imported data/TLS/runtime
features, worker affinity and cancellation drainage, and target-specific entropy.
Corrected stale claims that fibers, event integration, SIS resources/signing and
Linux frontend builds were absent. README now covers these work streams and
states that full release payloads and EKA1 libraries remain unfinished.

The emulator source guide now includes its source pin and complete ordered
22-root-patch application recipe instead of directing readers to an internal
journal. Replayed that exact sequence against clean files extracted from the
pinned EKA2L1 commit; all 22 patches applied. Temporary script and source copy
remain outside Git. This checks patch application, not runtime compatibility.

`uv run --no-sync --only-group docs ./doc/build.sh --strict` passed, including
local-link checks, strict MkDocs and both Doxygen indices. Historical header
Doxygen warnings (parameter names, legacy commands and unresolved original
symbols) remain; there were no Doxygen errors. Final strict MkDocs rebuild and
Markdown section-link scan passed (zero unresolved section links). `git diff
--check` passed after whitespace cleanup. The requested documentation-wide switch
from `uv run` to installed commands remains tied to making wheel publication
available; source-checkout instructions currently retain their environment runner.

## 2026-10-05 — reusable standalone host SDK and native publisher

Added an explicit `SYMBIAN_INSTALL_HOST_SDK` installation: one static
`libsymbian_host.a` merges the host format/network/device/concurrency libraries,
private Boost primitive bundle, pinned Abseil, OpenSSL crypto, libusb and zlib.
The relocatable `SymbianHost` CMake package supplies `Symbian::Host`, public
headers and preserved notices; consumers need only normal OS thread/system
libraries. Static zlib extends a ready dependency prefix without rebuilding
OpenSSL, libusb and Boost. `SYMBIAN_PREBUILT_HOST_SDK` configures only the
Python/canonical Status binding boundary against the same versioned archive.

The installed `symbian-native` tool publishes EXEs/DLLs and generates proxy
sources through the existing native implementations. `SymbianPic` chooses it
when present, retaining the Python path for older source SDKs. Input reads are
bounded, filesystem resolution uses error-code APIs, command-specific options
reject mistakes/duplicates, and input/output aliases are rejected. The
relocated consumer checker exercises parsing, HTTP, statuses, host fiber join,
OpenSSL random bytes and installed native CLI behavior, including same-path,
hardlink and symlink input preservation.

Ubuntu x86_64: 13/13 native tests passed (`/tmp/symbian-linux-host-sdk-tests-r4.log`),
relocated consumer/CLI passed (`/tmp/symbian-host-sdk-linux-consumer-r4.log`), and
CPython 3.12 binding-only wheel passed an outside-checkout installed audit
(`/tmp/symbian-linux-reused-wheel-audit-r2.log`). macOS arm64: 13/13 native tests
passed (`/tmp/symbian-macos-host-sdk-tests-r1.log`), relocated consumer/CLI passed
(`/tmp/symbian-host-sdk-macos-consumer-r4.log`), and the binding-only CPython
3.12 wheel passed its installed audit
(`/tmp/symbian-macos-reused-wheel-audit-r1.log`). The first macOS reuse build
failed because OpenSSL headers were omitted; Linux's implicit system headers
had masked that gap. Installed headers now include the matching OpenSSL tree,
and both relocated consumers explicitly compile/call RAND_bytes.

The fresh macOS dependency/core build uses a 14.4 deployment target. Native CLI
Mach-O reports minOS 14.4 and only system CoreFoundation/libc++/libSystem loads.
The Python wheel tag normalizes this to macosx_14_0_arm64; older macOS 14 patch
levels have not been tested, and release deployment/tag policy remains to be
settled. Local GTest came from a newer host floor, affecting test binaries only.
The entire no-Python guest/tool payload, four-host release matrix, all Python
versions, manylinux repair and full EKA1 runtime/API port remain unfinished.
No host-only archive is being presented as the requested complete native SDK.

## 2026-10-05 — Hands-on public guides and C++ snippet formatting

Added small application helpers to the capability/API-family articles: bounded
preview/draft I/O, battery-aware sync policy, display grid selection, camera
inventory, counter intervals, TCP notification/accept, full numeric-field parsing,
fallible scratch allocation, timer/worker Futures, bounded-memory copying and
C++20 positions. Fourteen complete helpers passed ARMv5T and ARMv6 syntax checks
against the exported guest headers; the C++20 layout helper passed separately
for both targets. These checks do not establish execution or loader behavior.
Logs: `/tmp/symbian-doc-snippets-armv5t.log` and
`/tmp/symbian-doc-snippets-armv6.log`.

Public CLI examples use `symbian` directly; source setup explicitly activates
its virtual environment. Source-only development/test commands retain `uv run`.
Removed the Tutorials navigation and redundant pages, preserving the runtime
build recipe in its guide and redirecting the GUI reference to its walkthrough.
All public Markdown C++ examples now use the root clang-format rules, including
InsertBraces. `doc/build.sh` checks future examples; Doxygen examples format at
build time without editing licensed native headers. Formatter checks passed
with macOS clang-format 23.1.2 and Ubuntu clang-format 18.1.3. The local link
checker now ignores fenced examples instead of interpreting lambda captures as
Markdown links. Strict MkDocs and both Doxygen indices passed; historical-header
warnings remain. No wheels were published by this documentation pass.

## 2026-10-05 — Doxygen functional descriptions before API metadata

The original-header input filter now moves publication, release/internal status,
prototype and capability tags to an end paragraph. Class-list briefs no longer
start with publication status; CActive's functional brief precedes its details,
which precede API status. Declaration comparison passed across all eleven
headers. The licensed snapshots and source-browser views remain unchanged.
The filter preserves non-UTF-8 bytes in historical comments; decoding strictly
as UTF-8 had caused silent filter failure on several headers, caught during
validation and fixed with byte-preserving decode/encode. Build guards now fail
on filter tracebacks as well as Doxygen errors. Final strict MkDocs, both
Doxygen indices, local links and formatting passed in
`/tmp/symbian-docs-final-all.log`, with remaining historical-header warnings.
This ordering fix does not complete the separate all-symbol documentation work.

## 2026-10-05 — Application-to-handset guide and clean-install acceptance

Renamed the public GUI walkthrough to “Build a real GUI app”. The hello_time
build guide now names the generated ELF/E32/SIS paths, packages menu resources,
creates/reuses a private signing identity, signs to a new file, stages that exact
signed package with device install --package, and walks through the handset file
browser, installer, launcher and controls. Added desktop-user Linux USB access
setup. CLI argument checks and strict documentation/link checks pass. No physical
phone installation was performed by this documentation edit.

After release CI and actual PyPI publication, replay Getting started, hello_time
and gui_app on fresh macOS/Linux environments using published wheels and empty
SDK/config/cache locations. Build, inspect, package, sign and verify resource
contents; import separately supplied firmware and exercise both emulator
backends where available. Keep physical-device staging/installation results
separate from mocked transfer checks and emulator execution. Capture commands,
versions and failures; fix the guides/tools rather than relying on prepared
worktree state. This acceptance remains pending publication.

## 2026-10-05 — Community resource links

Community credits now cite EKA2L1 Important Links and Delight alongside ROM
resources, and describe Symbian World, NNProject, the Symbian World Telegram
community and both requested Awesome Symbian lists. Documentation link checks
pass; these are external resources rather than bundled firmware or SDK tools.

## 2026-10-05 — Share release host builds across Python versions

Replaced the Linux-only, repeated-native/wheel workflow with a reusable four-host
matrix: Linux x86_64/aarch64 and macOS x86_64/arm64. cibuildwheel builds/tests and
installs the static host core once per container/host, then builds each Python
3.11–3.14 binding against that archive. The same installed core supplies a
relocated consumer audit and host-component archive. Native and Python audits
remain separate. The macOS release floor is now exactly 15.0 to avoid the
previous minOS 14.4 / macosx_14_0 tag ambiguity. Native core archives are labelled
host components; they do not yet claim the required complete no-Python native
SDK. Linux archives are copied through cibuildwheel's /host mount so they survive
container teardown. Shell/config checks passed; the actual four-host run and
publication/native payload gates remain pending.

## 2026-10-05 — Linux GUI checks and Thumb-2 GDB breakpoints

The SDK-backed Linux GUI/generated-project rerun passed 35 cases; its four
remaining failures now pass in a focused rerun. Capture scale is normalized
without dropping the pixel/text assertions. The error-model test observes exit
after the press instead of racing a release against teardown. The allocation
control requests more than the native signed-size limit, remaining a real
nothrow failure under both the original heap and mimalloc profiles.

The live GDB failure was an emulator defect: RSP kind 3 means a 32-bit Thumb
instruction, not a three-byte patch. The new GPL emulator patch uses a two-byte
Thumb trap and restores that halfword, rejecting invalid execute kinds. Consulted
https://sourceware.org/gdb/current/onlinedocs/gdb.html/ARM-Breakpoint-Kinds.html.
The Linux patched frontend now reaches both application/model source breakpoints
and reads clock values. Logs: /tmp/symbian-linux-sdk-gui-r3.log and
/tmp/symbian-linux-gui-negative-debug-r4.log. No phone compatibility claim follows.

## 2026-10-05 — Separate runnable diagnostics from application examples

Moved all seventeen root diagnostic projects to probes/, retaining gui_app in
examples/. Updated explicit source paths, dynamic CMake discovery, native indexing,
SDK runtime export inputs, test fixtures and documentation references. The
probe catalog labels these as runnable diagnostic examples, not user-oriented
apps. Both ARMv5T and ARMv6 root indexing presets configure/build from the new
paths. The affected image/build/import/package/signing/language checks pass:
80 passed, 23 optional guest cases skipped (/tmp/symbian-probes-move-tests.log).
Strict MkDocs, both Doxygen indices and local links pass after the move.
README now shares the requested community/ROM links. GitHub description and
thirteen relevant topics were set and read back successfully.

## 2026-10-05 — Correct CI host archive merger selection

The first shared-core matrix exposed two real build-system gaps: macOS runners
have Apple's libtool but no LLVM ar, and the Linux GNU-ar fallback was a normal
variable scoped only to the concurrency subdirectory. The parent host bundle
then received SYMBIAN_HOST_ARCHIVE_MERGER-NOTFOUND. The fallback now publishes a
cache FILEPATH, and Apple libtool -static can bundle the same archives without
installing a second toolchain. Intel Homebrew LLVM hints are supported too.
Forced Apple-libtool and GNU-ar builds both passed 13/13 native tests and the
relocated host consumer/CLI check. Logs: /tmp/symbian-apple-libtool-tests.log,
/tmp/symbian-apple-libtool-consumer.log and the Linux /tmp/symbian-gnu-ar-*-r4.log.
The full CI wheel matrix must still pass before publication.

## 2026-10-05 — manylinux worker-pool idle wait

Host SDK run 37301042669 reached 12 passing native targets on Linux arm64;
`SharedPoolRunsStacklessPostsAndAbsoluteTimers` timed out inside the remaining
fiber target. The pool passed `steady_clock::time_point::max()` to a predicate
condition-variable wait. A standalone Linux reproducer disabling libstdc++'s
`_GLIBCXX_USE_PTHREAD_COND_CLOCKWAIT` fallback completed with ordinary clockwait,
but timed out after the producer attempted the pool mutex in the old fallback.
Its steady-to-system deadline conversion overflowed, causing an immediate-wait
loop with the mutex held. Capping the idle deadline at 50ms, as A11 does, let
the same old-fallback reproducer terminate. Host fiber CTest passes on macOS and
Ubuntu after the fix. The regression now parks workers with an infinite timer
before immediate posts; the manylinux matrix remains the platform acceptance
check. No guest scheduler implementation changed.

## 2026-10-05 — Linux source sync and SDK preset selection

Preserved the prior Linux working-tree edits in a stash, then fast-forwarded
to the published source. Full source-GUI/oracle tests uncovered two derived
PROJECT.parent probe paths missed by literal relocation searches. Both now
select root probes. The root guest preset ignored SYMBIAN_SDK_MANIFEST and
required a user activation record even with an explicitly selected SDK; it
now uses the same environment-before-activation order as the Python selector.
Both ARM indexing/probe presets build on Linux with that override. Full enabled
Pytest under Xvfb, with the oracle build root (not its platform-tests subdir),
passes 330 tests with 387 optional skips. Native CTest passes 13 targets.
Source onboarding also needs the actual libc++/compiler-rt, Abseil, mimalloc,
MM/appsupport, nghttp2 and resource-compiler inputs, plus the maintained LLVM
patches. The public source guide now gives the missing source acquisition steps.
