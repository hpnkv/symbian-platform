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


def test_reviewed_utility_manifests_have_target_owned_payloads():
    root = Path(__file__).resolve().parents[2]
    inventory = json.loads(
        (root / "research/native-sdk/inventory.json").read_text()
    )
    owners = {
        "ossrv/genericservices/httputils/group/bld.inf": "Uri",
        "ossrv/lowlevellibsandfws/apputils/group/bld.inf": "Bafl",
        "ossrv/lowlevellibsandfws/pluginfw/Group/bld.inf": "ECom",
    }
    for manifest, owner in owners.items():
        headers = [
            header
            for header in inventory["headers"]
            if header.get("manifest") == manifest
            and header.get("classification") == "public_base_platform"
            and header.get("sha256")
        ]
        assert headers
        for header in headers:
            assert header.get("destination") == (
                "include/native/" + header["include"]
            )
            assert owner in header["targets"]


def test_legacy_cstdlib_conflict_remains_explicit():
    root = Path(__file__).resolve().parents[2]
    inventory = json.loads(
        (root / "research/native-sdk/inventory.json").read_text()
    )
    headers = [
        header
        for header in inventory["headers"]
        if header.get("manifest")
        == "ossrv/genericopenlibs/cstdlib/group/bld.inf"
        and header.get("classification") == "public_base_platform"
    ]
    assert headers
    assert any(
        "Alternative legacy cstdlib/libc export tree"
        in header.get("blocked", "")
        for header in headers
    )


def test_identical_app_architecture_exports_share_reviewed_payload():
    root = Path(__file__).resolve().parents[2]
    inventory = json.loads(
        (root / "research/native-sdk/inventory.json").read_text()
    )
    records = [
        header
        for header in inventory["headers"]
        if header["include"] == "apgcli.h"
        and header.get("classification") == "public_base_platform"
    ]
    assert {record["source"].split("/")[0] for record in records} == {
        "appsupport",
        "appsupport-sdk",
    }
    assert {record["destination"] for record in records} == {
        "include/native/apgcli.h"
    }
    assert all("AppArc" in record["targets"] for record in records)


def test_sip_stringtables_and_frozen_public_interfaces_are_inventoried():
    root = Path(__file__).resolve().parents[2]
    inventory = json.loads(
        (root / "research/native-sdk/inventory.json").read_text()
    )
    facilities = {item["target"]: item for item in inventory["facilities"]}
    for target in (
        "SdpCodec",
        "SipCodec",
        "SipClient",
        "SipProfileCore",
        "SipProfiles",
    ):
        facility = facilities[target]
        assert not facility.get("blocked")
        assert facility["definition"]["source"].startswith(
            "ipappprotocols/realtimenetprots/sipfw/"
        )
        assert facility["definition"]["destination"].endswith(".def")
    for name, target in (
        ("sdpcodecstringconstants.h", "SdpCodec"),
        ("sipstrconsts.h", "SipCodec"),
    ):
        header = next(
            item for item in inventory["headers"] if item["include"] == name
        )
        assert header["destination"] == "include/native/" + name
        assert target in header["targets"]
        assert header["generator"]["source"].endswith("stringtable.pl")


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


def test_avkon_generated_resource_block_keeps_frozen_interface():
    root = Path(__file__).resolve().parents[2]
    inventory = json.loads(
        (root / "research/native-sdk/inventory.json").read_text()
    )
    avkon = next(f for f in inventory["facilities"] if f["target"] == "Avkon")
    assert "avkon.rsg" in avkon["selection_blocked"]
    assert avkon["definition"]["source"].endswith("AVKONU.def")
    assert not avkon.get("blocked")


def test_native_example_manifest_covers_root_and_bundle_projects():
    root = Path(__file__).resolve().parents[2]
    names = json.loads((root / "cmake/SymbianNativeExamples.json").read_text())[
        "examples"
    ]
    assert len(names) == len(set(names))
    discovered = {
        path.name for path in (root / "examples").glob("*_app_classic")
    }
    # Qt's original GUI starter has its own source/bundle workflow.
    discovered.remove("qt_app_classic")
    assert set(names) == discovered | {"linking_app"}
    for name in names:
        project = root / "examples" / name
        assert (project / "CMakeLists.txt").is_file()
        assert (project / "symbian.toml").is_file()
        assert (project / "sdk.cmake").is_file()


def test_calendar_default_frozen_variant_is_explicit():
    root = Path(__file__).resolve().parents[2]
    inventory = json.loads(
        (root / "research/native-sdk/inventory.json").read_text()
    )
    calendar = next(
        item for item in inventory["facilities"] if item["target"] == "Calendar"
    )
    assert calendar["definition"]["source"].endswith("/calinterimapiv3u.def")
    assert calendar["mmp"].endswith("/calinterimapi.mmp")
    assert len(calendar["headers"]) == 23
