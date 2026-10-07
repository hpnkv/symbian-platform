cmake_minimum_required(VERSION 3.28)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(sdl2_app LANGUAGES C CXX ASM)
endif()
include(SymbianApp)
option(SYMBIAN_ARKANOID_GPU "Use SDL OpenGL ES 2 with software fallback" ON)
get_filename_component(sdl2_repository "${CMAKE_CURRENT_SOURCE_DIR}/../.."
  ABSOLUTE)
if(NOT DEFINED SYMBIAN_SDL2_SOURCE)
  set(SYMBIAN_SDL2_SOURCE
    "${sdl2_repository}/research/upstream/sdl2/SDL2-2.30.11")
endif()
if(NOT DEFINED SYMBIAN_SDL3_SOURCE)
  set(SYMBIAN_SDL3_SOURCE
    "${sdl2_repository}/research/upstream/sdl3/SDL3-3.2.22")
endif()
if(NOT TARGET symbian_portable_sdl2 AND NOT TARGET Symbian::PortableSdl2)
  add_subdirectory("${sdl2_repository}/cpp/symbian/portable/sdl2"
    "${CMAKE_CURRENT_BINARY_DIR}/sdl2")
endif()
set(arkanoid_sources
  main.cc application.cc game.cc renderer.cc
  assets/primitives.cc assets/block.cc assets/block_atlas.cc assets/ball.cc
  assets/platform.cc assets/glyphs.cc assets/text.cc)
# ARMv6 Thumb lacks the wide multiply used for constant-division strength
# reduction. Keep the small arithmetic-heavy simulation and scene TUs in ARM
# state; the rest of the app retains compact Thumb code.
set_source_files_properties(game.cc renderer.cc PROPERTIES COMPILE_OPTIONS -marm)
symbian_add_executable(sdl2_app ${arkanoid_sources} c_api_probe.c)
target_include_directories(sdl2_app PRIVATE "${sdl2_repository}/examples")
target_link_options(sdl2_app PRIVATE
  "SHELL:-z nocopyreloc" "SHELL:-z notext"
  "--undefined=symbian_native_leave_personalities")
if(NOT TARGET symbian_api_media AND NOT TARGET Symbian::Media)
  add_subdirectory("${sdl2_repository}/cpp/symbian/api/media"
    "${CMAKE_CURRENT_BINARY_DIR}/sdk_media")
endif()
if(TARGET Symbian::Media)
  target_link_libraries(sdl2_app PRIVATE Symbian::Media)
else()
  target_link_libraries(sdl2_app PRIVATE symbian_api_media)
endif()
if(SYMBIAN_ARKANOID_GPU)
  target_compile_definitions(sdl2_app PRIVATE SYMBIAN_ARKANOID_GPU=1)
  if(TARGET Symbian::PortableSdl2Gpu)
    target_link_libraries(sdl2_app PRIVATE Symbian::PortableSdl2Gpu)
  elseif(TARGET symbian_portable_sdl2_gpu)
    target_link_libraries(sdl2_app PRIVATE symbian_portable_sdl2_gpu)
  else()
    message(FATAL_ERROR "SDL2 GPU archive unavailable")
  endif()
elseif(TARGET Symbian::PortableSdl2)
  target_link_libraries(sdl2_app PRIVATE Symbian::PortableSdl2)
else()
  target_link_libraries(sdl2_app PRIVATE symbian_portable_sdl2)
  target_include_directories(sdl2_app PRIVATE
    "${SYMBIAN_SDL2_SOURCE}/include"
    "${sdl2_repository}/cpp/symbian/portable/sdl2")
endif()
symbian_publish_executable(sdl2_app UID3 0xe0000e20)

if(NOT TARGET symbian_portable_sdl3 AND NOT TARGET Symbian::PortableSdl3)
  add_subdirectory("${sdl2_repository}/cpp/symbian/portable/sdl3"
    "${CMAKE_CURRENT_BINARY_DIR}/sdl3")
endif()
symbian_add_executable(sdl3_app ${arkanoid_sources} c_api_probe_sdl3.c)
target_include_directories(sdl3_app PRIVATE "${sdl2_repository}/examples")
target_compile_definitions(sdl3_app PRIVATE
  SYMBIAN_ARKANOID_SDL3=1 SYMBIAN_CAF_V2 __SOFTFP=)
if(TARGET Symbian::Media)
  target_link_libraries(sdl3_app PRIVATE Symbian::Media)
else()
  target_link_libraries(sdl3_app PRIVATE symbian_api_media)
endif()
target_link_options(sdl3_app PRIVATE
  "SHELL:-z nocopyreloc" "SHELL:-z notext"
  "--undefined=symbian_native_leave_personalities")
if(SYMBIAN_ARKANOID_GPU)
  target_compile_definitions(sdl3_app PRIVATE SYMBIAN_ARKANOID_GPU=1)
  if(TARGET Symbian::PortableSdl3Gpu)
    target_link_libraries(sdl3_app PRIVATE Symbian::PortableSdl3Gpu)
  elseif(TARGET symbian_portable_sdl3_gpu)
    target_link_libraries(sdl3_app PRIVATE symbian_portable_sdl3_gpu)
  else()
    message(FATAL_ERROR "SDL3 GPU archive unavailable")
  endif()
elseif(TARGET Symbian::PortableSdl3)
  target_link_libraries(sdl3_app PRIVATE Symbian::PortableSdl3)
else()
  target_link_libraries(sdl3_app PRIVATE symbian_portable_sdl3)
endif()
symbian_publish_executable(sdl3_app UID3 0xe0000e22)
# Registered applications index every native file in their source tree.
# Each backend compiles only its matching C API probe.
get_target_property(sdl2_sources sdl2_app SOURCES)
list(FILTER sdl2_sources EXCLUDE REGEX "c_api_probe_sdl3\\.c$")
set_property(TARGET sdl2_app PROPERTY SOURCES "${sdl2_sources}")
get_target_property(sdl3_sources sdl3_app SOURCES)
list(FILTER sdl3_sources EXCLUDE REGEX "c_api_probe\\.c$")
set_property(TARGET sdl3_app PROPERTY SOURCES "${sdl3_sources}")
