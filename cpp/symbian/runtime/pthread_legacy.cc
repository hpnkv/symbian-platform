// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include <cstdint>
#include <limits>

#include <absl/base/nullability.h>
#include <e32base.h>
#include <e32std.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>

#include "abi.h"

namespace {

constexpr int kMaxThreads = 64;
constexpr int kMaxKeys = POSIX_THREAD_KEYS_MAX;
constexpr std::int64_t kUnixEpochMicros = 62168256000000000LL;

struct NativeMutex {
#ifdef SYMBIAN_RUNTIME_LEGACY_EUSER
  RSemaphore handle;
#else
  RMutex handle;
#endif
  std::uint32_t owner = 0;
  int depth = 0;
  bool recursive = false;
#ifdef SYMBIAN_RUNTIME_LEGACY_EUSER
  bool available = true;
  int waiters = 0;
#endif
};

#ifdef SYMBIAN_RUNTIME_LEGACY_EUSER
struct WaitNode {
  RSemaphore semaphore;
  WaitNode* absl_nullable next = nullptr;
  bool notified = false;
};
#endif

struct NativeCond {
#ifdef SYMBIAN_RUNTIME_LEGACY_EUSER
  WaitNode* absl_nullable waiters = nullptr;
#else
  RCondVar handle;
#endif
};

struct ThreadRecord {
  RThread handle;
  void* absl_nullable (*absl_nullable function)(void* absl_nullable) = nullptr;
  void* absl_nullable argument = nullptr;
  void* absl_nullable result = nullptr;
  pthread_t id = 0;
  bool detached = false;
  bool joining = false;
  bool complete = false;
};

struct KeyRecord {
  bool active = false;
  void (*absl_nullable destructor)(void* absl_nullable) = nullptr;
};

struct ThreadLocal {
  pthread_t id = 0;
  int error_value = 0;
  std::uint64_t mb_states[8] = {};
  void* absl_nullable values[kMaxKeys] = {};
};

int g_table_lock = 0;
std::uint32_t g_name_counter = 0;
ThreadRecord* absl_nullable g_threads[kMaxThreads] = {};
ThreadLocal* absl_nullable g_locals[kMaxThreads] = {};
KeyRecord g_keys[kMaxKeys] = {};

void LockTable() {
  while (__atomic_exchange_n(&g_table_lock, 1, __ATOMIC_ACQUIRE) != 0) {
    User::After(TTimeIntervalMicroSeconds32(0));
  }
}

void UnlockTable() {
  __atomic_store_n(&g_table_lock, 0, __ATOMIC_RELEASE);
}

template <typename T>
T* absl_nullable Allocate() {
  void* absl_nullable memory = User::AllocZ(sizeof(T));
  return memory == nullptr ? nullptr : new (memory) T;
}

NativeMutex* absl_nullable MakeMutex(bool recursive) {
  NativeMutex* absl_nullable state = Allocate<NativeMutex>();
  if (state == nullptr) {
    return nullptr;
  }
#ifdef SYMBIAN_RUNTIME_LEGACY_EUSER
  const TInt created = state->handle.CreateLocal(1);
#else
  const TInt created = state->handle.CreateLocal();
#endif
  if (created != KErrNone) {
    User::Free(state);
    return nullptr;
  }
  state->recursive = recursive;
  return state;
}

NativeMutex* absl_nullable GetMutex(pthread_mutex_t* absl_nullable mutex) {
  if (mutex == nullptr ||
      __atomic_load_n(&mutex->iState, __ATOMIC_ACQUIRE) == _EDestroyed) {
    return nullptr;
  }
  auto* absl_nullable state = reinterpret_cast<NativeMutex*>(
      __atomic_load_n(&mutex->iPtr, __ATOMIC_ACQUIRE));
  if (state != nullptr) {
    return state;
  }
  NativeMutex* absl_nullable created =
      MakeMutex(__atomic_load_n(&mutex->iState, __ATOMIC_ACQUIRE) ==
                _ENeedsRecursiveInit);
  if (created == nullptr) {
    return nullptr;
  }
  _pthread_mutex_t* absl_nullable expected = nullptr;
  if (!__atomic_compare_exchange_n(&mutex->iPtr, &expected,
                                   reinterpret_cast<_pthread_mutex_t*>(created),
                                   false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
    created->handle.Close();
    User::Free(created);
    return reinterpret_cast<NativeMutex*>(expected);
  }
  __atomic_store_n(&mutex->iState, _EInitialized, __ATOMIC_RELEASE);
  return created;
}

NativeCond* absl_nullable GetCond(pthread_cond_t* absl_nullable condition) {
  if (condition == nullptr ||
      __atomic_load_n(&condition->iState, __ATOMIC_ACQUIRE) == _EDestroyed) {
    return nullptr;
  }
  auto* absl_nullable state = reinterpret_cast<NativeCond*>(
      __atomic_load_n(&condition->iQueue.iHead, __ATOMIC_ACQUIRE));
  if (state != nullptr) {
    return state;
  }
  NativeCond* absl_nullable created = Allocate<NativeCond>();
  if (created == nullptr) {
    return nullptr;
  }
#ifndef SYMBIAN_RUNTIME_LEGACY_EUSER
  if (created->handle.CreateLocal() != KErrNone) {
    User::Free(created);
    return nullptr;
  }
#endif
  _CondNode* absl_nullable expected = nullptr;
  if (!__atomic_compare_exchange_n(&condition->iQueue.iHead, &expected,
                                   reinterpret_cast<_CondNode*>(created), false,
                                   __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
#ifndef SYMBIAN_RUNTIME_LEGACY_EUSER
    created->handle.Close();
#endif
    User::Free(created);
    return reinterpret_cast<NativeCond*>(expected);
  }
  __atomic_store_n(&condition->iState, _EInitialized, __ATOMIC_RELEASE);
  return created;
}

ThreadRecord* absl_nullable FindThread(pthread_t id) {
  for (ThreadRecord* absl_nullable record : g_threads) {
    if (record != nullptr && record->id == id) {
      return record;
    }
  }
  return nullptr;
}

void RemoveThread(ThreadRecord* absl_nonnull record) {
  for (int index = 0; index < kMaxThreads; ++index) {
    if (g_threads[index] == record) {
      g_threads[index] = nullptr;
      break;
    }
  }
}

void FreeThread(ThreadRecord* absl_nonnull record) {
  record->handle.Close();
  record->~ThreadRecord();
  User::Free(record);
}

ThreadLocal* absl_nullable FindLocal(pthread_t id) {
  for (ThreadLocal* absl_nullable local : g_locals) {
    if (local != nullptr && local->id == id) {
      return local;
    }
  }
  return nullptr;
}

ThreadLocal* absl_nullable EnsureLocal(pthread_t id) {
  ThreadLocal* absl_nullable local = FindLocal(id);
  if (local != nullptr) {
    return local;
  }
  local = Allocate<ThreadLocal>();
  if (local == nullptr) {
    return nullptr;
  }
  local->id = id;
  for (int index = 0; index < kMaxThreads; ++index) {
    if (g_locals[index] == nullptr) {
      g_locals[index] = local;
      return local;
    }
  }
  User::Free(local);
  return nullptr;
}

void RunKeyDestructors(pthread_t id) {
  for (int pass = 0; pass < POSIX_THREAD_DESTRUCTOR_ITERATIONS; ++pass) {
    bool called = false;
    for (int key = 0; key < kMaxKeys; ++key) {
      LockTable();
      ThreadLocal* absl_nullable local = FindLocal(id);
      void* absl_nullable value =
          local == nullptr ? nullptr : local->values[key];
      void (*absl_nullable destructor)(void* absl_nullable) =
          g_keys[key].active ? g_keys[key].destructor : nullptr;
      if (local != nullptr) {
        local->values[key] = nullptr;
      }
      UnlockTable();
      if (value != nullptr && destructor != nullptr) {
        destructor(value);
        called = true;
      }
    }
    if (!called) {
      break;
    }
  }
  LockTable();
  for (int index = 0; index < kMaxThreads; ++index) {
    if (g_locals[index] != nullptr && g_locals[index]->id == id) {
      User::Free(g_locals[index]);
      g_locals[index] = nullptr;
      break;
    }
  }
  UnlockTable();
}

TInt ThreadEntry(TAny* absl_nonnull argument) {
  auto* absl_nonnull record = static_cast<ThreadRecord*>(argument);
  User::SetCritical(User::ENotCritical);
  CTrapCleanup* absl_nullable cleanup = CTrapCleanup::New();
  if (cleanup == nullptr) {
    return KErrNoMemory;
  }
  SymbianRuntimeThreadCacheEnter();
  void* absl_nullable result = record->function(record->argument);
  RunKeyDestructors(record->id);
  SymbianRuntimeThreadCacheLeave();
  delete cleanup;
  LockTable();
  record->result = result;
  record->complete = true;
  const bool detached = record->detached;
  if (detached) {
    RemoveThread(record);
  }
  UnlockTable();
  if (detached) {
    FreeThread(record);
  }
  return KErrNone;
}

#ifdef SYMBIAN_RUNTIME_LEGACY_EUSER
void RemoveWaiter(NativeCond* absl_nonnull state, WaitNode* absl_nonnull node) {
  WaitNode* absl_nullable* absl_nonnull slot = &state->waiters;
  while (*slot != node) {
    slot = &(*slot)->next;
  }
  *slot = node->next;
}

int WaitCondLegacy(pthread_cond_t* absl_nullable condition,
                   pthread_mutex_t* absl_nullable mutex,
                   const timespec* absl_nullable absolute) {
  NativeCond* absl_nullable state = GetCond(condition);
  NativeMutex* absl_nullable lock = GetMutex(mutex);
  if (state == nullptr || lock == nullptr ||
      __atomic_load_n(&lock->owner, __ATOMIC_ACQUIRE) != RThread().Id().Id() ||
      lock->depth != 1) {
    return EINVAL;
  }
  std::int64_t deadline = 0;
  if (absolute != nullptr) {
    if (absolute->tv_nsec < 0 || absolute->tv_nsec >= 1000000000 ||
        absolute->tv_sec < 0) {
      return EINVAL;
    }
    const std::int64_t seconds = static_cast<std::int64_t>(absolute->tv_sec);
    const std::int64_t fraction = absolute->tv_nsec / 1000;
    if (seconds > (std::numeric_limits<std::int64_t>::max() - kUnixEpochMicros -
                   fraction) /
                      1000000) {
      return EINVAL;
    }
    deadline = seconds * 1000000 + fraction + kUnixEpochMicros;
  }
  WaitNode node;
  if (node.semaphore.CreateLocal(0) != KErrNone) {
    return ENOMEM;
  }
  LockTable();
  node.next = state->waiters;
  state->waiters = &node;
  UnlockTable();
  if (pthread_mutex_unlock(mutex) != 0) {
    LockTable();
    RemoveWaiter(state, &node);
    UnlockTable();
    node.semaphore.Close();
    return EINVAL;
  }
  if (absolute == nullptr) {
    node.semaphore.Wait();
  } else {
    TTime now;
    now.UniversalTime();
    std::int64_t remaining = deadline - now.Int64();
    while (remaining > 0) {
      const TInt timeout =
          static_cast<TInt>(remaining > KMaxTInt ? KMaxTInt : remaining);
      if (const TInt result = node.semaphore.Wait(timeout);
          result != KErrTimedOut) {
        break;
      }
      now.UniversalTime();
      remaining = deadline - now.Int64();
    }
  }
  LockTable();
  RemoveWaiter(state, &node);
  const bool notified = node.notified;
  UnlockTable();
  node.semaphore.Close();
  return pthread_mutex_lock(mutex) != 0 ? EINVAL : notified ? 0 : ETIMEDOUT;
}
#endif

int WaitCond(pthread_cond_t* absl_nullable condition,
             pthread_mutex_t* absl_nullable mutex,
             const timespec* absl_nullable absolute) {
#ifdef SYMBIAN_RUNTIME_LEGACY_EUSER
  return WaitCondLegacy(condition, mutex, absolute);
#else
  NativeCond* absl_nullable state = GetCond(condition);
  NativeMutex* absl_nullable lock = GetMutex(mutex);
  if (state == nullptr || lock == nullptr ||
      __atomic_load_n(&lock->owner, __ATOMIC_ACQUIRE) != RThread().Id().Id() ||
      lock->depth != 1) {
    return EINVAL;
  }
  __atomic_store_n(&lock->owner, 0, __ATOMIC_RELEASE);
  lock->depth = 0;
  TInt result = KErrNone;
  if (absolute == nullptr) {
    result = state->handle.Wait(lock->handle);
  } else {
    if (absolute->tv_nsec < 0 || absolute->tv_nsec >= 1000000000 ||
        absolute->tv_sec < 0) {
      __atomic_store_n(&lock->owner, RThread().Id().Id(), __ATOMIC_RELEASE);
      lock->depth = 1;
      return EINVAL;
    }
    TTime now;
    now.UniversalTime();
    const std::int64_t seconds = static_cast<std::int64_t>(absolute->tv_sec);
    const std::int64_t fraction = absolute->tv_nsec / 1000;
    if (seconds > (std::numeric_limits<std::int64_t>::max() - kUnixEpochMicros -
                   fraction) /
                      1000000) {
      __atomic_store_n(&lock->owner, RThread().Id().Id(), __ATOMIC_RELEASE);
      lock->depth = 1;
      return EINVAL;
    }
    const std::int64_t deadline =
        seconds * 1000000 + fraction + kUnixEpochMicros;
    std::int64_t remaining = deadline - now.Int64();
    while (remaining > 0) {
      const TInt timeout =
          static_cast<TInt>(remaining > KMaxTInt ? KMaxTInt : remaining);
      result = state->handle.TimedWait(lock->handle, timeout);
      if (result != KErrTimedOut) {
        break;
      }
      now.UniversalTime();
      remaining = deadline - now.Int64();
    }
    if (remaining <= 0 && result == KErrNone) {
      result = KErrTimedOut;
    }
  }
  __atomic_store_n(&lock->owner, RThread().Id().Id(), __ATOMIC_RELEASE);
  lock->depth = 1;
  return result == KErrNone ? 0 : result == KErrTimedOut ? ETIMEDOUT : EINVAL;
#endif
}

}  // namespace

extern "C" int pthread_mutex_init(
    pthread_mutex_t* absl_nullable mutex,
    const pthread_mutexattr_t* absl_nullable attr) {
  if (mutex == nullptr) {
    return EINVAL;
  }
  mutex->iPtr = nullptr;
  mutex->iState = attr != nullptr && attr->iMutexType == PTHREAD_MUTEX_RECURSIVE
                      ? _ENeedsRecursiveInit
                      : _ENeedsNormalInit;
  mutex->iReentry = 0;
  return GetMutex(mutex) == nullptr ? ENOMEM : 0;
}

extern "C" int pthread_mutex_lock(pthread_mutex_t* absl_nullable mutex) {
  NativeMutex* absl_nullable state = GetMutex(mutex);
  if (state == nullptr) {
    return EINVAL;
  }
  const std::uint32_t id = RThread().Id().Id();
  if (__atomic_load_n(&state->owner, __ATOMIC_ACQUIRE) == id) {
    if (state->recursive) {
      ++state->depth;
      return 0;
    }
    return EDEADLK;
  }
#ifdef SYMBIAN_RUNTIME_LEGACY_EUSER
  LockTable();
  const bool immediate = state->available;
  if (immediate) {
    state->available = false;
    state->handle.Wait();
  } else {
    ++state->waiters;
  }
  UnlockTable();
  if (!immediate) {
    state->handle.Wait();
    LockTable();
    --state->waiters;
    __atomic_store_n(&state->owner, id, __ATOMIC_RELEASE);
    state->depth = 1;
    UnlockTable();
    return 0;
  }
#else
  state->handle.Wait();
#endif
  __atomic_store_n(&state->owner, id, __ATOMIC_RELEASE);
  state->depth = 1;
  return 0;
}

extern "C" int pthread_mutex_trylock(pthread_mutex_t* absl_nullable mutex) {
  NativeMutex* absl_nullable state = GetMutex(mutex);
  if (state == nullptr) {
    return EINVAL;
  }
  const std::uint32_t id = RThread().Id().Id();
  if (__atomic_load_n(&state->owner, __ATOMIC_ACQUIRE) == id) {
    if (state->recursive) {
      ++state->depth;
      return 0;
    }
    return EBUSY;
  }
#ifdef SYMBIAN_RUNTIME_LEGACY_EUSER
  LockTable();
  if (!state->available) {
    UnlockTable();
    return EBUSY;
  }
  state->available = false;
  state->handle.Wait();
  __atomic_store_n(&state->owner, id, __ATOMIC_RELEASE);
  state->depth = 1;
  UnlockTable();
  return 0;
#else
  const TInt acquired = state->handle.Poll();
  if (acquired != KErrNone) {
    return EBUSY;
  }
  __atomic_store_n(&state->owner, id, __ATOMIC_RELEASE);
  state->depth = 1;
  return 0;
#endif
}

extern "C" int pthread_mutex_unlock(pthread_mutex_t* absl_nullable mutex) {
  NativeMutex* absl_nullable state = GetMutex(mutex);
  if (state == nullptr ||
      __atomic_load_n(&state->owner, __ATOMIC_ACQUIRE) != RThread().Id().Id()) {
    return EPERM;
  }
#ifdef SYMBIAN_RUNTIME_LEGACY_EUSER
  LockTable();
#endif
  if (--state->depth == 0) {
    __atomic_store_n(&state->owner, 0, __ATOMIC_RELEASE);
    state->handle.Signal();
#ifdef SYMBIAN_RUNTIME_LEGACY_EUSER
    state->available = state->waiters == 0;
#endif
  }
#ifdef SYMBIAN_RUNTIME_LEGACY_EUSER
  UnlockTable();
#endif
  return 0;
}

extern "C" int pthread_mutex_destroy(pthread_mutex_t* absl_nullable mutex) {
  if (mutex == nullptr) {
    return EINVAL;
  }
  NativeMutex* absl_nullable state = GetMutex(mutex);
  if (state != nullptr) {
    if (state->depth != 0) {
      return EBUSY;
    }
    mutex->iPtr = nullptr;
    state->handle.Close();
    User::Free(state);
  }
  mutex->iState = _EDestroyed;
  return 0;
}

extern "C" int pthread_cond_init(pthread_cond_t* absl_nullable condition,
                                 const pthread_condattr_t* absl_nullable) {
  if (condition == nullptr) {
    return EINVAL;
  }
  condition->iQueue.iHead = nullptr;
  condition->iQueue.iTail = nullptr;
  condition->iState = _ENeedsNormalInit;
  return GetCond(condition) == nullptr ? ENOMEM : 0;
}

extern "C" int pthread_cond_wait(pthread_cond_t* absl_nullable condition,
                                 pthread_mutex_t* absl_nullable mutex) {
  return WaitCond(condition, mutex, nullptr);
}

extern "C" int pthread_cond_timedwait(pthread_cond_t* absl_nullable condition,
                                      pthread_mutex_t* absl_nullable mutex,
                                      const timespec* absl_nullable absolute) {
  return absolute == nullptr ? EINVAL : WaitCond(condition, mutex, absolute);
}

extern "C" int pthread_cond_signal(pthread_cond_t* absl_nullable condition) {
  NativeCond* absl_nullable state = GetCond(condition);
  if (state == nullptr) {
    return EINVAL;
  }
#ifdef SYMBIAN_RUNTIME_LEGACY_EUSER
  LockTable();
  for (WaitNode* absl_nullable node = state->waiters; node != nullptr;
       node = node->next) {
    if (!node->notified) {
      node->notified = true;
      node->semaphore.Signal();
      break;
    }
  }
  UnlockTable();
#else
  state->handle.Signal();
#endif
  return 0;
}

extern "C" int pthread_cond_broadcast(pthread_cond_t* absl_nullable condition) {
  NativeCond* absl_nullable state = GetCond(condition);
  if (state == nullptr) {
    return EINVAL;
  }
#ifdef SYMBIAN_RUNTIME_LEGACY_EUSER
  LockTable();
  for (WaitNode* absl_nullable node = state->waiters; node != nullptr;
       node = node->next) {
    if (!node->notified) {
      node->notified = true;
      node->semaphore.Signal();
    }
  }
  UnlockTable();
#else
  state->handle.Broadcast();
#endif
  return 0;
}

extern "C" int pthread_cond_destroy(pthread_cond_t* absl_nullable condition) {
  if (condition == nullptr) {
    return EINVAL;
  }
  NativeCond* absl_nullable state = GetCond(condition);
  if (state != nullptr) {
#ifdef SYMBIAN_RUNTIME_LEGACY_EUSER
    LockTable();
    if (state->waiters != nullptr) {
      UnlockTable();
      return EBUSY;
    }
    condition->iQueue.iHead = nullptr;
    condition->iState = _EDestroyed;
    UnlockTable();
#else
    condition->iQueue.iHead = nullptr;
    state->handle.Close();
#endif
    User::Free(state);
  }
#ifndef SYMBIAN_RUNTIME_LEGACY_EUSER
  condition->iState = _EDestroyed;
#endif
  return 0;
}

extern "C" pthread_t pthread_self() {
  return RThread().Id().Id();
}

extern "C" int pthread_create(pthread_t* absl_nullable output,
                              pthread_attr_t* absl_nullable attributes,
                              thread_begin_routine function,
                              void* absl_nullable argument) {
  if (output == nullptr || function == nullptr || attributes != nullptr) {
    return EINVAL;
  }
  ThreadRecord* absl_nullable record = Allocate<ThreadRecord>();
  if (record == nullptr) {
    return ENOMEM;
  }
  record->function = function;
  record->argument = argument;
  const std::uint32_t number =
      __atomic_add_fetch(&g_name_counter, 1, __ATOMIC_RELAXED);
  TBuf<32> name;
  name.Format(_L("SDKThread%08x"), number);
  if (const TInt created = record->handle.Create(
          name, ThreadEntry, 16384, static_cast<RAllocator*>(nullptr), record);
      created != KErrNone) {
    FreeThread(record);
    return created == KErrNoMemory ? ENOMEM : EAGAIN;
  }
  record->id = record->handle.Id().Id();
  LockTable();
  int slot = 0;
  while (slot < kMaxThreads && g_threads[slot] != nullptr) {
    ++slot;
  }
  if (slot == kMaxThreads) {
    UnlockTable();
    record->handle.Kill(KErrCancel);
    FreeThread(record);
    return EAGAIN;
  }
  g_threads[slot] = record;
  UnlockTable();
  *output = record->id;
  record->handle.Resume();
  return 0;
}

extern "C" int pthread_join(pthread_t id,
                            void* absl_nullable* absl_nullable output) {
  LockTable();
  ThreadRecord* absl_nullable record = FindThread(id);
  if (record == nullptr || record->detached || record->joining ||
      id == pthread_self()) {
    UnlockTable();
    return EINVAL;
  }
  record->joining = true;
  UnlockTable();
  TRequestStatus status;
  record->handle.Logon(status);
  User::WaitForRequest(status);
  if (output != nullptr) {
    *output = record->result;
  }
  LockTable();
  RemoveThread(record);
  UnlockTable();
  FreeThread(record);
  return 0;
}

extern "C" int pthread_detach(pthread_t id) {
  LockTable();
  ThreadRecord* absl_nullable record = FindThread(id);
  if (record == nullptr || record->detached || record->joining) {
    UnlockTable();
    return EINVAL;
  }
  record->detached = true;
  const bool complete = record->complete;
  if (complete) {
    RemoveThread(record);
  }
  UnlockTable();
  if (complete) {
    FreeThread(record);
  }
  return 0;
}

extern "C" int pthread_key_create(
    pthread_key_t* absl_nullable output,
    void (*absl_nullable destructor)(void* absl_nullable)) {
  if (output == nullptr) {
    return EINVAL;
  }
  LockTable();
  for (int index = 0; index < kMaxKeys; ++index) {
    if (!g_keys[index].active) {
      g_keys[index].active = true;
      g_keys[index].destructor = destructor;
      *output = static_cast<pthread_key_t>(index + 1);
      UnlockTable();
      return 0;
    }
  }
  UnlockTable();
  return EAGAIN;
}

extern "C" int pthread_key_delete(pthread_key_t key) {
  if (key == 0 || key > kMaxKeys) {
    return EINVAL;
  }
  LockTable();
  if (!g_keys[key - 1].active) {
    UnlockTable();
    return EINVAL;
  }
  g_keys[key - 1].active = false;
  g_keys[key - 1].destructor = nullptr;
  for (ThreadLocal* absl_nullable local : g_locals) {
    if (local != nullptr) {
      local->values[key - 1] = nullptr;
    }
  }
  UnlockTable();
  return 0;
}

extern "C" void* absl_nullable pthread_getspecific(pthread_key_t key) {
  if (key == 0 || key > kMaxKeys) {
    return nullptr;
  }
  LockTable();
  ThreadLocal* absl_nullable local = FindLocal(pthread_self());
  void* absl_nullable result = local != nullptr && g_keys[key - 1].active
                                   ? local->values[key - 1]
                                   : nullptr;
  UnlockTable();
  return result;
}

extern "C" int pthread_setspecific(pthread_key_t key,
                                   const void* absl_nullable value) {
  if (key == 0 || key > kMaxKeys) {
    return EINVAL;
  }
  LockTable();
  if (!g_keys[key - 1].active) {
    UnlockTable();
    return EINVAL;
  }
  ThreadLocal* absl_nullable local = EnsureLocal(pthread_self());
  if (local == nullptr) {
    UnlockTable();
    return ENOMEM;
  }
  local->values[key - 1] = const_cast<void*>(value);
  UnlockTable();
  return 0;
}

extern "C" int* absl_nonnull __errno() {
  LockTable();
  ThreadLocal* absl_nullable local = EnsureLocal(pthread_self());
  if (local == nullptr) {
    UnlockTable();
    SymbianRuntimeExit(SymbianRuntimeExitReason::kOutOfMemory);
  }
  UnlockTable();
  return &local->error_value;
}

extern "C" void* absl_nonnull SymbianRuntimeMbState(int slot) {
  if (slot < 0 || slot >= 8) {
    SymbianRuntimeExit(SymbianRuntimeExitReason::kRuntimeContractFailure);
  }
  LockTable();
  ThreadLocal* absl_nullable local = EnsureLocal(pthread_self());
  if (local == nullptr) {
    UnlockTable();
    SymbianRuntimeExit(SymbianRuntimeExitReason::kOutOfMemory);
  }
  void* absl_nonnull state = &local->mb_states[slot];
  UnlockTable();
  return state;
}

extern "C" int pthread_once(pthread_once_t* absl_nullable once,
                            void (*absl_nullable initialize)()) {
  if (once == nullptr || initialize == nullptr) {
    return EINVAL;
  }
  if (int expected = _ENotDone;
      __atomic_compare_exchange_n(once, &expected, _EDoing, false,
                                  __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
    initialize();
    __atomic_store_n(once, _EDone, __ATOMIC_RELEASE);
    return 0;
  }
  while (__atomic_load_n(once, __ATOMIC_ACQUIRE) == _EDoing) {
    User::After(TTimeIntervalMicroSeconds32(0));
  }
  return 0;
}
