// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_API_CONNECTIVITY_TCP_CLIENT_H_
#define SYMBIAN_API_CONNECTIVITY_TCP_CLIENT_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

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
 * worker-facing helper on one thread, including destruction; it is not an
 * active-object transport and provides no in-flight cancellation. The native
 * socket session and handle close when the object is destroyed.
 */
class TcpClient {
 public:
  /** @brief Open an RSocketServ session and connect to four IPv4 octets. */
  static absl::StatusOr<TcpClient> ConnectIpv4(
      std::array<std::uint8_t, 4> address, std::uint16_t port);

  TcpClient(TcpClient&& other) noexcept;
  TcpClient& operator=(TcpClient&& other) noexcept;
  TcpClient(const TcpClient&) = delete;
  TcpClient& operator=(const TcpClient&) = delete;
  ~TcpClient();

  /**
   * @brief Complete one native send request of at most 32 KiB.
   *
   * The caller keeps @p bytes alive until this method returns. An empty span
   * succeeds without a socket request. This synchronous API does not report
   * partial progress; use a separate asynchronous owner for credited streams.
   */
  absl::Status Send(std::span<const std::uint8_t> bytes);

  /**
   * @brief Send at most 32 KiB with a 0–60 second deadline.
   *
   * On expiry the native send request is cancelled and drained before the
   * caller's buffer can be released. Delivery may already have occurred;
   * callers must use an application acknowledgement for exactly-once work.
   */
  absl::Status SendFor(std::span<const std::uint8_t> bytes,
                       absl::Duration timeout);

  /**
   * @brief Receive at least one byte into a buffer of at most 32 KiB.
   *
   * The returned count is the number of bytes written. An empty span returns
   * zero. Native EOF and disconnect results are returned as statuses.
   */
  absl::StatusOr<std::size_t> Receive(std::span<std::uint8_t> bytes);

  /**
   * @brief Receive at least one byte with a 0–60 second deadline.
   *
   * A timeout cancels and drains the pending native read. A later read can
   * reuse the same stream; no abandoned callback may write into @p bytes.
   */
  absl::StatusOr<std::size_t> ReceiveFor(std::span<std::uint8_t> bytes,
                                         absl::Duration timeout);

 private:
  friend class TcpListener;
  friend class ActiveTcpListener;

  explicit TcpClient(NativeTcpClient* native) : native_(native) {}

  NativeTcpClient* native_ = nullptr;
};

}  // namespace symbian::api::connectivity

#endif  // SYMBIAN_API_CONNECTIVITY_TCP_CLIENT_H_
