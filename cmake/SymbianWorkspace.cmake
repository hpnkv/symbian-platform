# The SDK workspace consumes its current source archives through the same
# public capability targets as installed applications. Preserved platform
# headers, import proxies and pinned Abseil remain SDK dependencies.
function(symbian_workspace_archive public_target source_target)
  if(NOT TARGET ${public_target})
    message(FATAL_ERROR "Workspace capability missing: ${public_target}")
  endif()
  get_target_property(source_binary ${source_target} BINARY_DIR)
  set_target_properties(${public_target} PROPERTIES IMPORTED_LOCATION
    "${source_binary}/${CMAKE_STATIC_LIBRARY_PREFIX}${source_target}${CMAKE_STATIC_LIBRARY_SUFFIX}")
  add_dependencies(${public_target} ${source_target})
endfunction()

function(symbian_workspace_components)
  foreach(component IN ITEMS System Connectivity Agent Power Display Camera Storage)
    string(TOLOWER "${component}" component_lower)
    symbian_workspace_archive(SymbianApi${component}
      symbian_api_${component_lower})
  endforeach()
  symbian_workspace_archive(SymbianRuntime symbian_guest_runtime)
  symbian_workspace_archive(SymbianStreams symbian_guest_runtime)
  symbian_workspace_archive(SymbianHttp symbian_http)
  set_property(TARGET SymbianHttp PROPERTY INTERFACE_LINK_LIBRARIES
    "Symbian::Connectivity;symbian_nghttp2")
  symbian_workspace_archive(SymbianWebSocket symbian_api_websocket)
  set_property(TARGET SymbianWebSocket PROPERTY INTERFACE_LINK_LIBRARIES
    "Symbian::Connectivity;symbian_websocket")
  # Put live SDK headers before the exported copy in application commands.
  foreach(target IN ITEMS SymbianRuntime SymbianStreams)
    get_target_property(source_definitions symbian_guest_runtime
      INTERFACE_COMPILE_DEFINITIONS)
    target_compile_definitions(${target} INTERFACE ${source_definitions})
    get_target_property(source_includes symbian_guest_runtime
      INTERFACE_INCLUDE_DIRECTORIES)
    target_include_directories(${target} BEFORE INTERFACE
      "${CMAKE_SOURCE_DIR}/cpp/symbian/api/include"
      "${CMAKE_SOURCE_DIR}/cpp"
      "${CMAKE_SOURCE_DIR}/cpp/symbian/concurrency/guest"
      "${CMAKE_SOURCE_DIR}/cpp/symbian/concurrency/common"
      ${source_includes})
  endforeach()
endfunction()

# Source dependencies use the same guest ABI/configuration as source libraries.
function(symbian_workspace_abseil)
  add_subdirectory(cmake/guest-abseil EXCLUDE_FROM_ALL)
  set(SYMBIAN_WORKSPACE_ABSEIL ON PARENT_SCOPE)
endfunction()
