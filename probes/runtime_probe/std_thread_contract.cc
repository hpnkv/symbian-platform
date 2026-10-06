#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <utility>

#include <absl/base/nullability.h>

#include "abi.h"

extern "C" int SymbianRuntimeRunThread(
    void* absl_nullable state, int (*absl_nonnull worker)(void* absl_nullable),
    int (*absl_nonnull parent)(void* absl_nullable));
extern "C" void SymbianRuntimeThreadYield();
#ifdef SYMBIAN_RUNTIME_MIMALLOC
extern "C" unsigned int SymbianRuntimeMimallocChunkCount();
extern "C" unsigned int SymbianRuntimeMimallocChunkBytes();
extern "C" unsigned int SymbianRuntimeMimallocReservedBytes();
extern "C" unsigned int SymbianRuntimeMimallocAddressBudget();
extern "C" unsigned int SymbianMimallocCacheEntries();
extern "C" int SymbianRuntimeRunManagedThread(
    void* absl_nullable state, int (*absl_nonnull worker)(void* absl_nullable),
    int (*absl_nonnull parent)(void* absl_nullable));
#endif

namespace {
struct CrossHeapState {
  void* absl_nullable heap = nullptr;
  std::string* absl_nullable text = nullptr;
  SymbianRuntimePageOwner* absl_nullable page_owner = nullptr;
  void* absl_nullable pages = nullptr;
  int page_size = 0;
  int page_result = -1;
};

struct LiveProducerState {
  std::atomic<int> phase{0};
  std::string* absl_nullable text = nullptr;
  void* absl_nullable producer_heap = nullptr;
  void* absl_nullable consumer_heap = nullptr;
};

int Nothing(void* absl_nullable) {
  return 0;
}

#ifdef SYMBIAN_RUNTIME_MIMALLOC
struct ManagedCacheState {
  unsigned int expected_entries = 0;
  int hits = 0;
};

int AllocateAndFree(void* absl_nullable) {
  auto* absl_nonnull value = new std::string(96, 't');
  if (value->size() != 96 || (*value)[95] != 't') {
    delete value;
    return -175;
  }
  delete value;
  return 0;
}

int ManagedAllocateAndFree(void* absl_nonnull opaque) {
  auto& state = *static_cast<ManagedCacheState*>(opaque);
  if (SymbianMimallocCacheEntries() == state.expected_entries + 1) {
    ++state.hits;
  }
  return AllocateAndFree(nullptr);
}
#endif

int ProduceOnPrivateHeap(void* absl_nonnull opaque) {
  auto& state = *static_cast<CrossHeapState*>(opaque);
  state.heap = SymbianRuntimeHeapIdentity();
  state.text = new std::string(257, 'w');
  state.page_result =
      SymbianRuntimePageCreate(static_cast<unsigned int>(state.page_size),
                               &state.page_owner, &state.pages);
  if (state.page_result == 0) {
    static_cast<unsigned char*>(state.pages)[state.page_size - 1] = 0x5a;
  }
  return 0;
}

int DisposeOnPrivateHeap(void* absl_nonnull opaque) {
  auto& state = *static_cast<CrossHeapState*>(opaque);
  state.heap = SymbianRuntimeHeapIdentity();
  delete state.text;
  state.text = nullptr;
  return 0;
}

int HoldProducerAlive(void* absl_nonnull opaque) {
  auto& state = *static_cast<LiveProducerState*>(opaque);
  state.producer_heap = SymbianRuntimeHeapIdentity();
  state.text = new std::string(257, 'l');
  state.phase.store(1, std::memory_order_release);
  for (int i = 0; i < 1000; ++i) {
    if (state.phase.load(std::memory_order_acquire) == 2) {
      return 0;
    }
    SymbianRuntimeThreadYield();
  }
  return -161;
}

int FreeWhileProducerAlive(void* absl_nonnull opaque) {
  auto& state = *static_cast<LiveProducerState*>(opaque);
  state.consumer_heap = SymbianRuntimeHeapIdentity();
  for (int i = 0; i < 1000; ++i) {
    if (state.phase.load(std::memory_order_acquire) == 1) {
      break;
    }
    SymbianRuntimeThreadYield();
  }
  if (state.phase.load(std::memory_order_acquire) != 1 ||
      state.text == nullptr || state.text->size() != 257 ||
      (*state.text)[256] != 'l') {
    state.phase.store(2, std::memory_order_release);
    return -162;
  }
  delete state.text;
  state.text = nullptr;
  state.phase.store(2, std::memory_order_release);
  return 0;
}
}  // namespace

extern "C" int SymbianRuntimeStdThreadProbe() {
#ifdef SYMBIAN_RUNTIME_MIMALLOC
  void* absl_nullable const warm = SymbianRuntimeAllocate(1);
  if (warm == nullptr) {
    return -176;
  }
  SymbianRuntimeFree(warm);
  const unsigned int before_cache_entries = SymbianMimallocCacheEntries();
  if (before_cache_entries != 1) {
    return -177;
  }
#endif
  const int before = SymbianRuntimeAllocationCells();
#ifdef SYMBIAN_RUNTIME_MIMALLOC
  const unsigned int before_bytes = SymbianRuntimeMimallocChunkBytes();
#endif
  std::atomic<int> counter{0};
  std::atomic<int> completed{0};
  auto shared = std::make_shared<int>(2026);
  std::weak_ptr<int> weak = shared;
  auto owned = std::make_unique<int>(14);
  std::thread worker(
      [shared, owned = std::move(owned), &counter, &completed]() mutable {
        for (int i = 0; i < 2000; ++i) {
          counter.fetch_add(1, std::memory_order_acq_rel);
        }
        completed.store(*shared + *owned, std::memory_order_release);
      });
  for (int i = 0; i < 2000; ++i) {
    counter.fetch_add(1, std::memory_order_acq_rel);
  }
  worker.join();
#ifdef SYMBIAN_RUNTIME_MIMALLOC
  if (SymbianMimallocCacheEntries() != before_cache_entries) {
    return -180 - static_cast<int>(SymbianMimallocCacheEntries());
  }
#endif
  if (counter.load(std::memory_order_acquire) != 4000 || *shared != 2026 ||
      shared.use_count() != 1 ||
      completed.load(std::memory_order_acquire) != 2040 || owned) {
    return -151;
  }
  shared.reset();
  if (!weak.expired()) {
    return -152;
  }
  weak.reset();
  void* absl_nullable const event_heap = SymbianRuntimeHeapIdentity();
  CrossHeapState cross_heap;
  const int page_size = SymbianRuntimePageSize();
  if (page_size <= 0) {
    return -155;
  }
  cross_heap.page_size = page_size;
  if (SymbianRuntimeRunThread(&cross_heap, ProduceOnPrivateHeap, Nothing) !=
      0) {
    return -156;
  }
#ifdef SYMBIAN_RUNTIME_MIMALLOC
  if (SymbianMimallocCacheEntries() != before_cache_entries) {
    return -178;
  }
#endif
  if (cross_heap.heap == nullptr || cross_heap.heap == event_heap) {
    return -158;
  }
  if (cross_heap.text == nullptr || cross_heap.text->size() != 257 ||
      (*cross_heap.text)[256] != 'w') {
    return -159;
  }
  if (cross_heap.page_result != 0 || cross_heap.page_owner == nullptr ||
      cross_heap.pages == nullptr ||
      static_cast<unsigned char*>(cross_heap.pages)[page_size - 1] != 0x5a) {
    return -157;
  }
  // The producer's thread heap has closed. Both allocations retain that
  // exact heap until their owner metadata is freed on this event thread.
  delete cross_heap.text;
  SymbianRuntimePageClose(cross_heap.page_owner);
  cross_heap.text = new std::string(257, 'e');
  cross_heap.heap = nullptr;
  if (SymbianRuntimeRunThread(&cross_heap, DisposeOnPrivateHeap, Nothing) !=
          0 ||
      cross_heap.heap == nullptr || cross_heap.heap == event_heap ||
      cross_heap.text != nullptr) {
    return -160;
  }
  LiveProducerState live;
  if (SymbianRuntimeRunThread(&live, HoldProducerAlive,
                              FreeWhileProducerAlive) != 0 ||
      live.phase.load(std::memory_order_acquire) != 2 || live.text != nullptr ||
      live.producer_heap == nullptr ||
      live.producer_heap == live.consumer_heap) {
    return -163;
  }
  SymbianRuntimeCollectAllocations();
#ifdef SYMBIAN_RUNTIME_MIMALLOC
  if (SymbianMimallocCacheEntries() != before_cache_entries) {
    return -178;
  }
  for (int i = 0; i < 16; ++i) {
    if (SymbianRuntimeRunThread(nullptr, AllocateAndFree, Nothing) != 0 ||
        SymbianMimallocCacheEntries() != before_cache_entries) {
      return -179;
    }
  }
  ManagedCacheState managed{before_cache_entries, 0};
  for (int i = 0; i < 16; ++i) {
    if (SymbianRuntimeRunManagedThread(&managed, ManagedAllocateAndFree,
                                       Nothing) != 0 ||
        SymbianMimallocCacheEntries() != before_cache_entries) {
      return -181;
    }
  }
  if (managed.hits == 0) {
    return -182;
  }
#endif
  if (SymbianRuntimeAllocationCells() != before) {
    return -153;
  }
#ifdef SYMBIAN_RUNTIME_MIMALLOC
  if (SymbianRuntimeMimallocChunkBytes() > before_bytes + 2 * 1024 * 1024) {
    return -164;
  }
  const unsigned int resting_bytes = SymbianRuntimeMimallocChunkBytes();
  for (int repeat = 0; repeat < 2; ++repeat) {
    void* absl_nullable allocations[512] = {};
    for (void* absl_nullable& allocation : allocations) {
      allocation = SymbianRuntimeAllocate(1024);
      if (allocation == nullptr) {
        return -165;
      }
      static_cast<unsigned char*>(allocation)[1023] = 0x5a;
    }
    if (SymbianRuntimeMimallocChunkBytes() > resting_bytes + 2 * 1024 * 1024) {
      return -166;
    }
    for (void* absl_nullable allocation : allocations) {
      if (static_cast<unsigned char*>(allocation)[1023] != 0x5a) {
        return -167;
      }
      SymbianRuntimeFree(allocation);
    }
    SymbianRuntimeCollectAllocations();
    if (SymbianRuntimeMimallocChunkBytes() > resting_bytes + 512 * 1024) {
      return -168;
    }
  }
  const unsigned int address_budget = SymbianRuntimeMimallocAddressBudget();
  if (SymbianRuntimeMimallocReservedBytes() > address_budget ||
      SymbianRuntimeAllocate(address_budget + 8 * 1024 * 1024) != nullptr ||
      SymbianRuntimeMimallocReservedBytes() > address_budget) {
    return -171;
  }
#endif
#ifdef SYMBIAN_RUNTIME_CHANGED_STD_THREAD
  return -154;
#else
  return 0;
#endif
}
