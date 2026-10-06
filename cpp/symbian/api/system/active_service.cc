// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "symbian/api/system/active_service.h"

#include <absl/base/nullability.h>

#include "native_active_service.h"
#include "symbian/native_status.h"

namespace symbian::api::system {
namespace {
struct Callbacks {
  const std::function<absl::Status()>& start;
  const std::function<void()>& on_stop;
  const std::function<void()>& on_ready;
  absl::Status start_status;
};

int Start(void* absl_nonnull context) {
  auto& callbacks = *static_cast<Callbacks*>(context);
  callbacks.start_status = callbacks.start();
  return symbian::NativeErrorFromStatus(callbacks.start_status);
}

void OnStop(void* absl_nonnull context) {
  static_cast<Callbacks*>(context)->on_stop();
}

void OnReady(void* absl_nonnull context) {
  static_cast<Callbacks*>(context)->on_ready();
}
}  // namespace

absl::Status RunActiveService(std::int32_t category, std::uint32_t stop_key,
                              const std::function<absl::Status()>& start,
                              const std::function<void()>& on_stop,
                              const std::function<void()>& on_ready) {
  if (category == 0 || stop_key == 0 || !start || !on_stop || !on_ready) {
    return absl::InvalidArgumentError("Invalid active service configuration");
  }
  Callbacks callbacks{start, on_stop, on_ready, absl::OkStatus()};
  const int result = SymbianDeviceRunActiveService(
      category, stop_key, &Start, &OnStop, &OnReady, &callbacks);
  if (!callbacks.start_status.ok()) {
    return callbacks.start_status;
  }
  return symbian::StatusFromNativeError(result, "Active service");
}

absl::Status RequestActiveServiceStop(std::int32_t category,
                                      std::uint32_t stop_key) {
  return symbian::StatusFromNativeError(
      SymbianDeviceRequestActiveServiceStop(category, stop_key),
      "Active service stop request");
}

void StopActiveService() {
  SymbianDeviceStopActiveService();
}

}  // namespace symbian::api::system
