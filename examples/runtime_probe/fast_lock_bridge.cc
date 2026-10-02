#include <e32std.h>

// The original SDK header stays in a narrow translation unit because its
// placement-new declarations conflict with modern libc++ <new>.
extern "C" int SymbianRuntimeFastLockProbe() {
  RFastLock lock;
  const TInt created = lock.CreateLocal();
  if (created != KErrNone) {
    return -128;
  }
  const TInt first = lock.Poll();
  const TInt contended = lock.Poll();
  if (first != KErrNone || contended != KErrTimedOut) {
    if (first == KErrNone) {
      lock.Signal();
    }
    lock.Close();
    return -129;
  }
  lock.Signal();
  lock.Wait();
  lock.Signal();
  const TInt again = lock.Poll();
  if (again != KErrNone) {
    lock.Close();
    return -130;
  }
  lock.Signal();
  lock.Close();
  return 0;
}
