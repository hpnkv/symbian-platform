# Symbian platform engineering

Read .dev/plan.md and .dev/research.md before expanding platform scope. Record
experiments and open questions in .dev/research-log.md. Update .dev/status.md
with evidence; ARM ELF generation does not prove Symbian loader compatibility.

Use ~/dev/a11 as the implementation reference. Python lives in symbian/,
native libraries in cpp/symbian/<component>/, and Python bindings in
cpp/python/. Use Google C++ style and Google Python docstrings. C++ libraries
default to exceptions disabled and return absl::Status/StatusOr. Selected
translation units may enable exceptions where an implementation boundary
requires them, as in A11's Boost fiber backend and pybind11 bindings. Keep
those boundaries explicit in CMake; exceptions are not the general error
policy. Do not duplicate native format logic in Python. Bindings release the
GIL for native work, acquire it
before accessing Python, and keep Python policy outside native libraries.

Use A11's cpp/thread library for native concurrency when concurrency is needed;
do not introduce a second scheduler. Follow A11's event-loop and deferred Python
reference holders for asynchronous bindings. The initial parsers are synchronous
and stateless, so no scheduler, Python callback, or object holder is needed.

Use GTest for native behavior and Pytest for Python and integration behavior.
Format Python with Black/Ruff at 80 columns, C++ with .clang-format. Use CMake
presets and Ninja for native builds. Keep compile_commands.json available.

Hardware recovery, flashing, erasure, bootloader, OTP, calibration, and partition
operations must never have an agent execution API. Dangerous device operations
need a human authorization mechanism enforced by the eventual device broker.
Preserve unknown device details as unknown. Read-only filesystem permissions
provide accidental-write protection, not an immutable archive against its owner;
preservation needs an independently held offline copy and recorded digest.

Keep upstream research checkouts, firmware, private device data, build products,
and emulator runtime state out of version control. Preserve upstream licenses.
