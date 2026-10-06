find_package(GTest CONFIG REQUIRED)
include(GoogleTest)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/server-cert.pem" test_certificate)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/server-key.pem" test_key)
configure_file(fixtures.h.in fixtures.h @ONLY)
add_executable(crypto_test crypto_test.cc tls_test.cc socket_bio_test.cc vectors.c
  ../library/sdk_heap.c ../library/sdk_time.c)
target_link_libraries(crypto_test PRIVATE MbedTLS::mbedtls GTest::gtest_main)
target_include_directories(crypto_test PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_options(crypto_test PRIVATE -fno-exceptions)
gtest_discover_tests(crypto_test)
if(SYMBIAN_MBEDTLS_OPENSSL_COMPAT)
  # Exercise the guest adapter on host with a controlled SDK entropy service.
  # The compatibility archive supplies the actual secret erasure routine.
  add_executable(rand_poll_test rand_poll_test.cc ../compat/openssl/rand_poll.c)
  target_compile_definitions(rand_poll_test PRIVATE MBEDTLS_ENTROPY_HARDWARE_ALT)
  target_link_libraries(rand_poll_test PRIVATE mbedcrypto
    openssl_compat_crypto GTest::gtest_main)
  target_compile_options(rand_poll_test PRIVATE -fno-exceptions)
  gtest_discover_tests(rand_poll_test)
endif()
