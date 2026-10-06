// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_API_SYSTEM_NATIVE_ACTIVE_SERVICE_H_
#define SYMBIAN_API_SYSTEM_NATIVE_ACTIVE_SERVICE_H_

#include <absl/base/nullability.h>

extern "C" int SymbianDeviceRunActiveService(
    int category, unsigned key, int (*absl_nonnull start)(void* absl_nullable),
    void (*absl_nonnull on_stop)(void* absl_nullable),
    void (*absl_nonnull on_ready)(void* absl_nullable),
    void* absl_nullable context);
extern "C" int SymbianDeviceRequestActiveServiceStop(int category,
                                                     unsigned key);
extern "C" void SymbianDeviceStopActiveService();

#endif  // SYMBIAN_API_SYSTEM_NATIVE_ACTIVE_SERVICE_H_
