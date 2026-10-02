# Import the bundled host thread implementation without exposing Boost.
get_filename_component(SYMBIAN_SDK_PREFIX
  "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(host_archive
  "${SYMBIAN_SDK_PREFIX}/lib/host/${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}/libsymbian_host_primitives.a")
if(NOT EXISTS "${host_archive}")
  message(FATAL_ERROR
    "No host concurrency archive for ${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}; install a matching SDK build")
endif()
if(NOT TARGET absl::statusor)
  find_package(absl CONFIG REQUIRED)
endif()
add_library(SymbianHostConcurrency STATIC IMPORTED)
set_target_properties(SymbianHostConcurrency PROPERTIES
  IMPORTED_LOCATION "${host_archive}")
target_include_directories(SymbianHostConcurrency INTERFACE
  "${SYMBIAN_SDK_PREFIX}/include/host"
  "${SYMBIAN_SDK_PREFIX}/include")
target_link_libraries(SymbianHostConcurrency INTERFACE
  absl::statusor absl::time absl::log absl::log_internal_check_op
  absl::random_random)
add_library(Symbian::HostConcurrency ALIAS SymbianHostConcurrency)
