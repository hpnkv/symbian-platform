// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "symbian/agent/frame.h"

namespace {

using symbian::agent::EncodeFrame;
using symbian::agent::Frame;
using symbian::agent::FrameDecoder;
using symbian::agent::kMaximumFrameBytes;

TEST(AgentFrame, IncrementalPayloadAndAdjacentFrames) {
  FrameDecoder decoder;
  std::optional<Frame> completed;
  const std::uint8_t first[] = {0, 0, 0, 3, 0x81, 0xa1};
  auto count = decoder.Consume(first, &completed);
  ASSERT_TRUE(count.ok()) << count.status();
  EXPECT_EQ(*count, sizeof(first));
  EXPECT_FALSE(completed.has_value());
  EXPECT_EQ(decoder.buffered_bytes(), 2);

  const std::uint8_t rest[] = {'x', 0, 0, 0, 1, 0xc0};
  count = decoder.Consume(rest, &completed);
  ASSERT_TRUE(count.ok()) << count.status();
  EXPECT_EQ(*count, 1);
  ASSERT_TRUE(completed.has_value());
  EXPECT_EQ(completed->payload, std::string("\x81\xa1x", 3));
  count = decoder.Consume(std::span(rest).subspan(*count), &completed);
  ASSERT_TRUE(count.ok()) << count.status();
  EXPECT_EQ(*count, 5);
  ASSERT_TRUE(completed.has_value());
  EXPECT_EQ(completed->payload, std::string("\xc0", 1));
}

TEST(AgentFrame, RejectsOversizeBeforePayloadAllocation) {
  FrameDecoder decoder;
  std::optional<Frame> completed;
  const std::uint8_t too_large[] = {0, 1, 0, 1};
  auto result = decoder.Consume(too_large, &completed);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(decoder.buffered_bytes(), 0);
  const std::uint8_t valid[] = {0, 0, 0, 1, 0xc0};
  EXPECT_FALSE(decoder.Consume(valid, &completed).ok());
  decoder.Reset();
  result = decoder.Consume(valid, &completed);
  EXPECT_TRUE(result.ok()) << result.status();
  ASSERT_TRUE(completed.has_value());
}

TEST(AgentFrame, RejectsZeroAndEnforcesSmallerConfiguredLimit) {
  FrameDecoder decoder(2);
  std::optional<Frame> completed;
  const std::uint8_t zero[] = {0, 0, 0, 0};
  EXPECT_FALSE(decoder.Consume(zero, &completed).ok());
  decoder.Reset();
  const std::uint8_t three[] = {0, 0, 0, 3};
  EXPECT_FALSE(decoder.Consume(three, &completed).ok());
  EXPECT_FALSE(EncodeFrame(std::span(three).subspan(0, 3), 2).ok());
}

TEST(AgentFrame, EncodesBoundaryAndRoundTripsBinary) {
  std::vector<std::uint8_t> payload(kMaximumFrameBytes, 0xc0);
  auto frame = EncodeFrame(payload);
  ASSERT_TRUE(frame.ok()) << frame.status();
  EXPECT_EQ(frame->size(), kMaximumFrameBytes + 4);
  EXPECT_EQ(static_cast<unsigned char>((*frame)[0]), 0);
  EXPECT_EQ(static_cast<unsigned char>((*frame)[1]), 1);
  FrameDecoder decoder;
  std::optional<Frame> completed;
  const auto* data = reinterpret_cast<const std::uint8_t*>(frame->data());
  auto count = decoder.Consume(std::span(data, frame->size()), &completed);
  ASSERT_TRUE(count.ok()) << count.status();
  EXPECT_EQ(*count, frame->size());
  ASSERT_TRUE(completed.has_value());
  EXPECT_EQ(completed->payload.size(), payload.size());
  EXPECT_FALSE(EncodeFrame(std::span<const std::uint8_t>()).ok());
  payload.push_back(0);
  EXPECT_FALSE(EncodeFrame(payload).ok());
}

}  // namespace
