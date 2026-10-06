#include <absl/base/nullability.h>
#include <e32hal.h>
#include <e32std.h>

#include "abi.h"

// Keep the original platform header in a narrow native bridge. The page
// source must be verified before Abseil's LowLevelAlloc can use it.
extern "C" int SymbianRuntimeChunkProbe() {
  TInt page_size = 0;
  if (UserHal::PageSizeInBytes(page_size) != KErrNone || page_size < 1024 ||
      page_size > 32768 || (page_size & (page_size - 1)) != 0) {
    return -206;
  }
  for (int iteration = 0; iteration < 16; ++iteration) {
    RChunk chunk;
    if (chunk.CreateLocal(page_size, page_size * 2, EOwnerProcess) !=
        KErrNone) {
      return -200;
    }
    TUint8* absl_nullable base = chunk.Base();
    if (base == nullptr || chunk.Size() < page_size ||
        chunk.MaxSize() < page_size * 2) {
      chunk.Close();
      return -201;
    }
    base[0] = static_cast<TUint8>(iteration + 1);
    base[page_size - 1] = static_cast<TUint8>(iteration + 2);
    if (base[0] != iteration + 1 || base[page_size - 1] != iteration + 2) {
      chunk.Close();
      return -202;
    }
    if (chunk.Adjust(page_size * 2) != KErrNone ||
        chunk.Size() < page_size * 2) {
      chunk.Close();
      return -203;
    }
    base = chunk.Base();
#ifdef SYMBIAN_RUNTIME_CHANGED_CHUNK
    const TInt expected_last = iteration + 3;
#else
    const TInt expected_last = iteration + 2;
#endif
    if (base == nullptr || base[0] != iteration + 1 ||
        base[page_size - 1] != expected_last) {
      chunk.Close();
#ifdef SYMBIAN_RUNTIME_CHANGED_CHUNK
      return -205;
#else
      return -204;
#endif
    }
    chunk.Close();
  }
  const int baseline = SymbianRuntimeAllocationCells();
  SymbianRuntimePageOwner* absl_nullable owner = nullptr;
  void* absl_nullable pages = nullptr;
  if (SymbianRuntimePageCreate(0, &owner, &pages) != KErrArgument ||
      owner != nullptr || pages != nullptr ||
      SymbianRuntimePageCreate(page_size + 1, &owner, &pages) != KErrArgument) {
    return -207;
  }
  for (int iteration = 1; iteration <= 16; ++iteration) {
    const unsigned int bytes = page_size * (iteration % 4 + 1);
    if (SymbianRuntimePageCreate(bytes, &owner, &pages) != KErrNone ||
        owner == nullptr || pages == nullptr ||
        (reinterpret_cast<TUintPtr>(pages) & (page_size - 1)) != 0) {
      return -208;
    }
    auto* absl_nonnull data = static_cast<TUint8*>(pages);
    data[0] = static_cast<TUint8>(iteration);
    data[bytes - 1] = static_cast<TUint8>(iteration + 1);
    if (data[0] != iteration || data[bytes - 1] != iteration + 1) {
      return -209;
    }
    SymbianRuntimePageClose(owner);
    owner = nullptr;
    pages = nullptr;
  }
  if (SymbianRuntimeAllocationCells() != baseline) {
    return -210;
  }
  return 0;
}
