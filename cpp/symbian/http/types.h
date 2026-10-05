// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.
#ifndef SYMBIAN_HTTP_TYPES_H_
#define SYMBIAN_HTTP_TYPES_H_
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"

namespace symbian::http {
using Headers = std::vector<std::pair<std::string, std::string>>;
enum class Role { kClient, kServer };

struct RequestHead {
  std::string method = "GET";
  std::string scheme = "http";
  std::string authority;
  std::string path = "/";
  std::string protocol;  // Extended CONNECT, e.g. websocket.
  Headers headers;
};

struct ResponseHead {
  int status = 0;
  Headers headers;
};

struct Limits {
  std::size_t maximum_header_bytes = 16384;
  std::size_t maximum_headers = 64;
  std::size_t maximum_buffered_bytes = 65536;
  std::size_t maximum_body_bytes = 32 * 1024 * 1024;
};

void NormalizeHeaders(Headers* headers);
std::optional<std::string> GetHeader(const Headers& headers,
                                     std::string_view name);
absl::Status ValidateHeaders(const Headers& headers, const Limits& limits);
absl::Status ValidateRequest(const RequestHead& head, const Limits& limits);
}  // namespace symbian::http
#endif
