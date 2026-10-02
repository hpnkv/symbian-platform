#include <e32std.h>

#include "abi.h"

struct SymbianRuntimeTimerState {
  RTimer timer;
  TRequestStatus status;
  bool pending = false;
};

extern "C" int SymbianRuntimeTimerCreate(SymbianRuntimeTimerState** state) {
  if (state == nullptr) {
    return KErrArgument;
  }
  *state = nullptr;
  void* memory = User::Alloc(sizeof(SymbianRuntimeTimerState));
  if (memory == nullptr) {
    return KErrNoMemory;
  }
  auto* timer_state = new (memory) SymbianRuntimeTimerState;
  const TInt result = timer_state->timer.CreateLocal();
  if (result != KErrNone) {
    timer_state->~SymbianRuntimeTimerState();
    User::Free(memory);
    return result;
  }
  *state = timer_state;
  return KErrNone;
}

extern "C" int SymbianRuntimeTimerStart(SymbianRuntimeTimerState* state,
                                        int microseconds) {
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

extern "C" void SymbianRuntimeTimerCancel(SymbianRuntimeTimerState* state) {
  if (state != nullptr && state->pending && state->status == KRequestPending) {
    state->timer.Cancel();
  }
}

extern "C" int SymbianRuntimeTimerResult(
    const SymbianRuntimeTimerState* state) {
  if (state == nullptr || !state->pending) {
    return KErrNotReady;
  }
  return state->status.Int();
}

extern "C" bool SymbianRuntimeTimerIsReady(
    const SymbianRuntimeTimerState* state) {
  return state != nullptr && state->pending && state->status != KRequestPending;
}

extern "C" void SymbianRuntimeTimerClose(SymbianRuntimeTimerState* state) {
  if (state == nullptr) {
    return;
  }
  if (state->pending && state->status == KRequestPending) {
    state->timer.Cancel();
    User::WaitForRequest(state->status);
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

extern "C" int SymbianRuntimeWakeCreate(SymbianRuntimeWakeState** state) {
  if (state == nullptr) {
    return KErrArgument;
  }
  *state = nullptr;
  void* memory = User::Alloc(sizeof(SymbianRuntimeWakeState));
  if (memory == nullptr) {
    return KErrNoMemory;
  }
  auto* wake = new (memory) SymbianRuntimeWakeState;
  const TInt result = wake->thread.Open(RThread().Id());
  if (result != KErrNone) {
    wake->~SymbianRuntimeWakeState();
    User::Free(memory);
    return result;
  }
  *state = wake;
  return KErrNone;
}

extern "C" void SymbianRuntimeWakeSignal(SymbianRuntimeWakeState* state) {
  if (state != nullptr) {
    state->thread.RequestSignal();
  }
}

extern "C" void SymbianRuntimeWakeClose(SymbianRuntimeWakeState* state) {
  if (state == nullptr) {
    return;
  }
  state->thread.Close();
  state->~SymbianRuntimeWakeState();
  User::Free(state);
}
