# Symbian ARM EABI cross toolchain. ISA, host tools and target ABI are separate.
set(SYMBIAN_TARGET_ARCH armv6 CACHE STRING "Guest ISA: armv6 or armv5t")
set_property(CACHE SYMBIAN_TARGET_ARCH PROPERTY STRINGS armv6 armv5t)
if(NOT SYMBIAN_TARGET_ARCH MATCHES "^(armv5t|armv6)$")
  message(FATAL_ERROR "Unsupported SYMBIAN_TARGET_ARCH; choose armv6 or armv5t")
endif()
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES SYMBIAN_TARGET_ARCH)
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR "${SYMBIAN_TARGET_ARCH}")
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY CACHE STRING
    "Cross-compiler probes must not link a host executable" FORCE)

# Root IDE profiles follow the activated SDK, including after an SDK export.
# Standalone application profiles keep their explicit sdk.cmake selection.
if(SYMBIAN_USE_ACTIVE_SDK)
  if(DEFINED ENV{SYMBIAN_SDK_MANIFEST} AND
     NOT "$ENV{SYMBIAN_SDK_MANIFEST}" STREQUAL "")
    set(active_sdk_manifest "$ENV{SYMBIAN_SDK_MANIFEST}")
    if(NOT EXISTS "${active_sdk_manifest}")
      message(FATAL_ERROR "SDK manifest does not exist: ${active_sdk_manifest}")
    endif()
  else()
    if(DEFINED ENV{XDG_CONFIG_HOME})
      set(active_sdk_file "$ENV{XDG_CONFIG_HOME}/symbian/active-sdk.json")
    else()
      set(active_sdk_file "$ENV{HOME}/.config/symbian/active-sdk.json")
    endif()
    if(NOT EXISTS "${active_sdk_file}")
      message(FATAL_ERROR "No active SDK; run symbian sdk install")
    endif()
    file(READ "${active_sdk_file}" active_sdk_json)
    string(JSON active_sdk_manifest ERROR_VARIABLE active_sdk_error
           GET "${active_sdk_json}" manifest)
    if(active_sdk_error OR NOT EXISTS "${active_sdk_manifest}")
      message(FATAL_ERROR "Active SDK manifest is invalid: ${active_sdk_file}")
    endif()
  endif()
  get_filename_component(active_sdk_prefix "${active_sdk_manifest}" DIRECTORY)
  set(SYMBIAN_SDK_PREFIX "${active_sdk_prefix}" CACHE PATH
      "Active SDK used by the root guest IDE profile" FORCE)
endif()

if(SYMBIAN_SDK_PREFIX AND EXISTS "${SYMBIAN_SDK_PREFIX}/bin/clang++")
  set(CMAKE_CXX_COMPILER "${SYMBIAN_SDK_PREFIX}/bin/clang++")
  set(CMAKE_C_COMPILER "${SYMBIAN_SDK_PREFIX}/bin/clang")
else()
  find_program(CMAKE_CXX_COMPILER NAMES clang++ REQUIRED)
  find_program(CMAKE_C_COMPILER NAMES clang REQUIRED)
endif()
set(CMAKE_ASM_COMPILER "${CMAKE_CXX_COMPILER}")
if(SYMBIAN_SDK_PREFIX AND EXISTS "${SYMBIAN_SDK_PREFIX}/bin/ld.lld")
  set(symbian_lld "${SYMBIAN_SDK_PREFIX}/bin/ld.lld")
else()
  find_program(symbian_lld NAMES ld.lld REQUIRED)
endif()
# CMake can seed CMAKE_LINKER with the host linker during compiler detection;
# an ARM executable must always use the selected SDK's LLD driver.
set(CMAKE_LINKER "${symbian_lld}" CACHE FILEPATH "Symbian ARM linker" FORCE)
set(CMAKE_CXX_COMPILER_TARGET ${SYMBIAN_TARGET_ARCH}-none-eabi)
set(CMAKE_C_COMPILER_TARGET ${SYMBIAN_TARGET_ARCH}-none-eabi)
set(CMAKE_ASM_COMPILER_TARGET ${SYMBIAN_TARGET_ARCH}-none-eabi)
set(CMAKE_CXX_FLAGS_INIT
    "--target=${SYMBIAN_TARGET_ARCH}-none-eabi -mthumb -march=${SYMBIAN_TARGET_ARCH} -mfpu=none -mfloat-abi=soft -mabi=aapcs -ffreestanding -fno-exceptions -fno-rtti -nostdinc -O2")
set(CMAKE_C_FLAGS_INIT
    "--target=${SYMBIAN_TARGET_ARCH}-none-eabi -mthumb -march=${SYMBIAN_TARGET_ARCH} -mfpu=none -mfloat-abi=soft -mabi=aapcs -ffreestanding -nostdinc -O2")
set(CMAKE_ASM_FLAGS_INIT "--target=${SYMBIAN_TARGET_ARCH}-none-eabi -marm -march=${SYMBIAN_TARGET_ARCH} -mfpu=none -mfloat-abi=soft -nostdinc")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Call LLD directly so no host-native linker can enter the ARM target link.
# Do not resolve the ld.lld symlink: its argv[0] selects the LLVM driver.
set(CMAKE_CXX_LINK_EXECUTABLE
    "<CMAKE_LINKER> -m armelf --target1-abs --no-undefined --emit-relocs --build-id=none <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>")
set(CMAKE_C_LINK_EXECUTABLE
    "<CMAKE_LINKER> -m armelf --target1-abs --no-undefined --emit-relocs --build-id=none <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>")
list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}")

# Each Mbed TLS export carries one architecture's static archives. Merely
# making the package discoverable does not add it to an application's link.
if(SYMBIAN_SDK_PREFIX AND EXISTS
    "${SYMBIAN_SDK_PREFIX}/lib/${SYMBIAN_TARGET_ARCH}/mbedtls/lib/cmake/MbedTLS/MbedTLSConfig.cmake")
  list(PREPEND CMAKE_PREFIX_PATH
    "${SYMBIAN_SDK_PREFIX}/lib/${SYMBIAN_TARGET_ARCH}/mbedtls")
endif()

# Programs run on the host; libraries/headers/packages must come from the target.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
