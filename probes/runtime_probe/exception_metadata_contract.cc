// Converter control: preserve genuine unwind tables and a catch landing pad
// without claiming that a thrown exception can be processed yet.
extern int RuntimeExceptionCall(int value);

namespace {
struct Guard {
  int* count;

  ~Guard() { ++*count; }
};
}  // namespace

extern "C" int SymbianRuntimeExceptionProbe() {
  int cleaned = 0;
  try {
    Guard guard{&cleaned};
    if (RuntimeExceptionCall(7) != 7) {
      return -153;
    }
  } catch (...) {
    return -154;
  }
  return cleaned == 1 ? 0 : -155;
}
