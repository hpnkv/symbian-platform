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
foreach(required_target Symbian::Connectivity Symbian::Agent Symbian::Crypto
    Symbian::Stackless Symbian::System Symbian::Display)
  if(NOT TARGET ${required_target})
    message(FATAL_ERROR "Selected SDK lacks ${required_target}")
  endif()
endforeach()

# The RM-807 entropy adapter has emulator evidence only; a phone-specific
# build remains an explicitly unverified physical profile.
set(SYMBIAN_AGENT_PRIVATE_KEY_FILE "" CACHE FILEPATH
  "32-byte private agent key; required for a Wi-Fi listener")
if(NOT SYMBIAN_AGENT_PRIVATE_KEY_FILE AND
    NOT SYMBIAN_EMULATOR_RM807_AGENT AND NOT SYMBIAN_INDEX_GUEST_PROBES)
  message(FATAL_ERROR "Public-key builds require the RM-807 emulator profile")
endif()
if(SYMBIAN_AGENT_PRIVATE_KEY_FILE)
  file(SIZE "${SYMBIAN_AGENT_PRIVATE_KEY_FILE}" key_size)
  if(NOT key_size EQUAL 32)
    message(FATAL_ERROR "Agent key must contain exactly 32 bytes")
  endif()
  file(READ "${SYMBIAN_AGENT_PRIVATE_KEY_FILE}" agent_key_hex HEX)
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${SYMBIAN_AGENT_PRIVATE_KEY_FILE}")
  file(SHA256 "${SYMBIAN_AGENT_PRIVATE_KEY_FILE}" actual_key_sha256)
  if(SYMBIAN_AGENT_KEY_SHA256 AND
      NOT actual_key_sha256 STREQUAL SYMBIAN_AGENT_KEY_SHA256)
    message(FATAL_ERROR "Agent key changed during this build")
  endif()
  if(agent_key_hex STREQUAL
      "e388b273d3df70fdfddafce1fb3f1681d8116baee784ebbf542855501f7d916a")
    message(FATAL_ERROR "The public emulator key cannot be used on Wi-Fi")
  endif()
  string(LENGTH "${SYMBIAN_AGENT_PAIRING_CODE}" pairing_code_length)
  if(NOT pairing_code_length EQUAL 8 OR
      NOT SYMBIAN_AGENT_PAIRING_CODE MATCHES "^[ABCEGIKNOPRSTU]+$")
    message(FATAL_ERROR "Set the 8-character agent pairing code")
  endif()
  set(agent_private_profile 1)
  set(agent_panel_state "PAIR ${SYMBIAN_AGENT_PAIRING_CODE}")
else()
  # Public emulator fixture. Never listen outside loopback with this key.
  set(agent_key_hex
    "e388b273d3df70fdfddafce1fb3f1681d8116baee784ebbf542855501f7d916a")
  set(agent_private_profile 0)
  set(agent_panel_state "RUNNING")
endif()
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/agent_key.h"
  "#pragma once\n#define SYMBIAN_AGENT_KEY_HEX \"${agent_key_hex}\"\n#define SYMBIAN_AGENT_PRIVATE_PROFILE ${agent_private_profile}\n#define SYMBIAN_AGENT_PANEL_STATE \"${agent_panel_state}\"\n")

symbian_add_import_executable(agent_service
  STARTUP startup.S
  LINKER_SCRIPT image.ld
  SOURCES startup.cc agent_service.cc
    sdk_entropy_rm807.cc sdk_entropy_rm807.S)
target_include_directories(agent_service PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_definitions(agent_service PRIVATE SYMBIAN_RM807_ENTROPY_PROBE=1
  MBEDTLS_STATIC SYMBIAN_MBEDTLS_GUEST=1
  MBEDTLS_USER_CONFIG_FILE="symbian_mbedtls/config.h")
target_link_libraries(agent_service PRIVATE Symbian::Connectivity Symbian::Agent
  Symbian::Crypto Symbian::Stackless Symbian::System Symbian::Display)
target_link_options(agent_service PRIVATE --gc-sections)
