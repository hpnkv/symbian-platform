// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_API_CONNECTIVITY_NATIVE_TCP_CLIENT_H_
#define SYMBIAN_API_CONNECTIVITY_NATIVE_TCP_CLIENT_H_

#include <cstdint>

#include <limits.h>

class TRequestStatus;

namespace symbian::api::connectivity {

struct NativeTcpClient;
struct NativeTcpListener;
struct NativeActiveTcpListener;

using NativeAcceptCallback = void (*)(void* context, NativeTcpClient* accepted,
                                      int result);

extern "C" int SymbianDeviceTcpConnect(unsigned address, unsigned port,
                                       NativeTcpClient** output,
                                       std::int64_t deadline = INT64_MAX);
extern "C" int SymbianDeviceTcpSend(NativeTcpClient* client,
                                    const unsigned char* bytes, int length,
                                    std::int64_t deadline = INT64_MAX);
extern "C" int SymbianDeviceTcpReceive(NativeTcpClient* client,
                                       unsigned char* bytes, int capacity,
                                       int* received,
                                       std::int64_t deadline = INT64_MAX);
extern "C" void SymbianDeviceTcpClose(NativeTcpClient* client);
extern "C" int SymbianDeviceTcpListen(unsigned address, unsigned port,
                                      bool share_with_workers,
                                      NativeTcpListener** output);
extern "C" int SymbianDeviceTcpAccept(NativeTcpListener* listener,
                                      NativeTcpClient** output,
                                      std::int64_t deadline = INT64_MAX);
extern "C" int SymbianDeviceTcpBeginAccept(NativeTcpListener* listener,
                                           TRequestStatus* status,
                                           NativeTcpClient** output);
extern "C" void SymbianDeviceTcpCancelAccept(NativeTcpListener* listener);
extern "C" void SymbianDeviceTcpListenerClose(NativeTcpListener* listener);
extern "C" int SymbianDeviceActiveTcpListen(unsigned address, unsigned port,
                                            bool share_with_workers,
                                            void* context,
                                            NativeAcceptCallback callback,
                                            NativeActiveTcpListener** output);
extern "C" int SymbianDeviceActiveTcpAcceptNext(
    NativeActiveTcpListener* listener);
extern "C" void SymbianDeviceActiveTcpClose(NativeActiveTcpListener* listener);

}  // namespace symbian::api::connectivity

#endif  // SYMBIAN_API_CONNECTIVITY_NATIVE_TCP_CLIENT_H_
