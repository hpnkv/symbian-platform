#include "probe.h"

extern "C" int ProbeMain() {
  volatile unsigned int input = 16U;
  const Callback* volatile callbacks = SymbianCallbacks;
  const Transformer transformer;
  const TaggedWord expected{.tag = {}, .value = Expected(16U)};
#if defined(SYMBIAN_CXX20_USE_LIBCXX)
  // Independent arithmetic reference for rotl(result, 5) ^ popcount(16).
  constexpr unsigned int kThumbExpected = (Expected(16U) << 5) ^ 1U;
#else
  constexpr unsigned int kThumbExpected = Expected(16U);
#endif
  const char8_t label[] = u8"ymbian";
  const unsigned int thumb_result = callbacks[0](input);
  const unsigned int arm_result = callbacks[1](input);
  const unsigned int virtual_result = Dispatch(&transformer, input);
  return thumb_result == kThumbExpected && arm_result == expected.value &&
                 virtual_result == expected.value &&
                 SymbianLabel[0] == static_cast<char>(label[0])
             ? 0
             : 42;
}
