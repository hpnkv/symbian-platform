#include <absl/base/nullability.h>

// Guest exception profile acceptance probe. Its application-facing code uses
// ordinary C++; the C entry is only the runtime probe's test boundary.
namespace {

struct Guard {
  int* absl_nonnull count;

  ~Guard() { ++*count; }
};

int ThrowThroughGuard(int value, int* absl_nonnull count) {
  Guard guard{count};
  throw value;
}

}  // namespace

extern "C" int SymbianRuntimeExceptionProbe() {
  int cleaned = 0;
  try {
    ThrowThroughGuard(7, &cleaned);
  } catch (int value) {
    return value == 7 && cleaned == 1 ? 0 : -151;
  }
  return -152;
}
