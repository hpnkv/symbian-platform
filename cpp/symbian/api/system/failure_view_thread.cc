// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "failure_view_thread.h"

#include <memory>

#include <absl/base/nullability.h>

namespace std {
bool uncaught_exception();
}

#define __EXCEPTION__
#include <e32base.h>
#undef __EXCEPTION__

#include "symbian/native_status.h"

namespace symbian::api::system::internal {

absl::Status RunFailureView(const std::function<absl::Status()>& present) {
  std::unique_ptr<CTrapCleanup> cleanup(CTrapCleanup::New());
  if (cleanup == nullptr) {
    return absl::ResourceExhaustedError("Failure view cleanup allocation");
  }
  absl::Status status;
  TRAPD(error, status = present());
  return error == KErrNone
             ? status
             : symbian::StatusFromNativeError(error, "Failure view");
}

}  // namespace symbian::api::system::internal
