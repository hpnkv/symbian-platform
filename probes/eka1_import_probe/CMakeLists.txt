cmake_minimum_required(VERSION 3.28)
project(eka1_import_probe LANGUAGES CXX ASM)
include(SymbianPic)
if(NOT SYMBIAN_TARGET_ARCH STREQUAL "armv5t")
  message(FATAL_ERROR "The EKA1 import probe requires armv5t")
endif()
set(SYMBIAN_EKA1_CORRUPT_COPY 0 CACHE STRING "Copy verification negative control")
set(SYMBIAN_EKA1_RESULT 7610 CACHE STRING "Expected process exit reason")
if(NOT SYMBIAN_EKA1_CORRUPT_COPY MATCHES "^[01]$")
  message(FATAL_ERROR "SYMBIAN_EKA1_CORRUPT_COPY must be 0 or 1")
endif()
if(NOT SYMBIAN_EKA1_RESULT MATCHES "^[0-9]+$")
  message(FATAL_ERROR "SYMBIAN_EKA1_RESULT must be a nonnegative integer")
endif()
symbian_add_import_executable(eka1_import_probe
  STARTUP startup.S LINKER_SCRIPT image.ld SOURCES probe.cc)
target_compile_definitions(eka1_import_probe PRIVATE
  SYMBIAN_EKA1_RESULT=${SYMBIAN_EKA1_RESULT}
  SYMBIAN_EKA1_CORRUPT_COPY=${SYMBIAN_EKA1_CORRUPT_COPY})
target_compile_options(eka1_import_probe PRIVATE
  -fno-unwind-tables -fno-asynchronous-unwind-tables)
