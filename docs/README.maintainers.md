# Maintaining the SMS documentation

The source of truth is [`docs-site/`](../docs-site/README.md): article bodies,
shared navigation/metadata, theme assets and a stdlib Python generator. Do not
edit generated top-level HTML/search data or `wiki/*.md` directly.

1. Verify the owning source and record the evidence level for a factual change.
2. Edit the relevant article, README/changelog if needed, and metadata/navigation.
3. Run `python docs-site/build.py`, then `python docs-site/build.py --check`.
4. Preview with `python -m http.server 8000 --directory docs`. Test search,
   keyboard/mobile navigation, deep links, table scrolling and motion controls.
5. Review the generated site/wiki diff and run `git diff --check`.
6. After merge, verify the Pages deployment and live content. Sync the native
   wiki through its separate clean Git checkout as described in the source README.

The documentation CI checks regeneration, local links/fragments, search targets
and JavaScript syntax. It publishes an offline artifact; it does not test PS2
playback or automatically force-push wiki history.

The [2026-10-27 audit](investigations/2026-10-27-documentation-audit.md) maps all
pages to source/evidence. Update the audit baseline/date after another review,
not on every rebuild. Preserve historical manuals and investigation snapshots.
