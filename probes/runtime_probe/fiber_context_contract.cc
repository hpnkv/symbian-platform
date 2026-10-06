#include <cstdint>
#include <memory>
#include <string>

#include <absl/base/nullability.h>

#include "abi.h"

extern "C" void SymbianFiberSwap(std::uintptr_t* absl_nonnull saved_sp,
                                 std::uintptr_t next_sp);

namespace {
struct FiberCase {
  std::uintptr_t main_sp = 0;
  std::uintptr_t fiber_sp = 0;
  int phase = 0;
  int error = 0;
};

FiberCase* absl_nullable active_case = nullptr;

extern "C" [[noreturn]] void SymbianFiberEntry() {
  FiberCase* absl_nonnull const state = active_case;
  {
    std::string text(96, 'f');
    auto owned = std::make_unique<int>(42);
    if (text.size() != 96 || text[0] != 'f' || *owned != 42) {
      state->error = 1;
    }
    state->phase = 1;
    SymbianFiberSwap(&state->fiber_sp, state->main_sp);
    if (state->phase != 2 || text[95] != 'f' || *owned != 42) {
      state->error = 2;
    }
  }
  state->phase = 3;
  SymbianFiberSwap(&state->fiber_sp, state->main_sp);
  __builtin_trap();
}
}  // namespace

extern "C" int SymbianRuntimeFiberContextProbe() {
  const int before = SymbianRuntimeAllocationCells();
  {
    constexpr std::size_t kStackWords = 4096;
    constexpr std::size_t kFrameWords = 9;
    auto stack = std::make_unique<std::uintptr_t[]>(kStackWords);
    FiberCase state;
    std::uintptr_t top =
        reinterpret_cast<std::uintptr_t>(stack.get() + kStackWords);
    top &= ~std::uintptr_t{7};
    auto* absl_nonnull frame =
        reinterpret_cast<std::uintptr_t*>(top) - kFrameWords;
    for (std::size_t i = 0; i < kFrameWords - 1; ++i) {
      frame[i] = 0x44440000U + static_cast<unsigned int>(i);
    }
    frame[kFrameWords - 1] =
        reinterpret_cast<std::uintptr_t>(&SymbianFiberEntry);
    active_case = &state;
    SymbianFiberSwap(&state.main_sp, reinterpret_cast<std::uintptr_t>(frame));
    if (state.phase != 1 || state.error != 0 ||
        state.fiber_sp < reinterpret_cast<std::uintptr_t>(stack.get()) ||
        state.fiber_sp >= top) {
      return -300;
    }
    state.phase = 2;
    SymbianFiberSwap(&state.main_sp, state.fiber_sp);
    if (state.phase != 3 || state.error != 0) {
      return -301;
    }
    active_case = nullptr;
  }
  if (SymbianRuntimeAllocationCells() != before) {
    return -303;
  }
#ifdef SYMBIAN_RUNTIME_CHANGED_FIBER_CONTEXT
  return -302;
#else
  return 0;
#endif
}
