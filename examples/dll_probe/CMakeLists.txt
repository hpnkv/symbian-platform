cmake_minimum_required(VERSION 3.28)
project(probe LANGUAGES CXX ASM)
include(SymbianPic)
symbian_add_pic_dll(probe
  STARTUP startup.S LINKER_SCRIPT image.ld SOURCES probe.cc)
