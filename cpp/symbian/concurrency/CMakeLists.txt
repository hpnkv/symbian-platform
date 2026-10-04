# The shared channel/Future/Task headers sit above an interchangeable
# thread::Mutex/CondVar/SleepFor backend. Host Boost.Fiber parks a fiber on
# contention. Boost is required to build the host implementation but is not
# exposed to consumers of the SDK target.
# Keep Boost headers and link targets inside the implementation library. The
# public thread:: ABI contains only opaque storage and Abseil time types.
set(Boost_USE_STATIC_LIBS ON)
find_package(Boost 1.82 CONFIG REQUIRED COMPONENTS fiber context)
find_package(Python3 COMPONENTS Interpreter REQUIRED)
get_filename_component(host_compiler_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
find_program(SYMBIAN_HOST_ARCHIVE_MERGER NAMES llvm-ar
  HINTS "${host_compiler_dir}" /opt/homebrew/opt/llvm/bin)
# GNU ar also supports the MRI -M script used by bundle_static.py. Linux
# manylinux images need not ship LLVM just to merge host static archives.
if(NOT SYMBIAN_HOST_ARCHIVE_MERGER AND CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
  set(SYMBIAN_HOST_ARCHIVE_MERGER "${CMAKE_AR}")
endif()
if(NOT SYMBIAN_HOST_ARCHIVE_MERGER)
  message(FATAL_ERROR "Install llvm-ar to merge host static archives")
endif()
add_library(symbian_concurrency INTERFACE)
add_library(symbian::concurrency ALIAS symbian_concurrency)
target_compile_features(symbian_concurrency INTERFACE cxx_std_20)
target_include_directories(symbian_concurrency INTERFACE
  "${CMAKE_CURRENT_SOURCE_DIR}/common")
add_library(symbian_host_primitive_objects STATIC
  host/thread/boost_primitives.cc
  host/thread/channel_waiter_state.cc
  host/thread/executor.cc
  host/thread/fiber.cc
  host/thread/select.cc
  host/thread/selectables.cc
  host/thread/util.cc
  host/thread/thread_pool.cc)
target_include_directories(symbian_host_primitive_objects PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/host")
target_include_directories(symbian_host_primitive_objects PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/common")
target_link_libraries(symbian_host_primitive_objects PRIVATE
  Boost::fiber Boost::context absl::status absl::log
  absl::log_internal_check_op absl::random_random)
symbian_native_library(symbian_host_primitive_objects)
# Boost.Context's forced unwind and Boost.Fiber's wait paths throw on Linux.
# Keep that required exception boundary in only the TUs that include Boost;
# the remaining implementation and public ABI retain -fno-exceptions.
set_source_files_properties(
  host/thread/boost_primitives.cc
  host/thread/executor.cc
  host/thread/fiber.cc
  PROPERTIES COMPILE_OPTIONS
  "$<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-fexceptions>")
set(bundle "${CMAKE_CURRENT_BINARY_DIR}/libsymbian_host_primitives.a")
get_filename_component(concurrency_workspace
  "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
add_custom_command(OUTPUT "${bundle}"
  COMMAND "${Python3_EXECUTABLE}" "${concurrency_workspace}/scripts/bundle_static.py"
    --ar "${SYMBIAN_HOST_ARCHIVE_MERGER}" --output "${bundle}"
    --library "$<TARGET_FILE:symbian_host_primitive_objects>"
    --library "$<TARGET_FILE:Boost::fiber>"
    --library "$<TARGET_FILE:Boost::context>"
  DEPENDS symbian_host_primitive_objects Boost::fiber Boost::context
    "${concurrency_workspace}/scripts/bundle_static.py"
  VERBATIM)
add_custom_target(symbian_host_primitives_bundle DEPENDS "${bundle}")
add_library(symbian_host_primitives STATIC IMPORTED GLOBAL)
set_target_properties(symbian_host_primitives PROPERTIES
  IMPORTED_LOCATION "${bundle}"
  INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/host"
  INTERFACE_LINK_LIBRARIES
    "absl::statusor;absl::time;absl::log;absl::log_internal_check_op;absl::random_random")
add_dependencies(symbian_host_primitives symbian_host_primitives_bundle)
target_link_libraries(symbian_concurrency INTERFACE
  symbian_host_primitives)
target_compile_definitions(symbian_concurrency INTERFACE
  SYMBIAN_CONCURRENCY_HOST_BOOST=1)

if(BUILD_TESTING)
  add_custom_target(symbian_a11_source_check
    COMMAND "${Python3_EXECUTABLE}"
            "${concurrency_workspace}/scripts/check_a11_concurrency.py"
    SOURCES sources.json "${concurrency_workspace}/.dev/thread-a11.md"
    VERBATIM)
endif()
