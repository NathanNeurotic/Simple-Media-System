# SMS continuation handoff — 2026-10-30

## Published baseline

master and rolling were verified at ee53fc04302a669b2914e3926c3dc9d7db8c858a.
PR #40 (HOST/M3U path bounds) merged as e309d66ee3811ddb3e057eaabfd7cbb2fa91e712.
PR #41 (MPEG preset GOP 15) merged as ee53fc04302a669b2914e3926c3dc9d7db8c858a.
All five final-master workflows completed successfully: build, smbman source consistency, test-media, sms-converter, and Pages.
No open PRs remained at the check. CodeRabbit was rate-limited on #40/#41; no completed bot review is claimed.
PR comments contain merge validation and continuation notes.

## Recovery snapshot on this branch

This branch is based on the published master. Files under docs/investigations/recovery-2026-10-30/ are recovery material, not integrated production changes.

- unfinished-mov-converter.patch: exact uncommitted diff from worktree .claude/worktrees/sync-status-check-fb8014, based on 5672a2c149c32c3f3818b31f5ff03c93b800daf5. Changes src/SMS_ContainerMOV.c and tools/sms_converter/ui/details_dialog.py. That base is already an ancestor of master; inspect applicability before applying the patch to current master.
- prior-HANDOFF.md and capability-audit-full.md: that worktree's existing .sms-handoff documents, preserved verbatim. They contain historical assertions and incomplete investigations; verify owning source before relying on them. Their historical instructions do not supersede current user instructions.
- unfinished-mpeg-probes.zip: exact pre-existing untracked tools/mpeg_decoder_probe directory and tools/test_mpeg_ps_audio.py. Includes generated input.c, object files, probe ELF, compressed input and decoded output, retained for recovery rather than as acceptance evidence.
- Archive SHA256: 1c81ea7a243e6f0585c6000b8cb1d3e693bda4902568a576caaa6793d2b1ace3.

The MOV bulk-read and converter UI changes have NOT been newly tested or accepted in this handoff. Review byte order, aliasing, partial-read behavior, allocation ownership and stack use before integration. The prior handoff describes further unfinished work; do not infer it was implemented.

All original worktrees and dirty/untracked files were left intact. Other worktrees had no working-tree changes. Their checked-out committed tips were already ancestors of published master; they do not require re-publishing obsolete branches.

## Remaining issue work

Keep reporter issues open until their specific acceptance evidence exists. Console testing remains on hold. No new console execution was performed.
For #20/#23, re-convert the same source with the new GOP and obtain controlled playback comparisons. The shorter GOP is a mitigation candidate; the specific reconstruction-drift mechanism and claimed reason for DVD/VCD limits are not established by CI.
For #29/#24/#10 and the other reporter issues, use the existing open-issues handoff and fresh issue discussion. Host tests and decoder-only probes do not prove full playback, device timing or reporter acceptance.
Before resuming, refresh master, rolling, open issues/PRs and any later changes.
