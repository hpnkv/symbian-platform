# Documentation development

`doc/mkdocs.yml` builds the developer site with the same Material palette,
Noto Sans and JetBrains Mono fonts, navigation features and code highlighting
settings as A11. `doc/cpp/Doxyfile` builds the native reference with the same
Doxygen Awesome theme and declaration highlighting. The copied theme/style
files retain their upstream licenses under `doc/cpp/`.

Install the `docs` dependency group, Doxygen and Graphviz, then run:

```sh
uv sync --only-group docs --no-install-project
uv run --no-sync --only-group docs ./doc/build.sh --strict
```

The complete site is written to ignored `doc/site/`. The build requires both
MkDocs and Doxygen; the GitHub Action uses the same script. Source articles
live under `doc/docs/`, while project plans and experiment logs live under
`.dev/`. All article filenames are lowercase.
