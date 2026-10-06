#include <absl/base/nullability.h>

// Separate definitions force GOT references to independently relocated storage.
extern "C" int RuntimeGotFunction(int value);
extern "C" {
#ifdef SYMBIAN_RUNTIME_CHANGED_DATA
int RuntimeInitialized = 2025;
#else
int RuntimeInitialized = 2026;
#endif
int RuntimeBss[64];
int* absl_nonnull RuntimeDataPointer = &RuntimeInitialized;
int* absl_nonnull RuntimeBssPointer = RuntimeBss;
int (*absl_nonnull RuntimeFunctionPointer)(int) = &RuntimeGotFunction;
}
