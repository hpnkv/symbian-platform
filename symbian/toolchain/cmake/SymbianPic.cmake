include_guard(GLOBAL)

function(_symbian_project_file output filename)
  file(REAL_PATH "${filename}" resolved BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
  cmake_path(IS_PREFIX CMAKE_SOURCE_DIR "${resolved}" NORMALIZE inside)
  set(sdk_cmake "${SYMBIAN_SDK_PREFIX}/cmake")
  cmake_path(IS_PREFIX sdk_cmake "${resolved}" NORMALIZE inside_sdk)
  if((NOT inside AND NOT inside_sdk) OR NOT EXISTS "${resolved}" OR
     IS_DIRECTORY "${resolved}")
    message(FATAL_ERROR "Symbian target input must be a project or selected SDK file: ${filename}")
  endif()
  set(${output} "${resolved}" PARENT_SCOPE)
endfunction()

function(symbian_add_import_executable target)
  if(NOT SYMBIAN_IMPORT_PROXIES)
    message(FATAL_ERROR "Imported application requires SYMBIAN_IMPORT_PROXIES")
  endif()
  symbian_add_pic_executable(${target} ${ARGN})
  # A registered application owns native sources in its source directory and
  # conventional src/cpp/include trees. CMake tracks additions so new app
  # files join the real guest target and IDE compile context automatically.
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/symbian.toml")
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/symbian.toml"
      app_section REGEX "^\\[application\\][ \t]*$")
    if(app_section)
      set(native_patterns "${CMAKE_CURRENT_SOURCE_DIR}/*")
      file(GLOB native_top CONFIGURE_DEPENDS ${native_patterns})
      set(native_files ${native_top})
      foreach(source_tree IN ITEMS src cpp include)
        if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${source_tree}")
          file(GLOB_RECURSE native_tree CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/${source_tree}/*")
          list(APPEND native_files ${native_tree})
        endif()
      endforeach()
      get_target_property(registered_sources ${target} SOURCES)
      foreach(native_file IN LISTS native_files)
        if(native_file MATCHES "\\.(c|cc|cpp|cxx|h|hh|hpp|hxx|S|s)$")
          list(FIND registered_sources "${native_file}" source_index)
          if(source_index EQUAL -1)
            target_sources(${target} PRIVATE "${native_file}")
          endif()
        endif()
      endforeach()
    endif()
  endif()
  # A project declares every proxy the converter may recognize. Only used
  # imports may become E32 DLL dependencies, including on older firmware.
  target_link_options(${target} PRIVATE --hash-style=sysv --no-dynamic-linker
    --as-needed)
  target_link_libraries(${target} PRIVATE ${SYMBIAN_IMPORT_PROXIES})
endfunction()

function(symbian_add_pic_executable target)
  cmake_parse_arguments(PARSE_ARGV 1 PIC "" "STARTUP;LINKER_SCRIPT" "SOURCES")
  if(PIC_UNPARSED_ARGUMENTS OR PIC_KEYWORDS_MISSING_VALUES OR
     NOT PIC_STARTUP OR NOT PIC_LINKER_SCRIPT OR NOT PIC_SOURCES)
    message(FATAL_ERROR "symbian_add_pic_executable needs STARTUP, LINKER_SCRIPT and SOURCES")
  endif()
  if(NOT CMAKE_SYSTEM_NAME STREQUAL "Generic" OR
     NOT (CMAKE_CXX_COMPILER_TARGET MATCHES "^arm(v5t|v6)-none-eabi$" OR
          CMAKE_C_COMPILER_TARGET MATCHES "^arm(v5t|v6)-none-eabi$"))
    message(FATAL_ERROR "Use the Symbian symbian-arm.cmake toolchain (armv6 or armv5t)")
  endif()
  _symbian_project_file(startup "${PIC_STARTUP}")
  _symbian_project_file(script "${PIC_LINKER_SCRIPT}")
  set(sources)
  foreach(source IN LISTS PIC_SOURCES)
    _symbian_project_file(resolved "${source}")
    list(APPEND sources "${resolved}")
  endforeach()
  add_executable(${target} "${startup}" ${sources})
  target_compile_options(${target} PRIVATE -fPIC)
  target_link_options(${target} PRIVATE -T "${script}")
  set_target_properties(${target} PROPERTIES
    SUFFIX ".elf" LINK_DEPENDS "${script}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endfunction()

# The ELF transport remains ET_EXEC; conversion creates E32 DLL identity,
# frozen exports, independent RX/RW mappings and relocation sections.
function(symbian_add_pic_dll target)
  symbian_add_pic_executable(${target} ${ARGN})
  if(SYMBIAN_IMPORT_PROXIES)
    target_link_options(${target} PRIVATE --hash-style=sysv --no-dynamic-linker
      --as-needed)
    target_link_libraries(${target} PRIVATE ${SYMBIAN_IMPORT_PROXIES})
  endif()
endfunction()

# Publish an E32 DLL from an ordinary CMake target. The ELF remains the debug
# symbol file. IMPORT_SYMBOLS creates a consumer-side ordinal proxy target.
# RUNTIME_TARGET selects one runtime for the default startup; use
# Symbian::Streams when linking Abseil-based SDK targets.
function(symbian_add_dynamic_library target)
  cmake_parse_arguments(PARSE_ARGV 1 DLL "" "STARTUP;LINKER_SCRIPT;EXPORT_DEFINITION;UID3;RUNTIME_TARGET"
                        "SOURCES;IMPORT_SYMBOLS;IMPORT_PROXIES")
  if(DLL_UNPARSED_ARGUMENTS OR DLL_KEYWORDS_MISSING_VALUES OR
     NOT DLL_SOURCES OR
     NOT DLL_EXPORT_DEFINITION OR NOT DLL_UID3)
    message(FATAL_ERROR "symbian_add_dynamic_library needs SOURCES, EXPORT_DEFINITION and UID3")
  endif()
  if(NOT SYMBIAN_SDK_PREFIX OR
     NOT EXISTS "${SYMBIAN_SDK_PREFIX}/bin/symbian")
    message(FATAL_ERROR "Dynamic-library publishing requires an installed Symbian SDK")
  endif()
  _symbian_project_file(definition "${DLL_EXPORT_DEFINITION}")
  set(default_startup FALSE)
  if(NOT DLL_STARTUP)
    set(DLL_STARTUP "${SYMBIAN_SDK_PREFIX}/cmake/dll_startup.S")
    set(default_startup TRUE)
  endif()
  if(NOT DLL_LINKER_SCRIPT)
    set(DLL_LINKER_SCRIPT "${SYMBIAN_SDK_PREFIX}/cmake/dll_image.ld")
  endif()
  set(elf_target "${target}_elf")
  symbian_add_pic_dll(${elf_target} STARTUP "${DLL_STARTUP}"
    LINKER_SCRIPT "${DLL_LINKER_SCRIPT}" SOURCES ${DLL_SOURCES})
  if(default_startup)
    if(NOT DLL_RUNTIME_TARGET)
      set(DLL_RUNTIME_TARGET Symbian::Runtime)
    endif()
    if(NOT TARGET ${DLL_RUNTIME_TARGET})
      message(FATAL_ERROR "The SDK DLL entry needs ${DLL_RUNTIME_TARGET}; include(SymbianApp)")
    endif()
    target_link_libraries(${elf_target} PRIVATE ${DLL_RUNTIME_TARGET})
    if(NOT DLL_IMPORT_PROXIES)
      set(DLL_IMPORT_PROXIES
        "${SYMBIAN_SDK_PREFIX}/proxies/euser/euser.dso")
    endif()
  endif()
  set(proxy_args)
  if(DLL_IMPORT_PROXIES)
    target_link_options(${elf_target} PRIVATE --hash-style=sysv --no-dynamic-linker)
    target_link_libraries(${elf_target} PRIVATE ${DLL_IMPORT_PROXIES})
    foreach(proxy IN LISTS DLL_IMPORT_PROXIES)
      list(APPEND proxy_args --import-proxy "${proxy}")
    endforeach()
  endif()
  set(dll "${CMAKE_BINARY_DIR}/${target}.dll")
  add_custom_command(OUTPUT "${dll}"
    COMMAND "${SYMBIAN_SDK_PREFIX}/bin/symbian" toolchain convert-dll
      "$<TARGET_FILE:${elf_target}>" --definition "${definition}"
      --uid3 "${DLL_UID3}" ${proxy_args} --output "${dll}"
    DEPENDS ${elf_target} "${definition}" ${DLL_IMPORT_PROXIES}
    VERBATIM)
  add_custom_target(${target} ALL DEPENDS "${dll}")
  if(DLL_IMPORT_SYMBOLS)
    set(proxy_dir "${CMAKE_BINARY_DIR}/${target}-import")
    set(proxy "${proxy_dir}/${target}.dso")
    set(proxy_args)
    foreach(symbol IN LISTS DLL_IMPORT_SYMBOLS)
      list(APPEND proxy_args --symbol "${symbol}")
    endforeach()
    add_custom_command(OUTPUT "${proxy}"
      COMMAND "${SYMBIAN_SDK_PREFIX}/bin/symbian" toolchain import-proxy
        "${definition}" ${proxy_args} --target-dll "${target}.dll"
        --output "${proxy_dir}"
        --compiler "${CMAKE_CXX_COMPILER}" --linker "${CMAKE_LINKER}"
      DEPENDS "${definition}"
      VERBATIM)
    add_custom_target(${target}_proxy DEPENDS "${proxy}")
    add_library(${target}_import UNKNOWN IMPORTED GLOBAL)
    set_target_properties(${target}_import PROPERTIES IMPORTED_LOCATION "${proxy}")
    add_dependencies(${target}_import ${target}_proxy)
  endif()
endfunction()
