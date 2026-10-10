// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.
#include "symbian/api/connectivity/websocket.h"

#include <algorithm>
#include <cstring>
#include <utility>

#include <absl/status/status_macros.h>

namespace symbian::api::connectivity {
WebSocketStream::WebSocketStream(net::ByteStream transport,
                                 std::unique_ptr<websocket::WebSocket> codec)
    : transport_(std::move(transport)), codec_(std::move(codec)) {}

WebSocketStream::WebSocketStream(WebSocketStream&& other) noexcept
    : transport_(std::move(other.transport_)),
      codec_(std::move(other.codec_)),
      pending_(std::move(other.pending_)),
      offset_(std::exchange(other.offset_, 0)) {
  other.pending_.clear();
}

WebSocketStream& WebSocketStream::operator=(WebSocketStream&& other) noexcept {
  if (this != &other) {
    Abort();
    transport_ = std::move(other.transport_);
    codec_ = std::move(other.codec_);
    pending_ = std::move(other.pending_);
    offset_ = std::exchange(other.offset_, 0);
    other.pending_.clear();
  }
  return *this;
}

absl::StatusOr<WebSocketStream> WebSocketStream::Connect(
    TcpClient client, websocket::Options options, absl::Time deadline) {
  return Open(std::make_unique<TcpByteStream>(std::move(client)),
              websocket::Role::kClient, std::move(options), deadline);
}

absl::StatusOr<WebSocketStream> WebSocketStream::Accept(
    TcpClient client, websocket::Options options, absl::Time deadline) {
  return Open(std::make_unique<TcpByteStream>(std::move(client)),
              websocket::Role::kServer, std::move(options), deadline);
}

absl::StatusOr<WebSocketStream> WebSocketStream::Connect(
    net::ByteStream transport, websocket::Options options,
    absl::Time deadline) {
  return Open(std::move(transport), websocket::Role::kClient,
              std::move(options), deadline);
}

absl::StatusOr<WebSocketStream> WebSocketStream::Accept(
    net::ByteStream transport, websocket::Options options,
    absl::Time deadline) {
  return Open(std::move(transport), websocket::Role::kServer,
              std::move(options), deadline);
}

absl::StatusOr<WebSocketStream> WebSocketStream::Open(
    net::ByteStream transport, websocket::Role role, websocket::Options options,
    absl::Time deadline) {
  if (!transport) {
    return absl::InvalidArgumentError("Missing WebSocket transport");
  }
  ABSL_ASSIGN_OR_RETURN(
      auto codec, websocket::WebSocket::CreateUnique(role, std::move(options)));
  WebSocketStream stream(std::move(transport), std::move(codec));
  ABSL_RETURN_IF_ERROR(stream.Flush(deadline));
  while (!stream.codec_->open()) {
    ABSL_RETURN_IF_ERROR(stream.Pump(deadline));
  }
  return stream;
}

absl::Status WebSocketStream::Flush(absl::Time deadline) {
  ABSL_ASSIGN_OR_RETURN(auto output, codec_->TakeOutput());
  std::string_view bytes = output;
  while (!bytes.empty()) {
    const auto count = std::min<std::size_t>(bytes.size(), 32768);
    if (auto status = transport_.Write(
            std::span(reinterpret_cast<const std::uint8_t*>(bytes.data()),
                      count),
            deadline);
        !status.ok()) {
      Abort();
      return status;
    }
    bytes.remove_prefix(count);
  }
  return absl::OkStatus();
}

absl::Status WebSocketStream::Pump(absl::Time deadline) {
  std::array<std::uint8_t, 16384> input{};
  auto count = transport_.Read(input, deadline);
  if (!count.ok()) {
    Abort();
    return count.status();
  }
  if (*count == 0) {
    Abort();
    return absl::UnavailableError("WebSocket EOF");
  }
  if (auto status = codec_->Feed(std::string_view(
          reinterpret_cast<const char*>(input.data()), *count));
      !status.ok()) {
    Abort();
    return status;
  }
  return Flush(deadline);
}

absl::Status WebSocketStream::Send(std::span<const std::uint8_t> bytes,
                                   absl::Time deadline) {
  if (codec_ == nullptr) {
    return absl::FailedPreconditionError("WebSocket stream is closed");
  }
  ABSL_RETURN_IF_ERROR(codec_->Send(std::string_view(
      reinterpret_cast<const char*>(bytes.data()), bytes.size())));
  ABSL_RETURN_IF_ERROR(Flush(deadline));
  // A successful socket write may only contain the DATA permitted by the
  // peer's current HTTP/2 window. Read its WINDOW_UPDATE and send the rest
  // before accepting another message into the bounded send queue.
  while (codec_->buffered_amount() != 0) {
    ABSL_RETURN_IF_ERROR(Pump(deadline));
  }
  return absl::OkStatus();
}

absl::StatusOr<std::size_t> WebSocketStream::Receive(
    std::span<std::uint8_t> bytes, absl::Time deadline) {
  if (codec_ == nullptr) {
    return absl::FailedPreconditionError("WebSocket stream is closed");
  }
  if (bytes.empty()) {
    return std::size_t{0};
  }
  while (offset_ == pending_.size()) {
    pending_.clear();
    offset_ = 0;
    ABSL_ASSIGN_OR_RETURN(auto message, codec_->Receive());
    if (message.has_value()) {
      pending_ = std::move(*message);
      if (!pending_.empty()) {
        break;
      }
    }
    if (codec_->closed()) {
      return absl::UnavailableError("WebSocket closed");
    }
    ABSL_RETURN_IF_ERROR(Pump(deadline));
  }
  const auto count = std::min(bytes.size(), pending_.size() - offset_);
  std::memcpy(bytes.data(), pending_.data() + offset_, count);
  offset_ += count;
  return count;
}

absl::Status WebSocketStream::Close(absl::Time deadline) {
  if (codec_ == nullptr) {
    return absl::OkStatus();
  }
  ABSL_RETURN_IF_ERROR(codec_->Close());
  ABSL_RETURN_IF_ERROR(Flush(deadline));
  while (!codec_->closed()) {
    ABSL_RETURN_IF_ERROR(Pump(deadline));
  }
  transport_.Close();
  return absl::OkStatus();
}

void WebSocketStream::Abort() {
  if (codec_ != nullptr) {
    codec_->Abort();
    transport_.Close();
    codec_.reset();
  }
  pending_.clear();
  offset_ = 0;
}

absl::StatusOr<WebSocketServer> WebSocketServer::ListenIpv4(
    std::array<std::uint8_t, 4> address, std::uint16_t port,
    websocket::Options options) {
  if (const auto validation =
          websocket::WebSocket::CreateUnique(websocket::Role::kServer, options);
      !validation.ok()) {
    return validation.status();
  }
  ABSL_ASSIGN_OR_RETURN(auto listener, TcpListener::ListenIpv4(address, port));
  return WebSocketServer(std::move(listener), std::move(options));
}

absl::StatusOr<WebSocketStream> WebSocketServer::Accept(absl::Time deadline) {
  ABSL_ASSIGN_OR_RETURN(auto client, listener_.Accept(deadline));
  return WebSocketStream::Accept(std::move(client), options_, deadline);
}
}  // namespace symbian::api::connectivity
