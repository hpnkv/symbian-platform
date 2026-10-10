// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_API_CONNECTIVITY_TLS_STREAM_H_
#define SYMBIAN_API_CONNECTIVITY_TLS_STREAM_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>

#include <absl/base/nullability.h>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "symbian/api/connectivity/tcp_client.h"
#include "symbian/net/byte_stream.h"

namespace symbian::api::connectivity {

/** @brief One protocol version supported by the vendored Mbed TLS server. */
enum class TlsVersion { kTls12, kTls13 };

/**
 * @brief Worker-owned TLS transport for outbound HTTP and inbound sessions.
 *
 * Connect verifies the server certificate chain and hostname, sends SNI and
 * negotiates one selected ALPN protocol. Create/Accept configure an inbound
 * server that requires a verified client certificate. Both paths share the
 * same Mbed TLS state, entropy, native socket BIO and deadline handling.
 * The caller supplies PEM roots/identities. The SDK supplies OS secure entropy
 * where supported and fails closed otherwise; no default roots or identity.
 *
 * Read returns zero only for authenticated close_notify; a bare transport EOF
 * is a truncation error. Read/Write cancel and drain native socket requests on
 * deadline. Calls and destruction stay on the socket's owning worker, outside
 * active scheduler callbacks. CloseSession cancels by closing the socket,
 * without a blocking close alert. Inbound state may then Accept another peer.
 */
class TlsStream {
 public:
  /**
   * @brief Configure identity, trust roots and one TLS protocol version.
   *
   * The vendored Mbed TLS server cannot negotiate a hybrid 1.2/1.3 profile.
   * Select the version explicitly; both are authenticated by the same policy.
   */
  static absl::StatusOr<TlsStream> Create(std::string_view server_certificate,
                                          std::string_view server_private_key,
                                          std::string_view client_ca_bundle,
                                          TlsVersion version);
  static absl::StatusOr<std::unique_ptr<TlsStream>> CreateUnique(
      std::string_view server_certificate, std::string_view server_private_key,
      std::string_view client_ca_bundle, TlsVersion version);

  TlsStream(TlsStream&& other) noexcept;
  TlsStream& operator=(TlsStream&& other) noexcept;
  TlsStream(const TlsStream&) = delete;
  TlsStream& operator=(const TlsStream&) = delete;
  ~TlsStream();

  /** @brief Authenticate an outbound peer using explicit roots and hostname.
   * Select one protocol version and ALPN protocol. No verification bypass or
   * system trust store is provided. Uses the same native BIO as server Accept.
   */
  static absl::StatusOr<TlsStream> Connect(
      TcpClient client, std::string_view hostname, std::string_view ca_bundle,
      TlsVersion version, std::string_view alpn = "http/1.1",
      absl::Time deadline = absl::InfiniteFuture());
  /** @brief Select the sole server ALPN protocol before Accept. */
  absl::Status SetAlpnProtocol(std::string_view protocol);
  std::string_view negotiated_version() const;
  std::string_view negotiated_protocol() const;

  void Close() { CloseSession(); }

  /** @brief Complete TLS 1.2/1.3 handshake by an absolute deadline. */
  absl::Status Accept(TcpClient&& client,
                      absl::Time deadline = absl::InfiniteFuture());

  /** @brief Receive application data into at most 32 KiB. */
  absl::StatusOr<std::size_t> Read(
      std::span<std::uint8_t> bytes,
      absl::Time deadline = absl::InfiniteFuture());

  /** @brief Send at most 32 KiB of application data. */
  absl::Status Write(std::span<const std::uint8_t> bytes,
                     absl::Time deadline = absl::InfiniteFuture());

  /** @brief Close the current stream and reset its TLS state. */
  void CloseSession();

  bool connected() const;

 private:
  struct Impl;

  static absl::StatusOr<TlsStream> Initialize(bool server, TlsVersion version);
  absl::Status Handshake(absl::Time deadline);

  explicit TlsStream(Impl* absl_nonnull impl) : impl_(impl) {}

  Impl* absl_nullable impl_ = nullptr;
};

}  // namespace symbian::api::connectivity

#endif  // SYMBIAN_API_CONNECTIVITY_TLS_STREAM_H_
