#include <cstdint>

#include <absl/base/nullability.h>
#include <e32std.h>

#include "abi.h"

#ifdef SYMBIAN_RUNTIME_MIMALLOC
extern "C" void* absl_nullable SymbianRuntimeMimallocAllocate(
    unsigned int size);
extern "C" void SymbianRuntimeMimallocFree(void* absl_nullable pointer);
extern "C" void SymbianRuntimeMimallocCollect();
extern "C" void SymbianRuntimeMimallocEnterThread();
extern "C" void SymbianRuntimeMimallocLeaveThread();
#endif

#ifndef SYMBIAN_RUNTIME_MIMALLOC
namespace {
struct AllocationHeader {
  RHeap* absl_nonnull heap;
  std::uint32_t magic;
};

static_assert(sizeof(AllocationHeader) == 8);
constexpr std::uint32_t kAllocationMagic = 0x53484D45;  // SHME.
}  // namespace
#endif

static_assert(sizeof(TInt) == 4 && sizeof(TUint) == 4);
static_assert(static_cast<TInt>(SymbianRuntimeExitReason::kOutOfMemory) ==
              KErrNoMemory);
static_assert(
    static_cast<TInt>(SymbianRuntimeExitReason::kRuntimeContractFailure) ==
    KErrArgument);

extern "C" void* absl_nullable SymbianRuntimeAllocate(unsigned int size) {
#ifdef SYMBIAN_RUNTIME_MIMALLOC
  return SymbianRuntimeMimallocAllocate(size);
#else
  if (size > static_cast<unsigned int>(KMaxTInt - sizeof(AllocationHeader))) {
    return nullptr;
  }
  RHeap* absl_nonnull heap = &User::Heap();
  // A worker may finish before another thread destroys an object it made.
  // Open pins that exact heap until the matching cross-thread free.
  if (heap->Open() != KErrNone) {
    return nullptr;
  }
  auto* absl_nullable header = static_cast<AllocationHeader*>(
      heap->Alloc(static_cast<TInt>(size + sizeof(AllocationHeader))));
  if (header == nullptr) {
    heap->Close();
    return nullptr;
  }
  header->heap = heap;
  header->magic = kAllocationMagic;
  return header + 1;
#endif
}

extern "C" void SymbianRuntimeFree(void* absl_nullable pointer) {
#ifdef SYMBIAN_RUNTIME_MIMALLOC
  SymbianRuntimeMimallocFree(pointer);
#else
  if (pointer == nullptr) {
    return;
  }
  auto* absl_nonnull header = static_cast<AllocationHeader*>(pointer) - 1;
  if (header->magic != kAllocationMagic || header->heap == nullptr) {
    SymbianRuntimeExit(SymbianRuntimeExitReason::kRuntimeContractFailure);
  }
  RHeap* absl_nullable heap = header->heap;
  header->magic = 0;
  heap->Free(header);
  heap->Close();
#endif
}

extern "C" void* absl_nonnull SymbianRuntimeHeapIdentity() {
  return &User::Heap();
}

extern "C" [[noreturn]] void SymbianRuntimeExit(
    SymbianRuntimeExitReason reason) {
  User::Exit(static_cast<TInt>(reason));
  __builtin_unreachable();
}

extern "C" int SymbianRuntimeAllocationCells() {
  return User::CountAllocCells();
}

extern "C" void SymbianRuntimeCollectAllocations() {
#ifdef SYMBIAN_RUNTIME_MIMALLOC
  SymbianRuntimeMimallocCollect();
#endif
}

extern "C" void SymbianRuntimeThreadCacheEnter() {
#ifdef SYMBIAN_RUNTIME_MIMALLOC
  SymbianRuntimeMimallocEnterThread();
#endif
}

extern "C" void SymbianRuntimeThreadCacheLeave() {
#ifdef SYMBIAN_RUNTIME_MIMALLOC
  SymbianRuntimeMimallocLeaveThread();
#endif
}
