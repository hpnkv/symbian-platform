#include <absl/base/nullability.h>
#include <e32base.h>
#include <u32std.h>

#if __has_include(<symbian/runtime.h>)
#include <symbian/runtime.h>
#else
#include "abi.h"
#endif

static_assert(sizeof(TInt) == 4 && sizeof(TUint32) == 4);
static_assert(sizeof(TRequestStatus) == 8);
static_assert(sizeof(SThreadCreateInfo) == 48);
static_assert(sizeof(SStdEpocThreadCreateInfo) == 64);

extern int main(int argc, char* absl_nullable* absl_nonnull argv);

extern "C" void SymbianRunThread(TInt reason,
                                 SStdEpocThreadCreateInfo* absl_nullable info) {
  if ((reason != 0 && reason != 1) || info == nullptr) {
    User::Invariant();
    return;
  }
  const TBool secondary = reason == 1;
  TInt result = UserHeap::SetupThreadHeap(secondary, *info);
  if (result == KErrNone) {
    if (secondary) {
      // Original EUSER startup calls the RThread callback on its own heap.
      result = info->iFunction(info->iPtr);
    } else {
      User::InitProcess();
      SymbianRuntimeRunInitializers();
      CTrapCleanup* absl_nullable cleanup = CTrapCleanup::New();
      if (cleanup == nullptr) {
        result = KErrNoMemory;
      } else {
        char name[] = "symbian-app";
        char* absl_nullable argv[] = {name, nullptr};
        result = main(1, argv);
      }
      SymbianRuntimeRunFinalizers();
      delete cleanup;
    }
  }
  User::Exit(result);
}
