# Handoff — SMS converter refinement + MP4 load-time fix

Paused 2026-07-29 (ran out of credits mid-task). Branch `claude/sync-status-check-fb8014`,
worktree `.claude/worktrees/sync-status-check-fb8014`. **Nothing is committed.** Two files are
modified in the working tree; `git diff` is the source of truth.

`.sms-handoff/` is scratch for this handoff — delete it before merging. It is untracked.

---

## The one thing to act on first

**The converter believes SMS can open ~4 GiB files. It cannot. The real ceiling is 2 GiB − 1.**

`STIO_Seek` (`src/SMS_FileContext.c:1307-1325`) takes an `unsigned int` position and passes it
straight into `fioLseek` / `fileXioLseek`, both of which take a **signed int** offset. There is
no `fioLseek64` / `fileXioLseek64` anywhere in `src/`. AVI and MOV both seek during *ordinary
playback*, not just user scrubbing, so this is not a "only breaks if you skip around" limit.

The converter currently encodes the wrong number in three places:

- `tools/sms_converter/config.py` — `SIZE_4_0_GIB`, `SIZE_3_8_GIB`, `MSG_FILE_SIZE_APPROACHING_LIMIT`
- `tools/sms_converter/validator.py:134` — the `>= SIZE_4_0_GIB` FAIL
- `tools/sms_converter/worker.py:309` — the mid-encode size abort
- `tools/sms_converter/ui/main_window.py:448` — the projected-size banner

Right now the tool will happily produce a 3 GiB file, validate it PASS, and hand the user
something SMS cannot play. This is a live correctness bug, not a refinement.

---

## State of the work

### DONE — `src/SMS_ContainerMOV.c` (MP4 sample-table bulk read)

Replaces the per-entry `_read_int_be` loops in all six sample-table readers (`stts`, `stsc`,
`stsz`, `stco`/`co64`, `ctts`, `stss`) with bulk `FileContext::Read` + in-place byte swap. This
is the "MP4 takes 8 minutes to start" fix — the tables are proportional to sample count, so a
2-hour 30 fps file was ~216,000 individual reads for `stsz` alone.

Two new helpers near the top: `_read_be32_tbl` and `_be64_tbl_join`.

**Verified:** `make BDM=1 all` in `ps2dev/ps2dev:v1.0` exits 0, no errors, no undefined
references, `bin/SMS.elf` links at 1,784,584 bytes. `SMS_ContainerMOV.c` compiles with **zero
warnings** at `-Wall`.

```bash
docker run --rm -v "C:\Users\natha\Github\Simple-Media-System\Simple-Media-System\.claude\worktrees\sync-status-check-fb8014:/src" -w /src ps2dev/ps2dev:v1.0 sh -c 'apk add --no-cache build-base git zip >/dev/null 2>&1; make BDM=1 all'
```

**NOT verified:** runtime behaviour. No PS2 here. This needs GhostTown (tester, on a **PSX
DESR** — 8 MB IOP RAM, which is why the info panel shows ~6.88 MB free IOP; that is normal, not
a leak).

**An adversarial review of this diff was launched and then killed for credits** (run
`wf_047568f7-7ad`). It did not finish. Partial per-agent output may be in
`.claude/projects/.../subagents/workflows/wf_047568f7-7ad/journal.jsonl`. **Re-run it before
trusting this change on hardware.** The four lenses it used were: byte-order equivalence,
degrade/cursor-sync paths, UB/strict-aliasing/overflow, and scope creep. Script is at
`.claude/projects/.../workflows/scripts/verify-mov-bulk-read-wf_047568f7-7ad.js`.

Specific things I want a second pair of eyes on:

1. **`co64` double-swap risk.** `_read_be32_tbl` byte-swaps each 32-bit half, then
   `_be64_tbl_join` swaps the two halves' *positions* only. I caught myself byte-swapping twice
   in the first draft and fixed it. Together they must equal the old
   `_read_long_be` = `(bswap32(first) << 32) | bswap32(second)`. Prove it with real bytes.
2. **`stco` zero-extension.** Values go through `(uint32_t)` before landing in `int64_t`, to
   match the old `_read_int_be` assignment. Check an offset ≥ `0x80000000` doesn't sign-extend.
3. **`lnEntries * 3` at the `stsc` call site.** The overflow guard is
   `UINT_MAX / sizeof(MOVStsc)` (= UINT_MAX/12) in the *caller*, but the multiply by 3 also
   happens in the caller before `_read_be32_tbl` sees it. Confirm it cannot wrap.
4. **`_mov_read_ctts`** — I removed the `File_Skip(8 * (lnEntries - i - 1))` on the negative-
   duration bail, because the bulk read already consumed the whole table. Verify for the case
   where the bad entry is first and where it is last.
5. **`STIO_Read` short-read quirk** (`src/SMS_FileContext.c:1265`): on a partial read it appears
   to return 0 rather than the byte count. My `!= lBytes` test should cover both, but confirm.
6. **1 KB stack array** `uint32_t lWnd[256]` in `_mov_read_stco`, reached through recursive atom
   descent. Check SMS's demuxer thread stack size.

Aliasing reasoning (this file is `-O2` **without** `-fno-strict-aliasing`): swaps go through the
unsigned counterpart of the destination's own type only — `uint32_t` over `int` (legal, C99
6.5p7 signed/unsigned variant) and `uint64_t` over `int64_t` (same). `stco` is the one table
whose on-disk width (4) ≠ in-memory width (8), so it uses a scratch window instead of an
illegal `uint32_t` view of an `int64_t` array. Don't "simplify" that away.

### DONE — `tools/sms_converter/ui/details_dialog.py` (SQUARE hint, converter side)

Quiet always-present footer below the tabs, plus the same line appended to the exported log.
Constant is `SQUARE_HINT` at the top of the file with the full reasoning. 20/20 converter tests
still pass (`QT_QPA_PLATFORM=offscreen python -m unittest test_converter`).

Why the hint exists at all: **SMS applies no aspect-ratio signalling to AVI or MP4.** MPEG-4's
`m_SampleAspectRatio` is parsed and never read (`SMS_MPEG4.c:800-807`), libmpeg discards
`aspect_ratio_information` (`src/libmpeg.c:266`), and the only widescreen flag in the tree is set
by the MPEG-PS demuxer alone (`SMS_ContainerMPEG_PS.c:304`). So the console genuinely cannot know
how a converted file should be framed, and SQUARE is the user's only recourse.

### NOT DONE — SMS file-browser hover hint

The user asked for the same one-liner "on file browser menu when hovering on a media file."
I investigated and did **not** implement it, because it needs a UI-surface decision I couldn't
make confidently. What I established:

- Browser is `src/SMS_GUIFileMenu.c`. Navigation is `SMS_PAD_UP` / `SMS_PAD_DOWN` → `_step_up` /
  `_step_down` → `_redraw` (~line 352).
- There **is** a bottom status bar: `g_pStatusLine`, repainted on every scroll step inside
  `_redraw`. The comment block there (lines ~358-372) explains it holds "status/path text" and
  documents a DMA-ordering hazard with the async clock — read it before touching that area.
- **Trap:** `SMS_PAD_SQUARE` in the *browser* is already bound, to a context action
  (`SMS_GUIFileMenu.c:946`). The pan/scan SQUARE is **playback-only** (`SMS_Player.c:1279`).
  The hint text must say "during playback" or it will be actively wrong.
- Adding a user-visible string means: a new `SMS_STRING(312)` in `include/SMS_Locale.h` (312
  strings exist today), a static + table entry in `src/SMS_Locale.c`, and 19 packs under `lng/`.
  Check first whether the loader tolerates a pack that is short by one entry and falls back to
  the built-in default — if it does, the packs can be updated lazily.

Open question for the user: the browser has no per-file hint area today, so this is either
(a) a line in the existing bottom status bar, (b) a new element, or (c) folded into the existing
file-info display. Worth asking rather than guessing.

---

## Original scope, still outstanding

From the user's first message — none of this is started:

1. **Source-confirmed ceilings in Advanced Settings, with warnings outside them.**
   Design is settled: **hard-block only what SMS cannot open; warn with real numbers for
   everything that merely degrades.** Evidence: tester played 1024×620 fine and 5 Mbps stuttered
   without crashing, so clamping the spinboxes down would remove things that demonstrably work.
2. **Audio passthrough** (`-c:a copy`) — for the 5.1 AC3 users. Confirmed viable, see below.
3. **Cross-conversion** audio-source → video-target and video-source → audio-target.
4. **Startup-cost warning** for large MP4s (new, from the tester report).

### Audio passthrough — the research is done, just needs building

SMS decodes 5.1 AC3 natively, two ways, chosen by an existing SMS setting
`"S/PDIF - Dolby Digital/DTS"` (`src/SMS_Locale.c:248`):

- **S/PDIF off** (`_check_ac3`, `src/SMS_Player.c:100-114`): channels clamp to 2, then
  `SMS_AC3.c:2590` sets `lFlags = AC3_DOLBY` and liba52's full downmixer (compiled in) folds 5.1
  → stereo. User-adjustable level `m_PlayerAC3RL`, default 6 (`SMS_Config.c:535`).
- **S/PDIF on**: `_ac3_spdif` wraps the untouched AC3 frame in an IEC 61937 burst (preamble
  `0xF872 0x4E1F`, `SMS_AC3.c:2637`) out the optical port — **true 5.1 to a receiver**.

So `-c:a copy` is lossless *and* faster than today's downmix-plus-re-encode. Same branch covers
DTS; multichannel AAC is clamped identically at `SMS_Player.c:112`.

Caveats to carry into the UI copy: `SMS_PF_PDW22` forces the software-downmix branch even with
S/PDIF on, so don't promise S/PDIF unconditionally. The S/PDIF path hardcodes `m_Channels = 5`
and fixed 6144-byte bursts; 48 kHz is the safe recommendation until 44.1/32 kHz is checked.

---

## The capability audit — read this before touching ceilings

`.sms-handoff/capability-audit-full.md` (366 KB) is the full output of a 10-agent, ~2.2M-token
source audit. It is **the** reference for what SMS can actually play, with `file:line` citations
and HARD/SOFT labels. Highlights beyond the 2 GiB bug:

| Claim | Verdict |
|---|---|
| `MAX_WIDTH`/`MAX_HEIGHT` = 1024 | **Correct.** HARD, `SMS_Player.c:1676-1681`, `STR_RESOLUTION_TOO_BIG` |
| `MAX_VIDEO_PACKET_SIZE` 2,097,088 / `MAX_AUDIO_PACKET_SIZE` 524,224 | **Correct** |
| `MAX_SMS_STREAMS` = 8 | Correct in general, but **MPEG-PS is ≤ 7** — the increment at `SMS_ContainerMPEG_PS.c:743` is unguarded and corrupts memory at 8 |
| AAC not allowed in AVI | **WRONG.** `0x00FF` is in the table (`SMS_Codec.c:67`) and ffmpeg emits exactly that. `config.py` and the assertion at `test_converter.py:232` both need fixing |
| Vorbis allowed in AVI | **WRONG and worse.** ffmpeg writes wFormatTag `0x566F`; SMS only knows `0x674F` → silent file that validates clean. Remove it |
| ~4 GiB file limit | **WRONG** — 2 GiB − 1, see top |

Also confirmed and relevant: `MAX_PICTURE_COUNT` is 3 (`include/SMS_MPEG.h:96`) with **no NULL
check** on `FindUnusedPic()`, so a B-pyramid is an unchecked deref — consecutive B-frames are
fine, pyramids are not. MSMPEG4v3 needs height ≥ 144 (`SMS_MSMPEG4.c:2139-2143`, division by
zero below that). And the padded-frame-area ceiling (~741,376 px, `SMS_IPU.c:735,751-757`)
independently predicts that 1024×620 fits and 1024×768 does not — which matches the tester's
result exactly, so that arithmetic is trustworthy.

The audit tags every claim `[SRC]`/`[SRC-V]`/`[EMP]`/`[EMP-V]`/`[UNCONF]`. Respect `[UNCONF]` —
nothing in it was validated on real hardware.

---

## Ground rules from the user

- SMS was originally off-limits ("leave SMS itself alone, its working perfectly"). That was
  **explicitly relaxed** for two items: the MP4 bulk-read, and the browser hover hint. Do not
  treat it as blanket permission for the rest of the SMS findings above — surface them and let
  the user decide.
- The user pushes back on unverified claims and is right to. Verify before asserting; say
  UNCONFIRMED when it is.
- Known SMS issues from the tester that are **not** in scope and not investigated: first-playback-
  after-cold-boot breakage (SQUARE clears it), clock corruption after playback (candidate fix on
  unmerged branch `codex/fix-clock-status-dma-ordering`, commit `95b803b`, one line in
  `SMS_GUIDesktop.c` — recent and authored by the user, but it sits in the *status-message* path
  so it may not be the same code path at all), and an FF crash seen once.
- I withdrew an earlier claim of a memory leak. The info panel's free-EE figure is
  `32MB - ps2_sbrk(0)`; `sbrk` never retracts, so a drop that persists after playback is a heap
  high-water mark, not a leak. Discriminator if anyone wants to settle it: play the same file
  3-4 times — plateau = fine, monotonic decline = real leak.
