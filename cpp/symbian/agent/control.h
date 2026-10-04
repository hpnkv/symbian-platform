// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_AGENT_CONTROL_H_
#define SYMBIAN_AGENT_CONTROL_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "absl/status/statusor.h"
#include "nlohmann/json.hpp"

namespace symbian::agent {

/** @brief Control messages have a smaller budget than data frames. */
inline constexpr std::size_t kMaximumControlBytes = 4096;

/** @brief Version-one control operations; none grants write or device access. */
enum class ControlKind : std::uint8_t {
  kHello = 1,
  kStatus = 2,
  kCancel = 3,
  kResult = 4,
  kError = 5,
  kLogs = 6,
};

/**
 * @brief Typed control envelope carried as MessagePack inside a wire frame.
 *
 * The absolute deadline is a protocol field whose clock basis will be fixed
 * at session negotiation. A zero value means no deadline was supplied, which
 * a request dispatcher may reject. Unknown top-level fields survive a parse
 * and re-encode so peers can extend this envelope without silent data loss.
 */
struct ControlMessage {
  std::uint32_t version = 1;
  std::uint64_t request_id = 0;
  ControlKind kind = ControlKind::kHello;
  std::uint64_t deadline_millis = 0;
  nlohmann::json body = nlohmann::json::object();
  nlohmann::json extensions = nlohmann::json::object();
};

/**
 * @brief Parse one bounded MessagePack control payload.
 * @return InvalidArgument for malformed fields or unknown operations, and
 *         ResourceExhausted when the encoded control budget is exceeded.
 */
absl::StatusOr<ControlMessage> ParseControl(std::string_view encoded);

/** @brief Validate and serialize a control envelope as MessagePack. */
absl::StatusOr<std::string> PackControl(const ControlMessage& message);

}  // namespace symbian::agent

#endif  // SYMBIAN_AGENT_CONTROL_H_
