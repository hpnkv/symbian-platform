# Status/StatusOr runtime integration ported from A11; see .dev/a11-status.md.
find_package(Python COMPONENTS Interpreter Development.Module REQUIRED)
set(Python3_EXECUTABLE "${Python_EXECUTABLE}")
find_package(Python3 COMPONENTS Interpreter Development.Module REQUIRED)
find_package(pybind11 3.0 CONFIG REQUIRED)

# A package manager's include prefix can contain a second Abseil ABI. Fence
# pybind11 headers off in the same generated overlay that A11 uses.
get_target_property(symbian_pybind_headers pybind11::pybind11_headers
                    INTERFACE_INCLUDE_DIRECTORIES)
set(symbian_pybind_overlay "${CMAKE_CURRENT_BINARY_DIR}/pybind11_include_overlay")
file(MAKE_DIRECTORY "${symbian_pybind_overlay}")
foreach(header_root IN LISTS symbian_pybind_headers)
  if(EXISTS "${header_root}/pybind11" AND
     NOT EXISTS "${symbian_pybind_overlay}/pybind11")
    file(CREATE_LINK "${header_root}/pybind11"
         "${symbian_pybind_overlay}/pybind11" SYMBOLIC)
  endif()
endforeach()
if(NOT EXISTS "${symbian_pybind_overlay}/pybind11")
  message(FATAL_ERROR "Cannot isolate pybind11 headers")
endif()
set_target_properties(pybind11::pybind11_headers PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${symbian_pybind_overlay}")
set(pybind11_INCLUDE_DIRS "${symbian_pybind_overlay};${Python3_INCLUDE_DIRS}")

set(USE_SYSTEM_ABSEIL ON CACHE BOOL "" FORCE)
set(USE_SYSTEM_PYBIND ON CACHE BOOL "" FORCE)
set(symbian_saved_testing "${BUILD_TESTING}")
set(BUILD_TESTING OFF)
set(SYMBIAN_PYBIND11_ABSEIL_SOURCE_DIR
    "$ENV{SYMBIAN_PYBIND11_ABSEIL_SOURCE_DIR}" CACHE PATH
    "Optional checkout of A11's pinned pybind11_abseil revision")
if(SYMBIAN_PYBIND11_ABSEIL_SOURCE_DIR)
  FetchContent_Declare(pybind11_abseil SOURCE_DIR
                      "${SYMBIAN_PYBIND11_ABSEIL_SOURCE_DIR}")
else()
  FetchContent_Declare(pybind11_abseil
    URL https://github.com/pybind/pybind11_abseil/archive/dba6e38b79d93bc718e28b8f8ea2e8b00b60cdec.tar.gz
    URL_HASH SHA256=c54a260dcb34ad2d83cdb273855f79ac4f98a34130aae1ddbf39da2d771f4090
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE SYSTEM)
endif()
FetchContent_MakeAvailable(pybind11_abseil)
set(BUILD_TESTING "${symbian_saved_testing}")
# The pinned upstream project omits these direct dependencies. A11's larger
# core happens to provide them; this SDK must link them explicitly.
target_link_libraries(py_base_utilities PUBLIC absl::absl_check absl::absl_log)
target_link_libraries(status_from_py_exc PUBLIC absl::absl_check absl::absl_log)
target_link_libraries(status_from_core_py_exc PUBLIC absl::flat_hash_map)
foreach(runtime_target IN ITEMS import_status_module ok_status_singleton
        ok_status_singleton_lib ok_status_singleton_pyinit_google3
        py_base_utilities register_status_bindings status_from_core_py_exc
        status_from_py_exc status_py_extension_stub status_pyinit_google3
        utils_pybind11_absl void_ptr_from_capsule)
  if(TARGET ${runtime_target})
    target_include_directories(${runtime_target} BEFORE PRIVATE
                               "${abseil_SOURCE_DIR}")
  endif()
endforeach()

pybind11_add_module(symbian_python MODULE module.cc agent_bindings.cc status_bindings.cc
                    status_interop.cc concurrency_interop.cc device_bindings.cc)
set_target_properties(symbian_python PROPERTIES OUTPUT_NAME _native)
target_include_directories(symbian_python BEFORE PRIVATE "${abseil_SOURCE_DIR}")
target_link_libraries(symbian_python PRIVATE symbian::sis symbian::sdk
                      symbian::status symbian::concurrency symbian::device
                      symbian::agent_frame
                      absl::log_initialize
                      pybind11_abseil::status_casters)
target_compile_features(symbian_python PRIVATE cxx_std_20)
# Only Python boundary translation units enable exceptions. Hide static vendor
# symbols on ELF as A11 does, avoiding interposition with CPython's libraries.
target_compile_options(symbian_python PRIVATE
  $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-fexceptions>)
target_link_options(symbian_python PRIVATE
                    $<$<PLATFORM_ID:Linux>:-Wl,--exclude-libs,ALL>)
install(TARGETS symbian_python LIBRARY DESTINATION symbian)
install(TARGETS status_py_extension_stub ok_status_singleton
        LIBRARY DESTINATION pybind11_abseil)
install(FILES "${pybind11_abseil_SOURCE_DIR}/LICENSE"
        DESTINATION symbian/licenses RENAME pybind11_abseil-LICENSE)
install(FILES "${PROJECT_SOURCE_DIR}/third_party/pybind11/LICENSE"
        DESTINATION symbian/licenses RENAME pybind11-LICENSE)

install(FILES "${SYMBIAN_LIBUSB_LICENSE}" DESTINATION symbian/licenses
        RENAME libusb-COPYING)
install(FILES "${SYMBIAN_LIBUSB_STATIC}" DESTINATION symbian/host
        RENAME libusb-1.0.a)
install(FILES "${SYMBIAN_LIBUSB_INCLUDE_DIR}/libusb.h"
        DESTINATION symbian/host/include RENAME libusb.h)
