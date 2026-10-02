"""Run visible SDK utilities without editable-install import redirections."""

import importlib.machinery
import runpy
import sys
from pathlib import Path

# Retain normal site-package dependencies, but ensure SDK modules are loaded
# from the selected SDK. Editable pip finders can otherwise override PYTHONPATH.
sys.meta_path = [
    importlib.machinery.BuiltinImporter,
    importlib.machinery.FrozenImporter,
    importlib.machinery.PathFinder,
]
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "lib/python"))
arguments = sys.argv[1:]
if arguments[:1] == ["-m"] and len(arguments) >= 2:
    sys.argv = arguments[1:]
    runpy.run_module(arguments[1], run_name="__main__", alter_sys=True)
elif arguments[:1] == ["-c"] and len(arguments) >= 2:
    sys.argv = ["-c", *arguments[2:]]
    exec(arguments[1], {"__name__": "__main__"})
elif arguments[:1] in (["--version"], ["-V"]):
    print(sys.version)
elif arguments:
    sys.argv = arguments
    runpy.run_path(arguments[0], run_name="__main__")
else:
    raise SystemExit("Use sdk/bin/python -m module, -c code, or script")
