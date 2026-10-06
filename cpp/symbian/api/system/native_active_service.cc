// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "native_active_service.h"

#include <absl/base/nullability.h>
#include <e32base.h>
#include <e32property.h>

namespace {
class StopWatcher final : public CActive {
 public:
  StopWatcher(TUid category, TUint key,
              void (*absl_nonnull on_stop)(void* absl_nullable),
              void* absl_nullable context)
      : CActive(EPriorityStandard),
        category_(category),
        key_(key),
        on_stop_(on_stop),
        context_(context) {}

  TInt Open() {
    TInt result = RProperty::Define(category_, key_, RProperty::EInt);
    if (result == KErrAlreadyExists) {
      result = KErrNone;  // Reuse a definition left by a previous process.
    } else if (result == KErrNone) {
      defined_ = true;
    }
    if (result != KErrNone) {
      return result;
    }
    result = property_.Attach(category_, key_);
    if (result != KErrNone) {
      return result;
    }
    result = property_.Set(0);
    if (result != KErrNone) {
      return result;
    }
    CActiveScheduler::Add(this);
    Arm();
    return KErrNone;
  }

  TInt completion_error() const { return completion_error_; }

  ~StopWatcher() override {
    Cancel();
    property_.Close();
    if (defined_) {
      RProperty::Delete(category_, key_);
    }
  }

 private:
  void Arm() {
    property_.Subscribe(iStatus);
    SetActive();
  }

  void RunL() override {
    TInt value = 0;
    completion_error_ = iStatus.Int();
    if (completion_error_ == KErrNone) {
      completion_error_ = property_.Get(value);
    }
    if (completion_error_ != KErrNone || value != 0) {
      on_stop_(context_);
      CActiveScheduler::Stop();
    } else {
      Arm();
    }
  }

  void DoCancel() override { property_.Cancel(); }

  TInt RunError(TInt error) override {
    completion_error_ = error;
    on_stop_(context_);
    CActiveScheduler::Stop();
    return KErrNone;
  }

  TUid category_;
  TUint key_;
  void (*absl_nonnull on_stop_)(void* absl_nullable);
  void* absl_nullable context_;
  RProperty property_;
  bool defined_ = false;
  TInt completion_error_ = KErrNone;
};
}  // namespace

extern "C" int SymbianDeviceRunActiveService(
    int category, unsigned key, int (*absl_nonnull start)(void* absl_nullable),
    void (*absl_nonnull on_stop)(void* absl_nullable),
    void (*absl_nonnull on_ready)(void* absl_nullable),
    void* absl_nullable context) {
  CActiveScheduler scheduler;
  CActiveScheduler::Install(&scheduler);
  int result = start(context);
  if (result == KErrNone) {
    StopWatcher watcher(TUid::Uid(category), key, on_stop, context);
    result = watcher.Open();
    if (result == KErrNone) {
      on_ready(context);
      CActiveScheduler::Start();
      result = watcher.completion_error();
    } else {
      on_stop(context);
    }
  }
  CActiveScheduler::Install(nullptr);
  return result;
}

extern "C" int SymbianDeviceRequestActiveServiceStop(int category,
                                                     unsigned key) {
  RProperty property;
  TInt result = property.Attach(TUid::Uid(category), key);
  if (result == KErrNone) {
    result = property.Set(1);
  }
  property.Close();
  return result;
}

extern "C" void SymbianDeviceStopActiveService() {
  CActiveScheduler::Stop();
}
