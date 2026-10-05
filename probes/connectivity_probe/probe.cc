// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include <array>
#include <cstdint>

#include "symbian/api/connectivity/tcp_client.h"

int main() {
  using symbian::api::connectivity::TcpClient;
  auto client = TcpClient::ConnectIpv4({127, 0, 0, 1}, 39094);
  if (!client.ok()) {
    return -201;
  }
  const std::array<std::uint8_t, 1> request{'N'};
  if (!client->Send(request).ok()) {
    return -202;
  }
  std::array<std::uint8_t, 1> response{};
  const auto received = client->Receive(response);
  if (!received.ok() || *received != 1 || response[0] != 'R') {
    return -203;
  }
  return 0;
}
