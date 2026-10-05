// Default-visible ELF storage makes Clang emit relocatable GOT slots. These
// variables are not in the frozen E32 export definition.
extern "C" {
__attribute__((visibility("hidden"))) volatile unsigned int SymbianProbeSeed = 0x808U;
__attribute__((visibility("hidden"))) volatile unsigned int SymbianProbeCalls;
}

extern "C" unsigned int SymbianProbeTransform(unsigned int value) {
  const unsigned int previous = SymbianProbeCalls;
  SymbianProbeCalls = previous + 1;
  return ((value * 17U) ^ SymbianProbeSeed) + previous;
}
