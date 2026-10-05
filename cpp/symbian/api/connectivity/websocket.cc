// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.
#include "symbian/api/connectivity/websocket.h"

#include <algorithm>
#include <cstring>
#include <utility>

namespace symbian::api::connectivity {
WebSocketStream::WebSocketStream(std::unique_ptr<net::ByteStream> transport,
                                 std::unique_ptr<websocket::WebSocket> codec)
    : transport_(std::move(transport)), codec_(std::move(codec)) {}

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
    std::unique_ptr<net::ByteStream> transport, websocket::Options options,
    absl::Time deadline) {
  return Open(std::move(transport), websocket::Role::kClient,
              std::move(options), deadline);
}

absl::StatusOr<WebSocketStream> WebSocketStream::Accept(
    std::unique_ptr<net::ByteStream> transport, websocket::Options options,
    absl::Time deadline) {
  return Open(std::move(transport), websocket::Role::kServer,
              std::move(options), deadline);
}

absl::StatusOr<WebSocketStream> WebSocketStream::Open(
    std::unique_ptr<net::ByteStream> transport, websocket::Role role,
    websocket::Options options, absl::Time deadline) {
  if (!transport) {
    return absl::InvalidArgumentError("Missing WebSocket transport");
  }
  auto codec = websocket::WebSocket::Create(role, std::move(options));
  if (!codec.ok()) {
    return codec.status();
  }
  WebSocketStream stream(std::move(transport), std::move(*codec));
  auto status = stream.Flush(deadline);
  if (!status.ok()) {
    return status;
  }
  while (!stream.codec_->open()) {
    status = stream.Pump(deadline);
    if (!status.ok()) {
      return status;
    }
  }
  return stream;
}

absl::Status WebSocketStream::Flush(absl::Time deadline) {
  auto output = codec_->TakeOutput();
  if (!output.ok()) {
    return output.status();
  }
  std::string_view bytes = *output;
  while (!bytes.empty()) {
    const auto count = std::min<std::size_t>(bytes.size(), 32768);
    auto status = transport_->Write(
        std::span(reinterpret_cast<const std::uint8_t*>(bytes.data()), count),
        deadline);
    if (!status.ok()) {
      Abort();
      return status;
    }
    bytes.remove_prefix(count);
  }
  return absl::OkStatus();
}

absl::Status WebSocketStream::Pump(absl::Time deadline) {
  std::array<std::uint8_t, 16384> input{};
  auto count = transport_->Read(input, deadline);
  if (!count.ok()) {
    Abort();
    return count.status();
  }
  if (*count == 0) {
    Abort();
    return absl::UnavailableError("WebSocket EOF");
  }
  auto status = codec_->Feed(
      std::string_view(reinterpret_cast<const char*>(input.data()), *count));
  if (!status.ok()) {
    Abort();
    return status;
  }
  return Flush(deadline);
}

absl::Status WebSocketStream::Send(std::span<const std::uint8_t> bytes,
                                   absl::Time deadline) {
  auto status = codec_->Send(std::string_view(
      reinterpret_cast<const char*>(bytes.data()), bytes.size()));
  return status.ok() ? Flush(deadline) : status;
}

absl::StatusOr<std::size_t> WebSocketStream::Receive(
    std::span<std::uint8_t> bytes, absl::Time deadline) {
  if (bytes.empty()) {
    return std::size_t{0};
  }
  while (offset_ == pending_.size()) {
    pending_.clear();
    offset_ = 0;
    auto message = codec_->Receive();
    if (!message.ok()) {
      return message.status();
    }
    if (message->has_value()) {
      pending_ = std::move(**message);
      if (!pending_.empty()) {
        break;
      }
    }
    if (codec_->closed()) {
      return absl::UnavailableError("WebSocket closed");
    }
    auto status = Pump(deadline);
    if (!status.ok()) {
      return status;
    }
  }
  const auto count = std::min(bytes.size(), pending_.size() - offset_);
  std::memcpy(bytes.data(), pending_.data() + offset_, count);
  offset_ += count;
  return count;
}

absl::Status WebSocketStream::Close(absl::Time deadline) {
  auto status = codec_->Close();
  if (!status.ok()) {
    return status;
  }
  status = Flush(deadline);
  if (!status.ok()) {
    return status;
  }
  while (!codec_->closed()) {
    status = Pump(deadline);
    if (!status.ok()) {
      return status;
    }
  }
  transport_->Close();
  return absl::OkStatus();
}

void WebSocketStream::Abort() {
  codec_->Abort();
  transport_->Close();
}

absl::StatusOr<WebSocketServer> WebSocketServer::ListenIpv4(
    std::array<std::uint8_t, 4> address, std::uint16_t port,
    websocket::Options options) {
  auto validation =
      websocket::WebSocket::Create(websocket::Role::kServer, options);
  if (!validation.ok()) {
    return validation.status();
  }
  auto listener = TcpListener::ListenIpv4(address, port);
  if (!listener.ok()) {
    return listener.status();
  }
  return WebSocketServer(std::move(*listener), std::move(options));
}

absl::StatusOr<WebSocketStream> WebSocketServer::Accept(absl::Time deadline) {
  auto client = listener_.Accept(deadline);
  if (!client.ok()) {
    return client.status();
  }
  return WebSocketStream::Accept(std::move(*client), options_, deadline);
}
}  // namespace symbian::api::connectivity
