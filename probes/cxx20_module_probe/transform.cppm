export module symbian_probe;

export consteval unsigned int ExpectedWord() {
  return 0x918U;
}

export unsigned int TransformWord(unsigned int value) {
  return (value * 17U) ^ 0x808U;
}
