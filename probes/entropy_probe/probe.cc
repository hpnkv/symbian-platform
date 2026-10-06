// SPDX-License-Identifier: Apache-2.0
#include <cstddef>

#include <absl/base/nullability.h>

#include "mbedtls/entropy.h"

extern "C" int mbedtls_hardware_poll(void* absl_nullable data,
                                     unsigned char* absl_nullable output,
                                     std::size_t length,
                                     std::size_t* absl_nullable produced);

int main() {
  unsigned char first[32];
  for (auto& byte : first) {
    byte = 0xa5;
  }
  std::size_t count = 123;
  const int result =
      mbedtls_hardware_poll(nullptr, first, sizeof(first), &count);
#if defined(SYMBIAN_EXPECT_SECURE_ENTROPY)
  if (result != 0 || count != sizeof(first)) {
    return -151;
  }
  unsigned char second[32] = {};
  count = 123;
  if (mbedtls_hardware_poll(nullptr, second, sizeof(second), &count) != 0 ||
      count != sizeof(second)) {
    return -152;
  }
  bool changed = false;
  bool nonzero = false;
  for (std::size_t i = 0; i < sizeof(first); ++i) {
    changed |= first[i] != second[i];
    nonzero |= first[i] != 0;
  }
  if (!changed || !nonzero) {
    return -153;
  }
#else
  if (result != MBEDTLS_ERR_ENTROPY_SOURCE_FAILED || count != 0) {
    return -154;
  }
  for (const auto byte : first) {
    if (byte != 0) {
      return -155;
    }
  }
#endif
  count = 123;
  if (mbedtls_hardware_poll(nullptr, first, 0, &count) !=
          MBEDTLS_ERR_ENTROPY_SOURCE_FAILED ||
      count != 0 ||
      mbedtls_hardware_poll(nullptr, nullptr, sizeof(first), &count) !=
          MBEDTLS_ERR_ENTROPY_SOURCE_FAILED ||
      mbedtls_hardware_poll(nullptr, first, sizeof(first), nullptr) !=
          MBEDTLS_ERR_ENTROPY_SOURCE_FAILED) {
    return -156;
  }
  return 0;
}
