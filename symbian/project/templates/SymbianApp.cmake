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
include(SymbianPlatform)
include(SymbianGraphics)
include(SymbianNativeSurface)
include(SymbianPortable)
include(SymbianQtMobility)
set(SYMBIAN_CA_BUNDLE "" CACHE STRING
    "Project PEM CA bundle; empty means no packaged trust roots")
option(SYMBIAN_RUNTIME_LEGACY_EKA2
  "Use the installed ARMv5T older-EKA2 runtime for SDK components" OFF)
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
if(NOT SYMBIAN_WORKSPACE_INPUTS AND NOT EXISTS "${runtime_archive}")
  message(FATAL_ERROR "SDK has no ${SYMBIAN_TARGET_ARCH} runtime; update the SDK or select a supported target")
endif()
if(NOT EXISTS "${SYMBIAN_SDK_PREFIX}/cmake/native_cpp_compat.h")
  message(FATAL_ERROR "SDK lacks cmake/native_cpp_compat.h; update the SDK")
endif()
add_library(SymbianRuntime STATIC IMPORTED)
set_target_properties(SymbianRuntime PROPERTIES
  IMPORTED_LOCATION "${runtime_archive}" SYMBIAN_RUNTIME_PROFILE default)
target_include_directories(SymbianRuntime SYSTEM INTERFACE
  "${SYMBIAN_SDK_PREFIX}/include/abseil"
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
if(NOT SYMBIAN_WORKSPACE_INPUTS OR SYMBIAN_RUNTIME_MIMALLOC)
  target_compile_definitions(SymbianRuntime INTERFACE
    SYMBIAN_RUNTIME_MIMALLOC=1)
endif()
target_compile_options(SymbianRuntime INTERFACE
  -fno-pic -fshort-wchar -fvisibility=hidden -fno-exceptions
  $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
  "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include \"${SYMBIAN_SDK_PREFIX}/cmake/native_cpp_compat.h\">"
  -ffunction-sections -fdata-sections)
add_library(Symbian::Runtime ALIAS SymbianRuntime)
if(SYMBIAN_RUNTIME_LOCAL_MATH AND SYMBIAN_WORKSPACE_INPUTS)
  # EUSER's C memory primitives are available on the older ROMs.
  target_link_libraries(SymbianRuntime INTERFACE
    "${SYMBIAN_SDK_PREFIX}/proxies/euser/euser.dso")
endif()
set(math_proxy "${SYMBIAN_SDK_PREFIX}/proxies/libm/libm.dso")
set(libc_proxy "${SYMBIAN_SDK_PREFIX}/proxies/libc/libc.dso")
set(legacy_estlib_proxy
  "${SYMBIAN_SDK_PREFIX}/proxies/estlib-legacy/estlib.dso")
# libc++ hash tables and clocks use these selected services. Ordinary apps
# must not acquire a new ROM dependency merely by linking Symbian::Runtime.
# Executable and DLL helpers apply --as-needed to the complete target graph.
# Nested dependency targets must not restore --no-as-needed and introduce
# unused OS services into the published image.
set(optional_runtime_proxies)
if(SYMBIAN_RUNTIME_LEGACY_EUSER AND EXISTS "${legacy_estlib_proxy}")
  list(APPEND optional_runtime_proxies "${legacy_estlib_proxy}")
endif()
if(EXISTS "${math_proxy}")
  list(APPEND optional_runtime_proxies "${math_proxy}")
endif()
if(EXISTS "${libc_proxy}")
  list(APPEND optional_runtime_proxies "${libc_proxy}")
endif()
if(optional_runtime_proxies)
  target_link_libraries(SymbianRuntime INTERFACE ${optional_runtime_proxies})
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
    IMPORTED_LOCATION "${native64_archive}" SYMBIAN_RUNTIME_PROFILE atomic64)
  foreach(property INTERFACE_INCLUDE_DIRECTORIES
      INTERFACE_COMPILE_DEFINITIONS INTERFACE_COMPILE_OPTIONS)
    get_target_property(value SymbianRuntime ${property})
    if(property STREQUAL "INTERFACE_COMPILE_DEFINITIONS")
      list(REMOVE_ITEM value SYMBIAN_RUNTIME_MIMALLOC=1)
    endif()
    set_target_properties(SymbianNativeAtomics64 PROPERTIES ${property} "${value}")
  endforeach()
  target_link_libraries(SymbianNativeAtomics64 INTERFACE
    "${native64_proxy}" "${math_proxy}" "${libc_proxy}")
  add_library(Symbian::NativeAtomics64 ALIAS SymbianNativeAtomics64)
endif()

# Complete alternate runtime archive with the classic-C locale and streams.
# Select this instead of Symbian::Runtime; the archives use different
# __config_site headers and must never be linked together.
set(stream_archive "${SYMBIAN_SDK_PREFIX}/lib/${SYMBIAN_TARGET_ARCH}/libsymbian_guest_runtime_streams.a")
set(stream_libc_proxy "${SYMBIAN_SDK_PREFIX}/proxies/libc/libc.dso")
if((SYMBIAN_WORKSPACE_INPUTS OR EXISTS "${stream_archive}") AND EXISTS "${stream_libc_proxy}")
  add_library(SymbianStreams STATIC IMPORTED)
  set_target_properties(SymbianStreams PROPERTIES
    IMPORTED_LOCATION "${stream_archive}" SYMBIAN_RUNTIME_PROFILE streams)
  target_include_directories(SymbianStreams SYSTEM INTERFACE
    "${SYMBIAN_SDK_PREFIX}/include/abseil"
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
  if(NOT SYMBIAN_WORKSPACE_INPUTS OR SYMBIAN_RUNTIME_MIMALLOC)
    target_compile_definitions(SymbianStreams INTERFACE
      SYMBIAN_RUNTIME_MIMALLOC=1)
  endif()
  target_compile_options(SymbianStreams INTERFACE
    -fno-pic -fshort-wchar -fvisibility=hidden -fno-exceptions
    $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
    "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include \"${SYMBIAN_SDK_PREFIX}/cmake/native_cpp_compat.h\">"
    -ffunction-sections -fdata-sections)
  if(SYMBIAN_RUNTIME_LOCAL_MATH AND SYMBIAN_WORKSPACE_INPUTS)
    target_link_libraries(SymbianStreams INTERFACE
      "${SYMBIAN_SDK_PREFIX}/proxies/euser/euser.dso")
  endif()
  if(SYMBIAN_RUNTIME_LEGACY_EUSER AND EXISTS "${legacy_estlib_proxy}")
    target_link_libraries(SymbianStreams INTERFACE
      "${legacy_estlib_proxy}")
  endif()
  target_link_libraries(SymbianStreams INTERFACE
    "${math_proxy}"
    "${stream_libc_proxy}"
    "${SYMBIAN_SDK_PREFIX}/proxies/libpthread/libpthread.dso"
    "${SYMBIAN_SDK_PREFIX}/proxies/drtaeabi/drtaeabi.dso")
  add_library(Symbian::Streams ALIAS SymbianStreams)
endif()

# Complete ARMv5T older-EKA2 runtime. It uses the ROM's ESTLIB file service
# and SDK-owned C, pthread, math, text, formatting and time adapters. Keep
# this archive separate from both standard runtime configurations.
set(legacy_eka2_archive
  "${SYMBIAN_SDK_PREFIX}/lib/armv5t/libsymbian_guest_runtime_legacy_eka2.a")
if(SYMBIAN_TARGET_ARCH STREQUAL "armv5t" AND
   EXISTS "${legacy_eka2_archive}" AND TARGET SymbianStreams)
  if(NOT EXISTS "${legacy_estlib_proxy}")
    message(FATAL_ERROR "Legacy EKA2 runtime needs the ESTLIB import proxy")
  endif()
  add_library(SymbianLegacyEka2 STATIC IMPORTED)
  set_target_properties(SymbianLegacyEka2 PROPERTIES
    IMPORTED_LOCATION "${legacy_eka2_archive}"
    SYMBIAN_RUNTIME_PROFILE legacy_eka2)
  foreach(property IN ITEMS INTERFACE_INCLUDE_DIRECTORIES
      INTERFACE_COMPILE_DEFINITIONS INTERFACE_COMPILE_OPTIONS)
    get_target_property(value SymbianStreams ${property})
    if(property STREQUAL "INTERFACE_COMPILE_DEFINITIONS")
      list(REMOVE_ITEM value SYMBIAN_RUNTIME_MIMALLOC=1)
      list(APPEND value SYMBIAN_RUNTIME_LEGACY_EUSER=1)
    endif()
    set_target_properties(SymbianLegacyEka2 PROPERTIES ${property} "${value}")
  endforeach()
  target_link_libraries(SymbianLegacyEka2 INTERFACE
    "${SYMBIAN_SDK_PREFIX}/proxies/euser/euser.dso"
    "${legacy_estlib_proxy}"
    "${SYMBIAN_SDK_PREFIX}/proxies/drtaeabi/drtaeabi.dso")
  foreach(symbol IN ITEMS ceilf memchr strchr strcmp strcpy strncmp strcasecmp
      strncasecmp strncpy strlcat
      strlcpy strstr wcslen wmemchr malloc calloc realloc free getenv
      strerror_r isspace _exit asprintf snprintf vsnprintf strtod strtof
      fputwc getwc ungetwc gmtime_r localtime_r mktime strftime strptime perror
      clock_gettime pthread_mutex_lock mbrlen mbrtowc mbsnrtowcs mbsrtowcs
      mbtowc wcrtomb wcsnrtombs wcsrtombs mbsinit)
    target_link_options(SymbianLegacyEka2 INTERFACE "--undefined=${symbol}")
  endforeach()
  foreach(symbol IN ITEMS abs atof atoi qsort strtol strtoull strtold abort
      __assert)
    target_link_options(SymbianLegacyEka2 INTERFACE
      "$<$<NOT:$<BOOL:$<TARGET_PROPERTY:SYMBIAN_RUNTIME_MINIMAL_C_LINK>>>:--undefined=${symbol}>")
  endforeach()
  add_library(Symbian::LegacyEka2 ALIAS SymbianLegacyEka2)
endif()
if(SYMBIAN_RUNTIME_LEGACY_EKA2 AND NOT SYMBIAN_WORKSPACE_INPUTS AND
   NOT TARGET Symbian::LegacyEka2)
  message(FATAL_ERROR
    "The selected SDK lacks the ARMv5T older-EKA2 runtime profile")
endif()

if(SYMBIAN_WORKSPACE_INPUTS)
  include("${SYMBIAN_SOURCE_WORKSPACE}/cmake/SymbianWorkspace.cmake")
  symbian_workspace_archive(SymbianRuntime symbian_guest_runtime)
  symbian_workspace_archive(SymbianStreams symbian_guest_runtime)
  get_target_property(workspace_includes symbian_guest_runtime INTERFACE_INCLUDE_DIRECTORIES)
  foreach(runtime IN ITEMS SymbianRuntime SymbianStreams)
    set_property(TARGET ${runtime} PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${workspace_includes}")
    # The workspace builds one streams-capable runtime, shared by all consumers.
    set_property(TARGET ${runtime} PROPERTY SYMBIAN_RUNTIME_PROFILE streams)
  endforeach()
  if(SYMBIAN_RUNTIME_LOCAL_MATH)
    target_link_options(SymbianRuntime INTERFACE --undefined=ceilf)
    target_link_options(SymbianStreams INTERFACE --undefined=ceilf)
  endif()
  if(SYMBIAN_RUNTIME_LOCAL_C_STRING)
    foreach(symbol IN ITEMS memchr strchr strcmp strcpy strncmp strlcat
                            strlcpy strstr wcslen wmemchr)
      target_link_options(SymbianRuntime INTERFACE "--undefined=${symbol}")
      target_link_options(SymbianStreams INTERFACE "--undefined=${symbol}")
    endforeach()
  endif()
  if(SYMBIAN_RUNTIME_LOCAL_C_STDLIB)
    foreach(symbol IN ITEMS abs atof atoi qsort strtol strtoull strtold abort
                            __assert)
      set(root_symbol
        "$<$<NOT:$<BOOL:$<TARGET_PROPERTY:SYMBIAN_RUNTIME_MINIMAL_C_LINK>>>:--undefined=${symbol}>")
      target_link_options(SymbianRuntime INTERFACE "${root_symbol}")
      target_link_options(SymbianStreams INTERFACE "${root_symbol}")
    endforeach()
    foreach(symbol IN ITEMS malloc calloc realloc free getenv strerror_r
                            isspace _exit)
      target_link_options(SymbianRuntime INTERFACE "--undefined=${symbol}")
      target_link_options(SymbianStreams INTERFACE "--undefined=${symbol}")
    endforeach()
    if(SYMBIAN_RUNTIME_LEGACY_EUSER)
      foreach(symbol IN ITEMS asprintf snprintf vsnprintf strtod strtof
                              fputwc getwc ungetwc gmtime_r localtime_r
                              mktime strftime)
        target_link_options(SymbianRuntime INTERFACE "--undefined=${symbol}")
        target_link_options(SymbianStreams INTERFACE "--undefined=${symbol}")
      endforeach()
    endif()
  endif()
  if(SYMBIAN_RUNTIME_LOCAL_POSIX_TIME)
    target_link_options(SymbianRuntime INTERFACE --undefined=clock_gettime)
    target_link_options(SymbianStreams INTERFACE --undefined=clock_gettime)
  endif()
  if(SYMBIAN_RUNTIME_NATIVE_PTHREAD)
    target_link_options(SymbianRuntime INTERFACE --undefined=pthread_mutex_lock)
    target_link_options(SymbianStreams INTERFACE --undefined=pthread_mutex_lock)
    if(SYMBIAN_RUNTIME_LOCAL_C_STDLIB)
      foreach(symbol IN ITEMS mbrlen mbrtowc mbsnrtowcs mbsrtowcs mbtowc
                              wcrtomb wcsnrtombs wcsrtombs mbsinit)
        target_link_options(SymbianRuntime INTERFACE "--undefined=${symbol}")
        target_link_options(SymbianStreams INTERFACE "--undefined=${symbol}")
      endforeach()
    endif()
  endif()
endif()

# The guest Abseil Status/StatusOr closure is an alternate runtime profile.
# Its target supplies the pinned headers and matching stream libc++ archive.
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/SymbianAbseil.cmake")
  include("${CMAKE_CURRENT_LIST_DIR}/SymbianAbseil.cmake")
endif()

# Each verified device capability is a separate opt-in archive. Absent
# archives create no target, so a project cannot accidentally link a planned
# but unimplemented device facility.
foreach(component IN ITEMS system connectivity agent power midi_output vibration
                           display sensors camera camera_gles2 storage
                           clipboard failure_handler)
  set(component_archive
    "${SYMBIAN_SDK_PREFIX}/lib/${SYMBIAN_TARGET_ARCH}/libsymbian_api_${component}.a")
  if((EXISTS "${component_archive}" OR
      (SYMBIAN_WORKSPACE_BUILD AND
       (EXISTS "${CMAKE_SOURCE_DIR}/cpp/symbian/api/${component}/CMakeLists.txt"
        OR ((component STREQUAL "midi_output" OR component STREQUAL "vibration") AND
            EXISTS "${CMAKE_SOURCE_DIR}/cpp/symbian/api/media/CMakeLists.txt")
        OR ((component STREQUAL "clipboard" OR
             component STREQUAL "failure_handler") AND
            EXISTS "${CMAKE_SOURCE_DIR}/cpp/symbian/api/system/CMakeLists.txt")
        OR (component STREQUAL "camera_gles2" AND
            EXISTS "${CMAKE_SOURCE_DIR}/cpp/symbian/api/camera/CMakeLists.txt"))))
     AND TARGET Symbian::AbseilStatusOr)
    string(SUBSTRING "${component}" 0 1 component_initial)
    string(TOUPPER "${component_initial}" component_initial)
    string(SUBSTRING "${component}" 1 -1 component_rest)
    set(component_name "${component_initial}${component_rest}")
    if(component STREQUAL "midi_output")
      set(component_name MidiOutput)
    elseif(component STREQUAL "camera_gles2")
      set(component_name CameraGles2)
    elseif(component STREQUAL "failure_handler")
      set(component_name FailureHandler)
    endif()
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
        "${SYMBIAN_SDK_PREFIX}/proxies/gdi/gdi.dso"
        "${SYMBIAN_SDK_PREFIX}/proxies/fbscli/fbscli.dso")
    endif()
    if(component STREQUAL "midi_output" OR component STREQUAL "vibration")
      target_link_libraries(${component_target} INTERFACE
        Symbian::Fibers)
    endif()
    if(component STREQUAL "midi_output")
      target_link_libraries(${component_target} INTERFACE
        "${SYMBIAN_SDK_PREFIX}/proxies/midiclient/midiclient.dso")
    endif()
    if(component STREQUAL "vibration")
      target_link_libraries(${component_target} INTERFACE
        "${SYMBIAN_SDK_PREFIX}/proxies/hwrmvibraclient/hwrmvibraclient.dso")
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
        "${SYMBIAN_SDK_PREFIX}/proxies/ecam/ecam.dso"
        "${SYMBIAN_SDK_PREFIX}/proxies/fbscli/fbscli.dso")
    endif()
    if(component STREQUAL "camera" OR component STREQUAL "midi_output" OR
        component STREQUAL "vibration" OR component STREQUAL "clipboard")
      target_link_libraries(${component_target} INTERFACE Symbian::CxxAbi)
      target_compile_definitions(${component_target} INTERFACE
        SYMBIAN_NATIVE_LEAVES=1)
      target_link_options(${component_target} INTERFACE
        "SHELL:-z nocopyreloc" "SHELL:-z notext"
        "--undefined=symbian_native_leave_personalities")
    endif()
    if(component STREQUAL "camera_gles2")
      target_link_libraries(${component_target} INTERFACE
        Symbian::Camera Symbian::GLES2)
    endif()
    if(component STREQUAL "clipboard")
      target_link_libraries(${component_target} INTERFACE
        "${SYMBIAN_SDK_PREFIX}/proxies/bafl/bafl.dso"
        "${SYMBIAN_SDK_PREFIX}/proxies/etext/etext.dso")
    endif()
    if(component STREQUAL "failure_handler")
      target_link_libraries(${component_target} INTERFACE
        Symbian::System Symbian::Display Symbian::Storage
        Symbian::Clipboard)
    endif()
    add_library(Symbian::${component_name} ALIAS ${component_target})
  endif()
endforeach()

set(gles_archive
  "${SYMBIAN_SDK_PREFIX}/lib/${SYMBIAN_TARGET_ARCH}/libsymbian_api_gles.a")
if(EXISTS "${gles_archive}" AND TARGET Symbian::Display AND TARGET Symbian::EGL)
  add_library(SymbianApiGles STATIC IMPORTED)
  set_target_properties(SymbianApiGles PROPERTIES
    IMPORTED_LOCATION "${gles_archive}")
  target_link_libraries(SymbianApiGles INTERFACE Symbian::Display Symbian::EGL)
  add_library(Symbian::GlesDisplay ALIAS SymbianApiGles)
endif()

set(sdl2_manifest
  "${SYMBIAN_SDK_PREFIX}/share/symbian/portable/sdl2.json")
if(NOT SYMBIAN_WORKSPACE_BUILD AND NOT SYMBIAN_SDK_BUILDING_SDL2
   AND EXISTS "${sdl2_manifest}"
   AND TARGET Symbian::Display)
  set(sdl2_archive
    "${SYMBIAN_SDK_PREFIX}/lib/${SYMBIAN_TARGET_ARCH}/libsymbian_portable_sdl2.a")
  foreach(required IN ITEMS "${sdl2_archive}"
      "${SYMBIAN_SDK_PREFIX}/include/portable/sdl2/SDL.h"
      "${SYMBIAN_SDK_PREFIX}/include/portable/sdl2/SDL_config.h"
      "${SYMBIAN_SDK_PREFIX}/include/symbian/sdl2/sdl2.h"
      "${SYMBIAN_SDK_PREFIX}/licenses/portable/SDL2-LICENSE.txt")
    if(NOT EXISTS "${required}")
      message(FATAL_ERROR "SDL2 SDK payload missing: ${required}")
    endif()
  endforeach()
  add_library(SymbianPortableSdl2 STATIC IMPORTED)
  set_target_properties(SymbianPortableSdl2 PROPERTIES
    IMPORTED_LOCATION "${sdl2_archive}")
  target_include_directories(SymbianPortableSdl2 SYSTEM INTERFACE
    "${SYMBIAN_SDK_PREFIX}/include/portable/sdl2")
  target_compile_definitions(SymbianPortableSdl2 INTERFACE
    __SOFTFP= SYMBIAN_CAF_V2 __LIBM_ALIASES_H__)
  target_link_libraries(SymbianPortableSdl2 INTERFACE Symbian::Display)
  add_library(Symbian::PortableSdl2 ALIAS SymbianPortableSdl2)
  set(sdl2_gpu_archive
    "${SYMBIAN_SDK_PREFIX}/lib/${SYMBIAN_TARGET_ARCH}/libsymbian_portable_sdl2_gpu.a")
  if(EXISTS "${sdl2_gpu_archive}" AND TARGET Symbian::GlesDisplay
     AND TARGET Symbian::GLES2)
    add_library(SymbianPortableSdl2Gpu STATIC IMPORTED)
    set_target_properties(SymbianPortableSdl2Gpu PROPERTIES
      IMPORTED_LOCATION "${sdl2_gpu_archive}")
    target_include_directories(SymbianPortableSdl2Gpu SYSTEM INTERFACE
      "${SYMBIAN_SDK_PREFIX}/include/portable/sdl2")
    target_compile_definitions(SymbianPortableSdl2Gpu INTERFACE
      __SOFTFP= SYMBIAN_CAF_V2 __LIBM_ALIASES_H__)
    target_link_libraries(SymbianPortableSdl2Gpu INTERFACE
      Symbian::GlesDisplay Symbian::GLES2)
    add_library(Symbian::PortableSdl2Gpu ALIAS SymbianPortableSdl2Gpu)
  endif()
endif()

set(sdl3_manifest
  "${SYMBIAN_SDK_PREFIX}/share/symbian/portable/sdl3.json")
if(NOT SYMBIAN_WORKSPACE_BUILD AND NOT SYMBIAN_SDK_BUILDING_SDL3
   AND EXISTS "${sdl3_manifest}"
   AND TARGET Symbian::Display)
  set(sdl3_archive
    "${SYMBIAN_SDK_PREFIX}/lib/${SYMBIAN_TARGET_ARCH}/libsymbian_portable_sdl3.a")
  foreach(required IN ITEMS "${sdl3_archive}"
      "${SYMBIAN_SDK_PREFIX}/include/portable/sdl3/SDL3/SDL.h"
      "${SYMBIAN_SDK_PREFIX}/include/portable/sdl3/SDL3/SDL_build_config.h"
      "${SYMBIAN_SDK_PREFIX}/include/symbian/sdl3/sdl3.h"
      "${SYMBIAN_SDK_PREFIX}/licenses/portable/SDL3-LICENSE.txt")
    if(NOT EXISTS "${required}")
      message(FATAL_ERROR "SDL3 SDK payload missing: ${required}")
    endif()
  endforeach()
  add_library(SymbianPortableSdl3 STATIC IMPORTED)
  set_target_properties(SymbianPortableSdl3 PROPERTIES
    IMPORTED_LOCATION "${sdl3_archive}")
  target_include_directories(SymbianPortableSdl3 SYSTEM INTERFACE
    "${SYMBIAN_SDK_PREFIX}/include/portable/sdl3")
  target_compile_definitions(SymbianPortableSdl3 INTERFACE
    __SOFTFP= SYMBIAN_CAF_V2)
  target_link_libraries(SymbianPortableSdl3 INTERFACE Symbian::Display)
  add_library(Symbian::PortableSdl3 ALIAS SymbianPortableSdl3)
  set(sdl3_gpu_archive
    "${SYMBIAN_SDK_PREFIX}/lib/${SYMBIAN_TARGET_ARCH}/libsymbian_portable_sdl3_gpu.a")
  if(EXISTS "${sdl3_gpu_archive}" AND TARGET Symbian::GlesDisplay
     AND TARGET Symbian::GLES2)
    add_library(SymbianPortableSdl3Gpu STATIC IMPORTED)
    set_target_properties(SymbianPortableSdl3Gpu PROPERTIES
      IMPORTED_LOCATION "${sdl3_gpu_archive}")
    target_include_directories(SymbianPortableSdl3Gpu SYSTEM INTERFACE
      "${SYMBIAN_SDK_PREFIX}/include/portable/sdl3")
    target_compile_definitions(SymbianPortableSdl3Gpu INTERFACE
      __SOFTFP= SYMBIAN_CAF_V2)
    target_link_libraries(SymbianPortableSdl3Gpu INTERFACE
      Symbian::GlesDisplay Symbian::GLES2)
    add_library(Symbian::PortableSdl3Gpu ALIAS SymbianPortableSdl3Gpu)
  endif()
endif()

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
  if(SYMBIAN_RUNTIME_LEGACY_EKA2 AND TARGET Symbian::LegacyEka2)
    target_link_libraries(SymbianThreads INTERFACE
      Symbian::LegacyEka2 "${cxxabi_proxy}")
  else()
    target_link_libraries(SymbianThreads INTERFACE
      Symbian::Runtime "${thread_proxy}" "${cxxabi_proxy}")
  endif()
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

# Public static-library helper: SOURCES are required. Produces an ordinary
# ARM archive with SDK runtime selection and debug symbols. Link the target
# into an EXE or DLL; that final E32 image owns its storage and symbols.
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
  cmake_language(EVAL CODE
    "cmake_language(DEFER CALL _symbian_link_default_runtime ${target})")
  target_compile_options(${target} PRIVATE -g -gdwarf-4
    "-fdebug-prefix-map=${CMAKE_BINARY_DIR}=/symbian-build/library"
    "$<$<CONFIG:Debug>:-O0>")
  set_target_properties(${target} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endfunction()

# Guest Qt is an original OS DLL dependency, distinct from host emulator Qt.
# Its headers need the SDK's standard C++ allocation declarations first.
if(EXISTS "${SYMBIAN_SDK_PREFIX}/include/qt4/QtCore/qglobal.h")
  foreach(module IN ITEMS Core Gui Network Sql Xml OpenGL Svg Script
      XmlPatterns Declarative Multimedia OpenVG Test WebKit)
    if(NOT EXISTS "${SYMBIAN_SDK_PREFIX}/include/qt4/Qt${module}")
      continue()
    endif()
    string(TOLOWER "qt${module}" dll)
    if(NOT EXISTS "${SYMBIAN_SDK_PREFIX}/proxies/${dll}/${dll}.dso")
      message(FATAL_ERROR "Guest Qt${module} header exists but its frozen import interface is missing: ${dll}.dso")
    endif()
    add_library(SymbianQt${module} SHARED IMPORTED GLOBAL)
    set_target_properties(SymbianQt${module} PROPERTIES IMPORTED_LOCATION
      "${SYMBIAN_SDK_PREFIX}/proxies/${dll}/${dll}.dso")
    target_include_directories(SymbianQt${module} SYSTEM INTERFACE
      "${SYMBIAN_SDK_PREFIX}/include/qt4"
      "${SYMBIAN_SDK_PREFIX}/include/qt4/QtCore"
      "${SYMBIAN_SDK_PREFIX}/include/qt4/Qt${module}")
    target_compile_definitions(SymbianQt${module} INTERFACE
      QT_KEYPAD_NAVIGATION QT_SOFTKEYS_ENABLED)
    target_compile_options(SymbianQt${module} INTERFACE "SHELL:-fPIC"
      "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include \"${SYMBIAN_SDK_PREFIX}/cmake/qt_compat.h\">")
    add_library(Symbian::Qt${module} ALIAS SymbianQt${module})
  endforeach()
  target_link_libraries(SymbianQtCore INTERFACE Symbian::EUser)
  target_sources(SymbianQtCore INTERFACE
    "${SYMBIAN_SDK_PREFIX}/share/symbian/qt/newallocator_hook.cpp")
  target_link_libraries(SymbianQtGui INTERFACE Symbian::QtCore)
  foreach(module IN ITEMS Network Sql Xml Script XmlPatterns Test)
    if(TARGET SymbianQt${module})
      target_link_libraries(SymbianQt${module} INTERFACE Symbian::QtCore)
    endif()
  endforeach()
  foreach(module IN ITEMS OpenGL Svg Multimedia OpenVG)
    if(TARGET SymbianQt${module})
      target_link_libraries(SymbianQt${module} INTERFACE Symbian::QtGui)
    endif()
  endforeach()
  if(TARGET SymbianQtOpenGL)
    target_compile_definitions(SymbianQtOpenGL INTERFACE QT_OPENGL_ES_2)
    target_link_libraries(SymbianQtOpenGL INTERFACE Symbian::GLES2 Symbian::EGL)
  endif()
  if(TARGET SymbianQtOpenVG)
    target_link_libraries(SymbianQtOpenVG INTERFACE Symbian::OpenVG)
  endif()
  if(TARGET SymbianQtDeclarative)
    target_link_libraries(SymbianQtDeclarative INTERFACE
      Symbian::QtGui Symbian::QtScript Symbian::QtNetwork)
  endif()
  if(TARGET SymbianQtWebKit)
    target_link_libraries(SymbianQtWebKit INTERFACE Symbian::QtGui
      Symbian::QtOpenGL Symbian::QtNetwork Symbian::QtXmlPatterns
      Symbian::QtScript)
  endif()
endif()

# Resolve abstract vtable slots to the SDK trap, not a firmware data relocation.
foreach(runtime_target IN ITEMS SymbianRuntime SymbianStreams
    SymbianNativeAtomics64 SymbianLegacyEka2)
  if(TARGET ${runtime_target})
    target_link_options(${runtime_target} INTERFACE
      --undefined=symbian_runtime_pure_virtual_anchor)
  endif()
endforeach()
