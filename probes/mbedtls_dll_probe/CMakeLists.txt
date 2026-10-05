cmake_minimum_required(VERSION 3.28)
project(mbedtls_dll_probe LANGUAGES C CXX ASM)

if(NOT MBEDTLS_SOURCE OR NOT EXISTS "${MBEDTLS_SOURCE}/include/mbedtls/sha256.h")
  message(FATAL_ERROR "Set MBEDTLS_SOURCE to the prepared Mbed TLS adaptation")
endif()
if(NOT MBEDTLS_ARCHIVE OR NOT EXISTS "${MBEDTLS_ARCHIVE}")
  message(FATAL_ERROR "Set MBEDTLS_ARCHIVE to its ARM libmbedcrypto.a")
endif()
get_filename_component(mbedtls_archive_dir "${MBEDTLS_ARCHIVE}" DIRECTORY)
set(mbedx509_archive "${mbedtls_archive_dir}/libmbedx509.a")
set(mbedtls_archive "${mbedtls_archive_dir}/libmbedtls.a")
if(NOT EXISTS "${mbedx509_archive}")
  message(FATAL_ERROR "The matching ARM libmbedx509.a is required")
endif()
if(NOT EXISTS "${mbedtls_archive}")
  message(FATAL_ERROR "The matching ARM libmbedtls.a is required")
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
file(READ "${MBEDTLS_SOURCE}/tests/fixtures/server-key.pem" test_private_key)
string(REPLACE "\n" "\\n\"\n\"" test_private_key_c "${test_private_key}")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/test_certificate.h"
  "static const char kTestPrivateKey[] = \"${test_private_key_c}\";\n")

include(SymbianApp)
option(SYMBIAN_RM807_ENTROPY_PROBE
  "Use the research-only RM-807 executive RNG adapter" OFF)
option(SYMBIAN_RM807_SOCKET_PROBE
  "Include the research-only connected receive/cancel probe" OFF)
option(SYMBIAN_RM807_TLS_HANDSHAKE_PROBE
  "Include the research-only authenticated native TCP TLS probe" OFF)
set(probe_sources bridge.c)
if(SYMBIAN_RM807_ENTROPY_PROBE OR SYMBIAN_RM807_TLS_HANDSHAKE_PROBE)
  list(APPEND probe_sources sdk_entropy_rm807.cc sdk_entropy_rm807.S)
endif()
if(SYMBIAN_RM807_SOCKET_PROBE)
  list(APPEND probe_sources socket_probe.c)
endif()
if(SYMBIAN_RM807_TLS_HANDSHAKE_PROBE)
  list(APPEND probe_sources tls_handshake.cc tls_server.cc tls_owner_probe.cc)
endif()
symbian_add_dynamic_library(mbedcrypto_probe
  SOURCES ${probe_sources} UID3 0xE0000D18)
target_link_libraries(mbedcrypto_probe PRIVATE
  Symbian::C Symbian::Pthread)
target_include_directories(mbedcrypto_probe PRIVATE
  "${MBEDTLS_SOURCE}/include" "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_definitions(mbedcrypto_probe PRIVATE
  MBEDTLS_STATIC MBEDTLS_SSL_PROTO_TLS1_3 SYMBIAN_MBEDTLS_GUEST=1
  MBEDTLS_USER_CONFIG_FILE="symbian_mbedtls/config.h")
if(SYMBIAN_RM807_ENTROPY_PROBE OR SYMBIAN_RM807_TLS_HANDSHAKE_PROBE)
  target_compile_definitions(mbedcrypto_probe PRIVATE
    SYMBIAN_RM807_ENTROPY_PROBE=1)
endif()
target_link_libraries(mbedcrypto_probe PRIVATE
  "${mbedtls_archive}" "${mbedx509_archive}" "${MBEDTLS_ARCHIVE}")
if(SYMBIAN_RM807_TLS_HANDSHAKE_PROBE)
  if(NOT TARGET Symbian::Agent OR NOT TARGET Symbian::Tls)
    message(FATAL_ERROR "TLS control probe needs installed Agent and Tls targets")
  endif()
  target_link_libraries(mbedcrypto_probe PRIVATE
    Symbian::Connectivity Symbian::Agent Symbian::Tls
    Symbian::SocketServer Symbian::Internet Symbian::Math Symbian::CxxAbi)
endif()
