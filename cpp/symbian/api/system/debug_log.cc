// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "symbian/api/system/debug_log.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <limits>

#include <absl/base/nullability.h>
#include <e32debug.h>

namespace symbian::api::system {
namespace {

constexpr std::size_t kLogCapacity = 16 * 1024;
std::array<char, kLogCapacity> recent_logs;
std::atomic_flag log_lock = ATOMIC_FLAG_INIT;
std::size_t log_next = 0;
std::size_t log_size = 0;

void LockLogs() {
  while (log_lock.test_and_set(std::memory_order_acquire)) {}
}

void UnlockLogs() {
  log_lock.clear(std::memory_order_release);
}

void AppendLog(std::string_view message) {
  LockLogs();
  for (const char character : message) {
    recent_logs[log_next] = character;
    log_next = (log_next + 1) % kLogCapacity;
    log_size = std::min(log_size + 1, kLogCapacity);
  }
  recent_logs[log_next] = '\n';
  log_next = (log_next + 1) % kLogCapacity;
  log_size = std::min(log_size + 1, kLogCapacity);
  UnlockLogs();
}

}  // namespace

void DebugLog(std::string_view message) {
  if (message.empty()) {
    return;
  }
  AppendLog(message);
  const auto* absl_nonnull bytes =
      reinterpret_cast<const TUint8*>(message.data());
  const auto length =
      std::min(message.size(),
               static_cast<std::size_t>(std::numeric_limits<TInt>::max()));
  const TPtrC8 descriptor(bytes, static_cast<TInt>(length));
  RDebug::RawPrint(descriptor);
}

std::size_t CopyRecentDebugLogs(std::span<char> destination) {
  LockLogs();
  const std::size_t count = std::min(destination.size(), log_size);
  std::size_t source = (log_next + kLogCapacity - count) % kLogCapacity;
  for (std::size_t index = 0; index < count; ++index) {
    destination[index] = recent_logs[source];
    source = (source + 1) % kLogCapacity;
  }
  UnlockLogs();
  return count;
}

}  // namespace symbian::api::system
