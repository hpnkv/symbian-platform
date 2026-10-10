// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_AGENT_GUEST_FILES_H_
#define SYMBIAN_AGENT_GUEST_FILES_H_

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "absl/status/status.h"

#include "absl/status/statusor.h"

namespace symbian::agent {

/** @brief One name in the agent-owned workspace, encoded as UTF-8. */
struct GuestFileEntry {
  std::string name;
  bool is_directory = false;
  bool is_read_only = false;
  std::optional<std::uint64_t> size_bytes;
};

/** @brief A bounded listing of the agent-owned workspace root. */
struct GuestFilePage {
  std::array<GuestFileEntry, 8> entries{};
  std::uint8_t count = 0;
  std::uint16_t next_offset = 0;
  bool more = false;
};

/**
 * @brief Read one page from the agent's private workspace root.
 *
 * The root is fixed to this agent's Symbian data cage. An absent root is an
 * empty listing. Offset pagination is bounded to 256 entries and is not a
 * snapshot: concurrent directory changes can shift later pages. Call from a
 * worker thread because File Server iteration may block.
 */
absl::StatusOr<GuestFilePage> ReadWorkspacePage(std::uint16_t after,
                                                std::uint8_t limit);

struct GuestResourceChunk {
  std::vector<std::uint8_t> bytes;
  std::uint64_t total_bytes = 0;
};

/** @brief Read one bounded chunk from the agent or shared app resource root. */
absl::StatusOr<GuestResourceChunk> ReadResourceChunk(
    std::uint8_t scope, std::uint32_t uid, std::string_view name,
    std::uint64_t offset, std::uint32_t length);

/** @brief Write and flush one bounded chunk using an explicit open mode. */
absl::Status WriteResourceChunk(std::uint8_t scope, std::uint32_t uid,
                                std::string_view name, std::uint64_t offset,
                                std::uint8_t mode,
                                std::span<const std::byte> bytes);

}  // namespace symbian::agent

#endif  // SYMBIAN_AGENT_GUEST_FILES_H_
