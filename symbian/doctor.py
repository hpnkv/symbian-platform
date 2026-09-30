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
        "native_analysis": importlib.util.find_spec("symbian._native")
        is not None,
        "target": {
            "model": "Nokia 808 PureView",
            "intended_rm": "RM-807",
            "device_identity_verified": False,
            "firmware_inventory_verified": False,
            "sdk_verified": False,
            "emulator_image_verified": False,
            "symbian_loader_verified": False,
        },
        "next_steps": [
            "Record device identity and firmware using recovery/README.md",
            "Preserve firmware/ROM/Z artifacts with an offline reference copy",
            "Run symbian toolchain probe to test ARM object code generation",
        ],
    }
