# Injected with CMAKE_PROJECT_EKA2L1_INCLUDE; upstream source stays separate.
get_filename_component(SYMBIAN_PLATFORM_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.."
                       ABSOLUTE)
include(FetchContent)
set(ABSL_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(ABSL_PROPAGATE_CXX_STD ON CACHE BOOL "" FORCE)
set(SYMBIAN_ABSEIL_SOURCE_DIR "$ENV{SYMBIAN_ABSEIL_SOURCE_DIR}" CACHE PATH
    "Optional pinned Abseil source checkout")
if(SYMBIAN_ABSEIL_SOURCE_DIR)
  FetchContent_Declare(abseil SOURCE_DIR "${SYMBIAN_ABSEIL_SOURCE_DIR}")
else()
  FetchContent_Declare(abseil
    GIT_REPOSITORY https://github.com/abseil/abseil-cpp.git
    GIT_TAG 5650e9cf76d3be4318d5fa3af38ee483ddfd5e4a)
endif()
# This hook runs inside project(), before upstream selects its C++ standard.
# Restore that setting after configuring our C++20-only dependencies.
set(_SYMBIAN_SAVED_CXX_STANDARD "${CMAKE_CXX_STANDARD}")
set(CMAKE_CXX_STANDARD 20)
FetchContent_MakeAvailable(abseil)
add_subdirectory("${SYMBIAN_PLATFORM_ROOT}/cpp/symbian/emulator"
                 "${CMAKE_BINARY_DIR}/platform-control")
if(_SYMBIAN_SAVED_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD "${_SYMBIAN_SAVED_CXX_STANDARD}")
else()
  unset(CMAKE_CXX_STANDARD)
endif()
unset(_SYMBIAN_SAVED_CXX_STANDARD)
add_subdirectory("${SYMBIAN_PLATFORM_ROOT}/cpp/tests/eka2l1"
                 "${CMAKE_BINARY_DIR}/platform-tests")
