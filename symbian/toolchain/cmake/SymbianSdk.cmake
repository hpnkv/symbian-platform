# Shared SDK selection for installed applications and source workspaces.
function(symbian_select_sdk)
  if(SYMBIAN_SOURCE_WORKSPACE)
    set(SYMBIAN_SDK_PREFIX "${SYMBIAN_SOURCE_WORKSPACE}/.symbian/workspace-inputs" PARENT_SCOPE)
    return()
  endif()
  set(selection_directory "${CMAKE_CURRENT_SOURCE_DIR}")
  if(ARGC GREATER 0)
    get_filename_component(selection_directory "${ARGV0}" ABSOLUTE
      BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
  endif()
  set(location "${selection_directory}/sdk-location.json")
  if(EXISTS "${location}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${location}")
    file(READ "${location}" selection)
    string(JSON prefix GET "${selection}" sdk)
    get_filename_component(prefix "${prefix}" ABSOLUTE
      BASE_DIR "${selection_directory}")
  elseif(SYMBIAN_SDK_PREFIX AND NOT SYMBIAN_USE_ACTIVE_SDK)
    set(prefix "${SYMBIAN_SDK_PREFIX}")
  elseif(DEFINED ENV{SYMBIAN_SDK_MANIFEST})
    get_filename_component(prefix "$ENV{SYMBIAN_SDK_MANIFEST}" DIRECTORY)
  else()
    if(DEFINED ENV{SYMBIAN_HOME} AND NOT "$ENV{SYMBIAN_HOME}" STREQUAL "")
      set(active "$ENV{SYMBIAN_HOME}/config/active-sdk.json")
    elseif(DEFINED ENV{XDG_CONFIG_HOME} AND NOT "$ENV{XDG_CONFIG_HOME}" STREQUAL "")
      set(active "$ENV{XDG_CONFIG_HOME}/symbian/active-sdk.json")
    else()
      set(active "$ENV{HOME}/.symbian/config/active-sdk.json")
    endif()
    if(NOT IS_ABSOLUTE "${active}")
      message(FATAL_ERROR "SYMBIAN_HOME and XDG_CONFIG_HOME must be absolute")
    endif()
    if(EXISTS "${active}")
      set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${active}")
      file(READ "${active}" selection)
      string(JSON manifest GET "${selection}" manifest)
      get_filename_component(prefix "${manifest}" DIRECTORY)
    endif()
  endif()
  if(prefix)
    set(SYMBIAN_SDK_PREFIX "${prefix}" PARENT_SCOPE)
  endif()
endfunction()

# Host and guest compiler/dependency identities require independent CMake graphs.
# The root guest graph builds the same live sources used by guest IDE profiles.
function(symbian_add_guest_subdirectory directory)
  include("${CMAKE_SOURCE_DIR}/cmake/SymbianWorkspaceInputs.cmake")
  include(ExternalProject)
  if(NOT SYMBIAN_TARGET_ARCH)
    set(SYMBIAN_TARGET_ARCH armv6)
  endif()
  set(guest_binary "${CMAKE_BINARY_DIR}/guest-${SYMBIAN_TARGET_ARCH}")
  file(STRINGS "${CMAKE_SOURCE_DIR}/${directory}/symbian.toml" name
    REGEX "^name[ \t]*=[ \t]*\"[A-Za-z][A-Za-z0-9_-]*\"[ \t]*$")
  file(STRINGS "${CMAKE_SOURCE_DIR}/${directory}/symbian.toml" uid
    REGEX "^uid3[ \t]*=[ \t]*0x[0-9A-Fa-f]+[ \t]*$")
  string(REGEX REPLACE "^name[ \t]*=[ \t]*\"([^\"]+)\"[ \t]*$" "\\1" name "${name}")
  string(REGEX REPLACE "^uid3[ \t]*=[ \t]*" "" uid "${uid}")
  ExternalProject_Add(${name}_guest_build
    SOURCE_DIR "${CMAKE_SOURCE_DIR}"
    BINARY_DIR "${guest_binary}"
    CMAKE_GENERATOR Ninja
    CMAKE_ARGS
      "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_SOURCE_DIR}/symbian/toolchain/cmake/symbian-arm.cmake"
      "-DSYMBIAN_INDEX_GUEST_PROBES=ON"
      "-DSYMBIAN_TARGET_ARCH=${SYMBIAN_TARGET_ARCH}"
      "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
      "-DCMAKE_CXX_COMPILER=${SYMBIAN_SDK_PREFIX}/bin/clang++"
      "-DCMAKE_C_COMPILER=${SYMBIAN_SDK_PREFIX}/bin/clang"
      "-DCMAKE_CXX_COMPILER_CLANG_SCAN_DEPS=${SYMBIAN_SDK_PREFIX}/bin/clang-scan-deps"
    BUILD_COMMAND "${CMAKE_COMMAND}" --build <BINARY_DIR> --target ${name}
    INSTALL_COMMAND "" BUILD_ALWAYS TRUE EXCLUDE_FROM_ALL TRUE)
  file(GLOB app_sources CONFIGURE_DEPENDS "${directory}/*.cc" "${directory}/*.h")
  add_custom_target(${name} DEPENDS ${name}_guest_build SOURCES ${app_sources})
  set(image "${guest_binary}/${directory}/e32/${name}.exe")
  file(GLOB proxies "${SYMBIAN_SDK_PREFIX}/proxies/*/*.dso")
  set(import_options)
  foreach(proxy IN LISTS proxies)
    if(NOT proxy MATCHES "/euser-(native64|eka1)/")
      list(APPEND import_options --import-proxy "${proxy}")
    endif()
  endforeach()
  add_custom_target(${name}_e32
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${guest_binary}/${directory}/e32"
    COMMAND "$<TARGET_FILE:symbian_native_tool>" convert-exe
      --input "${guest_binary}/${name}.elf"
      --uid3 "${uid}" ${import_options} --output "${image}"
    DEPENDS ${name} symbian_native_tool VERBATIM)
endfunction()
