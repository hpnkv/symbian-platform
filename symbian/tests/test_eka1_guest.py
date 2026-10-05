"""Opt-in named-firmware EKA1 process acceptance on both CPU backends."""

import os
from pathlib import Path

import pytest

from symbian.emulator.configuration import resolve
from symbian.emulator.eka1 import run_probe
from symbian.status import Code, StatusError
from symbian.tests.test_eka1 import eka1_images as build_eka1_images

eka1_images = build_eka1_images

ROOT = Path(__file__).parents[2]
pytestmark = pytest.mark.skipif(
    not os.environ.get("SYMBIAN_EKA1_GUEST"),
    reason="Set SYMBIAN_EKA1_GUEST for preserved Nokia 7610 acceptance",
)


@pytest.mark.parametrize("backend", ("dynarmic", "dyncom"))
@pytest.mark.parametrize("reason", (7610, 7611))
def test_eka1_process_exit(eka1_images, tmp_path, backend, reason):
    """Checks the real native exit and rejects a deliberately wrong oracle."""
    resolution = resolve(
        root=ROOT,
        overrides={
            "firmware": "7610",
            "store": ROOT / ".symbian/firmware-store",
            "backend": backend,
        },
    )
    image = Path(eka1_images[reason]["artifact"])
    report = run_probe(
        image,
        tmp_path / "accepted",
        resolution,
        expected_reason=reason,
    )
    assert report["accepted"] and report["golden_preserved"]
    assert report["process_exits"] == [
        {
            "name": "eka1_probe[e0000761]0001",
            "uid": 0xE0000761,
            "reason": reason,
            "type": 0,
        }
    ]
    if reason == 7611:
        with pytest.raises(StatusError) as caught:
            run_probe(
                image,
                tmp_path / "changed-result-control",
                resolution,
                expected_reason=7610,
            )
        assert caught.value.code == Code.FAILED_PRECONDITION
        assert "disagrees" in str(caught.value)
