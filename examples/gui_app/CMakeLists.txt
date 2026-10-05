cmake_minimum_required(VERSION 3.28)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(gui_app LANGUAGES CXX ASM)
endif()
include(SymbianApp)
symbian_add_executable(gui_app app.cc window_server.cc async_bridge.cc)
target_link_libraries(gui_app PRIVATE
  Symbian::Stackless Symbian::WindowServer Symbian::Gdi)
