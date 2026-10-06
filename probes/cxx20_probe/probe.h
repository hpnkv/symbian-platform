#ifndef SYMBIAN_CXX20_PROBE_H_
#define SYMBIAN_CXX20_PROBE_H_

#include <absl/base/nullability.h>

#if __cplusplus < 202002L
#error "This probe requires C++20"
#endif

static_assert(__cpp_concepts >= 201907L);
static_assert(__cpp_consteval >= 201811L);
static_assert(__cpp_constinit >= 201907L);
static_assert(__cpp_generic_lambdas >= 201707L);
static_assert(__cpp_designated_initializers >= 201707L);
static_assert(__cpp_char8_t >= 201811L);

struct Parameters {
  unsigned int multiplier;
  unsigned int mask;
  constexpr bool operator==(const Parameters&) const = default;
};

inline constexpr Parameters kParameters{.multiplier = 17U, .mask = 0x808U};

template <typename T>
concept Word = __is_same(T, unsigned int) && requires(T value) {
  value * 17U;
  value ^ 0x808U;
};

// The structural class template argument is a C++20 language feature.
template <Parameters parameters, Word T>
constexpr unsigned int Transform(T value) {
  return (value * parameters.multiplier) ^ parameters.mask;
}

consteval unsigned int Expected(unsigned int value) {
  return Transform<kParameters>(value);
}

struct Empty {};

struct TaggedWord {
  [[no_unique_address]] Empty tag;
  unsigned int value;
};

static_assert(sizeof(TaggedWord) == sizeof(unsigned int));
static_assert(kParameters == Parameters{17U, 0x808U});
static_assert(Expected(16U) == 0x918U);
static_assert(sizeof(char8_t) == 1);

using Callback = unsigned int (*absl_nonnull)(unsigned int);
extern "C" unsigned int SymbianAbiProbe(unsigned int value);
extern "C" unsigned int SymbianArmProbe(unsigned int value);
extern "C" __attribute__((visibility("hidden")))
const Callback SymbianCallbacks[2];
extern "C" __attribute__((visibility("hidden")))
const char* absl_nonnull const SymbianLabel;

class __attribute__((visibility("hidden"))) Transformer {
 public:
  virtual unsigned int Apply(unsigned int value) const;
};

unsigned int Dispatch(const Transformer* absl_nonnull transformer,
                      unsigned int value);

#endif  // SYMBIAN_CXX20_PROBE_H_
