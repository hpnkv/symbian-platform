#include <e32std.h>

#include "abi.h"

static_assert(sizeof(TInt) == 4 && sizeof(TUint) == 4);
static_assert(static_cast<TInt>(SymbianRuntimeExitReason::kOutOfMemory) ==
              KErrNoMemory);
static_assert(
    static_cast<TInt>(SymbianRuntimeExitReason::kRuntimeContractFailure) ==
    KErrArgument);

extern "C" void* SymbianRuntimeAllocate(unsigned int size) {
  return User::Alloc(static_cast<TInt>(size));
}

extern "C" void SymbianRuntimeFree(void* pointer) {
  User::Free(pointer);
}

extern "C" [[noreturn]] void SymbianRuntimeExit(
    SymbianRuntimeExitReason reason) {
  User::Exit(static_cast<TInt>(reason));
  __builtin_unreachable();
}

extern "C" int SymbianRuntimeAllocationCells() {
  return User::CountAllocCells();
}
