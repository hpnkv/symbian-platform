add_executable(symbian_analysis_test elf_test.cc)
target_link_libraries(symbian_analysis_test PRIVATE
                      symbian::analysis GTest::gtest_main)
symbian_native_library(symbian_analysis_test)
add_test(NAME symbian_analysis_test COMMAND symbian_analysis_test)
set_tests_properties(symbian_analysis_test PROPERTIES TIMEOUT 15)

add_executable(symbian_e32_test e32_test.cc arm_attributes_test.cc)
target_link_libraries(symbian_e32_test PRIVATE symbian::e32 GTest::gtest_main)
symbian_native_library(symbian_e32_test)
add_test(NAME symbian_e32_test COMMAND symbian_e32_test)
set_tests_properties(symbian_e32_test PROPERTIES TIMEOUT 15)

add_executable(symbian_sis_test sis_test.cc)
target_link_libraries(symbian_sis_test PRIVATE symbian::sis GTest::gtest_main)
symbian_native_library(symbian_sis_test)
add_test(NAME symbian_sis_test COMMAND symbian_sis_test)
set_tests_properties(symbian_sis_test PROPERTIES TIMEOUT 15)

add_executable(symbian_sdk_test sdk_test.cc)
target_link_libraries(symbian_sdk_test PRIVATE symbian::sdk GTest::gtest_main)
symbian_native_library(symbian_sdk_test)
add_test(NAME symbian_sdk_test COMMAND symbian_sdk_test)
set_tests_properties(symbian_sdk_test PROPERTIES TIMEOUT 15)

option(SYMBIAN_BUILD_SDK_ORDINAL_ORACLE "Build isolated historical SDK ordinal method oracle" OFF)
if(SYMBIAN_BUILD_SDK_ORDINAL_ORACLE)
  add_subdirectory(sdk_oracle)
endif()

add_executable(symbian_gui_model_test gui_model_test.cc)
target_include_directories(symbian_gui_model_test PRIVATE
                          "${CMAKE_CURRENT_SOURCE_DIR}/../../examples/gui_app")
target_link_libraries(symbian_gui_model_test PRIVATE GTest::gtest_main)
symbian_native_library(symbian_gui_model_test)
add_test(NAME symbian_gui_model_test COMMAND symbian_gui_model_test)
set_tests_properties(symbian_gui_model_test PROPERTIES TIMEOUT 15)

add_executable(symbian_status_test status_test.cc)
target_link_libraries(symbian_status_test PRIVATE symbian::status GTest::gtest_main)
symbian_native_library(symbian_status_test)
add_test(NAME symbian_status_test COMMAND symbian_status_test)
set_tests_properties(symbian_status_test PROPERTIES TIMEOUT 15)

add_executable(symbian_app_starter_test app_starter_test.cc
  "${CMAKE_SOURCE_DIR}/symbian/project/templates/model.cc"
  "${CMAKE_SOURCE_DIR}/symbian/project/templates/app_bridge.cc")
target_include_directories(symbian_app_starter_test PRIVATE
  "${CMAKE_SOURCE_DIR}/symbian/project/templates")
target_link_libraries(symbian_app_starter_test PRIVATE GTest::gtest_main)
symbian_native_library(symbian_app_starter_test)
add_test(NAME symbian_app_starter_test COMMAND symbian_app_starter_test)
set_tests_properties(symbian_app_starter_test PROPERTIES TIMEOUT 15)

add_executable(symbian_concurrency_test concurrency_test.cc)
target_link_libraries(symbian_concurrency_test PRIVATE
  symbian::concurrency GTest::gtest_main)
symbian_native_library(symbian_concurrency_test)
add_test(NAME symbian_concurrency_test COMMAND symbian_concurrency_test)
set_tests_properties(symbian_concurrency_test PROPERTIES TIMEOUT 15)
if(TARGET symbian_host_primitives)
  add_executable(symbian_concurrency_fiber_test concurrency_fiber_test.cc)
  target_link_libraries(symbian_concurrency_fiber_test PRIVATE
    symbian::concurrency GTest::gtest_main)
  symbian_native_library(symbian_concurrency_fiber_test)
  add_test(NAME symbian_concurrency_fiber_test
    COMMAND symbian_concurrency_fiber_test)
  set_tests_properties(symbian_concurrency_fiber_test PROPERTIES TIMEOUT 15)
endif()
