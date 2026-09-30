include_guard(GLOBAL)

function(_symbian_project_file output filename)
  file(REAL_PATH "${filename}" resolved BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
  cmake_path(IS_PREFIX CMAKE_SOURCE_DIR "${resolved}" NORMALIZE inside)
  if(NOT inside OR NOT EXISTS "${resolved}" OR IS_DIRECTORY "${resolved}")
    message(FATAL_ERROR "Symbian target input must be a file within the project: ${filename}")
  endif()
  set(${output} "${resolved}" PARENT_SCOPE)
endfunction()

function(symbian_add_import_executable target)
  if(NOT SYMBIAN_IMPORT_PROXIES)
    message(FATAL_ERROR "Import experiment requires SYMBIAN_IMPORT_PROXIES")
  endif()
  symbian_add_pic_executable(${target} ${ARGN})
  target_link_options(${target} PRIVATE --hash-style=sysv --no-dynamic-linker)
  target_link_libraries(${target} PRIVATE ${SYMBIAN_IMPORT_PROXIES})
endfunction()

function(symbian_add_pic_executable target)
  cmake_parse_arguments(PARSE_ARGV 1 PIC "" "STARTUP;LINKER_SCRIPT" "SOURCES")
  if(PIC_UNPARSED_ARGUMENTS OR PIC_KEYWORDS_MISSING_VALUES OR
     NOT PIC_STARTUP OR NOT PIC_LINKER_SCRIPT OR NOT PIC_SOURCES)
    message(FATAL_ERROR "symbian_add_pic_executable needs STARTUP, LINKER_SCRIPT and SOURCES")
  endif()
  if(NOT CMAKE_SYSTEM_NAME STREQUAL "Generic" OR
     NOT CMAKE_CXX_COMPILER_TARGET STREQUAL "armv5t-none-eabi")
    message(FATAL_ERROR "Use the Symbian armv5t-pic.cmake toolchain")
  endif()
  _symbian_project_file(startup "${PIC_STARTUP}")
  _symbian_project_file(script "${PIC_LINKER_SCRIPT}")
  set(sources)
  foreach(source IN LISTS PIC_SOURCES)
    _symbian_project_file(resolved "${source}")
    list(APPEND sources "${resolved}")
  endforeach()
  add_executable(${target} "${startup}" ${sources})
  target_compile_options(${target} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fPIC>)
  target_link_options(${target} PRIVATE -T "${script}")
  set_target_properties(${target} PROPERTIES
    SUFFIX ".elf" LINK_DEPENDS "${script}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endfunction()

# The ELF transport remains ET_EXEC with a single RX segment; native conversion
# creates the Symbian DLL identity, frozen exports and E32 relocation section.
function(symbian_add_pic_dll target)
  symbian_add_pic_executable(${target} ${ARGN})
  if(SYMBIAN_IMPORT_PROXIES)
    target_link_options(${target} PRIVATE --hash-style=sysv --no-dynamic-linker)
    target_link_libraries(${target} PRIVATE ${SYMBIAN_IMPORT_PROXIES})
  endif()
endfunction()
