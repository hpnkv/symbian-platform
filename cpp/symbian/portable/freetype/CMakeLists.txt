cmake_minimum_required(VERSION 3.28)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(symbian_portable_freetype LANGUAGES C CXX ASM)
  include(SymbianApp)
endif()

set(SYMBIAN_FREETYPE_SOURCE
  "${CMAKE_SOURCE_DIR}/research/upstream/freetype"
  CACHE PATH "Pinned upstream FreeType 2.13.2 source")
if(NOT EXISTS "${SYMBIAN_FREETYPE_SOURCE}/include/ft2build.h")
  message(FATAL_ERROR "Portable FreeType 2.13.2 source is missing at ${SYMBIAN_FREETYPE_SOURCE}; run scripts/prepare_native_sources.py")
endif()
foreach(dependency IN ITEMS ZLIB BZIP2 PNG HARFBUZZ BROTLI)
  set(FT_DISABLE_${dependency} TRUE CACHE BOOL "" FORCE)
endforeach()
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(DISABLE_FORCE_DEBUG_POSTFIX TRUE CACHE BOOL "" FORCE)
add_subdirectory("${SYMBIAN_FREETYPE_SOURCE}" upstream)
# FreeType's own install(EXPORT) disallows external SDK targets in its target
# link interface. Apply the public Open C/runtime compile profile to its
# source target; Symbian::PortableFreeType supplies the link closure.
foreach(dependency IN ITEMS Symbian::OpenC Symbian::Runtime)
  target_include_directories(freetype PRIVATE
    "$<TARGET_PROPERTY:${dependency},INTERFACE_INCLUDE_DIRECTORIES>")
  target_compile_definitions(freetype PRIVATE
    "$<TARGET_PROPERTY:${dependency},INTERFACE_COMPILE_DEFINITIONS>")
  target_compile_options(freetype PRIVATE
    "$<TARGET_PROPERTY:${dependency},INTERFACE_COMPILE_OPTIONS>")
endforeach()
