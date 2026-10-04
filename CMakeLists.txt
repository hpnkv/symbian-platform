cmake_minimum_required(VERSION 3.28)
project(symbian_platform VERSION 0.1.0 LANGUAGES CXX)

# CLion can load this same repository with a second, real ARM CMake profile.
# Keep the host dependency graph out of that profile so compiler discovery and
# compile_commands.json use the guest toolchain for every probe source.
option(SYMBIAN_INDEX_GUEST_PROBES "Expose all guest probes to CMake IDEs" OFF)
if(SYMBIAN_INDEX_GUEST_PROBES)
  if(NOT CMAKE_SYSTEM_NAME STREQUAL "Generic" OR
     NOT CMAKE_CXX_COMPILER_TARGET MATCHES "^arm(v5t|v6)-none-eabi$")
    message(FATAL_ERROR
      "Guest probe indexing requires the symbian-arm.cmake cross toolchain")
  endif()
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
  enable_language(C ASM)
  set(SYMBIAN_LIBCXX_SOURCE
    "${CMAKE_SOURCE_DIR}/research/upstream/llvm-project/libcxx")
  set(SYMBIAN_OPENC_SOURCE
    "${CMAKE_SOURCE_DIR}/research/upstream/ossrv/genericopenlibs/openenvcore")
  set(SYMBIAN_GUI_SDK_INCLUDE
    "${CMAKE_SOURCE_DIR}/.symbian/gui-sdk/include")
  # The indexing target uses the widest currently implemented no-exceptions
  # libc++ profile. The standalone probe project remains the execution entry;
  # no host IDE Run target is created for an ARM ELF.
  set(SYMBIAN_RUNTIME_LOCALE_STREAM ON CACHE BOOL "" FORCE)
  add_subdirectory(cpp/symbian/runtime)
  # Give each probe its own target so an IDE uses the right local include
  # directory and can report the source's project membership. Keep this list
  # explicit: a new experiment needs a deliberate target/profile choice.
  set(indexed_probe_directories
      abi_probe abseil_allocator_probe abseil_status_probe
      connectivity_probe cxx20_probe cxx20_module_probe dll_data_probe
      dll_lifecycle_probe dll_probe e32_probe import_probe
      mbedtls_dll_probe pointer_probe runtime_probe)
  file(GLOB existing_probe_directories RELATIVE "${CMAKE_SOURCE_DIR}/examples"
       "${CMAKE_SOURCE_DIR}/examples/*_probe")
  foreach(probe_directory IN LISTS existing_probe_directories)
    list(FIND indexed_probe_directories "${probe_directory}" probe_index)
    if(probe_index EQUAL -1)
      message(FATAL_ERROR
        "${probe_directory} has no ARM IDE indexing target; add its build rules")
    endif()
  endforeach()
  add_custom_target(symbian_probe_index)
  foreach(probe IN ITEMS abi cxx20 dll_data dll_lifecycle dll e32 import
                         pointer runtime)
    set(probe_dir "${CMAKE_SOURCE_DIR}/examples/${probe}_probe")
    file(GLOB probe_sources CONFIGURE_DEPENDS
         "${probe_dir}/*.cc" "${probe_dir}/*.S")
    add_library(symbian_index_${probe} OBJECT EXCLUDE_FROM_ALL ${probe_sources})
    target_link_libraries(symbian_index_${probe} PRIVATE symbian_guest_runtime)
    add_dependencies(symbian_probe_index symbian_index_${probe})
  endforeach()
  target_include_directories(symbian_index_runtime PRIVATE
    "${CMAKE_SOURCE_DIR}/cpp/symbian/concurrency/guest"
    "${CMAKE_SOURCE_DIR}/cpp/symbian/concurrency/common")
  if(NOT EXISTS "${SYMBIAN_SDK_PREFIX}/cmake/SymbianApp.cmake")
    message(FATAL_ERROR
      "Abseil probe indexing needs SYMBIAN_SDK_PREFIX with an installed SDK")
  endif()
  include("${SYMBIAN_SDK_PREFIX}/cmake/SymbianApp.cmake")
  if(NOT TARGET Symbian::AbseilStatusOr)
    message(FATAL_ERROR
      "The selected SDK has no Abseil StatusOr indexing profile; update it")
  endif()
  # The stackless runtime probes now use installed guest StatusOr and time.
  target_link_libraries(symbian_index_runtime PRIVATE
                        Symbian::AbseilStatusOr)
  if(NOT TARGET Symbian::Connectivity OR NOT TARGET Symbian::Stackless)
    message(FATAL_ERROR
      "SDK at ${SYMBIAN_SDK_PREFIX} lacks Connectivity or Stackless. "
      "Update the ignored CMakeUserPresets.json SDK prefix to the active "
      "sdk.json directory, then reload the CLion CMake profile")
  endif()
  set(connectivity_dir "${CMAKE_SOURCE_DIR}/examples/connectivity_probe")
  file(GLOB connectivity_sources CONFIGURE_DEPENDS
       "${connectivity_dir}/*.cc" "${connectivity_dir}/*.S")
  add_library(symbian_index_connectivity OBJECT EXCLUDE_FROM_ALL
              ${connectivity_sources})
  target_link_libraries(symbian_index_connectivity PRIVATE
                        Symbian::Connectivity Symbian::Stackless)
  add_dependencies(symbian_probe_index symbian_index_connectivity)
  foreach(probe IN ITEMS abseil_allocator abseil_status)
    set(probe_dir "${CMAKE_SOURCE_DIR}/examples/${probe}_probe")
    file(GLOB probe_sources CONFIGURE_DEPENDS
         "${probe_dir}/*.cc" "${probe_dir}/*.S")
    add_library(symbian_index_${probe} OBJECT EXCLUDE_FROM_ALL ${probe_sources})
    target_link_libraries(symbian_index_${probe} PRIVATE Symbian::AbseilStatusOr)
    target_include_directories(symbian_index_${probe} PRIVATE
      "${CMAKE_SOURCE_DIR}/cpp/symbian/concurrency/guest"
      "${CMAKE_SOURCE_DIR}/cpp/symbian/concurrency/common")
    add_dependencies(symbian_probe_index symbian_index_${probe})
  endforeach()
  # Modules require CMake's dependency scanner; this is the same source set
  # and file-set treatment as the standalone module experiment.
  if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR
     NOT EXISTS "${CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS}")
    message(FATAL_ERROR
      "Guest probe indexing requires upstream Clang with clang-scan-deps "
      "for the C++20 module probe; select a matching LLVM toolchain")
  endif()
  set(module_dir "${CMAKE_SOURCE_DIR}/examples/cxx20_module_probe")
  add_library(symbian_index_cxx20_module OBJECT EXCLUDE_FROM_ALL
              "${module_dir}/probe.cc" "${module_dir}/startup.S")
  target_sources(symbian_index_cxx20_module PRIVATE
    FILE_SET CXX_MODULES FILES "${module_dir}/transform.cppm")
  target_link_libraries(symbian_index_cxx20_module PRIVATE
                        symbian_guest_runtime)
  set_target_properties(symbian_index_cxx20_module PROPERTIES
                        CXX_SCAN_FOR_MODULES ON)
  add_dependencies(symbian_probe_index symbian_index_cxx20_module)
  # Mbed TLS is an independently supplied adaptation. The local preset may
  # point to it; a portable SDK checkout need not have that external tree.
  set(MBEDTLS_SOURCE "${CMAKE_SOURCE_DIR}/third_party/mbedtls-symbian"
      CACHE PATH "Vendored Mbed TLS source for probe indexing")
  if(MBEDTLS_SOURCE)
    if(NOT EXISTS "${MBEDTLS_SOURCE}/include/mbedtls/sha256.h")
      message(FATAL_ERROR "MBEDTLS_SOURCE lacks include/mbedtls/sha256.h")
    endif()
    if(NOT EXISTS "${SYMBIAN_SDK_PREFIX}/include/openc/limits.h")
      message(FATAL_ERROR
        "The Mbed TLS probe needs SYMBIAN_SDK_PREFIX with target C headers")
    endif()
    set(mbedtls_dir "${CMAKE_SOURCE_DIR}/examples/mbedtls_dll_probe")
    add_library(symbian_index_mbedtls OBJECT EXCLUDE_FROM_ALL
                "${mbedtls_dir}/bridge.c" "${mbedtls_dir}/startup.S")
    target_include_directories(symbian_index_mbedtls PRIVATE
      "${MBEDTLS_SOURCE}/include"
      "${SYMBIAN_SDK_PREFIX}/include/compiler"
      "${SYMBIAN_SDK_PREFIX}/include/openc"
      "${SYMBIAN_SDK_PREFIX}/include/libc"
      "${SYMBIAN_SDK_PREFIX}/include/pthread"
      "${SYMBIAN_SDK_PREFIX}/include/posix4")
    add_dependencies(symbian_probe_index symbian_index_mbedtls)
  endif()
  set_source_files_properties(
    "${CMAKE_SOURCE_DIR}/examples/runtime_probe/locale_api_contract.cc"
    PROPERTIES COMPILE_DEFINITIONS _LIBCPP_BUILDING_LIBRARY)
  set_source_files_properties(
    "${CMAKE_SOURCE_DIR}/examples/runtime_probe/exception_contract.cc"
    "${CMAKE_SOURCE_DIR}/examples/runtime_probe/exception_metadata_contract.cc"
    PROPERTIES COMPILE_OPTIONS "-fexceptions;-frtti;-fno-pic")
  return()
endif()

include(CTest)
include(FetchContent)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
set(SYMBIAN_DEPS_PREFIX "$ENV{SYMBIAN_DEPS_PREFIX}" CACHE PATH
    "Isolated host dependency prefix, shared by developer and wheel builds")
if(SYMBIAN_DEPS_PREFIX)
  list(PREPEND CMAKE_PREFIX_PATH "${SYMBIAN_DEPS_PREFIX}")
  set(OPENSSL_ROOT_DIR "${SYMBIAN_DEPS_PREFIX}" CACHE PATH "" FORCE)
endif()
set(BUILD_SHARED_LIBS OFF)
set(ABSL_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(ABSL_PROPAGATE_CXX_STD ON CACHE BOOL "" FORCE)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(SYMBIAN_BUILD_PYTHON "Build the Python boundary" ON)
set(SYMBIAN_ABSEIL_SOURCE_DIR "$ENV{SYMBIAN_ABSEIL_SOURCE_DIR}" CACHE PATH
    "Optional checkout of the pinned Abseil revision")
if(SYMBIAN_ABSEIL_SOURCE_DIR)
  FetchContent_Declare(abseil SOURCE_DIR "${SYMBIAN_ABSEIL_SOURCE_DIR}")
else()
  FetchContent_Declare(abseil
    GIT_REPOSITORY https://github.com/abseil/abseil-cpp.git
    GIT_TAG 5650e9cf76d3be4318d5fa3af38ee483ddfd5e4a)
endif()
FetchContent_MakeAvailable(abseil)

function(symbian_native_library target)
  target_compile_features(${target} PUBLIC cxx_std_20)
  target_compile_options(${target} PRIVATE
    $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-fno-exceptions;-Wall;-Wextra;-Wconversion;-Wshadow>
    $<$<CXX_COMPILER_ID:AppleClang,Clang>:-Wthread-safety;-Werror=thread-safety>)
endfunction()

set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
FetchContent_Declare(nlohmann_json
  URL https://github.com/nlohmann/json/archive/refs/tags/v3.12.0.tar.gz
  URL_HASH SHA256=4b92eb0c06d10683f7447ce9406cb97cd4b453be18d7279320f7b2f025c10187
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE SYSTEM)
FetchContent_MakeAvailable(nlohmann_json)
add_subdirectory(cpp/symbian/status)
add_subdirectory(cpp/symbian/agent)
add_subdirectory(cpp/symbian/concurrency)
add_subdirectory(cpp/symbian/analysis)
add_subdirectory(cpp/symbian/e32)
add_subdirectory(cpp/symbian/sis)
add_subdirectory(cpp/symbian/sdk)
if(SYMBIAN_BUILD_PYTHON)
  add_subdirectory(cpp/symbian/device)
endif()
if(SYMBIAN_BUILD_PYTHON)
  add_subdirectory(cpp/python)
endif()
if(BUILD_TESTING)
  find_package(GTest CONFIG QUIET)
  if(NOT GTest_FOUND)
    FetchContent_Declare(googletest
      GIT_REPOSITORY https://github.com/google/googletest.git
      GIT_TAG v1.16.0 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(googletest)
  endif()
  add_subdirectory(cpp/tests)
endif()

install(FILES third_party/a11/LICENSE
        DESTINATION symbian/licenses RENAME A11-LICENSE)
install(FILES third_party/boost/LICENSE_1_0.txt
        DESTINATION symbian/licenses RENAME Boost-LICENSE)
install(FILES "${abseil_SOURCE_DIR}/LICENSE"
        DESTINATION symbian/licenses RENAME Abseil-LICENSE)
install(FILES "${nlohmann_json_SOURCE_DIR}/LICENSE.MIT"
        DESTINATION symbian/licenses RENAME nlohmann-json-LICENSE)

option(SYMBIAN_BUILD_GUI_EXAMPLE "Expose the prepared ARM GUI in the host project" ON)
set(SYMBIAN_GUI_SDK_INCLUDE "${CMAKE_SOURCE_DIR}/.symbian/gui-sdk/include"
    CACHE PATH "Materialized/staged Symbian GUI headers")
if(SYMBIAN_BUILD_GUI_EXAMPLE)
  if(EXISTS "${SYMBIAN_GUI_SDK_INCLUDE}/w32std.h" AND
     CMAKE_CXX_COMPILER_ID MATCHES "^(AppleClang|Clang)$")
    add_subdirectory(examples/gui_app EXCLUDE_FROM_ALL)
    # CLion can run an executable target. A custom build target only executes
    # its command during Build and has no application for the Run button.
    set(SYMBIAN_HOST_PYTHON "${CMAKE_SOURCE_DIR}/.venv/bin/python" CACHE FILEPATH
        "Installed platform Python used by the GUI supervisor")
    add_executable(gui_app_run EXCLUDE_FROM_ALL
                   cpp/symbian/emulator/launcher.cc)
    # A stable host launcher path also works with CLion's existing "Debug"
    # profile, whose build directory may differ from the shared debug preset.
    set_target_properties(gui_app_run PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build/debug")
    symbian_native_library(gui_app_run)
    target_compile_definitions(gui_app_run PRIVATE
      SYMBIAN_LAUNCH_PYTHON="${SYMBIAN_HOST_PYTHON}"
      SYMBIAN_LAUNCH_ROOT="${CMAKE_SOURCE_DIR}")
  else()
    message(STATUS "GUI target unavailable: prepare-gui-sdk and a Clang host compiler are required")
  endif()
endif()
