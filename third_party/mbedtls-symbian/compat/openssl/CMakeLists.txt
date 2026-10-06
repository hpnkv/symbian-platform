set(openssl_source "${PROJECT_SOURCE_DIR}/third_party/openssl-symbian")
include(Sources.cmake)
include(configure.cmake)
# These modules are explicitly disabled by the native profile; their public
# headers deliberately reject compiling their implementation with NO_* set.
list(FILTER openssl_crypto_sources EXCLUDE REGEX "/src/crypto/(comp|dso)/")
# The original RAND_poll uses rand() without seeding OpenSSL's RNG. Guest builds
# require a real entropy provider; host verification uses original OS polling.
if(NOT SYMBIAN_MBEDTLS_HOST)
  list(REMOVE_ITEM openssl_crypto_sources "${openssl_source}/src/crypto/rand/rand_unix.c")
  list(APPEND openssl_crypto_sources "${CMAKE_CURRENT_SOURCE_DIR}/rand_poll.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/sdk_heap.c")
endif()
foreach(component crypto ssl)
  set(target openssl_compat_${component})
  if(SYMBIAN_MBEDTLS_HOST)
    add_library(${target} STATIC ${openssl_${component}_sources})
  else()
    symbian_add_static_library(${target} SOURCES ${openssl_${component}_sources})
    target_include_directories(${target} SYSTEM BEFORE PRIVATE
      "${SYMBIAN_SDK_PREFIX}/include/compiler"
      "${SYMBIAN_SDK_PREFIX}/include/openc")
    target_compile_definitions(${target} PUBLIC OPENSSL_SYS_SYMBIAN)
    target_compile_definitions(${target} PRIVATE
      malloc=symbian_tls_malloc calloc=symbian_tls_calloc
      realloc=symbian_tls_realloc free=symbian_tls_free)
    target_include_directories(${target} PRIVATE "${PROJECT_SOURCE_DIR}/include")
    # OpenC declarations require this platform ABI macro header before C libc.
    # This does not link an OpenC/PIPS DLL or include C++ SDK placement-new APIs.
    target_compile_options(${target} PRIVATE -include e32def.h)
    target_include_directories(${target} BEFORE PRIVATE
      "${PROJECT_SOURCE_DIR}/compat/sdk-headers")
  endif()
  target_include_directories(${target} PUBLIC
    "$<BUILD_INTERFACE:${openssl_include}>" "$<INSTALL_INTERFACE:include>"
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
    PRIVATE "${openssl_include}/openssl" "${openssl_source}/src/crypto"
      "${openssl_source}/inc/include/openssl" "${openssl_source}/src/ssl")
  target_compile_definitions(${target} PUBLIC SSL2_STATIC OPENSSL_NO_ASM
    OPENSSL_NO_SCTP OPENSSL_NO_SSL2 OPENSSL_NO_SSL3
    OPENSSL_NO_COMP OPENSSL_NO_DSO OPENSSL_NO_DYNAMIC_ENGINE NO_SYSLOG)
  target_compile_options(${target} PRIVATE -g -gdwarf-4 $<$<CONFIG:Debug>:-O0>)
  set_property(SOURCE ${openssl_${component}_sources} APPEND PROPERTY COMPILE_OPTIONS
    $<$<CONFIG:Debug>:-fno-function-sections>
    $<$<CONFIG:Debug>:-fno-data-sections>)
  if(component STREQUAL "crypto")
    add_library(OpenSSLCompat::Crypto ALIAS ${target})
    set_target_properties(${target} PROPERTIES EXPORT_NAME Crypto)
  else()
    add_library(OpenSSLCompat::SSL ALIAS ${target})
    set_target_properties(${target} PROPERTIES EXPORT_NAME SSL)
  endif()
  install(TARGETS ${target} EXPORT OpenSSLCompatTargets ARCHIVE DESTINATION lib)
endforeach()
if(NOT SYMBIAN_MBEDTLS_HOST)
  target_link_libraries(openssl_compat_crypto PUBLIC mbedcrypto)
else()
  target_link_libraries(openssl_compat_crypto PUBLIC absl::nullability)
endif()
target_link_libraries(openssl_compat_ssl PUBLIC openssl_compat_crypto)
symbian_header_canary(symbian_openssl_c_header_canary C
  HEADERS "${PROJECT_SOURCE_DIR}/include/symbian_tls/heap.h"
  LIBRARIES openssl_compat_crypto)
symbian_header_canary(symbian_openssl_cpp_header_canary
  HEADERS "${PROJECT_SOURCE_DIR}/include/symbian_tls/heap.h"
  LIBRARIES openssl_compat_crypto)
install(DIRECTORY "${openssl_include}/openssl" DESTINATION include)
install(EXPORT OpenSSLCompatTargets NAMESPACE OpenSSLCompat:: DESTINATION lib/cmake/OpenSSLCompat)
configure_package_config_file("${CMAKE_CURRENT_SOURCE_DIR}/OpenSSLCompatConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/OpenSSLCompatConfig.cmake"
  INSTALL_DESTINATION lib/cmake/OpenSSLCompat)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/OpenSSLCompatConfig.cmake"
  DESTINATION lib/cmake/OpenSSLCompat)
install(FILES "${openssl_source}/LICENSE" "${PROJECT_SOURCE_DIR}/docs/OPENSSL_PROVENANCE.md"
  DESTINATION share/licenses/symbian-tls-openssl-compat)
