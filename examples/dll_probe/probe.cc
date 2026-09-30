extern "C" unsigned int SymbianProbeTransform(unsigned int value) {
  return (value * 17U) ^ 0x808U;
}
