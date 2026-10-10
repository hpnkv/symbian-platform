# Documentation development

`doc/mkdocs.yml` builds the developer site with the same Material palette,
Noto Sans and JetBrains Mono fonts, navigation features and code highlighting
settings as A11. `doc/cpp/Doxyfile` builds the SDK-owned native reference;
`doc/cpp/platform/Doxyfile` builds a separate, curated reference for eleven
original Symbian headers. Both use the Doxygen Awesome theme. The original
header notices and copied theme/style licenses remain with their files.

Install the `docs` dependency group, Doxygen, Graphviz and clang-format 15+,
then run:

```sh
uv sync --only-group docs --no-install-project
uv run --no-sync --only-group docs ./doc/build.sh --strict
```

The complete site is written to ignored `doc/site/`. The build requires both
MkDocs and Doxygen; the GitHub Action uses the same script. Source articles
live under `doc/docs/`. The owner's plan and experiment archive is backed up
outside the repository at `~/.symbian-dev/`. All article filenames are lowercase.

The Console screenshots under `doc/docs/assets/screenshots/` are rendered from
the current frontend with sample paths and no connected phone. On macOS with
headless Chrome, refresh them with `uv run python doc/capture_console.py`.
The `clion-*.png` screenshots are cropped captures of the prepared
`examples/gui_app` project in IntelliJ IDEA with the CLion plugin on macOS.
They show CMake, Run and debugger configuration, not guest execution.
Refreshing them requires Screen Recording permission; keep other windows,
desktop notifications and private paths outside the final crops. The older
`clion-workflow.svg` remains a separate configuration illustration.

C++ Markdown examples use the root `.clang-format`, including braces for all
conditional and loop bodies. Format new or edited examples with
`python3 doc/format_cpp.py`; `doc/build.sh` checks them before building the site.
Use `--clang-format /path/to/clang-format` when it is not on `PATH`; the
Doxygen build also needs it on `PATH`. Doxygen comment examples are formatted
at build time by `doc/cpp/filter_examples.py`, preserving source files.

Original-header publication and API-status tags are placed after functional
documentation by `doc/cpp/platform/filter_comments.py`; the licensed snapshots
and source-browser views retain their original text.
