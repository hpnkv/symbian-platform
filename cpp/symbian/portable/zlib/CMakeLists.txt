cmake_minimum_required(VERSION 3.28)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(symbian_portable_zlib LANGUAGES C CXX ASM)
  include(SymbianApp)
endif()

set(SYMBIAN_ZLIB_SOURCE
  "${CMAKE_SOURCE_DIR}/research/upstream/zlib"
  CACHE PATH "Pinned upstream zlib 1.3.1 source")
if(NOT EXISTS "${SYMBIAN_ZLIB_SOURCE}/zlib.h" OR
   NOT EXISTS "${SYMBIAN_ZLIB_SOURCE}/zconf.h")
  message(FATAL_ERROR "Portable zlib 1.3.1 source is missing at ${SYMBIAN_ZLIB_SOURCE}; run scripts/prepare_native_sources.py")
endif()
set(zlib_sources
  adler32.c compress.c crc32.c deflate.c gzclose.c gzlib.c gzread.c
  gzwrite.c infback.c inffast.c inflate.c inftrees.c trees.c uncompr.c zutil.c)
list(TRANSFORM zlib_sources PREPEND "${SYMBIAN_ZLIB_SOURCE}/")
add_library(symbian_portable_zlib STATIC ${zlib_sources})
target_include_directories(symbian_portable_zlib PUBLIC "${SYMBIAN_ZLIB_SOURCE}")
target_link_libraries(symbian_portable_zlib PUBLIC Symbian::Runtime Symbian::OpenC)
# Upstream's gzip translation units use POSIX read/write/close without
# including unistd.h. Supply that public prerequisite at the build boundary.
foreach(source IN ITEMS gzclose.c gzlib.c gzread.c gzwrite.c)
  set_source_files_properties("${SYMBIAN_ZLIB_SOURCE}/${source}" PROPERTIES
    COMPILE_OPTIONS "-include;unistd.h")
endforeach()
