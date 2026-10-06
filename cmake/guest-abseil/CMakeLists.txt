set(ABSL_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(ABSL_PROPAGATE_CXX_STD ON CACHE BOOL "" FORCE)
set(CMAKE_HAVE_LIBC_PTHREAD 1 CACHE INTERNAL "Guest pthread symbols" FORCE)
get_target_property(includes symbian_guest_runtime INTERFACE_INCLUDE_DIRECTORIES)
include_directories(SYSTEM ${includes} "${SYMBIAN_SDK_PREFIX}/include")
add_compile_definitions(_LIBCPP_PROVIDES_DEFAULT_RUNE_TABLE
  _UNICODE __GCC32__ __GCCV3__ __EABI__ __EPOC32__ __MARM__
  __MARM_ARMV5__ __SYMBIAN32__ __LONG_LONG_SUPPORTED _POSIX_C_SOURCE=200112L)
add_compile_options(-fshort-wchar -fno-pic -fno-exceptions -fno-rtti
  "$<$<COMPILE_LANGUAGE:CXX>:-include;${SYMBIAN_LIBCXX_SOURCE}/include/iosfwd>")
add_subdirectory("${SYMBIAN_SDK_PREFIX}/abseil"
  "${CMAKE_CURRENT_BINARY_DIR}/guest-abseil" EXCLUDE_FROM_ALL)
