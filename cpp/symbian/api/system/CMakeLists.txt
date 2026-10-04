add_library(symbian_api_system STATIC counters.cc)
target_compile_features(symbian_api_system PUBLIC cxx_std_20)
target_compile_options(symbian_api_system PRIVATE -fno-pic -fno-exceptions)
target_include_directories(symbian_api_system PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../include")
target_link_libraries(symbian_api_system PUBLIC Symbian::AbseilStatusOr)
