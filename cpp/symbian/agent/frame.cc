// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "symbian/agent/frame.h"

#include <algorithm>

#include "absl/status/status.h"

namespace symbian::agent {

absl::StatusOr<std::size_t> DecodeFrameLength(
    std::span<const std::uint8_t> prefix, std::size_t maximum_frame_bytes) {
  if (prefix.size() != 4) {
    return absl::InvalidArgumentError("Frame prefix must be four bytes");
  }
  const std::size_t size = (static_cast<std::uint32_t>(prefix[0]) << 24) |
                           (static_cast<std::uint32_t>(prefix[1]) << 16) |
                           (static_cast<std::uint32_t>(prefix[2]) << 8) |
                           static_cast<std::uint32_t>(prefix[3]);
  if (size == 0 || size > maximum_frame_bytes || size > kMaximumFrameBytes) {
    return absl::InvalidArgumentError("Frame length is outside the limit");
  }
  return size;
}

FrameDecoder::FrameDecoder(std::size_t maximum_frame_bytes)
    : maximum_frame_bytes_(std::min(maximum_frame_bytes, kMaximumFrameBytes)) {}

void FrameDecoder::Reset() {
  prefix_size_ = 0;
  expected_size_ = 0;
  payload_.clear();
  failed_ = false;
}

absl::StatusOr<std::size_t> FrameDecoder::Consume(
    std::span<const std::uint8_t> bytes, std::optional<Frame>* completed) {
  if (completed == nullptr) {
    return absl::InvalidArgumentError("Frame output is null");
  }
  completed->reset();
  if (failed_) {
    return absl::FailedPreconditionError("Frame decoder needs Reset");
  }
  std::size_t offset = 0;
  while (prefix_size_ < sizeof(prefix_) && offset < bytes.size()) {
    prefix_[prefix_size_++] = bytes[offset++];
  }
  if (prefix_size_ < sizeof(prefix_)) {
    return offset;
  }
  if (expected_size_ == 0) {
    auto length = DecodeFrameLength(prefix_, maximum_frame_bytes_);
    if (!length.ok()) {
      failed_ = true;
      return length.status();
    }
    expected_size_ = *length;
    payload_.reserve(expected_size_);
  }
  const std::size_t available = bytes.size() - offset;
  const std::size_t needed = expected_size_ - payload_.size();
  const std::size_t count = std::min(available, needed);
  if (count != 0) {
    payload_.append(reinterpret_cast<const char*>(bytes.data() + offset),
                    count);
  }
  offset += count;
  if (payload_.size() == expected_size_) {
    *completed = Frame{std::move(payload_)};
    prefix_size_ = 0;
    expected_size_ = 0;
    payload_.clear();
  }
  return offset;
}

absl::StatusOr<std::string> EncodeFrame(std::span<const std::uint8_t> payload,
                                        std::size_t maximum_frame_bytes) {
  if (payload.empty() || payload.size() > maximum_frame_bytes ||
      payload.size() > kMaximumFrameBytes) {
    return absl::InvalidArgumentError("Frame payload is outside the limit");
  }
  const std::uint32_t size = static_cast<std::uint32_t>(payload.size());
  std::string frame;
  frame.reserve(sizeof(size) + size);
  frame.push_back(static_cast<char>(size >> 24));
  frame.push_back(static_cast<char>(size >> 16));
  frame.push_back(static_cast<char>(size >> 8));
  frame.push_back(static_cast<char>(size));
  frame.append(reinterpret_cast<const char*>(payload.data()), payload.size());
  return frame;
}

absl::Status InboundQueue::Push(Frame frame) {
  if (frame.payload.empty() || frame.payload.size() > kMaximumFrameBytes) {
    return absl::InvalidArgumentError("Queued frame is outside the limit");
  }
  if (frames_.size() >= kMaximumQueuedFrames ||
      frame.payload.size() > kMaximumQueuedBytes - queued_bytes_) {
    return absl::ResourceExhaustedError("Inbound frame queue is full");
  }
  queued_bytes_ += frame.payload.size();
  frames_.push_back(std::move(frame));
  return absl::OkStatus();
}

std::optional<Frame> InboundQueue::Pop() {
  if (frames_.empty()) {
    return std::nullopt;
  }
  Frame frame = std::move(frames_.front());
  queued_bytes_ -= frame.payload.size();
  frames_.pop_front();
  return frame;
}

void InboundQueue::Clear() {
  frames_.clear();
  queued_bytes_ = 0;
}

}  // namespace symbian::agent
