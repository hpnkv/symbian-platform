// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "native_tcp_client.h"

#include <es_sock.h>
#include <in_sock.h>

namespace symbian::api::connectivity {

struct NativeTcpClient {
  RSocketServ server;
  RSocket socket;
  bool server_open = false;
  bool socket_open = false;
};

extern "C" void SymbianDeviceTcpClose(NativeTcpClient* client) {
  if (client == nullptr) {
    return;
  }
  if (client->socket_open) {
    client->socket.Close();
  }
  if (client->server_open) {
    client->server.Close();
  }
  client->~NativeTcpClient();
  User::Free(client);
}

extern "C" int SymbianDeviceTcpConnect(unsigned address, unsigned port,
                                       NativeTcpClient** output) {
  if (output == nullptr || port == 0 || port > 65535) {
    return KErrArgument;
  }
  *output = nullptr;
  void* memory = User::Alloc(sizeof(NativeTcpClient));
  if (memory == nullptr) {
    return KErrNoMemory;
  }
  auto* client = new (memory) NativeTcpClient;
  TInt result = client->server.Connect();
  if (result != KErrNone) {
    SymbianDeviceTcpClose(client);
    return result;
  }
  client->server_open = true;
  result = client->socket.Open(client->server, KAfInet, KSockStream,
                               KProtocolInetTcp);
  if (result != KErrNone) {
    SymbianDeviceTcpClose(client);
    return result;
  }
  client->socket_open = true;
  TInetAddr peer(address, port);
  TRequestStatus request;
  client->socket.Connect(peer, request);
  User::WaitForRequest(request);
  result = request.Int();
  if (result != KErrNone) {
    SymbianDeviceTcpClose(client);
    return result;
  }
  *output = client;
  return KErrNone;
}

extern "C" int SymbianDeviceTcpSend(NativeTcpClient* client,
                                    const unsigned char* bytes, int length) {
  if (client == nullptr || bytes == nullptr || length <= 0 || length > 32768) {
    return KErrArgument;
  }
  TPtrC8 data(bytes, length);
  TRequestStatus request;
  client->socket.Send(data, 0, request);
  User::WaitForRequest(request);
  return request.Int();
}

extern "C" int SymbianDeviceTcpReceive(NativeTcpClient* client,
                                       unsigned char* bytes, int capacity,
                                       int* received) {
  if (client == nullptr || bytes == nullptr || capacity <= 0 ||
      capacity > 32768 || received == nullptr) {
    return KErrArgument;
  }
  *received = 0;
  TPtr8 data(bytes, 0, capacity);
  TRequestStatus request;
  client->socket.RecvOneOrMore(data, 0, request);
  User::WaitForRequest(request);
  if (request.Int() != KErrNone) {
    return request.Int();
  }
  *received = data.Length();
  return KErrNone;
}

}  // namespace symbian::api::connectivity
