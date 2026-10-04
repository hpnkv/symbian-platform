// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_AGENT_GUEST_LOG_H_
#define SYMBIAN_AGENT_GUEST_LOG_H_

#include <array>
#include <cstddef>
#include <cstdint>

#include "absl/status/statusor.h"

namespace symbian::agent {

/** @brief Small, stable agent event codes; unknown future values are allowed. */
enum class AgentLogCode : std::uint8_t {
  kAuthenticated = 1,
  kStatusRead = 2,
  kRejectedFrame = 3,
  kSessionClosed = 4,
};

/** @brief One retained event in the agent's own process. */
struct AgentLogRecord {
  std::uint64_t sequence = 0;
  AgentLogCode code = AgentLogCode::kAuthenticated;
};

/** @brief One bounded read result, including a lost-record signal. */
struct AgentLogPage {
  std::array<AgentLogRecord, 8> records{};
  std::uint8_t count = 0;
  std::uint64_t next_cursor = 0;
  bool gap = false;
};

/**
 * @brief Fixed 32-record service log with a monotonic sequence cursor.
 *
 * The owner must use this class on one worker thread. It performs no I/O and
 * allocates no records after construction. Records disappear on process exit;
 * this does not capture the operating system's own logs.
 */
class AgentLogRing {
 public:
  static constexpr std::size_t kCapacity = 32;

  /** @brief Append one event, overwriting the oldest at capacity. */
  void Append(AgentLogCode code);

  /**
   * @brief Return up to 1–8 records after a cursor.
   *
   * A cursor of zero starts with the oldest retained record. Gap is set if
   * records after the requested cursor have already been overwritten.
   */
  absl::StatusOr<AgentLogPage> ReadAfter(std::uint64_t cursor,
                                         std::uint8_t limit) const;

 private:
  std::array<AgentLogRecord, kCapacity> records_{};
  std::uint64_t next_sequence_ = 1;
};

}  // namespace symbian::agent

#endif  // SYMBIAN_AGENT_GUEST_LOG_H_
