# Internal HDD

SMS can read media straight off the PlayStation 2's internal hard drive — in **two completely different ways**. There's the classic **APA / PFS** scheme (the same format HDD-OSD and the OPL-era tools use), where SMS lists the drive's PFS partitions and browses whichever one you pick; and there's the modern route, a plain **exFAT** drive read through the **ATA block driver**, which shows up as an ordinary `massN:` device you can fill with files from any PC. Both live on the same physical disk, so you pick one per boot.

You need the hardware first

 The internal HDD is optional PS2 hardware. A fat PS2 needs the **Network Adapter** (the expansion-bay module that carries the internal IDE connector); the drive itself sits in the HDD bay behind it. Slim consoles have no internal HDD bay at all. If your console has no drive fitted, skip this page and use [USB](https://nathanneurotic.github.io/Simple-Media-System/usb.html), [MX4SIO](https://nathanneurotic.github.io/Simple-Media-System/mx4sio.html) or [SMB](https://nathanneurotic.github.io/Simple-Media-System/smb.html) instead.

## Two ways to use the HDD

SMS is a media player, not a game loader — so the HDD is just another place to keep your video, music and pictures. The catch is that the internal drive can be formatted in one of two schemes, and SMS reaches each one through a different driver stack:

- [APA / PFS The PS2's own partition scheme](https://nathanneurotic.github.io/Simple-Media-System/hdd.html#apa)
- [exFAT via ATA A plain drive as massN:](https://nathanneurotic.github.io/Simple-Media-System/hdd.html#exfat)

They are **mutually exclusive**: both paths want to own the internal ATA bus, so whichever one starts first takes it and the other quietly steps aside. You choose which by picking the matching **Autostart** toggle — see [Choosing one](https://nathanneurotic.github.io/Simple-Media-System/hdd.html#choosing) below.

| How it appears | APA / PFS | exFAT via ATA (BDM) |
| --- | --- | --- |
| Format on the disk | Sony's APA partition scheme + PFS filesystem | Plain exFAT partition |
| What SMS browses | Any PFS partition on the drive — you pick one from a list | The whole exFAT volume, any folders |
| Device name in SMS | Internal HDD (APA) | `massN:` (shared BDM mass namespace) |
| Made with | HDD-OSD / OPL-era APA & PFS tools | Any PC — format exFAT, copy files |
| Enable with | **Autostart HDD** (the APA/PFS row) | **Autostart HDD BDM** |

## APA / PFS — the PS2's native HDD format

The **APA** partition scheme and **PFS** filesystem are the PS2's own native HDD formats — the same ones used by Sony's HDD-OSD (the "PlayStation BB Navigator" era) and by the classic homebrew HDD tools. On this kind of drive, SMS lists the PFS partitions it finds on the APA table as a folder-like list. Pick one with `Cross` to mount it and browse the media inside; SMS remembers the partition you last used and re-enters it automatically on the next boot. There is no required partition name.

ℹ No special partition name needed

 Older guides tell you to create a partition called `SMS_Media`. That has not been necessary for a long time and this build does not look for it — SMS shows you every PFS partition on the drive and mounts the one you choose. Any partition made with a standard APA/PFS tool works, whatever it is called. If you already have an `SMS_Media` partition it will simply appear in the list like any other.

### Preparing an APA drive

SMS doesn't format drives or create partitions itself — you do that ahead of time with the usual PS2 HDD tools (HDD-OSD, or a homebrew APA/PFS partition manager such as uLaunchELF). The short version:

### Format the drive as APA

Initialise the internal drive with the standard PS2 APA scheme, the same way you would for any HDD-OSD / OPL-era setup. This is a one-time step done from a HDD tool, not from SMS.

### Create a PFS media partition

Make a PFS partition to hold your media — call it whatever you like. Copy your video, audio and picture files into it (folders inside are fine — SMS browses the raw tree).

### Enable the APA path in SMS

Turn on **Autostart HDD APA** in the settings (or use **Start HDD APA now** — see [Enabling & tips](https://nathanneurotic.github.io/Simple-Media-System/hdd.html#enabling)). SMS starts the HDD driver stack and lists the drive's PFS partitions.

### Browse and play

Pick the internal-HDD icon in the device menu and press `Down` into the list. Choose your partition with `Cross` to mount it, then open any file with `Cross` as usual — SMS remembers the partition and re-enters it on the next boot. It behaves exactly like every other SMS source — the raw filesystem, no mandatory folder layout.

APA drives are ~2 TB and older-drive friendly

 The APA scheme addresses drives up to about **2 TB**. If you have a larger drive — or you'd simply rather copy files from a PC without any PS2-side tools — use the exFAT route below instead.

## exFAT via ATA

The modern alternative skips APA entirely. Format the internal drive as plain **exFAT** on your PC, drop your media anywhere on it, and let SMS read the drive through its **ATA block driver**. Because that driver plugs into the same **Block Device Manager (BDM)** that handles USB, MX4SIO and i.LINK, the internal drive simply appears as another **`massN:`** device in SMS — no PS2-specific formatting, no partition juggling.

ℹ One shared `massN:` namespace

 USB, MX4SIO, the exFAT internal HDD and i.LINK all mount into the **same** `massN:` pool (`N` = 0, 1, 2…). The number is assigned as devices come up, so a drive is `mass0:` if it mounts first, `mass1:` if a USB stick got there ahead of it, and so on. You browse it by picking its device icon in the top row, not by memorising the number.

### Preparing an exFAT drive

### Format exFAT on your PC

Connect the drive to a computer (or format it in-bay with a PC-side tool) and format it as **exFAT**. Leave the allocation-unit / cluster size at the default for the drive's capacity rather than choosing a custom value.

### Copy your media

Drag your video, audio and picture files onto the drive — into any folders you like. SMS browses the raw filesystem, so there is no required folder tree (no OPL-style `DVD/`/`CD/` layout).

### Fit the drive and enable the BDM path

Install the drive in the console's HDD bay, then turn on **Autostart HDD BDM** in SMS (or **Start HDD BDM now**). SMS brings up the ATA driver and the exFAT volume mounts as `massN:`.

### Browse and play

Select the internal-HDD icon in the device menu and open files just like any other `massN:` device.

Copy large files one at a time

 The PS2 reads far better from files that aren't fragmented. When you copy a batch to an exFAT drive, add big video files **individually** rather than dumping the whole folder in one go — many PC copy operations interleave a large batch, fragmenting every file. Sequential copies keep playback smooth.

## Choosing one — they're mutually exclusive

Here's the important constraint: **the APA/PFS path and the exFAT/ATA path both own the internal ATA bus**, so only one can be active at a time. SMS enforces this with a simple "first come, first served" rule — whichever backend starts first claims the bus, and the other declines to start for that session.

ℹ First to start owns the bus

 If the APA HDD path comes up first, the exFAT/ATA path won't start (and vice versa). This isn't an error — it's SMS keeping two drivers that both drive the same internal disk from stepping on each other. To switch schemes, disable the one you don't want (or don't autostart it) and reboot so the other can claim the bus cleanly.

Practically, that means you decide up front which scheme your drive is formatted for and enable the matching autostart:

| Your drive is… | Turn on | Leave off |
| --- | --- | --- |
| APA-formatted (any PFS partition) | **Autostart HDD APA** | Autostart HDD BDM |
| exFAT-formatted (a plain drive) | **Autostart HDD BDM** | Autostart HDD APA |

**Autostart HDD BDM** is the ATA/exFAT path — "BDM" because the exFAT drive is read through the same Block Device Manager as your other mass devices. **Autostart HDD APA** is the classic native-partition path.

Don't enable both

 Turning on both autostarts doesn't get you both drives — it just means whichever one happens to initialise first wins the bus, and the result depends on start order. Pick the one that matches how your drive is formatted and leave the other off.

<details><summary>Can the HDD run alongside USB, MX4SIO, MMCE and SMB?</summary>

Yes — the internal-HDD bus contention is only *between the two HDD schemes*. Whichever HDD path you choose can run happily at the same time as USB, i.LINK and SMB. Note two other exclusions elsewhere in SMS: **MX4SIO and MMCE** share the memory-card (SIO2) bus, so only one of those two runs at once; and the exFAT/ATA HDD is a `massN:` device like USB and MX4SIO, so they simply each take their own slot in that shared namespace.

</details>

## Enabling & tips

On a clean install optional Autostart choices are off — you turn on exactly what you need. The relevant controls live in the settings menu:

- **Autostart HDD APA** — brings up the classic APA/PFS path at boot and lists the drive's PFS partitions.
- **Autostart HDD BDM** — brings up the exFAT drive over ATA at boot as a `massN:` device.
- **Start HDD … now** — when a device isn't already running, SMS also offers an on-demand start item, so you can bring the HDD up without a reboot. Handy for testing which scheme your drive actually mounts.

ℹ If a start stalls, watch the status line

 Starting the internal HDD spins the drive up and probes it, which takes a moment. SMS shows the current stage on the status line as it goes; if a start ever hangs on an unusual drive layout, the last message on screen tells you exactly which stage it reached. Because the HDD isn't set to auto-start on a clean install, a failed manual start won't stop SMS from booting next time — just reboot and try the other scheme, or fall back to USB/MX4SIO.

### Practical tips

- **Match the toggle to the format.** An exFAT drive will not mount under Autostart HDD APA, and an APA/PFS partition won't appear under Autostart HDD BDM. If a drive doesn't show up, you almost certainly enabled the wrong path for how it's formatted.
- **Big drive? Use exFAT.** APA tops out around 2 TB; exFAT lets you use larger drives and skip PS2-side tooling entirely — just format on a PC and copy files.
- **Big library already on APA? Keep it.** If your drive is already APA-formatted, the APA path reads your existing PFS partitions directly with no conversion — whatever they are named.
- **The drive is optional hardware.** Everything above assumes a Network Adapter and an internal drive are fitted. Without them, SMS is perfectly happy on [USB](https://nathanneurotic.github.io/Simple-Media-System/usb.html), [MX4SIO](https://nathanneurotic.github.io/Simple-Media-System/mx4sio.html), [MMCE](https://nathanneurotic.github.io/Simple-Media-System/mmce.html) or a [network share](https://nathanneurotic.github.io/Simple-Media-System/smb.html).

## Related pages

- [Devices Overview How every source fits together](https://nathanneurotic.github.io/Simple-Media-System/devices.html)
- [USB The other big massN: device](https://nathanneurotic.github.io/Simple-Media-System/usb.html)
- [i.LINK FireWire storage, also BDM mass](https://nathanneurotic.github.io/Simple-Media-System/ilink.html)
- [Settings Reference Every autostart toggle](https://nathanneurotic.github.io/Simple-Media-System/settings.html)

**File-size and test scope:** keep every SMS media file below 4 GiB, including on exFAT. Existing project reports do not qualify every drive/adapter or the current development build. See [known issues](https://nathanneurotic.github.io/Simple-Media-System/known-issues.html) and keep the same file/build/device when comparing results.

---
[Searchable manual](https://nathanneurotic.github.io/Simple-Media-System/hdd.html) · [Edit the maintained source](https://github.com/NathanNeurotic/Simple-Media-System/blob/master/docs-site/content/hdd.html)
