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

### Continuation: MOV packet handling and converter #22

The continuation after `a99a1a2` found additional work on our side. It adds the
MOV fixes below, multi-audio conversion, embedded text-subtitle burn-in,
source-SD sizing and a high-quality Xvid/PCM preset. It also repairs an actual
Windows CI download failure. The separate
[color investigation](2026-10-26-mpeg-color-control.md) records fresh decoder-only
emulator controls, with their limitations. None of these results closes a
reporter-owned playback issue.

### Completed review of 266851d: additional fixes

The review that was pending at the first handoff found more implementation
defects. These were verified against the callers and addressed:

- Folder shuffle now starts the initial menu selection at the **new** list
  head. The prior local variable retained the original head, so accepting the
  menu could skip the earlier part of the shuffled list. The test menu stub
  incorrectly reset that selection and masked the defect; it now preserves
  the incoming selection like the real menu, with a deterministic permutation
  that moves the original head.
- Both PlayerControl resume paths now request the same block/sector count as
  playback startup (`m_StreamSize >> 3`): 48 STIO blocks or 54 CDDA sectors.
  They previously requested eight times that amount after a seek.
- Matching stream-buffer size/count now reuses existing buffers without
  allocating replacements. Genuine replacements reset the cursors to their
  new buffer. CDDA also resets its active buffer/cursors before its first read,
  including reuse, so a failed read/sync cannot leave a cursor into a freed or
  different buffer. Actual CDDA read/sync-failure tests exercise ordinary
  seek/read recovery after both replacement and reuse.
- Converter source-track selection no longer confuses an eight-output-stream
  limit with the source-track index; later source tracks can be selected.
  Changing tracks/subtitles marks settings custom, and loading another preset
  recomputes the badge while preserving those source selections.

The corrected allocation, CDDA, rebuffer and playlist regressions fail against
266851d and pass with these changes. These findings were work remaining on our
side; reporter confirmation alone would not have completed that earlier head.

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

### MOV packet selection and composition timing (#10)

`src/SMS_ContainerMOV.c` had more defects beyond large atom skipping:

- When skipping disabled streams, `_ReadPacket` retained the preceding sample
  pointer between iterations. Once all streams were exhausted, the stale
  pointer could keep the loop running indefinitely. Each selection now starts
  empty and is bounded by both the sample count and the built index.
- The composition-time (`ctts`) entry was dereferenced before its bound was
  checked. A truncated table produced a heap-buffer-overflow. Offsets now walk
  with every consumed sample, including skipped streams, skip empty entries,
  and fall back to DTS after the table ends. Seeking synchronizes to the same
  exhausted state, rather than reusing the last offset indefinitely.
- DTS ordering and cross-stream seeking multiplied two 32-bit values before
  widening them. The multiplication now takes place in 64 bits.
- Sample offset/size bounds are checked before allocation. Failed positioning
  and short payload reads terminate instead of returning incomplete packets.
  Zero-byte samples are skipped with a finite sample cursor. Stream/codec
  allocations in the track parser are checked.
- Index construction keeps cross-stream chunk offsets alive until every stream
  has been indexed. The previous loop freed a video stream's offsets before
  later per-chunk audio sizing inspected them: an actual heap-use-after-free.
  Other sample tables are still released after their own index is built to
  limit peak memory. Destruction also releases partially built tables.
- Index growth and `mdat` growth now preserve their old pointers/counts on
  allocation failure. A failed index build rejects and cleans the probe rather
  than dereferencing NULL or publishing a partial index. The container-context
  allocation is checked, and the track limit reserves the stream sentinel.
- Audio chunk durations consume only the samples covered by each timing entry
  and accumulate in 64 bits. The prior calculation overcounted a chunk crossing
  timing entries (a 2x10 + 3x20 control produced 110 instead of 80).
- The M4A metadata probe now initializes its temporary container and sets the
  parser's base-container pointer before dispatching child atoms. Packet bounds
  are checked only for enabled tracks with nonempty samples, preserving valid
  playback when an ignored track has unusable offsets.

`tools/test_mov_packets.py` compiles the actual packet/timing functions under
ASan/UBSan. The `a99a1a2` baseline independently fails its disabled-stream test
by timeout, composition test with a heap-buffer-overflow, timestamp-ordering
assertion and short-read assertion. The current source passes all cases. This
is not an end-to-end MOV parser/decoder or real-device test, and the exact
0:28/0:35 reporter file remains unavailable.

`tools/test_mov_index.py` additionally compiles the actual index/header,
allocation, track-limit and cleanup functions. Its parser/codec stubs do not
establish full atom-parser acceptance. Against `049f571`, separate cases
reproduce the cross-stream heap-use-after-free, incorrect chunk timestamp,
NULL index-allocation write, missing stream sentinel and missing M4A base
relationship. Current cases pass ASan/UBSan, including index failure after an
earlier stream has already been built and partly cleaned.

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

### Converter track output, SD sizing and high-quality preset (#22)

- Optional **Keep all source audio tracks** encodes all dubs/commentary with
  the chosen settings in AVI, MPEG-PS or MP4. It validates the source metadata
  before encoding and rejects more than six audio tracks plus video, reserving
  the player's trailing NULL stream slot. Manual single-track selection remains
  the default. Language/title metadata is mapped where the muxer supports it.
- Optional embedded **text** subtitle selection burns that track permanently
  into the picture. It rejects absent/bitmap tracks and simultaneous external
  and embedded choices. This does not add switchable embedded subtitles,
  sidecar export, bitmap conversion or a new player subtitle decoder.
- **Keep source SD resolution** retains even source dimensions up to 720x576;
  larger sources use the preset target. It remains opt-in for existing presets.
- The new **High Quality - Fast Storage** Xvid AVI preset targets 10 Mbps,
  keyframe interval 6, 48 kHz stereo PCM16 audio and source SD resolution. PCM
  is uncompressed after conversion; resampling/channel conversion means it is
  not a promise of a bit-identical source audio copy. It makes no universal
  transport/real-console throughput guarantee.
- Validation checks every output audio/video stream, not only the first.
  Estimates include every mapped audio track and PCM's fixed data rate. Source
  probing is cached across both encoding passes. FFprobe JSON is explicitly
  read as UTF-8, preserving non-Latin track tags on Windows.
- Track-title tests verify the converted output: AVI uses its stream title,
  and MP4 stores the name as `handler_name`. Invalid track selections are
  reported as settings errors before encoding, without a crash report.

### Windows FFmpeg fallback build blocker

The push workflow for `a99a1a2` had a genuine Win7-job setup failure: Chocolatey
returned 503/504, then the pinned August 8 FFmpeg fallback returned 404. The
equivalent pull-request job passed; that did not repair the fallback.

BtbN's [retention policy](https://github.com/BtbN/FFmpeg-Builds#release-retention-policy)
keeps daily builds for 14 days and month-end builds for two years. The action
now pins the retained September 30 build, still checks its SHA-256 before
unpacking, and runs that path directly in the Win7 job so it is exercised
regularly. Refresh the pin before September 2028. The runner's FFmpeg validation
does not prove that this FFmpeg binary runs on Windows 7 itself.

Verified fallback archive SHA-256:
`7b801cdd3a1a0bb54ae6f572187e68b4ed54f52086cfee133fab2180bfb429fa`.

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
  gate, all 27 existing converter tests, nine new track/worker/UI tests and all
  ten converter preset conversions passed. New real FFmpeg cases cover both
  dubs in MP4, AVI and MPEG-PS, language tags, two-pass encoding with one source
  probe, distinct burned subtitle pictures, SD sizing, invalid track selections,
  excess streams and secondary unsupported codecs. They also passed using the
  exact replacement fallback archive.
- Standard, packed, `BDM=0` and `SMS_DIAG=1` builds passed using the repository's
  existing `ps2dev/ps2dev:v1.0` GCC image. Builds ran in an isolated container
  copy, preserving the existing checkout's `obj/` and `bin/` recovery artifacts.
- All new regressions, including CDDA failure recovery and start/resume stream
  sizing, MOV packet handling and MOV index lifetime/failure handling, are
  included in the compilation workflow.
  Modern Windows/Linux and the PySide2 Win7 job run the new converter tests.
- Fresh isolated PCSX2 2.9.52 controls decoded five synthetic MPEG-2 frames in
  both 16-bit and 32-bit modes using SMS's actual libmpeg/IPU source. See the
  separate color investigation for hashes, errors and excluded player paths.
- No physical-console run or reporter acceptance test was performed. Host
  stubs and decoder-only emulation do not establish device timing, complete
  player output, audio/video synchronization or the reporters' reproductions.

Local audit artifacts are under ignored `build/issue-audit/`: target build logs,
four ELFs in `dist/`, the original translation ZIP, import validation script,
baseline failure logs and a backup of the pre-existing unfinished audio test.

## Remaining work by issue

| Issue | Current evidence and next acceptance step |
| --- | --- |
| [#29 MPEG-2/AC3 freeze](https://github.com/NathanNeurotic/Simple-Media-System/issues/29) | Highest regression priority. Test the unchanged reporter MPG with the new standard and DIAG builds and the same USB/boot setup, against 2.9rev4. Obtain a short failing sample if it still fails. Diagnostic numbers are decimal: MPEG-PS is **E48 probe mpg**, PTS phases are **E50/E51/E52**. Save the last code and exact artifact identity. Existing comment text saying E40-E4B uses the wrong notation. |
| [#24 high-bitrate stutter](https://github.com/NathanNeurotic/Simple-Media-System/issues/24) | The reporter rejected the larger-buffer candidate in `aa801fec`; the current 32 KiB synchronous refill batch remains unconfirmed. Compare one unchanged MPG on USB and MX4SIO with 2.9rev4 and the new build. Measure bytes/call, elapsed read time, queue starvation and decode time before selecting a throughput change. A smaller batch does not add read-ahead or reduce the total RPC count. If starvation is proven, design an owned reader queue/cancellation path; do not switch shared fileXio global async mode casually. Preserve existing 4 KiB MX4SIO/MMCE calls and UDPFS sizing. |
| [#20 green color](https://github.com/NathanNeurotic/Simple-Media-System/issues/20), [#23 red fluctuation](https://github.com/NathanNeurotic/Simple-Media-System/issues/23) | Unresolved. The forced BT.601/range converter change was tested and failed/worsened the result, then reverted. No decoder/color coefficient fix is supplied by this audit. Obtain the exact converted failing MPG and source/FFmpeg log. Host-decode that MPG, then compare the identical bytes in 2.9rev4/current SMS with fixed PS2 video mode, RGB/component output, cable/display and 16/32-bit color settings. Compare converter outputs using one fixed player separately. An ignored matrix tag alone does not explain or prove the cause of abrupt saturated color patches. |
| [#10 MP4 stall](https://github.com/NathanNeurotic/Simple-Media-System/issues/10) | Large skips, failed-seek bookkeeping, disabled-stream EOF loops, truncated composition tables, 32-bit DTS multiplication and incomplete sample reads are addressed in code. The 0:28 start and 0:35 video/audio-loop stall are still unproven. Get the exact original and converter-produced files; record atom layout, codecs, timestamps, sample tables, file size and device offsets. Test moov-at-end and fast-start controls, including a device large-offset case. A fast open is not decode-stall acceptance. |
| [#27 skin restart](https://github.com/NathanNeurotic/Simple-Media-System/issues/27) | Existing PR repaint candidate needs a USB boot with default PAL/NTSC mode: import/select skin, save configuration, restart, confirm the background. Also retain MC boot and non-default display-mode controls. Spanish submission is now integrated; validate glyphs, clipping and the installed language pack on console. Translation acceptance does not close the skin defect. |
| [#25 shuffle](https://github.com/NathanNeurotic/Simple-Media-System/issues/25) | Folder shuffle implementation exists and failed-open/lifetime defects are now fixed. Test Randomize-playlist on/off with Play all -> Video, every eligible file appearing once, Stop/next behavior, a missing/unreadable item and a large folder. The toggle remains on the MP3 settings page, so document its effect on video folders. Console behavior is not yet accepted. |
| [#22 converter tracks/subtitles](https://github.com/NathanNeurotic/Simple-Media-System/issues/22) | Multi-audio output, external/embedded text-subtitle burn-in, keep-source-SD sizing and the requested 10 Mbps/GOP 6 convenience preset are implemented and host-tested. Validate actual audio switching, SD aspect/display and high-quality Xvid/PCM on SMS with suitable storage. Switchable embedded subtitles, sidecar export and bitmap subtitles remain outside the implemented burn-in path; decide whether any is needed after testing the requested workflow. Uncompressed 48 kHz stereo PCM is not a bit-identical source-preservation promise. |
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
4. Verify #22's new outputs with the player's audio switching and external
   subtitle support; distinguish optional further subtitle work from implemented
   burn-in. Develop #26 independently from playback regression work.
5. Reconcile merge-policy requirements and publish only after the intended
   review/acceptance decision. Leave reporter-owned issues open for confirmation.

The checkout initially contained untracked `tools/mpeg_decoder_probe/` and
`tools/test_mpeg_ps_audio.py`. They were preserved and excluded from the
follow-up commit. Their decoder output files and a pre-existing emulator log
are unfinished investigation artifacts, not new acceptance evidence. Do not
remove them or mistake them for a completed console test. The new, tracked
`tools/mpeg_color_probe/` and its explicitly recorded isolated emulator runs
are separate from those pre-existing artifacts.
