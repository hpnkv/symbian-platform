# Startup/linker files are the current SDK starter adapters, recorded in docs.
set(SYMBIAN_IMPORT_PROXIES "${SYMBIAN_SDK_PREFIX}/proxies/euser/euser.dso")
symbian_add_import_executable(crypto_probe STARTUP startup.S
  LINKER_SCRIPT image.ld SOURCES startup.cc main.cc ../../tests/vectors.c)
target_link_libraries(crypto_probe PRIVATE MbedTLS::mbedcrypto)
target_include_directories(crypto_probe PRIVATE "${PROJECT_SOURCE_DIR}/tests")
target_include_directories(crypto_probe SYSTEM BEFORE PRIVATE
  "${SYMBIAN_SDK_PREFIX}/include/compiler"
  "${SYMBIAN_SDK_PREFIX}/include/openc")
target_compile_options(crypto_probe PRIVATE -g -gdwarf-4)
target_link_options(crypto_probe PRIVATE --gc-sections)
set_target_properties(crypto_probe PROPERTIES LINKER_LANGUAGE CXX)
option(SYMBIAN_MBEDTLS_CHANGED_VECTOR "Negative AES known-answer control" OFF)
if(SYMBIAN_MBEDTLS_CHANGED_VECTOR)
  target_compile_definitions(crypto_probe PRIVATE SYMBIAN_MBEDTLS_CHANGED_VECTOR)
endif()
