add_executable(symbian_analysis_test elf_test.cc)
target_link_libraries(symbian_analysis_test PRIVATE
                      symbian::analysis GTest::gtest_main)
symbian_native_library(symbian_analysis_test)
add_test(NAME symbian_analysis_test COMMAND symbian_analysis_test)
set_tests_properties(symbian_analysis_test PROPERTIES TIMEOUT 15)

add_executable(symbian_e32_test e32_test.cc)
target_link_libraries(symbian_e32_test PRIVATE symbian::e32 GTest::gtest_main)
symbian_native_library(symbian_e32_test)
add_test(NAME symbian_e32_test COMMAND symbian_e32_test)
set_tests_properties(symbian_e32_test PROPERTIES TIMEOUT 15)

add_executable(symbian_sis_test sis_test.cc)
target_link_libraries(symbian_sis_test PRIVATE symbian::sis GTest::gtest_main)
symbian_native_library(symbian_sis_test)
add_test(NAME symbian_sis_test COMMAND symbian_sis_test)
set_tests_properties(symbian_sis_test PROPERTIES TIMEOUT 15)
