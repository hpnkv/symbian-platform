// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "symbian/api/connectivity/broadcast_probe.h"

#include <limits.h>

#include "native_broadcast_probe.h"
#include "symbian/native_status.h"

namespace symbian::api::connectivity {

absl::StatusOr<std::array<std::uint8_t, 4>> BroadcastProbe(
    std::uint16_t port, std::span<const std::uint8_t> request,
    std::span<const std::uint8_t> expected_reply, absl::Time deadline) {
  if (port == 0 || request.empty() || request.size() > 64 ||
      expected_reply.empty() || expected_reply.size() > 64) {
    return absl::InvalidArgumentError("Invalid UDP broadcast probe");
  }
  unsigned address = 0;
  if (const int result = SymbianDeviceBroadcastProbe(
          port, request.data(), static_cast<int>(request.size()),
          expected_reply.data(), static_cast<int>(expected_reply.size()),
          &address,
          deadline == absl::InfiniteFuture() ? INT64_MAX
                                             : absl::ToUnixMicros(deadline));
      result != 0) {
    return symbian::StatusFromNativeError(result, "Discover UDP peer");
  }
  return std::array<std::uint8_t, 4>{static_cast<std::uint8_t>(address >> 24),
                                     static_cast<std::uint8_t>(address >> 16),
                                     static_cast<std::uint8_t>(address >> 8),
                                     static_cast<std::uint8_t>(address)};
}

}  // namespace symbian::api::connectivity
