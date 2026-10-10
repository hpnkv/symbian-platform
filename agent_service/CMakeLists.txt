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
if(NOT TARGET Symbian::Runtime)
  include("${SYMBIAN_SDK_PREFIX}/cmake/SymbianApp.cmake")
endif()
foreach(required_target Symbian::Connectivity Symbian::WebSocket Symbian::Agent Symbian::Crypto
    Symbian::Stackless Symbian::System Symbian::Display Symbian::Storage
    Symbian::FailureHandler)
  if(NOT TARGET ${required_target})
    message(FATAL_ERROR "Selected SDK lacks ${required_target}")
  endif()
endforeach()

# A physical installer contains no device secret. The agent generates one in
# its own private storage on first launch; USB pairing will export proof of it.
if(SYMBIAN_EMULATOR_RM807_AGENT OR SYMBIAN_INDEX_GUEST_PROBES)
  # Public emulator fixture. Never listen outside loopback with this key.
  set(agent_key_hex
    "e388b273d3df70fdfddafce1fb3f1681d8116baee784ebbf542855501f7d916a")
  set(agent_private_profile 0)
  set(agent_emulator_profile 1)
  set(agent_panel_state "RUNNING")
else()
  set(agent_key_hex "")
  set(agent_private_profile 1)
  set(agent_emulator_profile 0)
  set(agent_panel_state "PAIR ON USB")
endif()
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/agent_key.h"
  "#pragma once\n#define SYMBIAN_AGENT_KEY_HEX \"${agent_key_hex}\"\n#define SYMBIAN_AGENT_PRIVATE_PROFILE ${agent_private_profile}\n#define SYMBIAN_AGENT_PANEL_STATE \"${agent_panel_state}\"\n")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/agent_key.h"
  "#define SYMBIAN_AGENT_EMULATOR_PROFILE ${agent_emulator_profile}\n#define SYMBIAN_AGENT_WIFI_PAIRED 0\n")

symbian_add_import_executable(agent_service
  STARTUP startup.S
  LINKER_SCRIPT image.ld
  SOURCES startup.cc agent_service.cc agent_identity.cc resident_panel.cc
    native_resident_panel.cc window_task_identity.cc)
target_include_directories(agent_service PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
# Custom startup uses the same independent code/data relocation contract as
# ordinary SDK application targets.
target_compile_options(agent_service PRIVATE "SHELL:-fno-pic")
target_compile_definitions(agent_service PRIVATE MBEDTLS_STATIC SYMBIAN_MBEDTLS_GUEST=1
  MBEDTLS_USER_CONFIG_FILE="symbian_mbedtls/config.h")
target_link_libraries(agent_service PRIVATE Symbian::Connectivity Symbian::WebSocket Symbian::Agent
  Symbian::Crypto Symbian::Stackless Symbian::System Symbian::Display
  Symbian::Storage Symbian::FailureHandler)
target_link_options(agent_service PRIVATE --gc-sections)
