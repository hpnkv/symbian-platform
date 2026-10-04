// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_API_CONNECTIVITY_BROADCAST_PROBE_H_
#define SYMBIAN_API_CONNECTIVITY_BROADCAST_PROBE_H_

#include <array>
#include <cstdint>
#include <span>

#include "absl/status/statusor.h"
#include "absl/time/time.h"

namespace symbian::api::connectivity {

/**
 * @brief Find a local IPv4 peer by one UDP broadcast request and exact reply.
 *
 * The reply is application-defined and must be authenticated by the caller
 * when its address will be trusted. Both messages are limited to 64 bytes.
 * The receive request is cancelled and drained when @p deadline expires.
 */
absl::StatusOr<std::array<std::uint8_t, 4>> BroadcastProbe(
    std::uint16_t port, std::span<const std::uint8_t> request,
    std::span<const std::uint8_t> expected_reply,
    absl::Time deadline = absl::InfiniteFuture());

}  // namespace symbian::api::connectivity

#endif  // SYMBIAN_API_CONNECTIVITY_BROADCAST_PROBE_H_
