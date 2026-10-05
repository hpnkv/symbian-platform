// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include <algorithm>
#include <cstdlib>
#include <new>
#include <optional>
#include <span>
#include <string>
#include <utility>

#include "absl/time/clock.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/pk.h"
#include "mbedtls/ssl.h"
#include "mbedtls/x509_crt.h"
#include "psa/crypto.h"
#include "symbian/api/connectivity/tls_stream.h"
#include "symbian/native_status.h"

namespace symbian::api::connectivity {
namespace {

constexpr std::size_t kMaximumPemBytes = 262144;
constexpr std::size_t kMaximumIoBytes = 32768;

absl::Status TlsError(std::string_view operation, int code) {
  return code == 0 ? absl::OkStatus()
                   : absl::UnavailableError(std::string(operation) + ": " +
                                            std::to_string(code));
}

}  // namespace

struct TlsStream::Impl {
  Impl() {
    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&config);
    mbedtls_ctr_drbg_init(&random);
    mbedtls_entropy_init(&entropy);
    mbedtls_x509_crt_init(&certificate);
    mbedtls_x509_crt_init(&client_roots);
    mbedtls_pk_init(&private_key);
  }

  ~Impl() {
    mbedtls_ssl_free(&ssl);
    mbedtls_ssl_config_free(&config);
    mbedtls_ctr_drbg_free(&random);
    mbedtls_entropy_free(&entropy);
    mbedtls_x509_crt_free(&certificate);
    mbedtls_x509_crt_free(&client_roots);
    mbedtls_pk_free(&private_key);
  }

  absl::Status CheckDeadline() const {
    if (deadline != absl::InfiniteFuture() && absl::Now() >= deadline) {
      return absl::DeadlineExceededError("TLS operation deadline expired");
    }
    return absl::OkStatus();
  }

  static int Send(void* context, const unsigned char* bytes,
                  std::size_t length) {
    auto* self = static_cast<Impl*>(context);
    self->io_status = self->CheckDeadline();
    if (!self->io_status.ok()) {
      return MBEDTLS_ERR_NET_SEND_FAILED;
    }
    if (length > kMaximumIoBytes) {
      self->io_status = absl::ResourceExhaustedError("TLS send exceeds 32 KiB");
      return MBEDTLS_ERR_NET_SEND_FAILED;
    }
    self->io_status =
        self->client->Send(std::span(bytes, length), self->deadline);
    return self->io_status.ok() ? static_cast<int>(length)
                                : MBEDTLS_ERR_NET_SEND_FAILED;
  }

  static int Receive(void* context, unsigned char* bytes,
                     std::size_t capacity) {
    auto* self = static_cast<Impl*>(context);
    self->io_status = self->CheckDeadline();
    if (!self->io_status.ok()) {
      return MBEDTLS_ERR_NET_RECV_FAILED;
    }
    auto count = self->client->Receive(
        std::span(bytes, std::min(capacity, kMaximumIoBytes)), self->deadline);
    if (!count.ok()) {
      if (NativeErrorFromStatus(count.status()) == -25) {
        return 0;
      }
      self->io_status = count.status();
      return MBEDTLS_ERR_NET_RECV_FAILED;
    }
    self->io_status = absl::OkStatus();
    return static_cast<int>(*count);
  }

  std::string alpn;
  const char* protocols[2] = {nullptr, nullptr};
  mbedtls_ssl_context ssl;
  mbedtls_ssl_config config;
  mbedtls_ctr_drbg_context random;
  mbedtls_entropy_context entropy;
  mbedtls_x509_crt certificate;
  mbedtls_x509_crt client_roots;
  mbedtls_pk_context private_key;
  std::optional<TcpClient> client;
  absl::Time deadline = absl::InfiniteFuture();
  absl::Status io_status;
  bool connected = false;
};

absl::StatusOr<TlsStream> TlsStream::Create(std::string_view server_certificate,
                                            std::string_view server_private_key,
                                            std::string_view client_ca_bundle,
                                            TlsVersion version) {
  if (server_certificate.empty() || server_private_key.empty() ||
      client_ca_bundle.empty() ||
      server_certificate.size() > kMaximumPemBytes ||
      server_private_key.size() > kMaximumPemBytes ||
      client_ca_bundle.size() > kMaximumPemBytes) {
    return absl::InvalidArgumentError("TLS PEM input is empty or oversized");
  }
  auto initialized = Initialize(true, version);
  if (!initialized.ok()) {
    return initialized.status();
  }
  TlsStream result = std::move(*initialized);
  auto* impl = result.impl_;
  int status = 0;
  const std::string certificate(server_certificate);
  const std::string key(server_private_key);
  const std::string ca(client_ca_bundle);
  status = mbedtls_x509_crt_parse(
      &impl->certificate,
      reinterpret_cast<const unsigned char*>(certificate.c_str()),
      certificate.size() + 1);
  if (status != 0) {
    return TlsError("TLS server certificate rejected", status);
  }
  status = mbedtls_x509_crt_parse(
      &impl->client_roots, reinterpret_cast<const unsigned char*>(ca.c_str()),
      ca.size() + 1);
  if (status != 0) {
    return TlsError("TLS client CA rejected", status);
  }
  mbedtls_ssl_conf_ca_chain(&impl->config, &impl->client_roots, nullptr);
  status = mbedtls_pk_parse_key(
      &impl->private_key, reinterpret_cast<const unsigned char*>(key.c_str()),
      key.size() + 1, nullptr, 0, mbedtls_ctr_drbg_random, &impl->random);
  if (status != 0) {
    return TlsError("TLS private key rejected", status);
  }
  status = mbedtls_ssl_conf_own_cert(&impl->config, &impl->certificate,
                                     &impl->private_key);
  if (status != 0) {
    return TlsError("TLS server identity rejected", status);
  }
  return result;
}

absl::StatusOr<TlsStream> TlsStream::Initialize(bool server,
                                                TlsVersion version) {
  void* memory = std::malloc(sizeof(Impl));
  if (memory == nullptr) {
    return absl::ResourceExhaustedError("TLS context allocation failed");
  }
  auto* impl = new (memory) Impl;
  TlsStream result(impl);
  if (psa_crypto_init() != PSA_SUCCESS) {
    return absl::UnavailableError("PSA crypto initialization failed");
  }
  const int seed = mbedtls_ctr_drbg_seed(&impl->random, mbedtls_entropy_func,
                                         &impl->entropy, nullptr, 0);
  if (seed != 0) {
    return TlsError("TLS entropy seed failed", seed);
  }
  int status = mbedtls_ssl_config_defaults(
      &impl->config, server ? MBEDTLS_SSL_IS_SERVER : MBEDTLS_SSL_IS_CLIENT,
      MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
  if (status != 0) {
    return TlsError("TLS server configuration failed", status);
  }
  mbedtls_ssl_conf_rng(&impl->config, mbedtls_ctr_drbg_random, &impl->random);
  const auto protocol = version == TlsVersion::kTls12
                            ? MBEDTLS_SSL_VERSION_TLS1_2
                            : MBEDTLS_SSL_VERSION_TLS1_3;
  mbedtls_ssl_conf_min_tls_version(&impl->config, protocol);
  mbedtls_ssl_conf_max_tls_version(&impl->config, protocol);
  mbedtls_ssl_conf_authmode(&impl->config, MBEDTLS_SSL_VERIFY_REQUIRED);
  return result;
}

absl::StatusOr<TlsStream> TlsStream::Connect(
    TcpClient client, std::string_view hostname, std::string_view ca_bundle,
    TlsVersion version, std::string_view alpn, absl::Time deadline) {
  if (hostname.empty() || hostname.size() > 253 ||
      hostname.find('\0') != std::string_view::npos || ca_bundle.empty() ||
      ca_bundle.size() > kMaximumPemBytes ||
      (alpn != "http/1.1" && alpn != "h2")) {
    return absl::InvalidArgumentError("Invalid TLS hostname, roots or ALPN");
  }
  auto configured = Initialize(false, version);
  if (!configured.ok()) {
    return configured.status();
  }
  TlsStream result = std::move(*configured);
  auto* impl = result.impl_;
  std::string roots(ca_bundle);
  int code = mbedtls_x509_crt_parse(
      &impl->client_roots,
      reinterpret_cast<const unsigned char*>(roots.c_str()), roots.size() + 1);
  if (code != 0) {
    return TlsError("TLS CA roots rejected", code);
  }
  mbedtls_ssl_conf_ca_chain(&impl->config, &impl->client_roots, nullptr);
  impl->alpn = alpn;
  impl->protocols[0] = impl->alpn.c_str();
  code = mbedtls_ssl_conf_alpn_protocols(&impl->config, impl->protocols);
  if (code) {
    return TlsError("TLS ALPN configuration failed", code);
  }
  code = mbedtls_ssl_setup(&impl->ssl, &impl->config);
  if (code) {
    return TlsError("TLS setup failed", code);
  }
  const std::string name(hostname);
  code = mbedtls_ssl_set_hostname(&impl->ssl, name.c_str());
  if (code) {
    return TlsError("TLS hostname rejected", code);
  }
  impl->client.emplace(std::move(client));
  mbedtls_ssl_set_bio(&impl->ssl, impl, &Impl::Send, &Impl::Receive, nullptr);
  auto status = result.Handshake(deadline);
  if (!status.ok()) {
    return status;
  }
  const char* selected = mbedtls_ssl_get_alpn_protocol(&impl->ssl);
  if ((selected && alpn != selected) || (!selected && alpn == "h2")) {
    return absl::FailedPreconditionError(
        "TLS peer did not negotiate required HTTP protocol");
  }
  return result;
}

absl::Status TlsStream::SetAlpnProtocol(std::string_view protocol) {
  if (impl_ == nullptr || impl_->client) {
    return absl::FailedPreconditionError(
        "Set ALPN before opening a TLS session");
  }
  if (protocol != "http/1.1" && protocol != "h2") {
    return absl::InvalidArgumentError("Unsupported TLS ALPN protocol");
  }
  impl_->alpn = protocol;
  impl_->protocols[0] = impl_->alpn.c_str();
  return TlsError(
      "TLS ALPN configuration failed",
      mbedtls_ssl_conf_alpn_protocols(&impl_->config, impl_->protocols));
}

std::string_view TlsStream::negotiated_version() const {
  return connected() ? mbedtls_ssl_get_version(&impl_->ssl) : "";
}

std::string_view TlsStream::negotiated_protocol() const {
  if (!connected()) {
    return "";
  }
  const char* protocol = mbedtls_ssl_get_alpn_protocol(&impl_->ssl);
  return protocol ? protocol : "";
}

TlsStream::TlsStream(TlsStream&& other) noexcept
    : impl_(std::exchange(other.impl_, nullptr)) {}

TlsStream& TlsStream::operator=(TlsStream&& other) noexcept {
  if (this != &other) {
    if (impl_ != nullptr) {
      impl_->~Impl();
      std::free(impl_);
    }
    impl_ = std::exchange(other.impl_, nullptr);
  }
  return *this;
}

TlsStream::~TlsStream() {
  if (impl_ != nullptr) {
    impl_->~Impl();
    std::free(impl_);
  }
}

absl::Status TlsStream::Accept(TcpClient&& client, absl::Time deadline) {
  if (impl_ == nullptr || impl_->client) {
    return absl::FailedPreconditionError("TLS server already has a stream");
  }
  impl_->client.emplace(std::move(client));
  impl_->deadline = deadline;
  impl_->io_status = absl::OkStatus();
  int status = mbedtls_ssl_setup(&impl_->ssl, &impl_->config);
  if (status != 0) {
    CloseSession();
    return TlsError("TLS setup failed", status);
  }
  mbedtls_ssl_set_bio(&impl_->ssl, impl_, &Impl::Send, &Impl::Receive, nullptr);
  return Handshake(deadline);
}

absl::Status TlsStream::Handshake(absl::Time deadline) {
  int status = 0;
  impl_->deadline = deadline;
  impl_->io_status = absl::OkStatus();
  for (int attempt = 0; attempt < 64; ++attempt) {
    status = mbedtls_ssl_handshake(&impl_->ssl);
    if (status == 0) {
      if (mbedtls_ssl_get_verify_result(&impl_->ssl) != 0) {
        CloseSession();
        return absl::UnauthenticatedError("TLS client certificate rejected");
      }
      impl_->connected = true;
      return absl::OkStatus();
    }
    if (!impl_->io_status.ok()) {
      absl::Status error = impl_->io_status;
      CloseSession();
      return error;
    }
    if (status != MBEDTLS_ERR_SSL_WANT_READ &&
        status != MBEDTLS_ERR_SSL_WANT_WRITE) {
      CloseSession();
      return TlsError("TLS handshake failed", status);
    }
  }
  CloseSession();
  return absl::DeadlineExceededError("TLS handshake retry limit reached");
}

absl::StatusOr<std::size_t> TlsStream::Read(std::span<std::uint8_t> bytes,
                                            absl::Time deadline) {
  if (!connected() || bytes.empty() || bytes.size() > kMaximumIoBytes) {
    return absl::FailedPreconditionError("TLS read needs a live 1-32 KiB span");
  }
  impl_->deadline = deadline;
  impl_->io_status = absl::OkStatus();
  for (int attempt = 0; attempt < 64; ++attempt) {
    const int count = mbedtls_ssl_read(&impl_->ssl, bytes.data(), bytes.size());
    if (count > 0) {
      return static_cast<std::size_t>(count);
    }
    if (!impl_->io_status.ok()) {
      return impl_->io_status;
    }
    if (count == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
      return std::size_t{0};
    }
    if (count == 0) {
      return absl::DataLossError("TLS transport EOF without close_notify");
    }
    if (count != MBEDTLS_ERR_SSL_WANT_READ &&
        count != MBEDTLS_ERR_SSL_WANT_WRITE &&
        count != MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET) {
      return TlsError("TLS read failed", count);
    }
  }
  return absl::DeadlineExceededError("TLS read retry limit reached");
}

absl::Status TlsStream::Write(std::span<const std::uint8_t> bytes,
                              absl::Time deadline) {
  if (!connected() || bytes.empty() || bytes.size() > kMaximumIoBytes) {
    return absl::FailedPreconditionError(
        "TLS write needs a live 1-32 KiB span");
  }
  impl_->deadline = deadline;
  impl_->io_status = absl::OkStatus();
  std::size_t written = 0;
  for (int attempt = 0; attempt < 64 && written < bytes.size(); ++attempt) {
    const int count = mbedtls_ssl_write(&impl_->ssl, bytes.data() + written,
                                        bytes.size() - written);
    if (count > 0) {
      written += static_cast<std::size_t>(count);
      continue;
    }
    if (!impl_->io_status.ok()) {
      return impl_->io_status;
    }
    if (count != MBEDTLS_ERR_SSL_WANT_READ &&
        count != MBEDTLS_ERR_SSL_WANT_WRITE) {
      return TlsError("TLS write failed", count);
    }
  }
  return written == bytes.size()
             ? absl::OkStatus()
             : absl::DeadlineExceededError("TLS write retry limit reached");
}

void TlsStream::CloseSession() {
  if (impl_ != nullptr) {
    mbedtls_ssl_free(&impl_->ssl);
    mbedtls_ssl_init(&impl_->ssl);
    impl_->client.reset();
    impl_->connected = false;
  }
}

bool TlsStream::connected() const {
  return impl_ != nullptr && impl_->connected;
}

}  // namespace symbian::api::connectivity
