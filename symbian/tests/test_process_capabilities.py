"""Process capabilities declared by device-facing applications."""

import pytest

from symbian.status import Code, StatusError
from symbian.toolchain.executable import _process_capabilities


def test_agent_capabilities_encode_exact_e32_bits():
    assert (
        _process_capabilities(
            ["NetworkServices", "ReadUserData", "WriteUserData", "SwEvent"]
        )
        == 0x1B000
    )


@pytest.mark.parametrize(
    "names",
    [["AllFiles"], ["NetworkServices", "NetworkServices"], [12], ["TCB"]],
)
def test_unapproved_capabilities_are_rejected(names):
    with pytest.raises(StatusError) as error:
        _process_capabilities(names)
    assert error.value.code == Code.INVALID_ARGUMENT
