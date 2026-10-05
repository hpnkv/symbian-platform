cmake_minimum_required(VERSION 3.28)
project(pointer_probe LANGUAGES CXX ASM)
include(SymbianPic)
symbian_add_executable(pointer_probe probe.cc callbacks.cc transform.cc interworking.S)
