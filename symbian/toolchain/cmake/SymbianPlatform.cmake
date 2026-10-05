# Original OS libraries expose their full frozen ABI. Applications name targets;
# only symbols used by the linker become dependencies in the E32 image.
foreach(pair IN ITEMS "EUser:euser" "WindowServer:ws32" "Gdi:gdi" "Hal:hal"
    "FileServer:efsrv" "SocketServer:esock" "Internet:insock" "C:libc"
    "Math:libm" "Pthread:libpthread" "CxxAbi:drtaeabi" "CameraNative:ecam")
  string(REPLACE ":" ";" fields "${pair}")
  list(GET fields 0 name)
  list(GET fields 1 dll)
  set(proxy "${SYMBIAN_SDK_PREFIX}/proxies/${dll}/${dll}.dso")
  # Source-runtime checks can select a diagnostic library through the build
  # tool. It is still represented by one normal imported CMake target.
  foreach(override IN LISTS SYMBIAN_IMPORT_PROXIES)
    if(override MATCHES "/${dll}\\.dso$")
      set(proxy "${override}")
    endif()
  endforeach()
  if(EXISTS "${proxy}" AND NOT TARGET Symbian::${name})
    add_library(Symbian${name} SHARED IMPORTED GLOBAL)
    set_target_properties(Symbian${name} PROPERTIES IMPORTED_LOCATION "${proxy}")
    target_include_directories(Symbian${name} SYSTEM INTERFACE
      "${SYMBIAN_SDK_PREFIX}/include/platform")
    target_compile_definitions(Symbian${name} INTERFACE _UNICODE __GCC32__
      __GCCV3__ __EABI__ __EPOC32__ __MARM__ __MARM_ARMV5__)
    target_compile_options(Symbian${name} INTERFACE -fshort-wchar)
    add_library(Symbian::${name} ALIAS Symbian${name})
  endif()
endforeach()
