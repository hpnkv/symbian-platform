// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include <cstring>
#include <deque>

#include <absl/base/nullability.h>
#include <gtest/gtest.h>

#include "symbian/http/connection.h"

namespace symbian::http {
namespace {
class MemoryStream final {
 public:
  explicit MemoryStream(std::string input, std::size_t fragment = 7)
      : input_(std::move(input)), fragment_(fragment) {}

  absl::StatusOr<std::size_t> Read(std::span<std::uint8_t> bytes, absl::Time) {
    auto count = std::min({bytes.size(), input_.size(), fragment_});
    std::memcpy(bytes.data(), input_.data(), count);
    input_.erase(0, count);
    return count;
  }

  absl::Status Write(std::span<const std::uint8_t> bytes, absl::Time) {
    output.append(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    return absl::OkStatus();
  }

  void Close() { closed = true; }

  std::string output;
  bool closed = false;

 private:
  std::string input_;
  std::size_t fragment_;
};

constexpr auto kDeadline = absl::InfiniteFuture();

TEST(ByteStreamTest, MoveTransfersSingularTransportOwnership) {
  struct CountingTransport {
    int* absl_nonnull closed;
    int* absl_nonnull destroyed;

    ~CountingTransport() { ++*destroyed; }

    absl::StatusOr<std::size_t> Read(std::span<std::uint8_t>, absl::Time) {
      return std::size_t{0};
    }

    absl::Status Write(std::span<const std::uint8_t>, absl::Time) {
      return absl::OkStatus();
    }

    void Close() { ++*closed; }
  };

  int closed = 0;
  int destroyed = 0;
  {
    net::ByteStream first(
        std::make_unique<CountingTransport>(&closed, &destroyed));
    net::ByteStream second(std::move(first));
    EXPECT_FALSE(first);
    EXPECT_FALSE(first.Read({}, kDeadline).ok());
    EXPECT_TRUE(second);
    auto count = second.Read({}, kDeadline);
    ASSERT_TRUE(count.ok());
    EXPECT_EQ(*count, 0);
    second.Close();
    EXPECT_EQ(closed, 1);
    EXPECT_EQ(destroyed, 0);
  }
  EXPECT_EQ(destroyed, 1);
}

RequestHead Request(std::string method = "GET") {
  RequestHead head;
  head.authority = "example.test";
  head.method = std::move(method);
  return head;
}

std::unique_ptr<Connection> Client(std::string input,
                                   std::string method = "GET") {
  auto result = Connection::Client(
      std::make_unique<MemoryStream>(std::move(input)), Request(method));
  EXPECT_TRUE(result.ok()) << result.status();
  if (!result.ok()) {
    return nullptr;
  }
  EXPECT_TRUE((*result)->Finish(kDeadline).ok());
  return std::move(*result);
}

absl::StatusOr<std::string> ReadAll(Connection* absl_nonnull connection) {
  std::string body;
  while (true) {
    auto chunk = connection->Read(kDeadline);
    if (!chunk.ok()) {
      return chunk.status();
    }
    if (!chunk->has_value()) {
      return body;
    }
    EXPECT_FALSE((**chunk).empty());
    body.append(**chunk);
  }
}

TEST(Http1Test, PullFixedChunkedAndCloseDelimited) {
  for (const auto& wire :
       {"HTTP/1.1 200 OK\r\nContent-Length: 11\r\n\r\nhello world",
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: "
        "chunked\r\n\r\n5\r\nhello\r\n6\r\n world\r\n0\r\nx-check: yes\r\n\r\n",
        "HTTP/1.0 200 OK\r\n\r\nhello world"}) {
    auto c = Client(wire);
    ASSERT_NE(c, nullptr);
    auto body = ReadAll(c.get());
    ASSERT_TRUE(body.ok()) << body.status();
    EXPECT_EQ(*body, "hello world");
    EXPECT_EQ(c->response().status, 200);
  }
}

TEST(Http1Test, TrailersAndInformationalResponse) {
  auto c = Client(
      "HTTP/1.1 103 Early Hints\r\nLink: a\r\n\r\nHTTP/1.1 200 "
      "OK\r\nTransfer-Encoding: chunked\r\n\r\n1\r\nx\r\n0\r\nx-check: "
      "yes\r\n\r\n");
  auto body = ReadAll(c.get());
  ASSERT_TRUE(body.ok()) << body.status();
  EXPECT_EQ(*body, "x");
  EXPECT_EQ(GetHeader(c->trailers(), "x-check"), "yes");
}

TEST(Http1Test, HeadAndBodylessStatuses) {
  for (int status : {200, 204, 304}) {
    auto c = Client("HTTP/1.1 " + std::to_string(status) +
                        " OK\r\nContent-Length: 42\r\n\r\n",
                    "HEAD");
    auto body = ReadAll(c.get());
    ASSERT_TRUE(body.ok()) << body.status();
    EXPECT_TRUE(body->empty());
  }
}

TEST(Http1Test, TruncationAmbiguityAndInjectionFail) {
  for (const auto& wire :
       {"HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nx",
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nContent-Length: "
        "5\r\n\r\n",
        "HTTP/1.1 200 OK\r\nContent-Length: 5\r\nContent-Length: "
        "5\r\n\r\nhello",
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: gzip, chunked\r\n\r\n",
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: "
        "chunked\r\n\r\n1\r\nx!\n0\r\n\r\n",
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n1\r\nx\r\n",
        "HTTP/1.1 200 OK\r\nContent-Length: 184467440737095516160\r\n\r\n"}) {
    auto c = Client(wire);
    EXPECT_FALSE(ReadAll(c.get()).ok()) << wire;
  }
  auto head = Request();
  head.headers = {{"x", "ok\r\nInjected: yes"}};
  EXPECT_FALSE(
      Connection::Client(std::make_unique<MemoryStream>(""), head).ok());
}

TEST(Http1Test, ServerPullRequestAndStreamResponse) {
  auto transport = std::make_unique<MemoryStream>(
      "POST /upload HTTP/1.1\r\nHost: example.test\r\nTransfer-Encoding: "
      "chunked\r\n\r\n3\r\nabc\r\n0\r\n\r\n",
      1);
  auto* absl_nonnull wire = transport.get();
  auto server = Connection::Accept(std::move(transport));
  ASSERT_TRUE(server.ok()) << server.status();
  EXPECT_EQ((*server)->request().method, "POST");
  EXPECT_EQ((*server)->request().path, "/upload");
  auto body = ReadAll(&**server);
  ASSERT_TRUE(body.ok()) << body.status();
  EXPECT_EQ(*body, "abc");
  ASSERT_TRUE(
      (*server)
          ->SendHeaders({200, {{"content-type", "text/plain"}}}, kDeadline)
          .ok());
  ASSERT_TRUE((*server)->Write("first", kDeadline).ok());
  ASSERT_TRUE((*server)->Write("second", kDeadline).ok());
  ASSERT_TRUE((*server)->Finish(kDeadline).ok());
  auto c = Client(wire->output);
  EXPECT_EQ(*ReadAll(c.get()), "firstsecond");
  EXPECT_FALSE((*server)->Write("late", kDeadline).ok());
}

TEST(Http1Test, BoundedMetadataAndBody) {
  Limits limits;
  limits.maximum_body_bytes = 8;
  auto c = Connection::Client(
      std::make_unique<MemoryStream>(
          "HTTP/1.1 200 OK\r\nContent-Length: 9\r\n\r\n123456789"),
      Request(), Protocol::kHttp11, limits);
  ASSERT_TRUE(c.ok());
  EXPECT_FALSE(ReadAll(c->get()).ok());
  auto over = Client("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n" +
                     std::string(17000, 'f'));
  EXPECT_FALSE(ReadAll(over.get()).ok());
  auto fixed = Connection::Client(std::make_unique<MemoryStream>(""),
                                  Request("POST"), Protocol::kHttp11, {}, 4);
  ASSERT_TRUE(fixed.ok());
  EXPECT_FALSE((*fixed)->Finish(kDeadline).ok());
}

void Transfer(Http2* absl_nonnull from, Http2* absl_nonnull to,
              std::size_t fragment = 13) {
  auto wire = from->TakeOutput();
  ASSERT_TRUE(wire.ok()) << wire.status();
  std::string_view bytes(*wire);
  while (!bytes.empty()) {
    auto size = std::min(bytes.size(), fragment);
    ASSERT_TRUE(to->Feed(bytes.substr(0, size)).ok());
    bytes.remove_prefix(size);
  }
}

TEST(Http2Test, SharedDuplexAndPullFlowControl) {
  auto client = Http2::CreateUnique(Role::kClient);
  auto server = Http2::CreateUnique(Role::kServer);
  ASSERT_TRUE(client.ok());
  ASSERT_TRUE(server.ok());
  Transfer(&**client, &**server);
  Transfer(&**server, &**client);
  ASSERT_TRUE((*client)->SendRequest(Request("POST")).ok());
  Transfer(&**client, &**server);
  ASSERT_TRUE((*server)->headers_received());
  ASSERT_TRUE(
      (*server)
          ->SendHeaders({200, {{"content-type", "application/octet-stream"}}})
          .ok());
  Transfer(&**server, &**client);
  ASSERT_TRUE((*client)->headers_received());
  std::size_t received = 0;
  // Deliberately stop reading until the stream window is consumed.
  ASSERT_TRUE((*server)->Write(std::string(65536, 'x')).ok());
  Transfer(&**server, &**client);
  EXPECT_GT((*server)->buffered_amount(), 0);
  for (int i = 0; i < 80; ++i) {
    auto part = (*client)->Read();
    ASSERT_TRUE(part.ok()) << part.status();
    if (part->has_value()) {
      received += (**part).size();
    }
    Transfer(&**client, &**server);
    Transfer(&**server, &**client);
  }
  EXPECT_EQ(received, 65536);
  EXPECT_EQ((*server)->buffered_amount(), 0);
  ASSERT_TRUE((*client)->Finish().ok());
  Transfer(&**client, &**server);
  ASSERT_TRUE((*server)->Finish().ok());
  Transfer(&**server, &**client);
  EXPECT_TRUE((*client)->ended());
  EXPECT_TRUE((*server)->ended());
}

TEST(Http2Test, ConnectionClientReadsNativeServerOutput) {
  auto server = Http2::CreateUnique(Role::kServer);
  ASSERT_TRUE(server.ok());
  auto client_codec = Http2::CreateUnique(Role::kClient);
  ASSERT_TRUE(client_codec.ok());
  ASSERT_TRUE((*client_codec)->SendRequest(Request()).ok());
  ASSERT_TRUE((*client_codec)->Finish().ok());
  Transfer(&**client_codec, &**server);
  ASSERT_TRUE((*server)->SendHeaders({200, {}}).ok());
  ASSERT_TRUE((*server)->Write("hello").ok());
  ASSERT_TRUE((*server)->Finish().ok());
  auto wire = (*server)->TakeOutput();
  ASSERT_TRUE(wire.ok());
  auto c = Connection::Client(std::make_unique<MemoryStream>(*wire), Request(),
                              Protocol::kHttp2);
  ASSERT_TRUE(c.ok());
  ASSERT_TRUE((*c)->Finish(kDeadline).ok());
  auto body = ReadAll(&**c);
  ASSERT_TRUE(body.ok()) << body.status();
  EXPECT_EQ(*body, "hello");
}

TEST(Http2Test, MovesTransferTheSessionAndLeaveAnEmptyOwner) {
  auto original = Http2::Create(Role::kClient);
  auto server = Http2::Create(Role::kServer);
  ASSERT_TRUE(original.ok());
  ASSERT_TRUE(server.ok());
  Http2 client(std::move(*original));
  EXPECT_FALSE(original->Feed({}).ok());
  EXPECT_FALSE(original->TakeOutput().ok());
  EXPECT_FALSE(original->Read().ok());
  EXPECT_TRUE(original->ended());
  EXPECT_FALSE(original->headers_received());
  EXPECT_FALSE(original->peer_settings_received());
  EXPECT_FALSE(original->peer_connect_enabled());
  EXPECT_TRUE(original->request().headers.empty());
  EXPECT_TRUE(original->response().headers.empty());
  EXPECT_TRUE(original->trailers().empty());
  EXPECT_EQ(original->buffered_amount(), 0);
  original->Abort();
  auto replacement = Http2::Create(Role::kClient);
  ASSERT_TRUE(replacement.ok());
  *replacement = std::move(client);
  EXPECT_FALSE(client.Finish().ok());
  auto wire = replacement->TakeOutput();
  ASSERT_TRUE(wire.ok());
  ASSERT_FALSE(wire->empty());
  ASSERT_TRUE(server->Feed(*wire).ok());
  EXPECT_TRUE(server->peer_settings_received());
  auto reply = server->TakeOutput();
  ASSERT_TRUE(reply.ok());
  ASSERT_TRUE(replacement->Feed(*reply).ok());
  EXPECT_TRUE(replacement->peer_settings_received());
}
}  // namespace
}  // namespace symbian::http
