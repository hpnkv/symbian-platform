#ifndef SYMBIAN_RUNTIME_ABI_H_
#define SYMBIAN_RUNTIME_ABI_H_

#include <absl/base/nullability.h>

// Keep modern standard-library headers and frozen Symbian C++ headers in
// separate translation units. In particular their placement-new declarations
// have conflicting exception specifications and inline definitions.
extern "C" void* absl_nullable SymbianRuntimeAllocate(unsigned int size);
extern "C" void SymbianRuntimeFree(void* absl_nullable pointer);
// Opaque identity for verifying distinct native thread heaps in guest tests.
extern "C" void* absl_nonnull SymbianRuntimeHeapIdentity();
// Process exit codes use the corresponding Symbian error values. Keep the
// runtime's fatal cases named even in translation units without e32std.h.
enum class SymbianRuntimeExitReason : int {
  kOutOfMemory = -4,             // KErrNoMemory.
  kRuntimeContractFailure = -6,  // KErrArgument.
};
extern "C" [[noreturn]] void SymbianRuntimeExit(
    SymbianRuntimeExitReason reason);
extern "C" int SymbianRuntimeAllocationCells();
extern "C" void SymbianRuntimeCollectAllocations();
// Pair on native worker entry/exit. The default heap backend needs no state;
// mimalloc uses this scope for a fast thread-local cache and explicit teardown.
extern "C" void SymbianRuntimeThreadCacheEnter();
extern "C" void SymbianRuntimeThreadCacheLeave();
// Process-owned, page-aligned backing for low-level allocators. The opaque
// owner must outlive every access to the returned pages and be closed once.
struct SymbianRuntimePageOwner;
extern "C" int SymbianRuntimePageSize();
extern "C" int SymbianRuntimePageCreate(
    unsigned int bytes,
    SymbianRuntimePageOwner* absl_nullable* absl_nullable owner,
    void* absl_nullable* absl_nullable pages);
extern "C" void SymbianRuntimePageClose(
    SymbianRuntimePageOwner* absl_nullable owner);
extern "C" void SymbianRuntimePageSetNext(
    SymbianRuntimePageOwner* absl_nullable owner,
    SymbianRuntimePageOwner* absl_nullable next);
extern "C" SymbianRuntimePageOwner* absl_nullable SymbianRuntimePageNext(
    SymbianRuntimePageOwner* absl_nullable owner);
extern "C" void SymbianRuntimeRunInitializers();
extern "C" void SymbianRuntimeRunFinalizers();
extern "C" int SymbianRuntimeDllEntry(int reason);
extern "C" unsigned int SymbianRuntimeTickCount();
extern "C" int SymbianRuntimeTickPeriodMicros();
extern "C" unsigned int SymbianRuntimeNanoTickCount();
extern "C" int SymbianRuntimeNanoTickPeriodMicros();
extern "C" unsigned int SymbianRuntimeFastCounter();
extern "C" int SymbianRuntimeFastCounterFrequency();

// Internal narrow bridge for thread-relative RTimer requests. The caller must
// keep the state alive until cancellation has completed and been drained.
struct SymbianRuntimeTimerState;
extern "C" int SymbianRuntimeTimerCreate(
    SymbianRuntimeTimerState* absl_nullable* absl_nullable state);
extern "C" int SymbianRuntimeTimerStart(
    SymbianRuntimeTimerState* absl_nullable state, int microseconds);
extern "C" void SymbianRuntimeTimerCancel(
    SymbianRuntimeTimerState* absl_nullable state);
extern "C" int SymbianRuntimeTimerResult(
    const SymbianRuntimeTimerState* absl_nullable state);
extern "C" bool SymbianRuntimeTimerIsReady(
    const SymbianRuntimeTimerState* absl_nullable state);
extern "C" void SymbianRuntimeTimerClose(
    SymbianRuntimeTimerState* absl_nullable state);

struct SymbianRuntimePropertyState;
extern "C" int SymbianRuntimePropertyCreate(
    int category, unsigned int key,
    SymbianRuntimePropertyState* absl_nullable* absl_nonnull state);
extern "C" int SymbianRuntimePropertySubscribe(
    SymbianRuntimePropertyState* absl_nullable state);
extern "C" int SymbianRuntimePropertySet(
    SymbianRuntimePropertyState* absl_nullable state, int value);
extern "C" int SymbianRuntimePropertyResult(
    SymbianRuntimePropertyState* absl_nullable state, int* absl_nullable value);
extern "C" bool SymbianRuntimePropertyIsReady(
    const SymbianRuntimePropertyState* absl_nullable state);
extern "C" void SymbianRuntimePropertyCancel(
    SymbianRuntimePropertyState* absl_nullable state);
extern "C" void SymbianRuntimePropertyClose(
    SymbianRuntimePropertyState* absl_nullable state);
extern "C" void SymbianRuntimeWaitForAnyRequest();

// Cross-thread signal for the event thread's existing request semaphore.
struct SymbianRuntimeWakeState;
extern "C" int SymbianRuntimeWakeCreate(
    SymbianRuntimeWakeState* absl_nullable* absl_nullable state);
extern "C" void SymbianRuntimeWakeSignal(
    SymbianRuntimeWakeState* absl_nullable state);
extern "C" void SymbianRuntimeWakeClose(
    SymbianRuntimeWakeState* absl_nullable state);

#endif  // SYMBIAN_RUNTIME_ABI_H_
