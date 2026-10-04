// SPDX-License-Identifier: Apache-2.0
#include "symbian_mbedtls/platform.h"

// A missing or implausible wall clock must make X.509 date checks fail.
// 2020-01-01 UTC is a conservative floor for this 2026 development profile.
static const time_t kEarliestTrustedUtc = (time_t)1577836800;

time_t symbian_mbedtls_time(time_t* output) {
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

struct tm* symbian_mbedtls_utc_gmtime_r(const time_t* input,
                                        struct tm* output) {
  if (input == NULL || output == NULL || *input < kEarliestTrustedUtc) {
    return NULL;
  }
  return gmtime_r(input, output);
}

#ifdef SYMBIAN_MBEDTLS_GUEST
struct tm* mbedtls_platform_gmtime_r(const time_t* input, struct tm* output) {
  return symbian_mbedtls_utc_gmtime_r(input, output);
}
#endif
