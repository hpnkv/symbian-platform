// SPDX-License-Identifier: Apache-2.0
#include <absl/base/nullability.h>

#include "symbian_mbedtls/platform.h"

// A missing or implausible wall clock must make X.509 date checks fail.
// 2020-01-01 UTC is a conservative floor for this 2026 development profile.
static const time_t kEarliestTrustedUtc = (time_t)1577836800;

time_t symbian_mbedtls_time(time_t* absl_nullable output) {
  struct timespec now;
  time_t value = (time_t)-1;
  if (clock_gettime(CLOCK_REALTIME, &now) == 0 &&
      now.tv_sec >= kEarliestTrustedUtc) {
    value = now.tv_sec;
  }
  if (output != NULL) {
    *output = value;
  }
  return value;
}

#ifdef SYMBIAN_MBEDTLS_GUEST
// Mbed TLS 3.x still calls time() directly from its TLS 1.3 session path.
// Route those calls through the same checked UTC source used for X.509.
time_t time(time_t* absl_nullable output) {
  return symbian_mbedtls_time(output);
}
#endif

struct tm* absl_nullable symbian_mbedtls_utc_gmtime_r(
    const time_t* absl_nullable input, struct tm* absl_nullable output) {
  if (input == NULL || output == NULL || *input < kEarliestTrustedUtc) {
    return NULL;
  }
  return gmtime_r(input, output);
}

#ifdef SYMBIAN_MBEDTLS_GUEST
struct tm* absl_nullable mbedtls_platform_gmtime_r(
    const time_t* absl_nullable input, struct tm* absl_nullable output) {
  return symbian_mbedtls_utc_gmtime_r(input, output);
}
#endif
