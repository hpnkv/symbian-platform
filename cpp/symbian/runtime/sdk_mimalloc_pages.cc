// Copyright 2026 Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.
#include <cstddef>
#include <cstdint>

#include <e32std.h>

#include "abi.h"

namespace {
// Page requests are rare. A bounded process table keeps chunk handles alive
// across thread exit without allocating metadata from a thread-local heap.
constexpr unsigned int kMaxChunks = 64;
constexpr unsigned int kAddressBudget = SYMBIAN_MIMALLOC_ADDRESS_BUDGET_BYTES;

struct ChunkSlot {
  unsigned int state;  // 0 free, 1 publishing, 2 live, 3 closing.
  TInt handle;
  uintptr_t address;
  unsigned int size;
  unsigned int reserved;
  unsigned int committed;
};

ChunkSlot g_chunks[kMaxChunks] = {};
unsigned int g_chunk_count = 0;
unsigned int g_reserved_bytes = 0;
unsigned int g_committed_bytes = 0;

ChunkSlot* ReserveSlot() {
  for (auto& slot : g_chunks) {
    unsigned int expected = 0;
    if (__atomic_compare_exchange_n(&slot.state, &expected, 1, false,
                                    __ATOMIC_ACQ_REL, __ATOMIC_RELAXED)) {
      return &slot;
    }
  }
  return nullptr;
}

ChunkSlot* FindRange(void* address, size_t bytes) {
  const uintptr_t start = reinterpret_cast<uintptr_t>(address);
  if (bytes > UINT32_MAX || start > UINT32_MAX - bytes) {
    return nullptr;
  }
  for (auto& slot : g_chunks) {
    if (__atomic_load_n(&slot.state, __ATOMIC_ACQUIRE) == 2 &&
        start >= slot.address && start - slot.address <= slot.size &&
        bytes <= slot.size - (start - slot.address)) {
      return &slot;
    }
  }
  return nullptr;
}

bool PageMultiple(size_t value, int page_size) {
  return value != 0 && (value & static_cast<size_t>(page_size - 1)) == 0;
}

bool ReserveAddressBudget(unsigned int capacity) {
  unsigned int prior = __atomic_load_n(&g_reserved_bytes, __ATOMIC_RELAXED);
  while (prior <= kAddressBudget && capacity <= kAddressBudget - prior) {
    if (__atomic_compare_exchange_n(&g_reserved_bytes, &prior, prior + capacity,
                                    false, __ATOMIC_ACQ_REL,
                                    __ATOMIC_RELAXED)) {
      return true;
    }
  }
  return false;
}
}  // namespace

extern "C" int SymbianRuntimeMimallocPageSize() {
  return SymbianRuntimePageSize();
}

extern "C" void* SymbianRuntimeMimallocReserve(size_t bytes, size_t alignment,
                                               bool commit) {
  const int page_size = SymbianRuntimePageSize();
  if (page_size <= 0 || !PageMultiple(bytes, page_size)) {
    return nullptr;
  }
  if (alignment < static_cast<size_t>(page_size)) {
    alignment = page_size;
  }
  if ((alignment & (alignment - 1)) != 0 ||
      alignment > static_cast<size_t>(KMaxTInt) ||
      bytes > static_cast<size_t>(KMaxTInt) - alignment) {
    return nullptr;
  }
  ChunkSlot* slot = ReserveSlot();
  if (slot == nullptr) {
    return nullptr;
  }
  const TInt capacity = static_cast<TInt>(bytes + alignment);
  if (!ReserveAddressBudget(static_cast<unsigned int>(capacity))) {
    __atomic_store_n(&slot->state, 0, __ATOMIC_RELEASE);
    return nullptr;
  }
  RChunk chunk;
  const TInt create_result =
      chunk.CreateDisconnectedLocal(0, 0, capacity, EOwnerProcess);
  if (create_result != KErrNone) {
    __atomic_fetch_sub(&g_reserved_bytes, static_cast<unsigned int>(capacity),
                       __ATOMIC_RELAXED);
    __atomic_store_n(&slot->state, 0, __ATOMIC_RELEASE);
    return nullptr;
  }
  const uintptr_t base = reinterpret_cast<uintptr_t>(chunk.Base());
  const uintptr_t aligned = (base + alignment - 1) & ~(alignment - 1);
  if (base == 0 || aligned < base ||
      aligned - base > static_cast<uintptr_t>(capacity) - bytes) {
    chunk.Close();
    __atomic_fetch_sub(&g_reserved_bytes, static_cast<unsigned int>(capacity),
                       __ATOMIC_RELAXED);
    __atomic_store_n(&slot->state, 0, __ATOMIC_RELEASE);
    return nullptr;
  }
  const TInt commit_result =
      commit ? chunk.Commit(static_cast<TInt>(aligned - base),
                            static_cast<TInt>(bytes))
             : KErrNone;
  if (commit_result != KErrNone) {
    chunk.Close();
    __atomic_fetch_sub(&g_reserved_bytes, static_cast<unsigned int>(capacity),
                       __ATOMIC_RELAXED);
    __atomic_store_n(&slot->state, 0, __ATOMIC_RELEASE);
    return nullptr;
  }
  slot->handle = chunk.Handle();
  slot->address = aligned;
  slot->size = static_cast<unsigned int>(bytes);
  slot->reserved = static_cast<unsigned int>(capacity);
  slot->committed = commit ? static_cast<unsigned int>(bytes) : 0;
  __atomic_fetch_add(&g_chunk_count, 1U, __ATOMIC_RELAXED);
  if (commit) {
    __atomic_fetch_add(&g_committed_bytes, slot->committed, __ATOMIC_RELAXED);
  }
  __atomic_store_n(&slot->state, 2, __ATOMIC_RELEASE);
  return reinterpret_cast<void*>(aligned);
}

extern "C" int SymbianRuntimeMimallocCommit(void* address, size_t bytes) {
  const int page_size = SymbianRuntimePageSize();
  if (page_size <= 0 || !PageMultiple(bytes, page_size)) {
    return KErrArgument;
  }
  ChunkSlot* slot = FindRange(address, bytes);
  if (slot == nullptr) {
    return KErrArgument;
  }
  RChunk chunk;
  chunk.SetHandle(slot->handle);
  const uintptr_t base = reinterpret_cast<uintptr_t>(chunk.Base());
  const TInt offset =
      static_cast<TInt>(reinterpret_cast<uintptr_t>(address) - base);
  TInt result = chunk.Commit(offset, static_cast<TInt>(bytes));
  size_t newly_committed = result == KErrNone ? bytes : 0;
  if (result == KErrAlreadyExists) {
    result = KErrNone;
    for (size_t page = 0; page < bytes; page += page_size) {
      const TInt page_result =
          chunk.Commit(offset + static_cast<TInt>(page), page_size);
      if (page_result == KErrNone) {
        newly_committed += page_size;
      } else if (page_result != KErrAlreadyExists) {
        result = page_result;
        break;
      }
    }
  }
  if (newly_committed != 0) {
    __atomic_fetch_add(&slot->committed,
                       static_cast<unsigned int>(newly_committed),
                       __ATOMIC_RELAXED);
    __atomic_fetch_add(&g_committed_bytes,
                       static_cast<unsigned int>(newly_committed),
                       __ATOMIC_RELAXED);
  }
  return result;
}

extern "C" int SymbianRuntimeMimallocDecommit(void* address, size_t bytes) {
  const int page_size = SymbianRuntimePageSize();
  if (page_size <= 0 || !PageMultiple(bytes, page_size)) {
    return KErrArgument;
  }
  ChunkSlot* slot = FindRange(address, bytes);
  if (slot == nullptr) {
    return KErrArgument;
  }
  RChunk chunk;
  chunk.SetHandle(slot->handle);
  const uintptr_t base = reinterpret_cast<uintptr_t>(chunk.Base());
  const TInt result = chunk.Decommit(
      static_cast<TInt>(reinterpret_cast<uintptr_t>(address) - base),
      static_cast<TInt>(bytes));
  if (result == KErrNone) {
    __atomic_fetch_sub(&slot->committed, static_cast<unsigned int>(bytes),
                       __ATOMIC_RELAXED);
    __atomic_fetch_sub(&g_committed_bytes, static_cast<unsigned int>(bytes),
                       __ATOMIC_RELAXED);
  }
  return result;
}

extern "C" int SymbianRuntimeMimallocRelease(void* address) {
  ChunkSlot* slot = nullptr;
  for (auto& candidate : g_chunks) {
    if (__atomic_load_n(&candidate.state, __ATOMIC_ACQUIRE) == 2 &&
        candidate.address == reinterpret_cast<uintptr_t>(address)) {
      unsigned int expected = 2;
      if (__atomic_compare_exchange_n(&candidate.state, &expected, 3, false,
                                      __ATOMIC_ACQ_REL, __ATOMIC_RELAXED)) {
        slot = &candidate;
        break;
      }
    }
  }
  if (slot == nullptr) {
    return KErrArgument;
  }
  RChunk chunk;
  chunk.SetHandle(slot->handle);
  const unsigned int reserved = slot->reserved;
  const unsigned int committed =
      __atomic_load_n(&slot->committed, __ATOMIC_RELAXED);
  chunk.Close();
  __atomic_fetch_sub(&g_chunk_count, 1U, __ATOMIC_RELAXED);
  __atomic_fetch_sub(&g_reserved_bytes, reserved, __ATOMIC_RELAXED);
  __atomic_fetch_sub(&g_committed_bytes, committed, __ATOMIC_RELAXED);
  slot->handle = 0;
  slot->address = 0;
  slot->size = 0;
  slot->reserved = 0;
  slot->committed = 0;
  __atomic_store_n(&slot->state, 0, __ATOMIC_RELEASE);
  return KErrNone;
}

extern "C" unsigned int SymbianRuntimeMimallocChunkCount() {
  return __atomic_load_n(&g_chunk_count, __ATOMIC_RELAXED);
}

extern "C" unsigned int SymbianRuntimeMimallocChunkBytes() {
  return __atomic_load_n(&g_committed_bytes, __ATOMIC_RELAXED);
}

extern "C" unsigned int SymbianRuntimeMimallocReservedBytes() {
  return __atomic_load_n(&g_reserved_bytes, __ATOMIC_RELAXED);
}

extern "C" unsigned int SymbianRuntimeMimallocAddressBudget() {
  return kAddressBudget;
}

extern "C" uintptr_t SymbianRuntimeMimallocThreadId() {
  const TUint64 id = RThread().Id().Id();
  return static_cast<uintptr_t>((id ^ (id >> 30)) << 2);
}

extern "C" void SymbianRuntimeMimallocYield() {
  User::After(TTimeIntervalMicroSeconds32(0));
}

extern "C" long long SymbianRuntimeMimallocClockMillis() {
  const int period = SymbianRuntimeTickPeriodMicros();
  if (period <= 0) {
    return 0;
  }
  return static_cast<long long>(SymbianRuntimeTickCount()) * period / 1000;
}
