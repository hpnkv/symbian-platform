# Optional installed-SDK validation project: compile each exported owned header
# as an independent consumer, using only its capability target's public ABI.
include("${CMAKE_CURRENT_LIST_DIR}/SymbianHeaderCanary.cmake")
function(symbian_sdk_header_canaries)
  if(NOT TARGET Symbian::Runtime)
    include(SymbianApp)
  endif()
  find_package(MbedTLS CONFIG REQUIRED)
  file(GLOB port_headers CONFIGURE_DEPENDS
    "${SYMBIAN_SDK_PREFIX}/include/symbian_mbedtls/*.h"
    "${SYMBIAN_SDK_PREFIX}/include/symbian_tls/*.h")
  symbian_header_canary(symbian_sdk_tls_c_header_canary C
    HEADERS ${port_headers} LIBRARIES MbedTLS::mbedtls)
  symbian_header_canary(symbian_sdk_tls_cpp_header_canary
    HEADERS ${port_headers} LIBRARIES MbedTLS::mbedtls)
  symbian_header_canary(symbian_sdk_runtime_header_canary
    HEADERS "${SYMBIAN_SDK_PREFIX}/include/symbian/runtime.h"
    LIBRARIES Symbian::Runtime)
  symbian_header_canary(symbian_sdk_time_header_canary
    HEADERS
      "${SYMBIAN_SDK_PREFIX}/include/symbian/api/time/monotonic_clock.h"
      "${SYMBIAN_SDK_PREFIX}/include/symbian/api/time/frame_pacer.h"
      "${SYMBIAN_SDK_PREFIX}/include/symbian/api/time/sleep.h"
    LIBRARIES Symbian::Runtime)
  symbian_header_canary(symbian_sdk_text_header_canary
    HEADERS "${SYMBIAN_SDK_PREFIX}/include/symbian/api/text/utf8.h"
    LIBRARIES Symbian::AbseilStatusOr)
  foreach(component IN ITEMS System Connectivity Storage Power Display Camera)
    string(TOLOWER "${component}" directory)
    file(GLOB_RECURSE headers CONFIGURE_DEPENDS
      "${SYMBIAN_SDK_PREFIX}/include/symbian/api/${directory}/*.h")
    if(component STREQUAL "Camera")
      list(FILTER headers EXCLUDE REGEX "gles2_frame_backend\\.h$")
    elseif(component STREQUAL "Display")
      list(FILTER headers EXCLUDE REGEX "gles_(window_context|rect_batch)\\.h$|gles2_texture\\.h$")
    elseif(component STREQUAL "System")
      list(FILTER headers EXCLUDE REGEX "(clipboard|failure_handler)\\.h$")
    endif()
    symbian_header_canary(symbian_sdk_${directory}_header_canary
      HEADERS ${headers} LIBRARIES Symbian::${component})
  endforeach()
  if(TARGET Symbian::GlesDisplay)
    symbian_header_canary(symbian_sdk_gles_display_header_canary
      HEADERS
        "${SYMBIAN_SDK_PREFIX}/include/symbian/api/display/gles_window_context.h"
        "${SYMBIAN_SDK_PREFIX}/include/symbian/api/display/gles_rect_batch.h"
        "${SYMBIAN_SDK_PREFIX}/include/symbian/api/display/gles2_texture.h"
      LIBRARIES Symbian::GlesDisplay)
  endif()
  symbian_header_canary(symbian_sdk_midi_output_header_canary
    HEADERS "${SYMBIAN_SDK_PREFIX}/include/symbian/api/media/midi_output.h"
    LIBRARIES Symbian::MidiOutput)
  symbian_header_canary(symbian_sdk_vibration_header_canary
    HEADERS "${SYMBIAN_SDK_PREFIX}/include/symbian/api/media/vibration.h"
    LIBRARIES Symbian::Vibration)
  if(TARGET Symbian::Clipboard)
    symbian_header_canary(symbian_sdk_clipboard_header_canary
      HEADERS "${SYMBIAN_SDK_PREFIX}/include/symbian/api/system/clipboard.h"
      LIBRARIES Symbian::Clipboard)
  endif()
  if(TARGET Symbian::FailureHandler)
    symbian_header_canary(symbian_sdk_failure_handler_header_canary
      HEADERS "${SYMBIAN_SDK_PREFIX}/include/symbian/api/system/failure_handler.h"
      LIBRARIES Symbian::FailureHandler)
  endif()
  if(TARGET Symbian::CameraGles2)
    symbian_header_canary(symbian_sdk_camera_gles2_header_canary
      HEADERS "${SYMBIAN_SDK_PREFIX}/include/symbian/api/camera/gles2_frame_backend.h"
      LIBRARIES Symbian::CameraGles2)
  endif()
  if(TARGET Symbian::PortableSdl2)
    symbian_header_canary(symbian_sdk_sdl2_c_header_canary C
      HEADERS "${SYMBIAN_SDK_PREFIX}/include/portable/sdl2/SDL.h"
      LIBRARIES Symbian::PortableSdl2)
    symbian_header_canary(symbian_sdk_sdl2_cpp_header_canary
      HEADERS "${SYMBIAN_SDK_PREFIX}/include/symbian/sdl2/sdl2.h"
      LIBRARIES Symbian::PortableSdl2)
  endif()
  if(TARGET Symbian::PortableSdl3)
    symbian_header_canary(symbian_sdk_sdl3_c_header_canary C
      HEADERS "${SYMBIAN_SDK_PREFIX}/include/portable/sdl3/SDL3/SDL.h"
      LIBRARIES Symbian::PortableSdl3)
    symbian_header_canary(symbian_sdk_sdl3_cpp_header_canary
      HEADERS "${SYMBIAN_SDK_PREFIX}/include/symbian/sdl3/sdl3.h"
      LIBRARIES Symbian::PortableSdl3)
  endif()
  foreach(component IN ITEMS Http WebSocket Agent)
    string(TOLOWER "${component}" directory)
    file(GLOB headers CONFIGURE_DEPENDS
      "${SYMBIAN_SDK_PREFIX}/include/symbian/${directory}/*.h")
    symbian_header_canary(symbian_sdk_${directory}_header_canary
      HEADERS ${headers} LIBRARIES Symbian::${component})
  endforeach()
  symbian_directory_header_canary(symbian_sdk_net_header_canary
    Symbian::Http "${SYMBIAN_SDK_PREFIX}/include/symbian/net")
  symbian_directory_header_canary(symbian_sdk_concurrency_header_canary
    Symbian::Stackless "${SYMBIAN_SDK_PREFIX}/include/symbian/concurrency")
  symbian_directory_header_canary(symbian_sdk_thread_header_canary
    Symbian::Fibers "${SYMBIAN_SDK_PREFIX}/include/thread")
  symbian_header_canary(symbian_sdk_native_status_header_canary
    HEADERS "${SYMBIAN_SDK_PREFIX}/include/symbian/native_status.h"
    LIBRARIES Symbian::Stackless)
  if(TARGET Symbian::PortableZlib)
    set(zlib_headers
      "${SYMBIAN_SDK_PREFIX}/include/portable/zlib/zlib.h"
      "${SYMBIAN_SDK_PREFIX}/include/portable/zlib/zconf.h")
    symbian_header_canary(symbian_sdk_portable_zlib_c_header_canary C
      HEADERS ${zlib_headers} LIBRARIES Symbian::PortableZlib)
    symbian_header_canary(symbian_sdk_portable_zlib_cpp_header_canary
      HEADERS ${zlib_headers} LIBRARIES Symbian::PortableZlib)
  endif()
  if(TARGET Symbian::PortablePng)
    set(png_headers
      "${SYMBIAN_SDK_PREFIX}/include/portable/png/png.h"
      "${SYMBIAN_SDK_PREFIX}/include/portable/png/pngconf.h"
      "${SYMBIAN_SDK_PREFIX}/include/portable/png/pnglibconf.h")
    symbian_header_canary(symbian_sdk_portable_png_c_header_canary C
      HEADERS ${png_headers} LIBRARIES Symbian::PortablePng)
    symbian_header_canary(symbian_sdk_portable_png_cpp_header_canary
      HEADERS ${png_headers} LIBRARIES Symbian::PortablePng)
  endif()
  if(TARGET Symbian::PortableJpeg)
    set(jpeg_headers
      "${SYMBIAN_SDK_PREFIX}/include/portable/jpeg/jpeglib.h"
      "${SYMBIAN_SDK_PREFIX}/include/portable/jpeg/jconfig.h"
      "${SYMBIAN_SDK_PREFIX}/include/portable/jpeg/jmorecfg.h"
      "${SYMBIAN_SDK_PREFIX}/include/portable/jpeg/jerror.h")
    # Original IJG jpeglib.h requires size_t and FILE before inclusion.
    symbian_header_canary(symbian_sdk_portable_jpeg_c_header_canary C
      PREINCLUDES stddef.h stdio.h
      HEADERS ${jpeg_headers} LIBRARIES Symbian::PortableJpeg)
    symbian_header_canary(symbian_sdk_portable_jpeg_cpp_header_canary
      PREINCLUDES cstddef cstdio
      HEADERS ${jpeg_headers} LIBRARIES Symbian::PortableJpeg)
  endif()
  if(TARGET Symbian::PortableFreeType)
    file(READ
      "${SYMBIAN_SDK_PREFIX}/share/symbian/portable/freetype.json"
      freetype_json)
    string(JSON freetype_count LENGTH "${freetype_json}" canary_headers)
    set(freetype_headers)
    math(EXPR freetype_last "${freetype_count} - 1")
    foreach(index RANGE 0 ${freetype_last})
      string(JSON name GET "${freetype_json}" canary_headers ${index})
      string(JSON relative GET "${freetype_json}" headers ${name})
      list(APPEND freetype_headers "${SYMBIAN_SDK_PREFIX}/${relative}")
    endforeach()
    symbian_header_canary(symbian_sdk_portable_freetype_c_header_canary C
      PREINCLUDES ft2build.h freetype/freetype.h
      HEADERS ${freetype_headers} LIBRARIES Symbian::PortableFreeType)
    symbian_header_canary(symbian_sdk_portable_freetype_cpp_header_canary
      PREINCLUDES ft2build.h freetype/freetype.h
      HEADERS ${freetype_headers} LIBRARIES Symbian::PortableFreeType)
  endif()
endfunction()
