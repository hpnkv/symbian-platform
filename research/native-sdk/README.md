# Public native SDK inventory

`sources.json` pins public research snapshots. `facilities.json` records named
library selections and their consumer dependencies. `inventory.json` connects
public header-export declarations and Nokia API release metadata to delivery
paths, source digests and CMake targets. Regenerate with the project interpreter:

```sh
.venv/bin/python scripts/update_native_inventory.py
.venv/bin/python scripts/update_native_inventory.py --check
```

The scanner includes secondary export `.inf` files, excludes test exports, and
accepts Nokia `release category="sdk"` or `"public"` metadata as SDK inclusion
evidence. A PLATFORM export macro alone does not establish a public API.
Unresolved entries remain explicit; the inventory is not a completeness claim.
DLL implementation dependencies in MMP files are preserved as provenance. They
are not automatically application dependencies: internal server libraries must
not be exposed by following every MMP LIBRARY declaration.

These source snapshots chiefly describe Symbian^3 in 2010. They are not an
authenticated manifest of the Belle FP2 SDK or of any owner's phone. Expected
historical SDK inclusion, frozen-interface compatibility, loader acceptance,
named-firmware execution and physical-device compatibility are separate facts.
The Nokia 808/Belle fixture remains the initial runtime validation target.

Sources remain under ignored `research/upstream/`; only metadata and owned
staging/build logic belong in Git. Original notices accompany copied headers.
Import proxies contain frozen interfaces, not redistributed firmware DLL bodies.
