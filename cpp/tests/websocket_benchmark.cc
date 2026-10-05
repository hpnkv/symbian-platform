// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.
// Host codec round-trip benchmark; excludes socket I/O and guest execution.
#include <array>
#include <chrono>
#include <iostream>
#include <string>

#include <openssl/rand.h>

#include "symbian/websocket/websocket.h"

namespace {
absl::Status Relay(symbian::websocket::WebSocket& from,
                   symbian::websocket::WebSocket& to) {
  auto output = from.TakeOutput();
  return output.ok() ? to.Feed(*output) : output.status();
}
}  // namespace

int main() {
  using symbian::websocket::Role;
  using symbian::websocket::WebSocket;
  symbian::websocket::Options options;
  options.mask_provider = []() -> absl::StatusOr<std::array<std::uint8_t, 4>> {
    std::array<std::uint8_t, 4> mask{};
    if (RAND_bytes(mask.data(), static_cast<int>(mask.size())) != 1) {
      return absl::UnavailableError("Mask entropy unavailable");
    }
    return mask;
  };
  auto client = WebSocket::Create(Role::kClient, options);
  auto server = WebSocket::Create(Role::kServer);
  if (!client.ok() || !server.ok()) {
    return 1;
  }
  for (int i = 0; i < 4; ++i) {
    if (!Relay(**client, **server).ok() || !Relay(**server, **client).ok()) {
      return 1;
    }
  }
  constexpr int kIterations = 100000;
  const std::string payload(4096, 'x');
  const auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < kIterations; ++i) {
    if (!(*client)->Send(payload).ok() || !Relay(**client, **server).ok()) {
      return 1;
    }
    auto message = (*server)->Receive();
    if (!message.ok() || !message->has_value() || **message != payload) {
      return 1;
    }
    if (!(*server)->Send(**message).ok() || !Relay(**server, **client).ok()) {
      return 1;
    }
    message = (*client)->Receive();
    if (!message.ok() || !message->has_value() || **message != payload) {
      return 1;
    }
  }
  const double seconds =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - start)
          .count();
  std::cout << "iterations=" << kIterations << " bytes_per_message=4096 "
            << "round_trip_us=" << seconds * 1e6 / kIterations << " "
            << "payload_mib_per_second="
            << kIterations * payload.size() * 2.0 / seconds / 1048576 << '\n';
}
