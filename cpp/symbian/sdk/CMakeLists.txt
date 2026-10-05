add_library(symbian_sdk STATIC exports.cc)
add_library(symbian::sdk ALIAS symbian_sdk)
target_include_directories(symbian_sdk PUBLIC ${PROJECT_SOURCE_DIR}/cpp)
target_link_libraries(symbian_sdk PUBLIC symbian::analysis
                      absl::strings absl::statusor)
symbian_native_library(symbian_sdk)
