# Media component

**Planned, not exported:** this directory has no archive or public header yet.

## Motivation and proposed modernization

Legacy audio utilities combine callback state, descriptors and explicit
session cleanup. Playback and recording should instead have move-only stream
owners with a typed state machine, negotiated format, and `Future` results for
start, stop and completion. Audio bytes need explicit ownership: playback can
borrow a stable `std::span<const std::byte>` only for a documented synchronous
copy, while asynchronous buffers need an owned lifetime. Errors must identify
unsupported formats rather than silently resampling or changing sample rate.

## Concurrency and cost

Native completion callbacks belong on the existing event loop and should only
advance state or enqueue bounded work. Mixing, encoding and decoding belong
on workers. Buffer pools and backpressure avoid per-frame allocation and
unbounded queues; cancellation must drain the native request before releasing
buffers. Camera preview and still capture stay in the separate camera library.

## Evidence before implementation

Verify the chosen audio service's ordinals, callback order, format negotiation,
latency, device permissions and cancellation with an emulator and named
physical profile before exporting an archive.
