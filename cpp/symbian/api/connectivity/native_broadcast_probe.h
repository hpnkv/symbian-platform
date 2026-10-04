// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_API_CONNECTIVITY_NATIVE_BROADCAST_PROBE_H_
#define SYMBIAN_API_CONNECTIVITY_NATIVE_BROADCAST_PROBE_H_

#include <cstdint>

#include <limits.h>

extern "C" int SymbianDeviceBroadcastProbe(
    unsigned port, const unsigned char* request, int request_length,
    const unsigned char* expected_reply, int expected_length, unsigned* address,
    std::int64_t deadline = INT64_MAX);

#endif  // SYMBIAN_API_CONNECTIVITY_NATIVE_BROADCAST_PROBE_H_
