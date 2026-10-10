add_library(symbian_api_system STATIC counters.cc active_service.cc
  native_active_service.cc debug_log.cc application_management.cc
  serial_ports.cc)
target_compile_features(symbian_api_system PUBLIC cxx_std_20)
target_compile_options(symbian_api_system PRIVATE -fno-pic -fno-exceptions)
target_include_directories(symbian_api_system PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../include"
  "${SYMBIAN_SDK_PREFIX}/include/native"
  "${SYMBIAN_SDK_PREFIX}/include/platform")
target_link_libraries(symbian_api_system PUBLIC Symbian::AbseilStatusOr)

target_include_directories(symbian_api_system PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/../include")

add_library(symbian_api_clipboard STATIC clipboard.cc)
symbian_enable_native_leaves(symbian_api_clipboard SOURCES clipboard.cc)
set_property(SOURCE clipboard.cc APPEND PROPERTY COMPILE_OPTIONS -fno-rtti)
target_compile_features(symbian_api_clipboard PUBLIC cxx_std_20)
target_compile_options(symbian_api_clipboard PRIVATE
  -fno-pic -fno-exceptions -fno-rtti)
target_include_directories(symbian_api_clipboard PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/../include")
target_include_directories(symbian_api_clipboard PRIVATE
  "${SYMBIAN_SDK_PREFIX}/include/native"
  "${SYMBIAN_SDK_PREFIX}/include/platform")
target_link_libraries(symbian_api_clipboard PUBLIC Symbian::AbseilStatusOr
  "${SYMBIAN_SDK_PREFIX}/proxies/bafl/bafl.dso"
  "${SYMBIAN_SDK_PREFIX}/proxies/etext/etext.dso")

add_library(symbian_api_failure_handler STATIC failure_handler.cc)
target_compile_features(symbian_api_failure_handler PUBLIC cxx_std_20)
target_compile_options(symbian_api_failure_handler PRIVATE
  -fno-pic -fno-exceptions -fno-rtti)
target_include_directories(symbian_api_failure_handler PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/../include")
target_link_libraries(symbian_api_failure_handler PUBLIC
  symbian_api_clipboard symbian_api_system symbian_api_display
  symbian_api_storage)
