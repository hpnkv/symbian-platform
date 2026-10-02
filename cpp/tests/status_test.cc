#include "symbian/status/status.h"

#include <string>

#include <absl/status/status.h>
#include <absl/strings/cord.h>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "symbian/status/json_codec.h"

namespace symbian {
namespace {

TEST(StatusTest, DetailsSurviveNativeJsonAndMsgpack) {
  const nlohmann::json details = {{{"path", "missing"}, {"ordinal", 42}}};
  const absl::Status status =
      MakeStatus(absl::StatusCode::kNotFound, "absent", details);
  const auto document = StatusToJson(status);
  ASSERT_TRUE(document.ok()) << document.status();
  const auto packed = PackMsgpack(*document, "status");
  ASSERT_TRUE(packed.ok()) << packed.status();
  const auto unpacked = UnpackMsgpack(*packed, "status");
  ASSERT_TRUE(unpacked.ok()) << unpacked.status();
  const auto restored = StatusFromJson(*unpacked);
  ASSERT_TRUE(restored.ok()) << restored.status();
  EXPECT_EQ(*restored, status);
  EXPECT_EQ(StatusDetails(*restored), details);
  EXPECT_TRUE(restored->GetPayload(kStatusDetailsPayloadUrl).has_value());
}

TEST(StatusTest, CorruptPayloadDoesNotMasqueradeAsStructuredDetails) {
  absl::Status status = absl::NotFoundError("absent");
  status.SetPayload(kStatusDetailsPayloadUrl, absl::Cord("{broken"));
  EXPECT_EQ(StatusDetails(status), nlohmann::json::array());
  status.SetPayload(kStatusDetailsPayloadUrl, absl::Cord("{}"));
  EXPECT_EQ(StatusDetails(status), nlohmann::json::array());
}

TEST(StatusTest, UntrustedCodesAndDetailsAreRejected) {
  for (const nlohmann::json& document :
       {nlohmann::json{{"code", 17}, {"message", "bad"}},
        nlohmann::json{{"code", true}, {"message", "bad"}},
        nlohmann::json{{"code", 3}, {"message", 1}},
        nlohmann::json{{"code", 3}, {"message", "bad"}, {"details", 1}}}) {
    const auto result = StatusFromJson(document);
    EXPECT_FALSE(result.ok());
    EXPECT_EQ(result.status().code(), absl::StatusCode::kInvalidArgument);
  }
}

TEST(StatusCodecTest, InvalidUtf8ValuesAndKeysReturnErrorsWithoutExceptions) {
  const std::string invalid("\xed\xa0\x80",
                            3);  // Surrogate, not a Unicode scalar.
  for (const nlohmann::json& value : {nlohmann::json{{"nested", {invalid}}},
                                      nlohmann::json{{invalid, "value"}}}) {
    EXPECT_EQ(DumpJson(value, "details").status().code(),
              absl::StatusCode::kInvalidArgument);
    EXPECT_EQ(PackMsgpack(value, "details").status().code(),
              absl::StatusCode::kInvalidArgument);
    EXPECT_EQ(MakeStatus(absl::StatusCode::kNotFound, "bad",
                         nlohmann::json::array({value}))
                  .code(),
              absl::StatusCode::kInternal);
  }
}

TEST(StatusCodecTest, MalformedDocumentsDoNotAbortANoExceptionsBinary) {
  EXPECT_EQ(ParseJson("{broken", "status").status().code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_FALSE(UnpackMsgpack(std::string("\x91", 1), "status").ok());
  EXPECT_FALSE(UnpackMsgpack(std::string("\xc0\xc0", 2), "status").ok());
  const std::string nested = std::string(258, '\x91') + '\xc0';
  EXPECT_EQ(UnpackMsgpack(nested, "status").status().code(),
            absl::StatusCode::kResourceExhausted);
}

TEST(StatusTest, ProtocolMappingsRetainA11Contract) {
  EXPECT_EQ(StatusCodeFromHttp(404), absl::StatusCode::kNotFound);
  EXPECT_EQ(StatusCodeToHttp(absl::StatusCode::kPermissionDenied), 403);
  EXPECT_EQ(StatusCodeFromWebSocket(4007), absl::StatusCode::kUnauthenticated);
  EXPECT_EQ(StatusCodeToWebSocket(absl::StatusCode::kUnavailable), 4013);
}

}  // namespace
}  // namespace symbian
