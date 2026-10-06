cmake_minimum_required(VERSION 3.28)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(symbian_portable_jpeg LANGUAGES C CXX ASM)
  include(SymbianApp)
endif()

set(SYMBIAN_JPEG_SOURCE
  "${CMAKE_SOURCE_DIR}/research/upstream/qt4/src/3rdparty/libjpeg"
  CACHE PATH "Pinned Qt 4.8.1 IJG libjpeg 8c source")
if(NOT EXISTS "${SYMBIAN_JPEG_SOURCE}/jpeglib.h")
  message(FATAL_ERROR "Portable IJG libjpeg 8c source is missing at ${SYMBIAN_JPEG_SOURCE}; run scripts/prepare_native_sources.py")
endif()
set(jpeg_sources
  jaricom.c jcapimin.c jcapistd.c jcarith.c jccoefct.c
  jccolor.c jcdctmgr.c jchuff.c jcinit.c jcmainct.c
  jcmarker.c jcmaster.c jcomapi.c jcparam.c jcprepct.c
  jcsample.c jctrans.c jdapimin.c jdapistd.c jdarith.c
  jdatadst.c jdatasrc.c jdcoefct.c jdcolor.c jddctmgr.c
  jdhuff.c jdinput.c jdmainct.c jdmarker.c jdmaster.c
  jdmerge.c jdpostct.c jdsample.c jdtrans.c jerror.c
  jfdctflt.c jfdctfst.c jfdctint.c jidctflt.c jidctfst.c
  jidctint.c jquant1.c jquant2.c jutils.c jmemmgr.c
  jmemnobs.c
)
list(TRANSFORM jpeg_sources PREPEND "${SYMBIAN_JPEG_SOURCE}/")
add_library(symbian_portable_jpeg STATIC ${jpeg_sources})
target_include_directories(symbian_portable_jpeg PUBLIC "${SYMBIAN_JPEG_SOURCE}")
target_include_directories(symbian_portable_jpeg BEFORE PRIVATE
  "${SYMBIAN_SDK_PREFIX}/include/openc")
target_link_libraries(symbian_portable_jpeg PUBLIC Symbian::OpenC Symbian::Runtime)
