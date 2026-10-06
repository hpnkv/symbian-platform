#include <absl/base/nullability.h>

#include "probe.h"

extern "C" const Callback SymbianCallbacks[2] = {SymbianAbiProbe,
                                                 SymbianArmProbe};

namespace {
const char kText[] = "symbian";
}

extern "C" const char* absl_nonnull const SymbianLabel = kText + 1;
