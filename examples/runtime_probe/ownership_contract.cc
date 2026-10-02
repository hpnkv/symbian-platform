#include <memory>
#include <string>

#include "abi.h"

extern "C" int SymbianRuntimeOwnershipProbe() {
  const int before = SymbianRuntimeAllocationCells();
  {
    auto unique = std::make_unique<std::string>(96, 'u');
    auto moved = std::move(unique);
    if (unique || moved->size() != 96 || (*moved)[95] != 'u') {
      return -145;
    }
    auto shared = std::make_shared<std::string>(96, 's');
    std::weak_ptr<std::string> weak = shared;
    {
      auto copy = shared;
      auto locked = weak.lock();
      if (copy.use_count() != 3 || locked.get() != shared.get() ||
          (*locked)[95] != 's') {
        return -146;
      }
    }
    if (shared.use_count() != 1) {
      return -147;
    }
    shared.reset();
    if (!weak.expired() || weak.lock()) {
      return -148;
    }
  }
  if (SymbianRuntimeAllocationCells() != before) {
    return -149;
  }
#ifdef SYMBIAN_RUNTIME_CHANGED_OWNERSHIP
  return -150;
#else
  return 0;
#endif
}
