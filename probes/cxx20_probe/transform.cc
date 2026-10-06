#include <absl/base/nullability.h>

#include "probe.h"

#if defined(SYMBIAN_CXX20_USE_LIBCXX)
#include <bit>
#include <concepts>
#include <span>

static_assert(std::unsigned_integral<unsigned int>);
static_assert(std::bit_cast<unsigned int>(1.0F) == 0x3f800000U);
#endif

extern "C" unsigned int SymbianAbiProbe(unsigned int value) {
#if defined(SYMBIAN_CXX20_USE_LIBCXX)
  const unsigned int words[] = {value, 17U, 0x808U};
  const std::span<const unsigned int, 3> operands(words);
  return std::rotl((operands[0] * operands[1]) ^ operands[2], 5) ^
         static_cast<unsigned int>(std::popcount(operands[0]));
#else
  const auto apply = []<Word T>(T word) {
    return Transform<kParameters>(word);
  };
  return apply(value);
#endif
}

unsigned int Transformer::Apply(unsigned int value) const {
  return Transform<kParameters>(value);
}

__attribute__((noinline)) unsigned int Dispatch(
    const Transformer* absl_nonnull transformer, unsigned int value) {
  return transformer->Apply(value);
}
