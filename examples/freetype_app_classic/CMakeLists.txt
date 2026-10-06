cmake_minimum_required(VERSION 3.28)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(freetype_app_classic LANGUAGES C CXX ASM)
endif()
include(SymbianApp)
symbian_add_executable(freetype_app_classic main.cc ui.cc)
target_link_libraries(freetype_app_classic PRIVATE Symbian::PortableFreeType
  Symbian::WindowServer Symbian::Gdi)
symbian_publish_executable(freetype_app_classic UID3 0xe0000e15)
