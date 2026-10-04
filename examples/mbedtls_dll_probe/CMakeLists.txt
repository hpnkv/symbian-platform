cmake_minimum_required(VERSION 3.28)
project(mbedtls_dll_probe LANGUAGES C ASM)

if(NOT MBEDTLS_SOURCE OR NOT EXISTS "${MBEDTLS_SOURCE}/include/mbedtls/sha256.h")
  message(FATAL_ERROR "Set MBEDTLS_SOURCE to the prepared Mbed TLS adaptation")
endif()
if(NOT MBEDTLS_ARCHIVE OR NOT EXISTS "${MBEDTLS_ARCHIVE}")
  message(FATAL_ERROR "Set MBEDTLS_ARCHIVE to its ARM libmbedcrypto.a")
endif()
get_filename_component(mbedtls_archive_dir "${MBEDTLS_ARCHIVE}" DIRECTORY)
set(mbedx509_archive "${mbedtls_archive_dir}/libmbedx509.a")
if(NOT EXISTS "${mbedx509_archive}")
  message(FATAL_ERROR "The matching ARM libmbedx509.a is required")
endif()
file(READ "${MBEDTLS_SOURCE}/tests/fixtures/server-cert.pem" test_certificate)
string(REPLACE "\n" "\\n\"\n\"" test_certificate_c "${test_certificate}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/test_certificate.h"
  "static const char kTestCertificate[] = \"${test_certificate_c}\";\n")
file(READ "${MBEDTLS_SOURCE}/tests/fixtures/expired-cert.pem" expired_certificate)
string(REPLACE "\n" "\\n\"\n\"" expired_certificate_c
  "${expired_certificate}")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/test_certificate.h"
  "static const char kExpiredCertificate[] = \"${expired_certificate_c}\";\n")

include(SymbianApp)
symbian_add_dynamic_library(mbedcrypto_probe
  SOURCES bridge.c
  EXPORT_DEFINITION exports.def UID3 0xE0000D18
  IMPORT_SYMBOLS MbedSha256 MbedUtcProbe MbedVerifyCert
  IMPORT_PROXIES
    "${SYMBIAN_SDK_PREFIX}/proxies/euser/euser.dso"
    "${SYMBIAN_SDK_PREFIX}/proxies/libc/libc.dso"
    "${SYMBIAN_SDK_PREFIX}/proxies/libpthread/libpthread.dso")
target_include_directories(mbedcrypto_probe_elf PRIVATE
  "${MBEDTLS_SOURCE}/include" "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(mbedcrypto_probe_elf PRIVATE
  "${mbedx509_archive}" "${MBEDTLS_ARCHIVE}")
# The runtime archive contains optional mimalloc POSIX helpers that Belle does
# not import. Retain the declared DLL export while discarding unused helpers.
target_link_options(mbedcrypto_probe_elf PRIVATE
  --gc-sections --undefined=MbedSha256 --undefined=MbedUtcProbe
  --undefined=MbedVerifyCert)
