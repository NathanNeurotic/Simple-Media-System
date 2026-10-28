# Known Issues & Testing

Status reviewed 2026-10-27 against `4a6959a`, the merge of PR #30. All nine reports below were open. Consult the linked issue for newer reports; this page is a dated evidence summary.

## MPEG-2/AC3 opening freeze — #29

[The reporter's MPG freezes at container opening on an SCPH-77004/USB setup and worked in 2.9rev4.](https://github.com/NathanNeurotic/Simple-Media-System/issues/29) Probing, cursor restoration, short-read handling, metadata retries and progress exits have fixes with host regressions. Retest the unchanged reporter file on the same console/source using standard and DIAG builds. Record the last label: MPEG probing is `E48 probe mpg`; include its text because diagnostic numbers can overlap other phases.

## High-bitrate USB stutter — #24

[USB stutter remained in the reporter's later test; MX4SIO reduced it and 2.9rev4 played the same USB video smoothly.](https://github.com/NathanNeurotic/Simple-Media-System/issues/24) Allocation/recovery fixes and bounded 32 KiB synchronous refills are implemented. A smaller batch is not asynchronous read-ahead and does not establish a throughput fix. Compare the identical file, source and settings while measuring read time, queue starvation and decode time before selecting another I/O change.

## Green or red video color — #20 / #23

[Green color](https://github.com/NathanNeurotic/Simple-Media-System/issues/20) and [red fluctuation](https://github.com/NathanNeurotic/Simple-Media-System/issues/23) have newer paired recordings and converter logs. Conversion success does not establish correct console output. The exact failing MPG bytes/hash are still needed.

The MPEG RGB ownership fix removes a reproduced held-buffer overwrite in controlled host/emulator tests. Synthetic raw decoder controls also pass at 512×480 and 640×480 in 16/32-bit modes. These controls exclude the affected console's full display path. Test the unchanged failing MPG, pause/resume, seek, stop and sequence changes on the affected setup. No blanket BT.601/range transform is supplied.

## MP4 late start and stall — #10

[The reported 0:28 start and 0:35 video/audio-loop stall remain unconfirmed.](https://github.com/NathanNeurotic/Simple-Media-System/issues/10) MOV packet selection, offsets, timing tables, co64/extended atoms, allocation ownership and failed sample reads have host-tested fixes. Obtain the exact original and converted files and test both ordinary and fast-start layouts through the full player. Faster opening alone does not demonstrate that the stall is fixed.

## Saved skin after restart / Spanish — #27

[Skin persistence still needs the reporter's USB save/restart test.](https://github.com/NathanNeurotic/Simple-Media-System/issues/27) The late-configuration repaint and skin bounds/allocation paths were fixed, and the Spanish strings were integrated. Test default PAL/NTSC, a non-default output mode and card/USB launch paths; check Spanish glyphs and label clipping separately. HD gradient behavior is not the same issue.

## Folder shuffle — #25

[Folder shuffle is implemented, with failed-open and playlist-lifetime regressions covered on the host.](https://github.com/NathanNeurotic/Simple-Media-System/issues/25) Enable **Randomize playlist** under **Player settings → MP3 settings**, then use **Play all → Video**. Check every eligible item appears once, Stop/next behavior, missing/unreadable files and a large folder on console.

## Converter tracks and subtitles — #22

[Multi-audio, text burn-in, source-SD sizing and the high-quality Xvid/PCM preset are implemented and host-tested.](https://github.com/NathanNeurotic/Simple-Media-System/issues/22) SMS audio switching, SD aspect/display and high-bitrate playback on suitable storage remain acceptance work. Switchable embedded text, sidecar export and bitmap conversion are separate from the implemented burn-in control.

## Poster / cover-art grid — #26

[Poster-grid browsing is unimplemented.](https://github.com/NathanNeurotic/Simple-Media-System/issues/26) A future feature needs artwork naming, fallback/navigation rules, a bounded JPEG cache, lifecycle cleanup, and memory/VRAM and slow-storage testing. The current picture viewer is not a poster browser.

## How to read validation claims

| Evidence | What it establishes |
| --- | --- |
| Host tests / sanitizers | Specific C/Python behavior with fixtures and controlled interfaces; not PS2 scheduling, bus throughput or display output. |
| PS2 target build / ABI checks | Compilation, linking and checked module imports; not actual device operation. |
| Controlled PCSX2 probes | Only the exercised decoder, buffer, IPU and scheduling paths; not a full console playback result. |
| Real-console comparison | The recorded console, source, media bytes, build and settings. Do not generalize a passing profile to every encode or transport. |

For the detailed reproductions and next-agent work, read the [engineering handoff](https://github.com/NathanNeurotic/Simple-Media-System/blob/master/docs/investigations/2026-10-26-open-issues-handoff.md) and [color-control report](https://github.com/NathanNeurotic/Simple-Media-System/blob/master/docs/investigations/2026-10-26-mpeg-color-control.md). Use [the reporting checklist](https://nathanneurotic.github.io/Simple-Media-System/troubleshooting.html#reporting) for a new result.

---
[Searchable manual](https://nathanneurotic.github.io/Simple-Media-System/known-issues.html) · [Edit the maintained source](https://github.com/NathanNeurotic/Simple-Media-System/blob/master/docs-site/content/known-issues.html)
