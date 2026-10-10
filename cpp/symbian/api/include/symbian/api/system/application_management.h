// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_API_SYSTEM_APPLICATION_MANAGEMENT_H_
#define SYMBIAN_API_SYSTEM_APPLICATION_MANAGEMENT_H_

#include <cstdint>
#include <string_view>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace symbian::api::system {

/** @brief Ask the OS to open a document with its registered application. */
absl::Status OpenDocument(std::u16string_view absolute_path);

/** @brief Check whether AppArc currently knows an application UID. */
absl::StatusOr<bool> IsApplicationRegistered(std::uint32_t uid);

}  // namespace symbian::api::system

#endif  // SYMBIAN_API_SYSTEM_APPLICATION_MANAGEMENT_H_
