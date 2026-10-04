// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "symbian/agent/control.h"
#include "symbian/agent/frame.h"
#include "symbian/status/json_codec.h"

namespace {

using symbian::agent::EncodeFrame;
using symbian::agent::Frame;
using symbian::agent::FrameDecoder;
using symbian::agent::InboundQueue;
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

TEST(AgentFrame, QueueBoundsAndReleasesPayloads) {
  InboundQueue queue;
  for (int i = 0; i < 4; ++i) {
    EXPECT_TRUE(queue.Push(Frame{std::string(64 * 1024, 'x')}).ok());
  }
  EXPECT_EQ(queue.size(), 4);
  EXPECT_EQ(queue.queued_bytes(), 256 * 1024);
  EXPECT_FALSE(queue.Push(Frame{"x"}).ok());
  auto oldest = queue.Pop();
  ASSERT_TRUE(oldest.has_value());
  EXPECT_EQ(oldest->payload.size(), 64 * 1024);
  EXPECT_EQ(queue.queued_bytes(), 192 * 1024);
  EXPECT_TRUE(queue.Push(Frame{"x"}).ok());
  EXPECT_EQ(queue.size(), 4);
  queue.Clear();
  EXPECT_EQ(queue.queued_bytes(), 0);
  EXPECT_FALSE(queue.Pop().has_value());
  EXPECT_FALSE(queue.Push(Frame{""}).ok());
  EXPECT_FALSE(queue.Push(Frame{std::string(64 * 1024 + 1, 'x')}).ok());
}

TEST(AgentFrame, ControlRoundTripPreservesUnknownFields) {
  symbian::agent::ControlMessage message;
  message.request_id = 17;
  message.kind = symbian::agent::ControlKind::kStatus;
  message.deadline_millis = 1500;
  message.body = {{"scope", "self"}};
  message.extensions = {{"future", 42}};
  auto encoded = symbian::agent::PackControl(message);
  ASSERT_TRUE(encoded.ok()) << encoded.status();
  auto parsed = symbian::agent::ParseControl(*encoded);
  ASSERT_TRUE(parsed.ok()) << parsed.status();
  EXPECT_EQ(parsed->request_id, 17);
  EXPECT_EQ(parsed->kind, symbian::agent::ControlKind::kStatus);
  EXPECT_EQ(parsed->deadline_millis, 1500);
  EXPECT_EQ(parsed->body, message.body);
  EXPECT_EQ(parsed->extensions, message.extensions);
}

TEST(AgentFrame, ControlRejectsMalformedAndOversizedInput) {
  auto packed = symbian::PackMsgpack(
      nlohmann::json{{"v", 1}, {"id", 9}, {"kind", 250}}, "test");
  ASSERT_TRUE(packed.ok());
  EXPECT_FALSE(symbian::agent::ParseControl(*packed).ok());
  packed = symbian::PackMsgpack(
      nlohmann::json{{"v", 1}, {"id", 9}, {"kind", 2}, {"body", "bad"}},
      "test");
  ASSERT_TRUE(packed.ok());
  EXPECT_FALSE(symbian::agent::ParseControl(*packed).ok());
  packed = symbian::PackMsgpack(
      nlohmann::json{{"v", 1}, {"id", 9}, {"kind", 2}}, "test");
  ASSERT_TRUE(packed.ok());
  EXPECT_TRUE(symbian::agent::ParseControl(*packed).ok());
  EXPECT_FALSE(symbian::agent::ParseControl(std::string(4097, 'x')).ok());
  symbian::agent::ControlMessage too_large;
  too_large.request_id = 9;
  too_large.body = {{"bytes", std::string(4096, 'x')}};
  EXPECT_FALSE(symbian::agent::PackControl(too_large).ok());
}

}  // namespace
