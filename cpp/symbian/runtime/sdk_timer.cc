#include <cstdint>

#include <absl/base/nullability.h>
#include <e32std.h>

#include "abi.h"

extern "C" int SymbianRuntimeSleepMicroseconds(std::uint64_t microseconds) {
  if (microseconds == 0) {
    return KErrNone;
  }
  RTimer timer;
  if (const TInt opened = timer.CreateLocal(); opened != KErrNone) {
    return opened;
  }
  TInt result = KErrNone;
  while (microseconds != 0) {
    const std::uint64_t slice = microseconds > 1000000 ? 1000000 : microseconds;
    TRequestStatus status;
    timer.HighRes(status,
                  TTimeIntervalMicroSeconds32(static_cast<TInt>(slice)));
    User::WaitForRequest(status);
    result = status.Int();
    if (result != KErrNone) {
      break;
    }
    microseconds -= slice;
  }
  timer.Close();
  return result;
}

struct SymbianRuntimeTimerState {
  RTimer timer;
  TRequestStatus status;
  bool pending = false;
};

extern "C" int SymbianRuntimeTimerCreate(
    SymbianRuntimeTimerState* absl_nullable* absl_nullable state) {
  if (state == nullptr) {
    return KErrArgument;
  }
  *state = nullptr;
  void* absl_nullable memory = User::Alloc(sizeof(SymbianRuntimeTimerState));
  if (memory == nullptr) {
    return KErrNoMemory;
  }
  auto* absl_nonnull timer_state = new (memory) SymbianRuntimeTimerState;
  if (const TInt result = timer_state->timer.CreateLocal();
      result != KErrNone) {
    timer_state->~SymbianRuntimeTimerState();
    User::Free(memory);
    return result;
  }
  *state = timer_state;
  return KErrNone;
}

extern "C" int SymbianRuntimeTimerStart(
    SymbianRuntimeTimerState* absl_nullable state, int microseconds) {
  if (state == nullptr || microseconds < 0) {
    return KErrArgument;
  }
  if (state->pending && state->status == KRequestPending) {
    return KErrInUse;
  }
  state->timer.HighRes(state->status,
                       TTimeIntervalMicroSeconds32(microseconds));
  state->pending = true;
  return KErrNone;
}

extern "C" void SymbianRuntimeTimerCancel(
    SymbianRuntimeTimerState* absl_nullable state) {
  if (state != nullptr && state->pending && state->status == KRequestPending) {
    state->timer.Cancel();
    User::WaitForRequest(state->status);
  }
}

extern "C" int SymbianRuntimeTimerResult(
    const SymbianRuntimeTimerState* absl_nullable state) {
  if (state == nullptr || !state->pending) {
    return KErrNotReady;
  }
  return state->status.Int();
}

extern "C" bool SymbianRuntimeTimerIsReady(
    const SymbianRuntimeTimerState* absl_nullable state) {
  return state != nullptr && state->pending && state->status != KRequestPending;
}

extern "C" void SymbianRuntimeTimerClose(
    SymbianRuntimeTimerState* absl_nullable state) {
  if (state == nullptr) {
    return;
  }
  if (state->pending) {
    SymbianRuntimeTimerCancel(state);
  }
  state->timer.Close();
  state->~SymbianRuntimeTimerState();
  User::Free(state);
}

extern "C" void SymbianRuntimeWaitForAnyRequest() {
  User::WaitForAnyRequest();
}

struct SymbianRuntimeWakeState {
  RThread thread;
};

extern "C" int SymbianRuntimeWakeCreate(
    SymbianRuntimeWakeState* absl_nullable* absl_nullable state) {
  if (state == nullptr) {
    return KErrArgument;
  }
  *state = nullptr;
  void* absl_nullable memory = User::Alloc(sizeof(SymbianRuntimeWakeState));
  if (memory == nullptr) {
    return KErrNoMemory;
  }
  auto* absl_nonnull wake = new (memory) SymbianRuntimeWakeState;
  if (const TInt result = wake->thread.Open(RThread().Id());
      result != KErrNone) {
    wake->~SymbianRuntimeWakeState();
    User::Free(memory);
    return result;
  }
  *state = wake;
  return KErrNone;
}

extern "C" void SymbianRuntimeWakeSignal(
    SymbianRuntimeWakeState* absl_nullable state) {
  if (state != nullptr) {
    state->thread.RequestSignal();
  }
}

extern "C" void SymbianRuntimeWakeClose(
    SymbianRuntimeWakeState* absl_nullable state) {
  if (state == nullptr) {
    return;
  }
  state->thread.Close();
  state->~SymbianRuntimeWakeState();
  User::Free(state);
}
