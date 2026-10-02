# Explicit legacy ARMv5T choice, preserved for existing projects.
set(SYMBIAN_TARGET_ARCH armv5t CACHE STRING "Guest ISA: armv6 or armv5t")
include("${CMAKE_CURRENT_LIST_DIR}/symbian-arm.cmake")
