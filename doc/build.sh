#!/usr/bin/env bash
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$here"

if ! command -v mkdocs >/dev/null 2>&1; then
  echo "mkdocs is required; install the docs dependency group" >&2
  exit 1
fi
if ! command -v doxygen >/dev/null 2>&1; then
  echo "doxygen is required for the C++ reference" >&2
  exit 1
fi
if ! command -v dot >/dev/null 2>&1; then
  echo "graphviz dot is required for the C++ reference" >&2
  exit 1
fi

python3 check_links.py
mkdocs build "$@"
log="$(mktemp)"
trap 'rm -f "$log"' EXIT
(cd cpp && doxygen Doxyfile) >"$log" 2>&1 || {
  cat "$log" >&2
  exit 1
}
cat "$log"
if grep -E '(^|[[:space:]])error:' "$log" >/dev/null; then
  echo "Doxygen reported errors" >&2
  exit 1
fi
test -f site/cpp/index.html
(cd cpp/platform && doxygen Doxyfile) >"$log" 2>&1 || {
  cat "$log" >&2
  exit 1
}
cat "$log"
if grep -E '(^|[[:space:]])error:' "$log" >/dev/null; then
  echo "Platform Doxygen reported errors" >&2
  exit 1
fi
test -f site/cpp/platform/index.html
echo "Documentation built at $here/site"
