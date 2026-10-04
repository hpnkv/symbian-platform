cmake_minimum_required(VERSION 3.28)
project(symbian_guest_fiber LANGUAGES CXX)
include("${SYMBIAN_SDK_PREFIX}/cmake/SymbianApp.cmake")
if(NOT TARGET Symbian::Stackless)
  message(FATAL_ERROR "Guest fibers require the installed Abseil stackless profile")
endif()
add_library(symbian_guest_fiber STATIC
  symbian/concurrency/worker_executor.cc
  thread/fiber.cc
  thread/select.cc
  thread/selectables.cc)
target_link_libraries(symbian_guest_fiber PUBLIC Symbian::Stackless)
target_include_directories(symbian_guest_fiber PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/../common")
target_compile_options(symbian_guest_fiber PRIVATE -fno-pic)
