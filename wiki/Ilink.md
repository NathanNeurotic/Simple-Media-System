# i.LINK (IEEE 1394 / FireWire)

Some early PlayStation 2 models carry a small **i.LINK** port — Sony's name for **IEEE 1394**, the connector the rest of the world calls **FireWire**. This fork of SMS can bring an external i.LINK drive online and play media straight off it, exactly like a USB stick. The drive shows up in the device menu as a `massN:` volume, so once it appears you browse and play from it with the same controls you use everywhere else in SMS.

ℹ At a glance

 An external **FireWire / IEEE 1394** drive plugged into the PS2's **i.LINK** port is mounted by SMS as a **BDM mass device** (`mass0:`, `mass1:`, …). Format it **FAT32** or **exFAT**, turn on **Autostart i.LINK** (or use **Start i.LINK now**), give it a couple of seconds to be discovered, then browse and play. This has been **confirmed working on hardware** in this fork.

## What is i.LINK?

**i.LINK** is Sony's branding for the **IEEE 1394** serial bus — the same standard sold elsewhere as **FireWire** (Apple) or **Lynx** (Texas Instruments). Many PS2 models expose a 4-pin i.LINK port on the front or side of the console. It was originally intended for connecting the PS2 to other PS2s and to digital video gear; in practice, on homebrew, it is a handy way to attach an **external hard drive or SSD enclosure** that speaks FireWire.

When SMS starts its i.LINK support, it loads the IOP driver modules that walk the FireWire bus, find any attached storage, log in to it and expose its partitions to the console's block-device layer. From that point on the drive is just another **BDM mass device** — the same generic storage plumbing that USB sticks, MX4SIO microSD cards and exFAT internal HDDs all ride on. That is why an i.LINK drive appears as `massN:` and behaves identically to a USB drive once it is mounted: SMS does not treat i.LINK as a special "kind" of media, only as another source of `massN:` volumes.

Not every PS2 has i.LINK

 Later fat models and slimline PS2s can lack the i.LINK port; do not assume that a fat case guarantees one. If your console has no FireWire connector, this feature simply has nothing to attach to — use [USB](https://nathanneurotic.github.io/Simple-Media-System/usb.html), [MX4SIO](https://nathanneurotic.github.io/Simple-Media-System/mx4sio.html) or the [internal HDD](https://nathanneurotic.github.io/Simple-Media-System/hdd.html) instead. Check the front and sides of your console for a small 4-pin i.LINK-labelled socket before buying a cable.

## Compatibility

i.LINK storage in SMS is for **external FireWire drives**: a hard disk or SSD in an enclosure (or a dock) that connects over IEEE 1394. The drive is read through the same filesystem layer as every other BDM mass device, so the same formatting rules apply.

| Property | What works |
| --- | --- |
| Connection | i.LINK / IEEE 1394 / FireWire, into the PS2's i.LINK port |
| Device type | External hard drive or SSD enclosure that implements SBP-2 storage |
| Filesystem | **FAT32** or **exFAT** |
| Appears as | `massN:` (shares the BDM namespace with USB, MX4SIO and ATA/exFAT HDD) |
| Role in SMS | A browsable media source — play video, audio and pictures directly from it |

ℹ A modest note on tested drives

 i.LINK support is **confirmed working on real hardware** in this fork, mounting an exFAT drive over FireWire. That said, the FireWire enclosure market is wide and varied, and only a small number of drives have actually been tried. If your particular enclosure does not come up, it may be an enclosure/bridge-chip quirk rather than a problem with your media — try a different enclosure or fall back to [USB](https://nathanneurotic.github.io/Simple-Media-System/usb.html).

## Preparing the drive

Set the drive up on your PC the same way you would prepare a USB drive for SMS, then copy your media onto it.

### Format FAT32 or exFAT

On your computer, format the FireWire drive as **FAT32** or **exFAT**. exFAT is the easy choice for large drives and large files; FAT32 is fine for smaller drives.

### Copy your media on

Drop your video, audio and picture files onto the drive. SMS browses the **raw filesystem** — there is no mandatory folder layout to follow. Put your files wherever you like (a simple folder such as `/Movies` or `/Music` just keeps things tidy) and SMS will let you navigate to them.

### Eject, connect, and power the drive

Safely eject the drive from your PC, then plug it into the PS2's **i.LINK** port with a FireWire cable. If your enclosure needs external power, make sure it is powered and the disk is spun up before (or as) SMS looks for it.

ℹ Which formats will play

 Any file SMS understands will play from an i.LINK drive: **video** (DivX / XviD / MPEG-4 Part 2 AVI, `mp4v` MP4, MPEG-1/2 program streams), **audio** (MP2/MP3, AAC, AC3, FLAC, OGG Vorbis, WMA) and **JPEG** pictures, plus **M3U** playlists and external **SRT / SUB** subtitles. See [Video Playback](https://nathanneurotic.github.io/Simple-Media-System/video.html) and [Audio & Playlists](https://nathanneurotic.github.io/Simple-Media-System/audio.html) for the full detail.

## Enabling i.LINK in SMS

On a clean install optional Autostart choices are off — you turn on the devices you want. i.LINK is off until you enable it, in one of two ways.

### Turn on Autostart i.LINK (persistent)

In the settings menu, enable the **Autostart i.LINK** toggle. From then on, every time SMS boots it will bring the FireWire bus up and look for an attached drive automatically. This is the "set it and forget it" option if you keep a drive on the i.LINK port. Your choice is saved with the rest of your settings (see [Settings](https://nathanneurotic.github.io/Simple-Media-System/settings.html)).

### Or use "Start i.LINK now" (on demand)

If i.LINK is not already running, a **Start i.LINK now** item is available so you can bring it up on the spot — no reboot needed. Handy when you only occasionally attach a FireWire drive and would rather not auto-start it every boot.

### Give it a couple of seconds

FireWire discovery is not instant. After i.LINK starts, the driver needs a **short moment** — on the order of a couple of seconds — to discover the drive on the bus and log in to it before it appears in the device menu. If it does not show up immediately, wait a beat; this delay is normal.

Have the drive connected and ready first

 Because discovery happens shortly after i.LINK starts, plug the drive in and make sure it is powered and spun up **before** you enable Autostart i.LINK or press Start i.LINK now. A drive that is still spinning up, or connected after the scan, may be missed until the next start.

## Browse & play

Once the drive is discovered it appears in the top **device menu** as a `massN:` volume. From there it behaves exactly like any other storage device in SMS:

### Pick the device

In the device menu at the top, use **Left / Right** to highlight your i.LINK volume (a `massN:` entry), then press **Down** to drop into the file list.

### Navigate to your file

In the file list, move with **Up / Down**, page through long lists with **L1 / L2**, open a folder or start a file with **Cross**, and go up one level with **Triangle**. The bottom status line shows your current path on the left and a clock on the right.

### Play

Open a video, track or picture and it plays straight from the FireWire drive. During playback, **Select** pauses, **Start** resumes, and **Triangle** stops and returns you to the browser.

ℹ Which massN: is my i.LINK drive?

 USB, MX4SIO, i.LINK and exFAT/ATA HDD all share the single `massN:` numbering pool, and the number a device gets depends on what started and in what order. If you have several BDM devices connected at once, the simplest way to tell them apart is to open each `massN:` entry and recognise its contents. With only the i.LINK drive attached it will typically be `mass0:`.

## Notes & good to know

- **Its own bus.** i.LINK runs over the FireWire (IEEE 1394) bus, independent of USB and of the SIO2 / memory-card bus. It happily works **alongside USB and MX4SIO** at the same time — there is no conflict between them.
- **Confirmed on hardware.** i.LINK storage has been **tested and works on a real PS2** in this fork, mounting an exFAT drive over FireWire next to the other working devices.
- **A couple of seconds to appear.** The short discovery/login delay after starting i.LINK is expected — give the bus a moment before assuming the drive is not there.
- **FAT32 or exFAT only.** Keep the drive formatted FAT32 or exFAT; other filesystems are not read.
- **No special folder tree.** SMS reads the raw filesystem and plays media wherever it sits — there is no required directory layout to build.

<details><summary>Why does i.LINK take a moment to show up, when USB is instant?</summary>

FireWire storage is reached over a discovery-and-login handshake: after the bus comes up, the driver has to enumerate the devices on it, find the storage node, and log in to it before its partitions can be mounted. That handshake takes a short while — on the order of a couple of seconds — whereas USB tends to enumerate faster. The practical upshot is simply to have the drive connected, powered and spun up before you start i.LINK, and to wait a beat for the `massN:` entry to appear rather than assuming nothing happened.

</details>

<details><summary>Can I use i.LINK and the internal HDD together?</summary>

Yes. i.LINK is an *external* FireWire device on its own bus, so it does not clash with the internal HDD's ATA bus. The one exclusivity that lives on the internal ATA bus is between the classic **APA/PFS** HDD path and the **exFAT/ATA (BDM)** HDD path — you pick one of those two for the internal drive (see [Internal HDD](https://nathanneurotic.github.io/Simple-Media-System/hdd.html)). i.LINK sits outside that entirely and can run at the same time as either.

</details>

ℹ Related pages

 [USB](https://nathanneurotic.github.io/Simple-Media-System/usb.html) and [MX4SIO](https://nathanneurotic.github.io/Simple-Media-System/mx4sio.html) are the other BDM mass devices and behave the same way once mounted. For the internal drive (APA and exFAT) see [Internal HDD](https://nathanneurotic.github.io/Simple-Media-System/hdd.html). For the full list of per-device Autostart toggles, see the [Settings Reference](https://nathanneurotic.github.io/Simple-Media-System/settings.html). For a bird's-eye view of every storage backend, see the [Devices Overview](https://nathanneurotic.github.io/Simple-Media-System/devices.html).

The driver uses SBP-2/SCSI over IEEE 1394, not PS2 ATA commands. Historical project documentation reports an exFAT enclosure test; it does not establish every bridge, drive or console combination. Keep each media file below 2 GiB.

**File-size and test scope:** keep every SMS media file below 2 GiB, including on exFAT. Existing project reports do not qualify every drive/adapter or the current development build. See [known issues](https://nathanneurotic.github.io/Simple-Media-System/known-issues.html) and keep the same file/build/device when comparing results.

---
[Searchable manual](https://nathanneurotic.github.io/Simple-Media-System/ilink.html) · [Edit the maintained source](https://github.com/NathanNeurotic/Simple-Media-System/blob/master/docs-site/content/ilink.html)
