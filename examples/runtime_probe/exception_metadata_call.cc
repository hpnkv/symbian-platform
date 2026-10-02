__attribute__((noinline)) int RuntimeExceptionCall(int value) {
#ifdef SYMBIAN_RUNTIME_CHANGED_EXCEPTION_METADATA
  return value + 1;
#else
  return value;
#endif
}
