#include <absl/base/nullability.h>

#include "probe.h"

extern "C" constinit const Callback SymbianCallbacks[2] = {SymbianAbiProbe,
                                                           SymbianArmProbe};

namespace {
const char kText[] = "symbian";
}

extern "C" constinit const char* absl_nonnull const SymbianLabel = kText + 1;
