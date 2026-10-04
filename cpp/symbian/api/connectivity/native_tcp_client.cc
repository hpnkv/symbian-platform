// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "native_tcp_client.h"

#include <es_sock.h>
#include <in_sock.h>

namespace symbian::api::connectivity {

struct NativeTcpSession {
  RSocketServ server;
  int references = 1;
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
  if (session != nullptr && --session->references == 0) {
    session->server.Close();
    session->~NativeTcpSession();
    User::Free(session);
  }
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

int WaitForSocketRequest(RSocket& socket, TRequestStatus& request,
                         int milliseconds, void (RSocket::*cancel)()) {
  if (milliseconds < 0) {
    User::WaitForRequest(request);
    return request.Int();
  }
  RTimer timer;
  const TInt timer_result = timer.CreateLocal();
  if (timer_result != KErrNone) {
    (socket.*cancel)();
    User::WaitForRequest(request);
    return timer_result;
  }
  TRequestStatus deadline;
  timer.After(deadline, TTimeIntervalMicroSeconds32(milliseconds * 1000));
  User::WaitForRequest(request, deadline);
  TInt result = request.Int();
  if (result == KRequestPending) {
    (socket.*cancel)();
    User::WaitForRequest(request);
    result = KErrTimedOut;
  }
  timer.Cancel();
  if (deadline.Int() == KRequestPending) {
    User::WaitForRequest(deadline);
  }
  timer.Close();
  return result;
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
  User::WaitForRequest(request);
  result = request.Int();
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

extern "C" int SymbianDeviceTcpAccept(NativeTcpListener* listener,
                                      NativeTcpClient** output) {
  return SymbianDeviceTcpAcceptFor(listener, -1, output);
}

extern "C" int SymbianDeviceTcpAcceptFor(NativeTcpListener* listener,
                                         int milliseconds,
                                         NativeTcpClient** output) {
  if (listener == nullptr || output == nullptr || milliseconds < -1 ||
      milliseconds > 60000) {
    return KErrArgument;
  }
  *output = nullptr;
  void* memory = User::Alloc(sizeof(NativeTcpClient));
  if (memory == nullptr) {
    return KErrNoMemory;
  }
  auto* client = new (memory) NativeTcpClient;
  client->session = listener->session;
  ++client->session->references;
  RTimer timer;
  if (milliseconds >= 0) {
    const TInt timer_result = timer.CreateLocal();
    if (timer_result != KErrNone) {
      SymbianDeviceTcpClose(client);
      return timer_result;
    }
  }
  TInt result = client->socket.Open(client->session->server);
  if (result == KErrNone) {
    client->socket_open = true;
    TRequestStatus request;
    listener->socket.Accept(client->socket, request);
    if (milliseconds < 0) {
      User::WaitForRequest(request);
      result = request.Int();
    } else {
      TRequestStatus deadline;
      timer.After(deadline, TTimeIntervalMicroSeconds32(milliseconds * 1000));
      User::WaitForRequest(request, deadline);
      if (request.Int() == KRequestPending) {
        listener->socket.CancelAccept();
        User::WaitForRequest(request);
        result = KErrTimedOut;
      } else {
        result = request.Int();
      }
      timer.Cancel();
      if (deadline.Int() == KRequestPending) {
        User::WaitForRequest(deadline);
      }
    }
  }
  if (milliseconds >= 0) {
    timer.Close();
  }
  if (result != KErrNone) {
    SymbianDeviceTcpClose(client);
    return result;
  }
  *output = client;
  return KErrNone;
}

extern "C" int SymbianDeviceTcpSend(NativeTcpClient* client,
                                    const unsigned char* bytes, int length) {
  return SymbianDeviceTcpSendFor(client, bytes, length, -1);
}

extern "C" int SymbianDeviceTcpSendFor(NativeTcpClient* client,
                                       const unsigned char* bytes, int length,
                                       int milliseconds) {
  if (client == nullptr || bytes == nullptr || length <= 0 || length > 32768 ||
      milliseconds < -1 || milliseconds > 60000) {
    return KErrArgument;
  }
  TPtrC8 data(bytes, length);
  TRequestStatus request;
  client->socket.Send(data, 0, request);
  return WaitForSocketRequest(client->socket, request, milliseconds,
                              &RSocket::CancelSend);
}

extern "C" int SymbianDeviceTcpReceive(NativeTcpClient* client,
                                       unsigned char* bytes, int capacity,
                                       int* received) {
  return SymbianDeviceTcpReceiveFor(client, bytes, capacity, -1, received);
}

extern "C" int SymbianDeviceTcpReceiveFor(NativeTcpClient* client,
                                          unsigned char* bytes, int capacity,
                                          int milliseconds, int* received) {
  if (client == nullptr || bytes == nullptr || capacity <= 0 ||
      capacity > 32768 || received == nullptr || milliseconds < -1 ||
      milliseconds > 60000) {
    return KErrArgument;
  }
  *received = 0;
  TPtr8 data(bytes, 0, capacity);
  TRequestStatus request;
  client->socket.RecvOneOrMore(data, 0, request);
  const TInt result = WaitForSocketRequest(client->socket, request,
                                           milliseconds, &RSocket::CancelRecv);
  if (result != KErrNone) {
    return result;
  }
  *received = data.Length();
  return KErrNone;
}

}  // namespace symbian::api::connectivity
