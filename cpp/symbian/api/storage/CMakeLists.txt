add_library(symbian_api_storage STATIC storage.cc native_storage.cc)
target_compile_features(symbian_api_storage PUBLIC cxx_std_20)
target_compile_options(symbian_api_storage PRIVATE -fno-pic -fno-exceptions)
target_include_directories(symbian_api_storage PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../include"
  "${SYMBIAN_SDK_PREFIX}/include/platform")
target_link_libraries(symbian_api_storage PUBLIC Symbian::AbseilStatusOr)
