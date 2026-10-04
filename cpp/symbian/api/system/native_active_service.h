// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_API_SYSTEM_NATIVE_ACTIVE_SERVICE_H_
#define SYMBIAN_API_SYSTEM_NATIVE_ACTIVE_SERVICE_H_

extern "C" int SymbianDeviceRunActiveService(
    int category, unsigned key, int (*start)(void*),
    void (*on_stop)(void*), void (*on_ready)(void*), void* context);
extern "C" int SymbianDeviceRequestActiveServiceStop(int category,
                                                      unsigned key);
extern "C" void SymbianDeviceStopActiveService();

#endif  // SYMBIAN_API_SYSTEM_NATIVE_ACTIVE_SERVICE_H_
