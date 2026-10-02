include(SymbianPic)
get_filename_component(SYMBIAN_SDK_PREFIX
  "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
if(NOT DEFINED SYMBIAN_TARGET_ARCH)
  set(SYMBIAN_TARGET_ARCH armv6)
endif()
set(runtime_archive "${SYMBIAN_SDK_PREFIX}/lib/${SYMBIAN_TARGET_ARCH}/libsymbian_guest_runtime.a")
if(NOT EXISTS "${runtime_archive}")
  message(FATAL_ERROR "SDK has no ${SYMBIAN_TARGET_ARCH} runtime; update the SDK or select a supported target")
endif()
add_library(SymbianRuntime STATIC IMPORTED)
set_target_properties(SymbianRuntime PROPERTIES
  IMPORTED_LOCATION "${runtime_archive}")
target_include_directories(SymbianRuntime SYSTEM INTERFACE
  "${SYMBIAN_SDK_PREFIX}/include"
  "${SYMBIAN_SDK_PREFIX}/include/config"
  "${SYMBIAN_SDK_PREFIX}/include/c++"
  "${SYMBIAN_SDK_PREFIX}/include/compiler"
  "${SYMBIAN_SDK_PREFIX}/include/platform"
  "${SYMBIAN_SDK_PREFIX}/include/openc"
  "${SYMBIAN_SDK_PREFIX}/include/libm"
  "${SYMBIAN_SDK_PREFIX}/include/libc"
  "${SYMBIAN_SDK_PREFIX}/include/pthread"
  "${SYMBIAN_SDK_PREFIX}/include/posix4")
target_compile_definitions(SymbianRuntime INTERFACE
  _UNICODE __GCC32__ __GCCV3__ __EABI__ __EPOC32__ __MARM__ __MARM_ARMV5__
  __SYMBIAN32__ __LONG_LONG_SUPPORTED _POSIX_C_SOURCE=200112L)
target_compile_options(SymbianRuntime INTERFACE
  -fno-pic -fshort-wchar -fvisibility=hidden -fno-exceptions
  $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
  -ffunction-sections -fdata-sections)
add_library(Symbian::Runtime ALIAS SymbianRuntime)
set(math_proxy "${SYMBIAN_SDK_PREFIX}/proxies/libm/libm.dso")
set(libc_proxy "${SYMBIAN_SDK_PREFIX}/proxies/libc/libc.dso")
# libc++ hash tables and clocks use these selected services. Ordinary apps
# must not acquire a new ROM dependency merely by linking Symbian::Runtime.
# CMake de-duplicates repeated --as-needed tokens when separate calls are
# flattened. Keep one contiguous scope or the second proxy becomes mandatory.
set(optional_runtime_proxies)
if(EXISTS "${math_proxy}")
  list(APPEND optional_runtime_proxies "${math_proxy}")
endif()
if(EXISTS "${libc_proxy}")
  list(APPEND optional_runtime_proxies "${libc_proxy}")
endif()
if(optional_runtime_proxies)
  target_link_libraries(SymbianRuntime INTERFACE
    --as-needed ${optional_runtime_proxies} --no-as-needed)
endif()

# Uses the ROM's 64-bit atomic entry points. This archive replaces the
# default runtime archive; select it only with a firmware whose EUSER exports
# and implementation have been verified. On ARMv6K those operations can use
# LDREXD/STREXD, while older ARM variants may have different implementation.
set(native64_archive "${SYMBIAN_SDK_PREFIX}/lib/${SYMBIAN_TARGET_ARCH}/libsymbian_guest_runtime_native_atomic64.a")
set(native64_proxy "${SYMBIAN_SDK_PREFIX}/proxies/euser-native64/euser.dso")
if(EXISTS "${native64_archive}" AND EXISTS "${native64_proxy}")
  add_library(SymbianNativeAtomics64 STATIC IMPORTED)
  set_target_properties(SymbianNativeAtomics64 PROPERTIES
    IMPORTED_LOCATION "${native64_archive}")
  foreach(property INTERFACE_INCLUDE_DIRECTORIES
      INTERFACE_COMPILE_DEFINITIONS INTERFACE_COMPILE_OPTIONS)
    get_target_property(value SymbianRuntime ${property})
    set_target_properties(SymbianNativeAtomics64 PROPERTIES ${property} "${value}")
  endforeach()
  target_link_libraries(SymbianNativeAtomics64 INTERFACE
    "${native64_proxy}" --as-needed "${math_proxy}"
    "${libc_proxy}" --no-as-needed)
  add_library(Symbian::NativeAtomics64 ALIAS SymbianNativeAtomics64)
endif()

# Complete alternate runtime archive with the classic-C locale and streams.
# Select this instead of Symbian::Runtime; the archives use different
# __config_site headers and must never be linked together.
set(stream_archive "${SYMBIAN_SDK_PREFIX}/lib/${SYMBIAN_TARGET_ARCH}/libsymbian_guest_runtime_streams.a")
set(stream_libc_proxy "${SYMBIAN_SDK_PREFIX}/proxies/libc/libc.dso")
if(EXISTS "${stream_archive}" AND EXISTS "${stream_libc_proxy}")
  add_library(SymbianStreams STATIC IMPORTED)
  set_target_properties(SymbianStreams PROPERTIES
    IMPORTED_LOCATION "${stream_archive}")
  target_include_directories(SymbianStreams SYSTEM INTERFACE
    "${SYMBIAN_SDK_PREFIX}/include/stream-config"
    "${SYMBIAN_SDK_PREFIX}/include"
    "${SYMBIAN_SDK_PREFIX}/include/config"
    "${SYMBIAN_SDK_PREFIX}/include/c++"
    "${SYMBIAN_SDK_PREFIX}/include/compiler"
    "${SYMBIAN_SDK_PREFIX}/include/platform"
    "${SYMBIAN_SDK_PREFIX}/include/openc"
    "${SYMBIAN_SDK_PREFIX}/include/libm"
    "${SYMBIAN_SDK_PREFIX}/include/libc"
    "${SYMBIAN_SDK_PREFIX}/include/pthread"
    "${SYMBIAN_SDK_PREFIX}/include/posix4")
  target_compile_definitions(SymbianStreams INTERFACE
    _UNICODE __GCC32__ __GCCV3__ __EABI__ __EPOC32__ __MARM__ __MARM_ARMV5__
    __SYMBIAN32__ __LONG_LONG_SUPPORTED _POSIX_C_SOURCE=200112L
    _LIBCPP_PROVIDES_DEFAULT_RUNE_TABLE)
  target_compile_options(SymbianStreams INTERFACE
    -fno-pic -fshort-wchar -fvisibility=hidden -fno-exceptions
    $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
    -ffunction-sections -fdata-sections)
  target_link_libraries(SymbianStreams INTERFACE
    --as-needed "${math_proxy}" --no-as-needed
    "${stream_libc_proxy}"
    "${SYMBIAN_SDK_PREFIX}/proxies/libpthread/libpthread.dso"
    "${SYMBIAN_SDK_PREFIX}/proxies/drtaeabi/drtaeabi.dso")
  add_library(Symbian::Streams ALIAS SymbianStreams)
endif()

# The guest Abseil Status/StatusOr closure is an alternate runtime profile.
# Its target supplies the pinned headers and matching stream libc++ archive.
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/SymbianAbseil.cmake")
  include("${CMAKE_CURRENT_LIST_DIR}/SymbianAbseil.cmake")
endif()

set(thread_proxy "${SYMBIAN_SDK_PREFIX}/proxies/libpthread/libpthread.dso")
set(cxxabi_proxy "${SYMBIAN_SDK_PREFIX}/proxies/drtaeabi/drtaeabi.dso")
if(EXISTS "${thread_proxy}" AND EXISTS "${cxxabi_proxy}")
  add_library(SymbianThreads INTERFACE)
  target_link_libraries(SymbianThreads INTERFACE
    Symbian::Runtime "${thread_proxy}" "${cxxabi_proxy}")
  add_library(Symbian::Threads ALIAS SymbianThreads)
  if(TARGET Symbian::AbseilStatusOr)
    # A11-derived Promise/Future/Task carry original guest StatusOr results.
    # The Abseil target owns the matching streams runtime and thread imports.
    # The public lock/channel headers can park a fiber, so their target must
    # include the fiber implementation whenever it is installed.
    add_library(SymbianStackless INTERFACE)
    target_link_libraries(SymbianStackless INTERFACE
      Symbian::AbseilStatusOr)
    add_library(Symbian::Stackless ALIAS SymbianStackless)
    add_library(Symbian::StacklessAbseil ALIAS SymbianStackless)
    set(fiber_archive
      "${SYMBIAN_SDK_PREFIX}/lib/${SYMBIAN_TARGET_ARCH}/libsymbian_guest_fiber.a")
    if(EXISTS "${fiber_archive}")
      add_library(SymbianGuestFibers STATIC IMPORTED)
      set_target_properties(SymbianGuestFibers PROPERTIES
        IMPORTED_LOCATION "${fiber_archive}")
      target_link_libraries(SymbianGuestFibers INTERFACE
        Symbian::AbseilStatusOr)
      add_library(Symbian::Fibers ALIAS SymbianGuestFibers)
      target_link_libraries(SymbianStackless INTERFACE
        Symbian::Fibers)
    endif()
  endif()
endif()

# Produces an ordinary ARM archive. Link it into an EXE or DLL to debug its
# source; the final E32 image owns the runtime mapping and symbols.
function(symbian_add_static_library target)
  cmake_parse_arguments(PARSE_ARGV 1 LIB "" "" "SOURCES")
  if(LIB_UNPARSED_ARGUMENTS OR LIB_KEYWORDS_MISSING_VALUES OR NOT LIB_SOURCES)
    message(FATAL_ERROR "symbian_add_static_library needs SOURCES")
  endif()
  if(NOT CMAKE_SYSTEM_NAME STREQUAL "Generic" OR
     NOT CMAKE_CXX_COMPILER_TARGET MATCHES "^arm(v5t|v6)-none-eabi$")
    message(FATAL_ERROR "Use the Symbian symbian-arm.cmake toolchain (armv6 or armv5t)")
  endif()
  set(sources)
  foreach(source IN LISTS LIB_SOURCES)
    _symbian_project_file(resolved "${source}")
    list(APPEND sources "${resolved}")
  endforeach()
  add_library(${target} STATIC ${sources})
  target_link_libraries(${target} PUBLIC Symbian::Runtime)
  target_compile_options(${target} PRIVATE -g -gdwarf-4
    "-fdebug-prefix-map=${CMAKE_BINARY_DIR}=/symbian-build/library")
  set_target_properties(${target} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endfunction()
