// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_API_CONNECTIVITY_NATIVE_TCP_CLIENT_H_
#define SYMBIAN_API_CONNECTIVITY_NATIVE_TCP_CLIENT_H_

class TRequestStatus;

namespace symbian::api::connectivity {

struct NativeTcpClient;
struct NativeTcpListener;
struct NativeActiveTcpListener;

using NativeAcceptCallback = void (*)(void* context, NativeTcpClient* accepted,
                                      int result);

extern "C" int SymbianDeviceTcpConnect(unsigned address, unsigned port,
                                       NativeTcpClient** output);
extern "C" int SymbianDeviceTcpSend(NativeTcpClient* client,
                                    const unsigned char* bytes, int length);
extern "C" int SymbianDeviceTcpSendFor(NativeTcpClient* client,
                                       const unsigned char* bytes, int length,
                                       int milliseconds);
extern "C" int SymbianDeviceTcpReceive(NativeTcpClient* client,
                                       unsigned char* bytes, int capacity,
                                       int* received);
extern "C" int SymbianDeviceTcpReceiveFor(NativeTcpClient* client,
                                          unsigned char* bytes, int capacity,
                                          int milliseconds, int* received);
extern "C" void SymbianDeviceTcpClose(NativeTcpClient* client);
extern "C" int SymbianDeviceTcpListen(unsigned address, unsigned port,
                                      NativeTcpListener** output);
extern "C" int SymbianDeviceTcpAccept(NativeTcpListener* listener,
                                      NativeTcpClient** output);
extern "C" int SymbianDeviceTcpAcceptFor(NativeTcpListener* listener,
                                         int milliseconds,
                                         NativeTcpClient** output);
extern "C" int SymbianDeviceTcpBeginAccept(NativeTcpListener* listener,
                                           TRequestStatus* status,
                                           NativeTcpClient** output);
extern "C" void SymbianDeviceTcpCancelAccept(NativeTcpListener* listener);
extern "C" void SymbianDeviceTcpListenerClose(NativeTcpListener* listener);
extern "C" int SymbianDeviceActiveTcpListen(unsigned address, unsigned port,
                                            void* context,
                                            NativeAcceptCallback callback,
                                            NativeActiveTcpListener** output);
extern "C" int SymbianDeviceActiveTcpAcceptNext(
    NativeActiveTcpListener* listener);
extern "C" void SymbianDeviceActiveTcpClose(NativeActiveTcpListener* listener);

}  // namespace symbian::api::connectivity

#endif  // SYMBIAN_API_CONNECTIVITY_NATIVE_TCP_CLIENT_H_
