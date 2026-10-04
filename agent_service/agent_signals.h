// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef AGENT_SERVICE_AGENT_SIGNALS_H_
#define AGENT_SERVICE_AGENT_SIGNALS_H_

#include <cstdint>

namespace agent_service {

constexpr std::int32_t kPropertyCategory =
    static_cast<std::int32_t>(0xe0000a31u);
constexpr std::uint32_t kRaisePanelKey = 0x4147454e;
constexpr std::uint32_t kStopServiceKey = 0x53544f50;

}  // namespace agent_service

#endif  // AGENT_SERVICE_AGENT_SIGNALS_H_
