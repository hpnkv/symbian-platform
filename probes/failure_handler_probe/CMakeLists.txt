cmake_minimum_required(VERSION 3.28)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(failure_handler_probe LANGUAGES CXX ASM)
endif()
include(SymbianApp)

symbian_add_executable(failure_handler_probe probe.cc)
target_include_directories(failure_handler_probe PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../../cpp")
target_link_libraries(failure_handler_probe PRIVATE Symbian::FailureHandler)
symbian_publish_executable(failure_handler_probe UID3 0xe0000a73)
