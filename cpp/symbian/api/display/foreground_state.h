// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_API_DISPLAY_FOREGROUND_STATE_H_
#define SYMBIAN_API_DISPLAY_FOREGROUND_STATE_H_

namespace symbian::api::display::internal {

// Remembers the focus state immediately before the last SDK window closed.
// This lets a top-level CHECK_OK(Main()) show an error after Main's local
// window has been destroyed.
bool LastWindowClosedInForeground();

}  // namespace symbian::api::display::internal

#endif  // SYMBIAN_API_DISPLAY_FOREGROUND_STATE_H_
