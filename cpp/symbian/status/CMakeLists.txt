add_library(symbian_status STATIC status.cc json_codec.cc)
add_library(symbian::status ALIAS symbian_status)
target_include_directories(symbian_status PUBLIC ${PROJECT_SOURCE_DIR}/cpp)
target_link_libraries(symbian_status PUBLIC absl::statusor absl::strings
                      absl::cord absl::status_macros nlohmann_json::nlohmann_json)
symbian_native_library(symbian_status)
