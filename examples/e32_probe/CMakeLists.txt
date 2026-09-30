cmake_minimum_required(VERSION 3.28)
project(symbian_e32_probe LANGUAGES CXX ASM)

include(SymbianPic)
symbian_add_pic_executable(e32_probe
  STARTUP startup.S
  LINKER_SCRIPT image.ld
  SOURCES probe.cc)
