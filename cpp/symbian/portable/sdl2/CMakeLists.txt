cmake_minimum_required(VERSION 3.28)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(symbian_portable_sdl2 LANGUAGES C CXX ASM)
  include(SymbianApp)
endif()
if(NOT TARGET symbian_api_display)
  add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../api/display"
    "${CMAKE_CURRENT_BINARY_DIR}/sdk_display")
endif()

set(SYMBIAN_SDL2_SOURCE
  "${CMAKE_SOURCE_DIR}/research/upstream/sdl2/SDL2-2.30.11"
  CACHE PATH "Pinned upstream SDL2 2.30.11 source")
if(NOT EXISTS "${SYMBIAN_SDL2_SOURCE}/src/video/SDL_video.c")
  message(FATAL_ERROR "SDL2 source missing at ${SYMBIAN_SDL2_SOURCE}")
endif()

# Keep upstream files intact. One checked bootstrap insertion registers the
# owned Belle backend in a disposable source copy with valid relative includes.
set(sdl_copy "${CMAKE_CURRENT_BINARY_DIR}/upstream")
file(COPY "${SYMBIAN_SDL2_SOURCE}/src" DESTINATION "${sdl_copy}")
file(COPY "${SYMBIAN_SDL2_SOURCE}/include" DESTINATION "${sdl_copy}")
file(READ "${sdl_copy}/src/video/SDL_video.c" video_source)
set(video_anchor "/* Available video drivers */\nstatic VideoBootStrap *bootstrap[] = {\n")
string(FIND "${video_source}" "${video_anchor}" video_anchor_position)
if(video_anchor_position EQUAL -1)
  message(FATAL_ERROR "SDL2 video bootstrap changed; review the pinned source")
endif()
string(REPLACE "${video_anchor}"
  "${video_anchor}#ifdef SDL_VIDEO_DRIVER_SYMBIAN\n    &SYMBIAN_bootstrap,\n#endif\n"
  video_source "${video_source}")
file(WRITE "${sdl_copy}/src/video/SDL_video.c" "${video_source}")
file(READ "${sdl_copy}/src/video/SDL_sysvideo.h" video_header)
string(REPLACE "extern VideoBootStrap NGAGE_bootstrap;"
  "extern VideoBootStrap NGAGE_bootstrap;\nextern VideoBootStrap SYMBIAN_bootstrap;"
  video_header "${video_header}")
file(WRITE "${sdl_copy}/src/video/SDL_sysvideo.h" "${video_header}")

set(sdl_sources)
foreach(path IN ITEMS
    src src/atomic src/audio src/cpuinfo src/dynapi src/events src/file
    src/joystick src/haptic src/hidapi src/libm src/locale src/misc
    src/power src/render src/render/software src/sensor src/stdlib
    src/thread src/timer src/video src/video/yuv2rgb
    src/audio/dummy src/filesystem/dummy src/haptic/dummy
    src/joystick/dummy src/loadso/dummy src/locale/dummy src/misc/dummy
    src/power/dummy src/sensor/dummy src/thread/generic
    src/thread/generic/SDL_syscond.c src/thread/generic/SDL_sysmutex.c
    src/thread/generic/SDL_syssem.c)
  file(GLOB path_sources "${sdl_copy}/${path}/*.c")
  list(APPEND sdl_sources ${path_sources})
endforeach()
list(APPEND sdl_sources
  "${CMAKE_CURRENT_SOURCE_DIR}/video.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/clock.cc")
set_source_files_properties(
  "${sdl_copy}/src/atomic/SDL_atomic.c"
  "${sdl_copy}/src/atomic/SDL_spinlock.c"
  PROPERTIES COMPILE_OPTIONS -marm)
add_library(symbian_portable_sdl2 STATIC ${sdl_sources})
target_include_directories(symbian_portable_sdl2 BEFORE PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${sdl_copy}/include"
  "${sdl_copy}/src/video"
  "${sdl_copy}/src/events")
target_include_directories(symbian_portable_sdl2 PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${sdl_copy}/include")
target_compile_options(symbian_portable_sdl2 PRIVATE
  "-include${CMAKE_CURRENT_SOURCE_DIR}/SDL_config.h")
target_compile_definitions(symbian_portable_sdl2 PRIVATE
  SDL_STATIC=1 __LIBM_ALIASES_H__ SYMBIAN_CAF_V2 __SOFTFP=)
target_link_libraries(symbian_portable_sdl2 PUBLIC
  symbian_api_display
  "${SYMBIAN_SDK_PREFIX}/proxies/ws32/ws32.dso"
  "${SYMBIAN_SDK_PREFIX}/proxies/gdi/gdi.dso"
  "${SYMBIAN_SDK_PREFIX}/proxies/fbscli/fbscli.dso"
  "${SYMBIAN_SDK_PREFIX}/proxies/hal/hal.dso")

# A separate archive keeps the software profile loadable on firmware without
# EGL/GLES2 imports. The GPU profile still retains SDL's software fallback.
file(GLOB sdl_gpu_sources "${sdl_copy}/src/render/opengles2/*.c")
set_source_files_properties(
  "${sdl_copy}/src/render/opengles2/SDL_render_gles2.c"
  PROPERTIES COMPILE_DEFINITIONS __SDL_NOGETPROCADDR__
  COMPILE_OPTIONS
  "-include${CMAKE_CURRENT_SOURCE_DIR}/../gles2_legacy_compat.h;-Wno-incompatible-function-pointer-types")
add_library(symbian_portable_sdl2_gpu STATIC ${sdl_sources} ${sdl_gpu_sources})
target_include_directories(symbian_portable_sdl2_gpu BEFORE PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${sdl_copy}/include"
  "${sdl_copy}/src/video"
  "${sdl_copy}/src/events")
target_include_directories(symbian_portable_sdl2_gpu PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${sdl_copy}/include")
target_compile_options(symbian_portable_sdl2_gpu PRIVATE
  "-include${CMAKE_CURRENT_SOURCE_DIR}/SDL_config.h")
target_compile_definitions(symbian_portable_sdl2_gpu PRIVATE
  SDL_STATIC=1 __LIBM_ALIASES_H__ SYMBIAN_CAF_V2 __SOFTFP=
  SYMBIAN_SDL_GPU=1 SDL_VIDEO_OPENGL_ES2=1 SDL_VIDEO_RENDER_OGL_ES2=1)
target_link_libraries(symbian_portable_sdl2_gpu PUBLIC
  symbian_api_gles Symbian::GLES2
  "${SYMBIAN_SDK_PREFIX}/proxies/ws32/ws32.dso"
  "${SYMBIAN_SDK_PREFIX}/proxies/gdi/gdi.dso"
  "${SYMBIAN_SDK_PREFIX}/proxies/fbscli/fbscli.dso"
  "${SYMBIAN_SDK_PREFIX}/proxies/hal/hal.dso")
