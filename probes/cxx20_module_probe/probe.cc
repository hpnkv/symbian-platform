import symbian_probe;

int main() {
  volatile unsigned int input = 16U;
  return TransformWord(input) == ExpectedWord() ? 0 : 42;
}
