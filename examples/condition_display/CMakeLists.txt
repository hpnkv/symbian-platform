cmake_minimum_required(VERSION 3.28)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(condition_display LANGUAGES CXX ASM)
endif()
include(SymbianApp)

symbian_add_executable(condition_display main.cc)
if(TARGET symbian_api_display)
  target_link_libraries(condition_display PRIVATE symbian_api_display
    symbian_api_power symbian_api_system)
else()
  target_link_libraries(condition_display PRIVATE Symbian::Display Symbian::Power
    Symbian::System)
endif()
symbian_publish_executable(condition_display UID3 0xe0000a70)
