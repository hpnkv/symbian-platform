// SPDX-License-Identifier: GPL-3.0-or-later
#include "symbian/emulator/distribution.h"

#include <cstdio>
#include <cstring>

namespace symbian::emulator {

bool PrintDistributionIfRequested(int argc, char** argv) {
  if (argc != 2 || std::strcmp(argv[1], "--symbian-sdk-capabilities") != 0) {
    return false;
  }
  std::puts(
      "{\"schema\":\"symbian.emulator-capabilities/v1\","
      "\"version\":\"" SYMBIAN_EMULATOR_VERSION
      "\",\"control_protocol\":\"symbian.emulator-control/v1\","
      "\"capabilities\":[\"isolated-data-root\",\"control-status\","
      "\"framebuffer-capture\",\"pointer-input\",\"guest-exit-record\","
      "\"firmware-import\",\"loopback-gdb\",\"dynarmic\",\"dyncom\"]}");
  return true;
}

}  // namespace symbian::emulator
