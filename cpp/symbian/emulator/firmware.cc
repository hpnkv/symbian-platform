// SPDX-License-Identifier: GPL-3.0-or-later
// Uses original EKA2L1 installation/loader implementations, revision recorded
// in research/eka2l1/README.md. No firmware parser is reimplemented here.
#include "symbian/emulator/firmware.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <set>
#include <system_error>

#include "common/buffer.h"
#include "config/config.h"
#include "loader/rom.h"
#include "system/devices.h"
#include "system/installation/archive.h"
#include "system/installation/firmware.h"
#include "system/installation/rpkg.h"
#include "system/software.h"

namespace symbian::emulator {
namespace {
namespace fs = std::filesystem;

std::string Lower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return value;
}

bool SafePath(const std::string& path) {
  if (path.empty() || path.front() == '/' || path.find(':') != path.npos ||
      path.find('\\') != path.npos || path.find('\0') != path.npos) {
    return false;
  }
  for (const auto& component : fs::path(path)) {
    if (component == "..") {
      return false;
    }
  }
  return true;
}

absl::Status CopyZ(const std::string& source, const fs::path& destination) {
  std::error_code error;
  fs::recursive_directory_iterator iterator(source, error), end;
  std::set<std::string> paths;
  for (; !error && iterator != end; iterator.increment(error)) {
    const auto& entry = *iterator;
    const auto status = entry.symlink_status(error);
    if (error) {
      break;
    }
    if (!fs::is_directory(status) && !fs::is_regular_file(status)) {
      return absl::InvalidArgumentError(
          "Drive Z must contain regular files and directories only");
    }
    const auto relative =
        entry.path().lexically_relative(source).generic_string();
    const auto lowered = Lower(relative);
    if (!SafePath(relative) || !paths.insert(lowered).second) {
      return absl::InvalidArgumentError(
          "Unsafe or case-colliding drive Z path: " + relative);
    }
    const auto target = destination / lowered;
    fs::create_directories(
        fs::is_directory(status) ? target : target.parent_path(), error);
    if (error) {
      break;
    }
    if (fs::is_regular_file(status)) {
      fs::copy_file(entry.path(), target, error);
    }
  }
  if (error) {
    return absl::InternalError(error.message());
  }
  return absl::OkStatus();
}

absl::Status InstallationStatus(eka2l1::device_installation_error error) {
  using namespace eka2l1;
  if (error == device_installation_none) {
    return absl::OkStatus();
  }
  if (error == device_installation_rpkg_missing) {
    return absl::FailedPreconditionError(
        "This ROM needs drive Z: supply its matching RPKG or extracted Z "
        "directory");
  }
  if (error == device_installation_not_exist) {
    return absl::NotFoundError("Firmware input not found");
  }
  if (error == device_installation_insufficent) {
    return absl::ResourceExhaustedError("Insufficient storage for firmware");
  }
  return absl::InvalidArgumentError(
      "EKA2L1 importer failed, installation error " + std::to_string(error));
}
}  // namespace

absl::Status ValidateArchive(
    const std::vector<eka2l1::common::archive_entry_info>& entries) {
  if (entries.size() > 200000) {
    return absl::ResourceExhaustedError("Archive has too many entries");
  }
  std::uint64_t total = 0;
  std::set<std::string> files, z_roots;
  int roms = 0, packages = 0;
  for (const auto& entry : entries) {
    if (!SafePath(entry.path)) {
      return absl::InvalidArgumentError("Unsafe archive path: " + entry.path);
    }
    if (entry.size > (8ULL << 30) || total > (16ULL << 30) - entry.size) {
      return absl::ResourceExhaustedError(
          "Archive exceeds 16 GiB unpacked limit");
    }
    total += entry.size;
    auto path = Lower(entry.path);
    while (!path.empty() && path.back() == '/') {
      path.pop_back();
    }
    if (!files.insert(path).second) {
      return absl::InvalidArgumentError(
          "Duplicate or case-colliding archive path: " + entry.path);
    }
    const auto marker = path.find("drives/z/");
    if (marker != path.npos && (marker == 0 || path[marker - 1] == '/')) {
      const auto start = marker + 9;
      z_roots.insert(path.substr(0, path.find('/', start)));
    }
    if (!entry.is_directory && marker == path.npos) {
      if (fs::path(path).extension() == ".rom") {
        ++roms;
      }
      if (fs::path(path).extension() == ".rpkg") {
        ++packages;
      }
    }
  }
  if (z_roots.size() > 1 || roms > 1 || packages > 1) {
    return absl::InvalidArgumentError(
        "Archive contains multiple device candidates; extract it and "
        "explicitly select --rom with --rpkg or --z-drive");
  }
  return absl::OkStatus();
}

absl::StatusOr<nlohmann::json> ImportFirmware(const std::string& form,
                                              const std::string& source,
                                              const std::string& companion,
                                              int variant) {
  std::error_code error;
  if (form == "probe") {
    std::vector<eka2l1::common::archive_entry_info> entries;
    if (!eka2l1::common::list_archive(source, entries)) {
      return absl::InvalidArgumentError("Cannot read firmware archive");
    }
    nlohmann::json candidates = nlohmann::json::array();
    std::uint64_t total = 0;
    for (const auto& entry : entries) {
      auto path = Lower(entry.path);
      while (!path.empty() && path.back() == '/') {
        path.pop_back();
      }
      const auto marker = path.find("drives/z/");
      if ((marker == path.npos && (fs::path(path).extension() == ".rom" ||
                                   fs::path(path).extension() == ".rpkg")) ||
          (entry.is_directory && marker != path.npos &&
           path.find('/', marker + 9) == path.npos)) {
        candidates.push_back({{"path", entry.path}, {"size", entry.size}});
      }
      total += entry.size;
    }
    const auto status = ValidateArchive(entries);
    return nlohmann::json{{"candidates", candidates},
                          {"entries", entries.size()},
                          {"unpacked_bytes", total},
                          {"validation",
                           {{"code", static_cast<int>(status.code())},
                            {"message", std::string(status.message())}}}};
  }
  if (fs::exists("data", error)) {
    return absl::FailedPreconditionError(
        "Importer requires a fresh working directory");
  }
  fs::create_directories("data/roms", error);
  if (error) {
    return absl::InternalError(error.message());
  }
  eka2l1::config::state config;
  config.storage = "data";
  eka2l1::device_manager manager(&config);
  eka2l1::device_installation_error result;
  if (form == "archive") {
    std::vector<eka2l1::common::archive_entry_info> entries;
    if (!eka2l1::common::list_archive(source, entries)) {
      return absl::InvalidArgumentError("Cannot read firmware archive");
    }
    auto status = ValidateArchive(entries);
    if (!status.ok()) {
      return status;
    }
    result = eka2l1::loader::install_archive(&manager, source, "data/roms/",
                                             "data/drives/z/", true, nullptr,
                                             nullptr);
  } else if (form == "rom") {
    result = eka2l1::loader::install_rom_with_optional_rpkg(
        &manager, source, companion, "data/roms/", "data/drives/z/", true,
        nullptr, nullptr);
  } else if (form == "z") {
    eka2l1::common::ro_std_file_stream stream(source, true);
    if (!stream.valid() || !eka2l1::loader::load_rom(&stream)) {
      return absl::InvalidArgumentError("Invalid ROM image");
    }
    auto status = CopyZ(companion, "data/drive-z-staging");
    if (!status.ok()) {
      return status;
    }
    std::string manufacturer, code, model;
    if (!eka2l1::loader::determine_rpkg_product_info(
            "data/drive-z-staging/", manufacturer, code, model)) {
      return absl::InvalidArgumentError(
          "Drive Z lacks supported product identification files");
    }
    if (!SafePath(code) || fs::path(code).has_parent_path()) {
      return absl::InvalidArgumentError("Unsafe firmware code");
    }
    const auto version =
        eka2l1::loader::determine_rpkg_symbian_version("data/drive-z-staging/");
    const auto uid =
        eka2l1::loader::determine_rpkg_machine_uid("data/drive-z-staging/");
    manager.add_new_device(code, model, manufacturer, version, uid, true);
    fs::create_directories("data/drives/z", error);
    if (!error) {
      fs::rename("data/drive-z-staging",
                 fs::path("data/drives/z") / Lower(code), error);
    }
    if (!error) {
      fs::create_directories(fs::path("data/roms") / Lower(code), error);
    }
    if (!error) {
      fs::copy_file(source, fs::path("data/roms") / Lower(code) / "SYM.ROM",
                    error);
    }
    if (error) {
      return absl::InternalError(error.message());
    }
    result = eka2l1::device_installation_none;
  } else if (form == "vpl") {
    bool ambiguous = false;
    result = eka2l1::install_firmware(
        &manager, source, "data/", "data/roms/", true,
        [&](const std::vector<std::string>& variants) {
          if (variant >= 0 && variant < variants.size()) {
            return variant;
          }
          if (variant < 0 && variants.size() == 1) {
            return 0;
          }
          ambiguous = true;
          return -1;
        },
        nullptr, nullptr);
    if (ambiguous) {
      return absl::InvalidArgumentError(
          "Select a valid VPL --variant index; multiple variants are never "
          "selected silently");
    }
  } else {
    return absl::InvalidArgumentError("Unknown import form");
  }
  auto status = InstallationStatus(result);
  if (!status.ok()) {
    return status;
  }
  if (manager.total() != 1) {
    return absl::FailedPreconditionError(
        "Import must produce exactly one device");
  }
  const auto& device = manager.get_devices().front();
  if (!SafePath(device.firmware_code) ||
      fs::path(device.firmware_code).has_parent_path()) {
    return absl::InvalidArgumentError("Unsafe firmware code");
  }
  manager.save_devices();
  const auto code = Lower(device.firmware_code);
  return nlohmann::json{
      {"firmware_code", device.firmware_code},
      {"model", device.model},
      {"manufacturer", device.manufacturer},
      {"symbian_version", epocver_to_string(device.ver)},
      {"kernel", is_epocver_eka1(device.ver) ? "eka1" : "eka2"},
      {"machine_uid", device.machine_uid},
      {"rom", "data/roms/" + code + "/SYM.ROM"},
      {"z_drive", "data/drives/z/" + code},
      {"c_drive", "data/drives/" + code + "/c"}};
}
}  // namespace symbian::emulator
