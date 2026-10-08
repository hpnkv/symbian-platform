cmake_minimum_required(VERSION 3.28)
file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/VERSION" SYMBIAN_VERSION LIMIT_COUNT 1)
if(NOT SYMBIAN_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
  message(FATAL_ERROR "VERSION must contain one semantic version")
endif()
if(SYMBIAN_INDEX_GUEST_PROBES)
  include(cmake/SymbianWorkspaceInputs.cmake)
  project(symbian_platform VERSION ${SYMBIAN_VERSION} LANGUAGES C CXX ASM)
else()
  project(symbian_platform VERSION ${SYMBIAN_VERSION} LANGUAGES C CXX)
endif()
include(cmake/SymbianNativeIndex.cmake)

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
  include(cmake/SymbianWorkspaceInputs.cmake)
  add_subdirectory(cpp/symbian/runtime)
  include(cmake/SymbianWorkspace.cmake)
  symbian_workspace_abseil()
  # Give each probe its own target so an IDE uses the right local include
  # directory and can report the source's project membership. Specialized
  # profiles below retain their exact source and dependency choices.
  set(indexed_probe_directories
      abi_probe abseil_allocator_probe abseil_status_probe
      connectivity_probe http_probe cxx20_probe cxx20_module_probe dll_data_probe
      dll_lifecycle_probe dll_probe e32_probe entropy_probe import_probe
      mbedtls_dll_probe pointer_probe runtime_probe)
  file(GLOB existing_probe_directories CONFIGURE_DEPENDS
       RELATIVE "${CMAKE_SOURCE_DIR}/probes"
       "${CMAKE_SOURCE_DIR}/probes/*_probe")
  add_custom_target(symbian_probe_index)
  foreach(probe_directory IN LISTS existing_probe_directories)
    list(FIND indexed_probe_directories "${probe_directory}" probe_index)
    if(probe_index EQUAL -1)
      set(probe_dir "${CMAKE_SOURCE_DIR}/probes/${probe_directory}")
      file(GLOB probe_sources CONFIGURE_DEPENDS
           "${probe_dir}/*.cc" "${probe_dir}/*.c" "${probe_dir}/*.S")
      if(probe_sources)
        add_library(symbian_index_${probe_directory} OBJECT EXCLUDE_FROM_ALL
                    ${probe_sources})
        target_link_libraries(symbian_index_${probe_directory} PRIVATE
                              symbian_guest_runtime)
        add_dependencies(symbian_probe_index
                         symbian_index_${probe_directory})
      endif()
    endif()
  endforeach()
  foreach(probe IN ITEMS abi cxx20 dll_data dll_lifecycle dll e32 import
                         pointer runtime)
    set(probe_dir "${CMAKE_SOURCE_DIR}/probes/${probe}_probe")
    file(GLOB probe_sources CONFIGURE_DEPENDS
         "${probe_dir}/*.cc" "${probe_dir}/*.S")
    add_library(symbian_index_${probe} OBJECT EXCLUDE_FROM_ALL ${probe_sources})
    target_link_libraries(symbian_index_${probe} PRIVATE symbian_guest_runtime)
    add_dependencies(symbian_probe_index symbian_index_${probe})
  endforeach()
  target_include_directories(symbian_index_runtime PRIVATE
    "${CMAKE_SOURCE_DIR}/cpp/symbian/concurrency/guest"
    "${CMAKE_SOURCE_DIR}/cpp/symbian/concurrency/common")
  include(symbian/project/templates/SymbianApp.cmake)
  add_subdirectory(cpp/symbian/portable/zlib EXCLUDE_FROM_ALL)
  symbian_workspace_archive(SymbianPortableZlib symbian_portable_zlib)
  add_subdirectory(cpp/symbian/portable/png EXCLUDE_FROM_ALL)
  symbian_workspace_archive(SymbianPortablePng symbian_portable_png)
  add_subdirectory(cpp/symbian/portable/jpeg EXCLUDE_FROM_ALL)
  symbian_workspace_archive(SymbianPortableJpeg symbian_portable_jpeg)
  add_subdirectory(cpp/symbian/portable/freetype EXCLUDE_FROM_ALL)
  symbian_workspace_archive(SymbianPortableFreeType freetype)
  # Component CMake files own the actual guest library sources and include
  # paths. Loading them here makes SDK internals navigable in the same ARM
  # profile as applications, without per-file IDE registration.
  add_subdirectory(cpp/symbian/api EXCLUDE_FROM_ALL)
  add_subdirectory(cpp/symbian/portable/sdl2 EXCLUDE_FROM_ALL)
  add_dependencies(symbian_probe_index symbian_portable_sdl2)
  add_subdirectory(cpp/symbian/portable/sdl3 EXCLUDE_FROM_ALL)
  add_dependencies(symbian_probe_index symbian_portable_sdl3)
  include(cmake/SymbianGuestHeaderCanaries.cmake)
  symbian_guest_sdl_header_canaries("${CMAKE_SOURCE_DIR}")
  include(cmake/SymbianWorkspace.cmake)
  symbian_workspace_components()
  set(SYMBIAN_MBEDTLS_OPENSSL_COMPAT OFF)
  add_subdirectory(third_party/mbedtls-symbian EXCLUDE_FROM_ALL)
  add_library(Symbian::Crypto ALIAS mbedcrypto)
  add_library(symbian_index_entropy OBJECT EXCLUDE_FROM_ALL
    probes/entropy_probe/probe.cc probes/entropy_probe/startup.cc)
  target_link_libraries(symbian_index_entropy PRIVATE
    symbian_guest_runtime MbedTLS::mbedcrypto)
  add_dependencies(symbian_probe_index symbian_index_entropy)
  set_property(TARGET SymbianTls PROPERTY INTERFACE_LINK_LIBRARIES
    "Symbian::Connectivity;MbedTLS::mbedtls;MbedTLS::mbedx509;MbedTLS::mbedcrypto")
  add_subdirectory(cpp/symbian/tls EXCLUDE_FROM_ALL)
  symbian_workspace_archive(SymbianTls symbian_api_tls)
  add_library(symbian_index_http OBJECT EXCLUDE_FROM_ALL
    probes/http_probe/probe.cc)
  target_link_libraries(symbian_index_http PRIVATE Symbian::Http Symbian::Tls
    Symbian::Storage)
  add_dependencies(symbian_probe_index symbian_index_http)
  add_subdirectory(cpp/symbian/concurrency/guest EXCLUDE_FROM_ALL)
  symbian_workspace_archive(SymbianGuestFibers symbian_guest_fiber)
  # The stackless runtime probes now use installed guest StatusOr and time.
  target_link_libraries(symbian_index_runtime PRIVATE
                        Symbian::AbseilStatusOr)
  if(NOT TARGET Symbian::Connectivity OR NOT TARGET Symbian::Stackless)
    message(FATAL_ERROR
      "SDK at ${SYMBIAN_SDK_PREFIX} lacks Connectivity or Stackless. "
      "Install and activate an SDK with these targets, then reload the "
      "CLion CMake profile")
  endif()
  set(connectivity_dir "${CMAKE_SOURCE_DIR}/probes/connectivity_probe")
  file(GLOB connectivity_sources CONFIGURE_DEPENDS
       "${connectivity_dir}/*.cc" "${connectivity_dir}/*.S")
  add_library(symbian_index_connectivity OBJECT EXCLUDE_FROM_ALL
              ${connectivity_sources})
  target_link_libraries(symbian_index_connectivity PRIVATE
                        Symbian::Connectivity Symbian::Stackless)
  add_dependencies(symbian_probe_index symbian_index_connectivity)
  foreach(probe IN ITEMS abseil_allocator abseil_status)
    set(probe_dir "${CMAKE_SOURCE_DIR}/probes/${probe}_probe")
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
  set(module_dir "${CMAKE_SOURCE_DIR}/probes/cxx20_module_probe")
  add_library(symbian_index_cxx20_module OBJECT EXCLUDE_FROM_ALL
              "${module_dir}/probe.cc")
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
    set(mbedtls_dir "${CMAKE_SOURCE_DIR}/probes/mbedtls_dll_probe")
    add_library(symbian_index_mbedtls OBJECT EXCLUDE_FROM_ALL
                "${mbedtls_dir}/bridge.c")
    target_link_libraries(symbian_index_mbedtls PRIVATE Symbian::Runtime)
    target_include_directories(symbian_index_mbedtls PRIVATE
      "${MBEDTLS_SOURCE}/include" "${CMAKE_CURRENT_BINARY_DIR}/mbedtls-fixtures")
    target_compile_definitions(symbian_index_mbedtls PRIVATE
      MBEDTLS_STATIC MBEDTLS_SSL_PROTO_TLS1_3 SYMBIAN_MBEDTLS_GUEST=1
      MBEDTLS_USER_CONFIG_FILE="symbian_mbedtls/config.h")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/mbedtls-fixtures")
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/mbedtls-fixtures/test_certificate.h" "")
    foreach(fixture IN ITEMS "server-cert.pem;kTestCertificate"
                             "expired-cert.pem;kExpiredCertificate"
                             "server-key.pem;kTestPrivateKey")
      list(GET fixture 0 fixture_file)
      list(GET fixture 1 fixture_name)
      file(READ "${MBEDTLS_SOURCE}/tests/fixtures/${fixture_file}" fixture_text)
      string(REPLACE "\n" "\\n\"\n\"" fixture_c "${fixture_text}")
      file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/mbedtls-fixtures/test_certificate.h"
        "static const char ${fixture_name}[] = \"${fixture_c}\";\n")
    endforeach()
    add_dependencies(symbian_probe_index symbian_index_mbedtls)
  endif()
  set_source_files_properties(
    "${CMAKE_SOURCE_DIR}/probes/runtime_probe/locale_api_contract.cc"
    PROPERTIES COMPILE_DEFINITIONS _LIBCPP_BUILDING_LIBRARY)
  set_source_files_properties(
    "${CMAKE_SOURCE_DIR}/probes/runtime_probe/exception_contract.cc"
    "${CMAKE_SOURCE_DIR}/probes/runtime_probe/exception_metadata_contract.cc"
    PROPERTIES COMPILE_OPTIONS "-fexceptions;-frtti;-fno-pic")
  include(cmake/SymbianNativeExamples.cmake)
  # Registered applications own their actual CMake targets. Discover root
  # projects, examples and probes rather than adding a rule per app.
  file(GLOB application_manifests CONFIGURE_DEPENDS
       "${CMAKE_SOURCE_DIR}/*/symbian.toml"
       "${CMAKE_SOURCE_DIR}/examples/*/symbian.toml"
       "${CMAKE_SOURCE_DIR}/probes/*/symbian.toml")
  foreach(application_manifest IN LISTS application_manifests)
    get_filename_component(application_dir "${application_manifest}" DIRECTORY)
    if(EXISTS "${application_dir}/CMakeLists.txt")
      file(READ "${application_manifest}" application_manifest_text)
      get_filename_component(application_name "${application_dir}" NAME)
      if(NOT application_manifest_text MATCHES "\\[application\\]" AND
          NOT application_name IN_LIST SYMBIAN_NATIVE_EXAMPLES)
        continue()
      endif()
      string(REGEX MATCHALL
        "proxies/[A-Za-z0-9_-]+/[A-Za-z0-9_-]+\\.dso"
        application_proxy_paths "${application_manifest_text}")
      set(SYMBIAN_IMPORT_PROXIES)
      foreach(application_proxy IN LISTS application_proxy_paths)
        list(APPEND SYMBIAN_IMPORT_PROXIES
             "${SYMBIAN_SDK_PREFIX}/${application_proxy}")
      endforeach()
      list(REMOVE_DUPLICATES SYMBIAN_IMPORT_PROXIES)
      add_subdirectory("${application_dir}" EXCLUDE_FROM_ALL)
    endif()
  endforeach()
  add_subdirectory(probes/native_pthread_probe EXCLUDE_FROM_ALL)
  include(cmake/SymbianGuestHeaderCanaries.cmake)
  symbian_guest_shared_header_canaries("${CMAKE_SOURCE_DIR}")
  symbian_index_remaining_native(symbian_index_remaining_guest
    Symbian::AbseilStatusOr cpp/symbian agent_service examples probes)
  return()
endif()

# Release wheels reuse the ABI-independent host build for this OS/architecture.
set(SYMBIAN_PREBUILT_HOST_SDK "" CACHE PATH "Installed host SDK for binding-only builds")
if(SYMBIAN_PREBUILT_HOST_SDK)
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
  set(CMAKE_CXX_VISIBILITY_PRESET hidden)
  set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
  find_package(SymbianHost ${PROJECT_VERSION} EXACT CONFIG REQUIRED
    PATHS "${SYMBIAN_PREBUILT_HOST_SDK}" NO_DEFAULT_PATH)
  include(FetchContent)
  add_subdirectory(cpp/python)
  install(DIRECTORY "${SYMBIAN_PREBUILT_HOST_SDK}/symbian/licenses/"
    DESTINATION symbian/licenses)
  return()
endif()
option(SYMBIAN_INSTALL_HOST_SDK "Install the complete static host library closure" OFF)
if(SYMBIAN_INSTALL_HOST_SDK)
  set(ZLIB_USE_STATIC_LIBS ON)
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
  target_link_libraries(${target} PUBLIC absl::nullability)
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
add_subdirectory(cpp/symbian/net)
add_subdirectory(cpp/symbian/http)
add_subdirectory(cpp/symbian/websocket)
add_subdirectory(cpp/symbian/concurrency)
add_subdirectory(cpp/symbian/analysis)
add_subdirectory(cpp/symbian/e32)
add_subdirectory(cpp/symbian/sis)
add_subdirectory(cpp/symbian/sdk)
add_subdirectory(cpp/symbian/device)
add_subdirectory(cpp/symbian/tools)
include(cmake/SymbianHostHeaderCanaries.cmake)
if(SYMBIAN_INSTALL_HOST_SDK)
  include(cmake/SymbianHostSdk.cmake)
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
install(FILES "${SYMBIAN_NGHTTP2_SOURCE}/COPYING"
        DESTINATION symbian/licenses RENAME nghttp2-LICENSE)
install(FILES third_party/boost/LICENSE_1_0.txt
        DESTINATION symbian/licenses RENAME Boost-LICENSE)
install(FILES third_party/stb_sprintf/LICENSE
        DESTINATION symbian/licenses RENAME stb_sprintf-LICENSE)
install(FILES "${abseil_SOURCE_DIR}/LICENSE"
        DESTINATION symbian/licenses RENAME Abseil-LICENSE)
install(FILES "${nlohmann_json_SOURCE_DIR}/LICENSE.MIT"
        DESTINATION symbian/licenses RENAME nlohmann-json-LICENSE)

option(SYMBIAN_BUILD_GUI_EXAMPLE "Expose the prepared ARM GUI in the host project" ON)
set(SYMBIAN_GUI_SDK_INCLUDE "${CMAKE_SOURCE_DIR}/.symbian/gui-sdk/include"
    CACHE PATH "Materialized/staged Symbian GUI headers")
if(SYMBIAN_BUILD_GUI_EXAMPLE)
  if(EXISTS "${SYMBIAN_GUI_SDK_INCLUDE}/w32std.h")
    include(symbian/toolchain/cmake/SymbianSdk.cmake)
    symbian_add_guest_subdirectory(examples/gui_app)
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
    message(STATUS "GUI target unavailable: prepare the source platform headers")
  endif()
endif()
foreach(application IN ITEMS gl_app qt_app_classic sdl2_app)
  if(SYMBIAN_BUILD_GUI_EXAMPLE)
    include(symbian/toolchain/cmake/SymbianSdk.cmake)
    symbian_add_guest_subdirectory(examples/${application})
  endif()
  add_executable(${application}_run EXCLUDE_FROM_ALL
                 cpp/symbian/emulator/launcher.cc)
  set_target_properties(${application}_run PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build/debug")
  symbian_native_library(${application}_run)
  target_compile_definitions(${application}_run PRIVATE
    SYMBIAN_LAUNCH_PYTHON="${CMAKE_SOURCE_DIR}/.venv/bin/python"
    SYMBIAN_LAUNCH_ROOT="${CMAKE_SOURCE_DIR}"
    SYMBIAN_LAUNCH_PROJECT="${CMAKE_SOURCE_DIR}/examples/${application}")
endforeach()
include(cmake/SymbianNativeExamples.cmake)
option(SYMBIAN_BUILD_NATIVE_EXAMPLES "Expose original native API and library examples" ON)
if(SYMBIAN_BUILD_NATIVE_EXAMPLES)
  include(symbian/toolchain/cmake/SymbianSdk.cmake)
  foreach(application IN LISTS SYMBIAN_NATIVE_EXAMPLES)
    symbian_add_guest_subdirectory(examples/${application})
  endforeach()
endif()
symbian_index_remaining_native(symbian_index_remaining_host absl::statusor
  cpp)
