add_executable(symbian_analysis_test elf_test.cc)
target_link_libraries(symbian_analysis_test PRIVATE
                      symbian::analysis GTest::gtest_main)
symbian_native_library(symbian_analysis_test)
add_test(NAME symbian_analysis_test COMMAND symbian_analysis_test)
set_tests_properties(symbian_analysis_test PROPERTIES TIMEOUT 15)
