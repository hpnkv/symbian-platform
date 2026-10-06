# Copyright The Mbed TLS Contributors. SPDX-License-Identifier: Apache-2.0
# Adapted for static SDK consumption; module inventory is in cmake/Sources.cmake.
include("${PROJECT_SOURCE_DIR}/cmake/Sources.cmake")
foreach(component crypto x509 tls)
  if(component STREQUAL "crypto")
    set(target mbedcrypto)
  elseif(component STREQUAL "x509")
    set(target mbedx509)
  else()
    set(target mbedtls)
  endif()
  if(SYMBIAN_MBEDTLS_HOST)
    add_library(${target} STATIC ${src_${component}})
    target_compile_options(${target} PRIVATE -ffunction-sections -fdata-sections)
  else()
    symbian_add_static_library(${target} SOURCES ${src_${component}})
    # C translation units need C headers, rather than libc++'s C++ wrappers.
    target_include_directories(${target} SYSTEM BEFORE PRIVATE
      "$<$<COMPILE_LANGUAGE:C>:${SYMBIAN_SDK_PREFIX}/include/compiler>"
      "$<$<COMPILE_LANGUAGE:C>:${SYMBIAN_SDK_PREFIX}/include/openc>")
  endif()
  # Debug keeps each reached module's functions together. This preserves
  # source variables and avoids LLD's overlapping Thumb tombstone ranges for
  # discarded individual functions. Release retains SDK section collection.
  target_compile_options(${target} PRIVATE -g -gdwarf-4 $<$<CONFIG:Debug>:-O0>)
  # Source options follow the SDK's transitive section flags in CMake's command.
  set_property(SOURCE ${src_${component}} APPEND PROPERTY COMPILE_OPTIONS
    $<$<CONFIG:Debug>:-fno-function-sections>
    $<$<CONFIG:Debug>:-fno-data-sections>)
  add_library(MbedTLS::${target} ALIAS ${target})
  target_include_directories(${target} PUBLIC
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>" "$<INSTALL_INTERFACE:include>")
  target_compile_definitions(${target} PUBLIC MBEDTLS_STATIC
    MBEDTLS_USER_CONFIG_FILE="symbian_mbedtls/config.h")
  if(SYMBIAN_MBEDTLS_TLS13)
    target_compile_definitions(${target} PUBLIC MBEDTLS_SSL_PROTO_TLS1_3)
  endif()
  if(NOT SYMBIAN_MBEDTLS_HOST)
    target_compile_definitions(${target} PUBLIC SYMBIAN_MBEDTLS_GUEST=1)
  endif()
  install(TARGETS ${target} EXPORT MbedTLSTargets ARCHIVE DESTINATION lib)
endforeach()
if(SYMBIAN_MBEDTLS_HOST)
  target_link_libraries(mbedcrypto PUBLIC absl::nullability)
else()
  # Keep the concrete name so TLS profile selection can replace Runtime with
  # Streams consistently across all imported port targets.
  target_link_libraries(mbedcrypto PUBLIC SymbianRuntime)
endif()
target_sources(mbedcrypto PRIVATE platform_zeroize.c)
target_sources(mbedtls PRIVATE sdk_socket_bio.c)
if(NOT SYMBIAN_MBEDTLS_HOST)
  # Installed source exports carry the same owned provider beside the port.
  set(entropy_source "${PROJECT_SOURCE_DIR}/sdk_entropy")
  if(NOT EXISTS "${entropy_source}/sdk_entropy.cc")
    get_filename_component(entropy_source
      "${PROJECT_SOURCE_DIR}/../../cpp/symbian/entropy" ABSOLUTE)
  endif()
  target_sources(mbedcrypto PRIVATE sdk_heap.c sdk_time.c
    "${entropy_source}/sdk_entropy.cc")
endif()
target_link_libraries(mbedx509 PUBLIC mbedcrypto)
target_link_libraries(mbedtls PUBLIC mbedx509)
