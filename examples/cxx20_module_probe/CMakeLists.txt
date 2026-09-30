cmake_minimum_required(VERSION 3.28)
project(cxx20_module_probe LANGUAGES CXX ASM)
include(SymbianPic)
if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR
   CMAKE_CXX_COMPILER_VERSION VERSION_LESS 16)
  message(FATAL_ERROR "This module experiment requires upstream Clang 16+ with clang-scan-deps")
endif()
symbian_add_pic_executable(cxx20_module_probe
  STARTUP startup.S LINKER_SCRIPT image.ld SOURCES probe.cc)
target_sources(cxx20_module_probe PRIVATE
  FILE_SET CXX_MODULES FILES transform.cppm)
set_target_properties(cxx20_module_probe PROPERTIES CXX_SCAN_FOR_MODULES ON)
