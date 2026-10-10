add_library(symbian_api_camera STATIC camera.cc frame.cc native_camera.cc
  camera_stream.cc native_camera_stream.cc)
option(SYMBIAN_CAMERA_NATIVE_TRACE "Enable temporary on-device ECam trace" OFF)
if(SYMBIAN_CAMERA_NATIVE_TRACE)
  target_compile_definitions(symbian_api_camera PRIVATE
    SYMBIAN_CAMERA_NATIVE_TRACE=1)
  target_link_libraries(symbian_api_camera PUBLIC Symbian::FileServer)
endif()
symbian_enable_native_leaves(symbian_api_camera SOURCES native_camera_stream.cc)
# The ECam leave boundary needs EH, but polymorphic RTTI would import
# drtaeabi type-info objects outside the supported E32 relocation contract.
set_property(SOURCE native_camera_stream.cc APPEND PROPERTY COMPILE_OPTIONS
  -fno-rtti)
target_compile_features(symbian_api_camera PUBLIC cxx_std_20)
target_compile_options(symbian_api_camera PRIVATE -fno-pic -fno-exceptions)
target_include_directories(symbian_api_camera PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../include"
  "${SYMBIAN_SDK_PREFIX}/include/platform")
target_link_libraries(symbian_api_camera PUBLIC Symbian::AbseilStatusOr
  "${SYMBIAN_SDK_PREFIX}/proxies/ecam/ecam.dso"
  "${SYMBIAN_SDK_PREFIX}/proxies/fbscli/fbscli.dso")

target_include_directories(symbian_api_camera PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/../include")

add_library(symbian_api_camera_gles2 STATIC gles2_frame_backend.cc)
target_compile_features(symbian_api_camera_gles2 PUBLIC cxx_std_20)
target_compile_options(symbian_api_camera_gles2 PRIVATE -fno-pic -fno-exceptions)
target_include_directories(symbian_api_camera_gles2 PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/../include")
target_link_libraries(symbian_api_camera_gles2 PUBLIC
  symbian_api_camera Symbian::GLES2)
