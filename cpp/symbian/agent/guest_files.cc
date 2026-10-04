// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "symbian/agent/guest_files.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include "absl/status/status.h"
#include "symbian/api/storage/storage.h"

namespace symbian::agent {
namespace {

constexpr std::u16string_view kWorkspace =
    u"C:\\private\\e0000a31\\workspace\\";

absl::StatusOr<std::string> Utf16ToUtf8(std::u16string_view input) {
  std::string output;
  output.reserve(input.size());
  for (std::size_t index = 0; index < input.size(); ++index) {
    std::uint32_t scalar = input[index];
    if (scalar >= 0xd800 && scalar <= 0xdbff) {
      if (++index == input.size() || input[index] < 0xdc00 ||
          input[index] > 0xdfff) {
        return absl::DataLossError("Workspace name has invalid UTF-16");
      }
      scalar = 0x10000 + ((scalar - 0xd800) << 10) + (input[index] - 0xdc00);
    } else if (scalar >= 0xdc00 && scalar <= 0xdfff) {
      return absl::DataLossError("Workspace name has invalid UTF-16");
    }
    if (scalar <= 0x7f) {
      output.push_back(static_cast<char>(scalar));
    } else if (scalar <= 0x7ff) {
      output.push_back(static_cast<char>(0xc0 | (scalar >> 6)));
      output.push_back(static_cast<char>(0x80 | (scalar & 0x3f)));
    } else if (scalar <= 0xffff) {
      output.push_back(static_cast<char>(0xe0 | (scalar >> 12)));
      output.push_back(static_cast<char>(0x80 | ((scalar >> 6) & 0x3f)));
      output.push_back(static_cast<char>(0x80 | (scalar & 0x3f)));
    } else {
      output.push_back(static_cast<char>(0xf0 | (scalar >> 18)));
      output.push_back(static_cast<char>(0x80 | ((scalar >> 12) & 0x3f)));
      output.push_back(static_cast<char>(0x80 | ((scalar >> 6) & 0x3f)));
      output.push_back(static_cast<char>(0x80 | (scalar & 0x3f)));
    }
  }
  return output;
}

}  // namespace

absl::StatusOr<GuestFilePage> ReadWorkspacePage(std::uint16_t after,
                                                std::uint8_t limit) {
  if (limit == 0 || limit > 8) {
    return absl::InvalidArgumentError("Workspace page is out of bounds");
  }
  if (after >= 256) {
    return absl::ResourceExhaustedError(
        "Workspace listing exceeds 256 entries");
  }
  auto reader = api::storage::DirectoryReader::Open(kWorkspace);
  if (!reader.ok()) {
    if (reader.status().code() == absl::StatusCode::kNotFound) {
      return GuestFilePage{.next_offset = after};
    }
    return reader.status();
  }
  GuestFilePage page{.next_offset = after};
  for (std::uint16_t index = 0; index < after; ++index) {
    auto skipped = reader->Next();
    if (!skipped.ok()) {
      return skipped.status();
    }
    if (!skipped->has_value()) {
      return page;
    }
  }
  for (; page.count < limit && page.next_offset < 256; ++page.count) {
    auto next = reader->Next();
    if (!next.ok()) {
      return next.status();
    }
    if (!next->has_value()) {
      return page;
    }
    auto name = Utf16ToUtf8((*next)->name);
    if (!name.ok()) {
      return name.status();
    }
    page.entries[page.count] =
        GuestFileEntry{.name = std::move(*name),
                       .is_directory = (*next)->is_directory,
                       .is_read_only = (*next)->is_read_only,
                       .size_bytes = (*next)->size_bytes};
    ++page.next_offset;
  }
  auto extra = reader->Next();
  if (!extra.ok()) {
    return extra.status();
  }
  page.more = extra->has_value();
  return page;
}

}  // namespace symbian::agent
