// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.
#include "symbian/websocket/websocket.h"

#include <deque>

#include "symbian/http/http2.h"
#include "symbian/websocket/framing.h"

namespace symbian::websocket {
struct WebSocket::State {
  Role role;
  Options options;
  std::unique_ptr<http::Http2> http;
  bool opened = false, closed = false, closing = false, submitted = false;
  std::size_t queued_bytes = 0;
  internal::Framing framing;
  std::deque<std::string> messages;
  absl::Status error;

  State(Role r, Options o)
      : role(r),
        options(std::move(o)),
        framing(r, options.maximum_message_bytes) {}

  int Fail(absl::Status status) {
    if (error.ok()) {
      error = std::move(status);
    }
    closed = true;
    return -1;
  }

  absl::Status Frame(std::uint8_t opcode, std::string_view payload,
                     std::array<std::uint8_t, 4> mask) {
    if (role == Role::kClient) {
      auto random = options.mask_provider();
      if (!random.ok()) {
        return random.status();
      }
      mask = *random;
    }
    if (const std::size_t overhead = role == Role::kClient ? 8 : 4;
        payload.size() + overhead >
        options.maximum_buffered_bytes - http->buffered_amount()) {
      return absl::ResourceExhaustedError("WebSocket send queue full");
    }
    const std::uint32_t key = (static_cast<std::uint32_t>(mask[0]) << 24) |
                              (static_cast<std::uint32_t>(mask[1]) << 16) |
                              (static_cast<std::uint32_t>(mask[2]) << 8) |
                              mask[3];
    return http->Write(framing.WriteFrame(opcode, payload, key));
  }

  int Parse() {
    internal::Framing::ParsedActions actions;
    auto status = framing.ParseFrames(&actions);
    if (!status.ok()) {
      return Fail(status);
    }
    for (auto& pong : actions.pongs) {
      status = Frame(10, pong, {});
      if (!status.ok()) {
        return Fail(status);
      }
    }
    for (auto& message : actions.messages) {
      if (closed || messages.size() >= 16 ||
          queued_bytes + message.size() > options.maximum_buffered_bytes) {
        return Fail(
            absl::ResourceExhaustedError("WebSocket receive queue full"));
      }
      queued_bytes += message.size();
      messages.push_back(std::move(message));
    }
    if (actions.close) {
      if (!closing) {
        status = Frame(8, *actions.close, {});
        if (!status.ok()) {
          return Fail(status);
        }
        closing = true;
      }
      closed = true;
    }
    return 0;
  }

  absl::Status Advance() {
    if (!error.ok()) {
      return error;
    }
    if (role == Role::kClient && !submitted && http->peer_settings_received() &&
        !http->peer_connect_enabled()) {
      Fail(absl::FailedPreconditionError("Peer lacks RFC 8441 support"));
      return error;
    }
    if (role == Role::kClient && !submitted && http->peer_connect_enabled()) {
      http::RequestHead request;
      request.method = "CONNECT";
      request.protocol = "websocket";
      request.authority = options.authority;
      request.path = options.path;
      request.headers = {{"sec-websocket-version", "13"}};
      if (auto status = http->SendRequest(std::move(request)); !status.ok()) {
        Fail(status);
        return error;
      }
      submitted = true;
    }
    if (!opened && http->headers_received()) {
      if (http->ended()) {
        Fail(absl::InvalidArgumentError("Closed WebSocket handshake"));
        return error;
      }
      if (role == Role::kServer) {
        const auto& r = http->request();
        if (r.method != "CONNECT" || r.protocol != "websocket" ||
            r.scheme != "http" || r.path != options.path ||
            http::GetHeader(r.headers, "sec-websocket-version") != "13") {
          Fail(absl::InvalidArgumentError("Invalid WebSocket CONNECT"));
          return error;
        }
        if (auto status = http->SendHeaders({.status = 200, .headers = {}});
            !status.ok()) {
          Fail(status);
          return error;
        }
      } else if (http->response().status != 200) {
        Fail(absl::FailedPreconditionError("WebSocket CONNECT rejected"));
        return error;
      }
      opened = true;
    }
    while (true) {
      auto data = http->Read();
      if (!data.ok()) {
        Fail(data.status());
        return error;
      }
      if (!data->has_value()) {
        break;
      }
      if (!opened || framing.input_.size() + (**data).size() >
                         options.maximum_buffered_bytes) {
        Fail(absl::ResourceExhaustedError("Invalid WebSocket DATA"));
        return error;
      }
      framing.input_.append(**data);
      Parse();
      if (!error.ok()) {
        return error;
      }
    }
    if (closing) {
      if (auto status = http->Finish(); !status.ok()) {
        Fail(status);
      }
    }
    if (http->ended() && !closed) {
      Fail(absl::DataLossError("WebSocket EOF without close"));
    }
    return error;
  }
};

WebSocket::WebSocket(std::unique_ptr<State> state) : state_(std::move(state)) {}

WebSocket::~WebSocket() = default;

absl::StatusOr<std::unique_ptr<WebSocket>> WebSocket::Create(Role role,
                                                             Options options) {
  if ((role == Role::kClient && !options.mask_provider) ||
      options.path.empty() || options.path[0] != '/' ||
      options.path.size() > 512 || options.authority.empty() ||
      options.authority.size() > 512 || options.maximum_message_bytes == 0 ||
      options.maximum_message_bytes > 32768 ||
      options.maximum_buffered_bytes < options.maximum_message_bytes + 16384 ||
      options.maximum_buffered_bytes > 1048576) {
    return absl::InvalidArgumentError("Invalid WebSocket options");
  }
  auto state = std::make_unique<State>(role, std::move(options));
  http::Limits limits;
  limits.maximum_headers = 16;
  limits.maximum_header_bytes = 2048;
  limits.maximum_buffered_bytes = state->options.maximum_buffered_bytes;
  auto http = http::Http2::Create(
      role == Role::kClient ? http::Role::kClient : http::Role::kServer,
      limits);
  if (!http.ok()) {
    return http.status();
  }
  state->http = std::move(*http);
  return std::unique_ptr<WebSocket>(new WebSocket(std::move(state)));
}

absl::Status WebSocket::Feed(std::string_view bytes) {
  if (!state_->error.ok()) {
    return state_->error;
  }
  if (auto status = state_->http->Feed(bytes); !status.ok()) {
    state_->Fail(status);
    return status;
  }
  return state_->Advance();
}

absl::StatusOr<std::string> WebSocket::TakeOutput() {
  if (!state_->error.ok()) {
    return state_->error;
  }
  return state_->http->TakeOutput();
}

absl::Status WebSocket::Send(std::string_view message) {
  auto& s = *state_;
  if (!s.error.ok()) {
    return s.error;
  }
  if (!open()) {
    return absl::FailedPreconditionError("WebSocket is not open");
  }
  if (message.size() > s.options.maximum_message_bytes) {
    return absl::ResourceExhaustedError("WebSocket message too large");
  }
  return s.Frame(2, message, {});
}

absl::StatusOr<std::optional<std::string>> WebSocket::Receive() {
  auto& s = *state_;
  if (!s.error.ok()) {
    return s.error;
  }
  if (s.messages.empty()) {
    return std::optional<std::string>();
  }
  std::string value = std::move(s.messages.front());
  s.messages.pop_front();
  s.queued_bytes -= value.size();
  return std::optional<std::string>(std::move(value));
}

absl::Status WebSocket::Close() {
  auto& s = *state_;
  if (!s.error.ok()) {
    return s.error;
  }
  if (s.closing) {
    return absl::OkStatus();
  }
  if (!s.opened) {
    return absl::FailedPreconditionError("WebSocket handshake incomplete");
  }
  auto status = s.Frame(8, {}, {});
  if (status.ok()) {
    s.closing = true;
    status = s.http->Finish();
  }
  return status;
}

void WebSocket::Abort() {
  state_->Fail(absl::CancelledError("WebSocket aborted"));
}

bool WebSocket::open() const {
  return state_->opened && !state_->closed && !state_->closing;
}

bool WebSocket::closed() const {
  return state_->closed;
}

std::size_t WebSocket::buffered_amount() const {
  return state_->http->buffered_amount();
}
}  // namespace symbian::websocket
