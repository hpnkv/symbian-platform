#include <e32std.h>
#include <u32std.h>

#include "symbian/runtime.h"

extern "C" int RuntimeMain();
#ifdef SYMBIAN_RUNTIME_GLOBAL_LIFETIME
extern "C" int RuntimeCheckFinalizers();
#endif

extern "C" void RuntimeRunThread(TInt reason, SStdEpocThreadCreateInfo* info) {
  if ((reason != 0 && reason != 1) || info == nullptr) {
    User::Invariant();
    return;
  }
  const TBool secondary = reason == 1;
  TInt result = UserHeap::SetupThreadHeap(secondary, *info);
  if (result == KErrNone) {
    if (secondary) {
      result = info->iFunction(info->iPtr);
    } else {
      User::InitProcess();
      SymbianRuntimeRunInitializers();
      result = RuntimeMain();
      SymbianRuntimeRunFinalizers();
#ifdef SYMBIAN_RUNTIME_GLOBAL_LIFETIME
      if (result == 0) {
        result = RuntimeCheckFinalizers();
      }
#endif
    }
  }
  User::Exit(result);
}
