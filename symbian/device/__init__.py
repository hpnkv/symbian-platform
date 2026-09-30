"""Hardware policy descriptions; this package exposes no device executor."""

from symbian.status import Code, StatusError

SAFE_OPERATIONS = frozenset(
    {
        "info",
        "logs",
        "screenshot",
        "install-application",
        "uninstall-application",
        "run-application",
        "diagnostics",
    }
)
HUMAN_OPERATIONS = frozenset(
    {
        "reboot",
        "modify-system-files",
        "install-system-component",
        "replace-system-dll",
        "modify-persistent-state",
    }
)
RECOVERY_OPERATIONS = frozenset(
    {
        "flash",
        "erase",
        "bootloader",
        "partition",
        "otp",
        "calibration",
        "low-level-recovery",
    }
)


def policy(operation: str) -> dict:
    """Classifies authority without approving or executing an operation."""
    if operation in SAFE_OPERATIONS:
        tier = "safe-candidate"
    elif operation in HUMAN_OPERATIONS:
        tier = "human-authorization-required"
    elif operation in RECOVERY_OPERATIONS:
        tier = "outside-agent-authority"
    else:
        raise StatusError(Code.PERMISSION_DENIED, "Unknown device operation")
    return {
        "operation": operation,
        "tier": tier,
        "executor_available": False,
        "authorized": False,
    }
