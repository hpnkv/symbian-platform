cmake_minimum_required(VERSION 3.28)
project(symbian_import_probe LANGUAGES CXX ASM)

include(SymbianPic)
symbian_add_import_executable(import_probe
  STARTUP startup.S
  LINKER_SCRIPT image.ld
  SOURCES probe.cc)
