cmake_minimum_required(VERSION 3.28)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(symbian_portable_png LANGUAGES C CXX ASM)
  include(SymbianApp)
endif()

set(SYMBIAN_PNG_SOURCE
  "${CMAKE_SOURCE_DIR}/research/upstream/libpng"
  CACHE PATH "Pinned upstream libpng 1.6.53 source")
if(NOT EXISTS "${SYMBIAN_PNG_SOURCE}/png.h" OR
   NOT EXISTS "${SYMBIAN_PNG_SOURCE}/scripts/pnglibconf.h.prebuilt")
  message(FATAL_ERROR "Portable libpng 1.6.53 source is missing at ${SYMBIAN_PNG_SOURCE}; run scripts/prepare_native_sources.py")
endif()
configure_file("${SYMBIAN_PNG_SOURCE}/scripts/pnglibconf.h.prebuilt"
  "${CMAKE_CURRENT_BINARY_DIR}/pnglibconf.h" COPYONLY)
set(png_sources
  png.c pngerror.c pngget.c pngmem.c pngpread.c pngread.c pngrio.c
  pngrtran.c pngrutil.c pngset.c pngtrans.c pngwio.c pngwrite.c
  pngwtran.c pngwutil.c)
list(TRANSFORM png_sources PREPEND "${SYMBIAN_PNG_SOURCE}/")
add_library(symbian_portable_png STATIC ${png_sources})
target_include_directories(symbian_portable_png PUBLIC
  "${CMAKE_CURRENT_BINARY_DIR}" "${SYMBIAN_PNG_SOURCE}")
target_include_directories(symbian_portable_png BEFORE PRIVATE
  "${SYMBIAN_SDK_PREFIX}/include/openc")
target_link_libraries(symbian_portable_png PUBLIC Symbian::PortableZlib)
target_compile_definitions(symbian_portable_png PRIVATE PNG_CONFIGURE_LIBPNG)
