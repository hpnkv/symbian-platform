"""Explicit ARM target policy, separate from host and firmware identity."""

import json
from pathlib import Path
from typing import Literal

from pydantic import BaseModel, ConfigDict

from symbian.status import Code, StatusError

Architecture = Literal["armv5t", "armv6"]
DEFAULT_ARCHITECTURE: Architecture = "armv6"


class ArmTarget(BaseModel):
    """One supported instruction-set profile with the shared Symbian EABI."""

    model_config = ConfigDict(frozen=True)
    architecture: Architecture
    triple: str
    cpu_attribute: int
    e32_cpu: int
    address_bits: int = 32
    float_abi: str = "soft"


TARGETS = {
    "armv5t": ArmTarget(
        architecture="armv5t",
        triple="armv5t-none-eabi",
        cpu_attribute=3,
        e32_cpu=0x2001,
    ),
    "armv6": ArmTarget(
        architecture="armv6",
        triple="armv6-none-eabi",
        cpu_attribute=6,
        e32_cpu=0x2002,
    ),
}


def target(value: str) -> ArmTarget:
    """Returns a supported target or a recoverable configuration error."""
    if value not in TARGETS:
        raise StatusError(
            Code.INVALID_ARGUMENT,
            f"Unsupported ARM target {value!r}; choose armv6 or armv5t",
        )
    return TARGETS[value]


def project_architecture(project: Path, options: dict, preset: str) -> str:
    """Resolves the declared project or CMake target architecture."""
    declared = options.get("architecture")
    if declared is not None:
        return target(declared).architecture
    preferences = project / "symbian-project.json"
    if preferences.is_file():
        saved = json.loads(preferences.read_text())["preferences"]
        return target(
            saved.get("architecture", DEFAULT_ARCHITECTURE)
        ).architecture
    presets = json.loads((project / "CMakePresets.json").read_text())
    chosen = next(
        (
            item
            for item in presets["configurePresets"]
            if item["name"] == preset
        ),
        {},
    )
    cached = chosen.get("cacheVariables", {}).get("SYMBIAN_TARGET_ARCH")
    if cached is not None:
        return target(cached).architecture
    return DEFAULT_ARCHITECTURE


def require_execution_architecture(architecture: str, backend: str) -> None:
    """Checks the maintained emulator ISA envelope before session creation.

    Firmware build ISA is not evidence of a phone's physical chip. Both owned
    emulator backends provide the tested ARMv6 envelope independently of it.
    """
    if backend not in ("dynarmic", "dyncom") or architecture not in TARGETS:
        raise StatusError(
            Code.FAILED_PRECONDITION,
            f"Cannot run {architecture} on {backend}; rebuild for armv6 or "
            "armv5t and select dynarmic or dyncom. No guest was started",
        )
