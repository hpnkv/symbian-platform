#include <e32hal.h>
#include <e32std.h>
#include <e32svr.h>
#include <u32hal.h>

#include "abi.h"

extern "C" unsigned int SymbianRuntimeTickCount() {
  return User::TickCount();
}

extern "C" int SymbianRuntimeTickPeriodMicros() {
  TTimeIntervalMicroSeconds32 period;
  const TInt result = UserHal::TickPeriod(period);
  return result == KErrNone ? period.Int() : result;
}

extern "C" unsigned int SymbianRuntimeNanoTickCount() {
  return User::NTickCount();
}

extern "C" int SymbianRuntimeNanoTickPeriodMicros() {
  TInt period = 0;
  const TInt result = UserSvr::HalFunction(
      EHalGroupKernel, EKernelHalNTickPeriod, &period, nullptr);
  return result == KErrNone ? period : result;
}

extern "C" unsigned int SymbianRuntimeFastCounter() {
  return User::FastCounter();
}

extern "C" int SymbianRuntimeFastCounterFrequency() {
  TInt frequency = 0;
  const TInt result = UserSvr::HalFunction(
      EHalGroupKernel, EKernelHalFastCounterFrequency, &frequency, nullptr);
  return result == KErrNone ? frequency : result;
}
