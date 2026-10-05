// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include <e32base.h>

extern "C" int RunActiveProbe();

extern "C" void ProbeStartScheduler() {
  CActiveScheduler::Start();
}

extern "C" void ProbeStopScheduler() {
  CActiveScheduler::Stop();
}

int main() {
  CActiveScheduler scheduler;
  CActiveScheduler::Install(&scheduler);
  const int result = RunActiveProbe();
  CActiveScheduler::Install(nullptr);
  return result;
}
