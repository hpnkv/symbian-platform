// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.
#ifndef SYMBIAN_HTTP_HTTP2_H_
#define SYMBIAN_HTTP_HTTP2_H_
#include <memory>

#include "absl/status/statusor.h"
#include "symbian/http/types.h"

namespace symbian::http {
/** @brief Bounded, I/O-free single-exchange nghttp2 connection.
 *
 * Shared HTTP DATA primitive for ordinary HTTP and RFC 8441 WebSockets.
 * Feed arbitrary transport chunks; drain TakeOutput before reading transport.
 * Read returns an empty optional while no DATA is available; ended() separately
 * identifies clean END_STREAM. Reading releases flow-control credit. Write
 * and Finish operate independently of the read half for duplex CONNECT.
 * Header and DATA errors are terminal. No sockets, callbacks into Python, or
 * scheduler are owned here. Only one request is supported per connection.
 * A moved-from owner is ended and empty; status-returning operations fail
 * with FailedPrecondition, queries return empty values, and Abort is harmless.
 */
class Http2 {
 public:
  static absl::StatusOr<Http2> Create(Role role, Limits limits = {});
  static absl::StatusOr<std::unique_ptr<Http2>> CreateUnique(
      Role role, Limits limits = {});
  Http2(const Http2&) = delete;
  Http2& operator=(const Http2&) = delete;
  ~Http2();
  Http2(Http2&& other) noexcept;
  Http2& operator=(Http2&& other) noexcept;
  absl::Status Feed(std::string_view bytes);
  absl::StatusOr<std::string> TakeOutput();
  absl::Status SendRequest(RequestHead head);
  absl::Status SendHeaders(ResponseHead head);
  absl::Status Write(std::string_view bytes);
  absl::Status Finish();
  absl::StatusOr<std::optional<std::string>> Read();
  bool headers_received() const;
  bool ended() const;
  bool peer_settings_received() const;
  bool peer_connect_enabled() const;
  const RequestHead& request() const;
  const ResponseHead& response() const;
  const Headers& trailers() const;
  std::size_t buffered_amount() const;
  void Abort();

 private:
  struct State;
  explicit Http2(std::unique_ptr<State> state);
  std::unique_ptr<State> state_;
};
}  // namespace symbian::http
#endif
