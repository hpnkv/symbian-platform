extern "C" unsigned int SymbianProbeTransform(unsigned int value);

extern "C" int ProbeMain() {
  volatile unsigned int input = 16;
  return SymbianProbeTransform(input) == 0x918U ? 0 : 42;
}
