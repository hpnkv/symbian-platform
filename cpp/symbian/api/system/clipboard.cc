// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "symbian/api/system/clipboard.h"

#include <cstdint>
#include <string>
#include <string_view>

#include <absl/base/nullability.h>

namespace std {
bool uncaught_exception();
}

#define __EXCEPTION__
#include <e32base.h>
#undef __EXCEPTION__
#include <baclipb.h>
#include <f32file.h>
#include <txtetext.h>

#include "symbian/native_status.h"

namespace symbian::api::system {
namespace {

void CopyTextL(RFs* absl_nonnull session, const TDesC& text) {
  CClipboard* absl_nonnull clipboard = CClipboard::NewForWritingLC(*session);
  CPlainText* absl_nonnull plain = CPlainText::NewL();
  CleanupStack::PushL(plain);
  plain->InsertL(0, text);
  plain->CopyToStoreL(clipboard->Store(), clipboard->StreamDictionary(), 0,
                      text.Length());
  clipboard->CommitL();
  CleanupStack::PopAndDestroy(2, clipboard);
}

void ReadTextL(RFs* absl_nonnull session, std::u16string* absl_nonnull output) {
  CClipboard* absl_nonnull clipboard = CClipboard::NewForReadingLC(*session);
  CPlainText* absl_nonnull plain = CPlainText::NewL();
  CleanupStack::PushL(plain);
  const TInt length = plain->PasteFromStoreL(clipboard->Store(),
                                             clipboard->StreamDictionary(), 0);
  if (length > 64 * 1024) {
    User::Leave(KErrTooBig);
  }
  HBufC* absl_nonnull buffer = HBufC::NewLC(length);
  if (TPtr content = buffer->Des(); length > 0) {
    plain->Extract(content, 0, length);
    output->assign(reinterpret_cast<const char16_t*>(content.Ptr()),
                   static_cast<std::size_t>(content.Length()));
  }
  CleanupStack::PopAndDestroy(3, clipboard);
}

}  // namespace

absl::Status CopyTextToClipboard(std::u16string_view text) {
  if (text.size() > 64 * 1024) {
    return absl::InvalidArgumentError("clipboard text exceeds 64 KiB");
  }
  RFs session;
  if (const TInt connected = session.Connect(); connected != KErrNone) {
    return symbian::StatusFromNativeError(connected, "clipboard file server");
  }
  const TPtrC descriptor(reinterpret_cast<const TUint16*>(text.data()),
                         static_cast<TInt>(text.size()));
  TRAPD(error, CopyTextL(&session, descriptor));
  session.RSessionBase::Close();
  return symbian::StatusFromNativeError(error, "clipboard write");
}

absl::StatusOr<std::u16string> ReadTextFromClipboard() {
  RFs session;
  if (const TInt connected = session.Connect(); connected != KErrNone) {
    return symbian::StatusFromNativeError(connected, "clipboard file server");
  }
  std::u16string text;
  TRAPD(error, ReadTextL(&session, &text));
  session.RSessionBase::Close();
  if (error != KErrNone) {
    return symbian::StatusFromNativeError(error, "clipboard read");
  }
  return text;
}

}  // namespace symbian::api::system
