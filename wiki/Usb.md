# USB

The simplest way to get media onto SMS: copy your files to a USB flash drive or hard disk, plug it into the PlayStation 2's USB port, and browse them straight from the pad. SMS reads the drive's **raw filesystem** and plays your files wherever they sit — there is no special folder tree to build. USB drives mount into the BDM mass-storage namespace as `mass0:`, `mass1:`, and so on.

ℹ USB is the friendliest place to start

 Every fat PS2 has a USB port on the front (slim consoles too), so USB needs no extra adapter, no hard drive and no network. If you are brand new to SMS, a USB stick is the easiest device to try first — format it, drop a video on it, and go. See [Getting Started](https://nathanneurotic.github.io/Simple-Media-System/getting-started.html) for the full first-file walkthrough.

## Overview

A USB mass-storage device — a flash drive/stick, or a self-powered USB hard disk — connects to the PS2's USB port. SMS loads its USB driver on the IOP and mounts the drive through **BDM** (the Block Device Manager), the same mounting layer used by MX4SIO, the exFAT internal HDD and i.LINK. Because they all share that layer, USB drives take device names in the common `massN:` namespace: the first mounted BDM volume is `mass0:`, the next `mass1:`, and so on.

Once mounted, the drive appears as a device icon in the **device menu** (the top row of the SMS screen). Select it and press Down to drop into the **file list** and browse. SMS plays media directly off the drive — video, audio and pictures alike.

Which `massN:` is my USB stick?

 The number is assigned in mount order, not per port, so it depends on which BDM devices are running and the order they came up. If you also run MX4SIO, i.LINK or an exFAT HDD, your USB drive may not always be `mass0:`. The active device's name is shown on the **status line** at the bottom of the screen — just glance there to confirm which volume you are browsing.

## Compatibility

SMS reads USB drives formatted with a standard PC filesystem and a normal partition table.

| Requirement | Details |
| --- | --- |
| Filesystem | **FAT32** or **exFAT** |
| Partition table | **MBR** (master boot record) — the usual default for USB media |
| Device type | USB flash drive / stick, or a self- or externally-powered USB hard disk |
| Media location | Anywhere on the drive — SMS browses the raw filesystem, so any folder layout works |

Format the drive as a single MBR partition. FAT32 is the most universally compatible choice; use **exFAT** if you prefer that filesystem on a larger volume; each SMS media file must still be below 2 GiB. There is no coverart, ISO or special-folder requirement — SMS is a media player that reads your files where they are, not a game loader.

ℹ A note on USB speed

 The PlayStation 2's USB hardware is USB 1.1, so raw throughput off any USB drive is modest by modern standards. It can handle conservative media profiles, but throughput and queue behavior depend on the file and build. The current [USB stutter report](https://nathanneurotic.github.io/Simple-Media-System/known-issues.html#usb) remains open. Very high bitrate files may be the exception — if a demanding clip stutters, a lower-bitrate encode or a faster device such as MX4SIO or the internal HDD will help. Keeping files unfragmented (below) matters more than the drive's own speed rating.

## Preparing the drive

### Format as FAT32 or exFAT

On your PC, format the USB drive as a single **FAT32** or **exFAT** partition on an **MBR** partition table. If you need to reformat, back up the contents first: formatting erases the volume. An existing compatible filesystem does not need to be reformatted just to try SMS.

### Copy your media anywhere

Drag your videos, music and pictures onto the drive. You can drop them at the root, or organise them into folders however you like — SMS shows whatever directory tree is on the drive. There is **no** mandatory folder structure: no `DVD/`, `CD/` or `ART/` tree, no ISOs. Just files.

### Use supported formats

Copy files SMS can play: video such as **AVI** (DivX/Xvid/MPEG-4 Part 2), **MP4** (MPEG-4 Part 2 `mp4v`) and **MPEG** program streams; audio such as **MP2/MP3**, **AAC**, **FLAC**, **OGG**, **WMA** and **AC3**; and **JPEG** pictures. H.264/H.265 video is not supported even when stored in an MP4 file. External **SRT/SUB** subtitles and **M3U** playlists work too. The [Video](https://nathanneurotic.github.io/Simple-Media-System/video.html) and [Audio](https://nathanneurotic.github.io/Simple-Media-System/audio.html) pages list every format in detail.

### Eject cleanly, then plug into the PS2

Safely eject the drive from your PC so all writes are flushed, then plug it into the PlayStation 2's USB port. Fat consoles have their ports on the front; slims have them at the top-front. Then boot SMS.

## Enabling USB in SMS

On a clean install SMS leaves optional Autostart flags off — you turn on the devices you want. There are two ways to get USB running.

### Autostart USB (recommended)

In the settings menu, turn on **Autostart USB**. From then on, SMS loads the USB driver at boot, so a plugged-in drive is detected and mounted automatically every time you start the player. This is the set-and-forget option if you use USB regularly.

### Start USB now (on demand)

If USB is not currently running — for example you left Autostart off — SMS offers a **Start USB support** item so you can bring the device up on the spot, without rebooting. If USB is *already* running and you plug a drive in afterwards, that item is gone; use **Refresh connections** at the bottom of Device settings instead, which re-scans the mass slots for newly attached drives. Trigger it, give the driver a moment to load and enumerate the drive, and the USB device appears in the device menu.

ℹ Pick USB in the device menu

 Once USB is running, use **Left/Right** in the device menu to move the highlight onto the USB (`massN:`) device, then press **Down** to enter its file list. The status line at the bottom shows the current device and path so you always know where you are.

## Browsing & playing

Inside the file list, navigation is the same as anywhere in SMS:

| Input | Action |
| --- | --- |
| **Up / Down** | Move the selection through the file list |
| **L1 / L2** | Page-scroll through a long list |
| **Cross (✕)** | Open the highlighted folder, or start playing the highlighted file |
| **Triangle (△)** | Go up one level — folder → parent → back out to the device menu |
| **Left / Right** | Return to the device menu to switch devices |

Select a video, music track or picture with **Cross** and it plays. During video or audio playback, **Select** pauses, **Start** resumes, and **Triangle** stops and returns you to the browser. Subtitles and M3U playlists that live alongside your media on the USB drive are picked up automatically — see [Subtitles](https://nathanneurotic.github.io/Simple-Media-System/subtitles.html) and [Audio & Playlists](https://nathanneurotic.github.io/Simple-Media-System/audio.html).

## Tips & troubleshooting

- **Keep every media file below 2 GiB.** exFAT can store larger files, but SMS still uses signed 32-bit seek offsets on common file paths. Split or re-encode an oversized movie; changing the filesystem does not widen those offsets.
- **Keep files unfragmented.** On a heavily-fragmented drive, playback can stutter as the PS2 chases scattered pieces of a file. The simplest fix is to copy everything off, reformat the drive, and copy it back so each file lands contiguously. On slow or old media this matters more than anything else.
- **Avoid too many small fragments.** A near-full drive that has had files added and deleted many times fragments faster; leaving some free space and doing occasional clean reformats keeps large videos playing smoothly.
- **Confirm the device name on the status line.** If you are unsure which `massN:` is your USB drive (because MX4SIO, i.LINK or an exFAT HDD are also running), read the bottom status line — it shows the current path, which starts with the active device.
- **Nothing appears?** Make sure USB is actually started (Autostart USB, or Start USB now), that the drive is FAT32/exFAT on an MBR table, and that it is fully seated in the port. Powered USB hard disks should have their own power connected. See [Troubleshooting](https://nathanneurotic.github.io/Simple-Media-System/troubleshooting.html) if a drive still won't show up.

ℹ USB and the other BDM devices

 USB, MX4SIO, the exFAT (BDM) internal HDD and i.LINK all share the `massN:` namespace and can generally run side by side. The one pairing to know about elsewhere is that **MX4SIO** and **MMCE** both use the memory-card (SIO2) bus, so only one of those two can be active at a time — USB itself is unaffected. See the [Devices Overview](https://nathanneurotic.github.io/Simple-Media-System/devices.html) for the full picture.

<details><summary>Why does my USB drive show up as  massN:  and not &quot;usb0:&quot;?</summary>

This build mounts USB through **BDM** (the Block Device Manager), a shared block-device layer that also handles MX4SIO, the exFAT internal HDD and i.LINK. BDM presents every attached block volume under one unified `massN:` namespace, numbered in the order the volumes are discovered. So the name reflects *mount order across all BDM devices*, not the USB port or the drive brand. Whichever volume comes up first is `mass0:`; the next is `mass1:`, and so on. When in doubt, the status line at the bottom of the screen always tells you which device and path you are currently browsing.

</details>

**File-size and test scope:** keep every SMS media file below 2 GiB, including on exFAT. Existing project reports do not qualify every drive/adapter or the current development build. See [known issues](https://nathanneurotic.github.io/Simple-Media-System/known-issues.html) and keep the same file/build/device when comparing results.

---
[Searchable manual](https://nathanneurotic.github.io/Simple-Media-System/usb.html) · [Edit the maintained source](https://github.com/NathanNeurotic/Simple-Media-System/blob/master/docs-site/content/usb.html)
