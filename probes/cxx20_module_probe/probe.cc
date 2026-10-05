import symbian_probe;

extern "C" int ProbeMain() {
  volatile unsigned int input = 16U;
  return TransformWord(input) == ExpectedWord() ? 0 : 42;
}
