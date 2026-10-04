# The SDK owns the ARM ABI. This adapter adds its currently missing C-language
# compiler setup; remove it once the SDK's toolchain supplies C directly.
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES SYMBIAN_SDK_PREFIX)
set(CMAKE_CXX_COMPILER "${SYMBIAN_SDK_PREFIX}/bin/clang++" CACHE FILEPATH "SDK compiler")
set(CMAKE_LINKER "${SYMBIAN_SDK_PREFIX}/bin/ld.lld" CACHE FILEPATH "SDK linker")
set(CMAKE_AR "${SYMBIAN_SDK_PREFIX}/bin/llvm-ar" CACHE FILEPATH "SDK archiver")
set(CMAKE_RANLIB "${SYMBIAN_SDK_PREFIX}/bin/llvm-ranlib" CACHE FILEPATH "SDK archive indexer")
include("${SYMBIAN_SDK_PREFIX}/cmake/symbian-arm.cmake")
set(CMAKE_C_COMPILER "${CMAKE_CXX_COMPILER}" CACHE FILEPATH "SDK C compiler")
set(CMAKE_C_COMPILER_TARGET "${CMAKE_CXX_COMPILER_TARGET}")
string(REPLACE "-fno-rtti" "" c_flags "${CMAKE_CXX_FLAGS_INIT}")
set(CMAKE_C_FLAGS_INIT "${c_flags} -x c")
# Final executables use the SDK's C++ link rule and runtime.
