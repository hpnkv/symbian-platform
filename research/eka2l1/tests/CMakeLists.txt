# Focused tests for the maintained Qt input patch, without starting a ROM.
cmake_minimum_required(VERSION 3.24)
project(symbian_emulator_input_tests LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Widgets OpenGL Test)
find_package(GTest CONFIG REQUIRED)
if(NOT EKA2L1_SOURCE OR NOT EKA2L1_BUILD)
  message(FATAL_ERROR "Set EKA2L1_SOURCE and EKA2L1_BUILD to the patched emulator")
endif()
add_executable(trackpad_scroll_test trackpad_scroll_test.cc
  ${EKA2L1_SOURCE}/src/emu/qt/src/displaywidget.cpp
  ${EKA2L1_SOURCE}/src/emu/qt/include/qt/displaywidget.h)
target_include_directories(trackpad_scroll_test PRIVATE
  ${EKA2L1_SOURCE}/src/emu/qt/include
  ${EKA2L1_SOURCE}/src/emu/drivers/include
  ${EKA2L1_SOURCE}/src/emu/common/include
  ${EKA2L1_SOURCE}/src/external/fmt/include
  ${EKA2L1_SOURCE}/src/external/spdlog/include
  ${EKA2L1_BUILD}/src/emu/common/include)
target_link_libraries(trackpad_scroll_test PRIVATE Qt6::Widgets Qt6::OpenGL
  Qt6::Test GTest::gtest)
enable_testing()
add_test(NAME trackpad_scroll COMMAND trackpad_scroll_test)
set_tests_properties(trackpad_scroll PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
