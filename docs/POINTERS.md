# Relocated const tables and C++ dispatch

The native E32 converter now emits code relocations for retained R_ARM_ABS32
words whose resolved values point into its RX mapping. This supports const
callback tables, pointers into constant text and simple C++ vtables. The
maintained pointer probe executes ARM and Thumb callbacks and a virtual method
on both emulator CPU backends. A packaged copy also installs and launches
through the existing emulator kernel. Matched Belle remains unverified.

```sh
uv run symbian build --project examples/pointer_probe \
  --output .symbian/pointer-probe
uv run symbian package --project examples/pointer_probe \
  --artifact .symbian/pointer-probe/pointer_probe.exe \
  --output .symbian/pointer-package
cmake --build build/eka2l1 --target symbian_pointer_probe symbian_package_probe
uv run symbian toolchain verify-pointers \
  .symbian/pointer-probe/pointer_probe.exe \
  --package .symbian/pointer-package/probe.sis
```

Build the research dependencies as described in research/eka2l1/README.md first.
Without `--package`, verification runs 14 checksum, structural validation and
mapped-pointer/dispatch cases. With it, seven checksum/installer cases bring
the run to 21, including registry reload, uninstall and reinstall. Reports
retain private input copies, binary/artifact digests, JSON and logs under
.symbian/pointer-check. They expose the scoped emulator checks while keeping
Belle runtime and physical installation flags false. `verify-probe` requires
the earlier relocation-free example; it rejects this new profile explicitly.

## Compiler and image contract

Clang emits address-bearing const objects into writable ELF `.data.rel.ro`
sections because their pointers require load-time adjustment. The example
linker preserves that section separately within its one RX load segment:

```ld
.data.rel.ro : { *(.data.rel.ro .data.rel.ro.*) } :code
```

The converter accepts SHT_PROGBITS sections named `.data.rel.ro` or with that
prefix and a dot suffix, without the executable flag, in the validated RX
mapping. Their names and contents must come from a trusted compiler/link.
They become code-region tables; this does not add a writable data segment.
Ordinary writable data/BSS, TLS, constructor arrays and unknown writable sections
remain rejected. Renaming the section or corrupting its name table is tested.
The profile does not establish semantic safety of arbitrary hand-authored code.

Internal const table declarations and the example class use hidden ELF
visibility. This lets Clang emit local relative access instead of unsupported
GOT_PREL references to potentially preemptible data. The normal project helper
continues to use PIC; visibility is selected in the example's declarations.
Public function exports still use their normal visible symbols for frozen DEF
resolution. Exceptions and RTTI are disabled; no hosted C++ runtime is linked.

Retained ABS32 in this ET_EXEC transport describes an already resolved word.
The converter keeps that value, including ARM/Thumb state and permitted in-range
addends, and emits a Symbian text relocation at its aligned code offset. It
must not add the symbol value again. Undefined/external absolute pointers,
GOT/dynamic metadata fixups, unaligned slots, duplicate fixups, out-of-range
values and mismatched function state remain rejected. Pointer targets must
stay within the code mapping; one-past-the-mapping values are not supported.
The combined application/export relocation count is bounded to 65,535.

Native inspection checks canonical relocation pages, bounded pointer targets,
all required DLL export fixups, and disjointness from eager import slots.
Internal pointer fixups can coexist with function imports and frozen exports;
separate real-link cases pass Nokia's checksum and validator for both layouts.
The combined DLL case uses executable startup and is layout evidence only.
Pointer records remain native C++ logic; Python owns build/verification policy.

## Runtime evidence and limits

Four mapped words are checked against the unchanged emulator loader's actual
code-base delta: a Thumb callback, an ARM callback, a pointer with addend one
into constant text, and a Thumb virtual method. The research harness observes
the CPU at all three function targets in the correct instruction state. Both
backends exit zero normally, exit 42 with changed input, and launch successfully
after the failure. Kernel exit releases the address space. The virtual object's
vptr lives on the stack; only its table needs image relocation.

The independent installer checks unchanged installed bytes and registry fields,
using a separate Python hashlib SHA-1 reference retained as a test input.
The original probe keeps its previously recorded hash by default. The verifier
clears inherited Symbian fixture variables and GTest filters/shards before
supplying private input copies. It requires every expected case to complete.
The installed wheel builds identical ELF/E32/SIS bytes and performs the same
21-case verification outside the source import path.

This is a no-resource, single-inheritance experiment. It does not establish
multiple/virtual inheritance, RTTI, dynamically allocated objects, global
construction/destruction, writable data/TLS, target DLL initialization, SDK
heap/cleanup/leaves or full Symbian C++ ABI. Direct ThreadKill still skips
User::Exit cleanup. No target EUSER, matched ROM/Z or physical phone is involved.
