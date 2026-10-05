# Research-only GPL adapter. Never linked into the production Python wheel.
find_package(Qt6 REQUIRED COMPONENTS Widgets Network)
add_library(symbian_emulator_control STATIC control.cc distribution.cc)
file(STRINGS "${SYMBIAN_PLATFORM_ROOT}/research/eka2l1/VERSION"
     SYMBIAN_EMULATOR_VERSION LIMIT_COUNT 1)
if(NOT SYMBIAN_EMULATOR_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
  message(FATAL_ERROR "Invalid emulator distribution VERSION")
endif()
target_compile_definitions(symbian_emulator_control PRIVATE
    SYMBIAN_EMULATOR_VERSION="${SYMBIAN_EMULATOR_VERSION}")
target_compile_features(symbian_emulator_control PUBLIC cxx_std_20)
target_compile_options(symbian_emulator_control PRIVATE -fno-exceptions)
# Original kernel/window headers contain throwing templates. These foreign
# implementation boundaries need exceptions even though our API uses
# Status; the other native adapters retain the default no-exceptions policy.
set_source_files_properties(control.cc firmware.cc firmware_main.cc
  PROPERTIES COMPILE_OPTIONS -fexceptions)
target_include_directories(symbian_emulator_control PUBLIC
                           "${SYMBIAN_PLATFORM_ROOT}/cpp")
target_include_directories(symbian_emulator_control PRIVATE
                           "${PROJECT_SOURCE_DIR}/src/emu/qt/include")
target_link_libraries(symbian_emulator_control PUBLIC
    absl::statusor nlohmann_json::nlohmann_json Qt6::Widgets Qt6::Network
    PRIVATE epoc epockern epocservs epocmem epocpkg config drivers)

add_library(symbian_emulator_firmware STATIC firmware.cc)
target_compile_features(symbian_emulator_firmware PUBLIC cxx_std_20)
target_compile_options(symbian_emulator_firmware PRIVATE -fno-exceptions)
target_include_directories(symbian_emulator_firmware PUBLIC
                           "${SYMBIAN_PLATFORM_ROOT}/cpp")
target_link_libraries(symbian_emulator_firmware PUBLIC
    absl::statusor nlohmann_json::nlohmann_json common
    PRIVATE epoc epocloader config)
add_executable(symbian_firmware_tool firmware_main.cc)
target_compile_options(symbian_firmware_tool PRIVATE -fno-exceptions)
target_link_libraries(symbian_firmware_tool PRIVATE symbian_emulator_firmware)
