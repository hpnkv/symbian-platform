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

set(SYMBIAN_KERNEL_SOURCE_DIR
    "${SYMBIAN_PLATFORM_ROOT}/research/upstream/kernelhwsrv" CACHE PATH
    "Pinned Nokia kernel and loader headers")
add_executable(symbian_validator_oracle validator_oracle_test.cc)
target_compile_features(symbian_validator_oracle PRIVATE cxx_std_20)
target_compile_options(symbian_validator_oracle PRIVATE
                       -fno-exceptions -Wno-invalid-offsetof)
target_include_directories(symbian_validator_oracle PRIVATE
    "${SYMBIAN_KERNEL_SOURCE_DIR}/kernel/eka/include"
    "${SYMBIAN_KERNEL_SOURCE_DIR}/userlibandfileserver/fileserver/inc")
target_link_libraries(symbian_validator_oracle PRIVATE
                      symbian_historical_checksum GTest::gtest_main)
add_test(NAME symbian_validator_oracle COMMAND symbian_validator_oracle)
set_tests_properties(symbian_validator_oracle PROPERTIES
  ENVIRONMENT "SYMBIAN_E32_TEST_IMAGE=${SYMBIAN_E32_TEST_IMAGE}" TIMEOUT 15)

add_executable(symbian_cpu_probe cpu_probe_test.cc)
target_compile_features(symbian_cpu_probe PRIVATE cxx_std_20)
target_compile_options(symbian_cpu_probe PRIVATE -fno-exceptions)
target_link_libraries(symbian_cpu_probe PRIVATE
                      cpu epocloader common GTest::gtest_main)
add_test(NAME symbian_cpu_probe COMMAND symbian_cpu_probe)
set_tests_properties(symbian_cpu_probe PROPERTIES
  ENVIRONMENT "SYMBIAN_E32_TEST_IMAGE=${SYMBIAN_E32_TEST_IMAGE}" TIMEOUT 15)

add_executable(symbian_process_probe process_probe_test.cc
    "${SYMBIAN_PLATFORM_ROOT}/research/upstream/EKA2L1/src/tests/platform.cpp")
target_compile_features(symbian_process_probe PRIVATE cxx_std_20)
target_compile_options(symbian_process_probe PRIVATE -fno-exceptions)
target_link_libraries(symbian_process_probe PRIVATE
                      epoc epockern epocmem epoctiming epocio GTest::gtest_main)
add_test(NAME symbian_process_probe COMMAND symbian_process_probe)
set_tests_properties(symbian_process_probe PROPERTIES
  ENVIRONMENT "SYMBIAN_E32_TEST_IMAGE=${SYMBIAN_E32_TEST_IMAGE}" TIMEOUT 15)

add_executable(symbian_package_probe package_probe_test.cc
    "${SYMBIAN_PLATFORM_ROOT}/research/upstream/EKA2L1/src/tests/platform.cpp")
target_compile_features(symbian_package_probe PRIVATE cxx_std_20)
target_compile_options(symbian_package_probe PRIVATE -fno-exceptions)
target_link_libraries(symbian_package_probe PRIVATE
                      epoc epockern epocmem epoctiming epocio epocpkg GTest::gtest_main)
set(SYMBIAN_SIS_TEST_PACKAGE
    "${SYMBIAN_PLATFORM_ROOT}/.symbian/package/probe.sis" CACHE FILEPATH
    "Maintained canonical SISX experiment, prevalidated by the native core")
add_test(NAME symbian_package_probe COMMAND symbian_package_probe)
set_tests_properties(symbian_package_probe PROPERTIES
  ENVIRONMENT "SYMBIAN_E32_TEST_IMAGE=${SYMBIAN_E32_TEST_IMAGE};SYMBIAN_SIS_TEST_PACKAGE=${SYMBIAN_SIS_TEST_PACKAGE}"
  TIMEOUT 15)

# Separate historical EPL oracle; no emulator or production format code linked.
add_executable(symbian_sis_checksum_oracle sis_checksum_oracle_test.cc)
target_compile_features(symbian_sis_checksum_oracle PRIVATE cxx_std_20)
target_compile_options(symbian_sis_checksum_oracle PRIVATE -fno-exceptions)
target_include_directories(symbian_sis_checksum_oracle PRIVATE
                           "${SYMBIAN_PLATFORM_ROOT}/cpp")
target_link_libraries(symbian_sis_checksum_oracle PRIVATE
                      symbian_historical_checksum GTest::gtest_main)
add_test(NAME symbian_sis_checksum_oracle COMMAND symbian_sis_checksum_oracle)
set_tests_properties(symbian_sis_checksum_oracle PROPERTIES
  ENVIRONMENT "SYMBIAN_SIS_TEST_PACKAGE=${SYMBIAN_SIS_TEST_PACKAGE}" TIMEOUT 15)
