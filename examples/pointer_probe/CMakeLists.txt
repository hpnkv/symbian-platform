cmake_minimum_required(VERSION 3.28)
project(pointer_probe LANGUAGES CXX ASM)
include(SymbianPic)
symbian_add_pic_executable(pointer_probe
  STARTUP startup.S LINKER_SCRIPT image.ld
  SOURCES probe.cc callbacks.cc transform.cc)
