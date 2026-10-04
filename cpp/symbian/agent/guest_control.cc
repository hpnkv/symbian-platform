// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "symbian/agent/guest_control.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "absl/status/status.h"

namespace symbian::agent {
namespace {

constexpr std::size_t kMaximumControlBytes = 4096;

class Cursor {
 public:
  explicit Cursor(std::string_view input) : input_(input) {}

  std::size_t position() const { return position_; }

  bool done() const { return position_ == input_.size(); }

  bool Byte(std::uint8_t* value) {
    if (position_ == input_.size()) {
      return false;
    }
    *value = static_cast<std::uint8_t>(input_[position_++]);
    return true;
  }

  bool Number(std::size_t width, std::uint64_t* value) {
    if (width > input_.size() - position_) {
      return false;
    }
    *value = 0;
    for (std::size_t index = 0; index < width; ++index) {
      *value = (*value << 8) | static_cast<std::uint8_t>(input_[position_++]);
    }
    return true;
  }

  bool Bytes(std::size_t count, std::string_view* value) {
    if (count > input_.size() - position_) {
      return false;
    }
    *value = input_.substr(position_, count);
    position_ += count;
    return true;
  }

  bool Unsigned(std::uint64_t* value) {
    std::uint8_t marker = 0;
    if (!Byte(&marker)) {
      return false;
    }
    if (marker <= 0x7f) {
      *value = marker;
      return true;
    }
    switch (marker) {
      case 0xcc:
        return Number(1, value);
      case 0xcd:
        return Number(2, value);
      case 0xce:
        return Number(4, value);
      case 0xcf:
        return Number(8, value);
      default:
        return false;
    }
  }

  bool String(std::string_view* value) {
    std::uint8_t marker = 0;
    if (!Byte(&marker)) {
      return false;
    }
    std::uint64_t length = 0;
    if ((marker & 0xe0) == 0xa0) {
      length = marker & 0x1f;
    } else if (marker == 0xd9) {
      if (!Number(1, &length)) {
        return false;
      }
    } else if (marker == 0xda) {
      if (!Number(2, &length)) {
        return false;
      }
    } else if (marker == 0xdb) {
      if (!Number(4, &length)) {
        return false;
      }
    } else {
      return false;
    }
    return length <= kMaximumControlBytes && Bytes(length, value);
  }

  bool Map(std::size_t* count) {
    std::uint8_t marker = 0;
    if (!Byte(&marker)) {
      return false;
    }
    std::uint64_t length = 0;
    if ((marker & 0xf0) == 0x80) {
      length = marker & 0x0f;
    } else if (marker == 0xde) {
      if (!Number(2, &length)) {
        return false;
      }
    } else if (marker == 0xdf) {
      if (!Number(4, &length)) {
        return false;
      }
    } else {
      return false;
    }
    if (length > 8) {
      return false;
    }
    *count = static_cast<std::size_t>(length);
    return true;
  }

  bool Skip(int depth) {
    if (depth > 4) {
      return false;
    }
    std::uint8_t marker = 0;
    if (!Byte(&marker)) {
      return false;
    }
    if (marker <= 0x7f || marker >= 0xe0 || marker == 0xc0 || marker == 0xc2 ||
        marker == 0xc3) {
      return true;
    }
    std::uint64_t length = 0;
    if ((marker & 0xe0) == 0xa0) {
      length = marker & 0x1f;
      return SkipBytes(length);
    }
    if ((marker & 0xf0) == 0x90) {
      length = marker & 0x0f;
      return SkipItems(length, depth);
    }
    if ((marker & 0xf0) == 0x80) {
      length = marker & 0x0f;
      return SkipItems(length * 2, depth);
    }
    switch (marker) {
      case 0xcc:
      case 0xd0:
        return SkipBytes(1);
      case 0xcd:
      case 0xd1:
        return SkipBytes(2);
      case 0xca:
      case 0xce:
      case 0xd2:
        return SkipBytes(4);
      case 0xcb:
      case 0xcf:
      case 0xd3:
        return SkipBytes(8);
      case 0xc4:
      case 0xd9:
        return Number(1, &length) && SkipBytes(length);
      case 0xc5:
      case 0xda:
        return Number(2, &length) && SkipBytes(length);
      case 0xc6:
      case 0xdb:
        return Number(4, &length) && SkipBytes(length);
      case 0xdc:
        return Number(2, &length) && SkipItems(length, depth);
      case 0xdd:
        return Number(4, &length) && SkipItems(length, depth);
      case 0xde:
        return Number(2, &length) && SkipItems(length * 2, depth);
      case 0xdf:
        return Number(4, &length) && SkipItems(length * 2, depth);
      default:
        return false;
    }
  }

 private:
  bool SkipBytes(std::uint64_t count) {
    if (count > input_.size() - position_) {
      return false;
    }
    position_ += static_cast<std::size_t>(count);
    return true;
  }

  bool SkipItems(std::uint64_t count, int depth) {
    if (count > input_.size() - position_) {
      return false;
    }
    for (std::uint64_t index = 0; index < count; ++index) {
      if (!Skip(depth + 1)) {
        return false;
      }
    }
    return true;
  }

  std::string_view input_;
  std::size_t position_ = 0;
};

void WriteUInt(std::string* result, std::uint64_t value) {
  if (value <= 0x7f) {
    result->push_back(static_cast<char>(value));
    return;
  }
  const std::size_t width = value <= 0xff         ? 1
                            : value <= 0xffff     ? 2
                            : value <= 0xffffffff ? 4
                                                  : 8;
  result->push_back(static_cast<char>(width == 1   ? 0xcc
                                      : width == 2 ? 0xcd
                                      : width == 4 ? 0xce
                                                   : 0xcf));
  for (std::size_t remaining = width; remaining > 0; --remaining) {
    result->push_back(static_cast<char>(value >> ((remaining - 1) * 8)));
  }
}

void WriteString(std::string* result, std::string_view value) {
  if (value.size() <= 31) {
    result->push_back(static_cast<char>(0xa0 | value.size()));
  } else if (value.size() <= 255) {
    result->push_back(static_cast<char>(0xd9));
    result->push_back(static_cast<char>(value.size()));
  } else {
    result->push_back(static_cast<char>(0xda));
    result->push_back(static_cast<char>(value.size() >> 8));
    result->push_back(static_cast<char>(value.size()));
  }
  result->append(value);
}

}  // namespace

absl::StatusOr<GuestControlRequest> ParseGuestControl(
    std::string_view payload) {
  if (payload.size() > kMaximumControlBytes) {
    return absl::ResourceExhaustedError("Agent control frame exceeds 4 KiB");
  }
  if (payload.empty()) {
    return absl::InvalidArgumentError("Empty agent control frame");
  }
  Cursor cursor(payload);
  std::size_t count = 0;
  if (!cursor.Map(&count)) {
    return absl::InvalidArgumentError("Agent control must be a map");
  }
  GuestControlRequest result;
  std::uint64_t version = 0;
  unsigned seen = 0;
  unsigned page_fields = 0;
  bool nonempty_body = false;
  std::array<std::string_view, 8> extension_keys{};
  for (std::size_t index = 0; index < count; ++index) {
    const std::size_t start = cursor.position();
    std::string_view key;
    if (!cursor.String(&key) || key.size() > 32) {
      return absl::InvalidArgumentError("Invalid agent control key");
    }
    unsigned bit = 0;
    std::uint64_t value = 0;
    if (key == "v") {
      bit = 1;
      if (!cursor.Unsigned(&value)) {
        return absl::InvalidArgumentError("Invalid control version");
      }
      version = value;
    } else if (key == "id") {
      bit = 2;
      if (!cursor.Unsigned(&result.request_id)) {
        return absl::InvalidArgumentError("Invalid control request ID");
      }
    } else if (key == "kind") {
      bit = 4;
      if (!cursor.Unsigned(&value) || value > 255) {
        return absl::InvalidArgumentError("Invalid control kind");
      }
      result.kind = static_cast<std::uint8_t>(value);
    } else if (key == "deadline_ms") {
      bit = 8;
      if (!cursor.Unsigned(&result.deadline_millis)) {
        return absl::InvalidArgumentError("Invalid control deadline");
      }
    } else if (key == "body") {
      bit = 16;
      std::size_t body_items = 0;
      if (!cursor.Map(&body_items) || body_items > 2) {
        return absl::InvalidArgumentError("Unsupported control body");
      }
      nonempty_body = body_items != 0;
      for (std::size_t body_index = 0; body_index < body_items; ++body_index) {
        std::string_view body_key;
        std::uint64_t body_value = 0;
        if (!cursor.String(&body_key) || !cursor.Unsigned(&body_value)) {
          return absl::InvalidArgumentError("Invalid page request body");
        }
        unsigned body_bit = 0;
        if (body_key == "after") {
          body_bit = 1;
          result.page_after = body_value;
        } else if (body_key == "limit" && body_value <= 8) {
          body_bit = 2;
          result.page_limit = static_cast<std::uint8_t>(body_value);
        } else {
          return absl::InvalidArgumentError("Unsupported page request field");
        }
        if ((page_fields & body_bit) != 0) {
          return absl::InvalidArgumentError("Duplicate page request field");
        }
        page_fields |= body_bit;
      }
    } else {
      for (std::uint8_t previous = 0; previous < result.extension_count;
           ++previous) {
        if (extension_keys[previous] == key) {
          return absl::InvalidArgumentError("Duplicate control extension");
        }
      }
      if (!cursor.Skip(0)) {
        return absl::InvalidArgumentError("Invalid control extension");
      }
      extension_keys[result.extension_count] = key;
      result.extensions.append(
          payload.substr(start, cursor.position() - start));
      ++result.extension_count;
      continue;
    }
    if ((seen & bit) != 0) {
      return absl::InvalidArgumentError("Duplicate control field");
    }
    seen |= bit;
  }
  if (!cursor.done() || (seen & 7) != 7 || version != 1 ||
      result.request_id == 0 ||
      (result.kind != 1 && result.kind != 2 && result.kind != 6 &&
       result.kind != 7) ||
      ((result.kind == 6 || result.kind == 7) &&
       (page_fields != 3 || result.page_limit == 0)) ||
      (result.kind != 6 && result.kind != 7 && nonempty_body)) {
    return absl::InvalidArgumentError("Unsupported agent control request");
  }
  return result;
}

absl::StatusOr<std::string> PackGuestLogResult(
    const GuestControlRequest& request, const AgentLogPage& page) {
  if (request.request_id == 0 || request.kind != 6 || page.count > 8 ||
      request.extension_count > 8 ||
      request.extensions.size() > kMaximumControlBytes) {
    return absl::InvalidArgumentError("Invalid guest log result");
  }
  std::string result;
  result.reserve(160 + page.count * 40 + request.extensions.size());
  result.push_back(static_cast<char>(0x85 + request.extension_count));
  WriteString(&result, "v");
  WriteUInt(&result, 1);
  WriteString(&result, "id");
  WriteUInt(&result, request.request_id);
  WriteString(&result, "kind");
  WriteUInt(&result, 4);
  WriteString(&result, "deadline_ms");
  WriteUInt(&result, request.deadline_millis);
  WriteString(&result, "body");
  result.push_back(static_cast<char>(0x83));
  WriteString(&result, "records");
  result.push_back(static_cast<char>(0x90 | page.count));
  for (std::uint8_t index = 0; index < page.count; ++index) {
    const AgentLogRecord& record = page.records[index];
    if (record.sequence == 0) {
      return absl::InvalidArgumentError("Invalid log sequence");
    }
    result.push_back(static_cast<char>(0x84));
    WriteString(&result, "sequence");
    WriteUInt(&result, record.sequence);
    WriteString(&result, "code");
    WriteUInt(&result, static_cast<std::uint8_t>(record.code));
    WriteString(&result, "severity");
    WriteUInt(&result, static_cast<std::uint8_t>(record.severity));
    WriteString(&result, "elapsed_us");
    WriteUInt(&result, record.elapsed_microseconds);
  }
  WriteString(&result, "next_cursor");
  WriteUInt(&result, page.next_cursor);
  WriteString(&result, "gap");
  result.push_back(static_cast<char>(page.gap ? 0xc3 : 0xc2));
  result.append(request.extensions);
  if (result.size() > kMaximumControlBytes) {
    return absl::ResourceExhaustedError("Agent log result exceeds 4 KiB");
  }
  return result;
}

absl::StatusOr<std::string> PackGuestWorkspaceResult(
    const GuestControlRequest& request, const GuestFilePage& page) {
  if (request.request_id == 0 || request.kind != 7 || page.count > 8 ||
      request.extension_count > 8 ||
      request.extensions.size() > kMaximumControlBytes) {
    return absl::InvalidArgumentError("Invalid workspace list result");
  }
  std::string result;
  result.reserve(256 + request.extensions.size());
  result.push_back(static_cast<char>(0x85 + request.extension_count));
  WriteString(&result, "v");
  WriteUInt(&result, 1);
  WriteString(&result, "id");
  WriteUInt(&result, request.request_id);
  WriteString(&result, "kind");
  WriteUInt(&result, 4);
  WriteString(&result, "deadline_ms");
  WriteUInt(&result, request.deadline_millis);
  WriteString(&result, "body");
  result.push_back(static_cast<char>(0x83));
  WriteString(&result, "entries");
  result.push_back(static_cast<char>(0x90 | page.count));
  for (std::uint8_t index = 0; index < page.count; ++index) {
    const GuestFileEntry& entry = page.entries[index];
    if (entry.name.empty() || entry.name.size() > 1024) {
      return absl::InvalidArgumentError("Invalid workspace entry name");
    }
    result.push_back(static_cast<char>(0x84));
    WriteString(&result, "name");
    WriteString(&result, entry.name);
    WriteString(&result, "directory");
    result.push_back(static_cast<char>(entry.is_directory ? 0xc3 : 0xc2));
    WriteString(&result, "read_only");
    result.push_back(static_cast<char>(entry.is_read_only ? 0xc3 : 0xc2));
    WriteString(&result, "size_bytes");
    if (entry.size_bytes) {
      WriteUInt(&result, *entry.size_bytes);
    } else {
      result.push_back(static_cast<char>(0xc0));
    }
  }
  WriteString(&result, "next_offset");
  WriteUInt(&result, page.next_offset);
  WriteString(&result, "more");
  result.push_back(static_cast<char>(page.more ? 0xc3 : 0xc2));
  result.append(request.extensions);
  if (result.size() > kMaximumControlBytes) {
    return absl::ResourceExhaustedError("Workspace result exceeds 4 KiB");
  }
  return result;
}

absl::StatusOr<std::string> PackGuestResult(
    const GuestControlRequest& request) {
  return PackGuestResult(request, GuestStatusSnapshot{});
}

absl::StatusOr<std::string> PackGuestHelloResult(
    const GuestControlRequest& request, bool logs_available,
    std::uint8_t maximum_requests, bool workspace_available) {
  if (request.request_id == 0 || request.kind != 1 || maximum_requests == 0 ||
      request.extension_count > 8 ||
      request.extensions.size() > kMaximumControlBytes) {
    return absl::InvalidArgumentError("Invalid guest hello result");
  }
  std::string result;
  result.reserve(180 + request.extensions.size());
  result.push_back(static_cast<char>(0x85 + request.extension_count));
  WriteString(&result, "v");
  WriteUInt(&result, 1);
  WriteString(&result, "id");
  WriteUInt(&result, request.request_id);
  WriteString(&result, "kind");
  WriteUInt(&result, 4);
  WriteString(&result, "deadline_ms");
  WriteUInt(&result, request.deadline_millis);
  WriteString(&result, "body");
  result.push_back(static_cast<char>(0x84));
  WriteString(&result, "protocol_version");
  WriteUInt(&result, 1);
  WriteString(&result, "maximum_control_bytes");
  WriteUInt(&result, kMaximumControlBytes);
  WriteString(&result, "maximum_requests");
  WriteUInt(&result, maximum_requests);
  WriteString(&result, "capabilities");
  result.push_back(
      static_cast<char>(0x91 + logs_available + workspace_available));
  WriteString(&result, "status");
  if (logs_available) {
    WriteString(&result, "logs");
  }
  if (workspace_available) {
    WriteString(&result, "workspace-list");
  }
  result.append(request.extensions);
  if (result.size() > kMaximumControlBytes) {
    return absl::ResourceExhaustedError("Agent hello exceeds 4 KiB");
  }
  return result;
}

absl::StatusOr<std::string> PackGuestResult(
    const GuestControlRequest& request, const GuestStatusSnapshot& snapshot) {
  if (request.request_id == 0 || request.kind != 2 ||
      request.extension_count > 8 ||
      request.extensions.size() > kMaximumControlBytes) {
    return absl::InvalidArgumentError("Invalid guest result request");
  }
  std::string result;
  result.reserve(128 + request.extensions.size());
  result.push_back(static_cast<char>(0x85 + request.extension_count));
  WriteString(&result, "v");
  WriteUInt(&result, 1);
  WriteString(&result, "id");
  WriteUInt(&result, request.request_id);
  WriteString(&result, "kind");
  WriteUInt(&result, 4);
  WriteString(&result, "deadline_ms");
  WriteUInt(&result, request.deadline_millis);
  WriteString(&result, "body");
  result.push_back(static_cast<char>(0x83 + snapshot.tick.has_value() +
                                     snapshot.display.has_value()));
  WriteString(&result, "service");
  WriteString(&result, "symbian-agent");
  WriteString(&result, "state");
  WriteString(&result, "ready");
  WriteString(&result, "capabilities");
  result.push_back(static_cast<char>(0x91 + snapshot.logs_available +
                                     snapshot.workspace_available));
  WriteString(&result, "status");
  if (snapshot.logs_available) {
    WriteString(&result, "logs");
  }
  if (snapshot.workspace_available) {
    WriteString(&result, "workspace-list");
  }
  if (snapshot.tick) {
    WriteString(&result, "system");
    result.push_back(static_cast<char>(0x82));
    WriteString(&result, "tick_count");
    WriteUInt(&result, snapshot.tick->count);
    WriteString(&result, "tick_period_us");
    WriteUInt(&result, snapshot.tick->period_microseconds);
  }
  if (snapshot.display) {
    WriteString(&result, "display");
    result.push_back(static_cast<char>(0x82));
    WriteString(&result, "width_pixels");
    WriteUInt(&result, snapshot.display->width_pixels);
    WriteString(&result, "height_pixels");
    WriteUInt(&result, snapshot.display->height_pixels);
  }
  result.append(request.extensions);
  if (result.size() > kMaximumControlBytes) {
    return absl::ResourceExhaustedError("Agent result exceeds 4 KiB");
  }
  return result;
}

}  // namespace symbian::agent
