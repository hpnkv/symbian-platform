// Keep definitions separate from callers so PIC references require GOT words.
extern "C" {
__attribute__((noinline)) unsigned int RuntimeReverseBytes(unsigned int value) {
  return __builtin_bswap32(value);
}

#ifdef SYMBIAN_RUNTIME_CHANGED_GOT_VALUE
extern const int RuntimeGotValue = 2027;
#else
extern const int RuntimeGotValue = 2026;
#endif

__attribute__((noinline)) int RuntimeGotFunction(int value) {
  return value + 808;
}
}
