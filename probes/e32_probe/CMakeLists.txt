cmake_minimum_required(VERSION 3.28)
project(symbian_e32_probe LANGUAGES CXX ASM)

include(SymbianPic)
symbian_add_executable(e32_probe probe.cc)
target_compile_options(e32_probe PRIVATE
  -fno-unwind-tables -fno-asynchronous-unwind-tables)
