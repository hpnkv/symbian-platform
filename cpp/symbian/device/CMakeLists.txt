set(OPENSSL_USE_STATIC_LIBS TRUE)
find_package(OpenSSL 3.0 REQUIRED COMPONENTS Crypto)
find_package(Threads REQUIRED)
if(SYMBIAN_DEPS_PREFIX)
  set(symbian_libusb_root "${SYMBIAN_DEPS_PREFIX}")
elseif(APPLE)
  set(symbian_libusb_root "/opt/homebrew/opt/libusb")
else()
  message(FATAL_ERROR "Set SYMBIAN_DEPS_PREFIX with static libusb 1.0.30")
endif()
find_path(SYMBIAN_LIBUSB_INCLUDE_DIR libusb.h
  HINTS "${symbian_libusb_root}/include/libusb-1.0"
  NO_DEFAULT_PATH REQUIRED)
find_library(SYMBIAN_LIBUSB_STATIC NAMES libusb-1.0.a
  HINTS "${symbian_libusb_root}/lib"
  NO_DEFAULT_PATH REQUIRED)
add_library(symbian_device STATIC usb.cc)
add_library(symbian::device ALIAS symbian_device)
target_include_directories(symbian_device PUBLIC ${PROJECT_SOURCE_DIR}/cpp
  PRIVATE ${SYMBIAN_LIBUSB_INCLUDE_DIR})
target_link_libraries(symbian_device PUBLIC absl::statusor symbian::concurrency
  OpenSSL::Crypto Threads::Threads PRIVATE ${SYMBIAN_LIBUSB_STATIC})
if(APPLE)
  target_link_libraries(symbian_device PRIVATE "-framework IOKit" "-framework CoreFoundation"
    "-framework Security" "-lobjc")
endif()
symbian_native_library(symbian_device)

find_file(SYMBIAN_LIBUSB_LICENSE COPYING
  HINTS "${symbian_libusb_root}" NO_DEFAULT_PATH REQUIRED)
find_file(SYMBIAN_LIBUSB_PKGCONFIG libusb-1.0.pc
  HINTS "${symbian_libusb_root}/lib/pkgconfig" NO_DEFAULT_PATH REQUIRED)
file(STRINGS "${SYMBIAN_LIBUSB_PKGCONFIG}" symbian_libusb_version
  REGEX "^Version: 1\\.0\\.30$")
if(NOT symbian_libusb_version)
  message(FATAL_ERROR "Static libusb 1.0.30 is required")
endif()
if(SYMBIAN_DEPS_PREFIX)
  foreach(asset IN ITEMS SYMBIAN_LIBUSB_INCLUDE_DIR SYMBIAN_LIBUSB_STATIC
                         SYMBIAN_LIBUSB_LICENSE SYMBIAN_LIBUSB_PKGCONFIG)
    string(FIND "${${asset}}" "${SYMBIAN_DEPS_PREFIX}/" asset_prefix)
    if(NOT asset_prefix EQUAL 0)
      message(FATAL_ERROR "${asset} must come from SYMBIAN_DEPS_PREFIX")
    endif()
  endforeach()
endif()
