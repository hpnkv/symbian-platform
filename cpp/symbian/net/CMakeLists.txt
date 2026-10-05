# Host and guest use the same nghttp2 memory-callback implementation.
include(FetchContent)
set(SYMBIAN_NGHTTP2_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/../../../third_party/nghttp2" CACHE PATH "Pinned nghttp2 checkout (optional)")
if(NOT EXISTS "${SYMBIAN_NGHTTP2_SOURCE}/lib/nghttp2_session.c")
  FetchContent_Declare(symbian_nghttp2
    GIT_REPOSITORY https://github.com/nghttp2/nghttp2.git
    GIT_TAG 85e300c79fb6dbcfa9c1013215c8710c1c2cd3d2
    SOURCE_SUBDIR symbian-unused)
  FetchContent_MakeAvailable(symbian_nghttp2)
  set(SYMBIAN_NGHTTP2_SOURCE "${symbian_nghttp2_SOURCE_DIR}")
endif()
# Check supplied archives as well as Git checkouts against the release inputs.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/nghttp2-source.json" ng_manifest)
string(JSON ng_file_count LENGTH "${ng_manifest}" files)
math(EXPR ng_last_file "${ng_file_count} - 1")
foreach(index RANGE 0 ${ng_last_file})
  string(JSON ng_file MEMBER "${ng_manifest}" files ${index})
  string(JSON ng_expected GET "${ng_manifest}" files "${ng_file}")
  file(SHA256 "${SYMBIAN_NGHTTP2_SOURCE}/${ng_file}" ng_actual)
  if(NOT ng_actual STREQUAL ng_expected)
    message(FATAL_ERROR "nghttp2 release input mismatch: ${ng_file}")
  endif()
endforeach()
set(ng_sources pq map queue frame buf stream outbound_item session submit helper
  alpn hd hd_huffman hd_huffman_data version priority_spec option callbacks mem
  http rcbuf extpri ratelim time debug)
list(TRANSFORM ng_sources PREPEND "${SYMBIAN_NGHTTP2_SOURCE}/lib/nghttp2_")
list(TRANSFORM ng_sources APPEND ".c")
list(APPEND ng_sources "${SYMBIAN_NGHTTP2_SOURCE}/lib/sfparse.c")
set(PACKAGE_VERSION "1.70.0")
set(PACKAGE_VERSION_NUM "0x014600")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/include/nghttp2")
configure_file("${SYMBIAN_NGHTTP2_SOURCE}/lib/includes/nghttp2/nghttp2ver.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/include/nghttp2/nghttp2ver.h" @ONLY)
if(TARGET Symbian::AbseilStatusOr)
  list(REMOVE_ITEM ng_sources "${SYMBIAN_NGHTTP2_SOURCE}/lib/nghttp2_time.c")
  list(APPEND ng_sources guest_time.cc)
endif()
add_library(symbian_nghttp2 STATIC ${ng_sources})
target_include_directories(symbian_nghttp2 PUBLIC
  "${SYMBIAN_NGHTTP2_SOURCE}/lib/includes"
  "${CMAKE_CURRENT_BINARY_DIR}/include")
target_compile_definitions(symbian_nghttp2 PUBLIC NGHTTP2_STATICLIB)
target_compile_definitions(symbian_nghttp2 PRIVATE BUILDING_NGHTTP2
  HAVE_CLOCK_GETTIME=1 HAVE_DECL_CLOCK_MONOTONIC=1)
target_compile_options(symbian_nghttp2 PRIVATE
  -include "${CMAKE_CURRENT_SOURCE_DIR}/network_byte_order.h")
set(SYMBIAN_NGHTTP2_SOURCE "${SYMBIAN_NGHTTP2_SOURCE}" PARENT_SCOPE)
if(TARGET Symbian::AbseilStatusOr)
  target_link_libraries(symbian_nghttp2 PRIVATE Symbian::Streams)
  target_compile_definitions(symbian_nghttp2 PRIVATE __SOFTFP=)
endif()
