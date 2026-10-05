// SPDX-License-Identifier: Apache-2.0
// Native format publisher for SDK consumers that do not install Python.
#include <charconv>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include <absl/status/status.h>
#include <absl/status/status_macros.h>
#include <absl/status/statusor.h>

#include "symbian/e32/e32.h"
#include "symbian/sdk/exports.h"

namespace {
namespace fs = std::filesystem;
using Options = std::map<std::string, std::vector<std::string>>;

absl::StatusOr<std::string> Read(const fs::path& path) {
  std::error_code error;
  const auto size = fs::file_size(path, error);
  if (error) {
    return absl::NotFoundError("Cannot read " + path.string());
  }
  if (size > 64 * 1024 * 1024) {
    return absl::ResourceExhaustedError("Input exceeds 64 MiB");
  }
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    return absl::NotFoundError("Cannot open " + path.string());
  }
  std::string data(size, '\0');
  stream.read(data.data(), static_cast<std::streamsize>(size));
  if (stream.gcount() != static_cast<std::streamsize>(size) ||
      stream.peek() != std::char_traits<char>::eof() || stream.bad()) {
    return absl::DataLossError("Input changed or could not be read");
  }
  return data;
}

absl::Status Write(const fs::path& path, std::string_view data,
                   const Options& options) {
  // Never replace an input through a spelling difference or hard link.
  for (const auto& key : {"--input", "--definition", "--import-proxy"}) {
    const auto found = options.find(key);
    if (found == options.end()) {
      continue;
    }
    for (const auto& input : found->second) {
      std::error_code error;
      if (fs::equivalent(path, input, error)) {
        return absl::InvalidArgumentError("Output would replace an input");
      }
      error.clear();
      const auto absolute_output = fs::absolute(path, error);
      if (error) {
        return absl::InvalidArgumentError("Cannot resolve output path");
      }
      const auto absolute_input = fs::absolute(input, error);
      if (error) {
        return absl::InvalidArgumentError("Cannot resolve input path");
      }
      if (absolute_output.lexically_normal() ==
          absolute_input.lexically_normal()) {
        return absl::InvalidArgumentError("Output would replace an input");
      }
    }
  }
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  if (!stream) {
    return absl::PermissionDeniedError("Cannot write " + path.string());
  }
  stream.write(data.data(), static_cast<std::streamsize>(data.size()));
  stream.close();
  return stream ? absl::OkStatus() : absl::DataLossError("Output write failed");
}

absl::StatusOr<std::string> Single(const Options& options, const char* key) {
  const auto found = options.find(key);
  if (found == options.end() || found->second.size() != 1) {
    return absl::InvalidArgumentError(std::string("Expected one ") + key);
  }
  return found->second.front();
}

absl::StatusOr<uint32_t> Number(std::string_view value) {
  int base = 10;
  if (value.starts_with("0x")) {
    value.remove_prefix(2);
    base = 16;
  }
  uint32_t result = 0;
  const auto parsed =
      std::from_chars(value.data(), value.data() + value.size(), result, base);
  if (value.empty() || parsed.ec != std::errc{} ||
      parsed.ptr != value.data() + value.size()) {
    return absl::InvalidArgumentError("Expected an unsigned 32-bit number");
  }
  return result;
}

absl::Status Run(int argc, char** argv) {
  if (argc < 2) {
    return absl::InvalidArgumentError("Expected a command; use --help");
  }
  const std::string command = argv[1];
  if (command != "proxy-sources" && command != "convert-exe" &&
      command != "convert-dll") {
    return absl::InvalidArgumentError("Unknown command: " + command);
  }
  Options options;
  for (int i = 2; i < argc; i += 2) {
    if (i + 1 == argc) {
      return absl::InvalidArgumentError("Option needs a value");
    }
    const std::string key = argv[i];
    const bool proxy_option = key == "--output" || key == "--definition" ||
                              key == "--symbol" || key == "--target-dll";
    const bool conversion_option =
        key == "--input" || key == "--output" || key == "--import-proxy" ||
        key == "--uid3" || key == "--capabilities" || key == "--kernel" ||
        (command == "convert-dll" && key == "--definition");
    if (!(command == "proxy-sources" ? proxy_option : conversion_option)) {
      return absl::InvalidArgumentError("Unknown option for " + command + ": " +
                                        key);
    }
    if (options.contains(key) && key != "--symbol" && key != "--import-proxy") {
      return absl::InvalidArgumentError("Duplicate option: " + key);
    }
    if (std::string_view(argv[i + 1]).starts_with("--")) {
      return absl::InvalidArgumentError("Option needs a value: " + key);
    }
    options[key].push_back(argv[i + 1]);
  }
  ABSL_ASSIGN_OR_RETURN(const auto output, Single(options, "--output"));
  if (command == "proxy-sources") {
    ABSL_ASSIGN_OR_RETURN(const auto definition_path,
                          Single(options, "--definition"));
    ABSL_ASSIGN_OR_RETURN(const auto definition, Read(definition_path));
    ABSL_ASSIGN_OR_RETURN(const auto dll, Single(options, "--target-dll"));
    const auto selected = options.find("--symbol");
    const auto suffix = dll.rfind(".dll");
    if (suffix == std::string::npos || suffix + 4 != dll.size()) {
      return absl::InvalidArgumentError("--target-dll must end in .dll");
    }
    ABSL_ASSIGN_OR_RETURN(
        const auto sources,
        symbian::sdk::GenerateProxy(definition,
                                    selected == options.end()
                                        ? std::vector<std::string>{}
                                        : selected->second,
                                    dll.substr(0, suffix) + ".dso", dll));
    std::error_code error;
    fs::create_directories(output, error);
    if (error) {
      return absl::PermissionDeniedError("Cannot create proxy output");
    }
    ABSL_RETURN_IF_ERROR(
        Write(fs::path(output) / "exports.S", sources.assembly, options));
    ABSL_RETURN_IF_ERROR(Write(fs::path(output) / "exports.map",
                               sources.version_script, options));
    return Write(fs::path(output) / "proxy.ld", sources.linker_script, options);
  }
  ABSL_ASSIGN_OR_RETURN(const auto input, Single(options, "--input"));
  ABSL_ASSIGN_OR_RETURN(const auto elf, Read(input));
  ABSL_ASSIGN_OR_RETURN(const auto uid_text, Single(options, "--uid3"));
  ABSL_ASSIGN_OR_RETURN(const auto uid, Number(uid_text));
  uint32_t capabilities = 0;
  if (options.contains("--capabilities")) {
    ABSL_ASSIGN_OR_RETURN(const auto text, Single(options, "--capabilities"));
    ABSL_ASSIGN_OR_RETURN(capabilities, Number(text));
  }
  std::vector<std::string> proxies;
  if (options.contains("--import-proxy")) {
    for (const auto& proxy : options.at("--import-proxy")) {
      ABSL_ASSIGN_OR_RETURN(auto bytes, Read(proxy));
      proxies.push_back(std::move(bytes));
    }
  }
  std::string kernel = "eka2";
  if (options.contains("--kernel")) {
    ABSL_ASSIGN_OR_RETURN(kernel, Single(options, "--kernel"));
  }
  if (kernel != "eka1" && kernel != "eka2") {
    return absl::InvalidArgumentError("--kernel must be eka1 or eka2");
  }
  absl::StatusOr<std::string> image =
      absl::UnimplementedError("Unsupported profile");
  if (command == "convert-dll") {
    if (kernel != "eka2") {
      return absl::UnimplementedError("EKA1 DLL publication");
    }
    ABSL_ASSIGN_OR_RETURN(const auto path, Single(options, "--definition"));
    ABSL_ASSIGN_OR_RETURN(const auto definition, Read(path));
    image =
        symbian::e32::ConvertDll(elf, definition, proxies, uid, capabilities);
  } else if (kernel == "eka1") {
    if (capabilities) {
      return absl::InvalidArgumentError("EKA1 has no capabilities field");
    }
    image = symbian::e32::ConvertEka1Executable(elf, uid, proxies);
  } else if (proxies.empty()) {
    image = symbian::e32::ConvertPicExecutable(elf, uid, capabilities);
  } else {
    image = symbian::e32::ConvertImportedExecutable(elf, proxies, uid,
                                                    capabilities);
  }
  if (!image.ok()) {
    return image.status();
  }
  return Write(output, *image, options);
}
}  // namespace

int main(int argc, char** argv) {
  if (argc == 2 && std::string_view(argv[1]) == "--help") {
    std::cout
        << "symbian-native convert-exe|convert-dll --input ELF --uid3 UID "
           "--output E32\n"
           "  [--definition DEF] [--import-proxy DSO ...] [--kernel "
           "eka1|eka2]\n"
           "  [--capabilities UINT]\n"
           "symbian-native proxy-sources --definition DEF --symbol NAME ...\n"
           "  --target-dll NAME.dll --output DIRECTORY\n";
    return 0;
  }
  const absl::Status status = Run(argc, argv);
  if (!status.ok()) {
    std::cerr << status << '\n';
  }
  return status.ok() ? 0 : 1;
}
