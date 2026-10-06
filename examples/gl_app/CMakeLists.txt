cmake_minimum_required(VERSION 3.28)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(gl_app LANGUAGES CXX ASM)
endif()
include(SymbianApp)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/shaders/cube.vert" CUBE_VERTEX)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/shaders/cube.frag" CUBE_FRAGMENT)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/shaders/exit_button.vert" EXIT_VERTEX)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/shaders/exit_button.frag" EXIT_FRAGMENT)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  shaders/cube.vert shaders/cube.frag
  shaders/exit_button.vert shaders/exit_button.frag)
configure_file(shaders.h.in generated/shaders.h @ONLY)
symbian_add_executable(gl_app app.cc application.cc renderer.cc cube.cc
  exit_button.cc shader.cc)
target_include_directories(gl_app PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(shader_files shaders/cube.vert shaders/cube.frag
  shaders/exit_button.vert shaders/exit_button.frag)
set_source_files_properties(${shader_files} PROPERTIES HEADER_FILE_ONLY TRUE)
target_sources(gl_app PRIVATE ${shader_files} shaders.h.in)
target_link_libraries(gl_app PRIVATE
  Symbian::GLES2 Symbian::EGL Symbian::WindowServer Symbian::Gdi
  Symbian::Math Symbian::Hal)
symbian_publish_executable(gl_app UID3 0xe0000831)
