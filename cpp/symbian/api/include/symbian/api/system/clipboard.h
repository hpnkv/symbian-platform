// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_API_SYSTEM_CLIPBOARD_H_
#define SYMBIAN_API_SYSTEM_CLIPBOARD_H_

#include <string>
#include <string_view>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace symbian::api::system {

// Replaces the device's interoperable plain-text clipboard content. The
// caller keeps text valid until the synchronous operation returns.
absl::Status CopyTextToClipboard(std::u16string_view text);

// Reads interoperable plain text from the device clipboard. An empty clipboard
// or one without plain text returns an empty string. Service failures return a
// status. Text is limited to 64 Ki UTF-16 code units.
absl::StatusOr<std::u16string> ReadTextFromClipboard();

}  // namespace symbian::api::system

#endif  // SYMBIAN_API_SYSTEM_CLIPBOARD_H_
