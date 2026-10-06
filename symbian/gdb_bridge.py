"""GDB Python hook for the actual guest mapping in a private GUI session."""

import json
import re
import time
from pathlib import Path


def quote(path: Path) -> str:
    """Quotes a local path for a GDB command without shell interpretation."""
    return '"' + str(path).replace("\\", "\\\\").replace('"', '\\"') + '"'


def register(
    directory: str,
    symbols: str,
    source: str,
    headers: str,
    base: int,
    name: str = "gui_app",
    uid: int = 0xE0000811,
):
    """Installs symbol relocation after the remote target has initialized.

    Args:
        directory: Retained private session directory containing kernel logs.
        symbols: ELF file corresponding to the freshly installed E32.
        source: Local source root for the GUI example.
        headers: Local staged SDK root.
        base: Link-time code base inspected by the native E32 parser.
        name: Application target name used in its debug source paths.
        uid: Application UID3 used to identify its loaded image.
    """
    import gdb

    class Relocate(gdb.Command):
        """Applies the mapping before CLion inserts guest breakpoints."""

        def __init__(self):
            super().__init__("symbian-relocate", gdb.COMMAND_FILES)

        def invoke(self, _argument, _from_tty):
            log = Path(directory) / "instance/EKA2L1.log"
            deadline = time.monotonic() + 10
            while True:
                matches = re.findall(
                    rf"{re.escape(name)}\.exe \(UID3=0x{uid:08X}\) "
                    r"runtime code: "
                    r"(0x[0-9a-fA-F]+)",
                    log.read_text(errors="replace"),
                )
                if len(matches) == 1:
                    break
                if time.monotonic() >= deadline:
                    raise gdb.GdbError("GUI runtime mapping unavailable")
                time.sleep(0.05)
            slide = int(matches[0], 16) - base
            gdb.execute(f"symbol-file -o {slide:#x} {quote(Path(symbols))}")
            gdb.execute(
                f"set substitute-path /symbian-src/{name} "
                + quote(Path(source))
            )
            gdb.execute(
                "set substitute-path /symbian-sdk/include "
                + quote(Path(headers))
            )
            (Path(directory) / "gdb-mapping.json").write_text(
                json.dumps(
                    {"runtime_base": int(matches[0], 16), "symbol_slide": slide}
                )
                + "\n"
            )

    Relocate()
