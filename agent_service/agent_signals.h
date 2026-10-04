// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef AGENT_SERVICE_AGENT_SIGNALS_H_
#define AGENT_SERVICE_AGENT_SIGNALS_H_

#include <e32std.h>

namespace agent_service {

const TUid kPropertyCategory = TUid::Uid(static_cast<TInt32>(0xe0000a31u));
constexpr TUint kRaisePanelKey = 0x4147454e;
constexpr TUint kStopServiceKey = 0x53544f50;

}  // namespace agent_service

#endif  // AGENT_SERVICE_AGENT_SIGNALS_H_
