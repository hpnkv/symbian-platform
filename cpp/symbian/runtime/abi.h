#ifndef SYMBIAN_RUNTIME_ABI_H_
#define SYMBIAN_RUNTIME_ABI_H_

// Keep modern standard-library headers and frozen Symbian C++ headers in
// separate translation units. In particular their placement-new declarations
// have conflicting exception specifications and inline definitions.
extern "C" void* SymbianRuntimeAllocate(unsigned int size);
extern "C" void SymbianRuntimeFree(void* pointer);
// Process exit codes use the corresponding Symbian error values. Keep the
// runtime's fatal cases named even in translation units without e32std.h.
enum class SymbianRuntimeExitReason : int {
  kOutOfMemory = -4,             // KErrNoMemory.
  kRuntimeContractFailure = -6,  // KErrArgument.
};
extern "C" [[noreturn]] void SymbianRuntimeExit(
    SymbianRuntimeExitReason reason);
extern "C" int SymbianRuntimeAllocationCells();
// Process-owned, page-aligned backing for low-level allocators. The opaque
// owner must outlive every access to the returned pages and be closed once.
struct SymbianRuntimePageOwner;
extern "C" int SymbianRuntimePageSize();
extern "C" int SymbianRuntimePageCreate(unsigned int bytes,
                                        SymbianRuntimePageOwner** owner,
                                        void** pages);
extern "C" void SymbianRuntimePageClose(SymbianRuntimePageOwner* owner);
extern "C" void SymbianRuntimePageSetNext(SymbianRuntimePageOwner* owner,
                                          SymbianRuntimePageOwner* next);
extern "C" SymbianRuntimePageOwner* SymbianRuntimePageNext(
    SymbianRuntimePageOwner* owner);
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
extern "C" int SymbianRuntimeTimerCreate(SymbianRuntimeTimerState** state);
extern "C" int SymbianRuntimeTimerStart(SymbianRuntimeTimerState* state,
                                        int microseconds);
extern "C" void SymbianRuntimeTimerCancel(SymbianRuntimeTimerState* state);
extern "C" int SymbianRuntimeTimerResult(const SymbianRuntimeTimerState* state);
extern "C" bool SymbianRuntimeTimerIsReady(
    const SymbianRuntimeTimerState* state);
extern "C" void SymbianRuntimeTimerClose(SymbianRuntimeTimerState* state);

struct SymbianRuntimePropertyState;
extern "C" int SymbianRuntimePropertyCreate(
    int category, unsigned int key, SymbianRuntimePropertyState** state);
extern "C" int SymbianRuntimePropertySubscribe(
    SymbianRuntimePropertyState* state);
extern "C" int SymbianRuntimePropertySet(SymbianRuntimePropertyState* state,
                                         int value);
extern "C" int SymbianRuntimePropertyResult(SymbianRuntimePropertyState* state,
                                            int* value);
extern "C" bool SymbianRuntimePropertyIsReady(
    const SymbianRuntimePropertyState* state);
extern "C" void SymbianRuntimePropertyCancel(
    SymbianRuntimePropertyState* state);
extern "C" void SymbianRuntimePropertyClose(SymbianRuntimePropertyState* state);
extern "C" void SymbianRuntimeWaitForAnyRequest();

// Cross-thread signal for the event thread's existing request semaphore.
struct SymbianRuntimeWakeState;
extern "C" int SymbianRuntimeWakeCreate(SymbianRuntimeWakeState** state);
extern "C" void SymbianRuntimeWakeSignal(SymbianRuntimeWakeState* state);
extern "C" void SymbianRuntimeWakeClose(SymbianRuntimeWakeState* state);

#endif  // SYMBIAN_RUNTIME_ABI_H_
