include_guard(GLOBAL)
if(CMAKE_SYSTEM_NAME STREQUAL "Generic")
  set_property(GLOBAL PROPERTY TARGET_SUPPORTS_SHARED_LIBS TRUE)
  set(CMAKE_SHARED_LIBRARY_CREATE_CXX_FLAGS "")
  set(CMAKE_SHARED_LIBRARY_CREATE_C_FLAGS "")
endif()
if(EXISTS "${SYMBIAN_SDK_PREFIX}/proxies/euser-eka1/euser.dso" AND
   NOT TARGET Symbian::Eka1EUser)
  add_library(Symbian::Eka1EUser SHARED IMPORTED GLOBAL)
  set_target_properties(Symbian::Eka1EUser PROPERTIES IMPORTED_LOCATION
    "${SYMBIAN_SDK_PREFIX}/proxies/euser-eka1/euser.dso")
endif()


function(_symbian_project_file output filename)
  file(REAL_PATH "${filename}" resolved BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
  file(REAL_PATH "${CMAKE_SOURCE_DIR}" project_root)
  cmake_path(IS_PREFIX project_root "${resolved}" NORMALIZE inside)
  file(REAL_PATH "${CMAKE_CURRENT_FUNCTION_LIST_DIR}" module_directory)
  cmake_path(IS_PREFIX module_directory "${resolved}" NORMALIZE inside_module)
  set(inside_sdk FALSE)
  set(inside_runtime FALSE)
  if(SYMBIAN_SDK_PREFIX)
    file(REAL_PATH "${SYMBIAN_SDK_PREFIX}/cmake" sdk_cmake)
    cmake_path(IS_PREFIX sdk_cmake "${resolved}" NORMALIZE inside_sdk)
    file(REAL_PATH "${SYMBIAN_SDK_PREFIX}/share/symbian/runtime" sdk_runtime)
    cmake_path(IS_PREFIX sdk_runtime "${resolved}" NORMALIZE inside_runtime)
  endif()
  if((NOT inside AND NOT inside_sdk AND NOT inside_runtime AND NOT inside_module) OR
     NOT EXISTS "${resolved}" OR
     IS_DIRECTORY "${resolved}")
    message(FATAL_ERROR "Symbian target input must be a project or selected SDK file: ${filename}")
  endif()
  set(${output} "${resolved}" PARENT_SCOPE)
endfunction()

function(_symbian_collect_native_files directory output)
  set(found)
  file(GLOB entries CONFIGURE_DEPENDS LIST_DIRECTORIES TRUE "${directory}/*")
  foreach(entry IN LISTS entries)
    get_filename_component(name "${entry}" NAME)
    if(IS_DIRECTORY "${entry}")
      if(name MATCHES "^(build|out|vendor|third_party|\\.symbian|\\.git|cmake-build[^/]*)$")
        continue()
      endif()
      _symbian_collect_native_files("${entry}" nested)
      list(APPEND found ${nested})
    elseif(entry MATCHES "\\.(c|cc|cpp|cxx|mm|S|s|h|hh|hpp|hxx)$")
      file(REAL_PATH "${entry}" native_file)
      list(APPEND found "${native_file}")
    endif()
  endforeach()
  set(${output} "${found}" PARENT_SCOPE)
endfunction()

function(symbian_add_import_executable target)
  symbian_add_pic_executable(${target} ${ARGN})
  # A registered application owns native files anywhere in its source tree.
  # CMake tracks additions so new files join its real guest target and IDE
  # compile context automatically. Build and dependency trees stay separate.
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/symbian.toml")
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/symbian.toml"
      app_section REGEX "^\\[application\\][ \t]*$")
    if(app_section)
      _symbian_collect_native_files("${CMAKE_CURRENT_SOURCE_DIR}" native_files)
      get_target_property(registered_sources ${target} SOURCES)
      set(registered_absolute_sources)
      foreach(registered_source IN LISTS registered_sources)
        if(NOT registered_source MATCHES "^\\$<")
          get_filename_component(registered_absolute "${registered_source}"
            ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
          list(APPEND registered_absolute_sources "${registered_absolute}")
        endif()
      endforeach()
      foreach(native_file IN LISTS native_files)
        list(FIND registered_absolute_sources "${native_file}" source_index)
        if(source_index EQUAL -1)
          target_sources(${target} PRIVATE "${native_file}")
        endif()
      endforeach()
    endif()
  endif()
  # Imports flow through ordinary library targets. Legacy diagnostic overrides
  # remain usable without making applications enumerate proxies.
  target_link_options(${target} PRIVATE --hash-style=sysv --no-dynamic-linker
    --as-needed)
  target_link_libraries(${target} PRIVATE ${SYMBIAN_IMPORT_PROXIES})
endfunction()

function(symbian_add_pic_executable target)
  cmake_parse_arguments(PARSE_ARGV 1 PIC "" "STARTUP;LINKER_SCRIPT" "SOURCES")
  if(PIC_UNPARSED_ARGUMENTS OR PIC_KEYWORDS_MISSING_VALUES OR
     NOT PIC_SOURCES)
    message(FATAL_ERROR "symbian_add_pic_executable needs SOURCES")
  endif()
  if(NOT CMAKE_SYSTEM_NAME STREQUAL "Generic" OR
     NOT (CMAKE_CXX_COMPILER_TARGET MATCHES "^arm(v5t|v6)-none-eabi$" OR
          CMAKE_C_COMPILER_TARGET MATCHES "^arm(v5t|v6)-none-eabi$"))
    message(FATAL_ERROR "Use the Symbian symbian-arm.cmake toolchain (armv6 or armv5t)")
  endif()
  set(default_startup FALSE)
  set(image_kernel eka2)
  if(NOT PIC_STARTUP)
    if(TARGET symbian_guest_runtime AND NOT TARGET Symbian::EUser)
      include(SymbianPlatform)
    endif()
    set(profile exe)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/symbian.toml")
      file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/symbian.toml" kind
        REGEX "^kind[ \t]*=[ \t]*\"")
    endif()
    if(kind MATCHES "e32-eka1")
      set(image_kernel eka1)
      set(profile eka1)
      if(kind MATCHES "e32-eka1-import")
        set(profile eka1_import)
      endif()
    elseif(NOT TARGET Symbian::EUser AND NOT TARGET symbian_guest_runtime)
      set(profile freestanding)
      if(kind MATCHES "e32-import")
        set(profile freestanding_import)
      endif()
    endif()
    if(profile STREQUAL "eka1_import")
      set(PIC_STARTUP "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/eka1_startup.S")
    elseif(profile STREQUAL "freestanding_import")
      set(PIC_STARTUP "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/freestanding_startup.S")
    else()
      set(PIC_STARTUP "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/${profile}_startup.S")
    endif()
    if(NOT PIC_LINKER_SCRIPT)
      set(PIC_LINKER_SCRIPT "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/${profile}_image.ld")
    endif()
    if(profile STREQUAL "exe")
      set(default_startup TRUE)
    endif()
  endif()
  if(NOT PIC_LINKER_SCRIPT)
    set(PIC_LINKER_SCRIPT "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/exe_image.ld")
  endif()
  _symbian_project_file(startup "${PIC_STARTUP}")
  _symbian_project_file(script "${PIC_LINKER_SCRIPT}")
  set(sources)
  foreach(source IN LISTS PIC_SOURCES)
    _symbian_project_file(resolved "${source}")
    list(APPEND sources "${resolved}")
  endforeach()
  add_executable(${target} "${startup}" ${sources})
  if(default_startup)
    target_sources(${target} PRIVATE "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/exe_startup.cc")
    if(TARGET Symbian::EUser)
      target_link_libraries(${target} PRIVATE Symbian::EUser)
    elseif(NOT TARGET symbian_guest_runtime OR NOT SYMBIAN_IMPORT_PROXIES)
      message(FATAL_ERROR "Include SymbianApp before creating an application")
    endif()
    set_target_properties(${target} PROPERTIES SYMBIAN_IMAGE_LAYOUT "${script}")
    cmake_language(EVAL CODE
      "cmake_language(DEFER CALL _symbian_executable_unwind ${target})")
  endif()
  target_compile_options(${target} PRIVATE -fPIC)
  target_link_options(${target} PRIVATE -T "${script}")
  set_target_properties(${target} PROPERTIES
    SUFFIX ".elf" LINK_DEPENDS "${script}"
    SYMBIAN_IMAGE_KERNEL "${image_kernel}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endfunction()

# Exception-enabled translation units need the SDK's retained unwind metadata
# and E32 descriptor. Source compile options select this after the whole target
# has been declared; applications do not supply assembly or another layout.
function(_symbian_executable_unwind target)
  _symbian_link_default_runtime(${target})
  get_target_property(sources ${target} SOURCES)
  get_target_property(options ${target} COMPILE_OPTIONS)
  foreach(source IN LISTS sources)
    get_source_file_property(source_options "${source}" COMPILE_OPTIONS)
    list(APPEND options ${source_options})
  endforeach()
  if(NOT "-fexceptions" IN_LIST options)
    return()
  endif()
  if(TARGET Symbian::CxxAbi)
    target_link_libraries(${target} PRIVATE Symbian::CxxAbi)
  endif()
  set(script "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/exe_unwind_image.ld")
  get_target_property(original_script ${target} SYMBIAN_IMAGE_LAYOUT)
  get_target_property(link_options ${target} LINK_OPTIONS)
  list(FIND link_options "${original_script}" position)
  list(REMOVE_AT link_options ${position})
  list(INSERT link_options ${position} "${script}")
  set_target_properties(${target} PROPERTIES LINK_OPTIONS "${link_options}"
    LINK_DEPENDS "${script}" SYMBIAN_IMAGE_LAYOUT "${script}")
  target_sources(${target} PRIVATE
    "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/exception_descriptor.S")
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

# Creates an ordinary mutable CMake ELF target plus a published E32 DLL and a
# complete consumer import target. A frozen definition is optional for libraries
# that must preserve ABI across separate releases; no consumer lists symbols.
function(symbian_add_dynamic_library target)
  cmake_parse_arguments(PARSE_ARGV 1 DLL "" "STARTUP;LINKER_SCRIPT;EXPORT_DEFINITION;UID3;RUNTIME_TARGET"
                        "SOURCES;IMPORT_SYMBOLS;IMPORT_PROXIES")
  if(DLL_UNPARSED_ARGUMENTS OR DLL_KEYWORDS_MISSING_VALUES OR NOT DLL_SOURCES)
    message(FATAL_ERROR "symbian_add_dynamic_library needs SOURCES")
  endif()
  if(NOT DLL_UID3)
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/symbian.toml" identity
      REGEX "^uid3[ \t]*=[ \t]*0x[0-9A-Fa-f]+[ \t]*$")
    list(GET identity 0 identity)
    string(REGEX REPLACE "^uid3[ \t]*=[ \t]*" "" DLL_UID3 "${identity}")
  endif()
  if(NOT DLL_STARTUP)
    if(TARGET Symbian::Runtime)
      set(DLL_STARTUP "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/dll_startup.S")
    else()
      set(DLL_STARTUP "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/freestanding_dll_startup.S")
      if(NOT DLL_LINKER_SCRIPT)
        set(DLL_LINKER_SCRIPT "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/freestanding_dll_image.ld")
      endif()
    endif()
  endif()
  if(NOT DLL_LINKER_SCRIPT)
    set(DLL_LINKER_SCRIPT "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/dll_image.ld")
  endif()
  _symbian_project_file(startup "${DLL_STARTUP}")
  _symbian_project_file(script "${DLL_LINKER_SCRIPT}")
  add_library(${target} SHARED "${startup}" ${DLL_SOURCES})
  # E32 relocates code and writable storage independently. Absolute local
  # references carry typed fixups; ELF PIC code-to-data deltas cannot survive.
  target_compile_options(${target} PRIVATE -fno-pic)
  set_target_properties(${target} PROPERTIES PREFIX "" SUFFIX ".dso"
    NO_SONAME TRUE LINK_DEPENDS "${script}" SYMBIAN_ORDINAL_LIBRARY TRUE)
  target_link_options(${target} PRIVATE -T "${script}")
  add_library(${target}_import ALIAS ${target})
  if(DLL_RUNTIME_TARGET)
    target_link_libraries(${target} PRIVATE ${DLL_RUNTIME_TARGET} Symbian::EUser)
  elseif(TARGET Symbian::EUser)
    target_link_libraries(${target} PRIVATE Symbian::EUser)
  endif()
  target_link_options(${target} PRIVATE --hash-style=sysv --no-dynamic-linker
    --as-needed --exclude-libs=ALL --gc-sections --export-dynamic
    "--export-dynamic-symbol=*")
  # Source functions form the public interface. Runtime archive implementation
  # stays hidden; explicit source visibility still supports private helpers.
  foreach(source IN LISTS DLL_SOURCES)
    set_property(SOURCE "${source}" APPEND PROPERTY COMPILE_OPTIONS -fvisibility=default)
  endforeach()
  if(DLL_EXPORT_DEFINITION)
    _symbian_project_file(definition "${DLL_EXPORT_DEFINITION}")
  else()
    set(definition "${CMAKE_CURRENT_BINARY_DIR}/${target}.def")
    set_target_properties(${target} PROPERTIES SYMBIAN_AUTOMATIC_EXPORTS TRUE)
  endif()
  set_target_properties(${target} PROPERTIES SYMBIAN_UID3 "${DLL_UID3}"
    SYMBIAN_DLL_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/${target}.dll"
    SYMBIAN_EXPORT_DEFINITION "${definition}"
    SYMBIAN_EXTRA_PROXIES "${DLL_IMPORT_PROXIES};${SYMBIAN_IMPORT_PROXIES}")
  target_link_libraries(${target} PRIVATE ${DLL_IMPORT_PROXIES})
  cmake_language(EVAL CODE
    "cmake_language(DEFER CALL _symbian_publish_library ${target})")

endfunction()

function(_symbian_publish_library target)
  _symbian_e32_converter(converter converter_dependency)
  if(TARGET "${converter_dependency}")
    add_dependencies(${target} "${converter_dependency}")
  else()
    set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS "${converter_dependency}")
  endif()
  _symbian_link_default_runtime(${target})
  _symbian_target_proxies(${target} "" proxies)
  get_target_property(extra ${target} SYMBIAN_EXTRA_PROXIES)
  list(APPEND proxies ${extra})
  list(REMOVE_ITEM proxies "")
  list(REMOVE_DUPLICATES proxies)
  set(options)
  foreach(proxy IN LISTS proxies)
    list(APPEND options --import-proxy "${proxy}")
  endforeach()
  get_target_property(uid3 ${target} SYMBIAN_UID3)
  get_target_property(definition ${target} SYMBIAN_EXPORT_DEFINITION)
  get_target_property(automatic ${target} SYMBIAN_AUTOMATIC_EXPORTS)
  set(export_command)
  if(automatic)
    set(export_command COMMAND "${converter}"
      export-definition --input "$<TARGET_FILE:${target}>"
      --output "${definition}")
  else()
    set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS "${definition}")
  endif()
  set(proxy_dir "${CMAKE_CURRENT_BINARY_DIR}/${target}-import")
  set(elf "${CMAKE_CURRENT_BINARY_DIR}/${target}_elf.elf")
  set(image "${CMAKE_CURRENT_BINARY_DIR}/${target}.dll")
  set_property(TARGET ${target} PROPERTY SYMBIAN_DLL_IMAGE "${image}")
  add_custom_command(TARGET ${target} POST_BUILD
    ${export_command}
    COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:${target}>" "${elf}"
    COMMAND "${converter}" convert-dll
      --input "${elf}" --definition "${definition}"
      --uid3 "${uid3}" ${options} --output "${image}"
    COMMAND "${converter}" proxy-sources
      --definition "${definition}" --target-dll "${target}.dll"
      --output "${proxy_dir}"
    COMMAND "${CMAKE_CXX_COMPILER}" --target=armv5t-none-eabi -march=armv5t
      -c "${proxy_dir}/exports.S" -o "${proxy_dir}/exports.o"
    COMMAND "${CMAKE_LINKER}" -m armelf -shared --hash-style=sysv
      --build-id=none "--soname=${target}.dso"
      "--version-script=${proxy_dir}/exports.map"
      -T "${proxy_dir}/proxy.ld" "${proxy_dir}/exports.o"
      -o "$<TARGET_FILE:${target}>"
    BYPRODUCTS "${elf}" "${image}" VERBATIM)
  # Retain the previous helper target spelling for source diagnostic consumers.
  add_custom_target(${target}_proxy DEPENDS ${target})
endfunction()

function(_symbian_runtime_profiles target visited output)
  if(NOT visited)
    set_property(GLOBAL PROPERTY SYMBIAN_RUNTIME_WALK "")
  endif()
  # Shared subgraphs are visited once per walk, including across separate
  # LINK_LIBRARIES and INTERFACE_LINK_LIBRARIES edges. Large SDK dependency
  # graphs otherwise expand exponentially during CMake configuration.
  get_property(seen GLOBAL PROPERTY SYMBIAN_RUNTIME_WALK)
  if(target IN_LIST seen)
    set(${output} "" PARENT_SCOPE)
    return()
  endif()
  set_property(GLOBAL APPEND PROPERTY SYMBIAN_RUNTIME_WALK "${target}")
  list(APPEND visited "${target}")
  get_target_property(profile ${target} SYMBIAN_RUNTIME_PROFILE)
  set(profiles)
  if(profile)
    list(APPEND profiles "${profile}")
  endif()
  foreach(property LINK_LIBRARIES INTERFACE_LINK_LIBRARIES)
    get_target_property(items ${target} ${property})
    foreach(item IN LISTS items)
      if(item MATCHES "^\\$<LINK_ONLY:([^>]+)>$")
        set(item "${CMAKE_MATCH_1}")
      endif()
      if(TARGET "${item}")
        _symbian_runtime_profiles("${item}" "${visited}" nested)
        list(APPEND profiles ${nested})
      endif()
    endforeach()
  endforeach()
  list(REMOVE_DUPLICATES profiles)
  set(${output} "${profiles}" PARENT_SCOPE)
endfunction()

function(_symbian_link_default_runtime target)
  _symbian_runtime_profiles(${target} "" profiles)
  list(LENGTH profiles count)
  if(count GREATER 1)
    message(FATAL_ERROR "Target ${target} links incompatible SDK runtimes: ${profiles}")
  elseif(TARGET Symbian::Runtime)
    # A static dependency can propagate LINK_ONLY runtime requirements. Each
    # target also needs that profile's headers and compiler ABI settings.
    set(runtime Symbian::Runtime)
    if(profiles STREQUAL "streams")
      set(runtime Symbian::Streams)
    elseif(profiles STREQUAL "atomic64")
      set(runtime Symbian::NativeAtomics64)
    endif()
    target_link_libraries(${target} PRIVATE ${runtime})
  endif()
endfunction()


# Convert the linked ELF to a runnable E32 image using the standalone host tool.
# The ELF remains available for debugging. This step requires no Python runtime.
function(_symbian_e32_converter output dependency)
  set(converter "${SYMBIAN_SDK_PREFIX}/bin/symbian-native")
  set(converter_dependency "${converter}")
  if(SYMBIAN_WORKSPACE_BUILD AND TARGET symbian_native_tool)
    set(converter "$<TARGET_FILE:symbian_native_tool>")
    set(converter_dependency symbian_native_tool)
  elseif(SYMBIAN_NATIVE_CONVERTER)
    set(converter "${SYMBIAN_NATIVE_CONVERTER}")
    set(converter_dependency "${converter}")
  endif()
  if(NOT TARGET "${converter_dependency}" AND NOT EXISTS "${converter}")
    message(FATAL_ERROR "E32 publishing needs the standalone symbian-native tool: ${converter}")
  endif()
  set(${output} "${converter}" PARENT_SCOPE)
  set(${dependency} "${converter_dependency}" PARENT_SCOPE)
endfunction()

function(symbian_publish_executable target)
  cmake_parse_arguments(PARSE_ARGV 1 EXE "" "UID3;CAPABILITIES;PROJECT_DLLS" "IMPORT_PROXIES")
  if(EXE_UNPARSED_ARGUMENTS OR EXE_KEYWORDS_MISSING_VALUES OR NOT EXE_UID3)
    message(FATAL_ERROR "symbian_publish_executable needs UID3")
  endif()
  if(EXE_PROJECT_DLLS AND NOT EXE_PROJECT_DLLS MATCHES "^(BUNDLE|RUNTIME)$")
    message(FATAL_ERROR
      "${target}: PROJECT_DLLS must be BUNDLE or RUNTIME")
  endif()
  _symbian_e32_converter(converter converter_dependency)
  set_target_properties(${target} PROPERTIES SYMBIAN_UID3 "${EXE_UID3}"
    SYMBIAN_PROJECT_DLLS "${EXE_PROJECT_DLLS}"
    SYMBIAN_CONVERTER "${converter}"
    SYMBIAN_CONVERTER_DEPENDENCY "${converter_dependency}"
    SYMBIAN_CAPABILITIES "${EXE_CAPABILITIES}"
    SYMBIAN_EXTRA_PROXIES "${EXE_IMPORT_PROXIES};${SYMBIAN_IMPORT_PROXIES}")
  cmake_language(EVAL CODE
    "cmake_language(DEFER CALL _symbian_publish_executable ${target})")
endfunction()

function(_symbian_publish_executable target)
  get_target_property(converter ${target} SYMBIAN_CONVERTER)
  get_target_property(converter_dependency ${target} SYMBIAN_CONVERTER_DEPENDENCY)
  _symbian_target_proxies(${target} "" proxies)
  get_target_property(extra ${target} SYMBIAN_EXTRA_PROXIES)
  list(APPEND proxies ${extra})
  list(REMOVE_ITEM proxies "")
  list(REMOVE_DUPLICATES proxies)
  set(options)
  foreach(proxy IN LISTS proxies)
    list(APPEND options --import-proxy "${proxy}")
  endforeach()
  get_target_property(uid3 ${target} SYMBIAN_UID3)
  get_target_property(kernel ${target} SYMBIAN_IMAGE_KERNEL)
  if(kernel)
    list(APPEND options --kernel "${kernel}")
  endif()
  get_target_property(capabilities ${target} SYMBIAN_CAPABILITIES)
  if(capabilities)
    list(APPEND options --capabilities "${capabilities}")
  endif()
  set(image "${CMAKE_CURRENT_BINARY_DIR}/e32/${target}.exe")
  add_custom_command(OUTPUT "${image}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/e32"
    COMMAND "${converter}" convert-exe --input "$<TARGET_FILE:${target}>"
      --uid3 "${uid3}" ${options} --output "${image}"
    COMMAND "${CMAKE_COMMAND}"
      "-DGRAPH_FILE=${CMAKE_BINARY_DIR}/${target}.libraries.json"
      "-DOUTPUT_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/e32"
      "-DTARGET_NAME=${target}"
      -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SymbianApplicationPayload.cmake"
    BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/e32/${target}.libraries.json"
    DEPENDS ${target} "${converter_dependency}" ${proxies}
      "${CMAKE_BINARY_DIR}/${target}.libraries.json"
      "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SymbianApplicationPayload.cmake"
    VERBATIM)
  add_custom_target(${target}_e32 ALL DEPENDS "${image}")
endfunction()

# Only DLLs built by the application graph are deployable payloads. Imported
# firmware DLL targets supply interfaces, never firmware implementations.
function(_symbian_application_libraries target output)
  get_property(seen GLOBAL PROPERTY SYMBIAN_APPLICATION_LIBRARY_WALK)
  if(target IN_LIST seen)
    set(${output} "" PARENT_SCOPE)
    return()
  endif()
  set_property(GLOBAL APPEND PROPERTY SYMBIAN_APPLICATION_LIBRARY_WALK "${target}")
  set(result)
  get_target_property(image ${target} SYMBIAN_DLL_IMAGE)
  if(image)
    list(APPEND result "${image}")
  endif()
  foreach(property LINK_LIBRARIES INTERFACE_LINK_LIBRARIES)
    get_target_property(items ${target} ${property})
    foreach(item IN LISTS items)
      if(item MATCHES "^\\$<LINK_ONLY:([^>]+)>$")
        set(item "${CMAKE_MATCH_1}")
      endif()
      if(TARGET "${item}")
        _symbian_application_libraries("${item}" nested)
        list(APPEND result ${nested})
      elseif(item MATCHES "\\$<")
        set_property(GLOBAL APPEND PROPERTY SYMBIAN_APPLICATION_LIBRARY_CONDITIONAL "${item}")
      endif()
    endforeach()
  endforeach()
  list(REMOVE_DUPLICATES result)
  set(${output} "${result}" PARENT_SCOPE)
endfunction()

function(_symbian_write_application_libraries target)
  set_property(GLOBAL PROPERTY SYMBIAN_APPLICATION_LIBRARY_WALK "")
  set_property(GLOBAL PROPERTY SYMBIAN_APPLICATION_LIBRARY_CONDITIONAL "")
  _symbian_application_libraries(${target} images)
  get_target_property(mode ${target} SYMBIAN_PROJECT_DLLS)
  if(images AND NOT mode)
    message(FATAL_ERROR
      "${target}: linked project DLLs require an explicit packaging choice: "
      "symbian_publish_executable(${target} ... PROJECT_DLLS BUNDLE|RUNTIME)")
  endif()
  get_property(conditional GLOBAL PROPERTY SYMBIAN_APPLICATION_LIBRARY_CONDITIONAL)
  if(images AND conditional AND mode STREQUAL "BUNDLE")
    message(FATAL_ERROR "${target}: application DLL packaging cannot resolve ${conditional}; select libraries with CMake if()")
  endif()
  if(mode STREQUAL "RUNTIME")
    set(images)
  endif()
  set(content "{\"schema\":\"symbian.application-libraries/v1\",\"libraries\":[")
  set(separator "")
  foreach(image IN LISTS images)
    string(REPLACE "\\" "\\\\" escaped "${image}")
    string(REPLACE "\"" "\\\"" escaped "${escaped}")
    string(APPEND content "${separator}\"${escaped}\"")
    set(separator ",")
  endforeach()
  string(APPEND content "]}\n")
  file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/${target}.libraries.json"
    CONTENT "${content}")
endfunction()

# Standard application entry is main() or main(int, char**). Runtime selection
# remains a target_link_libraries choice, so alternate profiles cannot collide.
function(symbian_add_executable target)
  if(ARGV1 STREQUAL "SOURCES")
    symbian_add_import_executable(${target} ${ARGN})
  else()
    symbian_add_import_executable(${target} SOURCES ${ARGN})
  endif()
  # Wait for sibling/subdirectory DLL publishers to finish declaring images.
  cmake_language(EVAL CODE
    "cmake_language(DEFER DIRECTORY \"${CMAKE_SOURCE_DIR}\" CALL _symbian_write_application_libraries ${target})")
  if(NOT TARGET symbian_guest_runtime AND NOT TARGET Symbian::Runtime AND
     EXISTS "${SYMBIAN_SDK_PREFIX}/include/abseil/absl/base/nullability.h")
    target_include_directories(${target} SYSTEM PRIVATE
      "${SYMBIAN_SDK_PREFIX}/include/abseil"
      "${SYMBIAN_SDK_PREFIX}/include/config"
      "${SYMBIAN_SDK_PREFIX}/include/c++"
      "${SYMBIAN_SDK_PREFIX}/include/compiler")
  endif()
  # Clang gives main C linkage in hosted mode; prevent host libc builtins.
  target_compile_options(${target} PRIVATE -fhosted -fno-builtin -g -gdwarf-4
    "-fdebug-compilation-dir=/symbian-build/${target}"
    "-fdebug-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}=/symbian-src/${target}"
    # Clang checks the last prefix first. Build trees often live inside the
    # source directory, so the more specific generated-header map comes last.
    "-fdebug-prefix-map=${CMAKE_BINARY_DIR}=/symbian-build/${target}"
    "-fdebug-prefix-map=${SYMBIAN_SDK_PREFIX}/include/platform=/symbian-sdk/include"
    "$<$<CONFIG:Debug>:-O0>")
  target_link_options(${target} PRIVATE --gc-sections)
endfunction()

# Traverse the target graph after all target_link_libraries calls. This is build
# metadata, not an application-maintained import manifest. Cycles are harmless.
function(_symbian_target_proxies target visited output)
  if(NOT visited)
    set_property(GLOBAL PROPERTY SYMBIAN_PROXY_WALK "")
  endif()
  # Shared subgraphs are visited once per walk, including across separate
  # LINK_LIBRARIES and INTERFACE_LINK_LIBRARIES edges. Large SDK dependency
  # graphs otherwise expand exponentially during CMake configuration.
  get_property(seen GLOBAL PROPERTY SYMBIAN_PROXY_WALK)
  if(target IN_LIST seen)
    set(${output} "" PARENT_SCOPE)
    return()
  endif()
  set_property(GLOBAL APPEND PROPERTY SYMBIAN_PROXY_WALK "${target}")
  list(APPEND visited "${target}")
  set(result)
  get_target_property(ordinal_library ${target} SYMBIAN_ORDINAL_LIBRARY)
  if(ordinal_library AND visited)
    # The root DLL is being converted; its own proxy is not its dependency.
    list(LENGTH visited depth)
    if(depth GREATER 1)
      list(APPEND result "$<TARGET_FILE:${target}>")
    endif()
  endif()
  foreach(property IMPORTED_LOCATION LINK_LIBRARIES INTERFACE_LINK_LIBRARIES)
    get_target_property(items ${target} ${property})
    if(NOT items)
      continue()
    endif()
    foreach(item IN LISTS items)
      if(item MATCHES "^\\$<LINK_ONLY:([^>]+)>$")
        set(item "${CMAKE_MATCH_1}")
      endif()
      if(TARGET "${item}")
        _symbian_target_proxies("${item}" "${visited}" nested)
        list(APPEND result ${nested})
      elseif(item MATCHES "\\.dso$")
        list(APPEND result "${item}")
      endif()
    endforeach()
  endforeach()
  list(REMOVE_DUPLICATES result)
  set(${output} "${result}" PARENT_SCOPE)
endfunction()
