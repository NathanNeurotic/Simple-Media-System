# Simple Media System

Your videos, music and photographs on the PS2. Browse files from USB, MX4SIO, internal HDD, MMCE, i.LINK, discs or a network share, with a companion converter for modern media.

[Play your first file](https://nathanneurotic.github.io/Simple-Media-System/getting-started.html) · [Get SMS & the converter](https://nathanneurotic.github.io/Simple-Media-System/releases.html)

MPEG-1 / MPEG-2 · DIVX / XVID · AUDIO & JPEG · MX4SIO · SMB / UDPFS

**Following the October fixes?** Use the [rolling development build](https://nathanneurotic.github.io/Simple-Media-System/releases.html#this-build). The July stable tag does not include the October parser, streaming, frame-ownership and converter changes. [Reported console regressions still need confirmation.](https://nathanneurotic.github.io/Simple-Media-System/known-issues.html)

## The maintained SMS fork

SMS was created by **Eugene Plotnikov** and developed by the PS2 homebrew community. This storage-focused fork adds MX4SIO, ATA/exFAT, i.LINK and MMCE paths, a folder-served UDPFS network drive, on-screen network configuration, USB/Bluetooth DualShock 3/4 input and updated artwork. You need a PS2 that can already launch homebrew; a hard disk or network adapter is optional.

The October development work adds folder shuffle, converter multi-audio output and text-subtitle burn-in, and fixes reproduced parser, streaming and frame-lifetime defects. [Known issues](https://nathanneurotic.github.io/Simple-Media-System/known-issues.html) distinguishes implemented fixes from the reports still awaiting affected-console tests. An optional [poster grid](https://nathanneurotic.github.io/Simple-Media-System/video.html#artwork-browser) is included in current master, with console acceptance pending.

## Choose your next step

- [First playback Install, enable a source and open a file.](https://nathanneurotic.github.io/Simple-Media-System/getting-started.html)
- [Prepare media Presets, tracks, subtitles and output validation.](https://nathanneurotic.github.io/Simple-Media-System/converter.html)
- [Choose storage Device names, setup and driver exclusions.](https://nathanneurotic.github.io/Simple-Media-System/devices.html)
- [Keep your settings Boot-folder config, network files and fallback paths.](https://nathanneurotic.github.io/Simple-Media-System/configuration.html)
- [Check current issues Open reports, tested scope and useful reproductions.](https://nathanneurotic.github.io/Simple-Media-System/known-issues.html)
- [Browse the wiki The complete guide and reference map.](https://nathanneurotic.github.io/Simple-Media-System/wiki.html)

## Formats and limits

| Media | Current routes |
| --- | --- |
| Video | MPEG-4 Part 2 / Xvid / DivX in AVI; `mp4v` in MP4; MPEG-1/2 in MPEG program streams. [Format details](https://nathanneurotic.github.io/Simple-Media-System/video.html). |
| Music | MP2/MP3/MPA, AAC/M4A, AC-3, FLAC, Ogg Vorbis and WMA v1/v2. [Audio constraints](https://nathanneurotic.github.io/Simple-Media-System/audio.html). |
| Pictures and text | Baseline JPEG, M3U playlists and external SRT/SUB/TXT subtitles. Existing AVI DXSB bitmap subtitles are a separate path. |
| Video dimensions | At most 1024 pixels on either axis. This is an acceptance check, not a real-time playback guarantee. |
| Files | Keep each file below 4 GiB on every source, including exFAT. |
| Tracks | Eight stream-table slots; MP4 and MPEG-PS retain at most seven streams to reserve a terminator. The converter allows video plus six audio tracks. |
| Unsupported video | H.264/AVC, H.265/HEVC, VP9, AV1 and WMV. An MP4 extension alone does not identify the video codec. |

Earlier project documentation records successful standard-definition fixture playback. Those are useful starting profiles; generating the [test-media pack](https://nathanneurotic.github.io/Simple-Media-System/releases.html#test-media) in CI does not retest a console. High bitrate, resolution, audio workload and source throughput still matter.

## Start with a small, known file

1. [Download and launch the standard ELF](https://nathanneurotic.github.io/Simple-Media-System/install.html) with your homebrew loader.
2. Use a supported existing FAT32/exFAT volume, copy a small MP3 or conservative video onto it, and enable that source in **Device settings**.
3. Pick the source, press Down to enter its files and Cross to open a file. Start opens settings from the browser; Triangle stops playback.
4. If it fails, keep the file unchanged and follow [the reporting checklist](https://nathanneurotic.github.io/Simple-Media-System/troubleshooting.html#reporting).

Read the [controls](https://nathanneurotic.github.io/Simple-Media-System/controls.html), [settings reference](https://nathanneurotic.github.io/Simple-Media-System/settings.html) and [language guide](https://nathanneurotic.github.io/Simple-Media-System/languages.html) when you are ready to customize. [Credits and preserved historical manuals](https://nathanneurotic.github.io/Simple-Media-System/credits.html) retain the project's lineage.

---
[Searchable manual](https://nathanneurotic.github.io/Simple-Media-System/index.html) · [Edit the maintained source](https://github.com/NathanNeurotic/Simple-Media-System/blob/master/docs-site/content/index.html)
