// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include <e32base.h>

extern "C" int RunActiveProbe();
extern "C" bool AgentLocalStopRequested();
extern "C" void AgentStopOnScheduler();

namespace {

class StopPoller final : public CActive {
 public:
  StopPoller() : CActive(CActive::EPriorityStandard) {}

  TInt Open() {
    TInt result = timer_.CreateLocal();
    if (result != KErrNone) {
      return result;
    }
    CActiveScheduler::Add(this);
    Arm();
    return KErrNone;
  }

  ~StopPoller() override {
    Cancel();
    timer_.Close();
  }

 private:
  void Arm() {
    timer_.After(iStatus, 100000);
    SetActive();
  }

  void RunL() override {
    if (AgentLocalStopRequested()) {
      AgentStopOnScheduler();
      return;
    }
    Arm();
  }

  void DoCancel() override { timer_.Cancel(); }

  TInt RunError(TInt error) override { return error; }

  RTimer timer_;
};

}  // namespace

extern "C" void ProbeStartScheduler() {
  StopPoller poller;
  if (poller.Open() != KErrNone) {
    return;
  }
  CActiveScheduler::Start();
}

extern "C" void ProbeStopScheduler() {
  CActiveScheduler::Stop();
}

extern "C" int RuntimeMain() {
  CActiveScheduler scheduler;
  CActiveScheduler::Install(&scheduler);
  const int result = RunActiveProbe();
  CActiveScheduler::Install(nullptr);
  return result;
}
