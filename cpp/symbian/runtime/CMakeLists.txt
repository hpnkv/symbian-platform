# Bounded no-exceptions guest runtime: actual LLVM sources and Symbian heap.
# It deliberately lives outside the host target graph.
enable_language(C)
foreach(input SYMBIAN_LIBCXX_SOURCE SYMBIAN_OPENC_SOURCE SYMBIAN_GUI_SDK_INCLUDE)
  if(NOT IS_DIRECTORY "${${input}}")
    message(FATAL_ERROR "${input} must name preserved target sources/headers")
  endif()
endforeach()
set(_LIBCPP_ABI_VERSION 1)
set(_LIBCPP_ABI_NAMESPACE __symbian)
set(_LIBCPP_HAS_UNICODE 1)
set(_LIBCPP_HAS_THREADS 1)
set(_LIBCPP_HAS_MONOTONIC_CLOCK 1)
set(_LIBCPP_HAS_THREAD_API_PTHREAD 1)
option(SYMBIAN_RUNTIME_LOCALE_STREAM
  "Build the bounded classic-C locale and iostream runtime profile" OFF)
option(SYMBIAN_RUNTIME_NATIVE_ATOMIC64
  "Use original EUSER lock-free 64-bit operations (verified ROM required)" OFF)
if(SYMBIAN_RUNTIME_LOCALE_STREAM)
  set(_LIBCPP_HAS_LOCALIZATION 1)
  # Abseil's formatting dependency instantiates std::wstring and
  # std::wstring_view even when the application formats only narrow strings.
  # Keep one consistent configured libc++ surface for that archive and its
  # consumers; guest wchar_t is the original 16-bit platform ABI.
  set(_LIBCPP_HAS_WIDE_CHARACTERS 1)
endif()
set(_LIBCPP_DISABLE_VISIBILITY_ANNOTATIONS ON)
set(_LIBCPP_PSTL_BACKEND_SERIAL ON)
set(_LIBCPP_HARDENING_MODE_DEFAULT 2)
set(_LIBCPP_ASSERTION_SEMANTIC_DEFAULT 32)
# All other configured optional facilities are disabled by configure_file's
# cmakedefine01. The library and its consumers receive this identical config.
set(config "${CMAKE_CURRENT_BINARY_DIR}/include")
file(MAKE_DIRECTORY "${config}")
configure_file("${SYMBIAN_LIBCXX_SOURCE}/include/__config_site.in"
               "${config}/__config_site" @ONLY)
configure_file("${SYMBIAN_LIBCXX_SOURCE}/vendor/llvm/default_assertion_handler.in"
               "${config}/__assertion_handler" COPYONLY)
file(CREATE_LINK "${SYMBIAN_OPENC_SOURCE}/include" "${config}/stdapis" SYMBOLIC)
execute_process(COMMAND "${CMAKE_CXX_COMPILER}" -print-resource-dir
                OUTPUT_VARIABLE resource OUTPUT_STRIP_TRAILING_WHITESPACE
                COMMAND_ERROR_IS_FATAL ANY)
set(builtins "${SYMBIAN_LIBCXX_SOURCE}/../compiler-rt/lib/builtins")
add_library(symbian_guest_runtime STATIC
  "${builtins}/arm/divsi3.S" "${builtins}/arm/udivsi3.S"
  "${builtins}/arm/divmodsi4.S" "${builtins}/arm/udivmodsi4.S"
  "${builtins}/arm/aeabi_idivmod.S" "${builtins}/arm/aeabi_uidivmod.S"
  "${builtins}/arm/aeabi_ldivmod.S" "${builtins}/arm/aeabi_uldivmod.S"
  "${builtins}/arm/aeabi_memcpy.S" "${builtins}/arm/aeabi_memmove.S"
  "${builtins}/arm/fixunssfsi.S"
  "${builtins}/arm/floatunsisf.S" "${builtins}/arm/mulsf3.S"
  "${builtins}/arm/addsf3.S" "${builtins}/arm/fixsfsi.S"
  "${builtins}/arm/fixunssfdi.S" "${builtins}/floatundisf.c"
  "${builtins}/divsf3.c" "${builtins}/arm/comparesf2.S"
  "${builtins}/arm/aeabi_fcmp.S"
  # The pinned ARM sources are patched only to avoid ARMv6T2 MOVW.
  "${builtins}/arm/adddf3.S" "${builtins}/arm/muldf3.S"
  "${builtins}/arm/divdf3.S"
  "${builtins}/arm/dnan2.c" "${builtins}/arm/dnorm2.c"
  "${builtins}/arm/dunder.c"
  "${builtins}/arm/aeabi_dcmp.S"
  "${builtins}/arm/cmpdf2.S" "${builtins}/arm/gedf2.S"
  "${builtins}/arm/unorddf2.S" "${builtins}/arm/fixdfsi.S"
  "${builtins}/arm/fixunsdfsi.S" "${builtins}/arm/fixdfdi.S"
  "${builtins}/arm/fixunsdfdi.S" "${builtins}/arm/floatsidf.S"
  "${builtins}/arm/floatunsidf.S" "${builtins}/arm/floatdidf.S"
  "${builtins}/arm/floatundidf.S" "${builtins}/arm/extendsfdf2.S"
  "${builtins}/arm/truncdfsf2.S"
  "${builtins}/arm/funder.c" "${builtins}/arm/fnorm2.c"
  "${builtins}/arm/fnan2.c"
  "${builtins}/divmoddi4.c" "${builtins}/udivmoddi4.c"
  "${builtins}/muldi3.c" "${builtins}/ashldi3.c"
  "${builtins}/lshrdi3.c"
  "${builtins}/clzsi2.c"
  heap.cc sdk_heap.cc sdk_pages.cc sdk_clock.cc clock.cc sdk_timer.cc sdk_property.cc atomic_bridge.cc math_c.cc lifecycle.cc sdk_lifecycle.cc
  "${SYMBIAN_LIBCXX_SOURCE}/src/string.cpp"
  "${SYMBIAN_LIBCXX_SOURCE}/src/new_helpers.cpp"
  "${SYMBIAN_LIBCXX_SOURCE}/src/memory.cpp"
  "${SYMBIAN_LIBCXX_SOURCE}/src/thread.cpp"
  "${SYMBIAN_LIBCXX_SOURCE}/src/mutex.cpp"
  "${SYMBIAN_LIBCXX_SOURCE}/src/mutex_destructor.cpp"
  "${SYMBIAN_LIBCXX_SOURCE}/src/condition_variable.cpp"
  "${SYMBIAN_LIBCXX_SOURCE}/src/condition_variable_destructor.cpp"
  "${SYMBIAN_LIBCXX_SOURCE}/src/future.cpp"
  "${SYMBIAN_LIBCXX_SOURCE}/src/error_category.cpp"
  "${SYMBIAN_LIBCXX_SOURCE}/src/system_error.cpp")
target_sources(symbian_guest_runtime PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../concurrency/guest/arm_fiber_context.S"
  "${SYMBIAN_LIBCXX_SOURCE}/src/hash.cpp"
  "${SYMBIAN_LIBCXX_SOURCE}/src/functional.cpp"
  "${SYMBIAN_LIBCXX_SOURCE}/src/chrono.cpp")
if(SYMBIAN_RUNTIME_NATIVE_ATOMIC64)
  target_compile_definitions(symbian_guest_runtime PRIVATE
    SYMBIAN_RUNTIME_NATIVE_ATOMIC64=1)
else()
  target_sources(symbian_guest_runtime PRIVATE sdk_atomic64.cc)
endif()
if(SYMBIAN_RUNTIME_LOCALE_STREAM)
  set(stream_sources
    "${SYMBIAN_LIBCXX_SOURCE}/src/locale.cpp"
    "${SYMBIAN_LIBCXX_SOURCE}/src/ios.cpp"
    "${SYMBIAN_LIBCXX_SOURCE}/src/ostream.cpp"
    "${SYMBIAN_LIBCXX_SOURCE}/src/iostream.cpp"
    "${SYMBIAN_LIBCXX_SOURCE}/src/strstream.cpp"
    "${SYMBIAN_LIBCXX_SOURCE}/../libcxxabi/src/stdlib_exception.cpp"
    "${SYMBIAN_LIBCXX_SOURCE}/src/ios.instantiations.cpp"
    "${SYMBIAN_LIBCXX_SOURCE}/src/call_once.cpp"
    "${SYMBIAN_LIBCXX_SOURCE}/src/stdexcept.cpp")
  target_sources(symbian_guest_runtime PRIVATE ${stream_sources}
    locale_c.cc exception_count.cc)
  set_source_files_properties(${stream_sources}
    PROPERTIES COMPILE_DEFINITIONS _LIBCPP_BUILDING_LIBRARY
               COMPILE_OPTIONS -fno-pic)
  target_compile_definitions(symbian_guest_runtime PUBLIC
    _LIBCPP_PROVIDES_DEFAULT_RUNE_TABLE)
endif()
set_source_files_properties(
  "${SYMBIAN_LIBCXX_SOURCE}/src/string.cpp"
  "${SYMBIAN_LIBCXX_SOURCE}/src/new_helpers.cpp"
  "${SYMBIAN_LIBCXX_SOURCE}/src/memory.cpp"
  "${SYMBIAN_LIBCXX_SOURCE}/src/thread.cpp"
  "${SYMBIAN_LIBCXX_SOURCE}/src/mutex.cpp"
  "${SYMBIAN_LIBCXX_SOURCE}/src/mutex_destructor.cpp"
  "${SYMBIAN_LIBCXX_SOURCE}/src/condition_variable.cpp"
  "${SYMBIAN_LIBCXX_SOURCE}/src/condition_variable_destructor.cpp"
  "${SYMBIAN_LIBCXX_SOURCE}/src/future.cpp"
  "${SYMBIAN_LIBCXX_SOURCE}/src/error_category.cpp"
  "${SYMBIAN_LIBCXX_SOURCE}/src/system_error.cpp"
  "${SYMBIAN_LIBCXX_SOURCE}/src/hash.cpp"
  "${SYMBIAN_LIBCXX_SOURCE}/src/functional.cpp"
  "${SYMBIAN_LIBCXX_SOURCE}/src/chrono.cpp"
  PROPERTIES COMPILE_DEFINITIONS _LIBCPP_BUILDING_LIBRARY)
set_source_files_properties("${SYMBIAN_LIBCXX_SOURCE}/src/chrono.cpp"
  PROPERTIES COMPILE_DEFINITIONS "_LIBCPP_BUILDING_LIBRARY;_POSIX_TIMERS=1")
# libc++'s thread-local-data path addresses mutable static storage. PIC emits
# PC-relative code-to-data references that E32 cannot rebase independently;
# absolute references remain typed E32 data relocations.
set_source_files_properties("${SYMBIAN_LIBCXX_SOURCE}/src/thread.cpp"
  PROPERTIES COMPILE_OPTIONS -fno-pic)
# The process-wide atomic64 lock/state is mutable .bss. It must use E32 data
# relocations, not a PC-relative text-to-data reference.
set_source_files_properties(sdk_atomic64.cc PROPERTIES COMPILE_OPTIONS -fno-pic)
set_source_files_properties(clock.cc PROPERTIES COMPILE_OPTIONS -fno-pic)
# Error categories use local static storage. Emit absolute references so the
# existing E32 data relocation pass can rebase them independently of code.
set_source_files_properties(
  "${SYMBIAN_LIBCXX_SOURCE}/src/error_category.cpp"
  "${SYMBIAN_LIBCXX_SOURCE}/src/system_error.cpp"
  PROPERTIES COMPILE_OPTIONS -fno-pic)
target_compile_features(symbian_guest_runtime PUBLIC cxx_std_20)
target_compile_options(symbian_guest_runtime PUBLIC
  -fPIC -fshort-wchar -fvisibility=hidden -fno-exceptions -fno-rtti -ffunction-sections -fdata-sections)
target_compile_definitions(symbian_guest_runtime PUBLIC
  _UNICODE __GCC32__ __GCCV3__ __EABI__ __EPOC32__ __MARM__ __MARM_ARMV5__
  __SYMBIAN32__ __LONG_LONG_SUPPORTED _POSIX_C_SOURCE=200112L)
target_include_directories(symbian_guest_runtime SYSTEM BEFORE PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${config}" "${SYMBIAN_LIBCXX_SOURCE}/include" "${resource}/include"
  "${SYMBIAN_GUI_SDK_INCLUDE}" "${SYMBIAN_OPENC_SOURCE}/include"
  "${SYMBIAN_OPENC_SOURCE}/libm/include"
  "${SYMBIAN_OPENC_SOURCE}/libc/inc"
  "${SYMBIAN_OPENC_SOURCE}/libpthread/inc"
  "${SYMBIAN_OPENC_SOURCE}/include/posix4")

target_include_directories(symbian_guest_runtime PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
