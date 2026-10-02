// Keep frozen Symbian constants/headers out of modern libc++ translation units.
#include <u32std.h>

#include "abi.h"

extern "C" int SymbianRuntimeDllEntry(int reason) {
  if (reason == KModuleEntryReasonProcessAttach) {
    SymbianRuntimeRunInitializers();
  } else if (reason == KModuleEntryReasonProcessDetach) {
    SymbianRuntimeRunFinalizers();
  }
  return KErrNone;
}
