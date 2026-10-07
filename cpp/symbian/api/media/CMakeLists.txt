add_library(symbian_api_media STATIC game_feedback.cc)
symbian_enable_native_leaves(symbian_api_media SOURCES game_feedback.cc)
set_property(SOURCE game_feedback.cc APPEND PROPERTY COMPILE_OPTIONS -fno-rtti)
target_compile_features(symbian_api_media PUBLIC cxx_std_20)
target_compile_options(symbian_api_media PRIVATE -fno-pic -fno-exceptions)
target_include_directories(symbian_api_media PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/../include")
target_include_directories(symbian_api_media PRIVATE
  "${SYMBIAN_SDK_PREFIX}/include/native"
  "${CMAKE_CURRENT_SOURCE_DIR}/compat/original")
target_link_libraries(symbian_api_media PUBLIC Symbian::AbseilStatusOr)
target_link_libraries(symbian_api_media PRIVATE Symbian::Fibers)
target_link_libraries(symbian_api_media PUBLIC
  "${SYMBIAN_SDK_PREFIX}/proxies/midiclient/midiclient.dso"
  "${SYMBIAN_SDK_PREFIX}/proxies/hwrmvibraclient/hwrmvibraclient.dso")
