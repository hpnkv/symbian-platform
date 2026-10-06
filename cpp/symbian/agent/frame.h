// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_AGENT_FRAME_H_
#define SYMBIAN_AGENT_FRAME_H_

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <span>
#include <string>

#include <absl/base/nullability.h>

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

/** @brief Validate a complete four-byte network-order prefix before allocation. */
absl::StatusOr<std::size_t> DecodeFrameLength(
    std::span<const std::uint8_t> prefix,
    std::size_t maximum_frame_bytes = kMaximumFrameBytes);

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
  absl::StatusOr<std::size_t> Consume(
      std::span<const std::uint8_t> bytes,
      std::optional<Frame>* absl_nullable completed);

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

/**
 * @brief Completed inbound frames awaiting schema dispatch.
 *
 * Counts encoded payload bytes. A session owns one decoder and one queue and
 * releases both on disconnect. This class does not authenticate or interpret
 * the queued MessagePack.
 */
class InboundQueue {
 public:
  /** @brief Admit a completed frame or return ResourceExhausted. */
  absl::Status Push(Frame frame);

  /** @brief Remove the oldest frame, if any. */
  std::optional<Frame> Pop();

  std::size_t queued_bytes() const { return queued_bytes_; }

  std::size_t size() const { return frames_.size(); }

  /** @brief Release all queued payloads after disconnect. */
  void Clear();

 private:
  std::deque<Frame> frames_;
  std::size_t queued_bytes_ = 0;
};

}  // namespace symbian::agent

#endif  // SYMBIAN_AGENT_FRAME_H_
