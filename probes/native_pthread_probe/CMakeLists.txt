cmake_minimum_required(VERSION 3.28)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(native_pthread_probe LANGUAGES CXX ASM)
endif()
include(SymbianApp)
symbian_add_executable(native_pthread_probe probe.cc)
target_link_libraries(native_pthread_probe PRIVATE Symbian::Threads)
target_link_options(native_pthread_probe PRIVATE --gc-sections)
set_target_properties(native_pthread_probe PROPERTIES
  SYMBIAN_RUNTIME_MINIMAL_C_LINK TRUE)
if(SYMBIAN_RUNTIME_LOCAL_C_STDLIB)
  target_compile_definitions(native_pthread_probe PRIVATE
    SYMBIAN_PROBE_LOCAL_C_ALLOC=1)
  if(SYMBIAN_RUNTIME_NATIVE_PTHREAD)
    target_compile_definitions(native_pthread_probe PRIVATE
      SYMBIAN_PROBE_LOCAL_C_UTF8=1)
  endif()
  if(SYMBIAN_RUNTIME_LEGACY_EUSER)
    target_compile_definitions(native_pthread_probe PRIVATE
      SYMBIAN_PROBE_LEGACY_C=1)
  endif()
endif()
symbian_publish_executable(native_pthread_probe UID3 0xe0000e26)
