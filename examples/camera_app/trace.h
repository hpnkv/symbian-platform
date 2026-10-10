// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef EXAMPLES_CAMERA_APP_TRACE_H_
#define EXAMPLES_CAMERA_APP_TRACE_H_

#include <string_view>

#include "absl/status/status.h"

namespace camera_app {

// Temporary phone diagnostic. Storage is owned by the SDK File API.
void OpenTrace();
void CloseTrace();
bool TraceReady();
void Trace(std::string_view message);
void RecordFailure(absl::Status error);
absl::Status FailureStatus();

}  // namespace camera_app

#endif  // EXAMPLES_CAMERA_APP_TRACE_H_
