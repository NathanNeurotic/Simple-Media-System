# Rolling publication identity and ordering

PR #36 starts from master `83c94b8`. The player release ZIP already records that
commit correctly, but the existing rolling tag was stale: `gh release edit
--target` does not advance an existing Git tag. The publication step explicitly
updates the lightweight tag after upload, uses `force=false`, and verifies its
readback. Stable version tags and converter asset names are unchanged.

## Ordering follow-up

A head check at the start of a step does not serialize its later writes. An
offline control executed two copies of the actual player publication Bash body
against a local GitHub CLI adapter:

1. Current build A passed the master check, then paused just before upload.
2. Master advanced to B; B uploaded its assets, advanced the tag and finished.
3. A resumed, overwrote the assets, then failed its fast-forward-only tag update.

The final adapter state was assets A / tag B, even though A's job failed. No
live release was modified by this control. Tag rewind protection alone cannot
protect asset writes that happen before the tag update.

The main player build now has one `sms-player-rolling` concurrency group for
master pushes, with `cancel-in-progress: false` and `queue: max`. Other events
use run/attempt-specific groups. GitHub's [concurrency contract](https://docs.github.com/en/actions/how-tos/write-workflows/choose-when-workflows-run/control-workflow-concurrency)
serializes the writers and retains queued jobs. Keeping the queue matters:
with the default single pending entry, an old rerun could replace a pending
current-master build, then skip itself and leave no latest publisher queued.
The master check still skips superseded work once it reaches publication.

Converter Windows, Linux and Windows 7 jobs use separate asset-family groups.
Each upload retry verifies current master. Superseded jobs exit before upload;
API errors or invalid identities fail explicitly. Waiting for player-owned
release creation and retrying uploads is bounded: exhausting 12 attempts fails
rather than silently reporting a successful publication. Converter steps never
edit the player's release notes or tag, and the stable publishing blocks are
unchanged. Independent platforms can still publish in parallel.

## Validation and limits

- `tools/test_rolling_release.py` executes the actual player shell body with a
  local CLI adapter: 11 cases cover existing/first/current tags, stale jobs,
  invalid/API identities, failed upload/tag update and readback errors.
- `tools/test_converter_publication.py` executes all three actual converter
  Bash bodies: 33 cases cover matching/stale heads, failed or invalid API
  reads, release-creation delays/exhaustion, failed/retried uploads, superseded
  waits and preservation of other asset families, notes and tags. These run in
  both modern and Windows 7 converter CI jobs, including Git Bash on Windows.
- Actionlint 1.7.12 validates both workflow files after excluding only its
  `unexpected key "queue" for "concurrency" section` schema diagnostic. Its
  current schema predates the documented `queue` key; no repository-wide ignore
  or workflow validation bypass was added. Native GitHub Actions parsing and
  execution remain the authority for that property.

The offline adapters test shell behavior and declarations; they do not emulate
GitHub's scheduler or publish a real release. In-place multi-asset upload plus a
tag PATCH is not a transaction: partial upload/API failure is an explicit
failed-publish condition. The groups prevent updated writers of the same asset
family from overlapping; they do not promise that every platform has the same
commit during a rolling update or after a platform build fails. Actual rolling
publication remains pending merge through the existing master workflow.
