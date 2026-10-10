// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "window_task_identity.h"

namespace agent_service::internal {

int SetWindowTaskIdentity(RWindowGroup* absl_nonnull group, std::uint32_t uid,
                          std::string_view caption) {
  // AppArc reads NUL-separated ready status, UID3, and caption fields.
  std::uint16_t name_data[96] = {};
  int length = 0;
  name_data[length++] = '4';
  name_data[length++] = '0';
  name_data[length++] = 0;
  for (int shift = 28; shift >= 0; shift -= 4) {
    const std::uint32_t digit = (uid >> shift) & 15;
    name_data[length++] = digit < 10 ? '0' + digit : 'a' + digit - 10;
  }
  name_data[length++] = 0;
  if (caption.size() > 96 - length - 1) {
    return KErrArgument;
  }
  for (const char character : caption) {
    name_data[length++] = static_cast<unsigned char>(character);
  }
  name_data[length++] = 0;
  const TPtrC16 name(name_data, length);
  return group->SetName(name);
}

}  // namespace agent_service::internal
