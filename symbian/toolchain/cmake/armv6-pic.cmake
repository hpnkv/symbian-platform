# Explicit ARMv6 choice; symbian-arm.cmake also defaults to ARMv6.
set(SYMBIAN_TARGET_ARCH armv6 CACHE STRING "Guest ISA: armv6 or armv5t")
include("${CMAKE_CURRENT_LIST_DIR}/symbian-arm.cmake")
