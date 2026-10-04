#include <mbedtls/entropy.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/sha256.h>
#include <mbedtls/x509_crt.h>
#include <symbian_mbedtls/platform.h>
#include <symbian_mbedtls/socket_bio.h>

#include "test_certificate.h"

__attribute__((visibility("default"))) int MbedSha256(
    const unsigned char* input, unsigned size, unsigned char* output) {
  return mbedtls_sha256(input, size, output, 0);
}

__attribute__((visibility("default"))) int MbedUtcProbe(void) {
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

__attribute__((visibility("default"))) int MbedVerifyCert(void) {
  mbedtls_x509_crt certificate;
  mbedtls_x509_crt trust;
  mbedtls_x509_crt_init(&certificate);
  mbedtls_x509_crt_init(&trust);
  int result = mbedtls_x509_crt_parse(&certificate,
                                      (const unsigned char*)kTestCertificate,
                                      sizeof(kTestCertificate));
  if (result == 0) {
    result =
        mbedtls_x509_crt_parse(&trust, (const unsigned char*)kTestCertificate,
                               sizeof(kTestCertificate));
  }
  if (result == 0) {
    unsigned flags = 0;
    result = mbedtls_x509_crt_verify(&certificate, &trust, NULL, "sdk-test",
                                     &flags, NULL, NULL);
    if (result != 0 || flags != 0) {
      result = -136;
    }
  }
  if (result == 0) {
    unsigned flags = 0;
    result = mbedtls_x509_crt_verify(&certificate, &trust, NULL, "wrong-name",
                                     &flags, NULL, NULL);
    if (result == 0 || (flags & MBEDTLS_X509_BADCERT_CN_MISMATCH) == 0) {
      result = -137;
    } else {
      result = 0;
    }
  }
  if (result == 0) {
    mbedtls_x509_crt expired;
    mbedtls_x509_crt_init(&expired);
    result = mbedtls_x509_crt_parse(&expired,
                                    (const unsigned char*)kExpiredCertificate,
                                    sizeof(kExpiredCertificate));
    if (result == 0) {
      unsigned flags = 0;
      result = mbedtls_x509_crt_verify(&expired, &trust, NULL, "sdk-test",
                                       &flags, NULL, NULL);
      if (result == 0 || (flags & MBEDTLS_X509_BADCERT_EXPIRED) == 0) {
        result = -139;
      } else {
        result = 0;
      }
    }
    mbedtls_x509_crt_free(&expired);
  }
  mbedtls_x509_crt_free(&trust);
  mbedtls_x509_crt_free(&certificate);
  return result;
}

__attribute__((visibility("default"))) int MbedSocketCancelProbe(void) {
  symbian_mbedtls_socket_bio bio = {0, 0};
  unsigned char byte = 0;
  if (symbian_mbedtls_socket_bio_attach(&bio, -1) == 0) {
    return -140;
  }
  bio.fd = 0;
  symbian_mbedtls_socket_bio_cancel(&bio);
  if (symbian_mbedtls_socket_bio_send(&bio, &byte, 1) !=
          MBEDTLS_ERR_NET_CONN_RESET ||
      symbian_mbedtls_socket_bio_recv(&bio, &byte, 1) !=
          MBEDTLS_ERR_NET_CONN_RESET) {
    return -141;
  }
  return 0;
}

__attribute__((visibility("default"))) int MbedEntropyFailureProbe(void) {
  extern int mbedtls_hardware_poll(void*, unsigned char*, size_t, size_t*);
  unsigned char bytes[32] = {0};
  size_t count = 123;
  int result = mbedtls_hardware_poll(NULL, bytes, sizeof(bytes), &count);
#ifdef SYMBIAN_RM807_ENTROPY_PROBE
  if (result != 0 || count != sizeof(bytes)) return -143;
  size_t invalid_count = 123;
  if (mbedtls_hardware_poll(NULL, bytes, 0, &invalid_count) !=
          MBEDTLS_ERR_ENTROPY_SOURCE_FAILED ||
      invalid_count != 0) {
    return -146;
  }
  unsigned char next[32] = {0};
  size_t next_count = 0;
  result = mbedtls_hardware_poll(NULL, next, sizeof(next), &next_count);
  if (result != 0 || next_count != sizeof(next)) return -145;
  int any_nonzero = 0;
  int any_difference = 0;
  for (size_t i = 0; i < sizeof(bytes); ++i) {
    any_nonzero |= bytes[i] != 0;
    any_difference |= bytes[i] != next[i];
  }
  return any_nonzero && any_difference ? 0 : -147;
#else
  return result == MBEDTLS_ERR_ENTROPY_SOURCE_FAILED && count == 0 ? 0 : -143;
#endif
}
