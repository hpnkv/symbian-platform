# Experimental ARM EABI toolchain; no target SDK or hosted C++ runtime.
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

find_program(CMAKE_CXX_COMPILER NAMES clang++ REQUIRED)
set(CMAKE_ASM_COMPILER "${CMAKE_CXX_COMPILER}")
find_program(CMAKE_LINKER NAMES ld.lld REQUIRED)
set(CMAKE_CXX_COMPILER_TARGET armv5t-none-eabi)
set(CMAKE_ASM_COMPILER_TARGET armv5t-none-eabi)
set(CMAKE_CXX_FLAGS_INIT
    "-mthumb -mfloat-abi=soft -mabi=aapcs -ffreestanding -fno-exceptions -fno-rtti -nostdinc -O2")
set(CMAKE_ASM_FLAGS_INIT "--target=armv5t-none-eabi -marm -nostdinc")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Call LLD directly so Apple's Mach-O linker cannot enter the target link.
# Do not resolve the ld.lld symlink: its argv[0] selects the LLVM driver.
set(CMAKE_CXX_LINK_EXECUTABLE
    "<CMAKE_LINKER> -m armelf --no-undefined --emit-relocs --build-id=none <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>")
list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}")
