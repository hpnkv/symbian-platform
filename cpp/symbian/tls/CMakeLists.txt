cmake_minimum_required(VERSION 3.28)
project(symbian_guest_tls LANGUAGES CXX)

if(NOT SYMBIAN_SDK_PREFIX)
  message(FATAL_ERROR "Guest TLS build requires SYMBIAN_SDK_PREFIX")
endif()
if(NOT TARGET Symbian::Runtime)
  include("${SYMBIAN_SDK_PREFIX}/cmake/SymbianApp.cmake")
endif()
find_package(MbedTLS 3.4.1 EXACT CONFIG REQUIRED)
# The StatusOr API requires the streams libc++ profile. The Mbed TLS C
# archives support either profile; select the same one for this closure.
# Linking both complete runtime archives duplicates std::to_string symbols.
foreach(tls_target MbedTLS::mbedcrypto MbedTLS::mbedx509 MbedTLS::mbedtls)
  get_target_property(tls_links ${tls_target} INTERFACE_LINK_LIBRARIES)
  list(TRANSFORM tls_links REPLACE "^SymbianRuntime$" "SymbianStreams")
  set_property(TARGET ${tls_target} PROPERTY INTERFACE_LINK_LIBRARIES "${tls_links}")
endforeach()
if(NOT TARGET Symbian::Connectivity)
  message(FATAL_ERROR "Guest TLS requires Symbian::Connectivity")
endif()

add_library(symbian_api_tls STATIC tls_server.cc)
target_compile_features(symbian_api_tls PUBLIC cxx_std_20)
target_compile_options(symbian_api_tls PRIVATE -fno-pic -fno-exceptions)
target_include_directories(symbian_api_tls PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../api/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/../..")
if(TARGET symbian_api_connectivity)
  set(tls_connectivity symbian_api_connectivity)
else()
  set(tls_connectivity Symbian::Connectivity)
endif()
target_link_libraries(symbian_api_tls PUBLIC ${tls_connectivity}
  MbedTLS::mbedtls MbedTLS::mbedx509 MbedTLS::mbedcrypto)
