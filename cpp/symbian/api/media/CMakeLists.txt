foreach(component IN ITEMS midi_output vibration)
  set(target "symbian_api_${component}")
  if(component STREQUAL "midi_output")
    set(source midi_output.cc)
  else()
    set(source vibration.cc)
  endif()
  add_library(${target} STATIC ${source})
  symbian_enable_native_leaves(${target} SOURCES ${source})
  # Leave personality is enabled for native API calls, but libc++ RTTI has no
  # complete guest type-info closure in the default runtime profile.
  set_property(SOURCE ${source} APPEND PROPERTY COMPILE_OPTIONS -fno-rtti)
  target_compile_features(${target} PUBLIC cxx_std_20)
  target_compile_options(${target} PRIVATE -fno-pic -fno-exceptions -fno-rtti)
  target_include_directories(${target} PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/../include")
  target_include_directories(${target} PRIVATE
    "${SYMBIAN_SDK_PREFIX}/include/native"
    "${CMAKE_CURRENT_SOURCE_DIR}/compat/original")
  target_link_libraries(${target} PUBLIC Symbian::AbseilStatusOr)
  target_link_libraries(${target} PRIVATE Symbian::Fibers)
endforeach()
target_link_libraries(symbian_api_midi_output PUBLIC
  "${SYMBIAN_SDK_PREFIX}/proxies/midiclient/midiclient.dso")
target_link_libraries(symbian_api_vibration PUBLIC
  "${SYMBIAN_SDK_PREFIX}/proxies/hwrmvibraclient/hwrmvibraclient.dso")
