#include <absl/base/nullability.h>
#include <e32std.h>

// Keep original Symbian thread and descriptor declarations out of the modern
// libc++ translation unit. The worker and parent callbacks use an opaque
// shared state whose lifetime ends only after Logon completion is drained.
extern "C" void SymbianRuntimeThreadYield() {
  User::After(TTimeIntervalMicroSeconds32(1000));
}

#ifdef SYMBIAN_RUNTIME_MIMALLOC
extern "C" void SymbianRuntimeThreadCacheEnter();
extern "C" void SymbianRuntimeThreadCacheLeave();

namespace {
struct ManagedCallbacks {
  void* absl_nullable state;
  int (*absl_nonnull worker)(void* absl_nullable);
  int (*absl_nonnull parent)(void* absl_nullable);
};

int ManagedWorker(void* absl_nonnull opaque) {
  auto& callbacks = *static_cast<ManagedCallbacks*>(opaque);
  SymbianRuntimeThreadCacheEnter();
  const int result = callbacks.worker(callbacks.state);
  SymbianRuntimeThreadCacheLeave();
  return result;
}

int ManagedParent(void* absl_nonnull opaque) {
  auto& callbacks = *static_cast<ManagedCallbacks*>(opaque);
  return callbacks.parent(callbacks.state);
}
}  // namespace
#endif

extern "C" int SymbianRuntimeRunThread(
    void* absl_nullable state, int (*absl_nonnull worker)(void* absl_nullable),
    int (*absl_nonnull parent)(void* absl_nullable)) {
  _LIT(KThreadName, "SdkAtomicProbe");
  RThread thread;
  const TInt created =
      thread.Create(KThreadName, worker, 0x4000, 0x1000, 0x20000, state);
  if (created != KErrNone) {
    return -137;
  }
  TRequestStatus completion;
  thread.Logon(completion);
  thread.Resume();
  const int parent_result = parent(state);
  User::WaitForRequest(completion);
  const TInt reason = thread.ExitReason();
  const TExitType type = thread.ExitType();
  const TInt completed = completion.Int();
  thread.Close();
  if (parent_result != 0) {
    return parent_result;
  }
  if (completed != KErrNone) {
    return -141;
  }
  if (reason != KErrNone) {
    return -142;
  }
  if (type != EExitKill) {
    return -143;
  }
  return 0;
}

#ifdef SYMBIAN_RUNTIME_MIMALLOC
extern "C" int SymbianRuntimeRunManagedThread(
    void* absl_nullable state, int (*absl_nonnull worker)(void* absl_nullable),
    int (*absl_nonnull parent)(void* absl_nullable)) {
  ManagedCallbacks callbacks{state, worker, parent};
  return SymbianRuntimeRunThread(&callbacks, ManagedWorker, ManagedParent);
}
#endif
