add_library(symbian_api_display STATIC display.cc native_display.cc resident_panel.cc native_resident_panel.cc)
target_compile_features(symbian_api_display PUBLIC cxx_std_20)
target_compile_options(symbian_api_display PRIVATE -fno-pic -fno-exceptions)
target_include_directories(symbian_api_display PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../include"
  "${SYMBIAN_SDK_PREFIX}/include/platform")
target_link_libraries(symbian_api_display PUBLIC Symbian::AbseilStatusOr)

target_include_directories(symbian_api_display PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/../include")
