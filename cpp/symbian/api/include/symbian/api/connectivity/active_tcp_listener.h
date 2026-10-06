// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_API_CONNECTIVITY_ACTIVE_TCP_LISTENER_H_
#define SYMBIAN_API_CONNECTIVITY_ACTIVE_TCP_LISTENER_H_

#include <array>
#include <cstdint>

#include <absl/base/nullability.h>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "symbian/api/connectivity/tcp_client.h"

namespace symbian::api::connectivity {

struct NativeTcpListener;
struct NativeActiveTcpListener;
struct NativeTcpClient;

/** @brief Receives one accepted stream or native error on the event thread. */
class TcpAcceptObserver {
 public:
  virtual ~TcpAcceptObserver() = default;
  virtual void OnAccept(absl::StatusOr<TcpClient> result) = 0;
};

/**
 * @brief A single pending RSocket accept managed by a Symbian active scheduler.
 *
 * Construct on a thread with an installed CActiveScheduler. ListenIpv4 arms
 * one accept. The observer must call AcceptNext when ready for another; no
 * polling or timeout wakeups occur while an accept is pending. The observer
 * and listener must remain alive until OnAccept returns. Stop synchronously
 * cancels and drains a pending accept before closing its native handles.
 *
 * OnAccept must not leave or destroy this listener. Keep it short. A connected
 * TcpClient is worker-facing; a service
 * should arrange its TLS work without blocking this scheduler thread.
 */
class ActiveTcpListener final {
 public:
  explicit ActiveTcpListener(TcpAcceptObserver* absl_nonnull observer);
  ActiveTcpListener(const ActiveTcpListener&) = delete;
  ActiveTcpListener& operator=(const ActiveTcpListener&) = delete;
  ~ActiveTcpListener();

  /** @brief Bind an explicit address and arm the first accept. */
  absl::Status ListenIpv4(std::array<std::uint8_t, 4> address,
                          std::uint16_t port);

  /** @brief Arm one accept after OnAccept; never starts a second in flight. */
  absl::Status AcceptNext();

  /**
   * @brief Ask the Socket Server to share its session with worker threads.
   *
   * Call before ListenIpv4, so the session is shared before its socket opens.
   * Workers must finish using their clients before this listener is destroyed.
   * Firmware that rejects shareable sessions returns a native error from
   * ListenIpv4.
   */
  absl::Status EnableWorkerSharing();

  /** @brief Cancel/drain and close; an owner cannot be restarted afterward. */
  void Stop();

  bool is_listening() const;

 private:
  static void OnNativeAccept(void* absl_nonnull context,
                             NativeTcpClient* absl_nonnull accepted,
                             int result);

  TcpAcceptObserver& observer_;
  NativeActiveTcpListener* absl_nullable native_ = nullptr;
  bool started_ = false;
  bool share_with_workers_ = false;
};

}  // namespace symbian::api::connectivity

#endif  // SYMBIAN_API_CONNECTIVITY_ACTIVE_TCP_LISTENER_H_
