# Optional research oracles; distribution builds use project-runtime.cmake.
include("${CMAKE_CURRENT_LIST_DIR}/project-runtime.cmake")
add_subdirectory("${SYMBIAN_PLATFORM_ROOT}/cpp/tests/eka2l1"
                 "${CMAKE_BINARY_DIR}/platform-tests")
