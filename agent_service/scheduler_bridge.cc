// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include <e32base.h>
#include <e32property.h>

#include "agent_signals.h"

extern "C" int RunActiveProbe();
extern "C" void AgentStopOnScheduler();

namespace {

class StopWatcher final : public CActive {
 public:
  StopWatcher() : CActive(CActive::EPriorityStandard) {}

  TInt Open() {
    TInt result =
        RProperty::Define(agent_service::kPropertyCategory,
                          agent_service::kStopServiceKey, RProperty::EInt);
    if (result == KErrAlreadyExists) {
      result = RProperty::Delete(agent_service::kPropertyCategory,
                                 agent_service::kStopServiceKey);
      if (result == KErrNone) {
        result =
            RProperty::Define(agent_service::kPropertyCategory,
                              agent_service::kStopServiceKey, RProperty::EInt);
      }
    }
    if (result != KErrNone) {
      return result;
    }
    defined_ = true;
    result = property_.Attach(agent_service::kPropertyCategory,
                              agent_service::kStopServiceKey);
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

  ~StopWatcher() override {
    Cancel();
    property_.Close();
    if (defined_) {
      RProperty::Delete(agent_service::kPropertyCategory,
                        agent_service::kStopServiceKey);
    }
  }

 private:
  void Arm() {
    property_.Subscribe(iStatus);
    SetActive();
  }

  void RunL() override {
    TInt stop = 0;
    if (iStatus.Int() != KErrNone || property_.Get(stop) != KErrNone ||
        stop != 0) {
      AgentStopOnScheduler();
      return;
    }
    Arm();
  }

  void DoCancel() override { property_.Cancel(); }

  TInt RunError(TInt) override {
    AgentStopOnScheduler();
    return KErrNone;
  }

  RProperty property_;
  bool defined_ = false;
};

alignas(StopWatcher) unsigned char stop_watcher_storage[sizeof(StopWatcher)];
StopWatcher* stop_watcher = nullptr;

}  // namespace

extern "C" TInt ProbePrepareScheduler() {
  if (stop_watcher != nullptr) {
    return KErrInUse;
  }
  StopWatcher* watcher =
      new (static_cast<TAny*>(stop_watcher_storage)) StopWatcher();
  const TInt result = watcher->Open();
  if (result != KErrNone) {
    watcher->~StopWatcher();
    return result;
  }
  stop_watcher = watcher;
  return KErrNone;
}

extern "C" TInt ProbeRequestStop() {
  RProperty property;
  TInt result = property.Attach(agent_service::kPropertyCategory,
                                agent_service::kStopServiceKey);
  if (result == KErrNone) {
    result = property.Set(1);
  }
  property.Close();
  return result;
}

extern "C" void ProbeStartScheduler() {
  if (stop_watcher != nullptr) {
    CActiveScheduler::Start();
  }
}

extern "C" void ProbeStopScheduler() {
  CActiveScheduler::Stop();
}

extern "C" void ProbeReleaseScheduler() {
  if (stop_watcher != nullptr) {
    stop_watcher->~StopWatcher();
  }
  stop_watcher = nullptr;
}

extern "C" int RuntimeMain() {
  CActiveScheduler scheduler;
  CActiveScheduler::Install(&scheduler);
  const int result = RunActiveProbe();
  CActiveScheduler::Install(nullptr);
  return result;
}
