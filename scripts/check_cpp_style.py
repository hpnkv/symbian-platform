"""Check SDK-owned C++ parameter and pointer declarations, including snippets.

Run with the development dependencies installed. Original platform headers,
vendored code, and C library compatibility declarations are external contracts.
"""

from __future__ import annotations

import argparse
import ast
import re
import subprocess
from pathlib import Path

import tree_sitter_cpp
from tree_sitter import Language, Parser

ROOT = Path(__file__).resolve().parents[1]
ANNOTATION = re.compile(rb"\babsl_(?:nonnull|nullable|nullability_unknown)\b")
EXTERNAL = (
    "third_party/",
    "research/",
    "doc/cpp/",
    "cpp/symbian/concurrency/upstream/",
    # Verbatim EPL Symbian tactile feedback ABI headers.
    "cpp/symbian/api/media/compat/original/",
)
# These implement standard C/libc++ signatures and must remain usable without
# Abseil (in particular while bootstrapping the guest runtime).
C_HEADERS = {
    # Documentation-generator fixtures reproduce original OS signatures.
    "scripts/tests/test_original_docs.py",
    "cpp/symbian/runtime/include/inttypes.h",
    "cpp/symbian/runtime/include/math.h",
    "cpp/symbian/runtime/include/__locale_dir/locale_base_api.h",
}
# Locally authored port glue remains owned even inside a vendored tree.
OWNED_VENDOR = (
    "third_party/mbedtls-symbian/include/symbian_mbedtls/",
    "third_party/mbedtls-symbian/include/symbian_tls/",
    "third_party/mbedtls-symbian/examples/crypto_probe/",
    "third_party/mbedtls-symbian/tests/",
    "third_party/mbedtls-symbian/library/sdk_heap.c",
    "third_party/mbedtls-symbian/library/sdk_time.c",
    "third_party/mbedtls-symbian/library/sdk_socket_bio.c",
    "third_party/mbedtls-symbian/library/platform_zeroize.c",
    "third_party/mbedtls-symbian/compat/openssl/sdk_heap.c",
    "third_party/mbedtls-symbian/compat/openssl/rand_poll.c",
)
CPP_SUFFIXES = {".h", ".hpp", ".cc", ".cpp", ".cppm", ".c"}


def owned_paths() -> list[Path]:
    """Return tracked SDK sources and documentation, excluding upstream code."""
    paths = subprocess.check_output(
        ["git", "ls-files", "--cached", "--others", "--exclude-standard"],
        cwd=ROOT,
        text=True,
    ).splitlines()
    return [
        ROOT / path
        for path in paths
        if (ROOT / path).is_file()
        and (not path.startswith(EXTERNAL) or path.startswith(OWNED_VENDOR))
        and path not in C_HEADERS
        and (Path(path).suffix in CPP_SUFFIXES | {".md", ".py"})
        and not path.startswith(".dev/")
    ]


def descendants(node):
    """Yield syntax nodes in source order."""
    yield node
    for child in node.named_children:
        yield from descendants(child)


def check_source(
    source: bytes, *, pointers: bool = True
) -> list[tuple[int, str]]:
    """Find mutable lvalue parameters and unannotated pointer declarations."""
    # Clang-style annotations are macro tokens, not part of C++ grammar.
    masked = ANNOTATION.sub(lambda match: b" " * len(match[0]), source)
    parser = Parser(Language(tree_sitter_cpp.language()))
    tree = parser.parse(masked)
    findings = []
    for node in descendants(tree.root_node):
        if node.type in {
            "parameter_declaration",
            "optional_parameter_declaration",
        }:
            declarator = node.child_by_field_name("declarator")
            if declarator is None or declarator.type != "reference_declarator":
                continue
            text = masked[declarator.start_byte : declarator.end_byte]
            prefix = masked[node.start_byte : declarator.start_byte]
            if text.lstrip().startswith(b"&&") or re.search(
                rb"\bconst\b", prefix
            ):
                continue
            owner = node.parent.parent
            if owner.type == "catch_clause":
                continue
            # External virtual callbacks must retain their exact override ABI.
            if b"override" in masked[owner.start_byte : owner.end_byte]:
                continue
            # C++ operators have language/standard-library operand contracts.
            name = owner.child_by_field_name("declarator")
            if name and any(
                n.type == "operator_name"
                and not re.fullmatch(
                    rb"operator\s*\(\s*\)",
                    masked[n.start_byte : n.end_byte],
                )
                for n in descendants(name)
            ):
                continue
            findings.append(
                (
                    node.start_point.row + 1,
                    "mutable parameter needs * absl_nonnull",
                )
            )
        if pointers and node.type in {
            "pointer_declarator",
            "abstract_pointer_declarator",
        }:
            parent = node.parent
            while parent and parent.type not in {
                "declaration",
                "field_declaration",
                "parameter_declaration",
                "optional_parameter_declaration",
                "type_definition",
                "alias_declaration",
                "cast_expression",
                "new_expression",
                "call_expression",
                "init_declarator",
                "function_definition",
            }:
                parent = parent.parent
            macro = node.parent
            while macro and macro.type != "function_declarator":
                macro = macro.parent
            if macro:
                name = macro.child_by_field_name("declarator")
                if name and masked[name.start_byte : name.end_byte].startswith(
                    b"ABSL_"
                ):
                    continue
            if parent is None or parent.type in {
                "cast_expression",
                "new_expression",
                "call_expression",
            }:
                continue
            if parent.type == "init_declarator":
                value = parent.child_by_field_name("value")
                if value and node.start_byte >= value.start_byte:
                    continue
            star = next((n for n in node.children if n.type == "*"), None)
            if star is None:
                continue
            after = source[star.end_byte : star.end_byte + 80]
            if not re.match(
                rb"\s*absl_(?:nonnull|nullable|nullability_unknown)\b", after
            ):
                findings.append(
                    (
                        star.start_point.row + 1,
                        "pointer needs a nullability annotation",
                    )
                )
    return sorted(set(findings))


def check_path(path: Path) -> list[str]:
    """Check C++ files and fenced documentation snippets."""
    source = path.read_bytes()
    units = [(0, source)] if path.suffix in CPP_SUFFIXES else []
    if path.suffix == ".md":
        for match in re.finditer(
            rb"```(?:cpp|c\+\+|cxx)\s*\n(.*?)```", source, re.S
        ):
            units.append((source[: match.start(1)].count(b"\n"), match[1]))
    if path.suffix == ".py":
        for node in ast.walk(ast.parse(source)):
            if isinstance(node, ast.Constant) and isinstance(node.value, str):
                if "#include" in node.value:
                    units.append((node.lineno - 1, node.value.encode()))
    return [
        f"{path.relative_to(ROOT)}:{offset + line}: {message}"
        for offset, unit in units
        for line, message in check_source(unit)
    ]


def main() -> int:
    """Check the repository or an explicit list of files."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("paths", nargs="*", type=Path)
    args = parser.parse_args()
    findings = [
        finding
        for path in (args.paths or owned_paths())
        for finding in check_path(path.resolve())
    ]
    for finding in findings:
        print(finding)
    print(f"C++ style: {len(findings)} violations")
    return bool(findings)


if __name__ == "__main__":
    raise SystemExit(main())
