// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_API_CONNECTIVITY_NATIVE_DEADLINE_H_
#define SYMBIAN_API_CONNECTIVITY_NATIVE_DEADLINE_H_

#include <cstdint>

#include <absl/base/nullability.h>

class RSocket;
class RHostResolver;
class TRequestStatus;

namespace symbian::api::connectivity {

// The deadline is Unix microseconds; INT64_MAX means no deadline. Only the
// absolute timestamp needs 64 bits. Native timer intervals remain 32-bit.
// On expiry, cancel and drain before the caller releases request buffers.
int WaitForSocketRequest(RSocket* absl_nonnull socket,
                         TRequestStatus* absl_nonnull request,
                         std::int64_t deadline,
                         void (RSocket::* absl_nonnull cancel)());

int WaitForResolverRequest(RHostResolver* absl_nonnull resolver,
                           TRequestStatus* absl_nonnull request,
                           std::int64_t deadline);

}  // namespace symbian::api::connectivity

#endif  // SYMBIAN_API_CONNECTIVITY_NATIVE_DEADLINE_H_
