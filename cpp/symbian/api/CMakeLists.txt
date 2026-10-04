cmake_minimum_required(VERSION 3.28)
project(symbian_device_api LANGUAGES CXX)

if(NOT SYMBIAN_SDK_PREFIX)
  message(FATAL_ERROR "Device API build requires SYMBIAN_SDK_PREFIX")
endif()
include("${SYMBIAN_SDK_PREFIX}/cmake/SymbianApp.cmake")
if(NOT TARGET Symbian::AbseilStatusOr)
  message(FATAL_ERROR "Device APIs require the installed Abseil StatusOr profile")
endif()

# Each implemented capability owns its archive and native service boundary.
add_subdirectory(system)
add_subdirectory(power)
add_subdirectory(display)
add_subdirectory(storage)
add_subdirectory(camera)
