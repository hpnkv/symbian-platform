include("${CMAKE_CURRENT_LIST_DIR}/SymbianHeaderCanary.cmake")

function(symbian_guest_runtime_header_canaries root)
  symbian_header_canary(symbian_runtime_header_canary
    HEADERS "${root}/cpp/symbian/runtime/abi.h"
    LIBRARIES symbian_guest_runtime)
  symbian_header_canary(symbian_native_cpp_compat_header_canary
    HEADERS "${root}/symbian/toolchain/cmake/native_cpp_compat.h"
    LIBRARIES symbian_guest_runtime)
  symbian_header_canary(symbian_runtime_c_header_canary C
    HEADERS "${root}/cpp/symbian/runtime/mimalloc_port.h"
    LIBRARIES symbian_guest_runtime)
endfunction()

function(symbian_guest_api_header_canaries root)
  symbian_header_canary(symbian_api_time_header_canary
    HEADERS
      "${root}/cpp/symbian/api/include/symbian/api/time/monotonic_clock.h"
      "${root}/cpp/symbian/api/include/symbian/api/time/sleep.h"
    LIBRARIES Symbian::Runtime)
  foreach(component IN ITEMS system connectivity storage agent power display media camera)
    # Public API consumers receive the source include root, as installed SDK
    # consumers receive include/symbian/api; internals use the same owner's ABI.
    file(GLOB_RECURSE public_headers CONFIGURE_DEPENDS
      "${root}/cpp/symbian/api/include/symbian/api/${component}/*.h")
    if(component STREQUAL "display")
      list(FILTER public_headers EXCLUDE REGEX "gles_window_context\\.h$")
    endif()
    if(public_headers)
      symbian_header_canary(symbian_api_${component}_public_header_canary
        HEADERS ${public_headers} LIBRARIES symbian_api_${component})
    endif()
    file(GLOB internal_headers CONFIGURE_DEPENDS
      "${root}/cpp/symbian/api/${component}/*.h")
    if(internal_headers)
      symbian_header_canary(symbian_api_${component}_internal_header_canary
        HEADERS ${internal_headers} LIBRARIES symbian_api_${component})
    endif()
  endforeach()
  symbian_header_canary(symbian_api_gles_public_header_canary
    HEADERS
      "${root}/cpp/symbian/api/include/symbian/api/display/gles_window_context.h"
    LIBRARIES symbian_api_gles)
  symbian_header_canary(symbian_guest_agent_header_canary
    HEADERS "${root}/cpp/symbian/agent/guest_control.h"
      "${root}/cpp/symbian/agent/guest_log.h"
      "${root}/cpp/symbian/agent/guest_files.h"
    LIBRARIES symbian_api_agent)
endfunction()

function(symbian_guest_sdl_header_canaries root)
  symbian_header_canary(symbian_guest_sdl2_cpp_header_canary
    HEADERS
      "${root}/cpp/symbian/portable/sdl2/include/symbian/sdl2/sdl2.h"
    LIBRARIES symbian_portable_sdl2)
  symbian_header_canary(symbian_guest_sdl3_cpp_header_canary
    HEADERS
      "${root}/cpp/symbian/portable/sdl3/include/symbian/sdl3/sdl3.h"
    LIBRARIES symbian_portable_sdl3)
endfunction()

function(symbian_guest_concurrency_header_canaries root)
  symbian_directory_header_canary(symbian_guest_concurrency_header_canary
    symbian_guest_fiber "${root}/cpp/symbian/concurrency/guest")
  # common/thread/channel.h is the host A11 implementation; ARM consumers use
  # guest/thread/channel.h, checked above. Shared modern headers work on both.
  symbian_directory_header_canary(symbian_guest_common_header_canary
    symbian_guest_fiber "${root}/cpp/symbian/concurrency/common/symbian")
endfunction()

# Remaining cross-platform guest consumers in the root ARM profile.
function(symbian_guest_shared_header_canaries root)
foreach(component IN ITEMS net http websocket)
  set(canary_owner symbian_${component})
  if(component STREQUAL "net")
    set(canary_owner symbian_http)
  endif()
  symbian_directory_header_canary(symbian_guest_${component}_header_canary
    ${canary_owner} "${root}/cpp/symbian/${component}")
endforeach()
symbian_header_canary(symbian_guest_entropy_header_canary
  HEADERS "${root}/cpp/symbian/entropy/veneer.h"
  LIBRARIES symbian_guest_runtime)
endfunction()
