// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.
// Research-only, manually addressed mutual-TLS listener in the emulator.

#include <array>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>

#include "absl/time/time.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/pk.h"
#include "mbedtls/ssl.h"
#include "mbedtls/x509_crt.h"
#include "psa/crypto.h"
#include "symbian/agent/guest_control.h"
#include "symbian/api/connectivity/tcp_listener.h"
#include "test_certificate.h"

namespace {

using symbian::api::connectivity::TcpClient;
using symbian::api::connectivity::TcpListener;

int Send(void* context, const unsigned char* bytes, size_t length) {
  auto* client = static_cast<TcpClient*>(context);
  const auto result =
      client->SendFor(std::span(bytes, length), absl::Seconds(5));
  return result.ok() ? static_cast<int>(length) : MBEDTLS_ERR_NET_SEND_FAILED;
}

int Receive(void* context, unsigned char* bytes, size_t capacity) {
  auto* client = static_cast<TcpClient*>(context);
  const auto result =
      client->ReceiveFor(std::span(bytes, capacity), absl::Seconds(5));
  return result.ok() ? static_cast<int>(*result) : MBEDTLS_ERR_NET_RECV_FAILED;
}

struct TlsState {
  TlsState() {
    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&config);
    mbedtls_ctr_drbg_init(&random);
    mbedtls_entropy_init(&entropy);
    mbedtls_x509_crt_init(&certificate);
    mbedtls_pk_init(&private_key);
  }

  ~TlsState() {
    mbedtls_ssl_free(&ssl);
    mbedtls_ssl_config_free(&config);
    mbedtls_ctr_drbg_free(&random);
    mbedtls_entropy_free(&entropy);
    mbedtls_x509_crt_free(&certificate);
    mbedtls_pk_free(&private_key);
  }

  mbedtls_ssl_context ssl;
  mbedtls_ssl_config config;
  mbedtls_ctr_drbg_context random;
  mbedtls_entropy_context entropy;
  mbedtls_x509_crt certificate;
  mbedtls_pk_context private_key;
};

int ReadExactly(mbedtls_ssl_context* ssl, unsigned char* bytes,
                std::size_t length) {
  std::size_t received = 0;
  int retries = 0;
  while (received < length && retries < 16) {
    const int result =
        mbedtls_ssl_read(ssl, bytes + received, length - received);
    if (result > 0) {
      received += static_cast<std::size_t>(result);
      retries = 0;
    } else if (result == MBEDTLS_ERR_SSL_WANT_READ ||
               result == MBEDTLS_ERR_SSL_WANT_WRITE ||
               result == MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET) {
      ++retries;
    } else {
      return -260;
    }
  }
  return received == length ? 0 : -260;
}

int WriteExactly(mbedtls_ssl_context* ssl, const unsigned char* bytes,
                 std::size_t length) {
  std::size_t sent = 0;
  int retries = 0;
  while (sent < length && retries < 16) {
    const int result = mbedtls_ssl_write(ssl, bytes + sent, length - sent);
    if (result > 0) {
      sent += static_cast<std::size_t>(result);
      retries = 0;
    } else if (result == MBEDTLS_ERR_SSL_WANT_READ ||
               result == MBEDTLS_ERR_SSL_WANT_WRITE) {
      ++retries;
    } else {
      return -261;
    }
  }
  return sent == length ? 0 : -261;
}

int ServeReadOnlyControl(mbedtls_ssl_context* ssl, bool expect_oversized) {
  std::array<unsigned char, 4> prefix{};
  if (ReadExactly(ssl, prefix.data(), prefix.size()) != 0) {
    return -262;
  }
  const std::uint32_t size = (static_cast<std::uint32_t>(prefix[0]) << 24) |
                             (static_cast<std::uint32_t>(prefix[1]) << 16) |
                             (static_cast<std::uint32_t>(prefix[2]) << 8) |
                             static_cast<std::uint32_t>(prefix[3]);
  if (size == 0 || size > 4096) {
    return expect_oversized ? 0 : -263;
  }
  if (expect_oversized) {
    return -264;
  }
  std::array<unsigned char, 4096> payload{};
  if (ReadExactly(ssl, payload.data(), size) != 0) {
    return -265;
  }
  auto request = symbian::agent::ParseGuestControl(
      std::string_view(reinterpret_cast<const char*>(payload.data()), size));
  if (!request.ok()) {
    return -266;
  }
  auto response = symbian::agent::PackGuestResult(*request);
  if (!response.ok()) {
    return -267;
  }
  const std::uint32_t response_size = response->size();
  const std::array<unsigned char, 4> response_prefix{
      static_cast<unsigned char>(response_size >> 24),
      static_cast<unsigned char>(response_size >> 16),
      static_cast<unsigned char>(response_size >> 8),
      static_cast<unsigned char>(response_size)};
  if (WriteExactly(ssl, response_prefix.data(), response_prefix.size()) != 0 ||
      WriteExactly(ssl,
                   reinterpret_cast<const unsigned char*>(response->data()),
                   response->size()) != 0) {
    return -268;
  }
  return 0;
}

}  // namespace

extern "C" __attribute__((visibility("default"))) int MbedNativeTlsServerProbe(
    int port, int version, int mode) {
  if (port <= 0 || port > 65535 || (version != 12 && version != 13) ||
      mode < 0 || mode > 3) {
    return -240;
  }
  if (psa_crypto_init() != PSA_SUCCESS) {
    return -241;
  }
  auto listener =
      TcpListener::ListenIpv4({127, 0, 0, 1}, static_cast<std::uint16_t>(port));
  if (!listener.ok()) {
    return -242;
  }
  auto client = listener->AcceptFor(absl::Seconds(10));
  if (!client.ok()) {
    return -243;
  }
  TlsState state;
  if (mbedtls_ctr_drbg_seed(&state.random, mbedtls_entropy_func, &state.entropy,
                            nullptr, 0) != 0) {
    return -244;
  }
  if (mbedtls_ssl_config_defaults(&state.config, MBEDTLS_SSL_IS_SERVER,
                                  MBEDTLS_SSL_TRANSPORT_STREAM,
                                  MBEDTLS_SSL_PRESET_DEFAULT) != 0) {
    return -245;
  }
  mbedtls_ssl_conf_rng(&state.config, mbedtls_ctr_drbg_random, &state.random);
  const auto protocol =
      version == 12 ? MBEDTLS_SSL_VERSION_TLS1_2 : MBEDTLS_SSL_VERSION_TLS1_3;
  mbedtls_ssl_conf_min_tls_version(&state.config, protocol);
  mbedtls_ssl_conf_max_tls_version(&state.config, protocol);
  mbedtls_ssl_conf_authmode(&state.config, MBEDTLS_SSL_VERIFY_REQUIRED);
  if (mbedtls_x509_crt_parse(
          &state.certificate,
          reinterpret_cast<const unsigned char*>(kTestCertificate),
          sizeof(kTestCertificate)) != 0) {
    return -246;
  }
  mbedtls_ssl_conf_ca_chain(&state.config, &state.certificate, nullptr);
  if (mbedtls_pk_parse_key(
          &state.private_key,
          reinterpret_cast<const unsigned char*>(kTestPrivateKey),
          sizeof(kTestPrivateKey), nullptr, 0, mbedtls_ctr_drbg_random,
          &state.random) != 0) {
    return -247;
  }
  if (mbedtls_ssl_conf_own_cert(&state.config, &state.certificate,
                                &state.private_key) != 0) {
    return -248;
  }
  if (mbedtls_ssl_setup(&state.ssl, &state.config) != 0) {
    return -249;
  }
  mbedtls_ssl_set_bio(&state.ssl, &*client, Send, Receive, nullptr);
  const int handshake = mbedtls_ssl_handshake(&state.ssl);
  if (mode == 1) {
    return handshake != 0 ? 0 : -250;
  }
  if (handshake != 0) {
    return handshake;
  }
  if (mbedtls_ssl_get_verify_result(&state.ssl) != 0) {
    return -251;
  }
  if (std::strcmp(mbedtls_ssl_get_version(&state.ssl),
                  version == 12 ? "TLSv1.2" : "TLSv1.3") != 0) {
    return -252;
  }
  if (mode == 2 || mode == 3) {
    return ServeReadOnlyControl(&state.ssl, mode == 3);
  }
  unsigned char request = 0;
  int received = MBEDTLS_ERR_SSL_WANT_READ;
  for (int attempt = 0; attempt < 8 && (received == MBEDTLS_ERR_SSL_WANT_READ ||
                                        received == MBEDTLS_ERR_SSL_WANT_WRITE);
       ++attempt) {
    received = mbedtls_ssl_read(&state.ssl, &request, 1);
  }
  if (received != 1 || request != 'H') {
    return -253;
  }
  const unsigned char response = 'S';
  if (mbedtls_ssl_write(&state.ssl, &response, 1) != 1) {
    return -254;
  }
  return 0;
}
