# Source-built optional libraries with version-matched public headers.
include_guard(GLOBAL)
set(zlib_archive
  "${SYMBIAN_SDK_PREFIX}/lib/${SYMBIAN_TARGET_ARCH}/libsymbian_portable_zlib.a")
set(zlib_headers "${SYMBIAN_SDK_PREFIX}/include/portable/zlib")
if(EXISTS "${SYMBIAN_SDK_PREFIX}/share/symbian/portable/zlib.json")
  add_library(SymbianPortableZlib STATIC IMPORTED)
  set_target_properties(SymbianPortableZlib PROPERTIES
    IMPORTED_LOCATION "${zlib_archive}" SYMBIAN_PORTABLE_ZLIB TRUE)
  target_include_directories(SymbianPortableZlib SYSTEM INTERFACE
    "${zlib_headers}")
  target_link_libraries(SymbianPortableZlib INTERFACE
    Symbian::Runtime Symbian::OpenC)
  add_library(Symbian::PortableZlib ALIAS SymbianPortableZlib)

  if(EXISTS "${SYMBIAN_SDK_PREFIX}/share/symbian/portable/png.json")
    add_library(SymbianPortablePng STATIC IMPORTED)
    set_target_properties(SymbianPortablePng PROPERTIES
      IMPORTED_LOCATION
        "${SYMBIAN_SDK_PREFIX}/lib/${SYMBIAN_TARGET_ARCH}/libsymbian_portable_png.a"
      SYMBIAN_PORTABLE_PNG TRUE)
    target_include_directories(SymbianPortablePng SYSTEM INTERFACE
      "${SYMBIAN_SDK_PREFIX}/include/portable/png")
    target_link_libraries(SymbianPortablePng INTERFACE
      Symbian::PortableZlib)
    add_library(Symbian::PortablePng ALIAS SymbianPortablePng)
  endif()

  function(_symbian_validate_portable_zlib directory)
    # This deferred function can run after SymbianApp was included from inside
    # another function, whose local path variables have gone out of scope.
    get_target_property(zlib_archive SymbianPortableZlib IMPORTED_LOCATION)
    get_filename_component(zlib_archdir "${zlib_archive}" DIRECTORY)
    get_filename_component(zlib_libdir "${zlib_archdir}" DIRECTORY)
    get_filename_component(zlib_prefix "${zlib_libdir}" DIRECTORY)
    set(zlib_headers "${zlib_prefix}/include/portable/zlib")
    get_property(targets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
    foreach(target IN LISTS targets)
      get_target_property(native ${target} SYMBIAN_NATIVE_API)
      if(native)
        continue()
      endif()
      set_property(GLOBAL PROPERTY SYMBIAN_GRAPHICS_WALK "")
      set_property(GLOBAL PROPERTY SYMBIAN_GRAPHICS_CONDITIONAL "")
      _symbian_graphics_walk(${target} ROOT)
      get_property(nodes GLOBAL PROPERTY SYMBIAN_GRAPHICS_WALK)
      set(portable FALSE)
      set(png FALSE)
      set(device FALSE)
      foreach(node IN LISTS nodes)
        get_target_property(portable_node ${node} SYMBIAN_PORTABLE_ZLIB)
        get_target_property(png_node ${node} SYMBIAN_PORTABLE_PNG)
        get_target_property(native_node ${node} SYMBIAN_NATIVE_API)
        if(portable_node)
          set(portable TRUE)
        endif()
        if(png_node)
          set(png TRUE)
        endif()
        if(native_node STREQUAL "Native_libz")
          set(device TRUE)
        endif()
      endforeach()
      if(portable AND device)
        message(FATAL_ERROR
          "${target}: Symbian::PortableZlib and Symbian::Native_libz both define zlib symbols; select exactly one implementation")
      endif()
      if(portable)
        foreach(required IN ITEMS "${zlib_archive}"
            "${zlib_headers}/zlib.h" "${zlib_headers}/zconf.h"
            "${zlib_prefix}/licenses/portable/zlib-README.txt")
          if(NOT EXISTS "${required}" AND NOT SYMBIAN_WORKSPACE_INPUTS)
            message(FATAL_ERROR
              "${target}: Symbian::PortableZlib is unavailable: missing SDK payload ${required}. Reinstall the complete SDK.")
          endif()
        endforeach()
      endif()
      if(png)
        get_target_property(png_archive SymbianPortablePng IMPORTED_LOCATION)
        foreach(required IN ITEMS "${png_archive}"
            "${zlib_prefix}/include/portable/png/png.h"
            "${zlib_prefix}/include/portable/png/pngconf.h"
            "${zlib_prefix}/include/portable/png/pnglibconf.h"
            "${zlib_prefix}/licenses/portable/libpng-LICENSE.txt")
          if(NOT EXISTS "${required}" AND NOT SYMBIAN_WORKSPACE_INPUTS)
            message(FATAL_ERROR
              "${target}: Symbian::PortablePng is unavailable: missing SDK payload ${required}. Reinstall the complete SDK.")
          endif()
        endforeach()
      endif()
    endforeach()
    get_property(children DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
    foreach(child IN LISTS children)
      _symbian_validate_portable_zlib("${child}")
    endforeach()
  endfunction()
  cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}"
    CALL _symbian_validate_portable_zlib "${CMAKE_SOURCE_DIR}")
endif()
