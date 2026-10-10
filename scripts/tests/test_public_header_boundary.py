"""The public SDK include graph must not expose original OS headers."""

from scripts.check_public_header_boundary import violations


def test_transitive_native_header_is_rejected(tmp_path):
    include = tmp_path / "include"
    public = include / "symbian/api/example.h"
    helper = include / "symbian/internal/helper.h"
    native = include / "platform/e32def.h"
    for path in (public, helper, native):
        path.parent.mkdir(parents=True, exist_ok=True)
    public.write_text('#include "symbian/internal/helper.h"\n')
    helper.write_text("#include <e32def.h>\n")
    native.write_text("// original OS header\n")

    assert len(violations(include)) == 1

    helper.write_text("#include <cstdint>\n")
    assert violations(include) == []
