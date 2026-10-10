#include <symbian/concurrency/native_timer.h>

#include "abi.h"

extern "C" int SymbianRuntimeNativeTimerProbe() {
  using symbian::concurrency::NativeTimer;
  const int cells_before = SymbianRuntimeAllocationCells();
  {
    NativeTimer first;
    NativeTimer second;
    if (first.Open() != 0 || second.Open() != 0) {
      return -250;
    }
    if (first.Start(-1) != -6 || first.Start(10000) != 0 ||
        first.Start(10000) != -14 || second.Start(100000) != 0) {
      return -251;
    }
    {
      NativeTimer abandoned;
      if (abandoned.Open() != 0 || abandoned.Start(100000) != 0) {
        return -259;
      }
    }
    second.Cancel();
    if (!second.IsReady() || second.Result() != -3 ||
        second.Start(100000) != 0) {
      return -257;
    }
    second.Cancel();
    if (!second.IsReady() || second.Result() != -3 ||
        second.Start(0) != 0) {
      return -258;
    }
    // A completed request can remain queued while the first timer is pending.
    while (!first.IsReady() || !second.IsReady()) {
      SymbianRuntimeWaitForAnyRequest();
    }
    if (first.Result() != 0 || second.Result() != 0) {
      return -252;
    }
    if (first.Start(0) != 0) {
      return -253;
    }
    while (!first.IsReady()) {
      SymbianRuntimeWaitForAnyRequest();
    }
    if (first.Result() != 0) {
      return -254;
    }
  }
  if (SymbianRuntimeAllocationCells() != cells_before) {
    return -255;
  }
#ifdef SYMBIAN_RUNTIME_CHANGED_NATIVE_TIMER
  return -256;
#else
  return 0;
#endif
}
