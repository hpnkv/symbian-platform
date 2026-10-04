// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "symbian/agent/guest_log.h"

#include <algorithm>

#include "absl/status/status.h"

namespace symbian::agent {

void AgentLogRing::Append(AgentLogCode code) {
  records_[(next_sequence_ - 1) % kCapacity] = {next_sequence_, code};
  ++next_sequence_;
}

absl::StatusOr<AgentLogPage> AgentLogRing::ReadAfter(std::uint64_t cursor,
                                                     std::uint8_t limit) const {
  if (limit == 0 || limit > 8) {
    return absl::InvalidArgumentError("Agent log limit must be 1-8");
  }
  AgentLogPage page;
  const std::uint64_t latest = next_sequence_ - 1;
  const std::uint64_t oldest =
      next_sequence_ > kCapacity ? next_sequence_ - kCapacity : 1;
  if (cursor < oldest - 1) {
    page.gap = true;
  }
  if (cursor >= latest) {
    page.next_cursor = latest;
    return page;
  }
  const std::uint64_t first = std::max(oldest, cursor + 1);
  for (std::uint64_t sequence = first; sequence <= latest && page.count < limit;
       ++sequence) {
    page.records[page.count++] = records_[(sequence - 1) % kCapacity];
  }
  page.next_cursor =
      page.count == 0 ? latest : page.records[page.count - 1].sequence;
  return page;
}

}  // namespace symbian::agent
