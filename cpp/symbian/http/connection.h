// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.
#ifndef SYMBIAN_HTTP_CONNECTION_H_
#define SYMBIAN_HTTP_CONNECTION_H_
#include <memory>

#include "symbian/http/http1.h"
#include "symbian/http/http2.h"
#include "symbian/net/byte_stream.h"

namespace symbian::http {
enum class Protocol { kHttp11, kHttp2 };

/** @brief Worker-owned HTTP exchange with pull bodies and a response writer.
 *
 * Client sends request headers; Write/Finish send its body. ReceiveHeaders
 * waits for the final response head. Accept waits only for a request head;
 * Read pulls the request body before or while sending a response. On either
 * side Read returns nullopt only at clean end, and errors on truncation.
 * SendHeaders/Write/Finish form the server response writer. An optional length
 * selects fixed framing; nullopt streams chunked HTTP/1.1 or HTTP/2 DATA.
 * Write accepts at most 32 KiB, limiting memory while transport applies
 * backpressure. The caller selects the protocol (TLS must negotiate it).
 *
 * One exchange per connection; no reuse, redirect, decompression, multiplexing
 * or hidden task/thread. Use the SDK worker executor for background execution.
 * Destruction cancels by closing the owned transport. Abort is terminal.
 */
class Connection {
 public:
  static absl::StatusOr<std::unique_ptr<Connection>> Client(
      std::unique_ptr<net::ByteStream> transport, RequestHead request,
      Protocol protocol = Protocol::kHttp11, Limits limits = {},
      std::optional<std::size_t> body_length = 0,
      absl::Time deadline = absl::InfiniteFuture());
  static absl::StatusOr<std::unique_ptr<Connection>> Accept(
      std::unique_ptr<net::ByteStream> transport,
      Protocol protocol = Protocol::kHttp11, Limits limits = {},
      absl::Time deadline = absl::InfiniteFuture());
  ~Connection();

  const RequestHead& request() const { return request_; }

  const ResponseHead& response() const { return response_; }

  const Headers& trailers() const { return trailers_; }

  absl::Status ReceiveHeaders(absl::Time deadline);
  absl::StatusOr<std::optional<std::string>> Read(absl::Time deadline);
  absl::Status SendHeaders(
      ResponseHead head, absl::Time deadline,
      std::optional<std::size_t> body_length = std::nullopt);
  absl::Status Write(std::string_view bytes, absl::Time deadline);
  absl::Status Finish(absl::Time deadline);
  void Abort();

 private:
  Connection(std::unique_ptr<net::ByteStream> transport, Role role,
             Protocol protocol, Limits limits);
  absl::Status Initialize();
  absl::Status Pump(absl::Time deadline);
  absl::Status Flush(absl::Time deadline);
  absl::Status Send(std::string_view bytes, absl::Time deadline);
  absl::Status ReadHead(absl::Time deadline);
  absl::Status Fail(absl::Status status);
  absl::Status PrepareOutput(Headers* headers,
                             std::optional<std::size_t> length, bool no_body);
  std::unique_ptr<net::ByteStream> transport_;
  Role role_;
  Protocol protocol_;
  Limits limits_;
  std::unique_ptr<Http2> h2_;
  RequestHead request_;
  ResponseHead response_;
  Headers trailers_;
  internal::BodyPlan plan_;
  internal::ChunkedDecoder decoder_;
  std::string input_;
  std::size_t read_bytes_ = 0, written_bytes_ = 0;
  std::optional<std::size_t> write_length_;
  bool received_head_ = false, sent_head_ = false, eof_ = false;
  bool read_end_ = false, write_end_ = false, no_write_body_ = false;
  absl::Status error_;
};
}  // namespace symbian::http
#endif
