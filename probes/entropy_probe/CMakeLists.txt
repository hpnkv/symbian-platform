cmake_minimum_required(VERSION 3.28)
project(entropy_probe LANGUAGES C CXX ASM)
include(SymbianApp)
symbian_add_import_executable(entropy_probe
  STARTUP "${SYMBIAN_SDK_PREFIX}/cmake/exe_startup.S"
  LINKER_SCRIPT "${SYMBIAN_SDK_PREFIX}/cmake/exe_image.ld"
  SOURCES probe.cc startup.cc)
target_compile_options(entropy_probe PRIVATE -fhosted -fno-builtin)
target_link_libraries(entropy_probe PRIVATE Symbian::Runtime Symbian::EUser)
target_link_libraries(entropy_probe PRIVATE Symbian::Crypto)
if(SYMBIAN_EXPECT_SECURE_ENTROPY)
  target_compile_definitions(entropy_probe PRIVATE SYMBIAN_EXPECT_SECURE_ENTROPY=1)
endif()
