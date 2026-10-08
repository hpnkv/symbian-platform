// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include <absl/base/nullability.h>

// The older no-Open-C process profile has no inherited C environment. Callers
// such as Abseil's time-zone loader use their documented default when absent.
extern "C" char* absl_nullable getenv(const char* absl_nonnull) {
  return nullptr;
}
