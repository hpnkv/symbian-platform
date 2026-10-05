"""Builds the shared target SDK once for reuse on all four host platforms."""

import argparse
from pathlib import Path

from symbian.project.sdk import prepare
from symbian.sdk import build_import_proxy
from symbian.sdk.staging import prepare_gui_sdk
from symbian.toolchain.host_tools import llvm_tool


def main() -> None:
    """Stages verified platform headers and builds both guest ISA profiles."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--workspace", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    root = args.workspace.resolve()
    prepare_gui_sdk(
        root / "research/gui_app/source-profile.json",
        root / "research/upstream",
        root / ".symbian/gui-sdk",
    )
    build_import_proxy(
        root / "research/upstream/kernelhwsrv/kernel/eka/eabi/euseru.def",
        [],
        "euser.dll",
        root / ".symbian/runtime-sdk/euser",
        str(llvm_tool("clang++")),
        str(llvm_tool("ld.lld")),
    )
    prepare(root, args.output, include_host=False)


if __name__ == "__main__":
    main()
