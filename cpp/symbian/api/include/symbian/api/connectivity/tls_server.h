// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_API_CONNECTIVITY_TLS_SERVER_H_
#define SYMBIAN_API_CONNECTIVITY_TLS_SERVER_H_

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "symbian/api/connectivity/tcp_client.h"

namespace symbian::api::connectivity {

/** @brief One protocol version supported by the vendored Mbed TLS server. */
enum class TlsVersion { kTls12, kTls13 };

/**
 * @brief One configured, mutually authenticated TLS server and current stream.
 *
 * Create parses caller-supplied PEM identities and CA roots; the SDK has no
 * default trust roots or key. A caller must provide a hardware entropy adapter
 * accepted by the Mbed TLS port. Handshake requires a verified client
 * certificate. ReadFor and WriteFor keep each native socket request bounded
 * and cancel/drain it on deadline. Call only on a worker thread owning the
 * TcpClient, never from an active scheduler callback.
 *
 * This owner handles one stream at a time. CloseSession releases it without
 * sending a potentially blocking TLS close alert, after which Accept may
 * establish another. The original Mbed TLS C API remains available for more
 * advanced TLS policy and asynchronous transports.
 */
class TlsServer {
 public:
  /**
   * @brief Configure identity, trust roots and one TLS protocol version.
   *
   * The vendored Mbed TLS server cannot negotiate a hybrid 1.2/1.3 profile.
   * Select the version explicitly; both are authenticated by the same policy.
   */
  static absl::StatusOr<TlsServer> Create(std::string_view server_certificate,
                                          std::string_view server_private_key,
                                          std::string_view client_ca_bundle,
                                          TlsVersion version);

  TlsServer(TlsServer&& other) noexcept;
  TlsServer& operator=(TlsServer&& other) noexcept;
  TlsServer(const TlsServer&) = delete;
  TlsServer& operator=(const TlsServer&) = delete;
  ~TlsServer();

  /** @brief Complete TLS 1.2/1.3 handshake with an aggregate 0–60s deadline. */
  absl::Status Accept(TcpClient&& client, std::chrono::milliseconds timeout);

  /** @brief Receive application data into at most 32 KiB. */
  absl::StatusOr<std::size_t> ReadFor(std::span<std::uint8_t> bytes,
                                      std::chrono::milliseconds timeout);

  /** @brief Send at most 32 KiB of application data. */
  absl::Status WriteFor(std::span<const std::uint8_t> bytes,
                        std::chrono::milliseconds timeout);

  /** @brief Close the current stream and reset its TLS state. */
  void CloseSession();

  bool connected() const;

 private:
  struct Impl;

  explicit TlsServer(Impl* impl) : impl_(impl) {}

  Impl* impl_ = nullptr;
};

}  // namespace symbian::api::connectivity

#endif  // SYMBIAN_API_CONNECTIVITY_TLS_SERVER_H_
