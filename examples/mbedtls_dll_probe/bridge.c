#include <mbedtls/sha256.h>
#include <mbedtls/x509_crt.h>
#include <symbian_mbedtls/platform.h>
#include "test_certificate.h"

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

__attribute__((visibility("default")))
int MbedVerifyCert(void) {
  mbedtls_x509_crt certificate;
  mbedtls_x509_crt trust;
  mbedtls_x509_crt_init(&certificate);
  mbedtls_x509_crt_init(&trust);
  int result = mbedtls_x509_crt_parse(
      &certificate, (const unsigned char*)kTestCertificate,
      sizeof(kTestCertificate));
  if (result == 0) {
    result = mbedtls_x509_crt_parse(
        &trust, (const unsigned char*)kTestCertificate,
        sizeof(kTestCertificate));
  }
  if (result == 0) {
    unsigned flags = 0;
    result = mbedtls_x509_crt_verify(&certificate, &trust, NULL,
                                     "sdk-test", &flags, NULL, NULL);
    if (result != 0 || flags != 0) result = -136;
  }
  if (result == 0) {
    unsigned flags = 0;
    result = mbedtls_x509_crt_verify(&certificate, &trust, NULL,
                                     "wrong-name", &flags, NULL, NULL);
    if (result == 0 || (flags & MBEDTLS_X509_BADCERT_CN_MISMATCH) == 0)
      result = -137;
    else
      result = 0;
  }
  if (result == 0) {
    mbedtls_x509_crt expired;
    mbedtls_x509_crt_init(&expired);
    result = mbedtls_x509_crt_parse(
        &expired, (const unsigned char*)kExpiredCertificate,
        sizeof(kExpiredCertificate));
    if (result == 0) {
      unsigned flags = 0;
      result = mbedtls_x509_crt_verify(&expired, &trust, NULL,
                                       "sdk-test", &flags, NULL, NULL);
      if (result == 0 || (flags & MBEDTLS_X509_BADCERT_EXPIRED) == 0)
        result = -139;
      else
        result = 0;
    }
    mbedtls_x509_crt_free(&expired);
  }
  mbedtls_x509_crt_free(&trust);
  mbedtls_x509_crt_free(&certificate);
  return result;
}
