cmake_minimum_required(VERSION 3.28)
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/sdk-location.json")
  message(FATAL_ERROR "Run python prepare.py --qt-source /path/to/qt-4.8.1 first")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/sdk-location.json")
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/sdk-location.json" sdk_json)
string(JSON SYMBIAN_SDK_PREFIX GET "${sdk_json}" sdk)
project(qt_app LANGUAGES CXX ASM)
include("${SYMBIAN_SDK_PREFIX}/cmake/SymbianApp.cmake")
symbian_add_import_executable(qt_app
  STARTUP startup.S LINKER_SCRIPT image.ld
  SOURCES app.cc startup.cc .symbian/qt/newallocator_hook.cpp)
target_link_libraries(qt_app PRIVATE Symbian::Runtime)
target_link_options(qt_app PRIVATE --gc-sections)
target_include_directories(qt_app SYSTEM PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/.symbian/qt/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/.symbian/qt/include/QtCore")
target_compile_definitions(qt_app PRIVATE QT_KEYPAD_NAVIGATION QT_SOFTKEYS_ENABLED)
target_compile_options(qt_app PRIVATE -g -gdwarf-4 -O1
  -fdebug-compilation-dir=/symbian-build/qt_app
  -fdebug-prefix-map=${CMAKE_BINARY_DIR}=/symbian-build/qt_app
  -fdebug-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}=/symbian-src/qt_app)
