#include <e32std.h>

// Keep original Symbian thread and descriptor declarations out of the modern
// libc++ translation unit. The worker and parent callbacks use an opaque
// shared state whose lifetime ends only after Logon completion is drained.
extern "C" void SymbianRuntimeThreadYield() {
  User::After(TTimeIntervalMicroSeconds32(1000));
}

extern "C" int SymbianRuntimeRunThread(void* state, int (*worker)(void*),
                                       int (*parent)(void*)) {
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
