"""Remove redundant parentheses around dereferences in SDK-owned C++ files."""

from __future__ import annotations

import argparse
from pathlib import Path

import tree_sitter_cpp
from check_cpp_style import ANNOTATION, CPP_SUFFIXES, descendants, owned_paths
from tree_sitter import Language, Parser


def redundant_dereferences(source: bytes) -> list[tuple[int, int]]:
    """Return redundant parenthesis byte ranges, preserving postfix operands."""
    masked = ANNOTATION.sub(lambda match: b" " * len(match[0]), source)
    tree = Parser(Language(tree_sitter_cpp.language())).parse(masked)
    ranges = []
    for node in descendants(tree.root_node):
        if node.type != "parenthesized_expression":
            continue
        children = node.named_children
        if len(children) != 1 or children[0].type != "pointer_expression":
            continue
        child = children[0]
        if masked[child.start_byte : child.start_byte + 1] != b"*":
            continue
        parent = node.parent
        # Postfix operators bind more tightly than unary *. These parentheses
        # are required: (*p)[i], (*p).member, (*p)(), (*p)++, (*p)->member.
        if parent.type in {
            "subscript_expression",
            "field_expression",
            "call_expression",
            "update_expression",
        }:
            continue
        # Use a conservative allowlist for expressions/statements where unary
        # * already binds tightly enough; leave unfamiliar grammar untouched.
        if parent.type in {
            "assignment_expression",
            "binary_expression",
            "return_statement",
            "expression_statement",
            "argument_list",
            "initializer_list",
            "init_declarator",
            "conditional_expression",
            "parenthesized_expression",
            "pointer_expression",
            "unary_expression",
            "cast_expression",
        }:
            ranges.append((node.start_byte, node.end_byte))
    return ranges


def simplify(source: bytes) -> bytes:
    """Remove only syntactically redundant dereference parentheses."""
    removals = {
        position
        for start, end in redundant_dereferences(source)
        for position in (start, end - 1)
    }
    return bytes(byte for i, byte in enumerate(source) if i not in removals)


def main() -> int:
    """Rewrite sources or report files needing simplification with --check."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    parser.add_argument("paths", nargs="*", type=Path)
    args = parser.parse_args()
    count = 0
    for path in args.paths or owned_paths():
        if path.suffix not in CPP_SUFFIXES:
            continue
        source = path.read_bytes()
        result = simplify(source)
        if source != result:
            count += 1
            print(path)
            if not args.check:
                path.write_bytes(result)
    return int(args.check and count != 0)


if __name__ == "__main__":
    raise SystemExit(main())
