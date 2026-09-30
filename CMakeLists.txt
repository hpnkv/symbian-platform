cmake_minimum_required(VERSION 3.28)
project(symbian_platform VERSION 0.1.0 LANGUAGES CXX)

include(CTest)
include(FetchContent)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(BUILD_SHARED_LIBS OFF)
set(ABSL_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(ABSL_PROPAGATE_CXX_STD ON CACHE BOOL "" FORCE)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(SYMBIAN_BUILD_PYTHON "Build the Python boundary" ON)
set(SYMBIAN_ABSEIL_SOURCE_DIR "$ENV{SYMBIAN_ABSEIL_SOURCE_DIR}" CACHE PATH
    "Optional checkout of the pinned Abseil revision")
if(SYMBIAN_ABSEIL_SOURCE_DIR)
  FetchContent_Declare(abseil SOURCE_DIR "${SYMBIAN_ABSEIL_SOURCE_DIR}")
else()
  FetchContent_Declare(abseil
    GIT_REPOSITORY https://github.com/abseil/abseil-cpp.git
    GIT_TAG 5650e9cf76d3be4318d5fa3af38ee483ddfd5e4a)
endif()
FetchContent_MakeAvailable(abseil)

function(symbian_native_library target)
  target_compile_features(${target} PUBLIC cxx_std_20)
  target_compile_options(${target} PRIVATE
    $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-fno-exceptions;-Wall;-Wextra;-Wconversion;-Wshadow>
    $<$<CXX_COMPILER_ID:AppleClang,Clang>:-Wthread-safety;-Werror=thread-safety>)
endfunction()

add_subdirectory(cpp/symbian/analysis)
add_subdirectory(cpp/symbian/e32)
if(SYMBIAN_BUILD_PYTHON)
  add_subdirectory(cpp/python)
endif()
if(BUILD_TESTING)
  find_package(GTest CONFIG QUIET)
  if(NOT GTest_FOUND)
    FetchContent_Declare(googletest
      GIT_REPOSITORY https://github.com/google/googletest.git
      GIT_TAG v1.16.0 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(googletest)
  endif()
  add_subdirectory(cpp/tests)
endif()
