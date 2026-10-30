<img width="400" height="92" alt="ANII-AI? This Software is NOT For You. Please Do NOT Use It." src="https://github.com/user-attachments/assets/55d2e5b6-77ca-434d-bc77-e45cf2755534" />

# Simple Media System for PlayStation 2

A maintained, storage-focused fork of Eugene Plotnikov's **SMS**: video, music,
JPEG pictures, subtitles and playlists on a PS2 that can run homebrew. The fork
adds MX4SIO, ATA/exFAT HDD, i.LINK, MMCE, folder-served UDPFS, on-screen network
configuration, USB/Bluetooth DS3/DS4 support and a desktop media converter.

**[Read the manual](https://nathanneurotic.github.io/Simple-Media-System/)** ·
**[Download rolling](https://github.com/NathanNeurotic/Simple-Media-System/releases/tag/rolling)** ·
**[Native wiki](https://github.com/NathanNeurotic/Simple-Media-System/wiki)** ·
**[Known issues](https://nathanneurotic.github.io/Simple-Media-System/known-issues.html)**

## Pick the right build

| Channel | What it contains |
|---|---|
| **Rolling development** | Current master fixes/features, player tester bundle, matching language pack, and Windows/Win7/Linux converter packages. Keep `build-info.json` with a test report. |
| **[v3.0.0-rev1 stable tag](https://github.com/NathanNeurotic/Simple-Media-System/releases/tag/v3.0.0-rev1)** | The fixed July 2026 release. It predates October's parser, streaming, frame-ownership, shuffle and converter changes. |
| **Actions / PR artifact** | A specific candidate that may not yet be in rolling. Record the run, artifact and source/head SHA. |

[PR #30](https://github.com/NathanNeurotic/Simple-Media-System/pull/30) merged on
2026-10-27 as `4a6959a`, retaining eggcatroll's continuation and adding MPEG RGB
ownership and shutdown cleanup. Host regressions, PS2 builds and controlled
emulator probes passed. **Affected-console reports remain open**: this is not a
claim that the exact color, USB-stutter, MP4-stall or skin-restart cases are resolved.

## First playback

1. Download `Simple-Media-System.zip` from rolling. Use the standard
   `Simple-Media-System.elf` first; the bundle also includes packed and DIAG builds.
2. Launch it from your existing homebrew loader, such as Launchstation/wLaunchELF
   or your configured boot menu. SMS does not install the exploit/loader for you.
3. Put a small supported file on an existing compatible FAT32/exFAT device. In
   **Device settings**, start that source or enable its Autostart and save settings.
4. Select the device, press Down to enter the file list and Cross to open a file.
   Triangle stops playback. [Full walkthrough](https://nathanneurotic.github.io/Simple-Media-System/getting-started.html).

The player embeds its IOP modules; ordinary installation needs no loose IRX files.
`SMS.cfg` normally follows a remountable launch directory, even on a memory-card
boot. `SMS.smb` follows the resolved config directory; `IPCONFIG.DAT` has separate
card/boot-folder rules. [Config paths](https://nathanneurotic.github.io/Simple-Media-System/configuration.html).

## Playback compatibility

| Media | Supported routes |
|---|---|
| Video | MPEG-4 Part 2 / DivX / Xvid / MS MPEG-4 v3 in AVI; `mp4v` in MP4; MPEG-1/2 in MPEG program streams. |
| Standalone audio | MP2/MP3/MPA, AAC/ADTS/M4A, AC-3, mono/stereo FLAC, Ogg Vorbis and WMA v1/v2. Container mappings and channel/sample limits still apply. |
| Pictures / playlists | Baseline JPEG and M3U. |
| Subtitles | External SRT/SUB/TXT text; existing AVI DXSB bitmap subtitles. Converter text burn-in is a separate feature. |

H.264/AVC, H.265/HEVC, VP9, AV1 and WMV video are not decoded. A familiar
extension is not enough: inspect the actual codec.

| Constraint | Current implementation |
|---|---|
| Video dimensions | At most 1024 pixels on either axis; this is an acceptance check, not a speed guarantee. |
| MPEG-1/2 rate table | 23.976, 24, 25, 29.97, 30, 50, 59.94 and 60 fps. |
| Streams | Eight table slots. MP4/MPEG-PS reserve a terminator and retain at most seven streams; the converter allows video plus six audio tracks. |
| Compressed packets | Video: 2,097,088 bytes; audio: 524,224 bytes. |
| Individual files | **Below 2 GiB for portable playback**, including on exFAT. Common fio/fileXio paths use signed 32-bit seek offsets. |

Earlier project documentation records standard-definition playback profiles, including
640×480 / 30 fps Xvid+MP3 and MPEG-2+MP2. These are historical reported baselines,
not a fresh qualification of every file or the current merge. The generated
[test-media pack](https://github.com/NathanNeurotic/Simple-Media-System/releases/tag/test-media)
defines repeatable inputs; its CI generation does not run a console.

See [video](https://nathanneurotic.github.io/Simple-Media-System/video.html),
[audio](https://nathanneurotic.github.io/Simple-Media-System/audio.html) and
[current issue status](https://nathanneurotic.github.io/Simple-Media-System/known-issues.html).
tktagmedia's reported 960×544 MPEG-2/MX4SIO profile is preserved with its actual
[settings and limitations](https://nathanneurotic.github.io/Simple-Media-System/video.html#community-mpeg2),
including the script's aspect/frame-rate behavior. It is not a universal default.

## Storage, settings and conversion

USB, MX4SIO, ATA/exFAT and i.LINK share BDM `massN:` naming; MMCE uses
`mmce0:`/`mmce1:`; APA/PFS, discs, SMB, UDPFS and developer `host:` are separate
paths. Mass numbering can change with discovery order.

SMS's current drivers enforce **MX4SIO versus MMCE** and **APA HDD versus ATA/BDM
HDD** exclusions. SMB/HOST versus UDPFS requires an explicit IOP restart or
relaunch; changing Autostart alone does not unload the running stack. An IOP
restart also invalidates storage mounts. [Devices](https://nathanneurotic.github.io/Simple-Media-System/devices.html)
· [Network setup](https://nathanneurotic.github.io/Simple-Media-System/network.html).

For folder shuffle, enable **Player settings → MP3 settings → Randomize playlist**,
then use **Play all → Video** or Audio. The preference also affects video folders.

Install language packs from the same build as the ELF. Current packs have 311
strings; Spanish was updated in the October continuation. The UI uses single-byte
code pages, while subtitles have separate UTF-8/MBCS handling.
[Language guide](https://nathanneurotic.github.io/Simple-Media-System/languages.html).

Rolling converter packages target Windows 10/11, Windows 7 and Linux, with
FFmpeg/FFprobe required. Features include two-pass encoding, multi-audio output,
text-subtitle burn-in, source-SD sizing and a high-quality Xvid/GOP6/PCM16 preset.
**PASS is not a console result**; WARN can mean incomplete bounded packet
inspection. Test a short output before converting a library.
[Converter guide](https://nathanneurotic.github.io/Simple-Media-System/converter.html).

## Development and documentation

The EE application retains `ps2dev/ps2dev:v1.0` / EE GCC 3.2.3. Normal builds use
committed compressed IOP modules; rebuilding drivers is a separate workflow.
See [building](https://nathanneurotic.github.io/Simple-Media-System/building.html)
and [architecture/testing](https://nathanneurotic.github.io/Simple-Media-System/developers.html).

The maintained site source is `docs-site/content/`, with shared navigation/assets.
Generate and check the published `/docs` site and versioned `wiki/` mirror with:

```sh
python docs-site/build.py
python docs-site/build.py --check
python -m http.server 8000 --directory docs
```

[Maintainer workflow](docs-site/README.md). Historical DocBook XML and the original
README/changelog are preserved separately from current user guidance.

## Credits & lineage

This edition builds directly on the original **Simple Media System** by **Eugene Plotnikov** and the ps2dev / ps2homebrew community, and on the MX4SIO groundwork from KrahJohlito's SMS branch. All original code, authorship and licensing (Academic Free License) are retained unchanged. The complete original README is preserved in full below.

### Testing & diagnosis

Homebrew runs on hardware no single person owns, and a fault nobody can reproduce is a fault nobody can fix. These people supplied the half of the work that isn't code:

- **Nadwiślański** *(Berion)* — graphics and interface work, and the sustained visual feedback behind the current theme, icons, splash and menu layout.
- **GhostTownUS** — systematic hardware testing on PS2 and PSX (DESR), with recorded sessions rather than descriptions. Traced the memory-card-less IP problem, and captured the screen that localised the PSX boot hang to a single step.
- **israpps** — root-caused the PSX boot failure from a DESR-5000 ROM dump: the DESR BIOS has no `rom0:LIBSD`, only the XMB and testmode variants. That one observation turned an unreproducible hang into a one-line diagnosis, and is why SMS now ships its own sound driver.
- **TnA** — encoder expertise behind the Media Converter: multi-pass encoding, the B-frames / keyframe-interval distinction, and the push that got real Xvid used where the preset claimed it.

---

Additional October contributors include **eggcatroll** (parser/stream/skin/playlist
continuation and regressions), **Nacheras** (Spanish strings and feedback), and the
reporters supplying exact-file, log and console comparisons. See the
[complete credits](https://nathanneurotic.github.io/Simple-Media-System/credits.html).

---

> ### 📜 Original project README
>
> Everything **below this line** is the original, unmodified README of Simple Media System, preserved in full as part of the project's history and out of respect for how well it was written. **Only the sections above reflect this fork's changes.**

---

# Simple Media System (SMS)

Copyright 2001-2005, ps2dev - http://www.ps2dev.org
All rights reserved.
Created by Eugene Plotnikov <e-plotnikov@operamail.com>

[SMS Documentation](https://ps2homebrew.github.io/SMS/)

[![CI](https://github.com/ps2homebrew/SMS/actions/workflows/compilation.yml/badge.svg)](https://github.com/ps2homebrew/SMS/actions?query=workflow%3ACI-compile)

## Introduction

Simple Media System (SMS) is a result of my curiosity about Sony's Playstation2, MIPS, DivX and MP3. My original goal was to create a simplest DivX player able to play good resolution movies at good frame rate on the unmodded PS2 without any extra equipment such as HDD and network adapter.

Why "system"? Just because having only a player program is not enough. Since PS2 is forced to ignore "illegal" disks, I've took an idea of Drakonite with his UMCDR and developed my own "UMCDR". (I called it CDDAFS). It just stores data on CD audio disk, which can be read on
(I hope) any PS2. So this is a first component of SMS. Second component is a PC program that makes cue/bin files in order to burn such a disk. And, finally, comes a player program itself. This system is not complete yet, but it can play DivX movies with sound at good frame rate already. Player's base is a famous ffmpeg project. I took just some parts of it (DivX 5XX and MP3) and made some modifications specific to PS2 hardware. I've tested it with a couple of DivX/XviD movies with different screen resolutions and encoding methods and it seems to be working. So, I've decided to put all this stuff to the CVS at ps2dev in hope that it could be interesting for PS2 enthusiasts who use it as base for their development. This "document" provides basic info about SMS.

## CDDAFS

It's just a method of storing a data on audio CD, so PS2 hardware can authenticate such a CD as "valid" one. Logical structure of such a CD is quite simple. Audio tracks are "directories". Track 0 is a control track, which stores global disk information.

Following "diagram" presents a logical disk structure:

    Track 0:
    GUID             - 16  bytes
    Disk name        - 64  bytes
    Disk description - 512 bytes
    Disk picture     - 9216 bytes (48x48x4 raw RGB image)[^1]
    ; then follows image table
    Number of images - 2 bytes (short, little endian)[^1]
    Image 1          - 4096 bytes (32*32*4 raw RGB image)[^1]
    .......
    Image N          - 4096 bytes (32*32*4 raw RGB image)[^1]
    ; then follows "directory" table (number of directories is number of tracks from CD TOC minus one)
    Track name        1 - 32 bytes
    Track image index 1 -  2 bytes (short, little endian, zero based)
    Reserved          1 - 30 bytes (just for possible future enhancements)
    Track name        2 - 32 bytes
    Track image index 2 -  2 bytes (short, little endian, zero based)
    Reserved          2 - 30 bytes (just for possible future enhancements)
    .......
    Other tracks:
    ; Directory entry 1
    Number of files     -  2 bytes (short, little endian)
    File name           - 32 bytes
    File image index    -  2 bytes (short, little endian, zero based)
    Reserved            - 30 bytes
    Start offset        -  4 bytes (physical sector relative to start sector of the current track, int, little endian, zero based)
    File size(in bytes) -  4 bytes (int, little endian)
    ; Directory entry 2
    ; etc.
    ; File data

[^1]: these "pictures" and "images" are in fact RGBA raw bitmaps, so future browser can be decorated with them;

Such a disk (without extra ECC codes) proved reliable enough in my configuration.
Source code that handles CD I/O located in CDDA.c (low level sector read operations) and in FileContext.c (logical read operations). I've also started implementation of "standard" I/O (STIO_xxx routines). Well, for me CDDA is enough. So anyone can finish "standard" I/O (perhaps, using asynchronous fileXio routines from ps2sdk).

## PC program

I've called it CDXplorer. Not fully completed yet, but it does its job of
making cue/bin pair. This program is uses Win32 API, so it is not compatible with linux (even WINE has troubles running it). I'm using Nero's Disk-At-Once/96 burning option. It is also possible to read CDDAFS using this proggy. Just insert CDDAFS disk in the drive and run the program. Maybe someone would like to create fs driver to support it :)?

## SMS Player

Just a test program for now and PS2 optimized ffmpeg. No GUI. It is possible to benchmark video/audio decoding process (I've got sometimes peak ~70 fps with 640x464 movie (~60fps is average)). I was able to play 720x528 DivX with sound at 25fps (but in this case there're slight sound distortions on scenes with a lot of motions (mainly "global" ones). Just watching a movie is also possible. Modify main's source at your taste. For sound output I'm using 'audsrv' 0.75 (magnificent work of gawd).

## ffmpeg modifications

Two things here. Using IPU for colorspace conversion (interrupt driven process that performs CSC->Display sequence for 640x464 picture in just ~4ms). This process goes in parallel with video/audio decoding. Second, I've made a transition from planar image format to the packed one, so video decoder produces macroblocks in IPU format.
This notable reduced data traffic between main CPU and memory. SPR is also
heavily used in conjunction with DMA transfers. This things gave ~300% performance boost (comparing to the original ffmpeg) in video decoding. MP3 decoder is virtually unmodified.

## Things to do
 ----------------------------------------------------------------------
There still a lot to do. As I'm a professional programmer, I just can't do this alone quickly. SMS is just a working skeleton of the future player. I've created it in my own free time. The following things I'd like to do with it:
- more testing (I've tested it with just about a dozen of DivX/XviD movies);
- more optimizations (mainly for GMC decoding. I've made just an 'ad hoc' code to show that it works. But this code is ugly and slow). QPel works, but it is unoptimized too (just an original ffmpeg). The MP3 decoder from ffmpeg is also almost untouched;
- better A/V synchronization. In fact there's no synchronization at all.
  But because multithreading on PS2 is not preemptive it happens "automatically" with a surprisingly good accuracy :). The only thing to fix was initial "audio preload". I'm not sure if I made it correctly, but it seems to be working at the moment. Realtime clock with 1ms resolution is also implemented, so it could be used for more precise synchronization;
- creation of the GUI. In case of CDDAFS it would be just a disk browser.
  There's already a base for it. There're routines to use ROM font (FONTM) and display icons from CDDAFS disk. There're also routines to load directory/file lists. The only thing missing is a pad management and an event loop;
- implementation of pause/stop functionality; As movie starts, there's no way back now;
- error checking (there're minimal checks only, so if, for example, press 'eject' CD button, then...a very interesting effect can be achieved :);
- btw, can somebody modify Makefile in order to make whole project more structured (all these 'ee', 'iop' etc. directories)?

## Restrictions
 ----------------------------------------------------------------------
- no support for interlaced movies;
- no support for non-interleaved avi files;
- only CodecID_MPEG4 support (that is DivX 5XX/XviD (I never, however, tried these 'MP4S', 'BLZ0' etc. (what are they?)) and MP3;
- other possible restrictions due to lack of detailed knowledge about things :);

## How to compile
 ----------------------------------------------------------------------
I'm using latest toolchain/ps2sdk from ps2dev and Win32 platform. Just 'make'.
Do not use 'old' ps2sdk (earlier than january 2005), since there's a bug in 'realloc', so program crashes immediately. In my setup I use ps2link at PS2 side and InLink at the PC. Note also that performace depends on PS2 cache usage, so even changing link order in Makefile can get more than 10% penalty (Currently Makefile contains 'best' link order (I think so). Pay attention to the location of 'audsrv.irx' (currently 'host:audsrv.irx').

## How to debug
 ----------------------------------------------------------------------
Crazy enough, but I've made the code Win32 compatible (with exception of some files). So, I'm using MSVC6 to debug major bugs, memory leaks and to understand ffmpeg's code. 'printf' does debug for PS2 :);

## How to make modifications
 ----------------------------------------------------------------------
At your taste (be careful, any alignment failure causes a crash (especially with structures allocated dynamically)). But first compile and run existing code (the only thing to modify is a filename in 'main.c'). To do that it's necessary to waste at least one CD-R for CDDAFS. Use CDXplorer application to make cue/bin pair. It is possible to copy files back from CDDAFS disk. Drag-and-drop is supported (one direction only: to the application). Context menus are also available. Number of directories has upper limit of 99 (only one level), and number of files is restricted by the CD capacity (I've put a 800MB movie, for example, on the ordinary 700MB disk without any problem). I have SCPH-30004R PAL unit, so it works at least on this model.

## Modified ps2sdk code
 ----------------------------------------------------------------------
It could be noted that there's a lot of modified ps2sdk code here. Why?

I wanted maximum performance. For 'audsrv' for example there's an unnecessary (I think so) calls for memcpy and FlushCache. 'libcdvd' caused strange problems of data corruprion when SifLoadModule was called. GSkit uses SPR badly needed for video decoding. Instead, I decode MP3 stream directly to the uncached, 64 byte aligned memory, I'm using 64 byte aligned buffers for disk I/O, and GS kit was modified to avoid SPR usage. Maybe I'm just a stupid person :o).

## License
 ----------------------------------------------------------------------
For my code it is the same as for ps2sdk (AFL). For ffmpeg - "GNU Lesser General Public License as published by the Free Software Foundation;
either version 2 of the License, or (at your option) any later version" :).

## Miscellaneous
 ----------------------------------------------------------------------
I've made this just for myself. I don't want to make any profit from it. Anyone is free to make anything with it (according to aforementioned licenses). If my code sucks (especially my formatting habits) - it sucks. I don't care. I just hope that it will help PS2 enthusiasts to make their programs quicker. I will continue slowly to make it more like a real system, but for me alone
it will take ages to finish it :).

## Questions
 ----------------------------------------------------------------------
- by email (see beginning of file);
- using private messages at ps2dev's forums;

## Thanks (in no particular order) ;o)
 ----------------------------------------------------------------------
- Sony for PS2 and Linux for it;
- Marcus R. Brown for discovering an exploit (without it I would never begin);
- Drakonite for UMCDR idea;
- Pixel for making an estimation of the code and for helping me out with CVS;
- J.F. for trying this out (btw. I don't know if you succeeded with it or not);
- PS2Reality team for inspiration and timer code;
- ffmpeg contributors;
- Chris "Neovanglist" Gilbert for GS and DMA code;
- gawd for 'audsrv' (virgin version was enough :));
- all others whose posts in the forums helped me in development;

[Changelog](CHANGELOG.md)

[Documentation](docs/README.md)
