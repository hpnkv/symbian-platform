# A single static host archive carries private dependencies into an ordinary
# CMake consumer. Python is a build tool here, not an installed runtime.
set(OPENSSL_USE_STATIC_LIBS TRUE)
find_package(OpenSSL 3.0 REQUIRED COMPONENTS Crypto)
find_package(ZLIB REQUIRED)
find_package(Python3 COMPONENTS Interpreter REQUIRED)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

function(symbian_collect_static directory output aliases)
  get_property(targets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
  set(result)
  set(alias_targets)
  foreach(target IN LISTS targets)
    list(APPEND alias_targets ${target})
    get_target_property(kind ${target} TYPE)
    if(kind STREQUAL "STATIC_LIBRARY")
      list(APPEND result ${target})
    endif()
  endforeach()
  get_property(children DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
  foreach(child IN LISTS children)
    symbian_collect_static("${child}" nested nested_aliases)
    list(APPEND result ${nested})
    list(APPEND alias_targets ${nested_aliases})
  endforeach()
  set(${output} "${result}" PARENT_SCOPE)
  set(${aliases} "${alias_targets}" PARENT_SCOPE)
endfunction()

symbian_collect_static("${abseil_SOURCE_DIR}" host_abseil_targets host_abseil_aliases)
set(host_libraries symbian_analysis symbian_e32 symbian_sis symbian_sdk
  symbian_status symbian_agent_frame symbian_http symbian_websocket
  symbian_nghttp2 symbian_device symbian_host_primitives
  ${host_abseil_targets} OpenSSL::Crypto ZLIB::ZLIB)
set(host_merge_args --library "${SYMBIAN_LIBUSB_STATIC}")
foreach(target IN LISTS host_libraries)
  list(APPEND host_merge_args --library "$<TARGET_FILE:${target}>")
endforeach()
foreach(host_zlib_archive IN LISTS ZLIB_LIBRARIES)
  if(host_zlib_archive MATCHES "^(optimized|debug|general)$")
    continue()
  endif()
  if(NOT host_zlib_archive MATCHES "\\.a$")
    message(FATAL_ERROR "The standalone host SDK requires static zlib: ${host_zlib_archive}")
  endif()
endforeach()
set(host_bundle "${CMAKE_CURRENT_BINARY_DIR}/libsymbian_host.a")
add_custom_command(OUTPUT "${host_bundle}"
  COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/scripts/bundle_static.py"
    --ar "${SYMBIAN_HOST_ARCHIVE_MERGER}" --output "${host_bundle}"
    ${host_merge_args}
  DEPENDS ${host_libraries} "${SYMBIAN_LIBUSB_STATIC}"
    "${PROJECT_SOURCE_DIR}/scripts/bundle_static.py"
  VERBATIM COMMAND_EXPAND_LISTS)
add_custom_target(symbian_host_sdk ALL DEPENDS "${host_bundle}")

install(FILES "${host_bundle}" DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY cpp/symbian/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/symbian
  FILES_MATCHING PATTERN "*.h")
install(DIRECTORY cpp/symbian/concurrency/common/
  cpp/symbian/concurrency/host/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING PATTERN "*.h")
install(DIRECTORY "${abseil_SOURCE_DIR}/absl"
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING PATTERN "*.h" PATTERN "*.inc")
install(DIRECTORY "${nlohmann_json_SOURCE_DIR}/include/"
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY "${OPENSSL_INCLUDE_DIR}/openssl"
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} FILES_MATCHING PATTERN "*.h")
install(DIRECTORY "${SYMBIAN_NGHTTP2_SOURCE}/lib/includes/nghttp2"
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} FILES_MATCHING PATTERN "*.h")
install(FILES "${CMAKE_BINARY_DIR}/cpp/symbian/net/include/nghttp2/nghttp2ver.h"
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nghttp2)
install(FILES "${SYMBIAN_LIBUSB_STATIC}"
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/symbian-host)
install(FILES "${SYMBIAN_LIBUSB_INCLUDE_DIR}/libusb.h"
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/libusb-1.0)
install(FILES "${SYMBIAN_LIBUSB_LICENSE}" DESTINATION share/symbian/licenses
  RENAME libusb-COPYING)
install(FILES "${SYMBIAN_DEPS_PREFIX}/share/symbian-OpenSSL-LICENSE"
  DESTINATION share/symbian/licenses RENAME OpenSSL-LICENSE)
install(FILES "${SYMBIAN_DEPS_PREFIX}/share/symbian-zlib-LICENSE"
  DESTINATION share/symbian/licenses RENAME zlib-LICENSE)
install(FILES LICENSE VERSION DESTINATION share/symbian)

# Also expose the exact Abseil target names for ABI-specific Python builds.
set(SYMBIAN_HOST_ABSEIL_ALIASES)
foreach(target IN LISTS host_abseil_aliases)
  if(TARGET absl::${target})
    set(short_name "${target}")
  else()
    string(REGEX REPLACE "^absl_" "" short_name "${target}")
  endif()
  if(NOT TARGET absl::${short_name})
    message(FATAL_ERROR "No public Abseil alias for ${target}")
  endif()
  string(APPEND SYMBIAN_HOST_ABSEIL_ALIASES
    "if(NOT TARGET absl::${short_name})\n"
    "  add_library(absl::${short_name} INTERFACE IMPORTED)\n"
    "  set_target_properties(absl::${short_name} PROPERTIES INTERFACE_LINK_LIBRARIES Symbian::Host)\n"
    "endif()\n")
endforeach()
configure_package_config_file(cmake/SymbianHostConfig.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/SymbianHostConfig.cmake"
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SymbianHost)
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/SymbianHostConfigVersion.cmake"
  VERSION ${PROJECT_VERSION} COMPATIBILITY ExactVersion)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/SymbianHostConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/SymbianHostConfigVersion.cmake"
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SymbianHost)
