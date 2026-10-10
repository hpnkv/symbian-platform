// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "symbian/agent/control.h"

#include <string>

#include <absl/status/status_macros.h>
#include <absl/base/nullability.h>

#include "absl/status/status.h"
#include "symbian/status/json_codec.h"

namespace symbian::agent {
namespace {

bool ValidKind(std::uint64_t kind) {
  return kind >= static_cast<std::uint64_t>(ControlKind::kHello) &&
         kind <= static_cast<std::uint64_t>(ControlKind::kAppRegistered);
}

absl::StatusOr<std::uint64_t> UnsignedField(const nlohmann::json& object,
                                            const char* absl_nonnull name,
                                            bool required) {
  const auto found = object.find(name);
  if (found == object.end()) {
    if (required) {
      return absl::InvalidArgumentError("Missing control field");
    }
    return std::uint64_t{0};
  }
  if (!found->is_number_unsigned()) {
    return absl::InvalidArgumentError("Control integer field is invalid");
  }
  return found->get<std::uint64_t>();
}

}  // namespace

absl::StatusOr<ControlMessage> ParseControl(std::string_view encoded) {
  if (encoded.empty()) {
    return absl::InvalidArgumentError("Empty control frame");
  }
  if (encoded.size() > kMaximumControlBytes) {
    return absl::ResourceExhaustedError("Control frame exceeds 4 KiB");
  }
  ABSL_ASSIGN_OR_RETURN(auto parsed, symbian::UnpackMsgpack(encoded, "agent control"));
  if (!parsed.is_object()) {
    return absl::InvalidArgumentError("Control frame must be a map");
  }
  ABSL_ASSIGN_OR_RETURN(auto version, UnsignedField(parsed, "v", true));
  ABSL_ASSIGN_OR_RETURN(auto request_id, UnsignedField(parsed, "id", true));
  ABSL_ASSIGN_OR_RETURN(auto kind, UnsignedField(parsed, "kind", true));
  ABSL_ASSIGN_OR_RETURN(auto deadline,
                        UnsignedField(parsed, "deadline_ms", false));
  if (version != 1 || request_id == 0 || !ValidKind(kind)) {
    return absl::InvalidArgumentError("Unsupported control envelope");
  }
  ControlMessage result;
  result.version = 1;
  result.request_id = request_id;
  result.kind = static_cast<ControlKind>(kind);
  result.deadline_millis = deadline;
  const auto body = parsed.find("body");
  if (body != parsed.end()) {
    if (!body->is_object()) {
      return absl::InvalidArgumentError("Control body must be a map");
    }
    result.body = *body;
  }
  result.extensions = parsed;
  for (const char* absl_nonnull key :
       {"v", "id", "kind", "deadline_ms", "body"}) {
    result.extensions.erase(key);
  }
  return result;
}

absl::StatusOr<std::string> PackControl(const ControlMessage& message) {
  if (message.version != 1 || message.request_id == 0 ||
      !ValidKind(static_cast<std::uint64_t>(message.kind)) ||
      !message.body.is_object() || !message.extensions.is_object()) {
    return absl::InvalidArgumentError("Invalid control envelope");
  }
  nlohmann::json encoded = message.extensions;
  for (const char* absl_nonnull key :
       {"v", "id", "kind", "deadline_ms", "body"}) {
    if (encoded.contains(key)) {
      return absl::InvalidArgumentError("Extension shadows control field");
    }
  }
  encoded["v"] = message.version;
  encoded["id"] = message.request_id;
  encoded["kind"] = static_cast<std::uint8_t>(message.kind);
  encoded["deadline_ms"] = message.deadline_millis;
  encoded["body"] = message.body;
  ABSL_ASSIGN_OR_RETURN(auto bytes,
                        symbian::PackMsgpack(encoded, "agent control"));
  if (bytes.size() > kMaximumControlBytes) {
    return absl::ResourceExhaustedError("Control frame exceeds 4 KiB");
  }
  return bytes;
}

}  // namespace symbian::agent
