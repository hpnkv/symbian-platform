"""Generates offline Doxygen supplements from indexed, hosted API references.

The original header snapshots remain untouched. Refreshes consume an explicit
local cache of live reference pages; the normal documentation build uses the
reviewed supplements and does not need network access.
"""

import argparse
import json
import re
import sys
import xml.etree.ElementTree as ET
from collections import defaultdict
from html import escape as html_escape
from html.parser import HTMLParser
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
sys.path.insert(0, str(Path(__file__).resolve().parent))
from filter_comments import filter_comments  # noqa: E402
from filter_examples import format_examples  # noqa: E402


class Node:
    """A small HTML tree for the preserved Developer Library's XHTML pages."""

    def __init__(self, tag="", attrs=()):
        self.tag = tag
        self.attrs = dict(attrs)
        self.children = []

    def text(self):
        return "".join(
            child.text() if isinstance(child, Node) else child
            for child in self.children
        )

    def nodes(self, tag=None, css=None):
        for child in self.children:
            if isinstance(child, Node):
                if (tag is None or child.tag == tag) and (
                    css is None or css in child.attrs.get("class", "").split()
                ):
                    yield child
                yield from child.nodes(tag, css)


class Tree(HTMLParser):
    """Parses XHTML while tolerating the mirror's optional closing tags."""

    def __init__(self, source):
        super().__init__(convert_charrefs=True)
        self.root = Node()
        self.stack = [self.root]
        self.feed(source)

    def handle_starttag(self, tag, attrs):
        node = Node(tag, attrs)
        self.stack[-1].children.append(node)
        if tag not in {"meta", "link", "img", "hr", "br", "input"}:
            self.stack.append(node)

    def handle_startendtag(self, tag, attrs):
        self.stack[-1].children.append(Node(tag, attrs))

    def handle_endtag(self, tag):
        for i in range(len(self.stack) - 1, 0, -1):
            if self.stack[i].tag == tag:
                del self.stack[i:]
                break

    def handle_data(self, data):
        self.stack[-1].children.append(data)


def text(element):
    """Normalizes XML text without losing nested type or reference names."""
    if element is None:
        return ""
    return clean("".join(element.itertext()))


def clean(value):
    return re.sub(r"\s+", " ", value).strip()


def signature(value):
    return re.sub(r"\s+", "", value.replace("IMPORT_C", ""))


def escape(value):
    """Escapes prose for a Doxygen comment, retaining paragraphs."""
    return (
        html_escape(value, quote=False)
        .replace('"', "&quot;")
        .replace("*/", "* / ")
        .replace("\\", "\\\\")
        .replace("@", "&#64;")
    )


def has_description(element):
    """Checks for descriptive prose, excluding parameters and API metadata."""

    def contents(node):
        if node.tag in {"parameterlist", "small"}:
            return ""
        if (
            node.tag == "simplesect"
            and text(node.find("title")) == "API status"
        ):
            return ""
        return (node.text or "") + "".join(
            contents(child) + (child.tail or "") for child in node
        )

    return any(
        clean(contents(description))
        for tag in ("briefdescription", "detaileddescription")
        if (description := element.find(tag)) is not None
    )


def prose(node):
    """Extracts paragraphs and formatted examples without duplicating notes."""
    result = []
    for child in node.children:
        if not isinstance(child, Node):
            continue
        if child.tag == "dl":
            continue
        if child.tag == "p":
            value = clean(child.text())
            if value and value not in {".", "..", "...", "-"}:
                result.append(value)
        elif child.tag == "pre":
            value = child.text().strip()
            if value:
                result.append("\\code{.cpp}\n" + value + "\n\\endcode")
        elif child.tag == "li":
            value = clean(child.text())
            if value:
                result.append("- " + value)
        else:
            result.extend(prose(child))
    return result


def reference(root, url):
    """Indexes overloads, constness, fields and enum-value contracts."""
    result = {}
    for member in root.nodes("div", "nested1"):
        title = next(member.nodes("h2", "topictitle2"), None)
        if title is None:
            continue
        name = clean(title.text())
        declaration = next(member.nodes("table", "signature"), None)
        if name.startswith("Enum "):
            name = name.removeprefix("Enum ")
        elif "(" not in name and declaration:
            # Data-member headings include their type; the signature separates
            # the field's identifier from its type and access annotation.
            cells = list(declaration.nodes("td"))
            if len(cells) >= 2:
                name = clean(cells[1].text())
        elif declaration:
            tail = declaration.text().rsplit(")", 1)[-1]
            if re.search(r"\bconst\b", tail):
                name += " const"
        description = []
        parameters = {}
        enumerators = {}
        notes = []
        for section in member.nodes("div", "section"):
            css = section.attrs.get("class", "").split()
            if "parameters" in css or "enumerators" in css:
                for row in section.nodes("tr"):
                    cells = list(row.nodes("td"))
                    if len(cells) != 2:
                        continue
                    label = clean(cells[0].text())
                    contract = clean(cells[1].text())
                    if "enumerators" in css:
                        enumerators[label.split("=", 1)[0].strip()] = contract
                    else:
                        names = re.findall(
                            r"[A-Za-z_][A-Za-z_0-9]*",
                            label.split("=", 1)[0],
                        )
                        if names:
                            parameters[names[-1]] = contract
            else:
                for value in prose(section):
                    if value not in description:
                        description.append(value)
                for item in section.nodes("dl"):
                    term = next(item.nodes("dt"), None)
                    definitions = list(item.nodes("dd"))
                    if term is None or not definitions:
                        continue
                    tag = clean(term.text()).lower().rstrip(":")
                    value = clean(" ".join(d.text() for d in definitions))
                    if not value:
                        continue
                    if tag in {"return", "returns", "leave", "panic", "since"}:
                        tag = "return" if tag == "returns" else tag
                        notes.append("\\" + tag + " " + value)
                    else:
                        notes.append(
                            "\\par " + clean(term.text()) + "\n" + value
                        )
        key = signature(name)
        item = {
            "description": description,
            "parameters": parameters,
            "enumerators": enumerators,
            "notes": notes,
            "url": url + "#" + member.attrs["id"],
        }
        # A duplicate declaration with no contract must not erase a documented
        # overload. Conflicting documented signatures need explicit review.
        old = result.get(key)
        if old and old != item:
            old_contract = old["description"] or old["parameters"]
            new_contract = description or parameters
            if old_contract and new_contract:
                raise ValueError(f"Ambiguous hosted overload: {url} {name}")
            if old_contract:
                continue
        result[key] = item
    return result


def command(member):
    """Builds an external Doxygen declaration using the indexed signature."""
    kind = member.get("kind")
    qualified = member.findtext("qualifiedname") or member.findtext("name")
    definition = text(member.find("definition"))
    args = text(member.find("argsstring"))
    if kind == "function":
        return "\\fn " + definition + args
    if kind == "enum":
        return "\\enum " + qualified
    if kind == "define":
        return "\\def " + qualified
    if kind == "typedef":
        return "\\typedef " + definition
    if kind == "variable":
        return "\\var " + definition
    if kind == "friend":
        return None
    raise ValueError(kind)


def generate(xml, cache, output):
    """Writes sourced supplements, leaving unresolved contracts visible."""
    urls = json.loads((cache / "fetch-input.json").read_text())
    files = defaultdict(list)
    files["types"] = []
    seen = set()
    documented = set()
    matched = missing = parameters = enum_values = 0
    for index in ET.parse(xml / "index.xml").findall("compound"):
        if index.get("kind") not in {
            "class",
            "struct",
            "union",
            "namespace",
            "file",
        }:
            continue
        path = xml / (index.get("refid") + ".xml")
        compound = ET.fromstring(
            path.read_text(encoding="utf-8", errors="replace")
        ).find("compounddef")
        owner = compound.findtext("compoundname")
        if index.get("kind") == "file":
            files[Path(owner).name]
        url = urls.get(owner)
        hosted = {}
        root = None
        if url:
            page = cache / url.rsplit("/", 1)[1]
            if not page.is_file():
                raise ValueError(f"Missing consulted reference page: {url}")
            root = Tree(page.read_text(encoding="utf-8", errors="replace")).root
            hosted = reference(root, url)
        if index.get("kind") in {
            "class",
            "struct",
            "union",
        } and not has_description(compound):
            paragraphs = []
            if root:
                top = next(root.nodes("div", "nested0"), None)
                section = (
                    next(top.nodes("div", "section"), None) if top else None
                )
                if section:
                    paragraphs = [clean(p.text()) for p in section.nodes("p")]
            desc = "\n\n".join(paragraphs)
            suffix = (
                f'(generated from <a href="{url}">'
                "Symbian Developer Library</a>)"
                if paragraphs
                else "(generated)"
            )
            if desc:
                files["types"].append(
                    f"/** \\{index.get('kind')} {owner}\n{escape(desc)}\n\n"
                    f"<p><small>{suffix}</small></p>\n*/\n"
                )
        for member in compound.findall("./sectiondef/memberdef"):
            if member.get("prot") == "private":
                continue
            if member.get("id") in seen:
                continue
            seen.add(member.get("id"))
            directive = command(member)
            if directive is None:
                continue
            params = member.findall("param")
            old_params = {text(n) for n in member.findall(".//parametername")}
            undocumented_params = [
                p
                for p in params
                if text(p.find("declname"))
                and text(p.find("declname")) not in old_params
            ]
            needs_description = not has_description(member)
            name = member.findtext("name")
            lookup = name
            if member.get("kind") == "function":
                lookup += (
                    "(" + ",".join(text(p.find("type")) for p in params) + ")"
                )
                if member.get("const") == "yes":
                    lookup += " const"
            item = hosted.get(signature(lookup))
            description = item["description"] if item else []
            body = []
            if not needs_description and not undocumented_params:
                if member.get("kind") != "enum":
                    continue
            if needs_description:
                if description:
                    body.append(
                        "\n\n".join(
                            escape(p) if not p.startswith("\\code") else p
                            for p in description
                        )
                    )
                    matched += 1
                    documented.add(member.get("id"))
                else:
                    missing += 1
            for param in undocumented_params:
                param_name = text(param.find("declname"))
                contract = item["parameters"].get(param_name) if item else None
                if (
                    not contract
                    and item
                    and len(item["parameters"]) == len(params)
                ):
                    # The exact type signature already matched. Some releases
                    # rename an argument without changing its position or ABI.
                    position = params.index(param)
                    contract = list(item["parameters"].values())[position]
                if not contract:
                    continue
                body.append(f"\\param {param_name} {escape(contract)}")
                parameters += 1
            if item:
                for note in item["notes"] if needs_description else ():
                    # Keep structural Doxygen commands, escaping their prose.
                    tag, _, content = note.partition(" ")
                    body.append(tag + " " + escape(content))
                for value in member.findall("enumvalue"):
                    if has_description(value):
                        continue
                    value_name = value.findtext("name")
                    contract = item["enumerators"].get(value_name)
                    if contract:
                        body.append(f"- `{value_name}`: {escape(contract)}")
                        enum_values += 1
            if not body:
                continue
            suffix = (
                f'(generated from <a href="{item["url"]}">'
                "Symbian Developer Library</a>)"
            )
            body.append("\n<p><small>" + suffix + "</small></p>")
            location = member.find("location")
            header = (
                Path(location.get("file")).name
                if location is not None
                else "types"
            )
            files[header].append(
                "/** " + directive + "\n" + "\n\n".join(body) + "\n*/\n"
            )
    output.mkdir(parents=True, exist_ok=True)
    for name, comments in files.items():
        destination = output / (name + ".dox")
        if not comments:
            destination.unlink(missing_ok=True)
            continue
        destination.write_text(
            format_examples(
                "// Documentation adapted from the Symbian Developer Library.\n"
                "// Original reference content is licensed under EPL-1.0.\n"
                "// SPDX-License-Identifier: EPL-1.0\n\n" + "\n".join(comments)
            )
        )
    print(
        f"Hosted descriptions: {matched}; unresolved descriptions: "
        f"{missing}; parameter supplements: {parameters}; "
        f"enum-value supplements: {enum_values}"
    )
    return documented


def implementation_key(declaration):
    """Identifies a qualified definition by argument types and constness."""
    match = re.search(
        r"([A-Za-z_][\w:]*::[~A-Za-z_][\w]*)\s*\(([^()]*)\)\s*(const)?",
        declaration,
    )
    if not match:
        return None
    arguments = []
    names = []
    for argument in match[2].split(","):
        argument = argument.split("=", 1)[0].strip()
        if not argument or argument == "void":
            continue
        name = re.search(r"\b([A-Za-z_]\w*)\s*$", argument)
        if name is None:
            return None
        arguments.append(signature(argument[: name.start()]))
        names.append(name[1])
    return (match[1], tuple(arguments), bool(match[3])), names


def generate_implementation(xml, source, url, output, documented=()):
    """Copies documented definitions after exact indexed type matching.

    Handles definitions whose documentation follows their signature, as in
    in_addr.cpp. Other source layouts need a separate supported parser.
    """
    indexed = {}
    for entry in ET.parse(xml / "index.xml").findall("compound"):
        if entry.get("kind") not in {"class", "struct", "namespace", "file"}:
            continue
        compound = ET.fromstring(
            (xml / (entry.get("refid") + ".xml")).read_text(errors="replace")
        )
        for member in compound.findall(".//memberdef"):
            if (
                member.get("kind") != "function"
                or member.get("prot") == "private"
            ):
                continue
            declaration = text(member.find("definition")) + text(
                member.find("argsstring")
            )
            item = implementation_key(declaration)
            if item:
                indexed[item[0]] = (member, item[1])
    original = source.read_text(errors="replace")
    notice = re.match(r"\A(?:[ \t]*//[^\n]*\n|[ \t]*\n)+", original)
    if notice is None or not any(
        license in notice[0]
        for license in (
            "Eclipse Public License",
            "SPDX-License-Identifier: EPL-1.0",
        )
    ):
        raise ValueError("Implementation needs its original EPL notice")
    pattern = re.compile(
        r"\bEXPORT_C\s+(?P<declaration>[^;{}]*?)\s*"
        r"/\*\*(?P<comment>.*?)\*/\s*\{",
        re.DOTALL,
    )
    comments = []
    for match in pattern.finditer(original):
        parsed = implementation_key(match["declaration"])
        if not parsed or parsed[0] not in indexed:
            continue
        member, declared_names = indexed[parsed[0]]
        if has_description(member) or member.get("id") in documented:
            continue
        body = re.sub(r"^\s*\* ?", "", match["comment"], flags=re.MULTILINE)
        replacements = dict(zip(parsed[1], declared_names, strict=True))
        body = re.sub(
            r"\b[A-Za-z_]\w*\b",
            lambda word, mapping=replacements: mapping.get(word[0], word[0]),
            body,
        )
        # Some original void methods describe an output argument as a return
        # value. Retain its contract under the actual parameter instead.
        if signature(text(member.find("type"))) in {"void", "IMPORT_Cvoid"}:
            body = re.sub(
                r"([@\\])retval\s+(\w+)\b",
                lambda tag, names=declared_names: (
                    tag[1] + "param[out] " + tag[2]
                    if tag[2] in names
                    else tag[0]
                ),
                body,
            )
        body = filter_comments("/**\n" + body + "\n*/")[3:-2].strip()
        line = original.count("\n", 0, match.start()) + 1
        suffix = (
            f'<p><small>(generated from <a href="{url}#L{line}">'
            "Original Symbian implementation</a>)</small></p>"
        )
        comments.append(
            "/** " + command(member) + "\n" + body + "\n\n" + suffix + "\n*/\n"
        )
    destination = output / (source.name + ".dox")
    output.mkdir(parents=True, exist_ok=True)
    if comments:
        destination.write_text(
            format_examples(
                notice[0]
                + "// Documentation adapted from the implementation below.\n"
                + "// SPDX-License-Identifier: EPL-1.0\n\n"
                + "\n".join(comments)
            )
        )
    else:
        destination.unlink(missing_ok=True)
    print(f"Implementation-backed descriptions: {len(comments)}")


def main():
    """Refreshes supplements using explicit XML and local reference inputs."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--xml", required=True, type=Path)
    parser.add_argument("--cache", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--implementation", type=Path)
    parser.add_argument("--implementation-url")
    args = parser.parse_args()
    if bool(args.implementation) != bool(args.implementation_url):
        parser.error("--implementation and --implementation-url must be paired")
    documented = generate(args.xml, args.cache, args.output)
    if args.implementation:
        generate_implementation(
            args.xml,
            args.implementation,
            args.implementation_url,
            args.output,
            documented,
        )


if __name__ == "__main__":
    main()
