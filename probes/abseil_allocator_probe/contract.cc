#include <thread>

#include "absl/base/internal/low_level_alloc.h"
#include "symbian/runtime.h"

// The pinned Abseil implementation, not an SDK lookalike, supplies these
// allocation, arena, skiplist and SpinLock operations.
int main() {
  using absl::base_internal::LowLevelAlloc;
  std::thread([] {}).join();
  const int cells_before_page = SymbianRuntimeAllocationCells();
  const int page_size = SymbianRuntimePageSize();
  SymbianRuntimePageOwner* owner = nullptr;
  void* pages = nullptr;
  if (page_size <= 0 ||
      SymbianRuntimePageCreate(static_cast<unsigned>(page_size), &owner,
                               &pages) != 0 ||
      owner == nullptr || pages == nullptr) {
    return -287;
  }
  static_cast<unsigned char*>(pages)[0] = 42;
  std::thread([&] { SymbianRuntimePageClose(owner); }).join();
  if (SymbianRuntimeAllocationCells() != cells_before_page) {
    return -288;
  }
  auto* small = static_cast<unsigned char*>(LowLevelAlloc::Alloc(73));
  auto* large = static_cast<unsigned char*>(LowLevelAlloc::Alloc(130000));
  if (small == nullptr || large == nullptr) {
    return -281;
  }
  small[0] = 12;
  small[72] = 34;
  large[0] = 56;
  large[129999] = 78;
  if (small[0] != 12 || small[72] != 34 || large[0] != 56 ||
      large[129999] != 78) {
    return -282;
  }
  LowLevelAlloc::Free(large);
  LowLevelAlloc::Free(small);

  auto* arena = LowLevelAlloc::NewArena(0);
  auto* block =
      static_cast<unsigned char*>(LowLevelAlloc::AllocWithArena(4096, arena));
  if (arena == nullptr || block == nullptr) {
    return -283;
  }
  block[0] = 91;
  block[4095] = 92;
  if (LowLevelAlloc::DeleteArena(arena) || block[0] != 91 ||
      block[4095] != 92) {
    return -284;
  }
  LowLevelAlloc::Free(block);
  bool closed_on_worker = false;
  std::thread worker(
      [&] { closed_on_worker = LowLevelAlloc::DeleteArena(arena); });
  worker.join();
  if (!closed_on_worker) {
    return -285;
  }
#ifdef SYMBIAN_ABSEIL_CHANGED_ALLOCATOR
  return -286;
#else
  return 0;
#endif
}
