cmake_minimum_required(VERSION 3.28)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/sdk-location.json")
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/sdk-location.json")
  file(READ "${CMAKE_CURRENT_SOURCE_DIR}/sdk-location.json" sdk_location_json)
  string(JSON selected_sdk GET "${sdk_location_json}" sdk)
  get_filename_component(SYMBIAN_SDK_PREFIX
    "${CMAKE_CURRENT_SOURCE_DIR}/${selected_sdk}" ABSOLUTE)
elseif(NOT SYMBIAN_SDK_PREFIX)
  if(DEFINED ENV{SYMBIAN_SDK_MANIFEST})
    get_filename_component(SYMBIAN_SDK_PREFIX "$ENV{SYMBIAN_SDK_MANIFEST}" DIRECTORY)
  else()
    if(DEFINED ENV{XDG_CONFIG_HOME})
      set(active_sdk "$ENV{XDG_CONFIG_HOME}/symbian/active-sdk.json")
    else()
      set(active_sdk "$ENV{HOME}/.config/symbian/active-sdk.json")
    endif()
    if(EXISTS "${active_sdk}")
      file(READ "${active_sdk}" active_sdk_json)
      string(JSON sdk_manifest GET "${active_sdk_json}" manifest)
      get_filename_component(SYMBIAN_SDK_PREFIX "${sdk_manifest}" DIRECTORY)
    endif()
  endif()
endif()
if(NOT EXISTS "${SYMBIAN_SDK_PREFIX}/cmake/SymbianApp.cmake")
  message(FATAL_ERROR "gui_app needs an installed SDK; set SYMBIAN_SDK_MANIFEST or activate one with symbian sdk install")
endif()
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(gui_app LANGUAGES CXX ASM)
else()
  # A target may compile with a different Clang triple in a host project. Keep
  # the guest flags and direct ELF link rule scoped to this directory; the
  # tooling's Mach-O/ELF rules remain in the root directory.
  set(SYMBIAN_GUI_TARGET_ARCH armv6 CACHE STRING "Guest GUI ISA: armv6 or armv5t")
  set(SYMBIAN_TARGET_ARCH "${SYMBIAN_GUI_TARGET_ARCH}")
  include("${CMAKE_SOURCE_DIR}/symbian/toolchain/cmake/symbian-arm.cmake")
  find_program(SYMBIAN_ARM_LINKER NAMES ld.lld REQUIRED)
  find_program(SYMBIAN_ARM_COMPILER NAMES clang++ REQUIRED)
  # <CMAKE_LINKER> is expanded from the root compiler configuration by CMake.
  # Spell out the guest driver's path in this directory's link rule instead.
  string(REPLACE "<CMAKE_LINKER>" "\"${SYMBIAN_ARM_LINKER}\""
         CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_LINK_EXECUTABLE}")
  set(CMAKE_LINKER "${SYMBIAN_ARM_LINKER}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS_INIT}")
  set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS_INIT}")
  set(CMAKE_CXX_LINK_FLAGS "")
  set(CMAKE_OSX_ARCHITECTURES "")
  set(CMAKE_OSX_SYSROOT "")
  set(CMAKE_OSX_DEPLOYMENT_TARGET "")
  enable_language(ASM)
endif()
set(SYMBIAN_IMPORT_PROXIES "")
foreach(dll euser ws32 gdi libc libm libpthread drtaeabi)
  list(APPEND SYMBIAN_IMPORT_PROXIES
       "${SYMBIAN_SDK_PREFIX}/proxies/${dll}/${dll}.dso")
endforeach()
include(SymbianPic)
include("${SYMBIAN_SDK_PREFIX}/cmake/SymbianApp.cmake")
if(NOT EXISTS "${SYMBIAN_GUI_SDK_INCLUDE}/w32std.h")
  message(FATAL_ERROR "Prepare the source SDK with symbian toolchain prepare-gui-sdk; see WALKTHROUGH.md")
endif()
symbian_add_import_executable(gui_app
  STARTUP startup.S LINKER_SCRIPT image.ld SOURCES app.cc window_server.cc async_bridge.cc startup.cc)
target_link_libraries(gui_app PRIVATE Symbian::Stackless Symbian::Threads)
target_link_options(gui_app PRIVATE --gc-sections)
target_include_directories(gui_app SYSTEM PRIVATE "${SYMBIAN_GUI_SDK_INCLUDE}")
target_compile_definitions(gui_app PRIVATE
  _UNICODE __GCC32__ __GCCV3__ __EABI__ __EPOC32__ __MARM__ __MARM_ARMV5__)
target_compile_options(gui_app PRIVATE $<$<COMPILE_LANGUAGE:CXX>:
  -fvisibility=hidden;-g;-gdwarf-4;-O1;
  -fdebug-compilation-dir=/symbian-build/gui_app;
  -fdebug-prefix-map=${CMAKE_BINARY_DIR}=/symbian-build/gui_app;
  -fdebug-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}=/symbian-src/gui_app;
  -fdebug-prefix-map=${SYMBIAN_GUI_SDK_INCLUDE}=/symbian-sdk/include>)

if(NOT CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  set(SYMBIAN_HOST_PYTHON "${CMAKE_SOURCE_DIR}/.venv/bin/python" CACHE FILEPATH
      "Installed platform Python used to publish the GUI E32 and run the emulator")
  set(SYMBIAN_GUI_OUTPUT "${CMAKE_SOURCE_DIR}/.symbian/gui-app" CACHE PATH
      "Published GUI ELF/E32 pair used by the emulator supervisor")
  add_custom_target(gui_app_e32
    COMMAND "${SYMBIAN_HOST_PYTHON}" -m symbian.cli build
            --project "${CMAKE_CURRENT_SOURCE_DIR}" --output "${SYMBIAN_GUI_OUTPUT}"
            --compiler "${SYMBIAN_ARM_COMPILER}" --linker "${SYMBIAN_ARM_LINKER}"
            --architecture "${SYMBIAN_GUI_TARGET_ARCH}"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    USES_TERMINAL
    COMMENT "Build, independently reproduce and publish the GUI ELF/E32 pair")
endif()
