// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_API_CONNECTIVITY_TCP_LISTENER_H_
#define SYMBIAN_API_CONNECTIVITY_TCP_LISTENER_H_

#include <array>
#include <cstdint>

#include <absl/base/nullability.h>

#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "symbian/api/connectivity/tcp_client.h"

namespace symbian::api::connectivity {

struct NativeTcpListener;

/**
 * @brief One bounded IPv4 listener backed by Symbian RSocket.
 *
 * ListenIpv4 binds one address and a nonzero port with backlog one. Accept
 * waits for a native request on the calling worker thread. An accepted client
 * may outlive the listener; both retain the socket-server session until their
 * own handles close. Accept cancels and drains an incomplete native accept
 * when its deadline expires. There is no active-object callback or
 * cross-thread cancellation yet. Use this synchronous owner on a worker.
 */
class TcpListener {
 public:
  /** @brief Bind and listen on an explicit four-octet IPv4 address. */
  static absl::StatusOr<TcpListener> ListenIpv4(
      std::array<std::uint8_t, 4> address, std::uint16_t port);

  TcpListener(TcpListener&& other) noexcept;
  TcpListener& operator=(TcpListener&& other) noexcept;
  TcpListener(const TcpListener&) = delete;
  TcpListener& operator=(const TcpListener&) = delete;
  ~TcpListener();

  /**
   * @brief Wait for one incoming stream until an absolute deadline.
   *
   * Expiry cancels and drains the native accept before returning a
   * deadline-exceeded status, so the listener can safely accept again.
   */
  absl::StatusOr<TcpClient> Accept(
      absl::Time deadline = absl::InfiniteFuture());

 private:
  explicit TcpListener(NativeTcpListener* absl_nonnull native)
      : native_(native) {}

  NativeTcpListener* absl_nullable native_ = nullptr;
};

}  // namespace symbian::api::connectivity

#endif  // SYMBIAN_API_CONNECTIVITY_TCP_LISTENER_H_
