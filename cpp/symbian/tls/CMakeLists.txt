cmake_minimum_required(VERSION 3.28)
project(symbian_guest_tls LANGUAGES CXX)

if(NOT SYMBIAN_SDK_PREFIX)
  message(FATAL_ERROR "Guest TLS build requires SYMBIAN_SDK_PREFIX")
endif()
include("${SYMBIAN_SDK_PREFIX}/cmake/SymbianApp.cmake")
find_package(MbedTLS 3.4.1 EXACT CONFIG REQUIRED)
if(NOT TARGET Symbian::Connectivity)
  message(FATAL_ERROR "Guest TLS requires Symbian::Connectivity")
endif()

add_library(symbian_api_tls STATIC tls_server.cc)
target_compile_features(symbian_api_tls PUBLIC cxx_std_20)
target_compile_options(symbian_api_tls PRIVATE -fno-pic -fno-exceptions)
target_include_directories(symbian_api_tls PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../api/include")
target_link_libraries(symbian_api_tls PUBLIC Symbian::Connectivity
  MbedTLS::mbedtls MbedTLS::mbedx509 MbedTLS::mbedcrypto)
