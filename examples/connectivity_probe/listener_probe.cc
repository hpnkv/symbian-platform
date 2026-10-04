// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include <array>
#include <cstdint>
#include <optional>
#include <utility>

#include "absl/time/time.h"
#include "symbian/api/connectivity/tcp_listener.h"

extern "C" int RuntimeMain() {
  using symbian::api::connectivity::TcpClient;
  using symbian::api::connectivity::TcpListener;
  std::optional<TcpClient> client;
  {
    auto listener = TcpListener::ListenIpv4({127, 0, 0, 1}, 39096);
    if (!listener.ok()) {
      return -220;
    }
    auto accepted = listener->Accept();
    if (!accepted.ok()) {
      return -221;
    }
    client.emplace(std::move(*accepted));
  }
  std::array<std::uint8_t, 1> request{};
  const auto count = client->Receive(request);
  if (!count.ok() || *count != 1 || request[0] != 'Q') {
    return -222;
  }
  const std::array<std::uint8_t, 1> response{'A'};
  if (!client->Send(response).ok()) {
    return -223;
  }
  auto stalled = client->ReceiveFor(request, absl::Milliseconds(50));
  if (stalled.ok() ||
      stalled.status().code() != absl::StatusCode::kDeadlineExceeded) {
    return -227;
  }
  const auto resumed = client->ReceiveFor(request, absl::Seconds(2));
  if (!resumed.ok() || *resumed != 1 || request[0] != 'R') {
    return -228;
  }
  auto idle = TcpListener::ListenIpv4({127, 0, 0, 1}, 39097);
  if (!idle.ok()) {
    return -224;
  }
  auto none = idle->AcceptFor(absl::Milliseconds(50));
  if (none.ok() ||
      none.status().code() != absl::StatusCode::kDeadlineExceeded) {
    return -225;
  }
  auto again = idle->AcceptFor(absl::Milliseconds(50));
  if (again.ok() ||
      again.status().code() != absl::StatusCode::kDeadlineExceeded) {
    return -226;
  }
  return 0;
}
