// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SYMBIAN_EMULATOR_DISTRIBUTION_H_
#define SYMBIAN_EMULATOR_DISTRIBUTION_H_

namespace symbian::emulator {

// Handles the SDK's display-independent distribution compatibility query.
bool PrintDistributionIfRequested(int argc, char** argv);

}  // namespace symbian::emulator

#endif  // SYMBIAN_EMULATOR_DISTRIBUTION_H_
