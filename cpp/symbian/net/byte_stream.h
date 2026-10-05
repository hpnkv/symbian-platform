// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.
#ifndef SYMBIAN_NET_BYTE_STREAM_H_
#define SYMBIAN_NET_BYTE_STREAM_H_
#include <cstdint>
#include <span>

#include "absl/status/statusor.h"
#include "absl/time/time.h"

namespace symbian::net {
/** @brief Single-owner transport shared by HTTP and WebSocket connections.
 *
 * Read returns zero only at clean EOF. Write completes the whole span or fails;
 * after a partial write failure discard the stream. Implementations own socket
 * and TLS lifetime, bound operations by absolute deadlines, and create no
 * scheduler. Calls and destruction stay on the owning worker.
 */
class ByteStream {
 public:
  virtual ~ByteStream() = default;
  virtual absl::StatusOr<std::size_t> Read(std::span<std::uint8_t> bytes,
                                           absl::Time deadline) = 0;
  virtual absl::Status Write(std::span<const std::uint8_t> bytes,
                             absl::Time deadline) = 0;
  virtual void Close() = 0;
};
}  // namespace symbian::net
#endif
