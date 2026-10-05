"""CMake SDK discovery for installed applications and workspace presets."""

import json
from pathlib import Path

from symbian.process import run


def test_active_sdk_preset_refreshes_cached_selection(tmp_path, monkeypatch):
    config = tmp_path / "config"
    (config / "symbian").mkdir(parents=True)
    active = config / "symbian/active-sdk.json"
    selected = tmp_path / "new-sdk"
    active.write_text(json.dumps({"manifest": str(selected / "sdk.json")}))
    monkeypatch.setenv("XDG_CONFIG_HOME", str(config))
    monkeypatch.delenv("SYMBIAN_SDK_MANIFEST", raising=False)
    module = Path(__file__).parents[1] / "toolchain/cmake/SymbianSdk.cmake"
    script = tmp_path / "select.cmake"
    script.write_text(
        f'include("{module}")\n'
        'set(SYMBIAN_SDK_PREFIX "old-sdk")\n'
        "set(SYMBIAN_USE_ACTIVE_SDK ON)\n"
        "symbian_select_sdk()\n"
        f'if(NOT SYMBIAN_SDK_PREFIX STREQUAL "{selected}")\n'
        '  message(FATAL_ERROR "Cached SDK masked the active SDK")\n'
        "endif()\n"
        "set(SYMBIAN_USE_ACTIVE_SDK OFF)\n"
        'set(SYMBIAN_SDK_PREFIX "explicit-sdk")\n'
        "symbian_select_sdk()\n"
        'if(NOT SYMBIAN_SDK_PREFIX STREQUAL "explicit-sdk")\n'
        '  message(FATAL_ERROR "Explicit SDK selection was lost")\n'
        "endif()\n"
    )
    run(["cmake", "-P", str(script)], cwd=tmp_path)
