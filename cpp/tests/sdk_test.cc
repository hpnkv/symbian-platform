#include <string>

#include <absl/status/status.h>
#include <gtest/gtest.h>

#include "symbian/sdk/exports.h"

namespace symbian::sdk {
namespace {

constexpr std::string_view kDefinition = R"(; frozen interface
EXPORTS
_ZN4User4ExitEi @ 641 NONAME ; User::Exit(int)
Data @ 17 NONAME DATA 4
Removed @ 18 NONAME ABSENT
Function @ 2 NONAME
)";

TEST(SdkTest, ReadsFrozenFunctionDataAndAbsentExportsWithoutRenumbering) {
  const auto table = ParseExports(kDefinition);
  ASSERT_TRUE(table.ok()) << table.status();
  ASSERT_EQ(table->size(), 4);
  EXPECT_EQ((*table)[0].ordinal, 2);
  EXPECT_TRUE((*table)[1].data);
  EXPECT_TRUE((*table)[2].absent);
  EXPECT_EQ((*table)[3].symbol, "_ZN4User4ExitEi");
  EXPECT_EQ((*table)[3].ordinal, 641);
  EXPECT_TRUE(ParseExports("EXPORTS\r\n\tName @ 65535 NONAME\r\n").ok());
}

TEST(SdkTest, RejectsDuplicateMalformedAndUnsupportedDefinitions) {
  for (const std::string& text :
       {"EXPORTS\nA @ 1 NONAME\nB @ 1 NONAME",
        "EXPORTS\nA @ 1 NONAME\nA @ 2 NONAME", "EXPORTS\nA @ 0 NONAME",
        "EXPORTS\nA @ 65536 NONAME", "EXPORTS\nA @ 1 NONAME DATA",
        "EXPORTS\nA @ 1 NONAME DATA 0", "EXPORTS\nA @ 1 NONAME ABSENT ABSENT",
        "EXPORTS\nA=B @ 1 NONAME", "EXPORTS\nA @ 1 NONAME R3UNUSED",
        "IMPORTS\nA @ 1 NONAME", "EXPORTS\nA @ 1", "EXPORTS"}) {
    EXPECT_FALSE(ParseExports(text).ok()) << text;
  }
  EXPECT_EQ(ParseExports(std::string(8 * 1024 * 1024 + 1, 'x')).status().code(),
            absl::StatusCode::kResourceExhausted);
  EXPECT_FALSE(ParseExports("EXPORTS\nA\x01 @ 1 NONAME").ok());
}

TEST(SdkTest, GeneratesStableOrdinalSourceInOrdinalOrder) {
  const auto first = GenerateProxy(kDefinition, {"_ZN4User4ExitEi", "Function"},
                                   "euser.dso", "euser.dll");
  const auto second = GenerateProxy(
      kDefinition, {"Function", "_ZN4User4ExitEi"}, "euser.dso", "euser.dll");
  ASSERT_TRUE(first.ok()) << first.status();
  ASSERT_TRUE(second.ok());
  EXPECT_EQ(first->assembly, second->assembly);
  EXPECT_EQ(first->version_script, second->version_script);
  EXPECT_NE(first->assembly.find(".word 641\n"), std::string::npos);
  EXPECT_EQ(first->exports.front().ordinal, 2);
  EXPECT_EQ(first->version_script.substr(0, 9), "euser.dll");
}

TEST(SdkTest, MissingRemovedDataDuplicateAndUnsafeSelectionsAreRejected) {
  EXPECT_EQ(GenerateProxy(kDefinition, {"Missing"}, "euser.dso", "euser.dll")
                .status()
                .code(),
            absl::StatusCode::kNotFound);
  for (const auto& name : {"Removed", "Data"}) {
    EXPECT_EQ(GenerateProxy(kDefinition, {name}, "euser.dso", "euser.dll")
                  .status()
                  .code(),
              absl::StatusCode::kUnimplemented);
  }
  EXPECT_FALSE(GenerateProxy(kDefinition, {"Function", "Function"}, "euser.dso",
                             "euser.dll")
                   .ok());
  for (const auto& name : {"../euser.dll", "euser.dll; evil", "\"euser.dll\"",
                           "euser{1}.dll", "euser\n.dll"}) {
    EXPECT_FALSE(
        GenerateProxy(kDefinition, {"Function"}, "euser.dso", name).ok());
  }
  EXPECT_FALSE(GenerateProxy(kDefinition, {}, "euser.dso", "euser.dll").ok());
}

TEST(SdkTest, ProxyInspectionRejectsUnboundedAndNonElfInput) {
  EXPECT_EQ(InspectProxy("not an ELF").status().code(),
            absl::StatusCode::kDataLoss);
  EXPECT_EQ(
      InspectProxy(std::string(2 * 1024 * 1024 + 1, '\0')).status().code(),
      absl::StatusCode::kResourceExhausted);
}

}  // namespace
}  // namespace symbian::sdk
