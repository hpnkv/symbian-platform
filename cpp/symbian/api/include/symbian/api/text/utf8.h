// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_API_TEXT_UTF8_H_
#define SYMBIAN_API_TEXT_UTF8_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace symbian::api::text {

// Converts strict UTF-8 to the UTF-16 code units used at native descriptor
// boundaries. Embedded NULs are retained. Invalid and truncated sequences
// return InvalidArgument; ordinary string allocation follows runtime policy.
inline absl::StatusOr<std::u16string> Utf8ToUtf16(std::string_view input) {
  std::u16string result;
  result.reserve(input.size());
  for (std::size_t index = 0; index < input.size();) {
    const auto first = static_cast<std::uint8_t>(input[index]);
    std::uint32_t scalar = 0;
    std::size_t length = 0;
    std::uint32_t minimum = 0;
    if (first < 0x80) {
      scalar = first;
      length = 1;
    } else if (first >= 0xC2 && first <= 0xDF) {
      scalar = first & 0x1F;
      length = 2;
      minimum = 0x80;
    } else if (first >= 0xE0 && first <= 0xEF) {
      scalar = first & 0x0F;
      length = 3;
      minimum = 0x800;
    } else if (first >= 0xF0 && first <= 0xF4) {
      scalar = first & 0x07;
      length = 4;
      minimum = 0x10000;
    } else {
      return absl::InvalidArgumentError("Invalid UTF-8 leading byte");
    }
    if (length > input.size() - index) {
      return absl::InvalidArgumentError("Truncated UTF-8 sequence");
    }
    for (std::size_t offset = 1; offset < length; ++offset) {
      const auto next = static_cast<std::uint8_t>(input[index + offset]);
      if ((next & 0xC0) != 0x80) {
        return absl::InvalidArgumentError("Invalid UTF-8 continuation");
      }
      scalar = (scalar << 6) | (next & 0x3F);
    }
    if (scalar < minimum || scalar > 0x10FFFF ||
        (scalar >= 0xD800 && scalar <= 0xDFFF)) {
      return absl::InvalidArgumentError("Invalid UTF-8 scalar");
    }
    if (scalar <= 0xFFFF) {
      result.push_back(static_cast<char16_t>(scalar));
    } else {
      scalar -= 0x10000;
      result.push_back(static_cast<char16_t>(0xD800 + (scalar >> 10)));
      result.push_back(static_cast<char16_t>(0xDC00 + (scalar & 0x3FF)));
    }
    index += length;
  }
  return result;
}

// Converts UTF-16, rejecting unpaired surrogates. Unlike the older C
// mbrtowc adapter, this API supports supplementary Unicode scalars.
inline absl::StatusOr<std::string> Utf16ToUtf8(std::u16string_view input) {
  std::string result;
  result.reserve(input.size());
  for (std::size_t index = 0; index < input.size(); ++index) {
    std::uint32_t scalar = input[index];
    if (scalar >= 0xD800 && scalar <= 0xDBFF) {
      if (index + 1 == input.size() || input[index + 1] < 0xDC00 ||
          input[index + 1] > 0xDFFF) {
        return absl::InvalidArgumentError("Unpaired UTF-16 high surrogate");
      }
      scalar = 0x10000 + ((scalar - 0xD800) << 10) +
               (input[++index] - 0xDC00);
    } else if (scalar >= 0xDC00 && scalar <= 0xDFFF) {
      return absl::InvalidArgumentError("Unpaired UTF-16 low surrogate");
    }
    if (scalar < 0x80) {
      result.push_back(static_cast<char>(scalar));
    } else if (scalar < 0x800) {
      result.push_back(static_cast<char>(0xC0 | (scalar >> 6)));
      result.push_back(static_cast<char>(0x80 | (scalar & 0x3F)));
    } else if (scalar < 0x10000) {
      result.push_back(static_cast<char>(0xE0 | (scalar >> 12)));
      result.push_back(static_cast<char>(0x80 | ((scalar >> 6) & 0x3F)));
      result.push_back(static_cast<char>(0x80 | (scalar & 0x3F)));
    } else {
      result.push_back(static_cast<char>(0xF0 | (scalar >> 18)));
      result.push_back(static_cast<char>(0x80 | ((scalar >> 12) & 0x3F)));
      result.push_back(static_cast<char>(0x80 | ((scalar >> 6) & 0x3F)));
      result.push_back(static_cast<char>(0x80 | (scalar & 0x3F)));
    }
  }
  return result;
}

}  // namespace symbian::api::text

#endif  // SYMBIAN_API_TEXT_UTF8_H_
