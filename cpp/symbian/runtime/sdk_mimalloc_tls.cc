// Copyright 2026 Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include <cstdint>

#include <absl/base/nullability.h>
#include <e32std.h>
#include <pthread.h>

namespace {
// One cache line is private to one live OS thread. A collision takes the
// original pthread route; no probing or cross-thread eviction is required.
constexpr unsigned int kSlotCount = 256;
constexpr pthread_key_t kInvalidKey = ~pthread_key_t{0};

struct Slot {
  unsigned int state;  // 0 empty, 1 publishing, 2 live, 3 clearing.
  unsigned int id_low;
  unsigned int id_high;
  pthread_key_t keys[2];
  void* absl_nullable values[2];
};

Slot g_slots[kSlotCount] = {};
unsigned int g_entries = 0;

struct ThreadIdentity {
  unsigned int low;
  unsigned int high;
  unsigned int index;
};

ThreadIdentity CurrentThread() {
  const std::uint64_t id = RThread().Id().Id();
  const unsigned int low = static_cast<unsigned int>(id);
  const unsigned int high = static_cast<unsigned int>(id >> 32);
  return {low, high, (low ^ high) & (kSlotCount - 1)};
}

Slot* absl_nullable Find(ThreadIdentity thread) {
  Slot* absl_nonnull slot = &g_slots[thread.index];
  if (__atomic_load_n(&slot->state, __ATOMIC_ACQUIRE) != 2 ||
      __atomic_load_n(&slot->id_low, __ATOMIC_RELAXED) != thread.low ||
      __atomic_load_n(&slot->id_high, __ATOMIC_RELAXED) != thread.high) {
    return nullptr;
  }
  return slot;
}

Slot* absl_nullable FindOrClaim(ThreadIdentity thread) {
  if (Slot* absl_nullable slot = Find(thread)) {
    return slot;
  }
  Slot* absl_nonnull slot = &g_slots[thread.index];
  unsigned int empty = 0;
  if (!__atomic_compare_exchange_n(&slot->state, &empty, 1, false,
                                   __ATOMIC_ACQ_REL, __ATOMIC_RELAXED)) {
    return nullptr;
  }
  __atomic_store_n(&slot->id_low, thread.low, __ATOMIC_RELAXED);
  __atomic_store_n(&slot->id_high, thread.high, __ATOMIC_RELAXED);
  slot->keys[0] = kInvalidKey;
  slot->keys[1] = kInvalidKey;
  slot->values[0] = nullptr;
  slot->values[1] = nullptr;
  __atomic_fetch_add(&g_entries, 1U, __ATOMIC_RELAXED);
  __atomic_store_n(&slot->state, 2, __ATOMIC_RELEASE);
  return slot;
}
}  // namespace

extern "C" void SymbianMimallocCacheEnter(pthread_key_t default_key,
                                          pthread_key_t cached_key) {
  Slot* absl_nullable slot = FindOrClaim(CurrentThread());
  if (slot == nullptr) {
    return;
  }
  if (default_key != kInvalidKey) {
    slot->keys[0] = default_key;
    slot->values[0] = pthread_getspecific(default_key);
  }
  if (cached_key != kInvalidKey) {
    slot->keys[1] = cached_key;
    slot->values[1] = pthread_getspecific(cached_key);
  }
}

extern "C" void* absl_nullable SymbianMimallocGetSpecific(pthread_key_t key) {
  if (Slot* absl_nullable slot = Find(CurrentThread())) {
    if (slot->keys[0] == key) {
      return slot->values[0];
    }
    if (slot->keys[1] == key) {
      return slot->values[1];
    }
  }
  return pthread_getspecific(key);
}

extern "C" int SymbianMimallocSetSpecific(pthread_key_t key,
                                          const void* absl_nullable value) {
  const int result = pthread_setspecific(key, value);
  if (result != 0) {
    return result;
  }
  Slot* absl_nullable slot = Find(CurrentThread());
  if (slot != nullptr) {
    for (int i = 0; i < 2; ++i) {
      if (slot->keys[i] == key) {
        slot->values[i] = const_cast<void*>(value);
        break;
      }
    }
  }
  return result;
}

extern "C" void SymbianMimallocForgetThread() {
  const ThreadIdentity thread = CurrentThread();
  Slot* absl_nullable slot = Find(thread);
  if (slot == nullptr) {
    return;
  }
  unsigned int live = 2;
  if (!__atomic_compare_exchange_n(&slot->state, &live, 3, false,
                                   __ATOMIC_ACQ_REL, __ATOMIC_RELAXED)) {
    return;
  }
  slot->values[0] = nullptr;
  slot->values[1] = nullptr;
  slot->keys[0] = kInvalidKey;
  slot->keys[1] = kInvalidKey;
  __atomic_fetch_sub(&g_entries, 1U, __ATOMIC_RELAXED);
  __atomic_store_n(&slot->state, 0, __ATOMIC_RELEASE);
}

extern "C" unsigned int SymbianMimallocCacheEntries() {
  return __atomic_load_n(&g_entries, __ATOMIC_RELAXED);
}
