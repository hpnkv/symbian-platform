// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.
#include "symbian/http/types.h"

#include <absl/base/nullability.h>

namespace symbian::http {
namespace {
bool Token(std::string_view value) {
  if (value.empty()) {
    return false;
  }
  for (char c : value) {
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') ||
          std::string_view("!#$%&'*+-.^_`|~").find(c) !=
              std::string_view::npos)) {
      return false;
    }
  }
  return true;
}

bool Equal(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) {
    return false;
  }
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (auto lower =
            [](char c) {
              return c >= 'A' && c <= 'Z' ? c + 32 : c;
            };
        lower(a[i]) != lower(b[i])) {
      return false;
    }
  }
  return true;
}
}  // namespace

void NormalizeHeaders(Headers* absl_nonnull headers) {
  for (auto& [name, value] : *headers) {
    for (char& c : name) {
      if (c >= 'A' && c <= 'Z') {
        c += 32;
      }
    }
  }
}

std::optional<std::string> GetHeader(const Headers& headers,
                                     std::string_view name) {
  for (const auto& [key, value] : headers) {
    if (Equal(key, name)) {
      return value;
    }
  }
  return std::nullopt;
}

absl::Status ValidateHeaders(const Headers& headers, const Limits& limits) {
  std::size_t total = 0;
  if (headers.size() > limits.maximum_headers) {
    return absl::ResourceExhaustedError("HTTP header count exceeds limit");
  }
  for (const auto& [name, value] : headers) {
    if (!Token(name)) {
      return absl::InvalidArgumentError("Invalid HTTP field name");
    }
    for (char c : value) {
      if ((c < 32 && c != '\t') || c == 127) {
        return absl::InvalidArgumentError("Invalid HTTP field value");
      }
    }
    total += name.size() + value.size() + 4;
    if (total > limits.maximum_header_bytes) {
      return absl::ResourceExhaustedError("HTTP headers exceed limit");
    }
  }
  return absl::OkStatus();
}

absl::Status ValidateRequest(const RequestHead& head, const Limits& limits) {
  if (!Token(head.method) || head.path.empty() || head.path[0] != '/' ||
      head.authority.empty() ||
      (head.scheme != "http" && head.scheme != "https") ||
      head.path.size() + head.authority.size() > limits.maximum_header_bytes) {
    return absl::InvalidArgumentError("Invalid HTTP request target");
  }
  for (const auto& v : {head.path, head.authority}) {
    for (char c : v) {
      if (c <= 32 || c == 127) {
        return absl::InvalidArgumentError("Invalid HTTP request target byte");
      }
    }
  }
  return ValidateHeaders(head.headers, limits);
}
}  // namespace symbian::http
