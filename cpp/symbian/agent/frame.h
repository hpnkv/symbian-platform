// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_AGENT_FRAME_H_
#define SYMBIAN_AGENT_FRAME_H_

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

#include "absl/status/statusor.h"

namespace symbian::agent {

/** @brief Maximum encoded MessagePack payload accepted by the phone profile. */
inline constexpr std::size_t kMaximumFrameBytes = 64 * 1024;

/** @brief Maximum encoded bytes queued across inbound phone messages. */
inline constexpr std::size_t kMaximumQueuedBytes = 256 * 1024;

/** @brief Maximum number of completed inbound phone messages. */
inline constexpr std::size_t kMaximumQueuedFrames = 4;

/** @brief One length-prefixed MessagePack payload, without a parsed schema. */
struct Frame {
  std::string payload;
};

/**
 * @brief Incrementally reads one network-order, 32-bit length-prefixed frame.
 *
 * No payload buffer is allocated until the complete four-byte prefix has been
 * checked against the configured limit. A zero length is invalid. The caller
 * must authenticate its transport before handing frames to a request parser;
 * this class does not grant permissions or interpret MessagePack.
 */
class FrameDecoder {
 public:
  explicit FrameDecoder(std::size_t maximum_frame_bytes = kMaximumFrameBytes);

  /** @brief Number of payload bytes already buffered for the current frame. */
  std::size_t buffered_bytes() const { return payload_.size(); }

  /** @brief Clear an incomplete frame after disconnect or cancellation. */
  void Reset();

  /**
   * @brief Consume at most one frame from @p bytes.
   * @param bytes Input bytes; any unused suffix belongs to the next frame.
   * @param completed Receives a frame when this call completes one.
   * @return Number of bytes consumed, or InvalidArgument for a bad prefix.
   *         After an error, call Reset before accepting further input.
   */
  absl::StatusOr<std::size_t> Consume(std::span<const std::uint8_t> bytes,
                                      std::optional<Frame>* completed);

 private:
  std::size_t maximum_frame_bytes_;
  std::uint8_t prefix_[4] = {};
  std::size_t prefix_size_ = 0;
  std::size_t expected_size_ = 0;
  std::string payload_;
  bool failed_ = false;
};

/**
 * @brief Encode one already validated MessagePack payload for an authenticated
 * transport. A zero-size or oversized payload returns InvalidArgument.
 */
absl::StatusOr<std::string> EncodeFrame(
    std::span<const std::uint8_t> payload,
    std::size_t maximum_frame_bytes = kMaximumFrameBytes);

}  // namespace symbian::agent

#endif  // SYMBIAN_AGENT_FRAME_H_
