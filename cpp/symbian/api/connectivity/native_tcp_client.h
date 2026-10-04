// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_API_CONNECTIVITY_NATIVE_TCP_CLIENT_H_
#define SYMBIAN_API_CONNECTIVITY_NATIVE_TCP_CLIENT_H_

namespace symbian::api::connectivity {

struct NativeTcpClient;

extern "C" int SymbianDeviceTcpConnect(unsigned address, unsigned port,
                                       NativeTcpClient** output);
extern "C" int SymbianDeviceTcpSend(NativeTcpClient* client,
                                    const unsigned char* bytes, int length);
extern "C" int SymbianDeviceTcpReceive(NativeTcpClient* client,
                                       unsigned char* bytes, int capacity,
                                       int* received);
extern "C" void SymbianDeviceTcpClose(NativeTcpClient* client);

}  // namespace symbian::api::connectivity

#endif  // SYMBIAN_API_CONNECTIVITY_NATIVE_TCP_CLIENT_H_
