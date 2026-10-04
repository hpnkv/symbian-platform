// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_API_SYSTEM_ACTIVE_SERVICE_H_
#define SYMBIAN_API_SYSTEM_ACTIVE_SERVICE_H_

#include <cstdint>
#include <functional>

#include "absl/status/status.h"

namespace symbian::api::system {

/**
 * @brief Run a service on the calling thread's native active scheduler.
 *
 * Owns a process-local stop property. The start callback runs after scheduler
 * installation and must arm its asynchronous requests before returning.
 * on_ready runs once after the stop signal is armed. on_stop runs when the
 * signal requests shutdown or its subscription fails. Both callbacks and
 * StopActiveService run on the scheduler thread. Another thread
 * can request shutdown through RequestActiveServiceStop. Only one service loop
 * may run per thread; the property category/key must be unique to the app.
 */
absl::Status RunActiveService(std::int32_t category, std::uint32_t stop_key,
                              const std::function<absl::Status()>& start,
                              const std::function<void()>& on_stop,
                              const std::function<void()>& on_ready);

/** @brief Request an active service to stop from another guest thread. */
absl::Status RequestActiveServiceStop(std::int32_t category,
                                      std::uint32_t stop_key);

/** @brief Stop the current thread's active service from its event callback. */
void StopActiveService();

}  // namespace symbian::api::system

#endif  // SYMBIAN_API_SYSTEM_ACTIVE_SERVICE_H_
