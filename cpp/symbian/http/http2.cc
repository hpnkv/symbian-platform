// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "symbian/http/http2.h"

#include <algorithm>
#include <cstring>
#include <deque>

#include <absl/base/nullability.h>
#include <nghttp2/nghttp2.h>

namespace symbian::http {
namespace {
absl::Status Error(int code) {
  return code < 0 ? absl::UnavailableError(nghttp2_strerror(code))
                  : absl::OkStatus();
}

std::vector<nghttp2_nv> Fields(Headers* absl_nonnull headers) {
  std::vector<nghttp2_nv> fields;
  for (auto& [name, value] : (*headers)) {
    fields.push_back({reinterpret_cast<std::uint8_t*>(name.data()),
                      reinterpret_cast<std::uint8_t*>(value.data()),
                      name.size(), value.size(), NGHTTP2_NV_FLAG_NONE});
  }
  return fields;
}
}  // namespace

struct Http2::State {
  Role role;
  Limits limits;
  nghttp2_session* absl_nullable session = nullptr;
  std::int32_t stream = 0;
  RequestHead request;
  ResponseHead response;
  Headers block, trailers;
  std::size_t header_bytes = 0, received_bytes = 0, queued_bytes = 0;
  std::string tx;
  std::size_t offset = 0;
  std::string rx;
  bool head_received = false, end = false, finished = false, sent = false;
  bool duplex = false, settings = false;
  absl::Status error;

  ~State() { nghttp2_session_del(session); }

  int Fail(absl::Status status) {
    if (error.ok()) {
      error = std::move(status);
    }
    return NGHTTP2_ERR_CALLBACK_FAILURE;
  }

  static ssize_t ReadData(nghttp2_session* absl_nonnull, std::int32_t,
                          std::uint8_t* absl_nonnull output,
                          std::size_t capacity,
                          std::uint32_t* absl_nonnull flags,
                          nghttp2_data_source* absl_nonnull source,
                          void* absl_nonnull) {
    auto& s = *static_cast<State*>(source->ptr);
    const auto count = std::min(capacity, s.tx.size() - s.offset);
    if (count == 0 && !s.finished) {
      return NGHTTP2_ERR_DEFERRED;
    }
    std::memcpy(output, s.tx.data() + s.offset, count);
    s.offset += count;
    if (s.offset == s.tx.size()) {
      s.tx.clear();
      s.offset = 0;
      if (s.finished) {
        *flags |= NGHTTP2_DATA_FLAG_EOF;
      }
    }
    return static_cast<ssize_t>(count);
  }

  nghttp2_data_provider Provider() {
    nghttp2_data_provider result{};
    result.source.ptr = this;
    result.read_callback = ReadData;
    return result;
  }

  static int Begin(nghttp2_session* absl_nonnull,
                   const nghttp2_frame* absl_nonnull frame,
                   void* absl_nonnull user) {
    auto& s = *static_cast<State*>(user);
    if (s.role == Role::kServer && s.stream == 0) {
      s.stream = frame->hd.stream_id;
    }
    if (s.stream != frame->hd.stream_id || s.end) {
      return s.Fail(absl::InvalidArgumentError("Unexpected HTTP/2 stream"));
    }
    s.block.clear();
    s.header_bytes = 0;
    return 0;
  }

  static int Header(nghttp2_session* absl_nonnull,
                    const nghttp2_frame* absl_nonnull,
                    const std::uint8_t* absl_nonnull name, std::size_t nl,
                    const std::uint8_t* absl_nonnull value, std::size_t vl,
                    std::uint8_t, void* absl_nonnull user) {
    auto& s = *static_cast<State*>(user);
    s.header_bytes += nl + vl;
    if (s.block.size() >= s.limits.maximum_headers ||
        s.header_bytes > s.limits.maximum_header_bytes) {
      return s.Fail(
          absl::ResourceExhaustedError("HTTP/2 headers exceed bounds"));
    }
    s.block.emplace_back(std::string(reinterpret_cast<const char*>(name), nl),
                         std::string(reinterpret_cast<const char*>(value), vl));
    return 0;
  }

  static int Frame(nghttp2_session* absl_nonnull,
                   const nghttp2_frame* absl_nonnull frame,
                   void* absl_nonnull user) {
    auto& s = *static_cast<State*>(user);
    if (frame->hd.type == NGHTTP2_SETTINGS) {
      s.settings = true;
    }
    if (frame->hd.type == NGHTTP2_RST_STREAM ||
        (frame->hd.type == NGHTTP2_GOAWAY && !s.end)) {
      return s.Fail(absl::UnavailableError("HTTP/2 peer terminated stream"));
    }
    if (frame->hd.type == NGHTTP2_HEADERS) {
      if (s.head_received) {
        if (!(frame->hd.flags & NGHTTP2_FLAG_END_STREAM)) {
          return s.Fail(
              absl::InvalidArgumentError("HTTP/2 trailers without END_STREAM"));
        }
        s.trailers = std::move(s.block);
        auto status = ValidateHeaders(s.trailers, s.limits);
        if (!status.ok()) {
          return s.Fail(status);
        }
      } else {
        for (const auto& [name, value] : s.block) {
          if (name == ":method") {
            s.request.method = value;
          } else if (name == ":scheme") {
            s.request.scheme = value;
          } else if (name == ":authority") {
            s.request.authority = value;
          } else if (name == ":path") {
            s.request.path = value;
          } else if (name == ":protocol") {
            s.request.protocol = value;
            s.duplex = true;
          } else if (name == ":status") {
            if (value.size() != 3 || value[0] < '1' || value[0] > '5' ||
                value[1] < '0' || value[1] > '9' || value[2] < '0' ||
                value[2] > '9') {
              return s.Fail(
                  absl::InvalidArgumentError("Invalid HTTP/2 status"));
            }
            s.response.status =
                (value[0] - '0') * 100 + (value[1] - '0') * 10 + value[2] - '0';
          } else if (!name.empty() && name[0] == ':') {
            return s.Fail(
                absl::InvalidArgumentError("Unexpected pseudo-header"));
          } else {
            (s.role == Role::kServer ? s.request.headers : s.response.headers)
                .emplace_back(name, value);
          }
        }
        if (s.role == Role::kClient && s.response.status < 200) {
          if (s.response.status == 101) {
            return s.Fail(
                absl::InvalidArgumentError("HTTP/2 upgrade response"));
          }
          s.response.headers.clear();
        } else {
          s.head_received = true;
          auto status = s.role == Role::kServer
                            ? ValidateRequest(s.request, s.limits)
                            : ValidateHeaders(s.response.headers, s.limits);
          if (!status.ok()) {
            return s.Fail(status);
          }
        }
      }
    }
    if ((frame->hd.type == NGHTTP2_DATA || frame->hd.type == NGHTTP2_HEADERS) &&
        (frame->hd.flags & NGHTTP2_FLAG_END_STREAM)) {
      s.end = true;
    }
    return 0;
  }

  static int InvalidFrame(nghttp2_session* absl_nonnull,
                          const nghttp2_frame* absl_nonnull, int code,
                          void* absl_nonnull user) {
    auto& s = *static_cast<State*>(user);
    return s.Fail(absl::DataLossError(nghttp2_strerror(code)));
  }

  static int StreamClosed(nghttp2_session* absl_nonnull, std::int32_t id,
                          std::uint32_t code, void* absl_nonnull user) {
    auto& s = *static_cast<State*>(user);
    if (id == s.stream && (code != NGHTTP2_NO_ERROR || !s.end)) {
      return s.Fail(absl::DataLossError(
          "HTTP/2 stream closed without a complete message"));
    }
    return 0;
  }

  static int Data(nghttp2_session* absl_nonnull, std::uint8_t, std::int32_t id,
                  const std::uint8_t* absl_nonnull bytes, std::size_t size,
                  void* absl_nonnull user) {
    auto& s = *static_cast<State*>(user);
    if (!s.head_received || id != s.stream ||
        size > s.limits.maximum_buffered_bytes - s.queued_bytes ||
        (!s.duplex && size > s.limits.maximum_body_bytes - s.received_bytes)) {
      return s.Fail(absl::ResourceExhaustedError("HTTP/2 DATA exceeds bounds"));
    }
    s.received_bytes += size;
    s.queued_bytes += size;
    if (size) {
      s.rx.append(reinterpret_cast<const char*>(bytes), size);
    }
    return 0;
  }
};

Http2::Http2(std::unique_ptr<State> state) : state_(std::move(state)) {}

Http2::~Http2() = default;

absl::StatusOr<std::unique_ptr<Http2>> Http2::Create(Role role, Limits limits) {
  if (limits.maximum_buffered_bytes < 16384 ||
      limits.maximum_buffered_bytes > 1048576 ||
      limits.maximum_header_bytes == 0 || limits.maximum_header_bytes > 65536 ||
      limits.maximum_headers == 0) {
    return absl::InvalidArgumentError("Invalid HTTP/2 limits");
  }
  auto s = std::make_unique<State>();
  s->role = role;
  s->limits = limits;
  nghttp2_session_callbacks* absl_nullable cb = nullptr;
  int code = nghttp2_session_callbacks_new(&cb);
  if (code) {
    return Error(code);
  }
  nghttp2_session_callbacks_set_on_begin_headers_callback(cb, State::Begin);
  nghttp2_session_callbacks_set_on_header_callback(cb, State::Header);
  nghttp2_session_callbacks_set_on_frame_recv_callback(cb, State::Frame);
  nghttp2_session_callbacks_set_on_data_chunk_recv_callback(cb, State::Data);
  nghttp2_session_callbacks_set_on_invalid_frame_recv_callback(
      cb, State::InvalidFrame);
  nghttp2_session_callbacks_set_on_stream_close_callback(cb,
                                                         State::StreamClosed);
  nghttp2_option* absl_nullable options = nullptr;
  code = nghttp2_option_new(&options);
  if (code) {
    nghttp2_session_callbacks_del(cb);
    return Error(code);
  }
  nghttp2_option_set_no_auto_window_update(options, 1);
  nghttp2_option_set_max_reserved_remote_streams(options, 0);
  nghttp2_option_set_max_send_header_block_length(options,
                                                  limits.maximum_header_bytes);
  nghttp2_option_set_max_deflate_dynamic_table_size(options, 0);
  nghttp2_option_set_no_closed_streams(options, 1);
  nghttp2_option_set_max_outbound_ack(options, 16);
  nghttp2_option_set_max_settings(options, 16);
  nghttp2_option_set_max_continuations(options, 4);
  code = role == Role::kClient
             ? nghttp2_session_client_new2(&s->session, cb, s.get(), options)
             : nghttp2_session_server_new2(&s->session, cb, s.get(), options);
  nghttp2_session_callbacks_del(cb);
  nghttp2_option_del(options);
  if (code) {
    return Error(code);
  }
  nghttp2_settings_entry settings[] = {
      {NGHTTP2_SETTINGS_ENABLE_CONNECT_PROTOCOL, 1},
      {NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS, 1},
      {NGHTTP2_SETTINGS_MAX_HEADER_LIST_SIZE,
       static_cast<std::uint32_t>(limits.maximum_header_bytes)},
      {NGHTTP2_SETTINGS_HEADER_TABLE_SIZE, 0},
      {NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE,
       static_cast<std::uint32_t>(
           std::min<std::size_t>(65535, limits.maximum_buffered_bytes))},
      {NGHTTP2_SETTINGS_ENABLE_PUSH, 0}};
  code = nghttp2_submit_settings(s->session, 0, settings,
                                 role == Role::kClient ? 6 : 5);
  if (code) {
    return Error(code);
  }
  return std::unique_ptr<Http2>(new Http2(std::move(s)));
}

absl::Status Http2::Feed(std::string_view bytes) {
  auto& s = *state_;
  if (!s.error.ok()) {
    return s.error;
  }
  auto count = nghttp2_session_mem_recv(
      s.session, reinterpret_cast<const std::uint8_t*>(bytes.data()),
      bytes.size());
  if (s.error.ok() &&
      (count < 0 || static_cast<std::size_t>(count) != bytes.size())) {
    s.Fail(count < 0 ? Error(static_cast<int>(count))
                     : absl::DataLossError("Incomplete HTTP/2 input"));
  }
  return s.error;
}

absl::StatusOr<std::string> Http2::TakeOutput() {
  auto& s = *state_;
  if (!s.error.ok()) {
    return s.error;
  }
  std::string result;
  const std::uint8_t* absl_nonnull bytes;
  while (true) {
    auto count = nghttp2_session_mem_send(s.session, &bytes);
    if (count < 0) {
      s.Fail(Error(static_cast<int>(count)));
      return s.error;
    }
    if (!count) {
      break;
    }
    if (result.size() + static_cast<std::size_t>(count) >
        s.limits.maximum_buffered_bytes + s.limits.maximum_header_bytes +
            16384) {
      s.Fail(absl::ResourceExhaustedError("HTTP/2 output exceeds bounds"));
      return s.error;
    }
    result.append(reinterpret_cast<const char*>(bytes),
                  static_cast<std::size_t>(count));
  }
  return result;
}

absl::Status Http2::SendRequest(RequestHead head) {
  auto& s = *state_;
  if (!s.error.ok()) {
    return s.error;
  }
  if (s.role != Role::kClient || s.sent) {
    return absl::FailedPreconditionError("HTTP/2 request already sent");
  }
  auto status = ValidateRequest(head, s.limits);
  if (!status.ok()) {
    return status;
  }
  if (!head.protocol.empty() && !peer_connect_enabled()) {
    return absl::FailedPreconditionError("Peer lacks RFC 8441 support");
  }
  NormalizeHeaders(&head.headers);
  Headers headers{{":method", head.method},
                  {":scheme", head.scheme},
                  {":authority", head.authority},
                  {":path", head.path}};
  if (!head.protocol.empty()) {
    headers.emplace_back(":protocol", head.protocol);
  }
  headers.insert(headers.end(), head.headers.begin(), head.headers.end());
  auto fields = Fields(&headers);
  auto provider = s.Provider();
  int id = nghttp2_submit_request(s.session, nullptr, fields.data(),
                                  fields.size(), &provider, nullptr);
  if (id < 0) {
    return Error(id);
  }
  s.stream = id;
  s.sent = true;
  s.duplex = !head.protocol.empty();
  s.request = std::move(head);
  return absl::OkStatus();
}

absl::Status Http2::SendHeaders(ResponseHead head) {
  auto& s = *state_;
  if (!s.error.ok()) {
    return s.error;
  }
  if (s.role != Role::kServer || !s.head_received || s.sent) {
    return absl::FailedPreconditionError("HTTP/2 response out of order");
  }
  if (head.status < 200 || head.status > 599) {
    return absl::InvalidArgumentError("Invalid HTTP status");
  }
  auto status = ValidateHeaders(head.headers, s.limits);
  if (!status.ok()) {
    return status;
  }
  NormalizeHeaders(&head.headers);
  Headers headers{{":status", std::to_string(head.status)}};
  headers.insert(headers.end(), head.headers.begin(), head.headers.end());
  auto fields = Fields(&headers);
  auto provider = s.Provider();
  int code = nghttp2_submit_response(s.session, s.stream, fields.data(),
                                     fields.size(), &provider);
  if (code) {
    return Error(code);
  }
  s.sent = true;
  s.response = std::move(head);
  return absl::OkStatus();
}

absl::Status Http2::Write(std::string_view bytes) {
  auto& s = *state_;
  if (!s.error.ok()) {
    return s.error;
  }
  if (!s.sent || s.finished) {
    return absl::FailedPreconditionError(
        "HTTP/2 write after finish or before headers");
  }
  if (bytes.size() > s.limits.maximum_buffered_bytes - buffered_amount()) {
    return absl::ResourceExhaustedError("HTTP/2 send queue full");
  }
  if (s.offset) {
    s.tx.erase(0, s.offset);
    s.offset = 0;
  }
  s.tx.append(bytes);
  int code = nghttp2_session_resume_data(s.session, s.stream);
  return code == NGHTTP2_ERR_INVALID_ARGUMENT ? absl::OkStatus() : Error(code);
}

absl::Status Http2::Finish() {
  auto& s = *state_;
  if (!s.error.ok()) {
    return s.error;
  }
  if (!s.sent) {
    return absl::FailedPreconditionError("HTTP/2 finish before headers");
  }
  s.finished = true;
  int code = nghttp2_session_resume_data(s.session, s.stream);
  return code == NGHTTP2_ERR_INVALID_ARGUMENT ? absl::OkStatus() : Error(code);
}

absl::StatusOr<std::optional<std::string>> Http2::Read() {
  auto& s = *state_;
  if (!s.error.ok()) {
    return s.error;
  }
  if (s.rx.empty()) {
    return std::optional<std::string>();
  }
  std::string result;
  result.swap(s.rx);
  s.queued_bytes -= result.size();
  auto status =
      Error(nghttp2_session_consume(s.session, s.stream, result.size()));
  if (!status.ok()) {
    return status;
  }
  return std::optional<std::string>(std::move(result));
}

bool Http2::headers_received() const {
  return state_->head_received;
}

bool Http2::ended() const {
  return state_->end;
}

bool Http2::peer_settings_received() const {
  return state_->settings;
}

bool Http2::peer_connect_enabled() const {
  return state_->settings &&
         nghttp2_session_get_remote_settings(
             state_->session, NGHTTP2_SETTINGS_ENABLE_CONNECT_PROTOCOL) == 1;
}

const RequestHead& Http2::request() const {
  return state_->request;
}

const ResponseHead& Http2::response() const {
  return state_->response;
}

const Headers& Http2::trailers() const {
  return state_->trailers;
}

std::size_t Http2::buffered_amount() const {
  return state_->tx.size() - state_->offset;
}

void Http2::Abort() {
  state_->Fail(absl::CancelledError("HTTP/2 aborted"));
}
}  // namespace symbian::http
