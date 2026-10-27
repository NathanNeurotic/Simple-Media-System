# Previewing and building SMS documentation

## Maintained manual and wiki

From the repository root:

```sh
python docs-site/build.py
python docs-site/build.py --check
python -m http.server 8000 --directory docs
```

Open <http://localhost:8000/>. No Python packages, Jekyll build, external fonts or
hosted search service are required. The generated local search script also works
offline. HTTP preview remains useful for checking live-equivalent routes and
browser errors. `--check` rejects stale output and invalid local links/anchors.

The old Node search-index command delegates to the same Python generator. See
[`docs-site/README.md`](../docs-site/README.md) for source editing, publication,
native wiki sync and preservation of historical material.

## Archived DocBook manual

`docs/src/`, `docs/Makefile` and `docs/configure` belong to the historic upstream
DocBook documentation system. They are preserved for provenance and are not used to
produce the current site. Its former generated `docs/HTML/` and `docs/HTML-single/`
directories are not committed here.

If you intentionally need to rebuild that archive, its legacy makefiles expect GNU
make, `xmllint`, `xsltproc`, the DocBook XML DTD and DocBook XSL stylesheets. Run
`make help` inside `docs/` for its old targets. Changes there must not be presented
as current-site updates unless the top-level static HTML is updated separately.
