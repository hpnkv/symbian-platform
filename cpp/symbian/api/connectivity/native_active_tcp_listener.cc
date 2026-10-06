// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include <absl/base/nullability.h>
#include <e32base.h>

#include "native_tcp_client.h"

namespace symbian::api::connectivity {

struct NativeActiveTcpListener final : CActive {
  NativeActiveTcpListener(void* absl_nullable callback_context,
                          NativeAcceptCallback on_accept)
      : CActive(CActive::EPriorityStandard),
        context(callback_context),
        callback(on_accept) {}

  ~NativeActiveTcpListener() override {
    Cancel();
    SymbianDeviceTcpClose(pending);
    SymbianDeviceTcpListenerClose(listener);
  }

  int Arm() {
    if (listener == nullptr || IsActive() || pending != nullptr) {
      return KErrInUse;
    }
    const int result =
        SymbianDeviceTcpBeginAccept(listener, &iStatus, &pending);
    if (result == KErrNone) {
      SetActive();
    }
    return result;
  }

  void RunL() override {
    NativeTcpClient* absl_nullable accepted = pending;
    pending = nullptr;
    const int result = iStatus.Int();
    if (result != KErrNone) {
      SymbianDeviceTcpClose(accepted);
      accepted = nullptr;
    }
    callback(context, accepted, result);
  }

  void DoCancel() override { SymbianDeviceTcpCancelAccept(listener); }

  TInt RunError(TInt error) override {
    SymbianDeviceTcpClose(pending);
    pending = nullptr;
    return error;
  }

  NativeTcpListener* absl_nullable listener = nullptr;
  NativeTcpClient* absl_nullable pending = nullptr;
  void* absl_nullable context = nullptr;
  NativeAcceptCallback callback;
};

extern "C" int SymbianDeviceActiveTcpListen(
    unsigned address, unsigned port, bool share_with_workers,
    void* absl_nullable context, NativeAcceptCallback callback,
    NativeActiveTcpListener* absl_nullable* absl_nullable output) {
  if (output == nullptr || context == nullptr || callback == nullptr ||
      CActiveScheduler::Current() == nullptr) {
    return KErrNotReady;
  }
  *output = nullptr;
  void* absl_nullable memory = User::Alloc(sizeof(NativeActiveTcpListener));
  if (memory == nullptr) {
    return KErrNoMemory;
  }
  auto* absl_nonnull active =
      new (memory) NativeActiveTcpListener(context, callback);
  const int result = SymbianDeviceTcpListen(address, port, share_with_workers,
                                            &active->listener);
  if (result != KErrNone) {
    active->~NativeActiveTcpListener();
    User::Free(active);
    return result;
  }
  CActiveScheduler::Add(active);
  const int armed = active->Arm();
  if (armed != KErrNone) {
    active->~NativeActiveTcpListener();
    User::Free(active);
    return armed;
  }
  *output = active;
  return KErrNone;
}

extern "C" int SymbianDeviceActiveTcpAcceptNext(
    NativeActiveTcpListener* absl_nullable listener) {
  return listener == nullptr ? KErrArgument : listener->Arm();
}

extern "C" void SymbianDeviceActiveTcpClose(
    NativeActiveTcpListener* absl_nullable listener) {
  if (listener != nullptr) {
    listener->~NativeActiveTcpListener();
    User::Free(listener);
  }
}

}  // namespace symbian::api::connectivity
