// SPDX-License-Identifier: Apache-2.0
#include <cstring>

#include <absl/base/nullability.h>
#include <gtest/gtest.h>

namespace {
int source_result;
size_t source_produced;
int seed_calls;
double credited_entropy;
}  // namespace

extern "C" int mbedtls_hardware_poll(void* absl_nullable,
                                     unsigned char* absl_nonnull output,
                                     size_t size,
                                     size_t* absl_nonnull produced) {
  std::memset(output, 0xa5, size);
  *produced = source_produced;
  return source_result;
}

extern "C" void RAND_add(const void* absl_nonnull, int size, double entropy) {
  EXPECT_EQ(size, 32);
  ++seed_calls;
  credited_entropy = entropy;
}

extern "C" int RAND_poll();

TEST(LegacyEntropy, CreditsOnlyACompleteSuccessfulSecureFill) {
  source_result = 0;
  source_produced = 32;
  seed_calls = 0;
  EXPECT_EQ(RAND_poll(), 1);
  EXPECT_EQ(seed_calls, 1);
  EXPECT_EQ(credited_entropy, 32);

  source_produced = 31;
  EXPECT_EQ(RAND_poll(), 0);
  EXPECT_EQ(seed_calls, 1);

  source_result = -1;
  source_produced = 32;
  EXPECT_EQ(RAND_poll(), 0);
  EXPECT_EQ(seed_calls, 1);

  source_produced = 0;
  EXPECT_EQ(RAND_poll(), 0);
  EXPECT_EQ(seed_calls, 1);
}
