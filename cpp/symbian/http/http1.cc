// Copyright 2026 The A11 Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "symbian/http/http1.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <absl/base/nullability.h>

namespace symbian::http::internal {
namespace {

std::string_view Trim(std::string_view v) {
  while (!v.empty() && (v.front() == ' ' || v.front() == '\t')) {
    v.remove_prefix(1);
  }
  while (!v.empty() && (v.back() == ' ' || v.back() == '\t')) {
    v.remove_suffix(1);
  }
  return v;
}

void Lower(std::string* absl_nonnull v) {
  for (char& c : *v) {
    if (c >= 'A' && c <= 'Z') {
      c += 32;
    }
  }
}

void Upper(std::string* absl_nonnull v) {
  for (char& c : *v) {
    if (c >= 'a' && c <= 'z') {
      c -= 32;
    }
  }
}

bool Equal(std::string_view a, std::string_view b) {
  std::string x(a), y(b);
  Lower(&x);
  Lower(&y);
  return x == y;
}

std::vector<std::string_view> Split(
    std::string_view v, char separator,
    std::size_t max = std::numeric_limits<std::size_t>::max()) {
  std::vector<std::string_view> out;
  while (out.size() < max) {
    auto pos = v.find(separator);
    if (pos == std::string_view::npos) {
      break;
    }
    out.push_back(v.substr(0, pos));
    v.remove_prefix(pos + 1);
  }
  out.push_back(v);
  return out;
}

template <class T>
bool Number(std::string_view v, T* absl_nonnull out, unsigned base) {
  if (v.empty()) {
    return false;
  }
  std::uint64_t n = 0;
  for (char c : v) {
    unsigned digit =
        c >= '0' && c <= '9'   ? static_cast<unsigned>(c - '0')
        : c >= 'a' && c <= 'f' ? static_cast<unsigned>(c - 'a' + 10)
        : c >= 'A' && c <= 'F' ? static_cast<unsigned>(c - 'A' + 10)
                               : 99;
    if (digit >= base || n > (std::numeric_limits<T>::max() - digit) / base) {
      return false;
    }
    n = n * base + digit;
  }
  *out = static_cast<T>(n);
  return true;
}

template <class T>
bool Decimal(std::string_view v, T* absl_nonnull out) {
  return Number(v, out, 10);
}

template <class T>
bool HexNumber(std::string_view v, T* absl_nonnull out) {
  return Number(v, out, 16);
}

std::string Hex(std::size_t n) {
  std::string result;
  do {
    result.insert(result.begin(), "0123456789abcdef"[n % 16]);
    n /= 16;
  } while (n);
  return result;
}

void Append(std::string* absl_nonnull out, std::string_view v) {
  out->append(v);
}

void Append(std::string* absl_nonnull out, int v) {
  out->append(std::to_string(v));
}

template <class... T>
std::string Cat(const T&... parts) {
  std::string out;
  (Append(&out, parts), ...);
  return out;
}

constexpr std::string_view kCrlf = "\r\n";

// Splits a header block (without the terminating blank line) into its start
// line and the individual, unfolded header lines.
absl::StatusOr<std::vector<std::string_view>> SplitLines(
    std::string_view head_block) {
  std::vector<std::string_view> lines;
  size_t start = 0;
  while (start <= head_block.size()) {
    const size_t newline = head_block.find('\n', start);
    std::string_view line;
    if (newline == std::string_view::npos) {
      line = head_block.substr(start);
      start = head_block.size() + 1;
    } else {
      line = head_block.substr(start, newline - start);
      start = newline + 1;
    }
    if (!line.empty() && line.back() == '\r') {
      line.remove_suffix(1);
    }
    if (line.empty()) {
      continue;  // Skip the trailing blank line, if any.
    }
    lines.push_back(line);
  }
  if (lines.empty()) {
    return absl::InvalidArgumentError("Empty HTTP/1.1 message head");
  }
  return lines;
}

absl::Status ParseHeaderLines(const std::vector<std::string_view>& lines,
                              size_t first, Headers* absl_nonnull headers) {
  for (size_t index = first; index < lines.size(); ++index) {
    const std::string_view line = lines[index];
    const size_t colon = line.find(':');
    if (colon == std::string_view::npos || colon == 0) {
      return absl::InvalidArgumentError(
          Cat("Malformed HTTP/1.1 header line: ", line));
    }
    std::string name(line.substr(0, colon));
    // Field names must not contain whitespace (obs-fold / smuggling guard).
    if (name.find_first_of(" \t") != std::string::npos) {
      return absl::InvalidArgumentError(
          Cat("Invalid whitespace in HTTP/1.1 header name: ", name));
    }
    Lower(&name);
    std::string_view value = line.substr(colon + 1);
    value = Trim(value);
    headers->emplace_back(std::move(name), std::string(value));
  }
  return absl::OkStatus();
}

// Case-insensitively returns whether the comma-separated header @p name
// contains the token @p token (e.g. Connection: keep-alive, Upgrade).
[[maybe_unused]] bool HeaderContainsToken(const Headers& headers,
                                          std::string_view name,
                                          std::string_view token) {
  const std::optional<std::string> value = GetHeader(headers, name);
  if (!value.has_value()) {
    return false;
  }
  for (std::string_view piece : Split(*value, ',')) {
    if (Equal(Trim(piece), token)) {
      return true;
    }
  }
  return false;
}

}  // namespace

std::optional<std::size_t> FindHeaderBlockEnd(std::string_view data) {
  const size_t crlf = data.find("\r\n\r\n");
  const size_t lf = data.find("\n\n");
  size_t best = std::string_view::npos;
  size_t width = 0;
  if (crlf != std::string_view::npos) {
    best = crlf;
    width = 4;
  }
  if (lf != std::string_view::npos && lf < best) {
    best = lf;
    width = 2;
  }
  if (best == std::string_view::npos) {
    return std::nullopt;
  }
  return best + width;
}

absl::StatusOr<Http1RequestHead> ParseRequestHead(std::string_view head_block) {
  auto parsed_lines = SplitLines(head_block);
  if (!parsed_lines.ok()) {
    return parsed_lines.status();
  }
  const auto& lines = *parsed_lines;
  const std::vector<std::string_view> parts = Split(lines[0], ' ', 2);
  if (parts.size() != 3 || parts[0].empty() || parts[1].empty()) {
    return absl::InvalidArgumentError(
        Cat("Malformed HTTP/1.1 request line: ", lines[0]));
  }
  if (parts[2] != "HTTP/1.1" && parts[2] != "HTTP/1.0") {
    return absl::InvalidArgumentError(
        Cat("Unsupported HTTP version: ", parts[2]));
  }
  Http1RequestHead head;
  head.method = std::string(parts[0]);

  head.target = std::string(parts[1]);
  head.version = std::string(parts[2]);
  auto header_status = ParseHeaderLines(lines, 1, &head.headers);
  if (!header_status.ok()) {
    return header_status;
  }
  return std::move(head);
}

absl::StatusOr<Http1ResponseHead> ParseResponseHead(
    std::string_view head_block) {
  auto parsed_lines = SplitLines(head_block);
  if (!parsed_lines.ok()) {
    return parsed_lines.status();
  }
  const auto& lines = *parsed_lines;
  const std::vector<std::string_view> parts = Split(lines[0], ' ', 2);
  if (parts.size() < 2) {
    return absl::InvalidArgumentError(
        Cat("Malformed HTTP/1.1 status line: ", lines[0]));
  }
  if (parts[0] != "HTTP/1.1" && parts[0] != "HTTP/1.0") {
    return absl::InvalidArgumentError(
        Cat("Unsupported HTTP version: ", parts[0]));
  }
  int status = 0;
  if (!Decimal(parts[1], &status) || status < 100 || status > 599) {
    return absl::InvalidArgumentError(
        Cat("Invalid HTTP/1.1 status code: ", parts[1]));
  }
  Http1ResponseHead head;
  head.version = std::string(parts[0]);
  head.status = status;
  head.reason = parts.size() == 3 ? std::string(parts[2]) : std::string();
  auto header_status = ParseHeaderLines(lines, 1, &head.headers);
  if (!header_status.ok()) {
    return header_status;
  }
  return std::move(head);
}

namespace {

absl::StatusOr<BodyPlan> PlanBody(const Headers& headers,
                                  bool allow_until_close) {
  auto valid = ValidateHeaders(headers, Limits{});
  if (!valid.ok()) {
    return valid;
  }
  std::optional<std::size_t> length;
  bool transfer = false;
  for (const auto& [name, value] : headers) {
    if (Equal(name, "transfer-encoding")) {
      if (transfer || !Equal(Trim(value), "chunked")) {
        return absl::InvalidArgumentError(
            "Unsupported or repeated Transfer-Encoding");
      }
      transfer = true;
    }
    if (Equal(name, "content-length")) {
      std::size_t parsed = 0;
      if (length || !Decimal(Trim(value), &parsed)) {
        return absl::InvalidArgumentError("Invalid or repeated Content-Length");
      }
      length = parsed;
    }
  }
  if (transfer && length) {
    return absl::InvalidArgumentError("Ambiguous HTTP body framing");
  }
  if (transfer) {
    return BodyPlan{.framing = BodyFraming::kChunked};
  }
  if (length) {
    return BodyPlan{.framing = BodyFraming::kContentLength,
                    .content_length = *length};
  }
  if (allow_until_close) {
    return BodyPlan{.framing = BodyFraming::kUntilClose};
  }
  return BodyPlan{.framing = BodyFraming::kNone};
}

}  // namespace

absl::StatusOr<BodyPlan> PlanRequestBody(const Headers& headers) {
  // A request without framing headers has no body (never reads until close).
  return PlanBody(headers, /*allow_until_close=*/false);
}

absl::StatusOr<BodyPlan> PlanResponseBody(std::string_view request_method,
                                          int status, const Headers& headers) {
  std::string method(request_method);
  Upper(&method);
  if (method == "HEAD" || (status >= 100 && status < 200) || status == 204 ||
      status == 304) {
    return BodyPlan{.framing = BodyFraming::kNone};
  }
  return PlanBody(headers, /*allow_until_close=*/true);
}

absl::Status ChunkedDecoder::Feed(std::string_view data,
                                  std::string* absl_nonnull out,
                                  bool* absl_nullable complete) {
  size_t offset = 0;
  while (offset < data.size() && state_ != State::kComplete) {
    switch (state_) {
      case State::kSize:
      case State::kTrailer: {
        const size_t newline = data.find('\n', offset);
        const bool have_line = newline != std::string_view::npos;
        const std::string_view slice = data.substr(
            offset, (have_line ? newline + 1 : data.size()) - offset);
        if (pending_.size() + slice.size() > 16384 ||
            metadata_bytes_ + slice.size() > 65536) {
          return absl::ResourceExhaustedError("Chunk metadata exceeds limit");
        }
        metadata_bytes_ += slice.size();
        pending_.append(slice);
        offset += slice.size();
        if (!have_line) {
          return absl::OkStatus();  // Wait for the rest of the line.
        }
        std::string_view line = pending_;
        if (!line.empty() && line.back() == '\n') {
          line.remove_suffix(1);
        }
        if (!line.empty() && line.back() == '\r') {
          line.remove_suffix(1);
        }
        if (state_ == State::kSize) {
          // Strip any chunk extensions after ';'.
          const size_t semicolon = line.find(';');
          if (semicolon != std::string_view::npos) {
            line = line.substr(0, semicolon);
          }
          line = Trim(line);
          std::uint64_t size = 0;
          if (line.empty() || !HexNumber(std::string(line), &size)) {
            return absl::InvalidArgumentError(
                Cat("Invalid chunk size: ", line));
          }
          pending_.clear();
          metadata_bytes_ = 0;
          if (size == 0) {
            state_ = State::kTrailer;  // Zero chunk: read optional trailers.
          } else {
            remaining_ = size;
            state_ = State::kData;
          }
        } else {  // kTrailer
          const bool blank = line.empty();
          if (blank) {
            state_ = State::kComplete;
          } else if (const size_t colon = line.find(':');
                     colon != std::string_view::npos) {
            std::string name(line.substr(0, colon));
            Lower(&name);
            trailers_.emplace_back(std::move(name),
                                   std::string(Trim(line.substr(colon + 1))));
          }
          if (!blank && line.find(':') == std::string_view::npos) {
            return absl::InvalidArgumentError("Malformed HTTP trailer");
          }
          pending_.clear();
          auto valid = ValidateHeaders(trailers_, Limits{});
          if (!valid.ok()) {
            return valid;
          }
        }
        break;
      }
      case State::kData: {
        const size_t take = std::min<size_t>(remaining_, data.size() - offset);
        out->append(data.substr(offset, take));
        offset += take;
        remaining_ -= take;
        if (remaining_ == 0) {
          state_ = State::kDataCrlf;
        }
        break;
      }
      case State::kDataCrlf: {
        // Consume the CRLF that terminates a chunk's data.
        const char character = data[offset];
        pending_.push_back(character);
        ++offset;
        if (character == '\n') {
          if (pending_ != "\r\n") {
            return absl::InvalidArgumentError("Missing CRLF after chunk data");
          }
          pending_.clear();
          state_ = State::kSize;
        } else if (pending_.size() > 2) {
          return absl::InvalidArgumentError("Missing CRLF after chunk data");
        }
        break;
      }
      case State::kComplete:
        break;
    }
  }
  if (complete != nullptr) {
    *complete = state_ == State::kComplete;
  }
  return absl::OkStatus();
}

std::string EncodeChunk(std::string_view data) {
  if (data.empty()) {
    return {};
  }
  return Cat(Hex(data.size()), kCrlf, data, kCrlf);
}

std::string EncodeLastChunk(const Headers& trailers) {
  if (trailers.empty()) {
    return "0\r\n\r\n";
  }
  std::string out = "0\r\n";
  AppendHeaderBlock(trailers, &out);
  out.append(kCrlf);
  return out;
}

void AppendHeaderBlock(const Headers& headers, std::string* absl_nonnull out) {
  for (const auto& [name, value] : headers) {
    out->append(Cat(name, ": ", value, kCrlf));
  }
}

std::string SerializeRequest(std::string_view method, std::string_view target,
                             const Headers& headers) {
  std::string out = Cat(method, " ", target, " HTTP/1.1", kCrlf);
  AppendHeaderBlock(headers, &out);
  out.append(kCrlf);
  return out;
}

std::string SerializeResponse(int status, const Headers& headers,
                              std::string_view reason) {
  const std::string_view phrase =
      reason.empty() ? DefaultReasonPhrase(status) : reason;
  std::string out = Cat("HTTP/1.1 ", status, " ", phrase, kCrlf);
  AppendHeaderBlock(headers, &out);
  out.append(kCrlf);
  return out;
}

std::string_view DefaultReasonPhrase(int status) {
  switch (status) {
    case 101:
      return "Switching Protocols";
    case 200:
      return "OK";
    case 204:
      return "No Content";
    case 400:
      return "Bad Request";
    case 404:
      return "Not Found";
    case 426:
      return "Upgrade Required";
    case 500:
      return "Internal Server Error";
    default:
      return "";
  }
}

}  // namespace symbian::http::internal
