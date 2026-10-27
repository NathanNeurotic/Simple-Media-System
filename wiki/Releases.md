# Downloads & Releases

Choose a build deliberately. Rolling follows development; version tags preserve an earlier release. The filename and in-app version are not enough to identify an October test build.

## Choose your build

| Channel | Use it for | Identity |
| --- | --- | --- |
| [Rolling development](https://github.com/NathanNeurotic/Simple-Media-System/releases/tag/rolling) | Current fixes, features and issue retests. Updated by successful master publication. | The release notes' commit and the tester bundle's `build-info.json`. |
| [v3.0.0-rev1 stable tag](https://github.com/NathanNeurotic/Simple-Media-System/releases/tag/v3.0.0-rev1) | The fixed July 2026 release. It predates the October fixes and converter additions. | Tag/source `95b803ba07d1c55aa644acbd633936eb0a1e6ad6`; published 2026-07-26 UTC. |
| [Actions / PR artifacts](https://github.com/NathanNeurotic/Simple-Media-System/actions) | Testing a particular unmerged candidate. | Exact run, artifact and manifest head/source SHA. A rolling download cannot test an unmerged PR. |

The [latest stable release link](https://github.com/NathanNeurotic/Simple-Media-System/releases/latest) excludes prereleases, so it can still lead to July while rolling contains much newer work.

## Rolling download contents

| File | Purpose |
| --- | --- |
| [Simple-Media-System.zip](https://github.com/NathanNeurotic/Simple-Media-System/releases/download/rolling/Simple-Media-System.zip) | Standard, packed and DIAG ELFs, build manifest and SMB component notices. |
| `Simple-Media-System.elf` | Standard player; use this for ordinary playback and first comparisons. |
| `Simple-Media-System-packed.elf` | The same player in a self-extracting executable. Use the standard ELF if your loader has trouble with packed files. |
| `Simple-Media-System-DIAG.elf` | On-screen startup/probe labels for reporting freezes. Record the number *and* label. |
| [Simple-Media-System-languages.zip](https://github.com/NathanNeurotic/Simple-Media-System/releases/download/rolling/Simple-Media-System-languages.zip) | Language variants matching this build's string table, including Spanish. See [installation](https://nathanneurotic.github.io/Simple-Media-System/languages.html). |
| [SMS-Media-Converter.exe](https://github.com/NathanNeurotic/Simple-Media-System/releases/download/rolling/SMS-Media-Converter.exe) | Windows 10/11 converter. |
| [SMS-Media-Converter-Win7.exe](https://github.com/NathanNeurotic/Simple-Media-System/releases/download/rolling/SMS-Media-Converter-Win7.exe) | Python 3.8 / Qt5 build for Windows 7. Packaging CI runs on a current Windows runner; it is not live Win7 acceptance. |
| [SMS-Media-Converter-Linux.zip](https://github.com/NathanNeurotic/Simple-Media-System/releases/download/rolling/SMS-Media-Converter-Linux.zip) | Linux source/application package; follow the included Python/PySide instructions. FFmpeg/FFprobe are separate tools. |

Embedded IOP modules are carried inside the player ELF; normal installation does not require copying loose IRX files. Keep matching translations with each build you compare.

## What the current development line contains

[PR #30](https://github.com/NathanNeurotic/Simple-Media-System/pull/30) merged on 2026-10-27 as [4a6959a](https://github.com/NathanNeurotic/Simple-Media-System/commit/4a6959a709b9679352f964fd412a69e539ef49ae). It includes MPEG-PS/MOV probing and progress fixes, stream failure recovery, MPEG RGB-frame ownership and shutdown cleanup, folder shuffle, Spanish integration and converter controls. See the [open acceptance work](https://nathanneurotic.github.io/Simple-Media-System/known-issues.html) before treating a reported issue as resolved.

### Verify the file you actually downloaded

Rolling assets are replaced over time. Read `build-info.json` inside the tester ZIP and keep it with your test report; it contains source/head commits, the Actions run and ELF SHA-256 hashes. Compare a downloaded ELF with those hashes, rather than a hash copied from an older guide.

```
sha256sum Simple-Media-System.elf
# PowerShell:
Get-FileHash .\Simple-Media-System.elf -Algorithm SHA256
```

The July stable release retains its own asset hashes on its release page. A source-code archive from a mutable rolling tag is not a substitute for the tester bundle's build identity.

## What the validation establishes

The merged work passed host regression tests, target builds and controlled emulator probes. Those checks expose specific parser, I/O and frame-lifetime defects. They do not establish affected-console color, USB throughput, exact-file MP4/MPEG playback, skin restart, shuffle or audio-switching acceptance. All nine reported issues were still open at this audit.

## Synthetic test-media pack

The [test-media prerelease](https://github.com/NathanNeurotic/Simple-Media-System/releases/tag/test-media) is regenerated on master from [tools/gen-test-media.sh](https://github.com/NathanNeurotic/Simple-Media-System/blob/master/tools/gen-test-media.sh). It includes short codec fixtures and the MP4 matrix with a README. Generation is a host operation. Record your console, storage source, build and actual result when playing it.

## Historical changelog

SMS has a long history. It was created by **Eugene Plotnikov** as, in his own words, "the simplest DivX player able to play good-resolution movies at good frame rate on the unmodded PS2 without any extra equipment," and was developed intensively across 2005–2009 before the ps2homebrew community carried it forward. The high-level lineage below is drawn from the project's own `CHANGELOG.md` — refer to that file in the repository for the exhaustive, entry-by-entry history.

| Era | What arrived |
| --- | --- |
| Mid–2005 — foundations | The core DivX/XviD/MPEG-4 player using the PS2's IPU for colour conversion; **CDDAFS** (playing data off a burned audio CD on an unmodded console); early A/V synchronisation and V-Blank sync; internal **HDD** support via APA/PFS partitions; a heavily PS2-optimised FFmpeg-derived core. |
| Summer 2005 — the GUI | The three-area **GUI** that SMS still uses — device menu, file browser and status line — with pause / resume / stop, the file browser, and screen-alignment adjustments; **USB** mass-storage support; AC3 audio; config saved to the memory card. |
| Late 2005 — network & discs | **Network (host:)** playback; ISO CD/DVD and later **DVD-Video** disc reading on unmodded consoles; more codecs; skin/background-image support; a proper SMS menu and internationalised, scalable fonts. |
| Late 2005 / early 2006 — subtitles & playlists | External **SRT / SUB subtitles** with adjustable size and colour, plus manual A/V and subtitle sync; multi-audio-track support; the object-oriented desktop rewrite; early **MP3 / M3U** playback and "play all in folder". |
| 2006 — more formats & input | Remote-control support; **MPEG-1/2 program-stream** video; DTS and **OGG Vorbis** audio; on-disc file management (copy/delete on the internal HDD); pan-scan and display tuning; HD/DTV video-mode groundwork. |
| Late 2006 — SMB | The **SMB/CIFS** network client, so SMS could browse Windows / Samba shares over the network adapter; faster network transfers; UTF-8 subtitle support. |
| 2007 — HD video & WMA | **720p / 1080i** and VESA/DTV video modes; a new floating-point MPEG audio decoder; **WMA (ASF)** and raw PCM audio; multiple SMB servers; assorted decoder and sync refinements. |
| 2008 — audio breadth & pictures | **AAC / M4A** and **FLAC** audio; a baseline **JPEG** viewer with slideshow and rotate; big (4 GB) OpenDML AVI support; USB/FAT driver rewrites; an on-screen exception handler. |
| 2009 — later refinements | 576p video mode and 2:2 pulldown for smoother 24fps playback; audio-driver and AC3 downmix improvements; further USB/FAT speed work — the last stretch of the original development line before the project's community era. |
| **This fork — stable and rolling development** | **Working MX4SIO playback**, MP4/`mp4v` video, UDPFS, exFAT/ATA HDD, i.LINK, MMCE, DualShock 3/4 input, on-screen network/SMB setup, save-to-boot-device config, a refreshed theme and the reliability fixes listed [above](https://nathanneurotic.github.io/Simple-Media-System/releases.html#whats-new). |

ℹ Reading the full changelog

 The dates and one-liners above summarise a much larger record. The complete, dated changelog lives in the repository as `CHANGELOG.md`; open it there if you want the exact entry for a given feature or fix.

<details><summary>Why does this fork jump to version 3.0?</summary>

SMS versioning has never been a single tidy line — the original author noted early on that different "distributors" would put their own signature on their builds. This documentation set describes the build labelled **v3.0.0-rev1**, displayed by SMS as **Version 3.0 (Rev.1)**: a storage-focused fork that keeps SMS's decades-old playback while adding the devices, MP4 path and on-screen setup covered throughout this site. The version shows on-screen during startup, so you can always confirm what you are running.

</details>

<details><summary>Is this an official SMS release?</summary>

No. This is a community fork built on the work of many contributors — Eugene Plotnikov's original SMS, the ps2homebrew community's modernisation, and KrahJohlito's BDM/MX4SIO integration foundation among them. It is offered as-is, not as an official release. See [Credits & Sources](https://nathanneurotic.github.io/Simple-Media-System/credits.html) for the full acknowledgements.

</details>

## Legacy tools & downloads

Companion tools and files for classic SMS, preserved from the original [ps2-home.com](https://www.ps2-home.com/) forum posts so they don't vanish. They are **third-party works by their original authors**, kept here purely for archival — we didn't write them and can't vouch for the Windows binaries.

Third-party, as-is

 The `.exe` tools below are old Windows binaries (2005–2014) from the PS2 homebrew community, with no stated license; we have not run or vetted them, and antivirus may flag old packed executables — use at your own discretion. Credit belongs to their original authors. Some archives are `.rar` (open with 7-Zip / WinRAR).

| Download | What it is | Author / source |
| --- | --- | --- |
| [SMS manual (PDF)](https://nathanneurotic.github.io/Simple-Media-System/downloads/SMS-manual.pdf) | The classic Simple Media System user manual. | SMS community · ps2-home.com |
| [Language pack — SMS_lng09](https://nathanneurotic.github.io/Simple-Media-System/downloads/SMS_lng09.rar) | 20 community translations (`.lng`): BG, CA, CZ, DE, EN, ES, FR, HU, ID, IT, NL, PL, PT, PT-BR, RU, SE, SK, TR, UA. **Vintage 2009** — likely missing newer menu strings, so treat as a reference/starting point. | Translators credited in the pack's `README` (EN by Eugene Plotnikov) |
| [pic2smi](https://nathanneurotic.github.io/Simple-Media-System/downloads/pic2smi.rar) | Windows tool: converts a picture into an SMS `.smi` skin / background. | ps2-home.com |
| [smspal](https://nathanneurotic.github.io/Simple-Media-System/downloads/smspal.rar) | Windows tool: edit / generate an SMS palette (`SMS.pal`). | ps2-home.com |
| [Rad HOST Client 1.8a](https://nathanneurotic.github.io/Simple-Media-System/downloads/Rad-HOST-Client-1.8a.zip) | Windows PC tool: streams media to SMS over its `host:` connection. | ps2-home.com |

## Where to next

- [Install & First Boot Get the ELF running](https://nathanneurotic.github.io/Simple-Media-System/install.html)
- [MX4SIO The headline feature](https://nathanneurotic.github.io/Simple-Media-System/mx4sio.html)
- [Building from Source Compile it yourself](https://nathanneurotic.github.io/Simple-Media-System/building.html)
- [Credits & Sources Who built what](https://nathanneurotic.github.io/Simple-Media-System/credits.html)

---
[Searchable manual](https://nathanneurotic.github.io/Simple-Media-System/releases.html) · [Edit the maintained source](https://github.com/NathanNeurotic/Simple-Media-System/blob/master/docs-site/content/releases.html)
