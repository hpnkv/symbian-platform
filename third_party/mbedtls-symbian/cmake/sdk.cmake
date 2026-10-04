# Select one local SDK before project(); consumers inherit their own toolchain.
if(NOT SYMBIAN_SDK_PREFIX AND DEFINED ENV{SYMBIAN_SDK_PREFIX})
  set(SYMBIAN_SDK_PREFIX "$ENV{SYMBIAN_SDK_PREFIX}")
endif()
if(NOT SYMBIAN_SDK_PREFIX AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/../sdk-location.json")
  file(READ "${CMAKE_CURRENT_LIST_DIR}/../sdk-location.json" location)
  string(JSON SYMBIAN_SDK_PREFIX GET "${location}" sdk)
  cmake_path(ABSOLUTE_PATH SYMBIAN_SDK_PREFIX
    BASE_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/.." NORMALIZE)
endif()
if(NOT EXISTS "${SYMBIAN_SDK_PREFIX}/cmake/symbian-arm.cmake")
  message(FATAL_ERROR
    "Select a current SDK with -DSYMBIAN_SDK_PREFIX=/path/to/sdk, the environment variable, or sdk-location.json")
endif()
set(SYMBIAN_SDK_PREFIX "${SYMBIAN_SDK_PREFIX}" CACHE PATH "Selected Symbian SDK")
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/SymbianMbedTLS.cmake")
