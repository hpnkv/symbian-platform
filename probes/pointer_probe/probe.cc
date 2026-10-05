#include "probe.h"

int main() {
  volatile unsigned int input = 16U;
  const Callback* volatile callbacks = SymbianCallbacks;
  const Transformer transformer;
  const unsigned int thumb_result = callbacks[0](input);
  const unsigned int arm_result = callbacks[1](input);
  const unsigned int virtual_result = Dispatch(&transformer, input);
  return thumb_result == 0x918U && arm_result == 0x918U &&
                 virtual_result == 0x918U && SymbianLabel[0] == 'y'
             ? 0
             : 42;
}
