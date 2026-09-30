extern "C" __attribute__((section(".text.probe_function"))) unsigned int
SymbianProbeTransform(unsigned int value) {
  return (value * 17U) ^ 0x808U;
}
