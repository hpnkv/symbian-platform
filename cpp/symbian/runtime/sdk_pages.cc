#include <e32hal.h>
#include <e32std.h>

#include "abi.h"

struct SymbianRuntimePageOwner {
  RChunk chunk;
  RHeap* heap = nullptr;
  SymbianRuntimePageOwner* next = nullptr;
};

extern "C" int SymbianRuntimePageSize() {
  TInt page_size = 0;
  if (UserHal::PageSizeInBytes(page_size) != KErrNone || page_size < 1024 ||
      page_size > 32768 || (page_size & (page_size - 1)) != 0) {
    return KErrNotSupported;
  }
  return page_size;
}

extern "C" int SymbianRuntimePageCreate(unsigned int bytes,
                                        SymbianRuntimePageOwner** owner,
                                        void** pages) {
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
  RHeap* heap = &User::Heap();
  if (heap->Open() != KErrNone) {
    return KErrGeneral;
  }
  void* storage = heap->Alloc(sizeof(SymbianRuntimePageOwner));
  if (storage == nullptr) {
    heap->Close();
    return KErrNoMemory;
  }
  auto* created = new (storage) SymbianRuntimePageOwner;
  created->heap = heap;
  const TInt result = created->chunk.CreateLocal(
      static_cast<TInt>(bytes), static_cast<TInt>(bytes), EOwnerProcess);
  if (result != KErrNone) {
    created->~SymbianRuntimePageOwner();
    heap->Free(storage);
    heap->Close();
    return result;
  }
  TUint8* base = created->chunk.Base();
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

extern "C" void SymbianRuntimePageClose(SymbianRuntimePageOwner* owner) {
  if (owner == nullptr) {
    return;
  }
  RHeap* heap = owner->heap;
  owner->chunk.Close();
  owner->~SymbianRuntimePageOwner();
  heap->Free(owner);
  heap->Close();
}

extern "C" void SymbianRuntimePageSetNext(SymbianRuntimePageOwner* owner,
                                          SymbianRuntimePageOwner* next) {
  if (owner != nullptr) {
    owner->next = next;
  }
}

extern "C" SymbianRuntimePageOwner* SymbianRuntimePageNext(
    SymbianRuntimePageOwner* owner) {
  return owner == nullptr ? nullptr : owner->next;
}
