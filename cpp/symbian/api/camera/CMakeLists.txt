add_library(symbian_api_camera STATIC camera.cc native_camera.cc)
target_compile_features(symbian_api_camera PUBLIC cxx_std_20)
target_compile_options(symbian_api_camera PRIVATE -fno-pic -fno-exceptions)
target_include_directories(symbian_api_camera PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../include"
  "${SYMBIAN_SDK_PREFIX}/include/platform")
target_link_libraries(symbian_api_camera PUBLIC Symbian::AbseilStatusOr)

target_include_directories(symbian_api_camera PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/../include")
