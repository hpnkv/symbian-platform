cmake_minimum_required(VERSION 3.28)
project(connectivity_probe LANGUAGES CXX ASM)

if(NOT EXISTS "${SYMBIAN_SDK_PREFIX}/cmake/SymbianApp.cmake")
  message(FATAL_ERROR "Select an exported Symbian SDK")
endif()
if(NOT SYMBIAN_IMPORT_PROXIES)
  foreach(dll euser libc libpthread libm drtaeabi esock insock)
    list(APPEND SYMBIAN_IMPORT_PROXIES
      "${SYMBIAN_SDK_PREFIX}/proxies/${dll}/${dll}.dso")
  endforeach()
endif()
include(SymbianPic)
include("${SYMBIAN_SDK_PREFIX}/cmake/SymbianApp.cmake")
if(NOT TARGET Symbian::Connectivity)
  message(FATAL_ERROR "The selected SDK does not provide Symbian::Connectivity")
endif()
symbian_add_import_executable(connectivity_probe
  STARTUP startup.S
  LINKER_SCRIPT image.ld
  SOURCES startup.cc probe.cc)
target_link_libraries(connectivity_probe PRIVATE Symbian::Connectivity)
target_link_options(connectivity_probe PRIVATE --gc-sections)
