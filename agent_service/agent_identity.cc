// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include <absl/status/status_macros.h>
#include "agent_identity.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "agent_key.h"
#include "mbedtls/entropy.h"
#include "mbedtls/platform_util.h"
#include "mbedtls/sha256.h"
#include "symbian/api/storage/storage.h"

namespace agent_service {
namespace {

constexpr std::u16string_view kKeyDirectory = u"C:\\private\\e0000a31";
constexpr std::u16string_view kKeyPath = u"C:\\private\\e0000a31\\agent.key";

absl::StatusOr<AgentIdentity::Key> ReadKey() {
  ABSL_ASSIGN_OR_RETURN(auto file, symbian::api::storage::ReadOnlyFile::Open(kKeyPath));
  ABSL_ASSIGN_OR_RETURN(const auto size, file.Size());
  if (size != 32) {
    return absl::DataLossError("Agent identity has an invalid key length");
  }
  AgentIdentity::Key key{};
  const auto destination =
      std::span(reinterpret_cast<std::byte*>(key.data()), key.size());
  ABSL_ASSIGN_OR_RETURN(const auto bytes, file.ReadAt(0, destination));
  if (bytes != key.size()) {
    return absl::DataLossError("Agent identity key is truncated");
  }
  return key;
}

absl::StatusOr<AgentIdentity::Key> CreateKey() {
  AgentIdentity::Key key{};
  mbedtls_entropy_context entropy;
  mbedtls_entropy_init(&entropy);
  const int random_result =
      mbedtls_entropy_func(&entropy, key.data(), key.size());
  mbedtls_entropy_free(&entropy);
  if (random_result != 0) {
    return absl::UnavailableError("Agent identity entropy unavailable");
  }
  ABSL_RETURN_IF_ERROR(
      symbian::api::storage::CreateDirectories(kKeyDirectory));
  auto file = symbian::api::storage::WritableFile::Open(
      kKeyPath, symbian::api::storage::WriteMode::kCreateNew);
  if (!file.ok()) {
    if (file.status().code() == absl::StatusCode::kAlreadyExists) {
      return ReadKey();
    }
    return file.status();
  }
  const auto bytes =
      std::span(reinterpret_cast<const std::byte*>(key.data()), key.size());
  ABSL_RETURN_IF_ERROR(file->WriteAt(0, bytes));
  ABSL_RETURN_IF_ERROR(file->Flush());
  return key;
}

absl::StatusOr<std::string> PairingCode(const AgentIdentity::Key& key) {
  std::array<unsigned char, 32> digest{};
  if (mbedtls_sha256(key.data(), key.size(), digest.data(), 0) != 0) {
    return absl::InternalError("Agent identity digest unavailable");
  }
  std::uint32_t value = (std::uint32_t{digest[0]} << 24) |
                        (std::uint32_t{digest[1]} << 16) |
                        (std::uint32_t{digest[2]} << 8) | digest[3];
  constexpr char kAlphabet[] = "ABCEGIKNOPRSTU";
  std::string code(8, 'A');
  for (int index = 7; index >= 0; --index) {
    code[static_cast<std::size_t>(index)] =
        kAlphabet[value % (sizeof(kAlphabet) - 1)];
    value /= sizeof(kAlphabet) - 1;
  }
  return code;
}

#if SYMBIAN_AGENT_EMULATOR_PROFILE
AgentIdentity::Key EmulatorKey() {
  constexpr char hex[] = SYMBIAN_AGENT_KEY_HEX;
  AgentIdentity::Key key{};
  const auto nibble = [](char digit) -> std::uint8_t {
    return digit >= 'a' ? static_cast<std::uint8_t>(digit - 'a' + 10)
                        : static_cast<std::uint8_t>(digit - '0');
  };
  for (std::size_t index = 0; index < key.size(); ++index) {
    key[index] = static_cast<std::uint8_t>((nibble(hex[2 * index]) << 4) |
                                           nibble(hex[2 * index + 1]));
  }
  return key;
}
#endif

}  // namespace

AgentIdentity::AgentIdentity(Key key, std::string pairing_code)
    : key_(std::move(key)), pairing_code_(std::move(pairing_code)) {}

AgentIdentity::~AgentIdentity() {
  mbedtls_platform_zeroize(key_.data(), key_.size());
}

absl::StatusOr<std::shared_ptr<const AgentIdentity>>
AgentIdentity::OpenOrCreate() {
#if SYMBIAN_AGENT_EMULATOR_PROFILE
  absl::StatusOr<Key> key = EmulatorKey();
#else
  auto key = ReadKey();
  if (!key.ok() && key.status().code() == absl::StatusCode::kNotFound) {
    key = CreateKey();
  }
#endif
  ABSL_ASSIGN_OR_RETURN(auto resolved_key, std::move(key));
  ABSL_ASSIGN_OR_RETURN(auto code, PairingCode(resolved_key));
  return std::shared_ptr<const AgentIdentity>(
      new AgentIdentity(std::move(resolved_key), std::move(code)));
}

}  // namespace agent_service
