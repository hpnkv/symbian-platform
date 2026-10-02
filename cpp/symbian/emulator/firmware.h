// SPDX-License-Identifier: GPL-3.0-or-later
// Adapter to pinned EKA2L1 installers; kept outside the Python wheel.
#ifndef SYMBIAN_EMULATOR_FIRMWARE_H_
#define SYMBIAN_EMULATOR_FIRMWARE_H_

#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "common/archive.h"
#include "nlohmann/json.hpp"

namespace symbian::emulator {
absl::Status ValidateArchive(
    const std::vector<eka2l1::common::archive_entry_info>& entries);
// Runs synchronously in a new, owned working directory. Never loads an
// existing device database (loading it can delete pending-deletion devices).
absl::StatusOr<nlohmann::json> ImportFirmware(const std::string& form,
                                              const std::string& source,
                                              const std::string& companion,
                                              int variant);
}  // namespace symbian::emulator
#endif  // SYMBIAN_EMULATOR_FIRMWARE_H_
