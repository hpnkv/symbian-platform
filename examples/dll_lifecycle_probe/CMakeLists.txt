cmake_minimum_required(VERSION 3.28)
project(dll_lifecycle_probe LANGUAGES CXX ASM)
include(SymbianApp)
symbian_add_dynamic_library(lifecycle_probe
  SOURCES probe.cc EXPORT_DEFINITION exports.def UID3 0xE0000D20
  IMPORT_SYMBOLS SymbianLifecycleState SymbianLifecycleSetSink)
