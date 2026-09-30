cmake_minimum_required(VERSION 3.28)
project(gui_app LANGUAGES CXX ASM)
include(SymbianPic)
if(NOT EXISTS "${SYMBIAN_GUI_SDK_INCLUDE}/w32std.h")
  message(FATAL_ERROR "Prepare the source SDK with symbian toolchain prepare-gui-sdk; see WALKTHROUGH.md")
endif()
symbian_add_import_executable(gui_app
  STARTUP startup.S LINKER_SCRIPT image.ld SOURCES app.cc startup.cc)
target_include_directories(gui_app SYSTEM PRIVATE "${SYMBIAN_GUI_SDK_INCLUDE}")
target_compile_definitions(gui_app PRIVATE
  _UNICODE __GCC32__ __GCCV3__ __EABI__ __EPOC32__ __MARM__ __MARM_ARMV5__)
target_compile_options(gui_app PRIVATE $<$<COMPILE_LANGUAGE:CXX>:
  -fvisibility=hidden;-g;-gdwarf-4;-O1;
  -fdebug-prefix-map=${CMAKE_BINARY_DIR}=/symbian-build/gui_app;
  -fdebug-prefix-map=${CMAKE_SOURCE_DIR}=/symbian-src/gui_app;
  -fdebug-prefix-map=${SYMBIAN_GUI_SDK_INCLUDE}=/symbian-sdk/include>)
