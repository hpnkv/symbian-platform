// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SYMBIAN_EMULATOR_DISTRIBUTION_H_
#define SYMBIAN_EMULATOR_DISTRIBUTION_H_

#include <absl/base/nullability.h>

namespace symbian::emulator {

// Handles the SDK's display-independent distribution compatibility query.
bool PrintDistributionIfRequested(int argc,
                                  char* absl_nullable* absl_nonnull argv);

}  // namespace symbian::emulator

#endif  // SYMBIAN_EMULATOR_DISTRIBUTION_H_
