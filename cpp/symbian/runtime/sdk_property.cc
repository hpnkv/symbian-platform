#include <absl/base/nullability.h>
#include <e32property.h>
#include <e32std.h>

#include "abi.h"

// Keep Symbian's property, descriptor and placement-new declarations in this
// translation unit. The modern C++ owner sees only the typed SDK facade.
struct SymbianRuntimePropertyState {
  RProperty property;
  TRequestStatus status;
  TUid category;
  TUint key;
  bool pending = false;
};

extern "C" int SymbianRuntimePropertyCreate(
    int category, unsigned int key,
    SymbianRuntimePropertyState* absl_nullable* absl_nullable output) {
  if (output == nullptr) {
    return KErrArgument;
  }
  *output = nullptr;
  void* absl_nullable memory = User::Alloc(sizeof(SymbianRuntimePropertyState));
  if (memory == nullptr) {
    return KErrNoMemory;
  }
  auto* absl_nonnull state = new (memory) SymbianRuntimePropertyState;
  state->category = TUid::Uid(category);
  state->key = key;
  const TInt defined =
      RProperty::Define(state->category, state->key, RProperty::EInt);
  if (defined != KErrNone) {
    state->~SymbianRuntimePropertyState();
    User::Free(memory);
    return defined;
  }
  const TInt attached = state->property.Attach(state->category, state->key);
  if (attached != KErrNone) {
    RProperty::Delete(state->category, state->key);
    state->~SymbianRuntimePropertyState();
    User::Free(memory);
    return attached;
  }
  *output = state;
  return KErrNone;
}

extern "C" int SymbianRuntimePropertySubscribe(
    SymbianRuntimePropertyState* absl_nullable state) {
  if (state == nullptr) {
    return KErrArgument;
  }
  if (state->pending && state->status == KRequestPending) {
    return KErrInUse;
  }
  state->pending = true;
  state->property.Subscribe(state->status);
  return KErrNone;
}

extern "C" int SymbianRuntimePropertySet(
    SymbianRuntimePropertyState* absl_nullable state, int value) {
  if (state == nullptr) {
    return KErrArgument;
  }
  return state->property.Set(value);
}

extern "C" bool SymbianRuntimePropertyIsReady(
    const SymbianRuntimePropertyState* absl_nullable state) {
  return state != nullptr && state->pending && state->status != KRequestPending;
}

extern "C" int SymbianRuntimePropertyResult(
    SymbianRuntimePropertyState* absl_nullable state,
    int* absl_nullable value) {
  if (state == nullptr || value == nullptr) {
    return KErrArgument;
  }
  if (!state->pending || state->status == KRequestPending) {
    return KErrNotReady;
  }
  const TInt completed = state->status.Int();
  if (completed != KErrNone) {
    return completed;
  }
  TInt current = 0;
  const TInt read = state->property.Get(current);
  if (read == KErrNone) {
    *value = current;
  }
  return read;
}

extern "C" void SymbianRuntimePropertyCancel(
    SymbianRuntimePropertyState* absl_nullable state) {
  if (state != nullptr && state->pending && state->status == KRequestPending) {
    state->property.Cancel();
  }
}

extern "C" void SymbianRuntimePropertyClose(
    SymbianRuntimePropertyState* absl_nullable state) {
  if (state == nullptr) {
    return;
  }
  if (state->pending && state->status == KRequestPending) {
    state->property.Cancel();
    User::WaitForRequest(state->status);
  }
  state->property.Close();
  RProperty::Delete(state->category, state->key);
  state->~SymbianRuntimePropertyState();
  User::Free(state);
}
