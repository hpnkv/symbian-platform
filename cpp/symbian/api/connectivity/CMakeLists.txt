add_library(symbian_api_connectivity STATIC
  tcp_client.cc tcp_listener.cc native_tcp_client.cc)
target_compile_features(symbian_api_connectivity PUBLIC cxx_std_20)
target_compile_options(symbian_api_connectivity PRIVATE -fno-pic -fno-exceptions)
target_include_directories(symbian_api_connectivity PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../include"
  "${SYMBIAN_SDK_PREFIX}/include/platform")
target_link_libraries(symbian_api_connectivity PUBLIC Symbian::AbseilStatusOr)
