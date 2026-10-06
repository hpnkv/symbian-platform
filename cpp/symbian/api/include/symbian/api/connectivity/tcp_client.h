// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_API_CONNECTIVITY_TCP_CLIENT_H_
#define SYMBIAN_API_CONNECTIVITY_TCP_CLIENT_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include <absl/base/nullability.h>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"

namespace symbian::api::connectivity {

struct NativeTcpClient;
class TcpListener;
class ActiveTcpListener;

/**
 * @brief One connected IPv4 TCP stream backed by Symbian RSocket.
 *
 * Connect, Send and Receive wait for native request completion. Use this
 * worker-facing helper on one thread, including destruction. A finite
 * deadline cancels and drains a pending native request on expiry. The native
 * socket session and handle close when the object is destroyed.
 */
class TcpClient {
 public:
  /**
   * @brief Connect to four IPv4 octets by an absolute deadline.
   *
   * InfiniteFuture waits until the native request completes. A finite
   * deadline cancels and drains the connect request on expiry.
   */
  static absl::StatusOr<TcpClient> ConnectIpv4(
      std::array<std::uint8_t, 4> address, std::uint16_t port,
      absl::Time deadline = absl::InfiniteFuture());

  /** @brief Resolve an ASCII DNS hostname and connect by one deadline.
   * Uses the native resolver, returning its first IPv4 result. International
   * names must be supplied as ASCII A-labels. Resolution cancels and drains on
   * expiry, like socket I/O. No resolver cache or scheduler is created.
   */
  static absl::StatusOr<TcpClient> ConnectHost(
      std::string_view hostname, std::uint16_t port,
      absl::Time deadline = absl::InfiniteFuture());

  TcpClient(TcpClient&& other) noexcept;
  TcpClient& operator=(TcpClient&& other) noexcept;
  TcpClient(const TcpClient&) = delete;
  TcpClient& operator=(const TcpClient&) = delete;
  ~TcpClient();

  /** @brief Close on the owner thread; repeated calls are harmless. */
  void Close();

  /** @brief Disable Nagle buffering for interactive message transports. */
  absl::Status SetNoDelay(bool enabled = true);

  /**
   * @brief Complete one native send request of at most 32 KiB by a deadline.
   *
   * The caller keeps @p bytes alive until this method returns. An empty span
   * succeeds without a socket request. Expiry cancels and drains the pending
   * send. Delivery may already have occurred; use an application acknowledgement
   * when it matters. This synchronous API does not report partial progress.
   */
  absl::Status Send(std::span<const std::uint8_t> bytes,
                    absl::Time deadline = absl::InfiniteFuture());

  /**
   * @brief Receive at least one byte into a buffer of at most 32 KiB.
   *
   * The returned count is the number of bytes written. An empty span returns
   * zero. Native EOF and disconnect results are returned as statuses. On
   * expiry the pending read is cancelled and drained; the stream can be reused.
   */
  absl::StatusOr<std::size_t> Receive(
      std::span<std::uint8_t> bytes,
      absl::Time deadline = absl::InfiniteFuture());

 private:
  friend class TcpListener;
  friend class ActiveTcpListener;

  explicit TcpClient(NativeTcpClient* absl_nonnull native) : native_(native) {}

  NativeTcpClient* absl_nullable native_ = nullptr;
};

}  // namespace symbian::api::connectivity

#endif  // SYMBIAN_API_CONNECTIVITY_TCP_CLIENT_H_
