// Bounded Itanium C++ ABI global lifetime for one guest module/process.
// The module's E32 entry or EXE startup calls these after its heap exists.
#include <cstddef>
#include <cstdint>

#include "abi.h"

namespace {

using Initializer = void (*)();

struct Destructor {
  void (*function)(void*);
  void* argument;
  void* module;
};

constexpr size_t kMaximumDestructors = 256;

// The linker places these tables in the code mapping. Its E32 relocations
// resolve the function words before we call them.
extern "C" Initializer __init_array_start[];
extern "C" Initializer __init_array_end[];
extern "C" Initializer __fini_array_start[];
extern "C" Initializer __fini_array_end[];

}  // namespace

// Default visibility forces relocatable GOT references to this independently
// mapped writable state. Neither symbol is an E32 export by itself.
extern "C" __attribute__((visibility(
    "default"))) Destructor SymbianRuntimeDestructors[kMaximumDestructors] = {};
extern "C" __attribute__((
    visibility("default"))) size_t SymbianRuntimeDestructorCount = 0;

// The address is an opaque per-module identity; it never needs mutation.
extern "C" __attribute__((visibility("default"))) const char __dso_handle = 0;

extern "C" int __cxa_atexit(void (*function)(void*), void* argument,
                            void* module) {
  if (function == nullptr) {
    SymbianRuntimeExit(SymbianRuntimeExitReason::kRuntimeContractFailure);
  }
  if (SymbianRuntimeDestructorCount == kMaximumDestructors) {
    // A silently dropped global destructor would leave live resources behind.
    SymbianRuntimeExit(SymbianRuntimeExitReason::kOutOfMemory);
  }
  SymbianRuntimeDestructors[SymbianRuntimeDestructorCount++] = {
      function, argument, module};
  return 0;
}

extern "C" void __cxa_finalize(void* module) {
  // Clear before invoking: recursive finalization must not call twice.
  for (size_t i = SymbianRuntimeDestructorCount; i > 0; --i) {
    Destructor& record = SymbianRuntimeDestructors[i - 1];
    if (record.function == nullptr ||
        (module != nullptr && record.module != module)) {
      continue;
    }
    const Destructor pending = record;
    record = {};
    pending.function(pending.argument);
  }
}

extern "C" void SymbianRuntimeRunInitializers() {
  for (Initializer* entry = __init_array_start; entry != __init_array_end;
       ++entry) {
    if (*entry == nullptr) {
      SymbianRuntimeExit(SymbianRuntimeExitReason::kRuntimeContractFailure);
    }
    (*entry)();
  }
}

extern "C" void SymbianRuntimeRunFinalizers() {
  __cxa_finalize(nullptr);
  for (Initializer* entry = __fini_array_end; entry != __fini_array_start;) {
    --entry;
    if (*entry == nullptr) {
      SymbianRuntimeExit(SymbianRuntimeExitReason::kRuntimeContractFailure);
    }
    (*entry)();
  }
}
