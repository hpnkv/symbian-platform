add_library(symbian_api_agent STATIC ../../agent/guest_control.cc)
target_compile_features(symbian_api_agent PUBLIC cxx_std_20)
target_compile_options(symbian_api_agent PRIVATE -fno-pic -fno-exceptions)
target_include_directories(symbian_api_agent PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
target_link_libraries(symbian_api_agent PUBLIC Symbian::AbseilStatusOr)
