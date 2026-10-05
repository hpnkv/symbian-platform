cmake_minimum_required(VERSION 3.28)
project(cxx20_probe LANGUAGES CXX ASM)
include(SymbianPic)
symbian_add_executable(cxx20_probe probe.cc callbacks.cc transform.cc interworking.S)

# This opt-in experiment consumes separately configured headers, not macOS
# runtime libraries. A target libc++ port is not supplied by this project.
option(SYMBIAN_CXX20_USE_LIBCXX "Experiment with external target-configured headers" OFF)
if(SYMBIAN_CXX20_USE_LIBCXX)
  foreach(input LIBCXX_CONFIG_INCLUDE LIBCXX_INCLUDE)
    if(NOT IS_DIRECTORY "${SYMBIAN_CXX20_${input}}")
      message(FATAL_ERROR "SYMBIAN_CXX20_${input} must be an existing directory")
    endif()
  endforeach()
  if(NOT EXISTS "${SYMBIAN_CXX20_LIBCXX_CONFIG_INCLUDE}/__config_site" OR
     NOT EXISTS "${SYMBIAN_CXX20_LIBCXX_INCLUDE}/span")
    message(FATAL_ERROR "Target libc++ configuration and headers are required")
  endif()
  execute_process(COMMAND "${CMAKE_CXX_COMPILER}" -print-resource-dir
    OUTPUT_VARIABLE resource OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY)
  target_compile_definitions(cxx20_probe PRIVATE SYMBIAN_CXX20_USE_LIBCXX=1)
  target_include_directories(cxx20_probe SYSTEM BEFORE PRIVATE
    "${SYMBIAN_CXX20_LIBCXX_CONFIG_INCLUDE}"
    "${SYMBIAN_CXX20_LIBCXX_INCLUDE}" "${resource}/include")
endif()
