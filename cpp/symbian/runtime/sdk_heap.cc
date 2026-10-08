#include <cstdint>

#include <absl/base/nullability.h>
#include <e32std.h>

#include "abi.h"

#ifdef SYMBIAN_RUNTIME_MIMALLOC
extern "C" void* absl_nullable SymbianRuntimeMimallocAllocate(
    unsigned int size);
extern "C" void* absl_nullable SymbianRuntimeMimallocReallocate(
    void* absl_nullable pointer, unsigned int size);
extern "C" void SymbianRuntimeMimallocFree(void* absl_nullable pointer);
extern "C" void SymbianRuntimeMimallocCollect();
extern "C" void SymbianRuntimeMimallocEnterThread();
extern "C" void SymbianRuntimeMimallocLeaveThread();
#endif

#ifndef SYMBIAN_RUNTIME_MIMALLOC
namespace {
struct alignas(8) AllocationHeader {
  RHeap* absl_nonnull heap;
  std::uint32_t magic;
  std::uint32_t size;
};

static_assert(sizeof(AllocationHeader) == 16);
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
  // Open pins that exact heap until the matching cross-thread free. The
  // legacy EUSER profile cannot use RHeap::Open on its worker thread; its
  // pthread adapter shares the creating process heap, whose owner must live
  // until all worker allocations have been released.
#ifndef SYMBIAN_RUNTIME_LEGACY_EUSER
  if (heap->Open() != KErrNone) {
    return nullptr;
  }
#endif
  auto* absl_nullable header = static_cast<AllocationHeader*>(
      heap->Alloc(static_cast<TInt>(size + sizeof(AllocationHeader))));
  if (header == nullptr) {
#ifndef SYMBIAN_RUNTIME_LEGACY_EUSER
    heap->Close();
#endif
    return nullptr;
  }
  header->heap = heap;
  header->magic = kAllocationMagic;
  header->size = size;
  return header + 1;
#endif
}

extern "C" void* absl_nullable SymbianRuntimeReallocate(
    void* absl_nullable pointer, unsigned int size) {
  if (pointer == nullptr) {
    return SymbianRuntimeAllocate(size);
  }
  if (size == 0) {
    SymbianRuntimeFree(pointer);
    return nullptr;
  }
#ifdef SYMBIAN_RUNTIME_MIMALLOC
  return SymbianRuntimeMimallocReallocate(pointer, size);
#else
  auto* absl_nonnull header = static_cast<AllocationHeader*>(pointer) - 1;
  if (header->magic != kAllocationMagic || header->heap == nullptr) {
    SymbianRuntimeExit(SymbianRuntimeExitReason::kRuntimeContractFailure);
  }
  void* absl_nullable replacement = SymbianRuntimeAllocate(size);
  if (replacement == nullptr) {
    return nullptr;
  }
  const unsigned int copied = size < header->size ? size : header->size;
  Mem::Copy(replacement, pointer, static_cast<TInt>(copied));
  SymbianRuntimeFree(pointer);
  return replacement;
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
#ifndef SYMBIAN_RUNTIME_LEGACY_EUSER
  heap->Close();
#endif
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
