// SPDX-License-Identifier: Apache-2.0
#include <cstdlib>
#include <cstring>
#include <limits>

#include <absl/base/nullability.h>
#include <gtest/gtest.h>
#include <time.h>

#include "mbedtls/gcm.h"
#include "mbedtls/platform_util.h"
#include "symbian_mbedtls/platform.h"
#include "vectors.h"

namespace {
bool fail_allocation = false;
int allocation_calls = 0;
}  // namespace

extern "C" void* absl_nullable SymbianRuntimeAllocate(size_t size) {
  ++allocation_calls;
  return fail_allocation ? nullptr : std::malloc(size);
}

extern "C" void SymbianRuntimeFree(void* absl_nullable pointer) {
  std::free(pointer);
}

TEST(Crypto, IndependentKnownAnswers) {
  EXPECT_EQ(SymbianMbedTlsKnownAnswers(), 0);
}

TEST(Crypto, GcmRejectsChangedTag) {
  const unsigned char key[16] = {};
  const unsigned char iv[12] = {};
  const unsigned char input[16] = {};
  unsigned char encrypted[16], decrypted[16], tag[16];
  mbedtls_gcm_context context;
  mbedtls_gcm_init(&context);
  ASSERT_EQ(mbedtls_gcm_setkey(&context, MBEDTLS_CIPHER_ID_AES, key, 128), 0);
  ASSERT_EQ(mbedtls_gcm_crypt_and_tag(&context, MBEDTLS_GCM_ENCRYPT,
                                      sizeof(input), iv, sizeof(iv), nullptr, 0,
                                      input, encrypted, sizeof(tag), tag),
            0);
  EXPECT_EQ(
      mbedtls_gcm_auth_decrypt(&context, sizeof(input), iv, sizeof(iv), nullptr,
                               0, tag, sizeof(tag), encrypted, decrypted),
      0);
  EXPECT_EQ(std::memcmp(decrypted, input, sizeof(input)), 0);
  tag[0] ^= 1;
  EXPECT_EQ(
      mbedtls_gcm_auth_decrypt(&context, sizeof(input), iv, sizeof(iv), nullptr,
                               0, tag, sizeof(tag), encrypted, decrypted),
      MBEDTLS_ERR_GCM_AUTH_FAILED);
  mbedtls_gcm_free(&context);
}

TEST(Platform, ErasesSecrets) {
  unsigned char bytes[32];
  std::memset(bytes, 0xa5, sizeof(bytes));
  mbedtls_platform_zeroize(bytes, sizeof(bytes));
  for (unsigned char byte : bytes)
    EXPECT_EQ(byte, 0);
  mbedtls_platform_zeroize(nullptr, 0);
}

TEST(Platform, UtcRejectsImplausibleClockValues) {
  time_t current = 0;
  EXPECT_EQ(symbian_mbedtls_time(&current), current);
  EXPECT_GE(current, static_cast<time_t>(1577836800));
  struct tm result{};
  const time_t invalid = static_cast<time_t>(-1);
  EXPECT_EQ(symbian_mbedtls_utc_gmtime_r(&invalid, &result), nullptr);
  EXPECT_NE(symbian_mbedtls_utc_gmtime_r(&current, &result), nullptr);
  EXPECT_GE(result.tm_year + 1900, 2020);
}

TEST(Platform, CallocZerosAndRejectsOverflow) {
  void* absl_nullable bytes = symbian_mbedtls_calloc(8, 4);
  ASSERT_NE(bytes, nullptr);
  for (int i = 0; i < 32; ++i) {
    EXPECT_EQ(static_cast<unsigned char*>(bytes)[i], 0);
  }
  symbian_mbedtls_free(bytes);
  const int calls = allocation_calls;
  EXPECT_EQ(symbian_mbedtls_calloc(std::numeric_limits<size_t>::max(), 2),
            nullptr);
  EXPECT_EQ(symbian_mbedtls_calloc(1, 0x80000000u), nullptr);
  EXPECT_EQ(allocation_calls, calls);
}

TEST(Platform, CallocReportsExhaustionAndAllowsZeroSize) {
  fail_allocation = true;
  EXPECT_EQ(symbian_mbedtls_calloc(1, 32), nullptr);
  fail_allocation = false;
  void* absl_nullable bytes = symbian_mbedtls_calloc(0, 32);
  EXPECT_NE(bytes, nullptr);
  symbian_mbedtls_free(bytes);
  symbian_mbedtls_free(nullptr);
}
