extern "C" __attribute__((noinline)) unsigned int SymbianAbiProbe(
    unsigned int value) {
  return (value * 17U) ^ 0x808U;
}

extern "C" int ProbeMain() {
  volatile unsigned int input = 16U;
  return SymbianAbiProbe(input) == 0x918U ? 0 : 42;
}
