add_library(symbian_api_power STATIC power.cc native_power.cc)
target_compile_features(symbian_api_power PUBLIC cxx_std_20)
target_compile_options(symbian_api_power PRIVATE -fno-pic -fno-exceptions)
target_include_directories(symbian_api_power PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../include"
  "${SYMBIAN_SDK_PREFIX}/include/platform")
target_link_libraries(symbian_api_power PUBLIC Symbian::AbseilStatusOr)

target_include_directories(symbian_api_power PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/../include")
