// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.
// Research-only consumer of the public Symbian::Tls owner.

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "symbian/agent/guest_control.h"
#include "symbian/api/connectivity/tcp_listener.h"
#include "symbian/api/connectivity/tls_server.h"
#include "test_certificate.h"

namespace {

using symbian::api::connectivity::TlsServer;

int ReadExactly(TlsServer* server, std::span<std::uint8_t> bytes) {
  std::size_t offset = 0;
  while (offset < bytes.size()) {
    auto count =
        server->Read(bytes.subspan(offset), absl::Now() + absl::Seconds(5));
    if (!count.ok() || *count == 0) {
      return -281;
    }
    offset += *count;
  }
  return 0;
}

int WriteExactly(TlsServer* server, std::span<const std::uint8_t> bytes) {
  return server->Write(bytes, absl::Now() + absl::Seconds(5)).ok() ? 0 : -282;
}

int ServeStatus(TlsServer* server, bool oversized) {
  std::array<std::uint8_t, 4> prefix{};
  if (ReadExactly(server, prefix) != 0) {
    return -283;
  }
  const std::uint32_t length = (static_cast<std::uint32_t>(prefix[0]) << 24) |
                               (static_cast<std::uint32_t>(prefix[1]) << 16) |
                               (static_cast<std::uint32_t>(prefix[2]) << 8) |
                               static_cast<std::uint32_t>(prefix[3]);
  if (length == 0 || length > 4096) {
    return oversized ? 0 : -284;
  }
  if (oversized) {
    return -285;
  }
  std::array<std::uint8_t, 4096> payload{};
  if (ReadExactly(server, std::span(payload).first(length)) != 0) {
    return -286;
  }
  auto request = symbian::agent::ParseGuestControl(
      std::string_view(reinterpret_cast<const char*>(payload.data()), length));
  if (!request.ok()) {
    return -287;
  }
  auto response = symbian::agent::PackGuestResult(*request);
  if (!response.ok()) {
    return -288;
  }
  const std::uint32_t count = response->size();
  const std::array<std::uint8_t, 4> response_prefix{
      static_cast<std::uint8_t>(count >> 24),
      static_cast<std::uint8_t>(count >> 16),
      static_cast<std::uint8_t>(count >> 8), static_cast<std::uint8_t>(count)};
  if (WriteExactly(server, response_prefix) != 0 ||
      WriteExactly(server, std::span(reinterpret_cast<const std::uint8_t*>(
                                         response->data()),
                                     response->size())) != 0) {
    return -289;
  }
  return 0;
}

}  // namespace

extern "C" __attribute__((visibility("default"))) int MbedOwnedTlsServerProbe(
    int port, int version, int mode) {
  if (port <= 0 || port > 65535 || (version != 12 && version != 13) ||
      mode < 0 || mode > 3) {
    return -270;
  }
  auto listener = symbian::api::connectivity::TcpListener::ListenIpv4(
      {127, 0, 0, 1}, static_cast<std::uint16_t>(port));
  if (!listener.ok()) {
    return -271;
  }
  auto client = listener->Accept(absl::Now() + absl::Seconds(10));
  if (!client.ok()) {
    return -272;
  }
  auto server = TlsServer::Create(
      kTestCertificate, kTestPrivateKey, kTestCertificate,
      version == 12 ? symbian::api::connectivity::TlsVersion::kTls12
                    : symbian::api::connectivity::TlsVersion::kTls13);
  if (!server.ok()) {
    return -273;
  }
  auto handshake =
      server->Accept(std::move(*client), absl::Now() + absl::Seconds(10));
  if (mode == 1) {
    return handshake.ok() ? -274 : 0;
  }
  if (!handshake.ok()) {
    // Preserve the underlying Mbed TLS or native socket code in the process
    // exit record while this research probe diagnoses owner failures.
    const std::string message(handshake.message());
    const std::size_t separator = message.rfind(':');
    if (separator == std::string::npos || separator + 2 >= message.size() ||
        message[separator + 1] != ' ' || message[separator + 2] != '-') {
      return -275;
    }
    int code = 0;
    for (std::size_t index = separator + 3; index < message.size(); ++index) {
      if (message[index] < '0' || message[index] > '9') {
        return -275;
      }
      code = code * 10 + message[index] - '0';
    }
    return -code;
  }
  if (mode == 2 || mode == 3) {
    return ServeStatus(&*server, mode == 3);
  }
  std::array<std::uint8_t, 1> request{};
  if (ReadExactly(&*server, request) != 0 || request[0] != 'H') {
    return -276;
  }
  const std::array<std::uint8_t, 1> response{'S'};
  return WriteExactly(&*server, response) == 0 ? 0 : -277;
}
