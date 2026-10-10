#include <absl/base/nullability.h>
#include <e32hal.h>
#include <e32std.h>

#include "abi.h"

struct SymbianRuntimePageOwner {
  RChunk chunk;
  RHeap* absl_nullable heap = nullptr;
  SymbianRuntimePageOwner* absl_nullable next = nullptr;
};

extern "C" int SymbianRuntimePageSize() {
  TInt page_size = 0;
  if (UserHal::PageSizeInBytes(page_size) != KErrNone || page_size < 1024 ||
      page_size > 32768 || (page_size & (page_size - 1)) != 0) {
    return KErrNotSupported;
  }
  return page_size;
}

extern "C" int SymbianRuntimePageCreate(
    unsigned int bytes,
    SymbianRuntimePageOwner* absl_nullable* absl_nullable owner,
    void* absl_nullable* absl_nullable pages) {
  if (owner == nullptr || pages == nullptr) {
    return KErrArgument;
  }
  *owner = nullptr;
  *pages = nullptr;
  const int page_size = SymbianRuntimePageSize();
  if (page_size < 0) {
    return page_size;
  }
  if (bytes == 0 || bytes > 0x7fffffffU ||
      (bytes & static_cast<unsigned>(page_size - 1)) != 0) {
    return KErrArgument;
  }
  RHeap* absl_nonnull heap = &User::Heap();
  if (heap->Open() != KErrNone) {
    return KErrGeneral;
  }
  void* absl_nullable storage = heap->Alloc(sizeof(SymbianRuntimePageOwner));
  if (storage == nullptr) {
    heap->Close();
    return KErrNoMemory;
  }
  auto* absl_nonnull created = new (storage) SymbianRuntimePageOwner;
  created->heap = heap;
  if (const TInt result = created->chunk.CreateLocal(
          static_cast<TInt>(bytes), static_cast<TInt>(bytes), EOwnerProcess);
      result != KErrNone) {
    created->~SymbianRuntimePageOwner();
    heap->Free(storage);
    heap->Close();
    return result;
  }
  TUint8* absl_nullable base = created->chunk.Base();
  if (base == nullptr ||
      (reinterpret_cast<TUintPtr>(base) & (page_size - 1)) != 0 ||
      created->chunk.Size() < static_cast<TInt>(bytes)) {
    created->chunk.Close();
    created->~SymbianRuntimePageOwner();
    heap->Free(storage);
    heap->Close();
    return KErrNotSupported;
  }
  *owner = created;
  *pages = base;
  return KErrNone;
}

extern "C" void SymbianRuntimePageClose(
    SymbianRuntimePageOwner* absl_nullable owner) {
  if (owner == nullptr) {
    return;
  }
  RHeap* absl_nonnull heap = owner->heap;
  owner->chunk.Close();
  owner->~SymbianRuntimePageOwner();
  heap->Free(owner);
  heap->Close();
}

extern "C" void SymbianRuntimePageSetNext(
    SymbianRuntimePageOwner* absl_nullable owner,
    SymbianRuntimePageOwner* absl_nullable next) {
  if (owner != nullptr) {
    owner->next = next;
  }
}

extern "C" SymbianRuntimePageOwner* absl_nonnull SymbianRuntimePageNext(
    SymbianRuntimePageOwner* absl_nullable owner) {
  return owner == nullptr ? nullptr : owner->next;
}
