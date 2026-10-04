# Development-agent wire framing

The planned resident service will use one protocol over an authenticated TCP
session. The first implemented piece is a native frame codec for the host. It
does not yet connect to an emulator process or accept commands.

## Frame shape and limits

Each frame starts with a four-byte unsigned length in network byte order,
followed by exactly that many MessagePack bytes. A zero length is invalid.
The initial phone profile accepts at most 64 KiB per frame, four completed
frames in a queue and 256 KiB of queued encoded bytes. The latter two limits
are service policy; the codec currently enforces only the per-frame limit.

`symbian::agent::FrameDecoder` accepts a fragment of input at a time and stops
after one complete frame, reporting how many bytes it used. The caller can
feed the unused suffix again. It checks the complete length prefix before
allocating a payload buffer. On an invalid prefix it enters a failed state;
the session should close, or call `Reset()` before using the decoder for a
new trusted stream. `EncodeFrame` applies the same maximum to outgoing bytes.
The C++ declarations and return types are in the
[native reference](../cpp/index.html).

Frame validation is only the outer bound. The service must authenticate the
connection before interpreting payloads, validate MessagePack schemas and
operation grants, enforce the queue limits, and give each request a deadline,
cancellation path and final status. None of those properties follows from
the frame codec alone. See the
[development-agent plan](https://github.com/hpnkv/symbian-platform/blob/main/.dev/development-agent.md)
for the intended service gates.
