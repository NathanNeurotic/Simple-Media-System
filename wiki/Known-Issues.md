# Known Issues & Testing

Status reviewed 2026-10-30 against merged master `34000cf`, including browser, MPEG preset and MOV sample-size follow-ups through [PR #42](https://github.com/NathanNeurotic/Simple-Media-System/pull/42). All nine reports below remain open. Consult the linked issue for newer reports; this page is a dated evidence summary.

## MPEG-2/AC3 opening freeze — #29

[The reporter's MPG freezes at container opening on an SCPH-77004/USB setup and worked in 2.9rev4.](https://github.com/NathanNeurotic/Simple-Media-System/issues/29) Probing, cursor restoration, short-read handling, metadata retries and progress exits have fixes with host regressions. Retest the unchanged reporter file on the same console/source using standard and DIAG builds. Record the last label: MPEG probing is `E48 probe mpg`; include its text because diagnostic numbers can overlap other phases.

## High-bitrate USB stutter — #24

[USB stutter remained in the reporter's later test; MX4SIO reduced it and 2.9rev4 played the same USB video smoothly.](https://github.com/NathanNeurotic/Simple-Media-System/issues/24) PR #35 adds driver-confirmed USB reads with a 64 KiB synchronous refill bound. Unknown/non-USB mass devices retain 4 KiB reads and 32 KiB refills; the MX4SIO workaround is preserved. Host controls establish routing, byte continuity and bounds; console throughput is unmeasured. Compare the identical file, source and settings with matched DIAG read/refill and queue measurements before calling the stutter resolved.

## Green or red video color — #20 / #23

[Green color](https://github.com/NathanNeurotic/Simple-Media-System/issues/20) and [red fluctuation](https://github.com/NathanNeurotic/Simple-Media-System/issues/23) have newer paired recordings and converter logs. Conversion success does not establish correct console output. The exact failing MPG bytes/hash are still needed.

The MPEG RGB ownership fix removes a reproduced held-buffer overwrite. Longer controls in PR #35 exposed signed chroma-vector rounding, XY interpolation and field-row order defects. Their fixes pass a native C/DMA/MMI and field-output oracle; unchanged 150-frame MPEG-1/2 inputs have substantially lower error in both 16/32-bit output. The field-row fix also removes all channel differences over 32 levels in the interlaced B-picture control. Read the [measured decoder report](https://github.com/NathanNeurotic/Simple-Media-System/blob/47b7df63abc0a05eb4f1c6c5c0b08851a03ae886/docs/investigations/2026-10-28-playback-followup.md) for scope and residual errors. These controls exclude the affected console's full display path. Test the unchanged failing MPG, pause/resume, seek, stop and sequence changes on the affected setup. No blanket BT.601/range transform is supplied. [PR #41 changed MPEG-1/2 converter presets from GOP 50 to 15](https://github.com/NathanNeurotic/Simple-Media-System/pull/41) to shorten prediction chains. Existing converted files are unchanged: re-convert the same source and compare both encodes on the affected setup. This is a mitigation candidate; the specific reconstruction-drift explanation and a complete color fix are not established by host or packaging tests.

## MP4 late start and stall — #10

[The reported 0:28 start and 0:35 video/audio-loop stall remain unconfirmed.](https://github.com/NathanNeurotic/Simple-Media-System/issues/10) MOV packet selection, offsets, timing tables, co64/extended atoms, allocation ownership and failed sample reads have host-tested fixes. [PR #42 additionally rejects incomplete variable sample-size tables](https://github.com/NathanNeurotic/Simple-Media-System/pull/42), with production-reader truncation controls. This does not establish faster opening. Obtain the exact original and converted files and test both ordinary and fast-start layouts through the full player. Faster opening alone does not demonstrate that the stall is fixed.

## Saved skin after restart / Spanish — #27

[Skin persistence still needs the reporter's USB save/restart test.](https://github.com/NathanNeurotic/Simple-Media-System/issues/27) The late-configuration repaint and skin bounds/allocation paths were fixed, and the Spanish strings were integrated. Test default PAL/NTSC, a non-default output mode and card/USB launch paths; check Spanish glyphs and label clipping separately. HD gradient behavior is not the same issue.

## Folder shuffle — #25

[Folder shuffle is implemented, with failed-open and playlist-lifetime regressions covered on the host.](https://github.com/NathanNeurotic/Simple-Media-System/issues/25) Enable **Randomize playlist** under **Player settings → MP3 settings**, then use **Play all → Video**. Check every eligible item appears once, Stop/next behavior, missing/unreadable files and a large folder on console.

## Converter tracks and subtitles — #22

[Multi-audio, text burn-in, source-SD sizing and the high-quality Xvid/PCM preset are implemented and host-tested.](https://github.com/NathanNeurotic/Simple-Media-System/issues/22) SMS audio switching, SD aspect/display and high-bitrate playback on suitable storage remain acceptance work. Switchable embedded text, sidecar export and bitmap conversion are separate from the implemented burn-in control.

## Poster / cover-art grid — #26

[PR #34 merged an optional R2 JPEG cover grid for #26 into master at 3eab0ee.](https://github.com/NathanNeurotic/Simple-Media-System/pull/34) Same-directory, same-basename JPEGs supply covers through a six-thumbnail cache; missing or unsupported artwork uses normal icons. See [setup, limits and controls](https://nathanneurotic.github.io/Simple-Media-System/video.html#artwork-browser). Lookup, cache, navigation, packet and lifecycle behavior have host controls. Merged follow-ups through PR #40 also cover long browser/HOST/M3U paths, navigation allocation failures and failed device entry. Master includes PR #35's playback fixes; console rendering, device removal, repeated navigation/playback transitions and resource-stability acceptance remain pending. #26 remains open.

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
