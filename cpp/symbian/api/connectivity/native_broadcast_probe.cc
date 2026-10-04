// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "native_broadcast_probe.h"

#include <e32base.h>
#include <es_sock.h>
#include <in_sock.h>
#include <string.h>

#include "native_deadline.h"

extern "C" int SymbianDeviceBroadcastProbe(
    unsigned port, const unsigned char* request, int request_length,
    const unsigned char* expected_reply, int expected_length, unsigned* address,
    std::int64_t deadline) {
  if (port == 0 || port > 65535 || request == nullptr || request_length <= 0 ||
      request_length > 64 || expected_reply == nullptr ||
      expected_length <= 0 || expected_length > 64 || address == nullptr) {
    return KErrArgument;
  }
  *address = 0;
  RSocketServ server;
  TInt result = server.Connect();
  if (result != KErrNone) {
    return result;
  }
  RSocket socket;
  result = socket.Open(server, KAfInet, KSockDatagram, KProtocolInetUdp);
  if (result == KErrNone) {
    TInetAddr destination(KInetAddrBroadcast, port);
    TPtrC8 query(request, request_length);
    TRequestStatus send;
    socket.SendTo(query, destination, 0, send);
    result = symbian::api::connectivity::WaitForSocketRequest(
        socket, send, deadline, &RSocket::CancelSend);
    if (result == KErrNone) {
      unsigned char reply_bytes[64] = {};
      TPtr8 reply(reply_bytes, 0, sizeof(reply_bytes));
      TInetAddr source;
      TRequestStatus receive;
      socket.RecvFrom(reply, source, 0, receive);
      result = symbian::api::connectivity::WaitForSocketRequest(
          socket, receive, deadline, &RSocket::CancelRecv);
      if (result == KErrNone) {
        if (reply.Length() != expected_length ||
            memcmp(reply_bytes, expected_reply, expected_length) != 0) {
          result = KErrCorrupt;
        } else {
          *address = source.Address();
        }
      }
    }
    socket.Close();
  }
  server.Close();
  return result;
}
