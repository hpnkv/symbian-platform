#ifndef SYMBIAN_POINTER_PROBE_H_
#define SYMBIAN_POINTER_PROBE_H_

#include <absl/base/nullability.h>

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

#endif  // SYMBIAN_POINTER_PROBE_H_
