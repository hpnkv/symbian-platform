"""Shared integration-test process policy."""

import pytest


@pytest.fixture(autouse=True)
def keep_emulator_windows_in_background(monkeypatch):
    """Leaves the developer's IDE or terminal active during GUI tests."""
    monkeypatch.setenv("EKA2L1_RESEARCH_BACKGROUND_WINDOW", "1")
    monkeypatch.setenv("QT_MAC_DISABLE_FOREGROUND_APPLICATION_TRANSFORM", "1")
