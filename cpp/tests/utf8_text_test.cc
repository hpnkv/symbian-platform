// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "symbian/api/text/utf8.h"

#include <string>
#include <string_view>

#include "absl/status/status.h"
#include "gtest/gtest.h"

namespace {

TEST(Utf8TextTest, RoundTripsBmpSupplementaryAndEmbeddedNull) {
  constexpr std::string_view utf8(
      "A\0\xE2\x82\xAC\xF0\x9F\x8E\xB2", 9);
  const auto wide = symbian::api::text::Utf8ToUtf16(utf8);
  ASSERT_TRUE(wide.ok()) << wide.status();
  EXPECT_EQ(*wide, std::u16string_view(u"A\0\u20AC\U0001F3B2", 5));
  const auto narrow = symbian::api::text::Utf16ToUtf8(*wide);
  ASSERT_TRUE(narrow.ok()) << narrow.status();
  EXPECT_EQ(*narrow, utf8);
}

TEST(Utf8TextTest, RejectsMalformedSequencesAndUnpairedSurrogates) {
  for (std::string_view invalid : {
           "\xC0\xAF", "\xE2\x82", "\xED\xA0\x80", "\xF4\x90\x80\x80",
           "\xE2\x28\xA1"}) {
    EXPECT_EQ(symbian::api::text::Utf8ToUtf16(invalid).status().code(),
              absl::StatusCode::kInvalidArgument);
  }
  EXPECT_EQ(symbian::api::text::Utf16ToUtf8(u"\xD800").status().code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_EQ(symbian::api::text::Utf16ToUtf8(u"\xDC00").status().code(),
            absl::StatusCode::kInvalidArgument);
}

}  // namespace
