set(symbian_native_examples_file
  "${CMAKE_CURRENT_LIST_DIR}/SymbianNativeExamples.json")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${symbian_native_examples_file}")
file(READ "${symbian_native_examples_file}"
  symbian_native_examples_manifest)
string(JSON symbian_native_example_count LENGTH
  "${symbian_native_examples_manifest}" examples)
set(SYMBIAN_NATIVE_EXAMPLES)
math(EXPR symbian_native_example_last "${symbian_native_example_count} - 1")
foreach(index RANGE 0 ${symbian_native_example_last})
  string(JSON application GET "${symbian_native_examples_manifest}" examples
    ${index})
  list(APPEND SYMBIAN_NATIVE_EXAMPLES "${application}")
endforeach()
