cmake_minimum_required(VERSION 3.28)
project(probe_impl LANGUAGES CXX ASM)
include(SymbianPic)
symbian_add_pic_executable(probe_impl
 STARTUP startup.S LINKER_SCRIPT image.ld SOURCES probe.cc)
