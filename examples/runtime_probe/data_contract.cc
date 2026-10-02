// Separate definitions force GOT references to independently relocated storage.
extern "C" int RuntimeGotFunction(int value);
extern "C" {
#ifdef SYMBIAN_RUNTIME_CHANGED_DATA
int RuntimeInitialized = 2025;
#else
int RuntimeInitialized = 2026;
#endif
int RuntimeBss[64];
int* RuntimeDataPointer = &RuntimeInitialized;
int* RuntimeBssPointer = RuntimeBss;
int (*RuntimeFunctionPointer)(int) = &RuntimeGotFunction;
}
