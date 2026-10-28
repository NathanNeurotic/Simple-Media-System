# MX4SIO (microSD)

**MX4SIO** is a microSD adapter that plugs into the PlayStation 2's memory-card slot and talks to the console over its **SIO2** bus. This build of SMS mounts the card as a `massN:` device and plays media straight off it — video, audio and pictures — **reliably**. Working MX4SIO playback is the headline feature of this fork, and the card gets its own device icon in the menu so you can tell it apart from USB.

ℹ MX4SIO is not MMCE

 Two very different SD-card adapters live in the memory-card slot. **MX4SIO** is an SD/SPI adapter that SMS reads as a **BDM mass-storage device** (`massN:`), sharing that namespace with USB, exFAT HDD and i.LINK. An **MMCE** device (SD2PSX or MemCard PRO2) speaks a different protocol and mounts as `mmce0:` / `mmce1:`. Because both sit on the same SIO2 / memory-card bus, **only one can be active at a time**. If you own an SD2PSX or MemCard PRO2, see [MMCE (SD2PSX)](https://nathanneurotic.github.io/Simple-Media-System/mmce.html) instead.

## What MX4SIO is

An MX4SIO adapter takes a microSD card and presents it to the PS2 through the memory-card slot, communicating over the console's **SIO2** controller bus (the same bus the pad and memory card use). To SMS the card looks like any other **BDM** (Block Device Manager) mass-storage volume: it is mounted from the FAT/exFAT layer and surfaces as `massN:`, exactly the abstraction USB, exFAT-over-ATA and i.LINK all share. That means SMS does not need a special "MX4SIO file browser" — it simply browses the card's raw filesystem and plays whatever media files it finds, wherever they sit. There is no mandatory folder tree to build.

In the device menu the card appears with the same `massN:` naming as the other mass devices, where `N` is `0`, `1`, `2`… depending on what else is mounted. Point the browser at the card, walk into your folders, and open a file.

## Why it is the headline feature

Plenty of PS2 tools can *mount* an MX4SIO card and list its directories — but that is the easy part. A media player is different: it streams **large** reads continuously while the rest of the console is busy decoding audio, driving the GUI and polling the pad. Under exactly those conditions, earlier attempts to use MX4SIO in SMS would mount the card and show the file list, then **stall the moment you tried to actually play a file**.

This build fixes that. Video and audio now play smoothly from a microSD in an MX4SIO adapter — the same experience you get from USB or the internal HDD. To make it a first-class citizen, MX4SIO also gets:

- Its **own device icon** in the top device bar, visually distinct from the USB icon, so you always know which card you are browsing.
- An **Autostart MX4SIO** toggle in the settings menu, so it can come up automatically at boot if you want it to.
- A **Start MX4SIO support** item, always present in Device settings, so you can bring the card online on demand without rebooting.

MX4SIO and MMCE cannot both run

 MX4SIO and MMCE both use the SIO2 / memory-card bus, so SMS can only drive **one of them at a time**. Turn on Autostart for the one you actually use, and leave the other off. See [MX4SIO vs MMCE](https://nathanneurotic.github.io/Simple-Media-System/mx4sio.html#vs-mmce) below.

## Compatibility

SMS reads the card through the standard FAT/exFAT layer, so a microSD formatted as **FAT32** or **exFAT** will mount. Use FAT32 or exFAT supported by your device/firmware. exFAT filesystem support does not remove SMS's below-4-GiB limit for each media file. Either way, the media files themselves must be a format SMS can play — see [Video](https://nathanneurotic.github.io/Simple-Media-System/video.html), [Audio](https://nathanneurotic.github.io/Simple-Media-System/audio.html) and [Pictures](https://nathanneurotic.github.io/Simple-Media-System/pictures.html) for the full list (MPEG-4 Part 2 AVI, `mp4v` MP4, MPEG-1/2 program streams, MP2/MP3, AAC, AC3, FLAC, OGG, WMA, JPEG, and so on).

ℹ Hardware note

 Playback was confirmed on real PS2 hardware with a FAT32-formatted microSD in an MX4SIO adapter — a 640×480 XviD + MP3 AVI and a standalone MP3 both play smoothly. As with any SD adapter, cheap or worn cards can misbehave; if a particular card gives trouble, try a different, known-good one.

## Preparing the card

### Format the microSD

Use a compatible **FAT32** or **exFAT** volume. Keep each SMS media file below 4 GiB on either filesystem. Back up the contents before reformatting; an already compatible volume can be used as it is.

### Copy your media on

Copy your video, audio, picture and playlist files onto the card from your PC. You can organise them into folders however you like — SMS browses the raw filesystem, so there is no required directory layout. Keep external subtitle files (`.srt` / `.sub`) next to the video they belong to; see [Subtitles](https://nathanneurotic.github.io/Simple-Media-System/subtitles.html).

### Seat the adapter

With the card loaded, insert the MX4SIO adapter into one of the PS2's memory-card slots. Remember that this is the same slot family MMCE devices and the memory card use — plan around the one-device-at-a-time rule below.

### Boot SMS and enable MX4SIO

Boot the PS2 into SMS, then turn MX4SIO on (next section). On a clean install optional Autostart choices are off, so you choose what to bring up.

## Enabling MX4SIO

On a fresh install SMS leaves optional Autostart flags off — you switch on the devices you want. There are two ways to bring MX4SIO up.

### Autostart MX4SIO

In the settings menu, turn on **Autostart MX4SIO**. From then on SMS loads the MX4SIO driver at boot and the card is ready to browse as soon as the desktop appears. This is the option to use if MX4SIO is your everyday media source. Your choice is saved with the rest of your settings (see [Settings](https://nathanneurotic.github.io/Simple-Media-System/settings.html)), so it sticks across reboots.

### Start MX4SIO now

Device settings always lists **Start MX4SIO support**, so you can bring the card online on demand — no reboot needed. It is shown even when MX4SIO is already up (a harmless no-op) and even when MMCE holds the SIO2 bus, in which case it reports an error explaining why MX4SIO cannot start. This is handy when you didn't autostart it but decided you want it this session, or when you have just seated the adapter.

### Pick it in the device menu

Once MX4SIO is running, the card shows up in the **device menu** (the top row of device icons) with its own icon, separate from USB. Use **Left**/**Right** to land on it, then press **Down** to drop into the file list and start browsing. From there it behaves like any SMS browser: **Up**/**Down** to move, **L1**/**L2** to page-scroll, **Cross** to open a file or folder, and **Triangle** to step up a level (file list → parent → back to the device menu).

ℹ Telling MX4SIO apart from USB

 USB and MX4SIO both live in the `massN:` namespace, so under the hood they are the same kind of device. SMS keeps them visually distinct in the device bar by giving MX4SIO its **own icon** — so even with a USB stick and an MX4SIO card mounted at the same time, you can see at a glance which one you are on.

## MX4SIO vs MMCE

This is the one gotcha worth internalising. Both MX4SIO and MMCE devices physically occupy a memory-card slot and talk over the SIO2 bus — so SMS can only drive **one of them at a time**. They are not interchangeable, either: they use different drivers and mount under different names.

|  | MX4SIO | MMCE (SD2PSX / MemCard PRO2) |
| --- | --- | --- |
| Card type | microSD in an SD/SPI adapter | SD card in an SD2PSX or MemCard PRO2 |
| How SMS sees it | BDM mass storage — `massN:` | MMCE device — `mmce0:` / `mmce1:` |
| Shares the slot with | USB / HDD / i.LINK (the `massN:` pool) | Its own `mmce` namespace |
| Enable via | Autostart MX4SIO / Start MX4SIO now | Autostart MMCE / Start MMCE now |
| Device icon | Its own icon, distinct from USB | Its own icon |

Rule of thumb: turn on Autostart for the adapter you actually own, and leave the other off. If your device is an SD2PSX or MemCard PRO2, use the [MMCE](https://nathanneurotic.github.io/Simple-Media-System/mmce.html) page. If it's a plain SD-over-SIO2 adapter, you're in the right place.

The other exclusion — HDD paths

 Separately from the SIO2 rule, note that the two *internal-HDD* access paths — classic APA/PFS and exFAT over ATA (BDM) — both own the internal ATA bus, so only one of those can run at once either. That's unrelated to MX4SIO, but if you're enabling several devices it's worth knowing. See [Internal HDD](https://nathanneurotic.github.io/Simple-Media-System/hdd.html).

## How it works (at a glance)

You don't need to know any of this to use MX4SIO — but in short: a continuously-streaming media player asks a lot more of an SD-over-SIO2 card than a lightweight file manager does, because it reads constantly while also decoding audio and driving the GUI. This build carries the MX4SIO driver and access-path hardening needed to keep playback reliable under exactly those demands. It is entirely automatic — there is nothing to enable, tune, or configure.

ℹ Nothing to configure

 The reliability work is all under the hood; the deeper engineering details live in the project's developer notes, not here. To use MX4SIO you simply format the card, copy your media, enable MX4SIO, and play.

## Notes & tips

- MX4SIO reads any format SMS can play — it is a storage device, not a format. Point the browser at your files and open them like on any other device.
- You can browse into any folder structure; there is no mandatory layout. Keep `.srt`/`.sub` subtitle files beside their video.
- Enable **either** MX4SIO **or** MMCE, never both — they share the SIO2 slot bus.
- Prefer **FAT32** for maximum compatibility; use **exFAT** if supported by your setup. Keep individual SMS media files below 4 GiB on either filesystem.
- If a specific card acts up, try another known-good microSD before assuming a software fault.
- Your Autostart MX4SIO choice is saved with your settings and persists across reboots — see [Settings](https://nathanneurotic.github.io/Simple-Media-System/settings.html) for where the config lives.

<details><summary>Why is MX4SIO reliable here when it can be finicky elsewhere?</summary>

An SD-over-SIO2 card behaves differently in a continuously-streaming media player than in a lightweight file manager that reads a little and then sits idle. This build carries the MX4SIO driver and access-path hardening needed for the media-player case, so playback stays smooth even while SMS is busy decoding audio and drawing the GUI. To the user it simply works — the reliability work is entirely under the hood.

</details>

<details><summary>Do I have to build a folder tree like OPL&#x27;s DVD/CD/ART?</summary>

No. SMS is a media player, not a game loader — it browses the raw filesystem of the card and plays media files wherever they are. There is no OPL-style mandatory folder structure and no ISO/coverart convention to follow. Copy your files on, organise them however you like, and browse to them.

</details>

**File-size and test scope:** keep every SMS media file below 4 GiB, including on exFAT. Existing project reports do not qualify every drive/adapter or the current development build. See [known issues](https://nathanneurotic.github.io/Simple-Media-System/known-issues.html) and keep the same file/build/device when comparing results.

---
[Searchable manual](https://nathanneurotic.github.io/Simple-Media-System/mx4sio.html) · [Edit the maintained source](https://github.com/NathanNeurotic/Simple-Media-System/blob/master/docs-site/content/mx4sio.html)
