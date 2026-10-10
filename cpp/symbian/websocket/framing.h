// Copyright 2026 The A11 Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Extracted from A11's Http2WebSocketChannel. Helpers and ParseFrames are
// verbatim, except the thread annotation: the SDK endpoint has one owner.
// WriteFrame only replaces the unavailable transport/RNG/lock boundary and
// accepts string_view to avoid a payload copy.
#ifndef SYMBIAN_WEBSOCKET_FRAMING_H_
#define SYMBIAN_WEBSOCKET_FRAMING_H_

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <absl/base/nullability.h>

#include "absl/status/status.h"
#include "symbian/websocket/websocket.h"

namespace symbian::websocket::internal {
constexpr std::uint8_t kContinuation = 0x0;
constexpr std::uint8_t kText = 0x1;
constexpr std::uint8_t kBinary = 0x2;
constexpr std::uint8_t kClose = 0x8;
constexpr std::uint8_t kPing = 0x9;
constexpr std::uint8_t kPong = 0xa;

void AppendBigEndian16(std::string* absl_nonnull output, std::uint16_t value) {
  output->push_back(static_cast<char>((value >> 8U) & 0xffU));
  output->push_back(static_cast<char>(value & 0xffU));
}

void AppendBigEndian64(std::string* absl_nonnull output, std::uint64_t value) {
  for (int shift = 56; shift >= 0; shift -= 8) {
    output->push_back(static_cast<char>((value >> shift) & 0xffU));
  }
}

/**
 * XORs `length` bytes at `data` with a repeating 4-byte WebSocket mask.
 *
 * A word at a time, because this runs over every byte a client sends and every
 * byte a server receives: RFC 6455 masking is not optional and not negotiable.
 * The obvious `data[i] ^= mask[i % 4]` costs a division per byte and defeats
 * vectorisation, which at 64 KiB per message is a real fraction of the
 * message's whole cost. Reading the mask as one 32-bit word and XORing
 * word-wise leaves the compiler free to widen it further.
 *
 * Always from the start of the mask, which is all RFC 6455 needs: every frame
 * carries its own masking key and applies it from its own first payload byte,
 * so a fragmented message's continuations do not continue the previous frame's
 * mask.
 */
void MaskInPlace(char* absl_nonnull data, size_t length,
                 const char (&mask)[4]) {
  std::uint32_t word = 0;
  std::memcpy(&word, mask, sizeof(word));
  size_t index = 0;
  for (; index + sizeof(word) <= length; index += sizeof(word)) {
    std::uint32_t chunk = 0;
    std::memcpy(&chunk, data + index, sizeof(chunk));
    chunk ^= word;
    std::memcpy(data + index, &chunk, sizeof(chunk));
  }
  for (; index < length; ++index) {
    data[index] ^= mask[index % 4];
  }
}

std::uint16_t ReadBigEndian16(std::string_view input, size_t offset) {
  return static_cast<std::uint16_t>(
      (static_cast<std::uint16_t>(static_cast<unsigned char>(input[offset]))
       << 8U) |
      static_cast<std::uint16_t>(
          static_cast<unsigned char>(input[offset + 1])));
}

std::uint64_t ReadBigEndian64(std::string_view input, size_t offset) {
  std::uint64_t result = 0;
  for (size_t index = 0; index < sizeof(result); ++index) {
    result = (result << 8U) | static_cast<unsigned char>(input[offset + index]);
  }
  return result;
}

class Framing {
 public:
  Framing(Role role, size_t maximum)
      : role_(role), max_message_size_(maximum) {}

  struct ParsedActions {
    std::vector<std::string> messages;
    std::vector<std::string> pongs;
    std::optional<std::string> close;
  };

  absl::Status ParseFrames(ParsedActions* absl_nonnull actions) {
    size_t consumed = 0;
    while (input_.size() - consumed >= 2) {
      const auto first = static_cast<unsigned char>(input_[consumed]);
      const auto second = static_cast<unsigned char>(input_[consumed + 1]);
      const bool final = (first & 0x80U) != 0;
      const std::uint8_t opcode = first & 0x0fU;
      if ((first & 0x70U) != 0) {
        return absl::InvalidArgumentError("WebSocket RSV bits are not zero");
      }
      const bool masked = (second & 0x80U) != 0;
      if (masked != (role_ == Role::kServer)) {
        return absl::InvalidArgumentError(
            role_ == Role::kServer
                ? "WebSocket client frame is not masked"
                : "WebSocket server frame is unexpectedly masked");
      }

      std::uint64_t payload_size = second & 0x7fU;
      size_t header_size = 2;
      if (payload_size == 126) {
        if (input_.size() - consumed < 4) {
          break;
        }
        payload_size = ReadBigEndian16(input_, consumed + 2);
        header_size = 4;
      } else if (payload_size == 127) {
        if (input_.size() - consumed < 10) {
          break;
        }
        payload_size = ReadBigEndian64(input_, consumed + 2);
        if ((payload_size & (std::uint64_t{1} << 63U)) != 0) {
          return absl::InvalidArgumentError("WebSocket length is invalid");
        }
        header_size = 10;
      }
      if (payload_size > max_message_size_ ||
          payload_size > std::numeric_limits<size_t>::max()) {
        return absl::OutOfRangeError(
            "WebSocket frame exceeds max_message_size");
      }
      if (const bool control = opcode >= kClose;
          control && (!final || payload_size > 125)) {
        return absl::InvalidArgumentError(
            "WebSocket control frame must be final and at most 125 bytes");
      }
      const size_t mask_size = masked ? 4 : 0;
      const size_t full_size =
          header_size + mask_size + static_cast<size_t>(payload_size);
      if (input_.size() - consumed < full_size) {
        break;
      }

      const size_t mask_offset = consumed + header_size;
      const size_t payload_offset = mask_offset + mask_size;
      // Copied out before the payload is taken, because taking it may move the
      // buffer the mask lives in.
      char mask[4] = {0, 0, 0, 0};
      if (masked) {
        std::memcpy(mask, input_.data() + mask_offset, sizeof(mask));
      }
      std::string payload;
      bool taken = false;
      if (consumed == 0 && payload_offset + payload_size == input_.size()) {
        // The buffer holds exactly this one frame and nothing before it, which
        // is the common case for a message that arrived in its own TCP read.
        payload = std::move(input_);
        input_.clear();
        payload.erase(0, payload_offset);
        taken = true;
      } else {
        payload =
            input_.substr(payload_offset, static_cast<size_t>(payload_size));
      }
      if (masked) {
        MaskInPlace(payload.data(), payload.size(), mask);
      }
      // Taking the buffer already consumed it: there is nothing left to skip
      // past and nothing left to erase, and leaving `consumed` set would make
      // the loop's `input_.size() - consumed` underflow on the next pass.
      consumed = taken ? 0 : consumed + full_size;

      if (opcode == kPing) {
        actions->pongs.push_back(std::move(payload));
      } else if (opcode == kPong) {
        continue;
      } else if (opcode == kClose) {
        if (payload.size() == 1) {
          return absl::InvalidArgumentError(
              "WebSocket close code is truncated");
        }
        actions->close = std::move(payload);
        break;
      } else if (opcode == kText) {
        return absl::InvalidArgumentError(
            "A11 binary channel received a text WebSocket frame");
      } else if (opcode == kBinary) {
        if (fragment_opcode_.has_value()) {
          return absl::InvalidArgumentError(
              "WebSocket data frame interrupted a fragmented message");
        }
        if (final) {
          actions->messages.push_back(std::move(payload));
        } else {
          fragment_opcode_ = opcode;
          fragmented_ = std::move(payload);
        }
      } else if (opcode == kContinuation) {
        if (!fragment_opcode_.has_value()) {
          return absl::InvalidArgumentError(
              "WebSocket continuation has no initial frame");
        }
        if (fragmented_.size() + payload.size() > max_message_size_) {
          return absl::OutOfRangeError(
              "Fragmented WebSocket message exceeds max_message_size");
        }
        fragmented_.append(payload);
        if (final) {
          actions->messages.push_back(std::move(fragmented_));
          fragmented_.clear();
          fragment_opcode_.reset();
        }
      } else {
        return absl::InvalidArgumentError("WebSocket opcode is unsupported");
      }
    }
    if (consumed != 0) {
      input_.erase(0, consumed);
    }
    return absl::OkStatus();
  }

  std::string WriteFrame(std::uint8_t opcode, std::string_view payload,
                         std::uint32_t masking_key) {
    const bool masked = role_ == Role::kClient;
    std::string frame;
    frame.reserve(payload.size() + 14);
    frame.push_back(static_cast<char>(0x80U | opcode));
    if (const std::uint8_t mask_flag = masked ? 0x80U : 0;
        payload.size() <= 125) {
      frame.push_back(static_cast<char>(mask_flag | payload.size()));
    } else if (payload.size() <= std::numeric_limits<std::uint16_t>::max()) {
      frame.push_back(static_cast<char>(mask_flag | 126U));
      AppendBigEndian16(&frame, static_cast<std::uint16_t>(payload.size()));
    } else {
      frame.push_back(static_cast<char>(mask_flag | 127U));
      AppendBigEndian64(&frame, payload.size());
    }
    if (masked) {
      const auto key = masking_key;
      const char mask[4] = {
          static_cast<char>((key >> 24U) & 0xffU),
          static_cast<char>((key >> 16U) & 0xffU),
          static_cast<char>((key >> 8U) & 0xffU),
          static_cast<char>(key & 0xffU),
      };
      frame.append(mask, sizeof(mask));
      const size_t body = frame.size();
      frame.append(payload);
      MaskInPlace(frame.data() + body, payload.size(), mask);
    } else {
      frame.append(payload);
    }
    return frame;
  }

  const Role role_;
  const size_t max_message_size_;
  std::string input_;
  std::optional<std::uint8_t> fragment_opcode_;
  std::string fragmented_;
};
}  // namespace symbian::websocket::internal
#endif
