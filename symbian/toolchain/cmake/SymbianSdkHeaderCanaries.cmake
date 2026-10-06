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
  foreach(component IN ITEMS System Connectivity Storage Power Display Camera)
    string(TOLOWER "${component}" directory)
    file(GLOB_RECURSE headers CONFIGURE_DEPENDS
      "${SYMBIAN_SDK_PREFIX}/include/symbian/api/${directory}/*.h")
    symbian_header_canary(symbian_sdk_${directory}_header_canary
      HEADERS ${headers} LIBRARIES Symbian::${component})
  endforeach()
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
endfunction()
