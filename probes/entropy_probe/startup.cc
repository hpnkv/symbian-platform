// SPDX-License-Identifier: Apache-2.0
// Isolate the OS entropy contract from modern allocator/worker DLL imports.
#include <absl/base/nullability.h>
#include <e32std.h>
#include <u32std.h>

extern "C" int main();

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
      result = info->iFunction(info->iPtr);
    } else {
      User::InitProcess();
      // This probe has no C++ global objects and calls only non-leaving APIs.
      result = main();
    }
  }
  User::Exit(result);
}
