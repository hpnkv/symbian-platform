// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "symbian/websocket/websocket.h"

#include <string>

#include <absl/base/nullability.h>
#include <gtest/gtest.h>

namespace symbian::websocket {
namespace {
class WebSocketTest : public ::testing::Test {
 protected:
  std::unique_ptr<WebSocket> client, server;

  void SetUp() override {
    Options options;
    options.mask_provider =
        []() -> absl::StatusOr<std::array<std::uint8_t, 4>> {
      return std::array<std::uint8_t, 4>{1, 2, 3, 4};
    };
    auto c = WebSocket::Create(Role::kClient, options);
    auto s = WebSocket::Create(Role::kServer);
    ASSERT_TRUE(c.ok());
    ASSERT_TRUE(s.ok());
    client = std::move(*c);
    server = std::move(*s);
    for (int i = 0; i < 4; ++i) {
      Transfer(client.get(), server.get());
      Transfer(server.get(), client.get());
    }
    ASSERT_TRUE(client->open());
    ASSERT_TRUE(server->open());
  }

  void Transfer(WebSocket* absl_nonnull from, WebSocket* absl_nonnull to) {
    auto output = from->TakeOutput();
    ASSERT_TRUE(output.ok()) << output.status();
    // TCP segmentation must be independent of HTTP/2 and WebSocket framing.
    for (char byte : *output) {
      ASSERT_TRUE(to->Feed(std::string_view(&byte, 1)).ok());
    }
  }

  std::string Data(std::string payload) {
    std::string data(9, '\0');
    data[0] = static_cast<char>(payload.size() >> 16);
    data[1] = static_cast<char>(payload.size() >> 8);
    data[2] = static_cast<char>(payload.size());
    data[8] = 1;  // First client's CONNECT stream.
    data += payload;
    return data;
  }
};

TEST_F(WebSocketTest, BinaryMessagesAndDrainedClose) {
  for (const auto& payload : {std::string(), std::string("hello\0world", 11),
                              std::string(4100, 'x')}) {
    ASSERT_TRUE(client->Send(payload).ok());
    Transfer(client.get(), server.get());
    auto received = server->Receive();
    ASSERT_TRUE(received.ok());
    ASSERT_TRUE(received->has_value());
    EXPECT_EQ(**received, payload);
    ASSERT_TRUE(server->Send(payload).ok());
    Transfer(server.get(), client.get());
    received = client->Receive();
    ASSERT_TRUE(received.ok());
    ASSERT_TRUE(received->has_value());
    EXPECT_EQ(**received, payload);
  }
  ASSERT_TRUE(client->Send("final").ok());
  ASSERT_TRUE(client->Close().ok());
  Transfer(client.get(), server.get());
  EXPECT_TRUE(server->closed());
  EXPECT_EQ(**server->Receive(), "final");
  Transfer(server.get(), client.get());
  EXPECT_TRUE(client->closed());
  EXPECT_FALSE(client->Send("late").ok());
}

TEST_F(WebSocketTest, ServerInitiatedClose) {
  ASSERT_TRUE(server->Close().ok());
  Transfer(server.get(), client.get());
  Transfer(client.get(), server.get());
  EXPECT_TRUE(client->closed());
  EXPECT_TRUE(server->closed());
}

TEST_F(WebSocketTest, RejectsUnmaskedClient) {
  auto status = server->Feed(Data(std::string("\x82\x01x", 3)));
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_FALSE(server->Feed("").ok());
}

TEST_F(WebSocketTest, RejectsOversizeBeforePayload) {
  auto status = server->Feed(Data(std::string("\x82\xfe\x10\x05", 4)));
  EXPECT_EQ(status.code(), absl::StatusCode::kOutOfRange);
}

TEST_F(WebSocketTest, FragmentedMessageAndPing) {
  // Server sends unmasked fragmented binary data, interleaved with a ping.
  ASSERT_TRUE(client
                  ->Feed(Data(std::string("\x02\x02"
                                          "ab"
                                          "\x89\x01"
                                          "p"
                                          "\x80\x02"
                                          "cd",
                                          11)))
                  .ok());
  auto result = client->Receive();
  ASSERT_TRUE(result.ok());
  ASSERT_TRUE(result->has_value());
  EXPECT_EQ(**result, "abcd");
  // Client answers with a masked pong, accepted by the server.
  Transfer(client.get(), server.get());
  EXPECT_FALSE(server->Receive()->has_value());
}

TEST_F(WebSocketTest, RejectsContinuationWithoutStart) {
  EXPECT_EQ(client->Feed(Data(std::string("\x80\x00", 2))).code(),
            absl::StatusCode::kInvalidArgument);
}

TEST_F(WebSocketTest, BoundedSendAndReceiveQueues) {
  EXPECT_EQ(client->Send(std::string(4101, 'x')).code(),
            absl::StatusCode::kResourceExhausted);
  for (int i = 0; i < 16; ++i) {
    ASSERT_TRUE(client->Send("x").ok());
  }
  Transfer(client.get(), server.get());
  ASSERT_TRUE(client->Send("overflow").ok());
  EXPECT_EQ(server->Feed(*client->TakeOutput()).code(),
            absl::StatusCode::kResourceExhausted);
}

TEST_F(WebSocketTest, RepeatedTrafficReplenishesFlowControl) {
  for (int i = 0; i < 100; ++i) {
    ASSERT_TRUE(client->Send(std::string(4100, 'z')).ok());
    Transfer(client.get(), server.get());
    ASSERT_TRUE(server->Receive()->has_value());
    Transfer(server.get(), client.get());
  }
}

TEST(WebSocketOptionsTest, RequiresClientEntropyAndValidBounds) {
  EXPECT_FALSE(WebSocket::Create(Role::kClient).ok());
  Options options;
  options.maximum_message_bytes = 32769;
  EXPECT_FALSE(WebSocket::Create(Role::kServer, options).ok());
}
}  // namespace
}  // namespace symbian::websocket
