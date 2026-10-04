// SPDX-License-Identifier: Apache-2.0
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>

#include <gtest/gtest.h>

#include "fixtures.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/debug.h"
#include "mbedtls/entropy.h"
#include "mbedtls/pk.h"
#include "mbedtls/ssl.h"
#include "mbedtls/x509_crt.h"
#include "psa/crypto.h"

namespace {
struct Wire {
  std::deque<unsigned char>* input;
  std::deque<unsigned char>* output;
};

int Send(void* opaque, const unsigned char* bytes, size_t size) {
  auto* wire = static_cast<Wire*>(opaque);
  wire->output->insert(wire->output->end(), bytes, bytes + size);
  return static_cast<int>(size);
}

int Receive(void* opaque, unsigned char* bytes, size_t size) {
  auto* wire = static_cast<Wire*>(opaque);
  if (wire->input->empty())
    return MBEDTLS_ERR_SSL_WANT_READ;
  size = std::min(size, wire->input->size());
  for (size_t i = 0; i < size; ++i) {
    bytes[i] = wire->input->front();
    wire->input->pop_front();
  }
  return static_cast<int>(size);
}

struct Endpoint {
  mbedtls_ssl_context ssl;
  mbedtls_ssl_config config;
  mbedtls_entropy_context entropy;
  mbedtls_ctr_drbg_context random;
  mbedtls_x509_crt certificate;
  mbedtls_pk_context private_key;

  Endpoint() {
    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&config);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&random);
    mbedtls_x509_crt_init(&certificate);
    mbedtls_pk_init(&private_key);
  }

  ~Endpoint() {
    mbedtls_ssl_free(&ssl);
    mbedtls_ssl_config_free(&config);
    mbedtls_ctr_drbg_free(&random);
    mbedtls_entropy_free(&entropy);
    mbedtls_x509_crt_free(&certificate);
    mbedtls_pk_free(&private_key);
  }

  int Configure(int role, mbedtls_ssl_protocol_version version,
                const char* hostname = "sdk-test") {
    int result = mbedtls_ctr_drbg_seed(&random, mbedtls_entropy_func, &entropy,
                                       nullptr, 0);
    if (result != 0)
      return result;
    result =
        mbedtls_ssl_config_defaults(&config, role, MBEDTLS_SSL_TRANSPORT_STREAM,
                                    MBEDTLS_SSL_PRESET_DEFAULT);
    if (result != 0)
      return result;
    if (std::getenv("SYMBIAN_MBEDTLS_TLS_TRACE")) {
      mbedtls_debug_set_threshold(2);
      mbedtls_ssl_conf_dbg(
          &config,
          [](void*, int, const char* file, int line, const char* message) {
            std::fprintf(stderr, "%s:%d: %s", file, line, message);
          },
          nullptr);
    }
    mbedtls_ssl_conf_rng(&config, mbedtls_ctr_drbg_random, &random);
    mbedtls_ssl_conf_min_tls_version(&config, version);
    mbedtls_ssl_conf_max_tls_version(&config, version);
    result = mbedtls_x509_crt_parse(
        &certificate, reinterpret_cast<const unsigned char*>(kTestCertificate),
        sizeof(kTestCertificate));
    if (result != 0)
      return result;
    if (role == MBEDTLS_SSL_IS_SERVER) {
      result = mbedtls_pk_parse_key(
          &private_key, reinterpret_cast<const unsigned char*>(kTestPrivateKey),
          sizeof(kTestPrivateKey), nullptr, 0, mbedtls_ctr_drbg_random,
          &random);
      if (result != 0)
        return result;
      result = mbedtls_ssl_conf_own_cert(&config, &certificate, &private_key);
      if (result != 0)
        return result;
    } else {
      mbedtls_ssl_conf_ca_chain(&config, &certificate, nullptr);
      mbedtls_ssl_conf_authmode(&config, MBEDTLS_SSL_VERIFY_REQUIRED);
    }
    result = mbedtls_ssl_setup(&ssl, &config);
    if (result == 0 && role == MBEDTLS_SSL_IS_CLIENT) {
      result = mbedtls_ssl_set_hostname(&ssl, hostname);
    }
    return result;
  }
};

class TlsTest : public testing::TestWithParam<mbedtls_ssl_protocol_version> {
 protected:
  void SetUp() override { ASSERT_EQ(psa_crypto_init(), PSA_SUCCESS); }

  void TearDown() override { mbedtls_psa_crypto_free(); }
};

TEST_P(TlsTest, HandshakeAndEncryptedApplicationData) {
  // Certificate authentication uses host OS entropy/time and explicit trust.
  // It does not test guest services. Every pump is bounded and nonblocking.
  Endpoint client, server;
  ASSERT_EQ(client.Configure(MBEDTLS_SSL_IS_CLIENT, GetParam()), 0);
  ASSERT_EQ(server.Configure(MBEDTLS_SSL_IS_SERVER, GetParam()), 0);
  std::deque<unsigned char> to_client, to_server;
  Wire client_wire{&to_client, &to_server}, server_wire{&to_server, &to_client};
  mbedtls_ssl_set_bio(&client.ssl, &client_wire, Send, Receive, nullptr);
  mbedtls_ssl_set_bio(&server.ssl, &server_wire, Send, Receive, nullptr);
  bool client_ready = false, server_ready = false;
  for (int turn = 0; turn < 1000 && !(client_ready && server_ready); ++turn) {
    if (!client_ready) {
      int result = mbedtls_ssl_handshake(&client.ssl);
      ASSERT_TRUE(result == 0 || result == MBEDTLS_ERR_SSL_WANT_READ ||
                  result == MBEDTLS_ERR_SSL_WANT_WRITE)
          << result;
      client_ready = result == 0;
    }
    if (!server_ready) {
      int result = mbedtls_ssl_handshake(&server.ssl);
      ASSERT_TRUE(result == 0 || result == MBEDTLS_ERR_SSL_WANT_READ ||
                  result == MBEDTLS_ERR_SSL_WANT_WRITE)
          << result;
      server_ready = result == 0;
    }
  }
  ASSERT_TRUE(client_ready && server_ready);
  EXPECT_STREQ(
      mbedtls_ssl_get_version(&client.ssl),
      GetParam() == MBEDTLS_SSL_VERSION_TLS1_2 ? "TLSv1.2" : "TLSv1.3");
  const unsigned char message[] = "hello from the SDK";
  ASSERT_EQ(mbedtls_ssl_write(&client.ssl, message, sizeof(message)),
            sizeof(message));
  std::array<unsigned char, 64> output{};
  ASSERT_EQ(mbedtls_ssl_read(&server.ssl, output.data(), output.size()),
            sizeof(message));
  EXPECT_EQ(std::memcmp(output.data(), message, sizeof(message)), 0);
}

TEST_P(TlsTest, RejectsWrongHostname) {
  Endpoint client, server;
  ASSERT_EQ(client.Configure(MBEDTLS_SSL_IS_CLIENT, GetParam(), "wrong-name"),
            0);
  ASSERT_EQ(server.Configure(MBEDTLS_SSL_IS_SERVER, GetParam()), 0);
  std::deque<unsigned char> to_client, to_server;
  Wire client_wire{&to_client, &to_server}, server_wire{&to_server, &to_client};
  mbedtls_ssl_set_bio(&client.ssl, &client_wire, Send, Receive, nullptr);
  mbedtls_ssl_set_bio(&server.ssl, &server_wire, Send, Receive, nullptr);
  int client_result = MBEDTLS_ERR_SSL_WANT_READ;
  for (int turn = 0; turn < 1000; ++turn) {
    client_result = mbedtls_ssl_handshake(&client.ssl);
    if (client_result != MBEDTLS_ERR_SSL_WANT_READ &&
        client_result != MBEDTLS_ERR_SSL_WANT_WRITE)
      break;
    int result = mbedtls_ssl_handshake(&server.ssl);
    ASSERT_TRUE(result == 0 || result == MBEDTLS_ERR_SSL_WANT_READ ||
                result == MBEDTLS_ERR_SSL_WANT_WRITE)
        << result;
  }
  EXPECT_EQ(client_result, MBEDTLS_ERR_X509_CERT_VERIFY_FAILED);
  EXPECT_NE(mbedtls_ssl_get_verify_result(&client.ssl) &
                MBEDTLS_X509_BADCERT_CN_MISMATCH,
            0u);
}

INSTANTIATE_TEST_SUITE_P(Protocols, TlsTest,
                         testing::Values(MBEDTLS_SSL_VERSION_TLS1_2
#if defined(MBEDTLS_SSL_PROTO_TLS1_3)
                                         ,
                                         MBEDTLS_SSL_VERSION_TLS1_3
#endif
                                         ));
}  // namespace
