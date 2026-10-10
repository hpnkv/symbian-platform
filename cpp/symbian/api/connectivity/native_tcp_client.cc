// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "native_tcp_client.h"

#include <absl/base/nullability.h>
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
  NativeTcpSession* absl_nullable session = nullptr;
  RSocket socket;
  bool socket_open = false;
};

struct NativeTcpListener {
  NativeTcpSession* absl_nullable session = nullptr;
  RSocket socket;
  bool socket_open = false;
};

namespace {

void ReleaseSession(NativeTcpSession* absl_nullable session) {
  if (session != nullptr &&
      __e32_atomic_add_ord32(&session->references, 0xffffffffU) == 1) {
    session->server.Close();
    session->~NativeTcpSession();
    User::Free(session);
  }
}

void RetainSession(NativeTcpSession* absl_nonnull session) {
  __e32_atomic_add_ord32(&session->references, 1);
}

int OpenSession(NativeTcpSession* absl_nullable* absl_nonnull output) {
  void* absl_nullable memory = User::Alloc(sizeof(NativeTcpSession));
  if (memory == nullptr) {
    return KErrNoMemory;
  }
  auto* absl_nonnull session = new (memory) NativeTcpSession;
  if (const TInt result = session->server.Connect(); result != KErrNone) {
    session->~NativeTcpSession();
    User::Free(session);
    return result;
  }
  *output = session;
  return KErrNone;
}

}  // namespace

extern "C" int SymbianDeviceResolveIpv4(const char* absl_nullable hostname,
                                        int length,
                                        unsigned* absl_nullable address,
                                        std::int64_t deadline) {
  if (!hostname || !address || length < 1 || length > 253) {
    return KErrArgument;
  }
  RSocketServ server;
  TInt result = server.Connect();
  if (result != KErrNone) {
    return result;
  }
  RHostResolver resolver;
  result = resolver.Open(server, KAfInet, KProtocolInetTcp);
  if (result == KErrNone) {
    TUint16 text[253];
    for (int i = 0; i < length; ++i) {
      text[i] = static_cast<TUint8>(hostname[i]);
    }
    TPtrC16 name(text, length);
    TNameEntry entry;
    TRequestStatus request;
    resolver.GetByName(name, entry, request);
    result = WaitForResolverRequest(&resolver, &request, deadline);
    if (result == KErrNone) {
      *address = TInetAddr::Cast(entry().iAddr).Address();
    }
    resolver.Close();
  }
  server.Close();
  return result;
}

extern "C" int SymbianDeviceTcpSetNoDelay(NativeTcpClient* absl_nullable client,
                                          bool enabled) {
  if (client == nullptr || !client->socket_open) {
    return KErrBadHandle;
  }
  return client->socket.SetOpt(KSoTcpNoDelay, KSolInetTcp, enabled ? 1 : 0);
}

extern "C" void SymbianDeviceTcpClose(NativeTcpClient* absl_nullable client) {
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

extern "C" int SymbianDeviceTcpConnect(
    unsigned address, unsigned port,
    NativeTcpClient* absl_nullable* absl_nullable output,
    std::int64_t deadline) {
  if (output == nullptr || port == 0 || port > 65535) {
    return KErrArgument;
  }
  *output = nullptr;
  void* absl_nullable memory = User::Alloc(sizeof(NativeTcpClient));
  if (memory == nullptr) {
    return KErrNoMemory;
  }
  auto* absl_nonnull client = new (memory) NativeTcpClient;
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
  result = WaitForSocketRequest(&(client->socket), &request, deadline,
                                &RSocket::CancelConnect);
  if (result != KErrNone) {
    SymbianDeviceTcpClose(client);
    return result;
  }
  *output = client;
  return KErrNone;
}

extern "C" void SymbianDeviceTcpListenerClose(
    NativeTcpListener* absl_nullable listener) {
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

extern "C" int SymbianDeviceTcpListen(
    unsigned address, unsigned port, bool share_with_workers,
    NativeTcpListener* absl_nullable* absl_nullable output) {
  if (output == nullptr || port == 0 || port > 65535) {
    return KErrArgument;
  }
  *output = nullptr;
  void* absl_nullable memory = User::Alloc(sizeof(NativeTcpListener));
  if (memory == nullptr) {
    return KErrNoMemory;
  }
  auto* absl_nonnull listener = new (memory) NativeTcpListener;
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

extern "C" int SymbianDeviceTcpBeginAccept(
    NativeTcpListener* absl_nullable listener,
    TRequestStatus* absl_nullable status,
    NativeTcpClient* absl_nullable* absl_nullable output) {
  if (listener == nullptr || status == nullptr || output == nullptr) {
    return KErrArgument;
  }
  *output = nullptr;
  void* absl_nullable memory = User::Alloc(sizeof(NativeTcpClient));
  if (memory == nullptr) {
    return KErrNoMemory;
  }
  auto* absl_nonnull client = new (memory) NativeTcpClient;
  client->session = listener->session;
  RetainSession(client->session);
  if (const TInt result = client->socket.Open(client->session->server);
      result != KErrNone) {
    SymbianDeviceTcpClose(client);
    return result;
  }
  client->socket_open = true;
  listener->socket.Accept(client->socket, *status);
  *output = client;
  return KErrNone;
}

extern "C" void SymbianDeviceTcpCancelAccept(
    NativeTcpListener* absl_nullable listener) {
  if (listener != nullptr) {
    listener->socket.CancelAccept();
  }
}

extern "C" int SymbianDeviceTcpAccept(
    NativeTcpListener* absl_nullable listener,
    NativeTcpClient* absl_nullable* absl_nullable output,
    std::int64_t deadline) {
  if (listener == nullptr || output == nullptr) {
    return KErrArgument;
  }
  *output = nullptr;
  void* absl_nullable memory = User::Alloc(sizeof(NativeTcpClient));
  if (memory == nullptr) {
    return KErrNoMemory;
  }
  auto* absl_nonnull client = new (memory) NativeTcpClient;
  client->session = listener->session;
  RetainSession(client->session);
  TInt result = client->socket.Open(client->session->server);
  if (result == KErrNone) {
    client->socket_open = true;
    TRequestStatus request;
    listener->socket.Accept(client->socket, request);
    result = WaitForSocketRequest(&(listener->socket), &request, deadline,
                                  &RSocket::CancelAccept);
  }
  if (result != KErrNone) {
    SymbianDeviceTcpClose(client);
    return result;
  }
  *output = client;
  return KErrNone;
}

extern "C" int SymbianDeviceTcpSend(NativeTcpClient* absl_nullable client,
                                    const unsigned char* absl_nullable bytes,
                                    int length, std::int64_t deadline) {
  if (client == nullptr || bytes == nullptr || length <= 0 || length > 32768) {
    return KErrArgument;
  }
  TPtrC8 data(bytes, length);
  TRequestStatus request;
  client->socket.Send(data, 0, request);
  return WaitForSocketRequest(&(client->socket), &request, deadline,
                              &RSocket::CancelSend);
}

extern "C" int SymbianDeviceTcpReceive(NativeTcpClient* absl_nullable client,
                                       unsigned char* absl_nullable bytes,
                                       int capacity,
                                       int* absl_nullable received,
                                       std::int64_t deadline) {
  if (client == nullptr || bytes == nullptr || capacity <= 0 ||
      capacity > 32768 || received == nullptr) {
    return KErrArgument;
  }
  *received = 0;
  TPtr8 data(bytes, 0, capacity);
  TRequestStatus request;
  client->socket.RecvOneOrMore(data, 0, request);
  if (const TInt result = WaitForSocketRequest(&(client->socket), &request,
                                               deadline, &RSocket::CancelRecv);
      result != KErrNone) {
    return result;
  }
  *received = data.Length();
  return KErrNone;
}

}  // namespace symbian::api::connectivity
