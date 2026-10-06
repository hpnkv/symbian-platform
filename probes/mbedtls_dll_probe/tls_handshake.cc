// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.
// RM-807 emulator research probe: never linked by the default SDK archive.

#include <array>
#include <cstdint>
#include <cstring>
#include <span>

#include <absl/base/nullability.h>

#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/x509_crt.h"
#include "psa/crypto.h"
#include "symbian/api/connectivity/tcp_client.h"
#include "test_certificate.h"

namespace {

using symbian::api::connectivity::TcpClient;

int Send(void* absl_nonnull context, const unsigned char* absl_nonnull bytes,
         size_t length) {
  auto* absl_nonnull client = static_cast<TcpClient*>(context);
  const auto result = client->Send(std::span(bytes, length));
  return result.ok() ? static_cast<int>(length) : MBEDTLS_ERR_NET_SEND_FAILED;
}

int Receive(void* absl_nonnull context, unsigned char* absl_nonnull bytes,
            size_t capacity) {
  auto* absl_nonnull client = static_cast<TcpClient*>(context);
  const auto result = client->Receive(std::span(bytes, capacity));
  return result.ok() ? static_cast<int>(*result) : MBEDTLS_ERR_NET_RECV_FAILED;
}

struct TlsState {
  TlsState() {
    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&config);
    mbedtls_ctr_drbg_init(&random);
    mbedtls_entropy_init(&entropy);
    mbedtls_x509_crt_init(&trust);
  }

  ~TlsState() {
    mbedtls_ssl_free(&ssl);
    mbedtls_ssl_config_free(&config);
    mbedtls_ctr_drbg_free(&random);
    mbedtls_entropy_free(&entropy);
    mbedtls_x509_crt_free(&trust);
  }

  mbedtls_ssl_context ssl;
  mbedtls_ssl_config config;
  mbedtls_ctr_drbg_context random;
  mbedtls_entropy_context entropy;
  mbedtls_x509_crt trust;
};

}  // namespace

extern "C" __attribute__((visibility("default"))) int
MbedNativeTlsHandshakeProbe(int port, int version, int mode) {
  if (port <= 0 || port > 65535 || (version != 12 && version != 13) ||
      mode < 0 || mode > 3) {
    return -180;
  }
  if (psa_crypto_init() != PSA_SUCCESS) {
    return -193;
  }
  auto client =
      TcpClient::ConnectIpv4({127, 0, 0, 1}, static_cast<std::uint16_t>(port));
  if (!client.ok()) {
    return -181;
  }
  TlsState state;
  if (mbedtls_ctr_drbg_seed(&state.random, mbedtls_entropy_func, &state.entropy,
                            nullptr, 0) != 0) {
    return -182;
  }
  if (mbedtls_ssl_config_defaults(&state.config, MBEDTLS_SSL_IS_CLIENT,
                                  MBEDTLS_SSL_TRANSPORT_STREAM,
                                  MBEDTLS_SSL_PRESET_DEFAULT) != 0) {
    return -183;
  }
  mbedtls_ssl_conf_rng(&state.config, mbedtls_ctr_drbg_random, &state.random);
  const auto protocol =
      version == 12 ? MBEDTLS_SSL_VERSION_TLS1_2 : MBEDTLS_SSL_VERSION_TLS1_3;
  mbedtls_ssl_conf_min_tls_version(&state.config, protocol);
  mbedtls_ssl_conf_max_tls_version(&state.config, protocol);
  mbedtls_ssl_conf_authmode(&state.config, MBEDTLS_SSL_VERIFY_REQUIRED);
  if (mode != 2) {
    const char* absl_nonnull certificate =
        mode == 3 ? kExpiredCertificate : kTestCertificate;
    const std::size_t certificate_length =
        mode == 3 ? sizeof(kExpiredCertificate) : sizeof(kTestCertificate);
    if (mbedtls_x509_crt_parse(
            &state.trust, reinterpret_cast<const unsigned char*>(certificate),
            certificate_length) != 0) {
      return -184;
    }
    mbedtls_ssl_conf_ca_chain(&state.config, &state.trust, nullptr);
  }
  if (mbedtls_ssl_setup(&state.ssl, &state.config) != 0) {
    return -185;
  }
  const char* absl_nonnull hostname = mode == 1 ? "wrong-name" : "sdk-test";
  if (mbedtls_ssl_set_hostname(&state.ssl, hostname) != 0) {
    return -186;
  }
  mbedtls_ssl_set_bio(&state.ssl, &*client, Send, Receive, nullptr);
  const int handshake = mbedtls_ssl_handshake(&state.ssl);
  const std::uint32_t flags = mbedtls_ssl_get_verify_result(&state.ssl);
  if (mode == 1) {
    return handshake != 0 && (flags & MBEDTLS_X509_BADCERT_CN_MISMATCH) != 0
               ? 0
               : -187;
  }
  if (mode == 2) {
    return handshake != 0 && (flags & MBEDTLS_X509_BADCERT_NOT_TRUSTED) != 0
               ? 0
               : -187;
  }
  if (mode == 3) {
    return handshake != 0 && (flags & MBEDTLS_X509_BADCERT_EXPIRED) != 0 ? 0
                                                                         : -187;
  }
  if (handshake != 0) {
    return handshake;
  }
  if (flags != 0) {
    return -189;
  }
  if (std::strcmp(mbedtls_ssl_get_version(&state.ssl),
                  version == 12 ? "TLSv1.2" : "TLSv1.3") != 0) {
    return -192;
  }
  const unsigned char request = 'H';
  if (mbedtls_ssl_write(&state.ssl, &request, 1) != 1) {
    return -190;
  }
  unsigned char reply = 0;
  int received = MBEDTLS_ERR_SSL_WANT_READ;
  for (int attempt = 0;
       attempt < 8 && (received == MBEDTLS_ERR_SSL_WANT_READ ||
                       received == MBEDTLS_ERR_SSL_WANT_WRITE ||
                       received == MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET);
       ++attempt) {
    received = mbedtls_ssl_read(&state.ssl, &reply, 1);
  }
  if (received != 1) {
    return received < 0 ? received : -191;
  }
  if (reply != 'S') {
    return -194;
  }
  return 0;
}
