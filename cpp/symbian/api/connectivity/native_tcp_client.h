// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_API_CONNECTIVITY_NATIVE_TCP_CLIENT_H_
#define SYMBIAN_API_CONNECTIVITY_NATIVE_TCP_CLIENT_H_

#include <cstdint>

#include <absl/base/nullability.h>
#include <limits.h>

class TRequestStatus;

namespace symbian::api::connectivity {

struct NativeTcpClient;
struct NativeTcpListener;
struct NativeActiveTcpListener;

using NativeAcceptCallback =
    void (*absl_nonnull)(void* absl_nullable context,
                         NativeTcpClient* absl_nullable accepted, int result);

extern "C" int SymbianDeviceResolveIpv4(const char* absl_nullable hostname,
                                        int length,
                                        unsigned* absl_nullable address,
                                        std::int64_t deadline);
extern "C" int SymbianDeviceTcpConnect(
    unsigned address, unsigned port,
    NativeTcpClient* absl_nullable* absl_nullable output,
    std::int64_t deadline = INT64_MAX);
extern "C" int SymbianDeviceTcpSend(NativeTcpClient* absl_nullable client,
                                    const unsigned char* absl_nullable bytes,
                                    int length,
                                    std::int64_t deadline = INT64_MAX);
extern "C" int SymbianDeviceTcpReceive(NativeTcpClient* absl_nullable client,
                                       unsigned char* absl_nullable bytes,
                                       int capacity,
                                       int* absl_nullable received,
                                       std::int64_t deadline = INT64_MAX);
extern "C" int SymbianDeviceTcpSetNoDelay(NativeTcpClient* absl_nullable client,
                                          bool enabled);
extern "C" void SymbianDeviceTcpClose(NativeTcpClient* absl_nullable client);
extern "C" int SymbianDeviceTcpListen(
    unsigned address, unsigned port, bool share_with_workers,
    NativeTcpListener* absl_nullable* absl_nullable output);
extern "C" int SymbianDeviceTcpAccept(
    NativeTcpListener* absl_nullable listener,
    NativeTcpClient* absl_nullable* absl_nullable output,
    std::int64_t deadline = INT64_MAX);
extern "C" int SymbianDeviceTcpBeginAccept(
    NativeTcpListener* absl_nullable listener,
    TRequestStatus* absl_nullable status,
    NativeTcpClient* absl_nullable* absl_nullable output);
extern "C" void SymbianDeviceTcpCancelAccept(
    NativeTcpListener* absl_nullable listener);
extern "C" void SymbianDeviceTcpListenerClose(
    NativeTcpListener* absl_nullable listener);
extern "C" int SymbianDeviceActiveTcpListen(
    unsigned address, unsigned port, bool share_with_workers,
    void* absl_nullable context, NativeAcceptCallback callback,
    NativeActiveTcpListener* absl_nullable* absl_nullable output);
extern "C" int SymbianDeviceActiveTcpAcceptNext(
    NativeActiveTcpListener* absl_nullable listener);
extern "C" void SymbianDeviceActiveTcpClose(
    NativeActiveTcpListener* absl_nullable listener);

}  // namespace symbian::api::connectivity

#endif  // SYMBIAN_API_CONNECTIVITY_NATIVE_TCP_CLIENT_H_
