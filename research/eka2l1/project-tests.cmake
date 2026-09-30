# Injected with CMAKE_PROJECT_EKA2L1_INCLUDE; upstream source stays separate.
get_filename_component(SYMBIAN_PLATFORM_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.."
                       ABSOLUTE)
add_subdirectory("${SYMBIAN_PLATFORM_ROOT}/cpp/tests/eka2l1"
                 "${CMAKE_BINARY_DIR}/platform-tests")
