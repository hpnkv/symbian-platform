"""Native frozen exports and real Clang/LLD ordinal proxy integration."""

import json
import os
import shutil
from pathlib import Path

import pytest

from symbian._native import inspect_import_proxy, parse_def
from symbian.cli.__main__ import main
from symbian.sdk import build_import_proxy, inspect_proxy
from symbian.status import Code, StatusError

DEFINITION = "EXPORTS\n_ZN4User4ExitEi @ 641 NONAME\nFunction @ 7 NONAME\n"


@pytest.fixture(scope="module")
def proxy(tmp_path_factory):
    if not shutil.which("clang++") or not shutil.which("ld.lld"):
        pytest.skip("Clang/LLD required")
    root = tmp_path_factory.mktemp("SDK paths with spaces")
    definition = root / "euser.def"
    definition.write_text(DEFINITION)
    return build_import_proxy(
        definition, ["_ZN4User4ExitEi", "Function"], "euser.dll", root / "proxy"
    )


def test_real_proxy_preserves_sparse_ordinals_and_version_metadata(proxy):
    assert proxy["reproducible"]
    assert proxy["proxy"]["target_dll"] == "euser.dll"
    assert [item["ordinal"] for item in proxy["proxy"]["exports"]] == [7, 641]
    assert inspect_proxy(Path(proxy["artifact"])) == proxy["proxy"]
    assert not proxy["import_execution_verified"]
    assert not proxy["symbian_loader_verified"]
    commands = json.loads(Path(proxy["compile_commands"]).read_text())
    assert any("exports.S" in row["file"] for row in commands)
    for row in commands:
        assert Path(row["directory"]).is_dir()
    assert proxy["inputs"]


def test_native_inspector_rejects_all_real_proxy_truncations(proxy):
    data = Path(proxy["artifact"]).read_bytes()
    for length in range(len(data)):
        with pytest.raises(StatusError):
            inspect_import_proxy(data[:length])
    result = inspect_import_proxy(data)
    with pytest.raises(AttributeError):
        result.exports[0].ordinal = 1


def test_cli_inspection_and_bad_definition_are_structured(
    proxy, tmp_path, capsys
):
    assert main(["inspect", proxy["artifact"], "--format", "import-proxy"]) == 0
    result = json.loads(capsys.readouterr().out)
    assert result["result"]["metadata"] == proxy["proxy"]
    definition = tmp_path / "malformed.def"
    definition.write_text("EXPORTS\nFunction @ 7 NONAME\nOther @ 7 NONAME\n")
    assert (
        main(
            [
                "toolchain",
                "import-proxy",
                str(definition),
                "--symbol",
                "Function",
                "--target-dll",
                "euser.dll",
                "--output",
                str(tmp_path / "output"),
            ]
        )
        == 1
    )
    assert (
        json.loads(capsys.readouterr().out)["status"]["name"]
        == "INVALID_ARGUMENT"
    )
    assert not (tmp_path / "output").exists()


def test_aliases_and_absent_exports_do_not_become_proxies(tmp_path):
    definition = tmp_path / "test.def"
    definition.write_text("EXPORTS\nRemoved @ 3 NONAME ABSENT\n")
    with pytest.raises(StatusError) as caught:
        build_import_proxy(
            definition, ["Removed"], "euser.dll", tmp_path / "out"
        )
    assert caught.value.code == Code.UNIMPLEMENTED
    with pytest.raises(StatusError):
        parse_def(b"EXPORTS\nA=B @ 1 NONAME\n")
    with pytest.raises(StatusError):
        parse_def(b"EXPORTS\nName @ 0 NONAME\n")


@pytest.mark.skipif(
    not os.environ.get("SYMBIAN_PUBLIC_KERNEL_SOURCE"),
    reason="Set SYMBIAN_PUBLIC_KERNEL_SOURCE for public SDK header research",
)
def test_original_public_headers_compile_and_link_user_exit(tmp_path):
    source = Path(os.environ["SYMBIAN_PUBLIC_KERNEL_SOURCE"]).resolve()
    result = build_import_proxy(
        source / "kernel/eka/eabi/euseru.def",
        ["_ZN4User4ExitEi"],
        "euser.dll",
        tmp_path / "out",
        headers=source / "kernel/eka/include",
    )
    assert result["exports_in_definition"] == 2546
    assert result["proxy"]["exports"][0]["ordinal"] == 641
    assert Path(result["sdk_header_link_probe"]).is_file()
    assert any(path.endswith("e32cmn.h") for path in result["inputs"])
    assert any(path.endswith("e32std.h") for path in result["inputs"])
    assert not result["import_execution_verified"]
