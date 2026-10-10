// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef AGENT_SERVICE_WINDOW_TASK_IDENTITY_H_
#define AGENT_SERVICE_WINDOW_TASK_IDENTITY_H_

#include <cstdint>
#include <string_view>

#include <absl/base/nullability.h>
#include <w32std.h>

namespace agent_service::internal {

// Adapts an SDK window group to the AppArc task-list name contract.
int SetWindowTaskIdentity(RWindowGroup* absl_nonnull group, std::uint32_t uid,
                          std::string_view caption);

}  // namespace agent_service::internal

#endif  // AGENT_SERVICE_WINDOW_TASK_IDENTITY_H_
