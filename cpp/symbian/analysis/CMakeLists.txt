add_library(symbian_analysis STATIC elf.cc arm_attributes.cc)
add_library(symbian::analysis ALIAS symbian_analysis)
target_include_directories(symbian_analysis PUBLIC ${PROJECT_SOURCE_DIR}/cpp)
target_link_libraries(symbian_analysis PUBLIC absl::statusor absl::status_macros
  absl::strings)
symbian_native_library(symbian_analysis)
