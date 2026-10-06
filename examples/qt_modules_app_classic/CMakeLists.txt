cmake_minimum_required(VERSION 3.28)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(qt_modules_app_classic LANGUAGES CXX ASM)
endif()
include(SymbianApp)
symbian_add_executable(qt_modules_app_classic main.cc)
target_link_libraries(qt_modules_app_classic PRIVATE Symbian::QtWebKit
  Symbian::QtSql Symbian::QtXml)
symbian_publish_executable(qt_modules_app_classic UID3 0xe0000e09)
