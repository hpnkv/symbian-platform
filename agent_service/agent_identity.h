// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef AGENT_SERVICE_AGENT_IDENTITY_H_
#define AGENT_SERVICE_AGENT_IDENTITY_H_

#include <array>
#include <cstdint>
#include <memory>
#include <string>

#include "absl/status/statusor.h"

namespace agent_service {

// A device-owned secret and its short visual fingerprint. Worker tasks share
// this owner because they can finish after the service stops accepting work.
class AgentIdentity final {
 public:
  using Key = std::array<std::uint8_t, 32>;

  AgentIdentity(const AgentIdentity&) = delete;
  AgentIdentity& operator=(const AgentIdentity&) = delete;
  ~AgentIdentity();

  static absl::StatusOr<std::shared_ptr<const AgentIdentity>> OpenOrCreate();

  const Key& key() const { return key_; }

  const std::string& pairing_code() const { return pairing_code_; }

 private:
  explicit AgentIdentity(Key key, std::string pairing_code);

  Key key_;
  std::string pairing_code_;
};

}  // namespace agent_service

#endif  // AGENT_SERVICE_AGENT_IDENTITY_H_
