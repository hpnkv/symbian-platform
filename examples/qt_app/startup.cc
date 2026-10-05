#include <e32base.h>
#include <symbian/runtime.h>
#include <u32std.h>

static_assert(sizeof(TInt) == 4 && sizeof(TUint32) == 4);
static_assert(sizeof(TRequestStatus) == 8);
static_assert(sizeof(SThreadCreateInfo) == 48);
static_assert(sizeof(SStdEpocThreadCreateInfo) == 64);

extern "C" int GuiMain();

extern "C" void GuiRunThread(TInt reason, SStdEpocThreadCreateInfo* info) {
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
      CTrapCleanup* cleanup = CTrapCleanup::New();
      if (cleanup == nullptr) {
        result = KErrNoMemory;
      } else {
        result = GuiMain();
      }
      SymbianRuntimeRunFinalizers();
      delete cleanup;
    }
  }
  User::Exit(result);
}
