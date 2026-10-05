cmake_minimum_required(VERSION 3.28)
project(abseil_status_probe LANGUAGES C CXX ASM)

option(SYMBIAN_ABSEIL_USE_SDK "Link installed guest Abseil target" OFF)
if(NOT EXISTS "${SYMBIAN_SDK_PREFIX}/cmake/SymbianApp.cmake")
  message(FATAL_ERROR "SYMBIAN_SDK_PREFIX must name an installed SDK")
endif()
include(SymbianApp)

if(NOT SYMBIAN_ABSEIL_USE_SDK)
if(NOT IS_DIRECTORY "${SYMBIAN_ABSEIL_SOURCE}")
  message(FATAL_ERROR "SYMBIAN_ABSEIL_SOURCE must be the prepared pinned source")
endif()
# The original Abseil targets consume the same configured guest headers and
# ABI as Symbian::Runtime. Absolute data references let E32 rebase code and
# writable data independently; PIC PC-relative text-to-data references do not.
include_directories(SYSTEM
  "${SYMBIAN_SDK_PREFIX}/include/stream-config"
  "${SYMBIAN_SDK_PREFIX}/include/config"
  "${SYMBIAN_SDK_PREFIX}/include/c++"
  "${SYMBIAN_SDK_PREFIX}/include/compiler"
  "${SYMBIAN_SDK_PREFIX}/include/platform"
  "${SYMBIAN_SDK_PREFIX}/include/openc"
  "${SYMBIAN_SDK_PREFIX}/include/libm"
  "${SYMBIAN_SDK_PREFIX}/include/libc"
  "${SYMBIAN_SDK_PREFIX}/include/pthread"
  "${SYMBIAN_SDK_PREFIX}/include/posix4"
  "${SYMBIAN_SDK_PREFIX}/include")
add_compile_definitions(
  _LIBCPP_PROVIDES_DEFAULT_RUNE_TABLE
  _UNICODE __GCC32__ __GCCV3__ __EABI__ __EPOC32__ __MARM__
  __MARM_ARMV5__ __SYMBIAN32__ __LONG_LONG_SUPPORTED
  _POSIX_C_SOURCE=200112L)
add_compile_options(-fshort-wchar -fno-pic -fno-exceptions -fno-rtti
  "$<$<COMPILE_LANGUAGE:CXX>:-include;${SYMBIAN_SDK_PREFIX}/include/c++/iosfwd>")
set(ABSL_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(ABSL_PROPAGATE_CXX_STD ON CACHE BOOL "" FORCE)
set(CMAKE_HAVE_LIBC_PTHREAD 1 CACHE INTERNAL "Guest pthread symbols" FORCE)
add_subdirectory("${SYMBIAN_ABSEIL_SOURCE}" "${CMAKE_BINARY_DIR}/abseil"
                 EXCLUDE_FROM_ALL)
endif()

symbian_add_executable(abseil_status_probe contract.cc)
if(NOT SYMBIAN_ABSEIL_USE_SDK)
  target_include_directories(abseil_status_probe PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../cpp/symbian/runtime")
endif()
set_target_properties(abseil_status_probe PROPERTIES SUFFIX ".elf")
target_link_options(abseil_status_probe PRIVATE
  --gc-sections --hash-style=sysv --no-dynamic-linker --as-needed
  --error-limit=0)
if(SYMBIAN_ABSEIL_USE_SDK)
  if(NOT TARGET Symbian::AbseilStatusOr)
    message(FATAL_ERROR "The selected SDK has no Abseil StatusOr profile")
  endif()
  target_link_libraries(abseil_status_probe PRIVATE Symbian::AbseilStatusOr)
else()
  target_link_libraries(abseil_status_probe PRIVATE
    absl::statusor absl::flat_hash_map
    "${SYMBIAN_SDK_PREFIX}/lib/${SYMBIAN_TARGET_ARCH}/libsymbian_guest_runtime_streams.a"
    "${SYMBIAN_SDK_PREFIX}/proxies/euser/euser.dso"
    "${SYMBIAN_SDK_PREFIX}/proxies/libc/libc.dso"
    "${SYMBIAN_SDK_PREFIX}/proxies/libpthread/libpthread.dso"
    "${SYMBIAN_SDK_PREFIX}/proxies/libm/libm.dso"
    "${SYMBIAN_SDK_PREFIX}/proxies/drtaeabi/drtaeabi.dso")
endif()
option(SYMBIAN_ABSEIL_CHANGED_STATUS "Fail the Abseil Status result" OFF)
if(SYMBIAN_ABSEIL_CHANGED_STATUS)
  target_compile_definitions(abseil_status_probe PRIVATE
    SYMBIAN_ABSEIL_CHANGED_STATUS=1)
endif()
