# Research-only GPL adapter. Never linked into the production Python wheel.
find_package(Qt6 REQUIRED COMPONENTS Widgets Network)
add_library(symbian_emulator_control STATIC control.cc)
target_compile_features(symbian_emulator_control PUBLIC cxx_std_20)
target_compile_options(symbian_emulator_control PRIVATE -fno-exceptions)
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
