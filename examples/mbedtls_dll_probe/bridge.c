#include <mbedtls/sha256.h>
#include <symbian_mbedtls/platform.h>

__attribute__((visibility("default")))
int MbedSha256(const unsigned char *input, unsigned size,
               unsigned char *output) {
  return mbedtls_sha256(input, size, output, 0);
}

__attribute__((visibility("default")))
int MbedUtcProbe(void) {
  const time_t known = (time_t)1704067200;  // 2024-01-01 00:00:00 UTC.
  const time_t invalid = (time_t)0;
  struct tm converted;
  if (symbian_mbedtls_utc_gmtime_r(&known, &converted) == NULL ||
      converted.tm_year != 124 || converted.tm_mon != 0 ||
      converted.tm_mday != 1 || converted.tm_hour != 0 ||
      converted.tm_min != 0 || converted.tm_sec != 0 ||
      symbian_mbedtls_utc_gmtime_r(&invalid, &converted) != NULL) {
    return -133;
  }
  time_t current = 0;
  if (symbian_mbedtls_time(&current) < (time_t)1577836800) {
    return -134;
  }
  return 0;
}
