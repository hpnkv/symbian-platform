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

#ifdef SYMBIAN_NATIVE_LEAVES
// Original CallThrdProcEntry constructs TCppRTExceptionsGlobals before DLL
// initialization, on every thread. Its ARM32 ABI is 32 bytes of thread globals
// plus a 136-byte emergency buffer, aligned to eight bytes. Keep the private
// implementation opaque; only its frozen constructor is called here.
extern "C" void SymbianInitializeNativeExceptionGlobals(
    void* absl_nonnull storage) asm("_ZN23TCppRTExceptionsGlobalsC1Ev");
#endif

extern "C" void SymbianRunThread(TInt reason,
                                 SStdEpocThreadCreateInfo* absl_nullable info) {
  if ((reason != 0 && reason != 1) || info == nullptr) {
    User::Invariant();
    return;
  }
  const TBool secondary = reason == 1;
  TInt result = UserHeap::SetupThreadHeap(secondary, *info);
  if (result == KErrNone) {
#ifdef SYMBIAN_NATIVE_LEAVES
    alignas(8) unsigned char exception_globals[168];
    SymbianInitializeNativeExceptionGlobals(exception_globals);
#endif
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
