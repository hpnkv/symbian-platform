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
  result->push_back(static_cast<char>(0xa0 | value.size()));
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
      if (!cursor.Map(&body_items) || body_items != 0) {
        return absl::InvalidArgumentError("Unsupported control body");
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
      result.request_id == 0 || (result.kind != 1 && result.kind != 2)) {
    return absl::InvalidArgumentError("Unsupported agent control request");
  }
  return result;
}

absl::StatusOr<std::string> PackGuestResult(
    const GuestControlRequest& request) {
  if (request.request_id == 0 || (request.kind != 1 && request.kind != 2) ||
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
  result.push_back(static_cast<char>(0x83));
  WriteString(&result, "service");
  WriteString(&result, "symbian-agent");
  WriteString(&result, "state");
  WriteString(&result, "ready");
  WriteString(&result, "capabilities");
  result.push_back(static_cast<char>(0x91));
  WriteString(&result, "status");
  result.append(request.extensions);
  if (result.size() > kMaximumControlBytes) {
    return absl::ResourceExhaustedError("Agent result exceeds 4 KiB");
  }
  return result;
}

}  // namespace symbian::agent
