# Attach every project-owned native file to an IDE target. Component targets
# remain authoritative; this fallback covers files awaiting build rules.

function(_symbian_native_target_sources directory result)
  set(found)
  get_property(targets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
  foreach(target IN LISTS targets)
    get_target_property(target_directory ${target} SOURCE_DIR)
    get_target_property(sources ${target} SOURCES)
    foreach(source IN LISTS sources)
      if(source MATCHES "^\\$<" OR NOT source MATCHES
          "\\.(c|cc|cpp|cxx|mm|S|s|h|hh|hpp|hxx)$")
        continue()
      endif()
      get_filename_component(path "${source}" ABSOLUTE
        BASE_DIR "${target_directory}")
      list(APPEND found "${path}")
    endforeach()
  endforeach()
  get_property(children DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
  foreach(child IN LISTS children)
    string(FIND "${child}" "${CMAKE_SOURCE_DIR}/" prefix)
    if(prefix EQUAL 0)
      _symbian_native_target_sources("${child}" child_sources)
      list(APPEND found ${child_sources})
    endif()
  endforeach()
  set(${result} "${found}" PARENT_SCOPE)
endfunction()

function(_symbian_native_files directory result)
  set(found)
  file(GLOB entries CONFIGURE_DEPENDS LIST_DIRECTORIES TRUE "${directory}/*")
  foreach(entry IN LISTS entries)
    get_filename_component(name "${entry}" NAME)
    if(IS_DIRECTORY "${entry}")
      if(name MATCHES "^(build|cmake-build[^/]*|\\.symbian|_deps|dist)$")
        continue()
      endif()
      _symbian_native_files("${entry}" child_files)
      list(APPEND found ${child_files})
    elseif(entry MATCHES "\\.(c|cc|cpp|cxx|mm|S|s|h|hh|hpp|hxx)$")
      list(APPEND found "${entry}")
    endif()
  endforeach()
  set(${result} "${found}" PARENT_SCOPE)
endfunction()

function(symbian_index_remaining_native target dependency)
  _symbian_native_target_sources("${CMAKE_SOURCE_DIR}" registered)
  list(REMOVE_DUPLICATES registered)
  set(native_files)
  foreach(root IN LISTS ARGN)
    set(source_root "${CMAKE_SOURCE_DIR}/${root}")
    _symbian_native_files("${source_root}" root_files)
    list(APPEND native_files ${root_files})
  endforeach()
  list(REMOVE_DUPLICATES native_files)
  set(remaining)
  foreach(native_file IN LISTS native_files)
    # The checked A11 source snapshot is for host-side inspection. The guest
    # library has its own copied implementation and build rules.
    if(SYMBIAN_INDEX_GUEST_PROBES AND native_file MATCHES
        "^${CMAKE_SOURCE_DIR}/cpp/symbian/concurrency/upstream/")
      continue()
    endif()
    # Guest libraries belong to the ARM profile. A host placeholder for the
    # same file makes CLion prefer host flags and lose generated guest headers.
    if(NOT SYMBIAN_INDEX_GUEST_PROBES AND native_file MATCHES
        "^${CMAKE_SOURCE_DIR}/cpp/symbian/(api|tls|runtime|concurrency/guest)/")
      continue()
    endif()
    list(FIND registered "${native_file}" found)
    if(found EQUAL -1)
      list(APPEND remaining "${native_file}")
      # The host root does not enable guest ASM. Keep these files visible as
      # members of the indexing target without asking its generator for an
      # assembler rule; the ARM profile supplies their actual compile model.
      if((NOT SYMBIAN_INDEX_GUEST_PROBES AND
          native_file MATCHES "\\.(S|s)$") OR
         (native_file MATCHES "\\.mm$" AND NOT CMAKE_OBJCXX_COMPILER))
        set_source_files_properties("${native_file}" PROPERTIES
          HEADER_FILE_ONLY TRUE)
      endif()
    endif()
  endforeach()
  if(remaining)
    add_library(${target} OBJECT EXCLUDE_FROM_ALL ${remaining})
    set_target_properties(${target} PROPERTIES LINKER_LANGUAGE CXX)
    target_link_libraries(${target} PRIVATE ${dependency})
    target_include_directories(${target} PRIVATE
      "${CMAKE_SOURCE_DIR}/cpp"
      "${CMAKE_SOURCE_DIR}/cpp/symbian/api/include"
      "${CMAKE_SOURCE_DIR}/cpp/symbian/concurrency/guest"
      "${CMAKE_SOURCE_DIR}/cpp/symbian/concurrency/host"
      "${CMAKE_SOURCE_DIR}/cpp/symbian/concurrency/common"
      "${CMAKE_SOURCE_DIR}/cpp/symbian/concurrency/upstream/cpp"
      "${CMAKE_SOURCE_DIR}/third_party/mbedtls-symbian/include")
    if(TARGET Boost::fiber)
      target_link_libraries(${target} PRIVATE Boost::fiber Boost::context)
    endif()
    if(NOT SYMBIAN_INDEX_GUEST_PROBES)
      find_package(pybind11 3.0 CONFIG QUIET)
      if(TARGET pybind11::module)
        target_link_libraries(${target} PRIVATE pybind11::module)
      endif()
    endif()
  endif()
endfunction()
