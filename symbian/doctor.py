"""Read-only host readiness diagnostics."""

import importlib.util
import platform
import shutil


def doctor() -> dict:
    """Reports demonstrated host tools separately from missing target assets."""
    tools = {
        name: shutil.which(name)
        for name in ("clang++", "cmake", "ninja", "clangd", "ld.lld", "eka2l1")
    }
    return {
        "schema": "symbian.doctor/v1",
        "host": {"system": platform.system(), "machine": platform.machine()},
        "tools": tools,
        "native_analysis": (
            importlib.util.find_spec("symbian._native") is not None
        ),
        "target": {
            "application_abi": "ARM EABI / EKA2 E32-V",
            "firmware_selection": "symbian emu resolve",
            "device_identity_verified": False,
            "firmware_inventory_verified": False,
            "sdk_verified": False,
            "emulator_image_verified": False,
            "symbian_loader_verified": False,
        },
        "next_steps": [
            "Import separately supplied ROM/Z using symbian firmware import",
            "Preserve firmware/ROM/Z artifacts with an offline reference copy",
            "Build examples/e32_probe to test Clang/LLD and E32 conversion",
            (
                "Validate the executable against the selected device's"
                " ABI/services"
            ),
        ],
    }
