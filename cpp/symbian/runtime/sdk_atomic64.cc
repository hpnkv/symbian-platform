#include <e32atomics.h>
#include <e32std.h>

// The default runtime serializes 64-bit compiler ABI operations with one
// process-owned RFastLock. Some older ROMs lack the native 64-bit exports;
// the separate verified-native profile uses those exports when available.
// User code still uses std::atomic<uint64_t> without owning a native lock.
namespace {

struct Atomic64State {
  RFastLock lock;
  volatile TUint32 phase = 0;  // 0: empty, 1: creating, 2: ready, 3: failed.

  ~Atomic64State() {
    if (__e32_atomic_load_acq32(&phase) == 2) {
      lock.Close();
    }
  }
};

Atomic64State state;

void EnsureLock() {
  TUint32 empty = 0;
  if (__e32_atomic_cas_ord32(&state.phase, &empty, 1)) {
    if (state.lock.CreateLocal() != KErrNone) {
      __e32_atomic_store_ord32(&state.phase, 3);
      User::Invariant();
    }
    __e32_atomic_store_ord32(&state.phase, 2);
    return;
  }
  for (;;) {
    const TUint32 phase = __e32_atomic_load_acq32(&state.phase);
    if (phase == 2) {
      return;
    }
    if (phase != 1) {
      User::Invariant();
    }
    User::After(TTimeIntervalMicroSeconds32(1000));
  }
}

struct Guard {
  Guard() {
    EnsureLock();
    state.lock.Wait();
  }

  ~Guard() { state.lock.Signal(); }
};

TUint64 Read(const volatile void* pointer) {
  return *static_cast<const volatile TUint64*>(pointer);
}

void Write(volatile void* pointer, TUint64 value) {
  *static_cast<volatile TUint64*>(pointer) = value;
}

}  // namespace

extern "C" void SymbianRuntimeAtomicLock() {
  EnsureLock();
  state.lock.Wait();
}

extern "C" void SymbianRuntimeAtomicUnlock() {
  state.lock.Signal();
}

extern "C" TUint64 SymbianRuntimeAtomic64Load(const volatile void* pointer) {
  Guard guard;
  return Read(pointer);
}

extern "C" void SymbianRuntimeAtomic64Store(volatile void* pointer,
                                            TUint64 value) {
  Guard guard;
  Write(pointer, value);
}

extern "C" TUint64 SymbianRuntimeAtomic64Exchange(volatile void* pointer,
                                                  TUint64 value) {
  Guard guard;
  const TUint64 old = Read(pointer);
  Write(pointer, value);
  return old;
}

extern "C" TBool SymbianRuntimeAtomic64CompareExchange(volatile void* pointer,
                                                       TUint64* expected,
                                                       TUint64 desired) {
  Guard guard;
  const TUint64 old = Read(pointer);
  if (old != *expected) {
    *expected = old;
    return EFalse;
  }
  Write(pointer, desired);
  return ETrue;
}

extern "C" TUint64 SymbianRuntimeAtomic64FetchAdd(volatile void* pointer,
                                                  TUint64 value) {
  Guard guard;
  const TUint64 old = Read(pointer);
  Write(pointer, old + value);
  return old;
}
