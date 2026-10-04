cmake_minimum_required(VERSION 3.28)
project(mbedtls_dll_probe LANGUAGES C ASM)

if(NOT MBEDTLS_SOURCE OR NOT EXISTS "${MBEDTLS_SOURCE}/include/mbedtls/sha256.h")
  message(FATAL_ERROR "Set MBEDTLS_SOURCE to the prepared Mbed TLS adaptation")
endif()
if(NOT MBEDTLS_ARCHIVE OR NOT EXISTS "${MBEDTLS_ARCHIVE}")
  message(FATAL_ERROR "Set MBEDTLS_ARCHIVE to its ARM libmbedcrypto.a")
endif()

include(SymbianApp)
symbian_add_dynamic_library(mbedcrypto_probe
  SOURCES bridge.c
  EXPORT_DEFINITION exports.def UID3 0xE0000D18
  IMPORT_SYMBOLS MbedSha256
  IMPORT_PROXIES
    "${SYMBIAN_SDK_PREFIX}/proxies/euser/euser.dso"
    "${SYMBIAN_SDK_PREFIX}/proxies/libc/libc.dso"
    "${SYMBIAN_SDK_PREFIX}/proxies/libpthread/libpthread.dso")
target_include_directories(mbedcrypto_probe_elf PRIVATE "${MBEDTLS_SOURCE}/include")
target_link_libraries(mbedcrypto_probe_elf PRIVATE "${MBEDTLS_ARCHIVE}")
# The runtime archive contains optional mimalloc POSIX helpers that Belle does
# not import. Retain the declared DLL export while discarding unused helpers.
target_link_options(mbedcrypto_probe_elf PRIVATE
  --gc-sections --undefined=MbedSha256)
