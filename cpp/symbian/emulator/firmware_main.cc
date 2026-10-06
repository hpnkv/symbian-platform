// SPDX-License-Identifier: GPL-3.0-or-later

#include <charconv>
#include <iostream>
#include <memory>

#include <absl/base/nullability.h>

#include "common/log.h"
#include "spdlog/sinks/stdout_sinks.h"
#include "symbian/emulator/firmware.h"

int main(int argc, char* absl_nullable* absl_nonnull argv) {
  // Keep upstream diagnostics in the retained stderr log, not the JSON stream
  // or the firmware baseline. This reader is synchronous and owns no workers.
  eka2l1::log::filterings = std::make_unique<eka2l1::log_filterings>();
  eka2l1::log::filterings->reset_all(spdlog::level::warn);
  eka2l1::log::spd_logger = std::make_shared<spdlog::logger>(
      "firmware", std::make_shared<spdlog::sinks::stderr_sink_st>());
  int variant = -1;
  if (argc < 3 || argc > 5) {
    std::cerr << "Usage: symbian_firmware_tool archive|rom|z|vpl SOURCE "
                 "[COMPANION] [VARIANT]\n";
    return 2;
  }
  if (argc == 5) {
    const std::string value = argv[4];
    const auto parsed =
        std::from_chars(value.data(), value.data() + value.size(), variant);
    if (parsed.ec != std::errc() || parsed.ptr != value.data() + value.size()) {
      return 2;
    }
  }
  const auto result = symbian::emulator::ImportFirmware(
      argv[1], argv[2], argc >= 4 ? argv[3] : "", variant);
  nlohmann::json response;
  response["status"] = {{"code", static_cast<int>(result.status().code())},
                        {"message", std::string(result.status().message())}};
  if (result.ok()) {
    response["result"] = *result;
  }
  std::cout << response.dump() << '\n';
  return result.ok() ? 0 : 1;
}
