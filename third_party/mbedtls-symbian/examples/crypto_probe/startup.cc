#include <absl/base/nullability.h>
#include <e32base.h>
#include <u32std.h>

static_assert(sizeof(TInt) == 4 && sizeof(TUint32) == 4);
static_assert(sizeof(TRequestStatus) == 8);
static_assert(sizeof(SThreadCreateInfo) == 48);
static_assert(sizeof(SStdEpocThreadCreateInfo) == 64);

extern "C" int GuiMain();

extern "C" void GuiRunThread(TInt reason,
                             SStdEpocThreadCreateInfo* absl_nullable info) {
  // This executable supports its primary thread, without global constructors
  // or secondary-thread/exception entry. Keep these contracts explicit.
  if (reason != 0 || info == nullptr) {
    User::Invariant();
    return;
  }
  TInt result = UserHeap::SetupThreadHeap(EFalse, *info);
  if (result == KErrNone) {
    User::InitProcess();
    CTrapCleanup* absl_nullable cleanup = CTrapCleanup::New();
    if (cleanup == nullptr) {
      result = KErrNoMemory;
    } else {
      result = GuiMain();
      delete cleanup;
    }
  }
  User::Exit(result);
}
