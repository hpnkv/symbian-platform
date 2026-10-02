# Only the local SDK location is machine-specific; project files stay relative.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_LIST_DIR}/sdk-location.json"
  "${CMAKE_CURRENT_LIST_DIR}/symbian-project.json")
file(READ "${CMAKE_CURRENT_LIST_DIR}/sdk-location.json" location_json)
string(JSON SYMBIAN_SDK_PREFIX GET "${location_json}" sdk)
if(NOT IS_ABSOLUTE "${SYMBIAN_SDK_PREFIX}")
  get_filename_component(SYMBIAN_SDK_PREFIX
    "${CMAKE_CURRENT_LIST_DIR}/${SYMBIAN_SDK_PREFIX}" ABSOLUTE)
endif()
if(NOT DEFINED SYMBIAN_TARGET_ARCH)
  file(READ "${CMAKE_CURRENT_LIST_DIR}/symbian-project.json" project_json)
  string(JSON SYMBIAN_TARGET_ARCH GET "${project_json}" preferences architecture)
endif()
set(CMAKE_TOOLCHAIN_FILE "${SYMBIAN_SDK_PREFIX}/cmake/symbian-arm.cmake")
list(PREPEND CMAKE_FIND_ROOT_PATH "${SYMBIAN_SDK_PREFIX}")
set(CMAKE_CXX_COMPILER "${SYMBIAN_SDK_PREFIX}/bin/clang++"
    CACHE FILEPATH "Project SDK compiler" FORCE)
set(CMAKE_C_COMPILER "${SYMBIAN_SDK_PREFIX}/bin/clang"
    CACHE FILEPATH "Project SDK C compiler" FORCE)
set(CMAKE_ASM_COMPILER "${CMAKE_CXX_COMPILER}" CACHE FILEPATH "SDK assembler" FORCE)
set(CMAKE_LINKER "${SYMBIAN_SDK_PREFIX}/bin/ld.lld"
    CACHE FILEPATH "Project SDK linker" FORCE)
if(EXISTS "${SYMBIAN_SDK_PREFIX}/bin/llvm-ar" AND
   EXISTS "${SYMBIAN_SDK_PREFIX}/bin/llvm-ranlib")
  set(CMAKE_AR "${SYMBIAN_SDK_PREFIX}/bin/llvm-ar"
      CACHE FILEPATH "Project SDK ARM archiver" FORCE)
  set(CMAKE_RANLIB "${SYMBIAN_SDK_PREFIX}/bin/llvm-ranlib"
      CACHE FILEPATH "Project SDK ARM archive indexer" FORCE)
endif()
set(SYMBIAN_IMPORT_PROXIES)
foreach(dll euser ws32 gdi)
  list(APPEND SYMBIAN_IMPORT_PROXIES
       "${SYMBIAN_SDK_PREFIX}/proxies/${dll}/${dll}.dso")
endforeach()
list(PREPEND CMAKE_MODULE_PATH "${SYMBIAN_SDK_PREFIX}/cmake")
