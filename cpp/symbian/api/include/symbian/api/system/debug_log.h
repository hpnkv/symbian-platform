// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_API_SYSTEM_DEBUG_LOG_H_
#define SYMBIAN_API_SYSTEM_DEBUG_LOG_H_

#include <cstddef>
#include <span>
#include <string_view>

namespace symbian::api::system {

// Writes a diagnostic byte string to the platform debug sink. This function
// does not allocate, buffer, or promise that a sink is present on the device.
void DebugLog(std::string_view message);

// Copies the newest complete or partial diagnostic lines into destination.
// Returns the number of bytes copied in chronological order. The fixed-size
// process ring captures DebugLog calls even when no platform sink is present.
std::size_t CopyRecentDebugLogs(std::span<char> destination);

}  // namespace symbian::api::system

#endif  // SYMBIAN_API_SYSTEM_DEBUG_LOG_H_
