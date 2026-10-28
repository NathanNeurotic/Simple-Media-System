# Install & First Boot

SMS ships as a single PlayStation 2 executable — an `SMS.elf` file. There is no installer and nothing to write to the console itself: you copy the ELF to storage your PS2 can read, then launch it however you normally launch homebrew. This page covers what you need, where to get the build, the common ways to run it, what the first boot looks like, and how to prepare a media device.

ℹ In one sentence

 Put `SMS.elf` somewhere your PS2 can reach (a memory card or a USB drive), launch it from your homebrew loader, and put your videos and music on any FAT32/exFAT drive. That is the whole install.

## What you need

SMS is homebrew: it is an unsigned program, so your console needs to be able to run unsigned programs. It does **not** modify your PS2, install anything to the system, or require a modchip — it just needs a way in.

- **A PS2 that can run homebrew.** Any of the usual routes works — a softmod such as **FreeMcBoot** on a memory card, **FreeDVDBoot** from a burned disc, or any other exploit/loader that lets you launch an ELF. If you already run homebrew on your PS2, you are ready.
- **The SMS build** — the `SMS.elf` file itself. See [Getting SMS](https://nathanneurotic.github.io/Simple-Media-System/install.html#getting-sms) below.
- **A media device** to play from. The simplest is a **FAT32 or exFAT USB flash drive**; if you want to use MX4SIO, the internal HDD, i.LINK, MMCE, SMB or UDPFS instead, each is covered under [Devices](https://nathanneurotic.github.io/Simple-Media-System/devices.html). On a very early / fat PS2 you can also just burn media to CD/DVD.
- **A controller** to drive the menus: DualShock 2, or DualShock 3/4 over USB or Bluetooth. A memory card is optional throughout: settings follow the boot device, and with no card fitted SMS writes `IPCONFIG.DAT` next to the ELF too.

ℹ DualShock 3 / 4

 Wired DS3/DS4 support uses USB and is loaded automatically. For Bluetooth, connect a compatible USB Bluetooth dongle, plug the controller into USB, choose **Device settings → Pair Bluetooth controller**, then unplug the cable and power the controller on. These USB controller paths do not contend with MX4SIO on SIO2.

Homebrew access is up to you

 This guide assumes your console can already launch homebrew. Setting up FreeMcBoot, FreeDVDBoot or another exploit is a separate, well-documented topic that depends on your exact console model and region — follow a current guide for *your* PS2. SMS itself asks nothing special of your console beyond "can run an ELF."

## Getting SMS

You need one file: the compiled `SMS.elf`. Grab it from the project's builds — the [Releases & Changelog](https://nathanneurotic.github.io/Simple-Media-System/releases.html) page tracks the available downloads. If you would rather build the ELF yourself from the source tree, the [Building from Source](https://nathanneurotic.github.io/Simple-Media-System/building.html) page walks through the Docker toolchain and `make`.

The IOP driver modules SMS needs (USB, MX4SIO, HDD, MMCE, network and the rest) are **embedded inside the ELF**, so a single file is genuinely all you copy — there are no loose `.irx` files to place beside it.

ℹ One file, many names

 You can rename the ELF to suit your loader (some setups expect `BOOT.ELF` on a memory card, for example). The program is the same regardless of the filename.

## Running the ELF

SMS launches like any other PS2 homebrew ELF. Use whichever method your console is already set up for — the following are the common ones, not an exhaustive list. If your loader can point at an ELF, it can launch SMS.

### From a memory card via FreeMcBoot / OSDSYS

If your memory card runs **FreeMcBoot**, copy `SMS.elf` to the card (via a memory-card manager on the PS2, or an FTP/USB transfer tool) and add it to your FreeMcBoot / OSDSYS launch menu, or drop it where your menu already looks for ELFs. Then pick it from the boot menu at power-on. This is the most convenient setup — SMS is available every time you turn the console on, no disc required.

### With wLaunchELF (uLaunchELF) off USB or a memory card

**wLaunchELF** (uLaunchELF / uLE) is the community file-manager-and-launcher most homebrew users already have. Copy `SMS.elf` to a USB drive or memory card, open uLaunchELF, browse to the file (`mass:` for USB, `mc0:`/`mc1:` for a memory card) and press the launch/confirm button on it. uLaunchELF loads the ELF and SMS starts. There are legacy walk-through screenshots of this flow in the project's image set.

### Launched by another loader or as an auto-boot ELF

Any launcher that can run an arbitrary ELF can start SMS — some people wire it into their boot menu so it runs automatically, or launch it from another homebrew front-end. As long as the loader hands control to `SMS.elf`, SMS behaves the same. Where SMS then *saves its settings* depends on where it was booted from — see [Next steps](https://nathanneurotic.github.io/Simple-Media-System/install.html#next-steps).

### From a burned disc (older consoles)

On consoles set up for it, SMS can also be run from optical media (for example via FreeDVDBoot or a boot disc), which is handy on an early PS2 with no other storage. Preparing a bootable disc is a console-specific process; once it boots, the SMS experience is identical.

ℹ Legacy install screenshots

 The project keeps older step-by-step images (uLaunchELF, CD burning, FTP transfer) under `docs/images/` — files named `installation-*.png`. They predate this fork's UI refresh but still illustrate the general launch-from-USB and copy-to-card flows.

## First boot — what you see

When SMS starts you land on its **desktop**: a full-screen background with three bordered areas.

- The **DEVICE menu** across the top — a row of device icons (USB, MX4SIO, HDD, MMCE, network and so on).
- The **FILE menu** in the middle — the file browser / list for whatever device is selected.
- The **STATUS line** along the bottom — the current path on the left and a clock on the right.

The active area is highlighted; each area sits inside its own rounded rectangle. You move between the device row and the file list with the D-pad — full navigation is covered in [Getting Started](https://nathanneurotic.github.io/Simple-Media-System/getting-started.html) and the [Devices Overview](https://nathanneurotic.github.io/Simple-Media-System/devices.html).

On a clean install, optional Autostart choices are off

 Out of the box SMS starts with **no storage devices running** — the device row will look sparse until you turn something on. This is deliberate: you enable exactly the devices you use. Open the settings menu and flip the matching **Autostart** toggle (USB, MX4SIO, HDD, i.LINK, MMCE, network or UDPFS), or use its **Start …** item to bring it up on the spot without a reboot. Once a device is started it appears in the top row and you can browse into it. Full details are on the [Settings Reference](https://nathanneurotic.github.io/Simple-Media-System/settings.html).

Three resource pairs — choose one from each

 **MX4SIO and MMCE** share SIO2; the internal HDD's **APA/PFS** and **exFAT/BDM** paths share ATA; and **SMB/host and UDPFS** load different stacks for the one Ethernet controller. Whichever starts first owns the shared resource for that boot. See [Devices Overview](https://nathanneurotic.github.io/Simple-Media-System/devices.html) for the specifics.

## Preparing a media device

SMS is a media player that browses the **raw filesystem** of whatever device you point it at and plays media files wherever they sit. There is **no required folder structure** — no special "movies" or "cover-art" tree to build. You just put files on a drive and browse to them.

### Format the drive FAT32 or exFAT

A standard **FAT32** or **exFAT** USB flash drive or microSD works. Format it on your PC the normal way; no special partitioning is needed for USB or MX4SIO. (The internal APA HDD is the exception — that path uses the PS2's own partition scheme; see [Internal HDD](https://nathanneurotic.github.io/Simple-Media-System/hdd.html).)

### Copy your media anywhere on it

Drop your videos, music and pictures onto the drive in whatever folders you like. SMS recognises files by extension — AVI/DivX/Xvid, `mp4v` MP4 and MPG/MPEG for video; MP2/MP3, AAC, AC3, FLAC, OGG and WMA for audio; JPEG for pictures; plus M3U playlists and external SRT/SUB subtitles. The full list is on the [Video](https://nathanneurotic.github.io/Simple-Media-System/video.html) and [Audio](https://nathanneurotic.github.io/Simple-Media-System/audio.html) pages.

### Plug it in and start the device in SMS

Connect the drive, then start the matching device from the SMS device menu or its Autostart toggle. Your files appear in the browser, ready to open with **Cross**. That is it — no import step, no database.

ℹ Device names you'll see

 BDM mass-storage devices (USB, MX4SIO, exFAT HDD, i.LINK) all appear as `massN:` where N is 0, 1, 2… MMCE units show as `mmce0:`/`mmce1:`, the internal APA HDD as `hdd0:`, UDPFS as `udpfs:`, and optical discs via `cdfs`. You rarely need to type these — you pick devices by their icon — but they show up in the status line's path.

## Next steps

SMS is running and a device is prepared — now go play something and tune the setup:

- [Getting Started Play your first file, end to end](https://nathanneurotic.github.io/Simple-Media-System/getting-started.html)
- [Devices Overview Every storage source explained](https://nathanneurotic.github.io/Simple-Media-System/devices.html)
- [Settings Reference Autostart toggles & options](https://nathanneurotic.github.io/Simple-Media-System/settings.html)
- [MX4SIO microSD playback — the headline](https://nathanneurotic.github.io/Simple-Media-System/mx4sio.html)
- [Network Setup On-screen IP, SMB & UDPFS](https://nathanneurotic.github.io/Simple-Media-System/network.html)
- [Releases Download the latest ELF](https://nathanneurotic.github.io/Simple-Media-System/releases.html)

<details><summary>Where do my settings get saved?</summary>

Use **Save settings** after changes. On a remountable launch source, `SMS.cfg` normally lives beside the ELF even when a memory card is fitted; `SMS.smb` follows its resolved directory. See [Config Files & Paths](https://nathanneurotic.github.io/Simple-Media-System/configuration.html) for card-slot, network-file and fallback rules.

</details>

<details><summary>Do I have to build my media into an OPL-style folder tree?</summary>

No. SMS is not OPL — there are no ISOs, no `VMC`, no `ul.*` files and no coverart folders. It plays whatever media files it finds, wherever they are on the drive. Just copy your files on and browse to them.

</details>

**Boot-source exception:** optional Autostart flags default off, but SMS may start/remount the device holding its ELF/config during startup. A source already visible after first boot does not mean another Autostart choice was saved. See [configuration discovery](https://nathanneurotic.github.io/Simple-Media-System/configuration.html).

---
[Searchable manual](https://nathanneurotic.github.io/Simple-Media-System/install.html) · [Edit the maintained source](https://github.com/NathanNeurotic/Simple-Media-System/blob/master/docs-site/content/install.html)
