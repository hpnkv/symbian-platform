// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_AGENT_GUEST_CONTROL_H_
#define SYMBIAN_AGENT_GUEST_CONTROL_H_

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "absl/status/statusor.h"
#include "absl/status/status.h"
#include "symbian/agent/guest_files.h"
#include "symbian/agent/guest_log.h"

namespace symbian::agent {

/** @brief One authenticated, read-only control request for the phone profile. */
struct GuestControlRequest {
  std::uint64_t request_id = 0;
  std::uint8_t kind = 0;  // 1: hello, 2: status, 6: logs, 7: list, 8/9: display.
  std::uint64_t deadline_millis = 0;
  std::uint64_t page_after = 0;
  std::uint8_t page_limit = 0;
  std::uint32_t pointer_x = 0;
  std::uint32_t pointer_y = 0;
  std::uint8_t pointer_action = 0;  // 1: move, 2: down, 3: up.
  std::uint8_t resource_scope = 0;  // 0: agent workspace, 1: shared app data.
  std::uint32_t resource_uid = 0;
  std::string resource_name;
  std::uint64_t resource_offset = 0;
  std::uint32_t resource_length = 0;
  std::uint8_t resource_mode = 0;  // 0: create, 1: existing, 2: replace.
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
  bool logs_available = false;
  bool workspace_available = false;
};

/**
 * @brief Parse a bounded MessagePack control map after TLS authentication.
 *
 * Accepts version 1 hello/status with an empty body map, or logs with a
 * bounded after/limit body, and up to eight
 * top-level fields. Unknown fields are retained byte-for-byte so a result
 * can echo them without silently discarding a future field. Invalid, nested
 * beyond four levels, duplicate and oversized inputs fail closed.
 */
absl::StatusOr<GuestControlRequest> ParseGuestControl(std::string_view payload);

/**
 * @brief Pack a status result compatible with the host ControlMessage.
 *
 * The body advertises only the status capability. This routine does not grant
 * permission; the caller must have authenticated the TLS peer first.
 */
absl::StatusOr<std::string> PackGuestResult(const GuestControlRequest& request);

/** @brief Pack status with only the native observations actually available. */
absl::StatusOr<std::string> PackGuestResult(
    const GuestControlRequest& request, const GuestStatusSnapshot& snapshot);

/** @brief Advertise the bounded version-one read-only service profile. */
absl::StatusOr<std::string> PackGuestHelloResult(
    const GuestControlRequest& request, bool logs_available,
    std::uint16_t maximum_requests, bool workspace_available = false);

/** @brief Pack one bounded log page into a version-one result envelope. */
absl::StatusOr<std::string> PackGuestLogResult(
    const GuestControlRequest& request, const AgentLogPage& page);

/** @brief Pack a bounded agent-private workspace directory page. */
absl::StatusOr<std::string> PackGuestWorkspaceResult(
    const GuestControlRequest& request, const GuestFilePage& page);

/** @brief Metadata for one immediately following bounded raw RGB565 frame. */
absl::StatusOr<std::string> PackGuestScreenResult(
    const GuestControlRequest& request, std::uint32_t width,
    std::uint32_t height, std::uint32_t stride, std::uint32_t bytes);

/** @brief Acknowledgment after a pointer event has been submitted. */
absl::StatusOr<std::string> PackGuestPointerResult(
    const GuestControlRequest& request);

/** @brief Metadata preceding raw file bytes for a bounded read. */
absl::StatusOr<std::string> PackGuestResourceReadResult(
    const GuestControlRequest& request, std::uint64_t total_bytes,
    std::uint32_t data_bytes);

/** @brief Acknowledgment after a bounded write and flush. */
absl::StatusOr<std::string> PackGuestResourceWriteResult(
    const GuestControlRequest& request);

/** @brief AppArc accepted the package document for the installer UI. */
absl::StatusOr<std::string> PackGuestPackageOpenResult(
    const GuestControlRequest& request, bool registered_before);

/** @brief Current AppArc registration state, independent of package transfer. */
absl::StatusOr<std::string> PackGuestAppRegisteredResult(
    const GuestControlRequest& request, bool registered);

/** @brief Report an authenticated operation failure without dropping the session. */
absl::StatusOr<std::string> PackGuestError(
    const GuestControlRequest& request, const absl::Status& status);

}  // namespace symbian::agent

#endif  // SYMBIAN_AGENT_GUEST_CONTROL_H_
