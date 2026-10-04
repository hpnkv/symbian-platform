// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_AGENT_GUEST_CONTROL_H_
#define SYMBIAN_AGENT_GUEST_CONTROL_H_

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "absl/status/statusor.h"

namespace symbian::agent {

/** @brief One authenticated, read-only control request for the phone profile. */
struct GuestControlRequest {
  std::uint64_t request_id = 0;
  std::uint8_t kind = 0;  // 1: hello, 2: status.
  std::uint64_t deadline_millis = 0;
  // Complete MessagePack key/value pairs for unknown top-level fields.
  std::string extensions;
  std::uint8_t extension_count = 0;
};

/** @brief Native tick reading exposed only after peer authentication. */
struct GuestTickSnapshot {
  std::uint32_t count = 0;
  std::uint64_t period_microseconds = 0;
};

/** @brief Primary HAL display dimensions, if the platform reports them. */
struct GuestDisplaySnapshot {
  std::uint32_t width_pixels = 0;
  std::uint32_t height_pixels = 0;
};

/** @brief Optional, point-in-time native observations in a status response. */
struct GuestStatusSnapshot {
  std::optional<GuestTickSnapshot> tick;
  std::optional<GuestDisplaySnapshot> display;
};

/**
 * @brief Parse a bounded MessagePack control map after TLS authentication.
 *
 * Accepts version 1 hello/status with an empty body map and up to eight
 * top-level fields. Unknown fields are retained byte-for-byte so a result
 * can echo them without silently discarding a future field. Invalid, nested
 * beyond four levels, duplicate and oversized inputs fail closed.
 */
absl::StatusOr<GuestControlRequest> ParseGuestControl(std::string_view payload);

/**
 * @brief Pack a read-only result compatible with the host ControlMessage.
 *
 * The body advertises only the status capability. This routine does not grant
 * permission; the caller must have authenticated the TLS peer first.
 */
absl::StatusOr<std::string> PackGuestResult(const GuestControlRequest& request);

/** @brief Pack status with only the native observations actually available. */
absl::StatusOr<std::string> PackGuestResult(
    const GuestControlRequest& request, const GuestStatusSnapshot& snapshot);

}  // namespace symbian::agent

#endif  // SYMBIAN_AGENT_GUEST_CONTROL_H_
