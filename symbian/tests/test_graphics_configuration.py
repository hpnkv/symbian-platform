"""Configuration diagnostics for native graphics consumers and dependencies."""

import subprocess
from pathlib import Path

import pytest

MODULE = (
    Path(__file__).parents[2] / "symbian/toolchain/cmake/SymbianGraphics.cmake"
)


@pytest.fixture
def configure(tmp_path):
    sdk = tmp_path / "sdk"
    for dll, header in (
        ("libegl", "EGL/egl.h"),
        ("libglesv1_cm", "GLES/gl.h"),
        ("libglesv2", "GLES2/gl2.h"),
    ):
        for path in (
            sdk / f"proxies/{dll}/{dll}.dso",
            sdk / f"include/graphics/{header}",
        ):
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("fixture")
    for header in (
        "GLES/glext.h",
        "GLES/glplatform.h",
        "GLES/glextplatform.h",
        "GLES/egl.h",
        "GLES/egltypes.h",
        "GLES/legacy_egl_1_1/egl.h",
        "GLES/legacy_egl_1_1/egltypes.h",
        "GLES2/gl2ext.h",
        "GLES2/gl2platform.h",
        "GLES2/gl2extplatform.h",
        "EGL/egltypes.h",
        "EGL/eglext.h",
        "EGL/khronos_types.h",
        "KHR/khrplatform.h",
    ):
        path = sdk / "include/graphics" / header
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("fixture")
    (sdk / "include/platform").mkdir()
    (sdk / "include/platform/e32def.h").write_text("fixture")

    def run(body, *, extra=""):
        (tmp_path / "app.cc").write_text("int main() { return 0; }\n")
        (tmp_path / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.28)\n"
            "project(check LANGUAGES CXX)\n"
            "set(CMAKE_SYSTEM_NAME Generic)\n"
            "set(SYMBIAN_TARGET_ARCH armv6)\n"
            "set(CMAKE_CXX_COMPILER_TARGET armv6-none-eabi)\n"
            f'set(SYMBIAN_SDK_PREFIX "{sdk}")\n'
            "add_library(Symbian::EUser INTERFACE IMPORTED GLOBAL)\n"
            f'include("{MODULE}")\n'
            "add_executable(app app.cc)\n" + body + "\n" + extra
        )
        result = subprocess.run(
            [
                "cmake",
                "-G",
                "Ninja",
                "-S",
                str(tmp_path),
                "-B",
                str(tmp_path / "build"),
            ],
            capture_output=True,
            text=True,
            check=False,
        )
        result.stderr = " ".join(result.stderr.split())
        return result

    return run, sdk


@pytest.mark.parametrize("api", ["GLES1", "GLES2", "EGL"])
def test_each_graphics_api_is_opt_in_and_can_configure(configure, api):
    run, _ = configure
    result = run(f"target_link_libraries(app PRIVATE Symbian::{api})")
    assert result.returncode == 0, result.stdout + result.stderr


@pytest.mark.parametrize(
    "extra, diagnostic",
    [
        ("set(SYMBIAN_TARGET_ARCH armv7)", "ARM EABI toolchain"),
        ("set(CMAKE_CXX_COMPILER_TARGET armv5t-none-eabi)", "ARM EABI"),
        ("set(CMAKE_SYSTEM_NAME Darwin)", "ARM EABI toolchain"),
        ('set(SYMBIAN_GRAPHICS_AVAILABLE_APIS "EGL;GLES1")', "unavailable"),
        ("set_property(TARGET app PROPERTY SYMBIAN_IMAGE_KERNEL eka1)", "EKA1"),
        ('set(CMAKE_CXX_FLAGS "-mfloat-abi=hard")', "incompatible ABI flags"),
        ("target_compile_options(app PRIVATE -fno-short-wchar)", "ABI flags"),
        (
            "target_compile_options(app PRIVATE -march=armv7-a)",
            "architecture flag",
        ),
        (
            "target_compile_options(app PRIVATE -mcpu=cortex-a8)",
            "CPU overrides",
        ),
        ("target_compile_options(app PRIVATE -mfpu=vfp)", "FPU flag"),
        ("target_compile_options(app PRIVATE -mfloat-abi=softfp)", "ABI flags"),
        (
            "set_source_files_properties(app.cc PROPERTIES COMPILE_DEFINITIONS "
            "__OPENGLESHEADERS_LEGACY_EGL_1_1)",
            "legacy EGL 1.1",
        ),
        (
            "set_source_files_properties(app.cc PROPERTIES COMPILE_OPTIONS "
            '"-mabi=aapcs-vfp")',
            "ABI flags",
        ),
    ],
)
def test_invalid_graphics_configuration_fails_before_build(
    configure, extra, diagnostic
):
    run, sdk = configure
    result = run(
        "target_link_libraries(app PRIVATE Symbian::GLES2)", extra=extra
    )
    assert result.returncode != 0
    assert diagnostic in result.stderr
    assert not (sdk.parent / "build/build.ninja").exists()


def test_missing_sdk_payload_has_a_consumer_diagnostic(configure):
    run, sdk = configure
    (sdk / "proxies/libglesv2/libglesv2.dso").unlink()
    result = run("target_link_libraries(app PRIVATE Symbian::GLES2)")
    assert result.returncode != 0
    assert "app: Symbian::GLES2 is unavailable" in result.stderr
    assert "missing SDK file" in result.stderr


def test_transitive_static_links_and_cycles_are_checked(configure):
    run, _ = configure
    result = run(
        "add_library(first STATIC app.cc)\n"
        "add_library(second INTERFACE)\n"
        "target_link_libraries(first PRIVATE second Symbian::GLES1)\n"
        "target_link_libraries(second INTERFACE first Symbian::GLES2)\n"
        "target_link_libraries(app PRIVATE first)\n"
    )
    assert result.returncode != 0
    assert "direct GLES1 and GLES2 imports overlap" in result.stderr


def test_transitive_abi_flags_are_checked(configure):
    run, _ = configure
    result = run(
        "add_library(wrapper INTERFACE)\n"
        "target_link_libraries(wrapper INTERFACE Symbian::GLES2)\n"
        "target_compile_options(wrapper INTERFACE -mfloat-abi=hard)\n"
        "target_link_libraries(app PRIVATE wrapper)\n"
    )
    assert result.returncode != 0
    assert "incompatible ABI flags" in result.stderr


def test_conditional_graphics_links_cannot_skip_validation(configure):
    run, _ = configure
    result = run(
        "target_link_libraries(app PRIVATE "
        '"$<$<CONFIG:Debug>:Symbian::GLES2>")'
    )
    assert result.returncode != 0
    assert "unsupported conditional links" in result.stderr


def test_non_graphics_projects_do_not_inherit_graphics_restrictions(configure):
    run, _ = configure
    result = run(
        "target_link_libraries(app PRIVATE "
        '"$<$<CONFIG:Debug>:Symbian::EUser>")',
        extra='set(SYMBIAN_GRAPHICS_AVAILABLE_APIS "")',
    )
    assert result.returncode == 0, result.stdout + result.stderr


def test_missing_extension_header_is_reported_at_configuration(configure):
    run, sdk = configure
    (sdk / "include/graphics/GLES2/gl2ext.h").unlink()
    result = run("target_link_libraries(app PRIVATE Symbian::GLES2)")
    assert result.returncode != 0
    assert "gl2ext.h" in result.stderr


def test_missing_deployment_library_is_reported_at_configuration(configure):
    run, sdk = configure
    firmware = sdk.parent / "z"
    (firmware / "sys/bin").mkdir(parents=True)
    (firmware / "sys/bin/libegl.dll").write_text("fixture")
    result = run(
        "target_link_libraries(app PRIVATE Symbian::GLES2 Symbian::EGL)",
        extra=f'set(SYMBIAN_GRAPHICS_FIRMWARE_DIR "{firmware}")',
    )
    assert result.returncode != 0
    assert "missing libglesv2.dll" in result.stderr


def test_separate_dlls_can_use_different_gles_apis(configure):
    run, _ = configure
    result = run(
        "add_library(fixed SHARED app.cc)\n"
        "add_library(programmable SHARED app.cc)\n"
        "target_link_libraries(fixed PRIVATE Symbian::GLES1)\n"
        "target_link_libraries(programmable PRIVATE Symbian::GLES2)\n"
        "target_link_libraries(app PRIVATE fixed programmable)\n"
    )
    assert result.returncode == 0, result.stdout + result.stderr


def test_subdirectory_graphics_links_are_validated_at_root(configure):
    run, sdk = configure
    sub = sdk.parent / "child"
    sub.mkdir()
    (sub / "CMakeLists.txt").write_text(
        "add_library(wrapper INTERFACE)\n"
        "target_link_libraries(wrapper INTERFACE Symbian::GLES2)\n"
        "target_compile_options(wrapper INTERFACE -mfloat-abi=hard)\n"
    )
    result = run(
        "add_subdirectory(child)\n"
        "target_link_libraries(app PRIVATE wrapper)\n"
    )
    assert result.returncode != 0
    assert "incompatible ABI flags" in result.stderr
