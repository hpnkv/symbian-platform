// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <absl/base/nullability.h>
#include <gtest/gtest.h>

#include "symbian/agent/control.h"
#include "symbian/agent/frame.h"
#include "symbian/agent/guest_control.h"
#include "symbian/agent/guest_log.h"
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
  const std::uint8_t acceptable[] = {0, 0, 0, 2};
  EXPECT_EQ(*symbian::agent::DecodeFrameLength(acceptable, 2), 2);
  EXPECT_FALSE(symbian::agent::DecodeFrameLength(acceptable, 1).ok());
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
  const auto* absl_nonnull data =
      reinterpret_cast<const std::uint8_t*>(frame->data());
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

TEST(AgentFrame, GuestReadOnlyResultMatchesHostControlEnvelope) {
  symbian::agent::ControlMessage request;
  request.request_id = 17;
  request.kind = symbian::agent::ControlKind::kStatus;
  request.deadline_millis = 1500;
  request.extensions = {{"future", 42}};
  auto encoded = symbian::agent::PackControl(request);
  ASSERT_TRUE(encoded.ok()) << encoded.status();
  auto guest = symbian::agent::ParseGuestControl(*encoded);
  ASSERT_TRUE(guest.ok()) << guest.status();
  EXPECT_EQ(guest->request_id, 17);
  EXPECT_EQ(guest->kind, 2);
  EXPECT_EQ(guest->deadline_millis, 1500);
  EXPECT_EQ(guest->extension_count, 1);
  auto response = symbian::agent::PackGuestResult(*guest);
  ASSERT_TRUE(response.ok()) << response.status();
  auto host = symbian::agent::ParseControl(*response);
  ASSERT_TRUE(host.ok()) << host.status();
  EXPECT_EQ(host->request_id, 17);
  EXPECT_EQ(host->kind, symbian::agent::ControlKind::kResult);
  EXPECT_EQ(host->body["service"], "symbian-agent");
  EXPECT_EQ(host->body["capabilities"],
            nlohmann::json::array({"status", "screen-capture",
                                   "pointer-event", "resource-read",
                                   "resource-write", "package-open",
                                   "app-registered"}));
  EXPECT_EQ(host->extensions, request.extensions);
}

TEST(AgentFrame, GuestStatusIncludesOnlyAvailableNativeSnapshots) {
  symbian::agent::GuestControlRequest request;
  request.request_id = 3;
  request.kind = 2;
  symbian::agent::GuestStatusSnapshot snapshot;
  snapshot.tick = symbian::agent::GuestTickSnapshot{91, 1000};
  auto encoded = symbian::agent::PackGuestResult(request, snapshot);
  ASSERT_TRUE(encoded.ok()) << encoded.status();
  auto parsed = symbian::agent::ParseControl(*encoded);
  ASSERT_TRUE(parsed.ok()) << parsed.status();
  EXPECT_EQ(parsed->body["system"]["tick_count"], 91);
  EXPECT_EQ(parsed->body["system"]["tick_period_us"], 1000);
  EXPECT_FALSE(parsed->body.contains("display"));
  snapshot.display = symbian::agent::GuestDisplaySnapshot{640, 360};
  encoded = symbian::agent::PackGuestResult(request, snapshot);
  ASSERT_TRUE(encoded.ok()) << encoded.status();
  parsed = symbian::agent::ParseControl(*encoded);
  ASSERT_TRUE(parsed.ok()) << parsed.status();
  EXPECT_EQ(parsed->body["display"]["width_pixels"], 640);
  EXPECT_EQ(parsed->body["display"]["height_pixels"], 360);
  snapshot.logs_available = true;
  encoded = symbian::agent::PackGuestResult(request, snapshot);
  ASSERT_TRUE(encoded.ok()) << encoded.status();
  parsed = symbian::agent::ParseControl(*encoded);
  ASSERT_TRUE(parsed.ok()) << parsed.status();
  EXPECT_EQ(parsed->body["capabilities"],
            nlohmann::json::array({"status", "logs", "screen-capture",
                                   "pointer-event", "resource-read",
                                   "resource-write", "package-open",
                                   "app-registered"}));
}

TEST(AgentFrame, GuestHelloAdvertisesBoundedServiceProfile) {
  symbian::agent::GuestControlRequest request;
  request.request_id = 11;
  request.kind = 1;
  auto encoded = symbian::agent::PackGuestHelloResult(request, true, 16);
  ASSERT_TRUE(encoded.ok()) << encoded.status();
  auto parsed = symbian::agent::ParseControl(*encoded);
  ASSERT_TRUE(parsed.ok()) << parsed.status();
  EXPECT_EQ(parsed->body["protocol_version"], 1);
  EXPECT_EQ(parsed->body["maximum_control_bytes"], 4096);
  EXPECT_EQ(parsed->body["maximum_requests"], 16);
  EXPECT_EQ(parsed->body["capabilities"],
            nlohmann::json::array({"status", "logs", "screen-capture",
                                   "pointer-event", "resource-read",
                                   "resource-write", "package-open",
                                   "app-registered"}));
  request.kind = 2;
  EXPECT_FALSE(symbian::agent::PackGuestHelloResult(request, true, 16).ok());
  request.kind = 1;
  EXPECT_FALSE(symbian::agent::PackGuestResult(request).ok());
}

TEST(AgentFrame, GuestLogCursorReportsOverwrittenRecords) {
  symbian::agent::AgentLogRing ring;
  for (int index = 0; index < 40; ++index) {
    ring.Append(symbian::agent::AgentLogCode::kStatusRead);
  }
  auto first = ring.ReadAfter(0, 8);
  ASSERT_TRUE(first.ok()) << first.status();
  EXPECT_TRUE(first->gap);
  ASSERT_EQ(first->count, 8);
  EXPECT_EQ(first->records[0].sequence, 9);
  EXPECT_EQ(first->records[0].severity,
            symbian::agent::AgentLogSeverity::kDebug);
  EXPECT_LE(first->records[0].elapsed_microseconds,
            first->records[1].elapsed_microseconds);
  EXPECT_EQ(first->next_cursor, 16);
  auto second = ring.ReadAfter(first->next_cursor, 8);
  ASSERT_TRUE(second.ok()) << second.status();
  EXPECT_FALSE(second->gap);
  EXPECT_EQ(second->records[0].sequence, 17);
  auto current = ring.ReadAfter(40, 8);
  ASSERT_TRUE(current.ok()) << current.status();
  EXPECT_EQ(current->count, 0);
  EXPECT_EQ(current->next_cursor, 40);
  EXPECT_FALSE(ring.ReadAfter(0, 0).ok());
}

TEST(AgentFrame, GuestLogRequestAndResultRoundTrip) {
  symbian::agent::ControlMessage request;
  request.request_id = 21;
  request.kind = symbian::agent::ControlKind::kLogs;
  request.body = {{"after", 4}, {"limit", 2}};
  auto encoded = symbian::agent::PackControl(request);
  ASSERT_TRUE(encoded.ok()) << encoded.status();
  auto guest = symbian::agent::ParseGuestControl(*encoded);
  ASSERT_TRUE(guest.ok()) << guest.status();
  EXPECT_EQ(guest->kind, 6);
  EXPECT_EQ(guest->page_after, 4);
  EXPECT_EQ(guest->page_limit, 2);
  symbian::agent::AgentLogRing ring;
  for (int index = 0; index < 6; ++index) {
    ring.Append(symbian::agent::AgentLogCode::kStatusRead);
  }
  auto page = ring.ReadAfter(guest->page_after, guest->page_limit);
  ASSERT_TRUE(page.ok()) << page.status();
  auto result = symbian::agent::PackGuestLogResult(*guest, *page);
  ASSERT_TRUE(result.ok()) << result.status();
  auto parsed = symbian::agent::ParseControl(*result);
  ASSERT_TRUE(parsed.ok()) << parsed.status();
  EXPECT_EQ(parsed->body["records"].size(), 2);
  EXPECT_EQ(parsed->body["records"][0]["severity"], 1);
  EXPECT_TRUE(parsed->body["records"][0]["elapsed_us"].is_number_unsigned());
  EXPECT_EQ(parsed->body["records"][0]["sequence"], 5);
  EXPECT_EQ(parsed->body["records"][0]["code"], 2);
  EXPECT_EQ(parsed->body["next_cursor"], 6);
  EXPECT_EQ(parsed->body["gap"], false);
  request.body = {{"after", 4}};
  encoded = symbian::agent::PackControl(request);
  ASSERT_TRUE(encoded.ok()) << encoded.status();
  EXPECT_FALSE(symbian::agent::ParseGuestControl(*encoded).ok());
}

TEST(AgentFrame, GuestWorkspacePageIsScopedAndBounded) {
  symbian::agent::ControlMessage request;
  request.request_id = 31;
  request.kind = symbian::agent::ControlKind::kWorkspaceList;
  request.body = {{"after", 3}, {"limit", 2}};
  auto encoded = symbian::agent::PackControl(request);
  ASSERT_TRUE(encoded.ok()) << encoded.status();
  auto guest = symbian::agent::ParseGuestControl(*encoded);
  ASSERT_TRUE(guest.ok()) << guest.status();
  EXPECT_EQ(guest->kind, 7);
  EXPECT_EQ(guest->page_after, 3);
  EXPECT_EQ(guest->page_limit, 2);

  symbian::agent::GuestFilePage page;
  page.count = 1;
  page.next_offset = 4;
  page.more = false;
  page.entries[0] = {.name = std::string(40, 'x') + "-\xc3\xa9.txt",
                     .is_directory = false,
                     .is_read_only = true,
                     .size_bytes = 17};
  auto result = symbian::agent::PackGuestWorkspaceResult(*guest, page);
  ASSERT_TRUE(result.ok()) << result.status();
  auto parsed = symbian::agent::ParseControl(*result);
  ASSERT_TRUE(parsed.ok()) << parsed.status();
  EXPECT_EQ(parsed->body["entries"][0]["name"], page.entries[0].name);
  EXPECT_EQ(parsed->body["entries"][0]["size_bytes"], 17);
  EXPECT_EQ(parsed->body["entries"][0]["read_only"], true);
  EXPECT_EQ(parsed->body["next_offset"], 4);

  request.body = {{"path", "C:\\\\sys\\bin"}, {"limit", 2}};
  encoded = symbian::agent::PackControl(request);
  ASSERT_TRUE(encoded.ok()) << encoded.status();
  EXPECT_FALSE(symbian::agent::ParseGuestControl(*encoded).ok());
  auto hello = symbian::agent::PackGuestHelloResult(
      symbian::agent::GuestControlRequest{.request_id = 1, .kind = 1}, true, 16,
      true);
  ASSERT_TRUE(hello.ok()) << hello.status();
  parsed = symbian::agent::ParseControl(*hello);
  ASSERT_TRUE(parsed.ok()) << parsed.status();
  EXPECT_EQ(parsed->body["capabilities"],
            nlohmann::json::array({"status", "logs", "workspace-list",
                                   "screen-capture", "pointer-event",
                                   "resource-read", "resource-write",
                                   "package-open", "app-registered"}));
}

TEST(AgentFrame, GuestScreenAndPointerRequestsAreBounded) {
  symbian::agent::ControlMessage request;
  request.request_id = 42;
  request.kind = symbian::agent::ControlKind::kScreenCapture;
  auto encoded = symbian::agent::PackControl(request);
  ASSERT_TRUE(encoded.ok());
  auto guest = symbian::agent::ParseGuestControl(*encoded);
  ASSERT_TRUE(guest.ok()) << guest.status();
  auto result = symbian::agent::PackGuestScreenResult(*guest, 360, 640, 720,
                                                       720 * 640);
  ASSERT_TRUE(result.ok()) << result.status();
  auto parsed = symbian::agent::ParseControl(*result);
  ASSERT_TRUE(parsed.ok()) << parsed.status();
  EXPECT_EQ(parsed->body["format"], "rgb565-le");
  EXPECT_EQ(parsed->body["data_bytes"], 720 * 640);
  EXPECT_FALSE(symbian::agent::PackGuestScreenResult(*guest, 360, 640, 700,
                                                      700 * 640).ok());

  request.kind = symbian::agent::ControlKind::kPointerEvent;
  request.body = {{"action", 2}, {"x", 120}, {"y", 240}};
  encoded = symbian::agent::PackControl(request);
  ASSERT_TRUE(encoded.ok());
  guest = symbian::agent::ParseGuestControl(*encoded);
  ASSERT_TRUE(guest.ok()) << guest.status();
  EXPECT_EQ(guest->pointer_action, 2);
  EXPECT_EQ(guest->pointer_x, 120);
  EXPECT_EQ(guest->pointer_y, 240);
  EXPECT_TRUE(symbian::agent::PackGuestPointerResult(*guest).ok());
  request.body["x"] = 4096;
  encoded = symbian::agent::PackControl(request);
  ASSERT_TRUE(encoded.ok());
  EXPECT_FALSE(symbian::agent::ParseGuestControl(*encoded).ok());
}

TEST(AgentFrame, GuestResourceTransferRequestsAreScoped) {
  symbian::agent::ControlMessage request;
  request.request_id = 44;
  request.kind = symbian::agent::ControlKind::kResourceRead;
  request.body = {{"scope", 1}, {"uid", 0xe0000a59u},
                  {"name", "capture.dat"}, {"offset", 32768},
                  {"length", 32768}};
  auto encoded = symbian::agent::PackControl(request);
  ASSERT_TRUE(encoded.ok());
  auto guest = symbian::agent::ParseGuestControl(*encoded);
  ASSERT_TRUE(guest.ok()) << guest.status();
  EXPECT_EQ(guest->resource_uid, 0xe0000a59u);
  EXPECT_EQ(guest->resource_name, "capture.dat");
  auto result =
      symbian::agent::PackGuestResourceReadResult(*guest, 65536, 32768);
  ASSERT_TRUE(result.ok()) << result.status();
  auto parsed = symbian::agent::ParseControl(*result);
  ASSERT_TRUE(parsed.ok());
  EXPECT_EQ(parsed->body["total_bytes"], 65536);
  EXPECT_EQ(parsed->body["data_bytes"], 32768);

  request.kind = symbian::agent::ControlKind::kResourceWrite;
  request.body["mode"] = 2;
  encoded = symbian::agent::PackControl(request);
  ASSERT_TRUE(encoded.ok());
  guest = symbian::agent::ParseGuestControl(*encoded);
  ASSERT_TRUE(guest.ok()) << guest.status();
  EXPECT_TRUE(symbian::agent::PackGuestResourceWriteResult(*guest).ok());
  request.body["name"] = "..\\sys\\bin";
  encoded = symbian::agent::PackControl(request);
  ASSERT_TRUE(encoded.ok());
  EXPECT_FALSE(symbian::agent::ParseGuestControl(*encoded).ok());
}

TEST(AgentFrame, GuestPackageLaunchAndRegistrationAreDistinct) {
  symbian::agent::ControlMessage request;
  request.request_id = 45;
  request.kind = symbian::agent::ControlKind::kPackageOpen;
  request.body = {{"uid", 0xe0000a59u}, {"name", "camera.sis"}};
  auto encoded = symbian::agent::PackControl(request);
  ASSERT_TRUE(encoded.ok());
  auto guest = symbian::agent::ParseGuestControl(*encoded);
  ASSERT_TRUE(guest.ok()) << guest.status();
  auto launched = symbian::agent::PackGuestPackageOpenResult(*guest, true);
  ASSERT_TRUE(launched.ok());
  auto parsed = symbian::agent::ParseControl(*launched);
  ASSERT_TRUE(parsed.ok());
  EXPECT_EQ(parsed->body["state"], "installer-launched");
  EXPECT_EQ(parsed->body["registered_before"], true);

  request.kind = symbian::agent::ControlKind::kAppRegistered;
  request.body = {{"uid", 0xe0000a59u}};
  encoded = symbian::agent::PackControl(request);
  ASSERT_TRUE(encoded.ok());
  guest = symbian::agent::ParseGuestControl(*encoded);
  ASSERT_TRUE(guest.ok()) << guest.status();
  auto registered = symbian::agent::PackGuestAppRegisteredResult(*guest, false);
  ASSERT_TRUE(registered.ok());
  parsed = symbian::agent::ParseControl(*registered);
  ASSERT_TRUE(parsed.ok());
  EXPECT_EQ(parsed->body["registered"], false);
}

TEST(AgentFrame, GuestReadOnlyRejectsMalformedAndExcessiveInput) {
  for (const nlohmann::json& value : {
           nlohmann::json{{"v", 1}, {"id", 0}, {"kind", 1}},
           nlohmann::json{{"v", 2}, {"id", 1}, {"kind", 1}},
           nlohmann::json{{"v", 1}, {"id", 1}, {"kind", 3}},
           nlohmann::json{
               {"v", 1}, {"id", 1}, {"kind", 1}, {"body", {{"write", true}}}},
       }) {
    auto bytes = symbian::PackMsgpack(value, "test");
    ASSERT_TRUE(bytes.ok());
    EXPECT_FALSE(symbian::agent::ParseGuestControl(*bytes).ok());
  }
  EXPECT_FALSE(symbian::agent::ParseGuestControl(std::string(4097, 'x')).ok());
  const std::string duplicate_id =
      "\x84\xa1v\x01\xa2id\x01\xa2id\x02\xa4kind\x01";
  EXPECT_FALSE(symbian::agent::ParseGuestControl(duplicate_id).ok());
  const std::string duplicate_extension =
      "\x85\xa1v\x01\xa2id\x01\xa4kind\x01\xa1x\x01\xa1x\x02";
  EXPECT_FALSE(symbian::agent::ParseGuestControl(duplicate_extension).ok());
  EXPECT_FALSE(symbian::agent::ParseGuestControl("").ok());
}

}  // namespace
