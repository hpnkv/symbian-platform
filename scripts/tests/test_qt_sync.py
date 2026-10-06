"""Checks reproducible staging of Qt's generated public umbrella headers."""

from symbian.project.qt import _normalize_master_header


def test_master_header_order_is_stable(tmp_path):
    first = tmp_path / "first"
    second = tmp_path / "second"
    first.write_text(
        '#ifndef QT_MODULE_H\n#define QT_MODULE_H\n#include "qz.h"\n'
        '#include "qa.h"\n#endif\n'
    )
    second.write_text(
        '#ifndef QT_MODULE_H\n#define QT_MODULE_H\n#include "qa.h"\n'
        '#include "qz.h"\n#endif\n'
    )
    _normalize_master_header(first)
    _normalize_master_header(second)
    assert first.read_bytes() == second.read_bytes()
