# PSX-Place v3.0.0-rev1 thread: report triage against master

Date: 2026-11-02. Source: a saved copy of the PSX-Place thread
["[REVIVED] Simple-Media-System (SMS) and Simple-Media-Converter!"](https://www.psx-place.com/threads/revived-simple-media-system-sms-and-simple-media-converter.50667/)
(posts 2026-07-25 to 2026-08-04) and two `sms-converter-crash.log` files from
[jolek](https://www.psx-place.com/members/jolek.33220/). Checked against master
`8a25e22`. The thread's screenshots were not in the saved copy, so items that
depend on an image are marked UNKNOWN rather than guessed.

Nothing here was tested on a console. "Fixed" means the current source handles
the reported case; it does not mean a reporter confirmed it.

## Still present: fixed in this branch

**720p display width can never return to its default.** `SMS_Config.c` ships
720p at 1216x676, but `_dispw_handler` capped 720p width at 1190. The first
X/O press on Display width snapped 1216 down to 1190 and 1216 was unreachable
after that. This is exactly jolek's report on SCPH-50004. It was inherited
unchanged from 2.9 rev4 (`OG-Simple-Media-System`, same tables).

Fix: the 720p ceiling is now 1216, the shipped default. It is deliberately not
1280: at 720p the framebuffer already takes about 3.3 MB of the GS's 4 MB, and
`SMS_GS_1.c` records that growing it risks the font/icon VRAM collision that
broke 1080i text. `tools/test_display_limits.py` reads both source tables and
fails if any of the 16 defaults sits outside its menu range. It fails on
`8a25e22` and passes with this change.

## Already fixed on master

| Report | Status | Where |
|---|---|---|
| No MC save icon when SMS runs from `mc?:/SMS/` | Fixed in source | `SMS_SaveConfig` writes `icon.sys` + `SMS.icn` beside the CWD config when the CWD is on a card (`_write_mc_icon`). |
| Converter: Win7 `api-ms-win-core-path` DLL error | Fixed | Separate `SMS-Media-Converter-Win7.exe` (Python 3.8, PySide2, PyInstaller 5) on rolling. |
| Converter: unreadable light-on-light buttons (Win7) | Fixed | Reported fixed in-thread on 2026-07-28. |
| Converter: closes on every MP4 convert | Fixed | `429a61d`. The crash log shows `-c:v libxvid -vtag XVID ... -f mp4`. `-vtag` is now emitted only for `.avi` (`ffmpeg_utils.py`), with a regression test. jolek's second log converts and validates successfully. |
| Converter: no crash log | Fixed | `crash_log.py`; both supplied logs came from it. |
| MP4 "loading" for minutes, then stuck at 0 s | Likely fixed, needs retest | Since July: large `File_Skip` seeks instead of reading (#10 multi-minute MP4 load), the `_ReadPacket` stale-pointer endless loop, `ctts` bounds and others (`docs/investigations/2026-10-26-open-issues-handoff.md`). The exact reporter files were never available. |
| MP4/XviD stutter or slideshow on USB vs 2.9 rev4 | Fix merged, needs retest | Issue #24, PR #49 (USB read-ahead). |

## Checked and not a bug in the current code

**MP4 read order.** I tested whether MOV playback seeks back and forth between
interleaved audio/video chunks (each backward seek drops the STIO buffer). A
60 s file made with the converter's MP4 preset command, parsed and read with the
real `SMS_ContainerMOV.c` packet functions on the host, read every packet in
file order: 4,614 packets, 0 backward seeks, 0 large forward seeks, about 1.01x
read amplification with a 192 KiB buffer model. That hypothesis is refuted for
converter output.

## Not bugs, or design questions

- **H.264 MP4 shows no picture.** Unsupported codec; documented.
- **Config saved beside the ELF (USB force-enabled when booted from USB).**
  Intended CWD design. [TnA](https://www.psx-place.com/members/tna.39081/)'s alternative (strip the `APP_` prefix to derive a
  separate config folder) is a design proposal for the owner, not a defect.
- **576p offered on a SCPH-50004.** Same mode list as 2.9 rev4. Whether early
  consoles output 576p correctly is UNKNOWN; no code change.
- **1080i width up to 1920 / 720p up to 1280.** Feature request. Blocked on the
  VRAM budget above; would need a GS memory plan and hardware testing.
- **Converter downmixes 5.1 AC3 to stereo.** Intended: SMS outputs stereo.

## Open: needs a sample file or a screenshot

- **Seek lands earlier than the chosen time (30:52 -> 29:59).** The scroll-bar
  seek always snaps to the previous keyframe. In `PlayerControl_ScrollBar`,
  `lPassTime` is overwritten with `lTime` every loop, so `lDir` is always -1.
  That is inherited from 2.9 rev4, and it is the usual choice (a forward snap
  would skip content). A 53 s gap means that file has keyframes about a minute
  apart or an index problem. Converter output uses `-g 50` (keyframe every
  1.7 s at 30 fps). Needs the reporter's file to go further.
- **"Glitch" on the right side of the movie in 720p/1080i.** The screenshot was
  not saved. Master already centers the 720p canvas and skips the background
  texture in HD modes. The 1190-width clamp above also widened the right-hand
  bar in 720p. Ask for a fresh photo on rolling once this lands.

## Smallest useful console test

Rolling build that includes this branch, 720p over component:
Settings > Display > Advanced, press X then O on Display width.
Expected: the value moves between 1215 and 1216, never drops to 1190, and the
picture still renders with fonts intact at 1216.
