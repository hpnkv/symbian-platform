include_guard(GLOBAL)

# Like A11's header canary, compile consumers with exceptions disabled. Each
# header has its own translation unit so another header cannot hide a missing
# include. Groups inherit their owning target's PUBLIC usage requirements;
# PRIVATE include directories must be requested explicitly for internal groups.
function(symbian_header_canary name)
  cmake_parse_arguments(CANARY "EXCEPTIONS;C" "" "HEADERS;LIBRARIES;INCLUDE_DIRECTORIES" ${ARGN})
  foreach(library IN LISTS CANARY_LIBRARIES)
    if(NOT TARGET ${library})
      message(FATAL_ERROR "Header canary ${name} has no owner target ${library}")
    endif()
  endforeach()
  if(NOT CANARY_HEADERS)
    message(FATAL_ERROR "Header canary ${name} has no headers")
  endif()
  if(NOT TARGET symbian_header_canaries)
    add_custom_target(symbian_header_canaries ALL)
  endif()
  set(sources)
  foreach(header IN LISTS CANARY_HEADERS)
    get_filename_component(header "${header}" ABSOLUTE)
    string(SHA256 identifier "${header}")
    if(CANARY_C)
      set(extension c)
    else()
      set(extension cc)
    endif()
    set(source "${CMAKE_CURRENT_BINARY_DIR}/header-canaries/${name}/${identifier}.${extension}")
    file(GENERATE OUTPUT "${source}" CONTENT "// Generated standalone header consumer.\n#include \"${header}\"\n")
    list(APPEND sources "${source}")
  endforeach()
  add_library(${name} OBJECT ${sources})
  # Expose the actual headers as target members for IDE include/ABI context.
  target_sources(${name} PRIVATE ${CANARY_HEADERS})
  target_link_libraries(${name} PRIVATE ${CANARY_LIBRARIES})
  target_include_directories(${name} PRIVATE ${CANARY_INCLUDE_DIRECTORIES})
  if(NOT CANARY_C)
    target_compile_features(${name} PRIVATE cxx_std_20)
    if(CANARY_EXCEPTIONS)
      target_compile_options(${name} PRIVATE
        $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-fexceptions>)
    else()
      target_compile_options(${name} PRIVATE
        $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-fno-exceptions>)
    endif()
  endif()
  add_dependencies(symbian_header_canaries ${name})
  if(TARGET symbian_probe_index)
    add_dependencies(symbian_probe_index ${name})
  endif()
endfunction()

# Directory discovery is bounded by the explicit owner/group; newly added
# owned headers automatically receive an independent compilation check.
function(symbian_directory_header_canary name library directory)
  file(GLOB_RECURSE headers CONFIGURE_DEPENDS "${directory}/*.h")
  symbian_header_canary(${name} HEADERS ${headers} LIBRARIES ${library} ${ARGN})
endfunction()
