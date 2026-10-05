extern "C" unsigned int SymbianProbeTransform(unsigned int value);

int main() {
  volatile unsigned int input = 16;
  return SymbianProbeTransform(input) == 0x918U ? 0 : 42;
}
