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
    absl::statusor Qt6::Widgets Qt6::Network
    PRIVATE epoc epockern epocservs epocmem epocpkg config drivers)
