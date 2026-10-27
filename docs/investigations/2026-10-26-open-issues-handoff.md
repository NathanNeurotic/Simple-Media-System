# Open issues: remaining work and validated fixes

Audit date: 2026-10-26 (America/Los_Angeles). Repository:
`NathanNeurotic/Simple-Media-System`.

## Starting state and build identity

- All nine open issues were read, including every available comment:
  #10, #20, #22, #23, #24, #25, #26, #27, #29.
- [PR #30](https://github.com/NathanNeurotic/Simple-Media-System/pull/30)
  is the existing issue-work branch, `fix/issues-2026-10`. Its inspected starting
  head was `c942e1cca411589a60aa97f783dea6f3798fe30b`; its checks were green.
  This follow-up belongs on that branch. Refresh its live head/CI before acting.
- Master and rolling were still
  `7b172d0c339f96b80c4eb22249c7f67c99180389`. Downloading rolling does not test
  an unmerged PR. Use an exact Actions run, its artifact and `build-info.json`.
- GitHub reported PR #30 as mergeable but `mergeable_state=blocked`.
  The active [Cautious ruleset](https://github.com/NathanNeurotic/Simple-Media-System/rules/18314832)
  restricts updates to the default branch and has bypass actors. Do not label
  this API status a test failure, a hardware result, or a missing review without
  checking the actual merge policy and account permissions.
- [PR #21](https://github.com/NathanNeurotic/Simple-Media-System/pull/21)
  is a separate draft MKV experiment at
  `ce76a18afa38617a3ee8e31030b63a837b1cc7f0`. Its contributor reported an actual
  console freeze with an Xvid/MP3 MKV, despite successful builds. It is not a
  prerequisite or a demonstrated fix for these nine issues. This audit read
  its metadata, changed-file list and discussion; it did not certify the demuxer.

## Fixes implemented in this follow-up

### MPEG-PS probing, allocation and progress (#29)

`src/SMS_ContainerMPEG_PS.c`:

- `_fill_audio_parameters` saved `m_Pos` (buffer/device bookkeeping) instead of
  the logical `m_CurPos`. Read-ahead and byte-wise parsing can make these differ.
  Restoring the wrong position misaligns the probe's subsequent skip/header read.
  It now restores the logical position.
- The audio probe used a PES-sized stack VLA, ignored the actual read count,
  subtracted four, then looped while the signed remainder was nonzero. A short
  packet could produce a negative bound/counter and out-of-bounds scanning.
  The checked heap allocation is limited by the 16-bit PES length, and scanning
  uses the actual bytes read and codec header sizes: MPEG audio 4, AC3 7, DTS 16,
  DVD PCM 3. Allocation/read failures reject the metadata probe safely.
- The video metadata probe now reads only the four sequence-header bytes it
  needs and rejects short reads, instead of allocating/reading the entire PES
  payload and parsing potentially uninitialized bytes.
- `_ReadPacket` now treats a failed header scan as end/error even before the
  advertised EOF. It flushes an existing video alignment tail once, with defined
  timestamps, then terminates. Previously it could use an uninitialized stream
  ID and repeatedly retry without progress.
- The inner MPEG seek scan now exits when start-code scanning fails.
- Probe allocations are checked, stream insertion reserves the NULL sentinel
  required by existing stream walkers, and rejected probes reset `m_nStm` after
  destroying their streams so a fallback probe sees a clean stream set.

These are reproduced code defects. They do **not** establish that the reporter's
precise MPEG-2/AC3 file now plays on SCPH-77004.

### Failed seek and large-skip fallback (#10)

`src/SMS_FileContext.c`: `STIO_Seek` now commits buffer/cursor changes only after
the native seek succeeds at the requested position. `File_Skip` checks that the
logical cursor reached the large-skip target; otherwise it uses the existing
short-read-bounded read loop. Previously a failed native seek could leave the
logical position advanced while the underlying descriptor never moved.

The existing 64 KiB seek optimization and EOF/overflow clamp remain. Driver
large-offset support and seek latency are not proven by the host stubs; do not
claim all devices seek in constant time or that all files above 2 GiB work.

### Folder-playlist failure handling (#25)

`src/SMS_GUICmdProc.c`: Play-all now skips a failed file open before dereferencing
its parameter wrapper, and frees that wrapper after player initialization.
This applies to sequential and shuffled folders. Existing shuffled node links
and the Randomize-playlist preference are retained.

### Spanish submission and translator instructions (#27)

Imported Nacheras's [SMS_es.zip attachment](https://github.com/NathanNeurotic/Simple-Media-System/issues/27#issuecomment-6417874587)
into `docs/translation/SMS.lng_ES` without rewriting its contents. Validation:
311 nonempty strings, matching printf formats/order, unchanged locked boot
paths, CP1252-compatible bytes and no Unicode BOM. The guide incorrectly said
308 strings and omitted the final three reference entries; it now says 311.

Attachment SHA-256:
`0996b2439d932e2a5d43421629b5e303d8fc86f9bd371286dd2ee65dca91168d`.
Submitted language SHA-256:
`792399020220beeb6cab9f82c39a8699186d2c36b38e37614d37239cd5ad9efc`.
Git normalizes text line endings; hashes describe the downloaded bytes. The
trailing spaces on the available-media and file-size labels are intentional
string spacing, not formatting cleanup candidates. On-console text clipping
and glyph rendering still need confirmation.

### Correct subtitle capability description (#22)

The converter's subtitle tooltip incorrectly said SMS cannot read separate
subtitles. It now describes optional permanent burn-in and existing external
SRT/SUB support. See `include/SMS_SubtitleContext.h`, AVI DXSB handling and the
player subtitle menu. Existing audio-language selection also exists in
`SMS_PlayerControl.c` / `SMS_PlayerMenu.c`; do not scope the converter's missing
multi-track output as if the entire runtime selection feature were absent.

## Validation performed

- New `tools/test_mpeg_ps_probe.py` compiles the actual entire MPEG-PS source
  with memory I/O and codec/DMA stubs under AddressSanitizer/UBSan. It covers
  exact-end headers, short/error reads, cursor restore, every context/stream/
  codec allocation failure, the audio buffer allocation failure, stream limits,
  tail flushing, malformed/truncated payload seeking and stalled packet reads.
- New `tools/test_playlist_playback.py` compiles the actual playlist function:
  failed opens, unsupported files, wrapper lifetime and shuffled-list links.
- New `tools/test_file_skip.py` compiles actual seek/skip functions: failed fio
  and fileXio seek preserves the prior context, fallback reads, large seek,
  overflow/EOF clamping, small skips and short reads.
- The original `c942e1c` audio function was checked separately: a valid AC3
  header restores the wrong cursor, and a zero-length payload triggers the
  sanitizer's non-positive VLA-bound error. Both fail before these fixes.
- Existing `test_stream_allocation.py`, `test_stio_refill.py`, dev9 v1.1 IRX ABI
  gate, all 26 converter tests and all nine converter preset conversions passed.
- Standard, packed, `BDM=0` and `SMS_DIAG=1` builds passed using the repository's
  existing `ps2dev/ps2dev:v1.0` GCC image. Builds ran in an isolated container
  copy, preserving the existing checkout's `obj/` and `bin/` recovery artifacts.
- The three new regressions are included in the compilation workflow.
- No new physical-console run or emulator acceptance test was performed.
  Codec/DMA stubs cannot establish color correctness, real decoding, device
  timing, audio/video synchronization or the reporters' reproduction results.

Local audit artifacts are under ignored `build/issue-audit/`: target build logs,
four ELFs in `dist/`, the original translation ZIP, import validation script,
baseline failure logs and a backup of the pre-existing unfinished audio test.

## Remaining work by issue

| Issue | Current evidence and next acceptance step |
| --- | --- |
| [#29 MPEG-2/AC3 freeze](https://github.com/NathanNeurotic/Simple-Media-System/issues/29) | Highest regression priority. Test the unchanged reporter MPG with the new standard and DIAG builds and the same USB/boot setup, against 2.9rev4. Obtain a short failing sample if it still fails. Diagnostic numbers are decimal: MPEG-PS is **E48 probe mpg**, PTS phases are **E50/E51/E52**. Save the last code and exact artifact identity. Existing comment text saying E40-E4B uses the wrong notation. |
| [#24 high-bitrate stutter](https://github.com/NathanNeurotic/Simple-Media-System/issues/24) | The reporter rejected the larger-buffer candidate in `aa801fec`; the current 32 KiB synchronous refill batch remains unconfirmed. Compare one unchanged MPG on USB and MX4SIO with 2.9rev4 and the new build. Measure bytes/call, elapsed read time, queue starvation and decode time before selecting a throughput change. A smaller batch does not add read-ahead or reduce the total RPC count. If starvation is proven, design an owned reader queue/cancellation path; do not switch shared fileXio global async mode casually. Preserve existing 4 KiB MX4SIO/MMCE calls and UDPFS sizing. |
| [#20 green color](https://github.com/NathanNeurotic/Simple-Media-System/issues/20), [#23 red fluctuation](https://github.com/NathanNeurotic/Simple-Media-System/issues/23) | Unresolved. The forced BT.601/range converter change was tested and failed/worsened the result, then reverted. No decoder/color coefficient fix is supplied by this audit. Obtain the exact converted failing MPG and source/FFmpeg log. Host-decode that MPG, then compare the identical bytes in 2.9rev4/current SMS with fixed PS2 video mode, RGB/component output, cable/display and 16/32-bit color settings. Compare converter outputs using one fixed player separately. An ignored matrix tag alone does not explain or prove the cause of abrupt saturated color patches. |
| [#10 MP4 stall](https://github.com/NathanNeurotic/Simple-Media-System/issues/10) | Large skips and failed-seek bookkeeping are addressed in code. The 0:28 start and 0:35 video/audio-loop stall are still unproven. Get the exact original and converter-produced files; record atom layout, codecs, timestamps, sample tables, file size and device offsets. Test moov-at-end and fast-start controls, including a device large-offset case. A fast open is not decode-stall acceptance. |
| [#27 skin restart](https://github.com/NathanNeurotic/Simple-Media-System/issues/27) | Existing PR repaint candidate needs a USB boot with default PAL/NTSC mode: import/select skin, save configuration, restart, confirm the background. Also retain MC boot and non-default display-mode controls. Spanish submission is now integrated; validate glyphs, clipping and the installed language pack on console. Translation acceptance does not close the skin defect. |
| [#25 shuffle](https://github.com/NathanNeurotic/Simple-Media-System/issues/25) | Folder shuffle implementation exists and failed-open/lifetime defects are now fixed. Test Randomize-playlist on/off with Play all -> Video, every eligible file appearing once, Stop/next behavior, a missing/unreadable item and a large folder. The toggle remains on the MP3 settings page, so document its effect on video folders. Console behavior is not yet accepted. |
| [#22 converter tracks/subtitles](https://github.com/NathanNeurotic/Simple-Media-System/issues/22) | Partial implementation only: select **one** source audio track and optionally burn an **external** subtitle file. Still missing: retaining multiple output audio tracks/dubs, embedded subtitle selection/export, the requested preserve-source SD resolution option and max-quality convenience preset. Existing manual controls are not that preset. Build on the player's existing audio switching and external subtitle support. Extend validation beyond the first audio/video stream before allowing multiple mapped streams; retain container/codec limits and the actual demuxer's stream capacity. Do not offer generic lossless audio in a container whose reader cannot decode it. Treat 10 Mbps playback as transport/content-specific evidence, not a universal throughput guarantee. |
| [#26 poster grid](https://github.com/NathanNeurotic/Simple-Media-System/issues/26) | No implementation in either open PR. A separate UI feature remains. Define artwork naming (e.g. same-basename JPEG), fallback behavior, dimensions, navigation and memory/VRAM budget. Reuse existing JPEG decode/rescale/upload for an initial bounded visible-page cache; release resources when changing folder or starting playback. Account for missing/corrupt/oversized art and slow/disconnected storage. PNG support must be established separately. Keep the normal list usable without artwork. |

## Next-agent execution order

1. Refresh PR #30/master/rolling and review this follow-up diff; use its exact
   Actions artifacts, never the older rolling download while it is unmerged.
   Save the diagnostic label as well as the number: M3U/MOV probe numbers overlap
   the PTS phase numbers, while their text labels distinguish the calls.
2. Prioritize #29, #24 and the #20/#23 A/B sample matrix. Retain all original
   issue symptoms; a successful host test or an unrelated fixed defect is not
   grounds for closing a reporter-owned issue.
3. Complete the #27 restart/language and #25 folder-playlist console checks.
4. Implement #22's remaining converter output/options against existing player
   capabilities. Develop #26 independently from playback regression work.
5. Reconcile merge-policy requirements and publish only after the intended
   review/acceptance decision. Leave reporter-owned issues open for confirmation.

The checkout initially contained untracked `tools/mpeg_decoder_probe/` and
`tools/test_mpeg_ps_audio.py`. They were preserved and excluded from the
follow-up commit. Their decoder output files and a pre-existing emulator log
are unfinished investigation artifacts, not new acceptance evidence. Do not
remove them or mistake them for a completed console test.
