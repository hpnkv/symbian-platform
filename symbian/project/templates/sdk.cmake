# Portable project toolchain locator. sdk-location.json is machine-local.
set(symbian_project_directory "${CMAKE_CURRENT_LIST_DIR}")
if(NOT EXISTS "${symbian_project_directory}/sdk-location.json")
  message(FATAL_ERROR "Select an installed native SDK in sdk-location.json before configuring this project")
endif()
file(READ "${symbian_project_directory}/sdk-location.json" symbian_sdk_selection)
string(JSON symbian_sdk_prefix GET "${symbian_sdk_selection}" sdk)
get_filename_component(symbian_sdk_prefix "${symbian_sdk_prefix}" ABSOLUTE
  BASE_DIR "${symbian_project_directory}")
set(SYMBIAN_SDK_PREFIX "${symbian_sdk_prefix}")
include("${SYMBIAN_SDK_PREFIX}/cmake/symbian-arm.cmake")
