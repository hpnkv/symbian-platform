# Build and execute the pinned, unmodified A11 host thread tests in isolation.
# This target is intentionally separate from the staged SDK host library: both
# define thread:: symbols and must not be linked into one process.
cmake_minimum_required(VERSION 3.28)
project(symbian_a11_full_host_probe LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(ABSL_BUILD_TESTING OFF)
set(ABSL_PROPAGATE_CXX_STD ON)
enable_testing()
set(BUILD_TESTING ON)

get_filename_component(symbian_workspace
  "${CMAKE_CURRENT_LIST_DIR}/../../../.." ABSOLUTE)
find_package(Boost 1.82 CONFIG REQUIRED COMPONENTS fiber context thread)
find_package(GTest CONFIG REQUIRED)
add_subdirectory("${symbian_workspace}/research/upstream/abseil-cpp" abseil)

function(a11_disallow_exceptions target)
  target_compile_options(${target} PRIVATE
    $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-fno-exceptions>)
endfunction()

function(a11_allow_exceptions)
  foreach(source IN LISTS ARGN)
    set_source_files_properties("${source}" PROPERTIES COMPILE_OPTIONS
      $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-fexceptions>)
  endforeach()
endfunction()

add_subdirectory(
  "${symbian_workspace}/cpp/symbian/concurrency/upstream/cpp/thread" thread)
