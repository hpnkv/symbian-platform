add_library(symbian_api_display STATIC display.cc native_display.cc window_surface.cc window_task_identity.cc screen_control.cc)
target_compile_features(symbian_api_display PUBLIC cxx_std_20)
target_compile_options(symbian_api_display PRIVATE -fno-pic -fno-exceptions)
target_include_directories(symbian_api_display PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../include"
  "${SYMBIAN_SDK_PREFIX}/include/platform")
target_link_libraries(symbian_api_display PUBLIC Symbian::AbseilStatusOr
  "${SYMBIAN_SDK_PREFIX}/proxies/ws32/ws32.dso"
  "${SYMBIAN_SDK_PREFIX}/proxies/gdi/gdi.dso"
  "${SYMBIAN_SDK_PREFIX}/proxies/fbscli/fbscli.dso"
  "${SYMBIAN_SDK_PREFIX}/proxies/hal/hal.dso")

target_include_directories(symbian_api_display PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/../include")

add_library(symbian_api_gles STATIC gles_window_context.cc gles_rect_batch.cc
  gles2_texture.cc)
target_compile_features(symbian_api_gles PUBLIC cxx_std_20)
target_compile_options(symbian_api_gles PRIVATE -fno-pic -fno-exceptions)
target_link_libraries(symbian_api_gles PUBLIC symbian_api_display Symbian::EGL Symbian::GLES2)
target_include_directories(symbian_api_gles PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/../include")
