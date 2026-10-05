include_guard(GLOBAL)
if(TARGET Symbian::Runtime)
  return()
endif()
include(SymbianPic)
if(NOT SYMBIAN_WORKSPACE_BUILD)
  get_filename_component(SYMBIAN_SDK_PREFIX
    "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
endif()
include(SymbianSdk)
symbian_select_sdk()
if(NOT SYMBIAN_WORKSPACE_BUILD)
  # CMake reloads its saved compiler description during project(). Refresh
  # driver paths afterwards too, so changing the SDK selector updates Ninja.
  include("${SYMBIAN_SDK_PREFIX}/cmake/symbian-arm.cmake")
endif()
# Original OS libraries expose their full frozen ABI. Applications name targets;
# only symbols used by the linker become dependencies in the E32 image.
foreach(pair IN ITEMS "EUser:euser" "WindowServer:ws32" "Gdi:gdi" "Hal:hal"
    "FileServer:efsrv" "SocketServer:esock" "Internet:insock" "C:libc"
    "Math:libm" "Pthread:libpthread" "CxxAbi:drtaeabi" "CameraNative:ecam")
  string(REPLACE ":" ";" fields "${pair}")
  list(GET fields 0 name)
  list(GET fields 1 dll)
  set(proxy "${SYMBIAN_SDK_PREFIX}/proxies/${dll}/${dll}.dso")
  if(EXISTS "${proxy}")
    add_library(Symbian${name} SHARED IMPORTED GLOBAL)
    set_target_properties(Symbian${name} PROPERTIES IMPORTED_LOCATION "${proxy}")
    target_include_directories(Symbian${name} SYSTEM INTERFACE
      "${SYMBIAN_SDK_PREFIX}/include/platform")
    target_compile_definitions(Symbian${name} INTERFACE _UNICODE __GCC32__
      __GCCV3__ __EABI__ __EPOC32__ __MARM__ __MARM_ARMV5__)
    target_compile_options(Symbian${name} INTERFACE -fshort-wchar)
    add_library(Symbian::${name} ALIAS Symbian${name})
  endif()
endforeach()
set(SYMBIAN_CA_BUNDLE "" CACHE STRING
    "Project PEM CA bundle; empty means no packaged trust roots")
if(SYMBIAN_CA_BUNDLE)
  if(IS_ABSOLUTE "${SYMBIAN_CA_BUNDLE}")
    set(symbian_ca_candidate "${SYMBIAN_CA_BUNDLE}")
  else()
    set(symbian_ca_candidate
      "${CMAKE_SOURCE_DIR}/${SYMBIAN_CA_BUNDLE}")
  endif()
  file(REAL_PATH "${symbian_ca_candidate}" symbian_ca_bundle)
  cmake_path(IS_PREFIX CMAKE_SOURCE_DIR "${symbian_ca_bundle}" NORMALIZE
    symbian_ca_inside_project)
  if(NOT symbian_ca_inside_project OR NOT EXISTS "${symbian_ca_bundle}" OR
     IS_DIRECTORY "${symbian_ca_bundle}")
    message(FATAL_ERROR "SYMBIAN_CA_BUNDLE must name a project PEM file")
  endif()
  file(SIZE "${symbian_ca_bundle}" symbian_ca_size)
  if(symbian_ca_size EQUAL 0 OR symbian_ca_size GREATER 262144)
    message(FATAL_ERROR "SYMBIAN_CA_BUNDLE must be 1..262144 bytes")
  endif()
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${symbian_ca_bundle}")
  file(SHA256 "${symbian_ca_bundle}" SYMBIAN_CA_BUNDLE_SHA256)
  set(SYMBIAN_CA_BUNDLE "${symbian_ca_bundle}" CACHE STRING
      "Project PEM CA bundle; empty means no packaged trust roots" FORCE)
  file(STRINGS "${CMAKE_SOURCE_DIR}/symbian.toml" symbian_executable_line
    REGEX "^executable_name[ \t]*=[ \t]*\"[A-Za-z][A-Za-z0-9_-]*\\.exe\"[ \t]*$")
  if(NOT symbian_executable_line OR
     NOT symbian_executable_line MATCHES "\"([A-Za-z][A-Za-z0-9_-]*)\\.exe\"")
    message(FATAL_ERROR "SYMBIAN_CA_BUNDLE needs symbian.toml executable_name")
  endif()
  set(SYMBIAN_CA_BUNDLE_PACKAGED_PATH
    "\\resource\\apps\\${CMAKE_MATCH_1}_ca.pem")
  message(STATUS "Project CA bundle SHA-256: ${SYMBIAN_CA_BUNDLE_SHA256}")
endif()
if(NOT DEFINED SYMBIAN_TARGET_ARCH)
  set(SYMBIAN_TARGET_ARCH armv6)
endif()
if(EXISTS
    "${SYMBIAN_SDK_PREFIX}/lib/${SYMBIAN_TARGET_ARCH}/mbedtls/lib/cmake/MbedTLS/MbedTLSConfig.cmake")
  list(PREPEND CMAKE_PREFIX_PATH
    "${SYMBIAN_SDK_PREFIX}/lib/${SYMBIAN_TARGET_ARCH}/mbedtls")
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
  __SYMBIAN32__ __LONG_LONG_SUPPORTED _POSIX_C_SOURCE=200112L
  SYMBIAN_RUNTIME_MIMALLOC=1)
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
target_link_libraries(SymbianRuntime INTERFACE
  "${SYMBIAN_SDK_PREFIX}/proxies/libpthread/libpthread.dso")

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
    if(property STREQUAL "INTERFACE_COMPILE_DEFINITIONS")
      list(REMOVE_ITEM value SYMBIAN_RUNTIME_MIMALLOC=1)
    endif()
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
    _LIBCPP_PROVIDES_DEFAULT_RUNE_TABLE SYMBIAN_RUNTIME_MIMALLOC=1)
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

# Each verified device capability is a separate opt-in archive. Absent
# archives create no target, so a project cannot accidentally link a planned
# but unimplemented device facility.
foreach(component IN ITEMS system connectivity agent power media display sensors
                           camera storage)
  set(component_archive
    "${SYMBIAN_SDK_PREFIX}/lib/${SYMBIAN_TARGET_ARCH}/libsymbian_api_${component}.a")
  if((EXISTS "${component_archive}" OR
      (SYMBIAN_WORKSPACE_BUILD AND EXISTS
       "${CMAKE_SOURCE_DIR}/cpp/symbian/api/${component}/CMakeLists.txt"))
     AND TARGET Symbian::AbseilStatusOr)
    string(SUBSTRING "${component}" 0 1 component_initial)
    string(TOUPPER "${component_initial}" component_initial)
    string(SUBSTRING "${component}" 1 -1 component_rest)
    set(component_name "${component_initial}${component_rest}")
    set(component_target "SymbianApi${component_name}")
    add_library(${component_target} STATIC IMPORTED)
    set_target_properties(${component_target} PROPERTIES
      IMPORTED_LOCATION "${component_archive}")
    target_link_libraries(${component_target} INTERFACE
      Symbian::AbseilStatusOr)
    if(component STREQUAL "power" OR component STREQUAL "display")
      target_link_libraries(${component_target} INTERFACE
        "${SYMBIAN_SDK_PREFIX}/proxies/hal/hal.dso")
    endif()
    if(component STREQUAL "display")
      target_link_libraries(${component_target} INTERFACE
        "${SYMBIAN_SDK_PREFIX}/proxies/ws32/ws32.dso"
        "${SYMBIAN_SDK_PREFIX}/proxies/gdi/gdi.dso")
    endif()
    if(component STREQUAL "storage")
      target_link_libraries(${component_target} INTERFACE
        "${SYMBIAN_SDK_PREFIX}/proxies/efsrv/efsrv.dso")
    endif()
    if(component STREQUAL "connectivity")
      target_link_libraries(${component_target} INTERFACE
        "${SYMBIAN_SDK_PREFIX}/proxies/esock/esock.dso"
        "${SYMBIAN_SDK_PREFIX}/proxies/insock/insock.dso")
    endif()
    if(component STREQUAL "camera")
      target_link_libraries(${component_target} INTERFACE
        "${SYMBIAN_SDK_PREFIX}/proxies/ecam/ecam.dso")
    endif()
    add_library(Symbian::${component_name} ALIAS ${component_target})
  endif()
endforeach()

# RFC 8441 WebSocket codec and worker-facing client/server wrappers.
set(http_archive "${SYMBIAN_SDK_PREFIX}/lib/${SYMBIAN_TARGET_ARCH}/libsymbian_http.a")
if((EXISTS "${http_archive}" OR SYMBIAN_WORKSPACE_BUILD)
   AND TARGET Symbian::Connectivity)
  add_library(SymbianHttp STATIC IMPORTED GLOBAL)
  set_target_properties(SymbianHttp PROPERTIES IMPORTED_LOCATION "${http_archive}")
  target_link_libraries(SymbianHttp INTERFACE Symbian::Connectivity
    "${SYMBIAN_SDK_PREFIX}/lib/${SYMBIAN_TARGET_ARCH}/libsymbian_nghttp2.a")
  add_library(Symbian::Http ALIAS SymbianHttp)
endif()

set(websocket_archive "${SYMBIAN_SDK_PREFIX}/lib/${SYMBIAN_TARGET_ARCH}/libsymbian_api_websocket.a")
if((EXISTS "${websocket_archive}" OR SYMBIAN_WORKSPACE_BUILD)
   AND TARGET Symbian::Connectivity)
  add_library(SymbianWebSocket STATIC IMPORTED)
  set_target_properties(SymbianWebSocket PROPERTIES IMPORTED_LOCATION "${websocket_archive}")
  target_link_libraries(SymbianWebSocket INTERFACE Symbian::Connectivity
    "${SYMBIAN_SDK_PREFIX}/lib/${SYMBIAN_TARGET_ARCH}/libsymbian_websocket.a")
  if(TARGET Symbian::Http)
    target_link_libraries(SymbianWebSocket INTERFACE Symbian::Http)
  else()
    target_link_libraries(SymbianWebSocket INTERFACE
      "${SYMBIAN_SDK_PREFIX}/lib/${SYMBIAN_TARGET_ARCH}/libsymbian_nghttp2.a")
  endif()
  add_library(Symbian::WebSocket ALIAS SymbianWebSocket)
endif()

# The C++ TLS owner is opt-in and keeps Mbed TLS out of plain TCP projects.
set(crypto_archive
  "${SYMBIAN_SDK_PREFIX}/lib/${SYMBIAN_TARGET_ARCH}/mbedtls/lib/libmbedcrypto.a")
if(EXISTS "${crypto_archive}")
  add_library(SymbianCrypto STATIC IMPORTED)
  set_target_properties(SymbianCrypto PROPERTIES IMPORTED_LOCATION "${crypto_archive}")
  add_library(Symbian::Crypto ALIAS SymbianCrypto)
endif()
set(tls_archive
  "${SYMBIAN_SDK_PREFIX}/lib/${SYMBIAN_TARGET_ARCH}/libsymbian_api_tls.a")
if((EXISTS "${tls_archive}" OR SYMBIAN_WORKSPACE_BUILD)
   AND TARGET Symbian::Connectivity)
  set(tls_mbed_lib
    "${SYMBIAN_SDK_PREFIX}/lib/${SYMBIAN_TARGET_ARCH}/mbedtls/lib")
  add_library(SymbianTls STATIC IMPORTED)
  set_target_properties(SymbianTls PROPERTIES IMPORTED_LOCATION "${tls_archive}")
  target_link_libraries(SymbianTls INTERFACE Symbian::Connectivity
    "${tls_mbed_lib}/libmbedtls.a"
    "${tls_mbed_lib}/libmbedx509.a"
    "${tls_mbed_lib}/libmbedcrypto.a")
  add_library(Symbian::Tls ALIAS SymbianTls)
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
    if(EXISTS "${fiber_archive}" OR SYMBIAN_WORKSPACE_BUILD)
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

# Guest Qt is an original OS DLL dependency, distinct from host emulator Qt.
# Its headers need the SDK's standard C++ allocation declarations first.
if(EXISTS "${SYMBIAN_SDK_PREFIX}/include/qt4/QtCore/qglobal.h")
  foreach(module IN ITEMS Core Gui)
    string(TOLOWER "qt${module}" dll)
    add_library(SymbianQt${module} SHARED IMPORTED GLOBAL)
    set_target_properties(SymbianQt${module} PROPERTIES IMPORTED_LOCATION
      "${SYMBIAN_SDK_PREFIX}/proxies/${dll}/${dll}.dso")
    target_include_directories(SymbianQt${module} SYSTEM INTERFACE
      "${SYMBIAN_SDK_PREFIX}/include/qt4"
      "${SYMBIAN_SDK_PREFIX}/include/qt4/QtCore")
    target_compile_definitions(SymbianQt${module} INTERFACE
      QT_KEYPAD_NAVIGATION QT_SOFTKEYS_ENABLED)
    target_compile_options(SymbianQt${module} INTERFACE "SHELL:-fPIC"
      "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include ${SYMBIAN_SDK_PREFIX}/cmake/qt_compat.h>")
    add_library(Symbian::Qt${module} ALIAS SymbianQt${module})
  endforeach()
  target_link_libraries(SymbianQtCore INTERFACE Symbian::EUser)
  target_sources(SymbianQtCore INTERFACE
    "${SYMBIAN_SDK_PREFIX}/share/symbian/qt/newallocator_hook.cpp")
  target_link_libraries(SymbianQtGui INTERFACE Symbian::QtCore)
endif()
