find_package(GTest CONFIG REQUIRED)
add_executable(symbian_e32_oracle e32_oracle_test.cc)
target_compile_features(symbian_e32_oracle PRIVATE cxx_std_20)
target_compile_options(symbian_e32_oracle PRIVATE -fno-exceptions)
target_link_libraries(symbian_e32_oracle PRIVATE
                      epocloader common GTest::gtest_main)
set(SYMBIAN_E32_TEST_IMAGE
    "${SYMBIAN_PLATFORM_ROOT}/.symbian/e32-probe/e32_probe.exe" CACHE FILEPATH
    "Experimental executable to check with the independent EKA2L1 parser")
enable_testing()
add_test(NAME symbian_e32_oracle COMMAND symbian_e32_oracle)
set_tests_properties(symbian_e32_oracle PROPERTIES
  ENVIRONMENT "SYMBIAN_E32_TEST_IMAGE=${SYMBIAN_E32_TEST_IMAGE}" TIMEOUT 15)

# The checksum implementation stays in the pinned upstream checkout with its
# original EPL license. Keep this oracle separate from the GPL emulator binary.
set(SYMBIAN_CHECKSUM_SOURCE_DIR
    "${SYMBIAN_PLATFORM_ROOT}/research/upstream/buildtools/toolsandutils/e32tools/elf2e32/source"
    CACHE PATH "Pinned Nokia elf2e32 checksum source")
add_library(symbian_historical_checksum STATIC
            "${SYMBIAN_CHECKSUM_SOURCE_DIR}/checksum.cpp")
target_compile_options(symbian_historical_checksum PRIVATE -fno-exceptions
  -include "${CMAKE_CURRENT_SOURCE_DIR}/checksum_host_types.h")
target_include_directories(symbian_historical_checksum PUBLIC
                           "${SYMBIAN_CHECKSUM_SOURCE_DIR}")
add_executable(symbian_checksum_oracle checksum_oracle_test.cc)
target_compile_features(symbian_checksum_oracle PRIVATE cxx_std_20)
target_compile_options(symbian_checksum_oracle PRIVATE -fno-exceptions)
target_include_directories(symbian_checksum_oracle PRIVATE
                           "${SYMBIAN_PLATFORM_ROOT}/cpp")
target_link_libraries(symbian_checksum_oracle PRIVATE
                      symbian_historical_checksum GTest::gtest_main)
add_test(NAME symbian_checksum_oracle COMMAND symbian_checksum_oracle)
set_tests_properties(symbian_checksum_oracle PROPERTIES
  ENVIRONMENT "SYMBIAN_E32_TEST_IMAGE=${SYMBIAN_E32_TEST_IMAGE}" TIMEOUT 15)
