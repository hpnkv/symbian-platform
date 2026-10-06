#include <absl/base/nullability.h>

#include "probe.h"

extern "C" unsigned int SymbianAbiProbe(unsigned int value) {
  return (value * 17U) ^ 0x808U;
}

unsigned int Transformer::Apply(unsigned int value) const {
  return (value * 17U) ^ 0x808U;
}

__attribute__((noinline)) unsigned int Dispatch(
    const Transformer* absl_nonnull transformer, unsigned int value) {
  return transformer->Apply(value);
}
