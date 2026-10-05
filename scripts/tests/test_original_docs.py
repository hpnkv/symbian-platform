"""Checks that original API supplements preserve exact reference contracts."""

import importlib.util
import json
import xml.etree.ElementTree as ET
from pathlib import Path

import pytest

_SPEC = importlib.util.spec_from_file_location(
    "original_docs",
    Path(__file__).parents[2] / "doc/cpp/platform/generate_descriptions.py",
)
assert _SPEC is not None and _SPEC.loader is not None
_DOCS = importlib.util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(_DOCS)


def member(identifier, title, signature, section):
    """Creates the hosted library's actual heading/signature/section shape."""
    return (
        f'<div class="nested1" id="{identifier}">'
        f'<h2 class="topictitle2">{title}</h2>'
        f'<table class="signature">{signature}</table>{section}</div>'
    )


def test_const_overloads_and_default_parameter_names():
    page = member(
        "read",
        "At(TInt)",
        "<tr><td>TInt</td><td>At</td><td>(</td><td>TInt</td>"
        "<td>aIndex</td><td>)</td><td>const [inline]</td></tr>",
        '<div class="section"><p>Reads an item.</p></div>'
        '<div class="section parameters"><table><tr>'
        "<td>TInt aIndex = 0</td><td>Zero-based item index.</td>"
        "</tr></table></div>",
    ) + member(
        "write",
        "At(TInt)",
        "<tr><td>TInt &amp;</td><td>At</td><td>(</td><td>TInt</td>"
        "<td>aIndex</td><td>)</td><td>[inline]</td></tr>",
        '<div class="section"><p>Returns a mutable item.</p></div>',
    )
    references = _DOCS.reference(_DOCS.Tree(page).root, "https://example.org")
    assert references["At(TInt)const"]["description"] == ["Reads an item."]
    assert references["At(TInt)const"]["parameters"] == {
        "aIndex": "Zero-based item index."
    }
    assert references["At(TInt)"]["description"] == ["Returns a mutable item."]


def test_fields_enumerators_and_error_contracts():
    page = member(
        "state",
        "TInt iState",
        "<tr><td>TInt</td><td>iState</td><td>[protected]</td></tr>",
        '<div class="section"><p>Current connection state.</p></div>',
    ) + member(
        "mode",
        "Enum TMode",
        "",
        '<div class="section"><p>Controls connection setup.</p>'
        '<dl class="user"><dt>panic</dt>'
        "<dd>Example 1 for an invalid mode.</dd></dl></div>"
        '<div class="section enumerators"><table><tr>'
        "<td>EConnect = 1</td><td>Open the connection.</td>"
        "</tr></table></div>",
    )
    references = _DOCS.reference(_DOCS.Tree(page).root, "https://example.org")
    assert references["iState"]["description"] == ["Current connection state."]
    assert references["TMode"]["description"] == ["Controls connection setup."]
    assert references["TMode"]["enumerators"] == {
        "EConnect": "Open the connection."
    }
    assert references["TMode"]["notes"] == [
        "\\panic Example 1 for an invalid mode."
    ]


def test_conflicting_documented_overloads_fail():
    first = member(
        "first", "Open()", "", '<div class="section"><p>Opens.</p></div>'
    )
    second = member(
        "second", "Open()", "", '<div class="section"><p>Closes.</p></div>'
    )
    with pytest.raises(ValueError, match="Ambiguous hosted overload"):
        _DOCS.reference(_DOCS.Tree(first + second).root, "https://example.org")


def test_generation_uses_contracts_and_end_suffixes(tmp_path, monkeypatch):
    xml = tmp_path / "xml"
    cache = tmp_path / "cache"
    output = tmp_path / "supplements"
    xml.mkdir()
    cache.mkdir()
    (xml / "index.xml").write_text(
        '<doxygenindex><compound refid="socket" kind="class">'
        "<name>Socket</name></compound></doxygenindex>"
    )
    (xml / "socket.xml").write_text(
        "<doxygen><compounddef><compoundname>Socket</compoundname>"
        '<sectiondef><memberdef kind="function" id="open" prot="public">'
        "<name>Open</name><definition>TInt Socket::Open</definition>"
        "<argsstring>(TInt aMode)</argsstring><type>TInt</type>"
        "<param><type>TInt</type><declname>aMode</declname></param>"
        '<location file="socket.h"/></memberdef>'
        '<memberdef kind="function" id="unknown" prot="public">'
        "<name>Unknown</name><definition>void Socket::Unknown</definition>"
        '<argsstring>()</argsstring><location file="socket.h"/>'
        "</memberdef></sectiondef></compounddef></doxygen>"
    )
    url = "https://example.org/socket.html"
    (cache / "fetch-input.json").write_text(json.dumps({"Socket": url}))
    (cache / "socket.html").write_text(
        member(
            "open",
            "Open(TInt)",
            "",
            '<div class="section"><p>Opens a connection.</p>'
            '<dl class="user"><dt>return</dt>'
            "<dd>Zero on success; an error otherwise.</dd></dl></div>"
            '<div class="section parameters"><table><tr>'
            "<td>TInt aConnectionMode</td><td>Connection mode.</td>"
            "</tr></table></div>",
        )
    )
    # This fixture has no C++ examples; formatting is tested by the shared
    # formatter's own checks and the rendered real supplements.
    monkeypatch.setattr(_DOCS, "format_examples", lambda source: source)
    _DOCS.generate(xml, cache, output)
    supplement = (output / "socket.h.dox").read_text()
    assert "Unknown" not in supplement
    assert "\\param aMode Connection mode." in supplement
    assert "\\return Zero on success; an error otherwise." in supplement
    assert supplement.index("Opens a connection.") < supplement.index(
        "(generated from"
    )
    assert supplement.rstrip().endswith(
        f'<p><small>(generated from <a href="{url}#open">'
        "Symbian Developer Library</a>)</small></p>\n*/"
    )


def test_prose_does_not_create_html_or_doxygen_commands():
    escaped = _DOCS.escape('TArray<T> & "value" @param */')
    assert escaped == (
        "TArray&lt;T&gt; &amp; &quot;value&quot; &#64;param * / "
    )


def test_api_status_is_not_a_function_description():
    symbol = ET.fromstring(
        "<memberdef><briefdescription/><detaileddescription><para>"
        '<simplesect kind="par"><title>API status</title>'
        "<para>Published to all clients.</para></simplesect>"
        "</para></detaileddescription></memberdef>"
    )
    assert not _DOCS.has_description(symbol)
    symbol.find("briefdescription").text = "Opens the file."
    assert _DOCS.has_description(symbol)
