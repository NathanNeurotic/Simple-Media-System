# SMS documentation sources

Edit the article bodies in `content/`, page metadata/navigation in `manifest.json`,
and shared files in `static/`. Documentation tooling requires **Python 3.9 or
newer** and uses only the standard library;
there is no hosted search, CDN, package install or paid service.

```sh
python docs-site/build.py
python docs-site/build.py --check
python -m http.server 8000 --directory docs
```

Open <http://localhost:8000/>. `docs/` is the generated, committed Pages site and
`wiki/` is the generated Markdown mirror. Search has JSON and local-JavaScript
forms, so it also works when the generated site is opened offline. Use HTTP for
normal testing and inspect browser errors, mobile navigation, tables, keyboard
search, reduced-motion defaults and deep links.

Do not edit generated top-level HTML, `docs/assets/`, `docs/data/` or `wiki/*.md`
directly. Regenerate after editing. `--check` compares the expected bytes without
writing and checks local targets, fragment IDs, search targets and icon classes.
LF attributes keep generated output consistent on Windows/Linux.

## Pages publication

GitHub Pages already publishes the `master` branch's `/docs` folder at
<https://nathanneurotic.github.io/Simple-Media-System/>. Keep that configuration;
the generator does not change hosting, app code, SDKs or repository permissions.
The documentation workflow checks regeneration on pushes/PRs and uploads an
offline site artifact. Merge the reviewed change, then check the actual Pages
deployment and live routes. CI/build success alone is not live publication.

The old `node tools/build-docs-search-index.mjs` command is retained as a wrapper
around this generator. It updates the complete site/mirror, not a separate index.

## Native GitHub wiki

The native wiki has its own Git history at
`git@github.com:NathanNeurotic/Simple-Media-System.wiki.git`. Its initial Home page
must exist on GitHub before that Git repository can be cloned. Once initialized:

```sh
git clone git@github.com:NathanNeurotic/Simple-Media-System.wiki.git ../Simple-Media-System.wiki
python docs-site/sync_wiki.py --checkout ../Simple-Media-System.wiki
python docs-site/sync_wiki.py --checkout ../Simple-Media-System.wiki --apply
git -C ../Simple-Media-System.wiki diff --check
git -C ../Simple-Media-System.wiki diff
git -C ../Simple-Media-System.wiki add -- '*.md'
git -C ../Simple-Media-System.wiki commit -m "Update the SMS manual mirror"
git -C ../Simple-Media-System.wiki push origin HEAD
```

The first sync command is a dry run. The script requires the expected remote and
a clean checkout, only copies the known generated names and never deletes other
wiki pages. Review changes before commit/push. A concurrently edited wiki can
reject a normal push; fetch and reconcile rather than force-pushing. Mirror links
point to the canonical site's stable routes/anchors so both formats share the
same content and source-edit links.

## Accuracy and historical material

The manifest's `audited_source`/`reviewed` identify the code baseline for factual
review; they are not dynamically rewritten to the docs commit. Update them after
a new source audit. Keep rolling versus stable, parser acceptance versus playback
speed, host/target/emulator checks versus physical acceptance, and previous
reported hardware results distinct.

Use the [audit record](../docs/investigations/2026-10-27-documentation-audit.md)
for page coverage and owning source pointers. Date known-issue summaries and
link to live issues. Avoid hard-coded rolling asset hashes: use the downloaded
bundle's manifest. Check current source before claiming a device limit or saved
config path.

`docs/src/`, `docs/pitrz/`, historical images/downloads and the original README and
changelog tails are preserved artifacts. The DocBook XML is not the source of
the current site; retain historical context when linking old encoding advice.

## Theme provenance

The base CSS comes from NathanNeurotic/Launchstation's shared PS2 documentation
theme, also used by the RiptOPL/POPSLoader family. `sms.css` supplies SMS layout,
contrast, responsive and reduced-motion adjustments. The jellyfish wallpaper and
SMS icon are existing project artwork. Cursor-orb code is local and optional;
storage is limited to its on/off preference, with no telemetry.
