cmake_minimum_required(VERSION 3.28)
project(runtime_probe LANGUAGES CXX ASM)
include(SymbianPic)
set(SYMBIAN_PLATFORM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." CACHE PATH
    "Workspace containing the guest runtime component")
set(root "${SYMBIAN_PLATFORM_ROOT}")
set(SYMBIAN_LIBCXX_SOURCE "${root}/research/upstream/llvm-project/libcxx"
    CACHE PATH "Pinned LLVM 23.1.2 libc++ sources")
set(SYMBIAN_OPENC_SOURCE "${root}/research/upstream/ossrv/genericopenlibs/openenvcore"
    CACHE PATH "Preserved OpenC C headers")
set(SYMBIAN_GUI_SDK_INCLUDE "${root}/.symbian/gui-sdk/include"
    CACHE PATH "Prepared Symbian platform headers")
option(SYMBIAN_RUNTIME_USE_SDK "Link the installed standard runtime" OFF)
if(SYMBIAN_RUNTIME_EVENT_EXECUTOR OR SYMBIAN_RUNTIME_USE_SDK OR
   SYMBIAN_RUNTIME_DEVICE_API)
  include(SymbianApp)
else()
  add_subdirectory("${root}/cpp/symbian/runtime" "runtime")
endif()
option(SYMBIAN_RUNTIME_EXCEPTIONS "Exercise guest exception ABI and unwinding" OFF)
option(SYMBIAN_RUNTIME_EXCEPTION_METADATA_ONLY
  "Preserve unwind metadata without executing a throw" OFF)
option(SYMBIAN_RUNTIME_CHANGED_EXCEPTION_METADATA
  "Fail the exception metadata call result" OFF)
symbian_add_executable(runtime_probe probe.cc got_contract.cc data_contract.cc)
if(SYMBIAN_RUNTIME_EXCEPTIONS)
  if(SYMBIAN_RUNTIME_EXCEPTION_METADATA_ONLY)
    target_sources(runtime_probe PRIVATE exception_metadata_contract.cc
      exception_metadata_call.cc)
    set_source_files_properties(exception_metadata_contract.cc
      PROPERTIES COMPILE_OPTIONS "-fexceptions;-frtti;-fno-pic")
    if(SYMBIAN_RUNTIME_CHANGED_EXCEPTION_METADATA)
      target_compile_definitions(runtime_probe PRIVATE
        SYMBIAN_RUNTIME_CHANGED_EXCEPTION_METADATA=1)
    endif()
  else()
    target_sources(runtime_probe PRIVATE exception_contract.cc)
    set_source_files_properties(exception_contract.cc
      PROPERTIES COMPILE_OPTIONS "-fexceptions;-frtti;-fno-pic")
  endif()
  target_sources(runtime_probe PRIVATE
    exception_personality.S)
  target_compile_definitions(runtime_probe PRIVATE SYMBIAN_RUNTIME_EXCEPTIONS=1)
endif()
if(SYMBIAN_RUNTIME_EVENT_EXECUTOR OR SYMBIAN_RUNTIME_DEVICE_API)
  target_link_libraries(runtime_probe PRIVATE Symbian::Stackless)
elseif(SYMBIAN_RUNTIME_USE_SDK)
  target_link_libraries(runtime_probe PRIVATE Symbian::Runtime)
else()
  target_link_libraries(runtime_probe PRIVATE symbian_guest_runtime)
endif()
if(SYMBIAN_RUNTIME_EVENT_EXECUTOR OR SYMBIAN_RUNTIME_USE_SDK OR
   SYMBIAN_RUNTIME_DEVICE_API)
  target_include_directories(runtime_probe PRIVATE
    "${root}/cpp/symbian/runtime")
endif()
target_link_options(runtime_probe PRIVATE --gc-sections)
option(SYMBIAN_RUNTIME_FAIL_ALLOCATION "Exercise ordinary-new failure exit" OFF)
if(SYMBIAN_RUNTIME_FAIL_ALLOCATION)
  target_compile_definitions(runtime_probe PRIVATE SYMBIAN_RUNTIME_FAIL_ALLOCATION=1)
endif()
option(SYMBIAN_RUNTIME_GLOBAL_NOTHROW "Exercise local object/function GOT relocation" OFF)
if(SYMBIAN_RUNTIME_GLOBAL_NOTHROW)
  target_compile_definitions(runtime_probe PRIVATE SYMBIAN_RUNTIME_GLOBAL_NOTHROW=1)
endif()
option(SYMBIAN_RUNTIME_CHANGED_GOT_VALUE "Fail the independently read GOT value" OFF)
if(SYMBIAN_RUNTIME_CHANGED_GOT_VALUE)
  target_compile_definitions(runtime_probe PRIVATE SYMBIAN_RUNTIME_CHANGED_GOT_VALUE=1)
endif()

option(SYMBIAN_RUNTIME_DATA "Exercise independent data/BSS fixups" OFF)
if(SYMBIAN_RUNTIME_DATA)
  target_compile_definitions(runtime_probe PRIVATE SYMBIAN_RUNTIME_DATA=1)
endif()
option(SYMBIAN_RUNTIME_CHANGED_DATA "Fail initialized data value" OFF)
if(SYMBIAN_RUNTIME_CHANGED_DATA)
  target_compile_definitions(runtime_probe PRIVATE SYMBIAN_RUNTIME_CHANGED_DATA=1)
endif()
option(SYMBIAN_RUNTIME_CHANGED_WIDE "Fail 64-bit arithmetic control" OFF)
if(SYMBIAN_RUNTIME_CHANGED_WIDE)
  target_compile_definitions(runtime_probe PRIVATE SYMBIAN_RUNTIME_CHANGED_WIDE=1)
endif()
option(SYMBIAN_RUNTIME_FAST_LOCK "Exercise native RFastLock fast path" OFF)
if(SYMBIAN_RUNTIME_FAST_LOCK)
  target_sources(runtime_probe PRIVATE fast_lock_bridge.cc)
  target_compile_definitions(runtime_probe PRIVATE SYMBIAN_RUNTIME_FAST_LOCK=1)
endif()
option(SYMBIAN_RUNTIME_CHUNK "Exercise native page-source lifetime" OFF)
if(SYMBIAN_RUNTIME_CHUNK)
  target_sources(runtime_probe PRIVATE chunk_bridge.cc)
  target_compile_definitions(runtime_probe PRIVATE SYMBIAN_RUNTIME_CHUNK=1)
endif()
option(SYMBIAN_RUNTIME_TLS "Exercise POSIX thread-key lifetime" OFF)
if(SYMBIAN_RUNTIME_TLS)
  target_sources(runtime_probe PRIVATE tls_contract.cc)
  target_compile_definitions(runtime_probe PRIVATE SYMBIAN_RUNTIME_TLS=1)
  target_compile_options(runtime_probe PRIVATE -fno-pic)
endif()
option(SYMBIAN_RUNTIME_CHANGED_TLS "Fail TLS contract control" OFF)
if(SYMBIAN_RUNTIME_CHANGED_TLS)
  target_compile_definitions(runtime_probe PRIVATE SYMBIAN_RUNTIME_CHANGED_TLS=1)
endif()
option(SYMBIAN_RUNTIME_CHANGED_CHUNK "Fail native chunk control" OFF)
if(SYMBIAN_RUNTIME_CHANGED_CHUNK)
  target_compile_definitions(runtime_probe PRIVATE
    SYMBIAN_RUNTIME_CHANGED_CHUNK=1)
endif()
option(SYMBIAN_RUNTIME_ATOMIC "Exercise 32-bit C++ atomic operations" OFF)
option(SYMBIAN_RUNTIME_COMPILER_RT
  "Exercise original ARM EABI copy and float conversion helpers" OFF)
if(SYMBIAN_RUNTIME_COMPILER_RT)
  target_sources(runtime_probe PRIVATE compiler_rt_contract.cc)
  target_compile_definitions(runtime_probe PRIVATE
    SYMBIAN_RUNTIME_COMPILER_RT=1)
endif()
option(SYMBIAN_RUNTIME_HASH_TABLE
  "Exercise libc++ hash-table growth and cleanup" OFF)
if(SYMBIAN_RUNTIME_HASH_TABLE)
  target_sources(runtime_probe PRIVATE hash_table_contract.cc)
  target_compile_definitions(runtime_probe PRIVATE
    SYMBIAN_RUNTIME_HASH_TABLE=1)
endif()
option(SYMBIAN_RUNTIME_CLOCK "Exercise libc++ guest clocks" OFF)
if(SYMBIAN_RUNTIME_CLOCK)
  target_sources(runtime_probe PRIVATE clock_contract.cc)
  target_compile_definitions(runtime_probe PRIVATE SYMBIAN_RUNTIME_CLOCK=1)
endif()
option(SYMBIAN_RUNTIME_CHANGED_CLOCK "Fail guest clock result" OFF)
if(SYMBIAN_RUNTIME_CHANGED_CLOCK)
  target_compile_definitions(runtime_probe PRIVATE
    SYMBIAN_RUNTIME_CHANGED_CLOCK=1)
endif()
option(SYMBIAN_RUNTIME_FAST_COUNTER
  "Measure the emulator fast-counter HAL contract" OFF)
if(SYMBIAN_RUNTIME_FAST_COUNTER)
  target_sources(runtime_probe PRIVATE fast_counter_contract.cc)
  target_compile_definitions(runtime_probe PRIVATE
    SYMBIAN_RUNTIME_FAST_COUNTER=1)
endif()
option(SYMBIAN_RUNTIME_CHANGED_FAST_COUNTER
  "Fail fast-counter result" OFF)
if(SYMBIAN_RUNTIME_CHANGED_FAST_COUNTER)
  target_compile_definitions(runtime_probe PRIVATE
    SYMBIAN_RUNTIME_CHANGED_FAST_COUNTER=1)
endif()
option(SYMBIAN_RUNTIME_CLOCK_THREAD
  "Read the monotonic clock concurrently from two guest threads" OFF)
if(SYMBIAN_RUNTIME_CLOCK_THREAD)
  target_sources(runtime_probe PRIVATE clock_thread_contract.cc)
  target_compile_definitions(runtime_probe PRIVATE
    SYMBIAN_RUNTIME_CLOCK_THREAD=1)
  target_compile_options(runtime_probe PRIVATE -fno-pic)
endif()
option(SYMBIAN_RUNTIME_NATIVE_TIMER
  "Exercise owned thread-relative native timer requests" OFF)
if(SYMBIAN_RUNTIME_NATIVE_TIMER)
  target_sources(runtime_probe PRIVATE native_timer_contract.cc)
  target_compile_definitions(runtime_probe PRIVATE
    SYMBIAN_RUNTIME_NATIVE_TIMER=1)
  target_compile_options(runtime_probe PRIVATE -fno-pic)
  target_include_directories(runtime_probe PRIVATE
    "${root}/cpp/symbian/concurrency/guest"
    "${root}/cpp/symbian/concurrency/common")
endif()
option(SYMBIAN_RUNTIME_CHANGED_NATIVE_TIMER
  "Fail native timer result control" OFF)
if(SYMBIAN_RUNTIME_CHANGED_NATIVE_TIMER)
  target_compile_definitions(runtime_probe PRIVATE
    SYMBIAN_RUNTIME_CHANGED_NATIVE_TIMER=1)
endif()
option(SYMBIAN_RUNTIME_TIMER_FUTURE
  "Exercise timer completion into A11-derived Future with cross-thread wake" OFF)
if(SYMBIAN_RUNTIME_TIMER_FUTURE)
  target_sources(runtime_probe PRIVATE timer_future_contract.cc)
  target_compile_definitions(runtime_probe PRIVATE
    SYMBIAN_RUNTIME_TIMER_FUTURE=1)
  target_compile_options(runtime_probe PRIVATE -fno-pic)
  target_include_directories(runtime_probe PRIVATE
    "${root}/cpp/symbian/concurrency/guest"
    "${root}/cpp/symbian/concurrency/common")
endif()
option(SYMBIAN_RUNTIME_FIBER_CONTEXT
  "Exercise bounded ARM/Thumb context switching without a scheduler" OFF)
if(SYMBIAN_RUNTIME_FIBER_CONTEXT)
  target_sources(runtime_probe PRIVATE fiber_context_contract.cc)
  target_compile_definitions(runtime_probe PRIVATE
    SYMBIAN_RUNTIME_FIBER_CONTEXT=1)
  target_compile_options(runtime_probe PRIVATE -fno-pic)
endif()
option(SYMBIAN_RUNTIME_FIBER_LOCKS
  "Exercise cooperative guest thread::Mutex/CondVar and Future::Await" OFF)
if(SYMBIAN_RUNTIME_FIBER_LOCKS)
  target_sources(runtime_probe PRIVATE fiber_locks_contract.cc)
  if(TARGET Symbian::Fibers)
    target_link_libraries(runtime_probe PRIVATE Symbian::Fibers)
  else()
    target_sources(runtime_probe PRIVATE
      "${root}/cpp/symbian/concurrency/guest/thread/fiber.cc")
  endif()
  target_compile_definitions(runtime_probe PRIVATE SYMBIAN_RUNTIME_FIBER_LOCKS=1)
  target_compile_options(runtime_probe PRIVATE -fno-pic)
  target_include_directories(runtime_probe PRIVATE
    "${root}/cpp/symbian/concurrency/guest"
    "${root}/cpp/symbian/concurrency/common")
endif()
option(SYMBIAN_RUNTIME_CHANGED_FIBER_LOCKS
  "Fail the guest fiber-aware synchronization control" OFF)
if(SYMBIAN_RUNTIME_CHANGED_FIBER_LOCKS)
  target_compile_definitions(runtime_probe PRIVATE
    SYMBIAN_RUNTIME_CHANGED_FIBER_LOCKS=1)
endif()
option(SYMBIAN_RUNTIME_EVENT_EXECUTOR
  "Exercise one timer/property/mailbox/fiber event owner" OFF)
if(SYMBIAN_RUNTIME_EVENT_EXECUTOR)
  target_sources(runtime_probe PRIVATE event_executor_contract.cc)
  if(TARGET Symbian::Fibers)
    target_link_libraries(runtime_probe PRIVATE Symbian::Fibers)
  else()
    target_sources(runtime_probe PRIVATE
      "${root}/cpp/symbian/concurrency/guest/thread/fiber.cc")
  endif()
  target_compile_definitions(runtime_probe PRIVATE
    SYMBIAN_RUNTIME_EVENT_EXECUTOR=1)
  target_compile_options(runtime_probe PRIVATE -fno-pic)
endif()
option(SYMBIAN_RUNTIME_DEVICE_API
  "Exercise the installed system device API component" OFF)
if(SYMBIAN_RUNTIME_DEVICE_API)
  if(NOT TARGET Symbian::System)
    message(FATAL_ERROR "The selected SDK has no Symbian::System component")
  endif()
  if(NOT TARGET Symbian::Power OR NOT TARGET Symbian::Display)
    message(FATAL_ERROR "The selected SDK lacks power or display components")
  endif()
  target_sources(runtime_probe PRIVATE device_api_contract.cc)
  target_link_libraries(runtime_probe PRIVATE Symbian::System Symbian::Power
    Symbian::Display)
  if(TARGET Symbian::Storage)
    target_link_libraries(runtime_probe PRIVATE Symbian::Storage)
    target_compile_definitions(runtime_probe PRIVATE
      SYMBIAN_RUNTIME_DEVICE_API_STORAGE=1)
  endif()
  if(TARGET Symbian::Camera)
    target_link_libraries(runtime_probe PRIVATE Symbian::Camera)
    target_compile_definitions(runtime_probe PRIVATE
      SYMBIAN_RUNTIME_DEVICE_API_CAMERA=1)
  endif()
  target_compile_definitions(runtime_probe PRIVATE SYMBIAN_RUNTIME_DEVICE_API=1)
endif()
option(SYMBIAN_RUNTIME_CHANGED_EVENT_EXECUTOR
  "Fail the event executor result control" OFF)
if(SYMBIAN_RUNTIME_CHANGED_EVENT_EXECUTOR)
  target_compile_definitions(runtime_probe PRIVATE
    SYMBIAN_RUNTIME_CHANGED_EVENT_EXECUTOR=1)
endif()
option(SYMBIAN_RUNTIME_CHANGED_FIBER_CONTEXT
  "Fail the fiber context result control" OFF)
if(SYMBIAN_RUNTIME_CHANGED_FIBER_CONTEXT)
  target_compile_definitions(runtime_probe PRIVATE
    SYMBIAN_RUNTIME_CHANGED_FIBER_CONTEXT=1)
endif()
option(SYMBIAN_RUNTIME_CHANGED_TIMER_FUTURE
  "Fail timer Future result control" OFF)
if(SYMBIAN_RUNTIME_CHANGED_TIMER_FUTURE)
  target_compile_definitions(runtime_probe PRIVATE
    SYMBIAN_RUNTIME_CHANGED_TIMER_FUTURE=1)
endif()
option(SYMBIAN_RUNTIME_CHANGED_CLOCK_THREAD
  "Fail concurrent clock result" OFF)
if(SYMBIAN_RUNTIME_CHANGED_CLOCK_THREAD)
  target_compile_definitions(runtime_probe PRIVATE
    SYMBIAN_RUNTIME_CHANGED_CLOCK_THREAD=1)
endif()
option(SYMBIAN_RUNTIME_THREAD_ERROR
  "Exercise the no-exceptions std::thread failure path" OFF)
if(SYMBIAN_RUNTIME_THREAD_ERROR)
  target_sources(runtime_probe PRIVATE thread_error_contract.cc)
  target_compile_definitions(runtime_probe PRIVATE
    SYMBIAN_RUNTIME_THREAD_ERROR=1)
  target_compile_options(runtime_probe PRIVATE -fno-pic)
endif()
option(SYMBIAN_RUNTIME_CHANGED_HASH_TABLE
  "Fail libc++ hash-table result" OFF)
if(SYMBIAN_RUNTIME_CHANGED_HASH_TABLE)
  target_compile_definitions(runtime_probe PRIVATE
    SYMBIAN_RUNTIME_CHANGED_HASH_TABLE=1)
endif()
option(SYMBIAN_RUNTIME_CHANGED_COMPILER_RT
  "Fail original ARM EABI helper result" OFF)
if(SYMBIAN_RUNTIME_CHANGED_COMPILER_RT)
  target_compile_definitions(runtime_probe PRIVATE
    SYMBIAN_RUNTIME_CHANGED_COMPILER_RT=1)
endif()
option(SYMBIAN_RUNTIME_ATOMIC64
  "Exercise native-backed 64-bit atomics across guest threads" OFF)
if(SYMBIAN_RUNTIME_ATOMIC64)
  target_sources(runtime_probe PRIVATE atomic64_contract.cc thread_bridge.cc)
  target_compile_definitions(runtime_probe PRIVATE SYMBIAN_RUNTIME_ATOMIC64=1)
  if(SYMBIAN_RUNTIME_NATIVE_ATOMIC64)
    target_compile_definitions(runtime_probe PRIVATE
      SYMBIAN_RUNTIME_EXPECT_NATIVE_ATOMIC64=1)
  endif()
endif()
option(SYMBIAN_RUNTIME_CHANGED_ATOMIC64 "Fail 64-bit atomic control" OFF)
if(SYMBIAN_RUNTIME_CHANGED_ATOMIC64)
  target_compile_definitions(runtime_probe PRIVATE
    SYMBIAN_RUNTIME_CHANGED_ATOMIC64=1)
endif()
option(SYMBIAN_RUNTIME_DIRECT_ATOMIC64_DIAGNOSTIC
  "Compare direct original EUSER 64-bit atomic exports" OFF)
if(SYMBIAN_RUNTIME_DIRECT_ATOMIC64_DIAGNOSTIC)
  target_sources(runtime_probe PRIVATE native_atomic64_contract.cc)
  target_compile_definitions(runtime_probe PRIVATE
    SYMBIAN_RUNTIME_DIRECT_ATOMIC64_DIAGNOSTIC=1)
endif()
if(SYMBIAN_RUNTIME_ATOMIC)
  target_sources(runtime_probe PRIVATE atomic_contract.cc)
  target_compile_definitions(runtime_probe PRIVATE SYMBIAN_RUNTIME_ATOMIC=1)
endif()
option(SYMBIAN_RUNTIME_CHANGED_ATOMIC "Fail the independent atomic result" OFF)
if(SYMBIAN_RUNTIME_CHANGED_ATOMIC)
  target_compile_definitions(runtime_probe PRIVATE SYMBIAN_RUNTIME_CHANGED_ATOMIC=1)
endif()
option(SYMBIAN_RUNTIME_THREAD_ATOMIC "Exercise two native threads with C++ atomics" OFF)
if(SYMBIAN_RUNTIME_THREAD_ATOMIC)
  target_sources(runtime_probe PRIVATE thread_bridge.cc thread_atomic_contract.cc)
  target_compile_definitions(runtime_probe PRIVATE SYMBIAN_RUNTIME_THREAD_ATOMIC=1)
endif()
option(SYMBIAN_RUNTIME_CHANGED_THREAD_ATOMIC "Fail the shared counter result" OFF)
if(SYMBIAN_RUNTIME_CHANGED_THREAD_ATOMIC)
  target_compile_definitions(runtime_probe PRIVATE SYMBIAN_RUNTIME_CHANGED_THREAD_ATOMIC=1)
endif()
option(SYMBIAN_RUNTIME_GLOBAL_LIFETIME "Exercise C++ globals and atexit" OFF)
if(SYMBIAN_RUNTIME_GLOBAL_LIFETIME)
  target_sources(runtime_probe PRIVATE global_contract.cc global_finalizers.cc)
  target_compile_definitions(runtime_probe PRIVATE SYMBIAN_RUNTIME_GLOBAL_LIFETIME=1)
endif()
option(SYMBIAN_RUNTIME_OWNERSHIP "Exercise unique/shared/weak ownership" OFF)
if(SYMBIAN_RUNTIME_OWNERSHIP)
  target_sources(runtime_probe PRIVATE ownership_contract.cc)
  target_compile_definitions(runtime_probe PRIVATE SYMBIAN_RUNTIME_OWNERSHIP=1)
  target_compile_options(runtime_probe PRIVATE -fno-pic)
endif()
option(SYMBIAN_RUNTIME_CHANGED_OWNERSHIP "Fail independent ownership control" OFF)
if(SYMBIAN_RUNTIME_CHANGED_OWNERSHIP)
  target_compile_definitions(runtime_probe PRIVATE SYMBIAN_RUNTIME_CHANGED_OWNERSHIP=1)
endif()
option(SYMBIAN_RUNTIME_STD_THREAD "Exercise libc++ std::thread and shared ownership" OFF)
option(SYMBIAN_RUNTIME_ALLOC_BENCH
  "Measure bounded guest allocator burst with the fast counter" OFF)
option(SYMBIAN_RUNTIME_ALLOC_BENCH_PTHREAD
  "Measure imported pthread TLS lookup calls instead" OFF)
option(SYMBIAN_RUNTIME_ALLOC_BENCH_ATOMIC64
  "Measure portable 64-bit atomic fetch-add calls instead" OFF)
if(SYMBIAN_RUNTIME_ALLOC_BENCH_PTHREAD AND
   SYMBIAN_RUNTIME_ALLOC_BENCH_ATOMIC64)
  message(FATAL_ERROR "Choose one allocator benchmark mode")
endif()
if(SYMBIAN_RUNTIME_ALLOC_BENCH)
  target_sources(runtime_probe PRIVATE alloc_bench.cc)
  target_compile_definitions(runtime_probe PRIVATE SYMBIAN_RUNTIME_ALLOC_BENCH=1)
  if(SYMBIAN_RUNTIME_ALLOC_BENCH_PTHREAD)
    target_compile_definitions(runtime_probe PRIVATE
      SYMBIAN_RUNTIME_ALLOC_BENCH_PTHREAD=1)
  endif()
  if(SYMBIAN_RUNTIME_ALLOC_BENCH_ATOMIC64)
    target_compile_definitions(runtime_probe PRIVATE
      SYMBIAN_RUNTIME_ALLOC_BENCH_ATOMIC64=1)
  endif()
endif()
if(SYMBIAN_RUNTIME_STD_THREAD)
  target_sources(runtime_probe PRIVATE std_thread_contract.cc thread_bridge.cc)
  target_compile_definitions(runtime_probe PRIVATE SYMBIAN_RUNTIME_STD_THREAD=1)
  target_compile_options(runtime_probe PRIVATE -fno-pic)
endif()
if(SYMBIAN_RUNTIME_MIMALLOC)
  target_compile_definitions(runtime_probe PRIVATE SYMBIAN_RUNTIME_MIMALLOC=1)
endif()
option(SYMBIAN_RUNTIME_MIMALLOC_SDK_PROBE
  "Exercise the SDK's exported mimalloc header and implementation" OFF)
if(SYMBIAN_RUNTIME_MIMALLOC_SDK_PROBE)
  target_sources(runtime_probe PRIVATE mimalloc_sdk_contract.cc)
  target_compile_definitions(runtime_probe PRIVATE
    SYMBIAN_RUNTIME_MIMALLOC_SDK_PROBE=1)
endif()
option(SYMBIAN_RUNTIME_CHANGED_STD_THREAD "Fail independent std::thread control" OFF)
if(SYMBIAN_RUNTIME_CHANGED_STD_THREAD)
  target_compile_definitions(runtime_probe PRIVATE SYMBIAN_RUNTIME_CHANGED_STD_THREAD=1)
endif()
option(SYMBIAN_RUNTIME_A11_STACKLESS "Exercise A11-derived guest futures" OFF)
if(SYMBIAN_RUNTIME_A11_STACKLESS)
  target_sources(runtime_probe PRIVATE a11_stackless_contract.cc)
  target_compile_definitions(runtime_probe PRIVATE SYMBIAN_RUNTIME_A11_STACKLESS=1)
  target_compile_options(runtime_probe PRIVATE -fno-pic)
  target_include_directories(runtime_probe PRIVATE
    "${root}/cpp/symbian/concurrency/guest"
    "${root}/cpp/symbian/concurrency/common")
endif()
option(SYMBIAN_RUNTIME_CHANGED_A11_STACKLESS "Fail A11 stackless control" OFF)
if(SYMBIAN_RUNTIME_CHANGED_A11_STACKLESS)
  target_compile_definitions(runtime_probe PRIVATE
    SYMBIAN_RUNTIME_CHANGED_A11_STACKLESS=1)
endif()
option(SYMBIAN_RUNTIME_VARARGS "Exercise compiler/OpenC variadic ABI" OFF)
if(SYMBIAN_RUNTIME_VARARGS)
  target_sources(runtime_probe PRIVATE varargs_contract.cc)
  target_compile_definitions(runtime_probe PRIVATE SYMBIAN_RUNTIME_VARARGS=1)
endif()
option(SYMBIAN_RUNTIME_CHANGED_VARARGS "Fail variadic result control" OFF)
if(SYMBIAN_RUNTIME_CHANGED_VARARGS)
  target_compile_definitions(runtime_probe PRIVATE
    SYMBIAN_RUNTIME_CHANGED_VARARGS=1)
endif()
option(SYMBIAN_RUNTIME_SYSTEM_ERROR "Exercise libc++ error categories" OFF)
if(SYMBIAN_RUNTIME_SYSTEM_ERROR)
  target_sources(runtime_probe PRIVATE system_error_contract.cc)
  target_compile_definitions(runtime_probe PRIVATE
    SYMBIAN_RUNTIME_SYSTEM_ERROR=1)
endif()
if(SYMBIAN_RUNTIME_LOCALE_STREAM)
  target_sources(runtime_probe PRIVATE locale_stream_contract.cc
    locale_api_contract.cc)
  set_source_files_properties(locale_api_contract.cc
    PROPERTIES COMPILE_DEFINITIONS _LIBCPP_BUILDING_LIBRARY)
  target_compile_definitions(runtime_probe PRIVATE
    SYMBIAN_RUNTIME_LOCALE_STREAM=1)
endif()
option(SYMBIAN_RUNTIME_CHANGED_LOCALE_STREAM
  "Fail the independent locale/stream result" OFF)
if(SYMBIAN_RUNTIME_CHANGED_LOCALE_STREAM)
  target_compile_definitions(runtime_probe PRIVATE
    SYMBIAN_RUNTIME_CHANGED_LOCALE_STREAM=1)
endif()
option(SYMBIAN_RUNTIME_IMPORT_POINTER "Exercise relocated imported PLT pointer" OFF)
if(SYMBIAN_RUNTIME_IMPORT_POINTER)
  target_sources(runtime_probe PRIVATE import_pointer_contract.cc)
  target_compile_definitions(runtime_probe PRIVATE
    SYMBIAN_RUNTIME_IMPORT_POINTER=1)
endif()
option(SYMBIAN_RUNTIME_LONG_THUNK "Exercise LLD ARM-to-Thumb long thunk" OFF)
if(SYMBIAN_RUNTIME_LONG_THUNK)
  target_sources(runtime_probe PRIVATE long_thunk_contract.S)
  target_compile_definitions(runtime_probe PRIVATE SYMBIAN_RUNTIME_LONG_THUNK=1)
endif()
option(SYMBIAN_RUNTIME_CHANGED_SYSTEM_ERROR "Fail error-category control" OFF)
if(SYMBIAN_RUNTIME_CHANGED_SYSTEM_ERROR)
  target_compile_definitions(runtime_probe PRIVATE
    SYMBIAN_RUNTIME_CHANGED_SYSTEM_ERROR=1)
endif()
