# Credits & Sources

Simple Media System is a community project with a long history. Every feature in this build stands on someone else's code and someone else's ideas — a media engine written two decades ago, a homebrew community that kept it alive and modern, and the storage-integration groundwork this fork was able to build on. This page names the people and projects it is grateful to, and cites its sources.

CREDITS

## Built on twenty years of shared work

SMS began as one developer's quest to play good-looking video on an unmodified PlayStation 2, and became a community effort spanning USB, HDD, network, a GUI, a skin engine and a long list of formats. This fork adds storage breadth — MX4SIO playback, exFAT, i.LINK, MMCE, SMB and UDPFS — plus DualShock 3/4 support. None of it would exist in isolation.

## The foundation

The heart of SMS is **Eugene Plotnikov**'s original media player and the years of [ps2homebrew](https://github.com/ps2homebrew) community work that modernised it. This fork's contribution is a focused one — making **MX4SIO** microSD playback work reliably and adding several more storage devices — but it rests entirely on that shared foundation.

Eugene Plotnikov

The original author of Simple Media System (2001–2005). He built the PS2-optimised, FFmpeg-derived video/audio player that uses the console's IPU for colour conversion, and devised **CDDAFS** — the technique for storing data on an audio CD so an unmodified PS2 accepts a burned disc. Everything SMS is began here.

The ps2homebrew community

The maintainers and contributors who adopted and modernised SMS over many years — adding USB, internal-HDD, network (SMB) and playlist support, a GUI file browser, the `.smi` skin engine, and support for many more container and codec formats. The PS2SDK, the IOP module ecosystem, and the toolchain this fork builds with all come from that same open, community-maintained work.

KrahJohlito

Provided the **BDM + MX4SIO integration foundation** this fork builds on: loading the block-device (BDM) module stack, the memory-card/pad rework that a shared SIO2 bus requires, and the device-menu wiring that lets MX4SIO appear as a first-class device. This groundwork is what made the playback fix possible to land.

El_isra, Ripto & Berion

Additional work and testing around the MX4SIO effort. Real-hardware testing and feedback are what turn a build that merely compiles into one that actually plays — and this fork's MX4SIO support was shaped by that hands-on QA.

bbsan2k

Author of the **MMCE (Memory Card Mass Storage)** protocol — the protocol behind loading files from an **SD2PSX** or **MemCard PRO2** through the PS2's memory-card slot. This fork's `mmce0:` / `mmce1:` device support builds directly on that work. See [MMCE](https://nathanneurotic.github.io/Simple-Media-System/mmce.html).

rickgaiser, Maximus32 & the neutrino contributors

The **UDPFS / UDPRDMA**, ministack and SMAP foundation vendored from [neutrino](https://github.com/rickgaiser/neutrino). The working load sequence was cross-checked against [wLaunchELF-R3Z](https://github.com/saildot4k/wLaunchELF_R3Z). SMS carries documented local integration changes while preserving upstream attribution.

The Open PS2 Loader contributors

The DS3/DS4 USB and Bluetooth driver/client foundation vendored from [Open PS2 Loader](https://github.com/ps2homebrew/Open-PS2-Loader), adapted here for SMS's controller path and in-app Bluetooth pairing.

The FFmpeg project

SMS's video and audio decoders trace their heritage to **FFmpeg**. Plotnikov's original player was a heavily PS2-optimised port of parts of FFmpeg, and the decoder lineage — for AVI/MP4 MPEG-4, MPEG program streams, MP3, AAC, AC3, FLAC, OGG and more — reaches back to that project's contributors.

This fork

The storage-focused work documented on this site: the **MX4SIO microSD playback fix** (the headline feature), **exFAT internal HDD over ATA** alongside classic APA, **i.LINK** storage, **MMCE** devices, the **UDPFS network drive**, fully on-screen **network & SMB setup**, DualShock 3/4 integration, config that saves to the boot device, MP4 support, a refreshed theme and device icons, and this documentation.

## What this fork contributes

To be precise about which parts are new here versus inherited: this fork's own work is the storage-breadth layer and the playback fix that headlines it. At a user level, that means:

- **MX4SIO microSD playback** — video and audio now play smoothly from a microSD in an MX4SIO adapter, with a hardened driver that keeps playback reliable under a media player's load. See [MX4SIO](https://nathanneurotic.github.io/Simple-Media-System/mx4sio.html).
- **exFAT internal HDD over ATA** — modern exFAT drives read alongside the classic APA/PFS APA/PFS partitions. See [Internal HDD](https://nathanneurotic.github.io/Simple-Media-System/hdd.html).
- **i.LINK (IEEE 1394) storage** — a FireWire drive as a media source. See [i.LINK](https://nathanneurotic.github.io/Simple-Media-System/ilink.html).
- **MMCE devices** — browsing media from an SD2PSX or MemCard PRO2 in the memory-card slot. See [MMCE](https://nathanneurotic.github.io/Simple-Media-System/mmce.html).
- **UDPFS network drive** — a folder-served network filesystem at `udpfs:`, distinct from UDPBD. See [Network Setup](https://nathanneurotic.github.io/Simple-Media-System/network.html#udpfs).
- **On-screen network & SMB setup** — configure your IP and connect to a share entirely from the pad, no hand-edited config files. See [Network Setup](https://nathanneurotic.github.io/Simple-Media-System/network.html).
- **DualShock 3 / 4 support** — wired USB and Bluetooth input, with controller pairing inside SMS.
- **MPEG-4 Part 2 MP4 playback** — `mp4v` video with AAC audio, covered by the hardware test pack.
- **Settings that save anywhere** — configuration persists to the memory card or to the device SMS booted from. See [Settings](https://nathanneurotic.github.io/Simple-Media-System/settings.html).
- **A refreshed theme** — new desktop artwork and device icons, on the existing skin engine. See [Themes & Skins](https://nathanneurotic.github.io/Simple-Media-System/themes.html).

## Testing & diagnosis

Some of the most valuable work on this fork was not writing code. Homebrew runs on hardware nobody has all of, and a fault nobody can reproduce is a fault nobody can fix. These people supplied the missing half.

- **Nadwiślański (Berion)** — the fork's graphics and interface work, and the sustained visual feedback behind the current theme, icons, splash and menu layout.
- **GhostTownUS** — systematic hardware testing on PS2 and PSX (DESR), with recorded sessions rather than descriptions. Traced the memory-card-less IP problem, and the screen capture that localised the PSX boot hang to a single step. Also spotted that wLaunchELF R3Z keeps its boot driver loaded where SMS resets the IOP — which is why config beside the ELF had to be read at a different point in boot.
- **israpps** — root-caused the PSX boot failure from a DESR‑5000 ROM dump: the DESR BIOS has no `rom0:LIBSD`, only the XMB and testmode variants. That single observation turned an unreproducible hang into a one-line diagnosis, and is why SMS now ships its own sound driver instead of relying on the console's.
- **TnA** — encoder expertise behind the Media Converter: multi-pass encoding, the distinction between B‑frames and the keyframe interval, and the push that led to real Xvid being used where the preset claimed it.

## From the original SMS thanks

The original SMS `README` records the people and projects Eugene Plotnikov thanked during the project's early development. A few relevant to what SMS is today:

| Credited | For |
| --- | --- |
| Marcus R. Brown | The early exploit work that made PS2 homebrew possible in the first place |
| The PS2Reality team | Inspiration and timer code |
| ffmpeg contributors | The decoder heritage SMS's player is derived from |
| Chris "Neovanglist" Gilbert | GS and DMA code |
| gawd | The `audsrv` audio server |

SMS was created by Eugene Plotnikov (copyright 2001–2005, ps2dev) and is documented by the ps2homebrew community. See the original project's [SMS documentation](https://ps2homebrew.github.io/SMS/) for its full history.

## A note on trademarks & affiliation

ℹ A community project, unaffiliated with Sony

 Simple Media System is a free, open-source, community project. **PlayStation 2** is a trademark of Sony Interactive Entertainment; this project and this fork are **unaffiliated** with Sony and are not official Sony software. Device and product names such as **SD2PSX**, **MemCard PRO2** and **MX4SIO** belong to their respective owners and are referenced only to describe the hardware SMS works with.

## Sister documentation

This site is one of a small family of PlayStation 2 homebrew documentation projects. If you are here for SMS you may also want:

- [POPStarter docs ↗ PS1 / POPSTARTER reference](https://nathanneurotic.github.io/POPSLoader/)
- [RiptOPL docs ↗ Open PS2 Loader fork reference](https://nathanneurotic.github.io/Open-PS2-Loader/)

<details><summary>Why is this a fork, and where does the canonical SMS live?</summary>

The community-maintained SMS project lives with the [ps2homebrew](https://github.com/ps2homebrew) organisation, and its documentation is at [ps2homebrew.github.io/SMS](https://ps2homebrew.github.io/SMS/). This documentation covers a **downstream fork** focused on storage breadth — it makes MX4SIO microSD playback work and adds exFAT/ATA HDD, i.LINK, MMCE and UDPFS devices, moves network and SMB setup fully on-screen, adds DualShock 3/4 support, and refreshes the look. It is a labour of love built on the work of everyone named above; the long-term home of SMS remains the community project.

</details>

## October continuation and documentation

- **eggcatroll** — the parser, stream, skin, playlist and regression continuation incorporated before PR #30 merged.
- **Nacheras** — updated Spanish strings and skin/language feedback.
- **tktagmedia** — the reported high-resolution MPEG-2/MX4SIO conversion profile, retained with its settings and limits.
- Issue reporters supplied videos, logs, hardware comparisons and reproductions that remain essential to acceptance.

The documentation theme shares the black/cyan PS2 design of the [Launchstation](https://nathanneurotic.github.io/Launchstation/), [POPSLoader](https://nathanneurotic.github.io/POPSLoader/) and [RiptOPL](https://nathanneurotic.github.io/Open-PS2-Loader/) sites. Existing SMS artwork is reused; the original manual/XML, screenshots, historical downloads and original README/changelog sections are preserved.

## Source and evidence map

Use [Known Issues & Testing](https://nathanneurotic.github.io/Simple-Media-System/known-issues.html) for the dated report status, [developer notes](https://nathanneurotic.github.io/Simple-Media-System/developers.html) for code/test boundaries and the [documentation audit record](https://github.com/NathanNeurotic/Simple-Media-System/blob/master/docs/investigations/2026-10-27-documentation-audit.md) for the page-by-page coverage. A previous documentation claim of hardware success is retained as a historical report unless this audit has direct repeat evidence.

---
[Searchable manual](https://nathanneurotic.github.io/Simple-Media-System/credits.html) · [Edit the maintained source](https://github.com/NathanNeurotic/Simple-Media-System/blob/master/docs-site/content/credits.html)
