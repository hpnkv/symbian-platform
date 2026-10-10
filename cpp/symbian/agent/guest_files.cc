// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "symbian/agent/guest_files.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "symbian/api/storage/storage.h"

namespace symbian::agent {
namespace {

constexpr std::u16string_view kWorkspace =
    u"C:\\private\\e0000a31\\workspace\\";
constexpr std::u16string_view kSharedApps = u"C:\\Data\\SymbianAgent\\apps\\";
constexpr std::uint64_t kMaximumResourceBytes = 16 * 1024 * 1024;

absl::StatusOr<std::u16string> ResourceDirectory(std::uint8_t scope,
                                                 std::uint32_t uid) {
  if (scope == 0 && uid == 0) {
    return std::u16string(kWorkspace);
  }
  if (scope != 1 || uid == 0) {
    return absl::InvalidArgumentError("Invalid resource scope");
  }
  std::u16string path(kSharedApps);
  constexpr char16_t kHex[] = u"0123456789abcdef";
  for (int shift = 28; shift >= 0; shift -= 4) {
    path.push_back(kHex[(uid >> shift) & 15]);
  }
  path.push_back(u'\\');
  return path;
}

absl::StatusOr<std::u16string> ResourcePath(std::uint8_t scope,
                                            std::uint32_t uid,
                                            std::string_view name) {
  if (name.empty() || name.size() > 64 || name == "." || name == "..") {
    return absl::InvalidArgumentError("Invalid resource name");
  }
  auto directory = ResourceDirectory(scope, uid);
  if (!directory.ok()) {
    return directory.status();
  }
  for (char ch : name) {
    if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
          (ch >= '0' && ch <= '9') || ch == '.' || ch == '_' || ch == '-')) {
      return absl::InvalidArgumentError("Invalid resource name");
    }
    directory->push_back(static_cast<char16_t>(ch));
  }
  return *directory;
}

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

absl::StatusOr<GuestResourceChunk> ReadResourceChunk(std::uint8_t scope,
                                                     std::uint32_t uid,
                                                     std::string_view name,
                                                     std::uint64_t offset,
                                                     std::uint32_t length) {
  if (length == 0 || length > 32768 || offset > kMaximumResourceBytes) {
    return absl::InvalidArgumentError("Invalid resource read bounds");
  }
  auto path = ResourcePath(scope, uid, name);
  if (!path.ok()) {
    return path.status();
  }
  auto file = api::storage::ReadOnlyFile::Open(*path);
  if (!file.ok()) {
    return file.status();
  }
  auto size = file->Size();
  if (!size.ok()) {
    return size.status();
  }
  if (*size > kMaximumResourceBytes) {
    return absl::ResourceExhaustedError("Resource exceeds 16 MiB");
  }
  GuestResourceChunk chunk;
  chunk.total_bytes = *size;
  if (offset >= *size) {
    return chunk;
  }
  chunk.bytes.resize(static_cast<std::size_t>(
      std::min<std::uint64_t>(length, *size - offset)));
  auto read = file->ReadAt(
      offset, std::span(reinterpret_cast<std::byte*>(chunk.bytes.data()),
                        chunk.bytes.size()));
  if (!read.ok()) {
    return read.status();
  }
  chunk.bytes.resize(*read);
  return chunk;
}

absl::Status WriteResourceChunk(std::uint8_t scope, std::uint32_t uid,
                                std::string_view name, std::uint64_t offset,
                                std::uint8_t mode,
                                std::span<const std::byte> bytes) {
  if (mode > 2 || bytes.empty() || bytes.size() > 32768 ||
      offset > kMaximumResourceBytes - bytes.size()) {
    return absl::InvalidArgumentError("Invalid resource write bounds");
  }
  auto path = ResourcePath(scope, uid, name);
  auto directory = ResourceDirectory(scope, uid);
  if (!path.ok()) {
    return path.status();
  }
  if (!directory.ok()) {
    return directory.status();
  }
  if (mode != 1) {
    if (absl::Status created = api::storage::CreateDirectories(*directory);
        !created.ok()) {
      return created;
    }
  }
  const api::storage::WriteMode write_mode =
      mode == 0   ? api::storage::WriteMode::kCreateNew
      : mode == 1 ? api::storage::WriteMode::kOpenExisting
                  : api::storage::WriteMode::kReplaceExisting;
  auto file = api::storage::WritableFile::Open(*path, write_mode);
  if (!file.ok()) {
    return file.status();
  }
  absl::Status written = file->WriteAt(offset, bytes);
  return written.ok() ? file->Flush() : written;
}

}  // namespace symbian::agent
