"""Regression checks for the owned C++ migration guard and simplifier."""

import importlib.util
import sys
from pathlib import Path

import pytest

SCRIPTS = Path(__file__).resolve().parents[1]


def _module(name):
    """Load a repository script without changing the process search path."""
    spec = importlib.util.spec_from_file_location(name, SCRIPTS / f"{name}.py")
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


style = _module("check_cpp_style")
simplifier = _module("simplify_cpp_dereferences")


@pytest.mark.parametrize(
    "source",
    [
        b"void Read(Buffer& out);",
        b"void Read(Buffer& out) {}",
        b"auto f = [](Buffer& out) {};",
        b"void operator()(Buffer& out);",
    ],
)
def test_mutable_parameters_are_rejected(source):
    assert style.check_source(source, pointers=False)


@pytest.mark.parametrize(
    "source",
    [
        b"void Read(Buffer* absl_nonnull out);",
        b"void Read(const Buffer& input);",
        b"void Read(Buffer&& input);",
        b"void Read() { try {} catch (Error& error) {} }",
        b"Buffer& operator=(const Buffer& other);",
        b"Buffer& operator++(Buffer& other);",
    ],
)
def test_pointer_const_forwarding_and_required_operator_contracts(source):
    assert not style.check_source(source)


def test_external_override_preserves_mutable_reference_signature():
    source = b"class Adapter { void Ready(NativeBuffer& buffer) override; };"
    assert not style.check_source(source, pointers=False)


@pytest.mark.parametrize(
    "source",
    [
        b"void Read(Buffer* out);",
        b"Buffer* result;",
        b"using Callback = void (*)(Buffer*);",
        b"Buffer** outputs;",
    ],
)
def test_unannotated_pointer_declarations_are_rejected(source):
    assert style.check_source(source)


@pytest.mark.parametrize(
    "source",
    [
        b"Buffer* absl_nullable out;",
        b"Buffer* absl_nullable* absl_nonnull outputs;",
        b"using Callback = void (* absl_nonnull)(Buffer* absl_nonnull);",
        b"void Run() { auto out = static_cast<Buffer*>(value); }",
    ],
)
def test_annotated_pointers_and_pointer_casts_are_accepted(source):
    assert not style.check_source(source)


def test_simplifies_dereferences_without_changing_postfix_precedence():
    source = b"""
void Run() {
  (*out) = std::move(value);
  foo((*ptr));
  auto sum = (*ptr) + 3;
  (*ptr)[idx] = 3;
  (*ptr).field = 4;
  (*callback)();
  (*ptr)++;
  (*ptr)->field = 5;
  return (*ptr);
}
"""
    expected = source.replace(b"(*out) =", b"*out =")
    expected = expected.replace(b"foo((*ptr))", b"foo(*ptr)")
    expected = expected.replace(b"(*ptr) +", b"*ptr +")
    expected = expected.replace(b"return (*ptr)", b"return *ptr")
    assert simplifier.simplify(source) == expected
    assert simplifier.simplify(expected) == expected


def test_fenced_cpp_is_checked_but_other_languages_are_ignored(tmp_path):
    path = tmp_path / "example.md"
    path.write_text("```cpp\nvoid Run(T& out);\n```\n```text\nT& out\n```\n")
    original = style.ROOT
    try:
        style.ROOT = tmp_path
        assert style.check_path(path) == [
            "example.md:2: mutable parameter needs * absl_nonnull"
        ]
    finally:
        style.ROOT = original
