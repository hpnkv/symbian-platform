// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.
// Keep Symbian's TInt result contract at the OS boundary. Application logic
// uses the original guest Abseil Status and StatusOr types.

#ifndef SYMBIAN_NATIVE_STATUS_H_
#define SYMBIAN_NATIVE_STATUS_H_

#include <cerrno>
#include <climits>
#include <cstdlib>
#include <string>

#include <absl/base/nullability.h>

#include "absl/status/status.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"

namespace symbian {

// Frozen system-wide TInt values from the original e32err.h contract.
// Keep them in this boundary header so application logic need not include
// the old platform declarations beside modern libc++ headers.
namespace native_error {
inline constexpr int kNone = 0;
inline constexpr int kNotFound = -1;
inline constexpr int kGeneral = -2;
inline constexpr int kCancel = -3;
inline constexpr int kNoMemory = -4;
inline constexpr int kNotSupported = -5;
inline constexpr int kArgument = -6;
inline constexpr int kAlreadyExists = -11;
inline constexpr int kInUse = -14;
inline constexpr int kNotReady = -18;
inline constexpr int kAccessDenied = -21;
inline constexpr int kTimedOut = -33;
inline constexpr int kPermissionDenied = -46;
}  // namespace native_error

inline constexpr absl::string_view kNativeErrorPayload =
    "symbian.sdk/native-error";

inline absl::Status StatusFromNativeError(int native_code,
                                          absl::string_view operation) {
  if (native_code == native_error::kNone) {
    return absl::OkStatus();
  }
  absl::StatusCode code = absl::StatusCode::kInternal;
  switch (native_code) {
    case native_error::kNotFound:
      code = absl::StatusCode::kNotFound;
      break;
    case native_error::kCancel:
      code = absl::StatusCode::kCancelled;
      break;
    case native_error::kNoMemory:
      code = absl::StatusCode::kResourceExhausted;
      break;
    case native_error::kNotSupported:
      code = absl::StatusCode::kUnimplemented;
      break;
    case native_error::kArgument:
      code = absl::StatusCode::kInvalidArgument;
      break;
    case native_error::kAlreadyExists:
      code = absl::StatusCode::kAlreadyExists;
      break;
    case native_error::kNotReady:
    case native_error::kInUse:
      code = absl::StatusCode::kFailedPrecondition;
      break;
    case native_error::kAccessDenied:
    case native_error::kPermissionDenied:
      code = absl::StatusCode::kPermissionDenied;
      break;
    case native_error::kTimedOut:
      code = absl::StatusCode::kDeadlineExceeded;
      break;
  }
  std::string message(operation);
  message += ": native error ";
  message += std::to_string(native_code);
  absl::Status result(code, message);
  result.SetPayload(kNativeErrorPayload,
                    absl::Cord(absl::string_view(std::to_string(native_code))));
  return result;
}

inline int NativeErrorFromStatus(const absl::Status& status) {
  if (status.ok()) {
    return native_error::kNone;
  }
  if (auto payload = status.GetPayload(kNativeErrorPayload)) {
    const std::string text(payload->Flatten());
    char* absl_nullable end = nullptr;
    errno = 0;
    if (const long value = std::strtol(text.c_str(), &end, 10);
        errno == 0 && end != text.c_str() && *end == '\0' && value >= INT_MIN &&
        value <= INT_MAX && value < 0) {
      return static_cast<int>(value);
    }
  }
  switch (status.code()) {
    case absl::StatusCode::kCancelled:
      return native_error::kCancel;
    case absl::StatusCode::kNotFound:
      return native_error::kNotFound;
    case absl::StatusCode::kInvalidArgument:
      return native_error::kArgument;
    case absl::StatusCode::kDeadlineExceeded:
      return native_error::kTimedOut;
    case absl::StatusCode::kAlreadyExists:
      return native_error::kAlreadyExists;
    case absl::StatusCode::kPermissionDenied:
      return native_error::kPermissionDenied;
    case absl::StatusCode::kResourceExhausted:
      return native_error::kNoMemory;
    case absl::StatusCode::kFailedPrecondition:
      return native_error::kNotReady;
    case absl::StatusCode::kUnimplemented:
      return native_error::kNotSupported;
    default:
      return native_error::kGeneral;
  }
}

}  // namespace symbian

#endif  // SYMBIAN_NATIVE_STATUS_H_
