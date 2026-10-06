include("${CMAKE_CURRENT_LIST_DIR}/SymbianHeaderCanary.cmake")
foreach(component IN ITEMS analysis e32 sis sdk device status net http websocket)
  set(canary_owner symbian_${component})
  if(component STREQUAL "net")
    set(canary_owner symbian_http)
  endif()
  symbian_directory_header_canary(symbian_${component}_header_canary
    ${canary_owner} "${CMAKE_SOURCE_DIR}/cpp/symbian/${component}")
endforeach()
# Host agent framing has no guest service dependencies.
symbian_directory_header_canary(symbian_agent_header_canary
  symbian_agent_frame "${CMAKE_SOURCE_DIR}/cpp/symbian/agent")
symbian_directory_header_canary(symbian_host_concurrency_header_canary
  symbian::concurrency "${CMAKE_SOURCE_DIR}/cpp/symbian/concurrency/host")
symbian_directory_header_canary(symbian_common_concurrency_header_canary
  symbian::concurrency "${CMAKE_SOURCE_DIR}/cpp/symbian/concurrency/common")
symbian_header_canary(symbian_entropy_header_canary
  HEADERS "${CMAKE_SOURCE_DIR}/cpp/symbian/entropy/veneer.h")

# The opaque runtime bridge is also consumed by host fakes and bindings. Keep
# its annotation dependency available in the host IDE profile independently of
# ARM platform implementation headers.
symbian_header_canary(symbian_host_runtime_bridge_header_canary
  HEADERS "${CMAKE_SOURCE_DIR}/cpp/symbian/runtime/abi.h"
  LIBRARIES absl::nullability)
