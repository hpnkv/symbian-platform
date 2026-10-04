cmake_minimum_required(VERSION 3.28)
project(agent_service LANGUAGES C CXX ASM)

# The ordinary `symbian app run` path selects an SDK compiler without setting
# the preset's optional SDK prefix environment variable.
if(NOT SYMBIAN_SDK_PREFIX AND CMAKE_CXX_COMPILER)
  get_filename_component(sdk_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
  get_filename_component(SYMBIAN_SDK_PREFIX "${sdk_bin}/.." ABSOLUTE)
endif()
if(NOT EXISTS "${SYMBIAN_SDK_PREFIX}/cmake/SymbianApp.cmake")
  message(FATAL_ERROR "Select an exported Symbian SDK")
endif()
include(SymbianPic)
include("${SYMBIAN_SDK_PREFIX}/cmake/SymbianApp.cmake")
foreach(required_target Symbian::Connectivity Symbian::Agent Symbian::Tls
    Symbian::Stackless Symbian::System Symbian::Display)
  if(NOT TARGET ${required_target})
    message(FATAL_ERROR "Selected SDK lacks ${required_target}")
  endif()
endforeach()

# This service remains validated only in the pinned RM-807 emulator profile.
# Its public TLS fixture is suitable only for loopback development sessions.
if(NOT SYMBIAN_EMULATOR_RM807_AGENT AND NOT SYMBIAN_INDEX_GUEST_PROBES)
  message(FATAL_ERROR "Set SYMBIAN_EMULATOR_RM807_AGENT=ON for the emulator")
endif()
set(SYMBIAN_AGENT_TEST_CERTIFICATE_DIR
  "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/mbedtls-symbian/tests/fixtures"
  CACHE PATH "Public, emulator-only TLS fixture")
set(fixture_dir "${SYMBIAN_AGENT_TEST_CERTIFICATE_DIR}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/agent_certificates.h" "")
foreach(part server-cert server-key)
  file(READ "${fixture_dir}/${part}.pem" pem)
  string(REPLACE "\n" "\\n\"\n\"" quoted_pem "${pem}")
  string(REPLACE "-" "_" identifier "${part}")
  file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/agent_certificates.h"
    "static const char k_${identifier}[] = \"${quoted_pem}\";\n")
endforeach()

symbian_add_import_executable(agent_service
  STARTUP startup.S
  LINKER_SCRIPT image.ld
  SOURCES startup.cc scheduler_bridge.cc agent_service.cc local_ui.cc
    sdk_entropy_rm807.cc sdk_entropy_rm807.S)
target_include_directories(agent_service PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_definitions(agent_service PRIVATE SYMBIAN_RM807_ENTROPY_PROBE=1
  MBEDTLS_STATIC MBEDTLS_SSL_PROTO_TLS1_3 SYMBIAN_MBEDTLS_GUEST=1
  MBEDTLS_USER_CONFIG_FILE="symbian_mbedtls/config.h")
target_link_libraries(agent_service PRIVATE Symbian::Connectivity Symbian::Agent
  Symbian::Tls Symbian::Stackless Symbian::System Symbian::Display)
target_link_options(agent_service PRIVATE --gc-sections)
