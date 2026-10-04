// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "native_tcp_client.h"

#include <e32atomics.h>
#include <es_sock.h>
#include <in_sock.h>

#include "native_deadline.h"

namespace symbian::api::connectivity {

struct NativeTcpSession {
  RSocketServ server;
  TInt references = 1;
};

struct NativeTcpClient {
  NativeTcpSession* session = nullptr;
  RSocket socket;
  bool socket_open = false;
};

struct NativeTcpListener {
  NativeTcpSession* session = nullptr;
  RSocket socket;
  bool socket_open = false;
};

namespace {

void ReleaseSession(NativeTcpSession* session) {
  if (session != nullptr &&
      __e32_atomic_add_ord32(&session->references, 0xffffffffU) == 1) {
    session->server.Close();
    session->~NativeTcpSession();
    User::Free(session);
  }
}

void RetainSession(NativeTcpSession* session) {
  __e32_atomic_add_ord32(&session->references, 1);
}

int OpenSession(NativeTcpSession** output) {
  void* memory = User::Alloc(sizeof(NativeTcpSession));
  if (memory == nullptr) {
    return KErrNoMemory;
  }
  auto* session = new (memory) NativeTcpSession;
  const TInt result = session->server.Connect();
  if (result != KErrNone) {
    session->~NativeTcpSession();
    User::Free(session);
    return result;
  }
  *output = session;
  return KErrNone;
}

}  // namespace

extern "C" void SymbianDeviceTcpClose(NativeTcpClient* client) {
  if (client == nullptr) {
    return;
  }
  if (client->socket_open) {
    client->socket.Close();
  }
  ReleaseSession(client->session);
  client->~NativeTcpClient();
  User::Free(client);
}

extern "C" int SymbianDeviceTcpConnect(unsigned address, unsigned port,
                                       NativeTcpClient** output,
                                       std::int64_t deadline) {
  if (output == nullptr || port == 0 || port > 65535) {
    return KErrArgument;
  }
  *output = nullptr;
  void* memory = User::Alloc(sizeof(NativeTcpClient));
  if (memory == nullptr) {
    return KErrNoMemory;
  }
  auto* client = new (memory) NativeTcpClient;
  TInt result = OpenSession(&client->session);
  if (result != KErrNone) {
    SymbianDeviceTcpClose(client);
    return result;
  }
  result = client->socket.Open(client->session->server, KAfInet, KSockStream,
                               KProtocolInetTcp);
  if (result != KErrNone) {
    SymbianDeviceTcpClose(client);
    return result;
  }
  client->socket_open = true;
  TInetAddr peer(address, port);
  TRequestStatus request;
  client->socket.Connect(peer, request);
  result = WaitForSocketRequest(client->socket, request, deadline,
                                &RSocket::CancelConnect);
  if (result != KErrNone) {
    SymbianDeviceTcpClose(client);
    return result;
  }
  *output = client;
  return KErrNone;
}

extern "C" void SymbianDeviceTcpListenerClose(NativeTcpListener* listener) {
  if (listener == nullptr) {
    return;
  }
  if (listener->socket_open) {
    listener->socket.Close();
  }
  ReleaseSession(listener->session);
  listener->~NativeTcpListener();
  User::Free(listener);
}

extern "C" int SymbianDeviceTcpListen(unsigned address, unsigned port,
                                      bool share_with_workers,
                                      NativeTcpListener** output) {
  if (output == nullptr || port == 0 || port > 65535) {
    return KErrArgument;
  }
  *output = nullptr;
  void* memory = User::Alloc(sizeof(NativeTcpListener));
  if (memory == nullptr) {
    return KErrNoMemory;
  }
  auto* listener = new (memory) NativeTcpListener;
  TInt result = OpenSession(&listener->session);
  if (result == KErrNone && share_with_workers) {
    result = listener->session->server.ShareAuto();
  }
  if (result == KErrNone) {
    result = listener->socket.Open(listener->session->server, KAfInet,
                                   KSockStream, KProtocolInetTcp);
    if (result == KErrNone) {
      listener->socket_open = true;
      TInetAddr local(address, port);
      result = listener->socket.Bind(local);
      if (result == KErrNone) {
        result = listener->socket.Listen(1);
      }
    }
  }
  if (result != KErrNone) {
    SymbianDeviceTcpListenerClose(listener);
    return result;
  }
  *output = listener;
  return KErrNone;
}

extern "C" int SymbianDeviceTcpBeginAccept(NativeTcpListener* listener,
                                           TRequestStatus* status,
                                           NativeTcpClient** output) {
  if (listener == nullptr || status == nullptr || output == nullptr) {
    return KErrArgument;
  }
  *output = nullptr;
  void* memory = User::Alloc(sizeof(NativeTcpClient));
  if (memory == nullptr) {
    return KErrNoMemory;
  }
  auto* client = new (memory) NativeTcpClient;
  client->session = listener->session;
  RetainSession(client->session);
  const TInt result = client->socket.Open(client->session->server);
  if (result != KErrNone) {
    SymbianDeviceTcpClose(client);
    return result;
  }
  client->socket_open = true;
  listener->socket.Accept(client->socket, *status);
  *output = client;
  return KErrNone;
}

extern "C" void SymbianDeviceTcpCancelAccept(NativeTcpListener* listener) {
  if (listener != nullptr) {
    listener->socket.CancelAccept();
  }
}

extern "C" int SymbianDeviceTcpAccept(NativeTcpListener* listener,
                                      NativeTcpClient** output,
                                      std::int64_t deadline) {
  if (listener == nullptr || output == nullptr) {
    return KErrArgument;
  }
  *output = nullptr;
  void* memory = User::Alloc(sizeof(NativeTcpClient));
  if (memory == nullptr) {
    return KErrNoMemory;
  }
  auto* client = new (memory) NativeTcpClient;
  client->session = listener->session;
  RetainSession(client->session);
  TInt result = client->socket.Open(client->session->server);
  if (result == KErrNone) {
    client->socket_open = true;
    TRequestStatus request;
    listener->socket.Accept(client->socket, request);
    result = WaitForSocketRequest(listener->socket, request, deadline,
                                  &RSocket::CancelAccept);
  }
  if (result != KErrNone) {
    SymbianDeviceTcpClose(client);
    return result;
  }
  *output = client;
  return KErrNone;
}

extern "C" int SymbianDeviceTcpSend(NativeTcpClient* client,
                                    const unsigned char* bytes, int length,
                                    std::int64_t deadline) {
  if (client == nullptr || bytes == nullptr || length <= 0 || length > 32768) {
    return KErrArgument;
  }
  TPtrC8 data(bytes, length);
  TRequestStatus request;
  client->socket.Send(data, 0, request);
  return WaitForSocketRequest(client->socket, request, deadline,
                              &RSocket::CancelSend);
}

extern "C" int SymbianDeviceTcpReceive(NativeTcpClient* client,
                                       unsigned char* bytes, int capacity,
                                       int* received, std::int64_t deadline) {
  if (client == nullptr || bytes == nullptr || capacity <= 0 ||
      capacity > 32768 || received == nullptr) {
    return KErrArgument;
  }
  *received = 0;
  TPtr8 data(bytes, 0, capacity);
  TRequestStatus request;
  client->socket.RecvOneOrMore(data, 0, request);
  const TInt result = WaitForSocketRequest(client->socket, request, deadline,
                                           &RSocket::CancelRecv);
  if (result != KErrNone) {
    return result;
  }
  *received = data.Length();
  return KErrNone;
}

}  // namespace symbian::api::connectivity
