# SMS documentation

The current manual is <https://nathanneurotic.github.io/Simple-Media-System/>.
GitHub Pages publishes `master:/docs`.

This directory's top-level HTML, assets and search data are generated from
[`docs-site/`](../docs-site/README.md). Edit `docs-site/content/` and the shared
manifest/assets, then run:

```sh
python docs-site/build.py
python docs-site/build.py --check
```

The same source generates the versioned [`wiki/`](../wiki) Markdown mirror used
by the native GitHub wiki. [Maintainer workflow](../docs-site/README.md).

`src/`, `Makefile`, `configure`, `stylesheets/`, `images/`, `downloads/` and
`pitrz/` retain historical manual material or supporting assets. DocBook XML is
not the source of the maintained top-level site. The old generated multilingual
landing-page directories are absent; use `legacy-manual.html` for the archive map.
