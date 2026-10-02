# Installed Abseil Status/StatusOr closure built from the A11-pinned sources.
# The alternate streams runtime supplies the matching libc++ configuration.
set(_symbian_abseil_root
  "${SYMBIAN_SDK_PREFIX}/lib/${SYMBIAN_TARGET_ARCH}/abseil")
if(EXISTS "${_symbian_abseil_root}/libabsl_statusor.a" AND
   EXISTS "${SYMBIAN_SDK_PREFIX}/include/abseil/absl/status/statusor.h" AND
   TARGET Symbian::Streams)
  file(GLOB _symbian_abseil_archives
    "${_symbian_abseil_root}/libabsl_*.a")
  list(SORT _symbian_abseil_archives)
  add_library(SymbianAbseilStatusOr INTERFACE)
  target_include_directories(SymbianAbseilStatusOr SYSTEM INTERFACE
    "${SYMBIAN_SDK_PREFIX}/include/abseil")
  target_link_libraries(SymbianAbseilStatusOr INTERFACE
    --start-group ${_symbian_abseil_archives} --end-group
    Symbian::Streams
    "${SYMBIAN_SDK_PREFIX}/proxies/euser/euser.dso")
  add_library(Symbian::AbseilStatusOr ALIAS SymbianAbseilStatusOr)
endif()
