extern "C" unsigned int SymbianAbiProbe(unsigned int value) {
  return (value * 17U) ^ 0x808U;
}
