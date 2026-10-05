cmake_minimum_required(VERSION 3.28)
project(probe LANGUAGES CXX ASM)
include(SymbianPic)
symbian_add_dynamic_library(probe SOURCES probe.cc)
