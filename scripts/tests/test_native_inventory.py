"""Native API inventory controls using real source-manifest syntax."""

import json
from pathlib import Path

from scripts.update_native_inventory import exports, resolve_case
from scripts.update_qt_mobility_inventory import public_aliases


def test_export_boundary_and_sdk_packaging(tmp_path):
    component = tmp_path / "example/api"
    group = component / "group"
    group.mkdir(parents=True)
    (component / "public.h").write_text("// EPL-1.0\n")
    (component / "private.h").write_text("// internal\n")
    (group / "bld_include.inf").write_text(
        "PRJ_EXPORTS\n"
        "../public.h OS_LAYER_PLATFORM_EXPORT_PATH(public.h)\n"
        "PRJ_TESTEXPORTS\n"
        "../private.h OS_LAYER_PUBLIC_EXPORT_PATH(private.h)\n"
    )
    (component / "api.metaxml").write_text(
        '<api><libs><lib name="api.lib"/></libs>'
        '<release category="sdk"/></api>'
    )
    public, private = exports(tmp_path, ["example"])
    assert [r["include"] for r in public] == ["public.h"]
    assert public[0]["api_libraries"] == ["api.lib"]
    assert not private


def test_platform_source_does_not_imply_public_api(tmp_path):
    group = tmp_path / "example/group"
    group.mkdir(parents=True)
    (group.parent / "private.h").write_text("// implementation\n")
    (group / "bld.inf").write_text(
        "PRJ_EXPORTS\n"
        "../private.h OS_LAYER_PLATFORM_EXPORT_PATH(private.h)\n"
    )
    public, private = exports(tmp_path, ["example"])
    assert not public
    assert private[0]["classification"] == "private_internal"


def test_windows_source_case_resolves_on_every_host(tmp_path):
    (tmp_path / "Upper.H").write_text("original")
    assert resolve_case(tmp_path / "upper.h").read_text() == "original"


def test_inventory_covers_more_than_requested_facilities():
    root = Path(__file__).resolve().parents[2]
    inventory = json.loads(
        (root / "research/native-sdk/inventory.json").read_text()
    )
    assert len(inventory["facilities"]) > 100
    assert len(inventory["headers"]) > 2000
    assert inventory["private_export_count"] > 0
    assert any(h.get("blocked") for h in inventory["headers"])
    assert all(
        h.get("destination") or h.get("blocked") for h in inventory["headers"]
    )
    assert any(f.get("api_metadata") for f in inventory["facilities"])


def test_qt_mobility_aliases_exclude_private_classes(tmp_path):
    header = tmp_path / "src/contacts/qcontact.h"
    header.parent.mkdir(parents=True)
    header.write_text(
        "class Q_CONTACTS_EXPORT QContact : public QObject {};\n"
        "class QContactPrivate;\n"
        "class QContactPrivate\n{\n};\n"
    )
    assert public_aliases(tmp_path, "contacts", ["qcontact.h"]) == [
        {"include": "QtContacts/QContact", "header": "qcontact.h"}
    ]
