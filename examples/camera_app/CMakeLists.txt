cmake_minimum_required(VERSION 3.28)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(camera_app LANGUAGES CXX ASM)
endif()
include(SymbianApp)

symbian_add_executable(camera_app main.cc application.cc trace.cc)
# Registered apps index every source in their directory. Keep this optional
# implementation out of the software executable's compilation and imports.
get_target_property(camera_sources camera_app SOURCES)
list(REMOVE_ITEM camera_sources "${CMAKE_CURRENT_SOURCE_DIR}/gpu_probe.cc")
set_property(TARGET camera_app PROPERTY SOURCES "${camera_sources}")
if(TARGET symbian_api_camera)
  target_link_libraries(camera_app PRIVATE symbian_api_camera symbian_api_display
    symbian_api_storage symbian_api_system symbian_api_failure_handler)
else()
  target_link_libraries(camera_app PRIVATE Symbian::Camera Symbian::Display
    Symbian::Storage Symbian::System Symbian::FailureHandler)
endif()
# ECam construction requires UserEnvironment (capability bit 19).
symbian_publish_executable(camera_app UID3 0xe0000a59 CAPABILITIES 0x80000)

option(SYMBIAN_CAMERA_GPU_PROBE "Build the GLES2 frame diagnostic" ON)
option(SYMBIAN_CAMERA_DIAGNOSTIC_FRAME "Save one raw ECam frame" OFF)
if(SYMBIAN_CAMERA_GPU_PROBE AND
    ((TARGET symbian_api_camera_gles2 AND TARGET symbian_api_gles) OR
     (TARGET Symbian::CameraGles2 AND TARGET Symbian::GlesDisplay)))
  symbian_add_executable(camera_app_gles2 main.cc application.cc gpu_probe.cc
    trace.cc)
  target_compile_definitions(camera_app_gles2 PRIVATE SYMBIAN_CAMERA_GPU_PROBE=1)
  if(SYMBIAN_CAMERA_DIAGNOSTIC_FRAME)
    target_compile_definitions(camera_app_gles2 PRIVATE
      SYMBIAN_CAMERA_DIAGNOSTIC_FRAME=1)
  endif()
  if(TARGET symbian_api_camera_gles2)
    target_link_libraries(camera_app_gles2 PRIVATE symbian_api_camera_gles2
      symbian_api_gles symbian_api_storage symbian_api_system
      symbian_api_failure_handler)
  elseif(TARGET Symbian::CameraGles2 AND TARGET Symbian::GlesDisplay)
    target_link_libraries(camera_app_gles2 PRIVATE Symbian::CameraGles2
      Symbian::GlesDisplay Symbian::Storage Symbian::System
      Symbian::FailureHandler)
  else()
    message(FATAL_ERROR "Camera GLES2 archives are missing")
  endif()
  symbian_publish_executable(camera_app_gles2 UID3 0xe0000a5a
    CAPABILITIES 0x80000)
endif()
