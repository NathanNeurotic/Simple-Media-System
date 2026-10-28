# Troubleshooting

A problem-and-solution reference for the things that most commonly trip people up: video that stutters or won't start, a storage device that refuses to appear, an SMB share that won't connect, settings that don't stick, and a garbled screen in HD video modes. Start with the matching checks, then compare the [open issue map](https://nathanneurotic.github.io/Simple-Media-System/known-issues.html); some reported regressions are still under investigation.

ℹ Start with the obvious

 Before diving in, sanity-check the basics: the file is actually on the device you're browsing; the device is formatted correctly (see below); the device's **Autostart** is on (or you started it from the menu); and the media is a format SMS actually supports. Most "it won't work" reports come down to one of these four.

## A video stutters, drops frames, or won't play

SMS decodes video in software on the Emotion Engine and uses the PS2's **IPU** for colour conversion. That is a fixed budget — a file that is too demanding for the console will stutter no matter how fast your storage is. Work through these in order.

### Confirm the format is supported

SMS plays a specific set of containers and codecs. If a file uses anything outside this list, it will fail to open or may play only its supported audio track. Supported video is **AVI** (DivX / Xvid / MPEG-4 Part 2), **MP4** (`mp4v`, MPEG-4 Part 2), and **MPEG program streams** (MPEG-1/2, `.mpg`/`.mpeg`). H.264/AVC, H.265/HEVC, VP9, AV1 and WMV video are **not** supported — re-encode those to MPEG-4 Part 2 first. See [Video Playback](https://nathanneurotic.github.io/Simple-Media-System/video.html) for the full format table.

### Lower the bitrate and resolution

SMS rejects either video dimension above **1024 pixels**, but passing that check does not guarantee real-time playback. Resolution, frame rate, B-frames, QPel/GMC, motion complexity, audio work, bitrate and storage speed all matter. Start from the previously reported 640×480, 30 fps, 1.5 Mbit/s MPEG-4 Part 2 profile and reduce resolution, frame rate or bitrate if needed. If the same file plays from USB but stutters over the network or from MX4SIO, storage throughput or fragmentation is the likely difference.

### Rule out a damaged or fragmented file

A partial download, a bad burn, or a heavily fragmented file can cause hitching or a mid-file stop. Re-copy the file to the device (ideally to a freshly formatted or defragmented card/drive), or re-download it, and try again. A PC passing the file does not distinguish a PS2 parser/decoder regression from a demanding encode or storage problem. Keep an unchanged copy and its hash before converting or reorganizing it.

### Check the encode features

Interlaced MPEG-4 and MPEG-2 decode paths exist, but the current hardware fixture baseline is progressive. B-frames, QPel, GMC and interlacing can add work or expose compatibility differences. If a supported file is marginal, re-encode it progressive with no B-frames using the known-good command on the [Video Playback](https://nathanneurotic.github.io/Simple-Media-System/video.html#encoding) page.

Audio-only files that won't play

 The same "is it a supported format" rule applies to audio. SMS handles **MP3, AAC, AC3, FLAC, OGG Vorbis** and **WMA**. A file with an unusual codec inside a familiar-looking container (for example a lossless track the decoder doesn't recognise) can fail to open — re-encode to plain MP3 to confirm the file, then work back from there.

## A storage device doesn't appear

If a device you plugged in isn't showing in the top-row device menu, it's almost always one of three things: it isn't started, it isn't formatted the way SMS expects, or it's blocked by a mutual exclusion.

### Start the device (or turn on its Autostart)

On a clean install **optional Autostart choices are off** — this is by design (see below). Open the settings menu and either flip on that device's **Autostart** toggle (Autostart USB, Autostart MX4SIO, Autostart HDD APA, Autostart HDD flip on that device's **Autostart** toggle (Autostart USB, Autostart i.LINK, Autostart MX4SIO, Autostart MMCE, Autostart HDD APA, Autostart HDD BDM for ATA/exFAT, then the three mutually exclusive network rows — **Autostart HOST ( ps2dev )**, **Autostart SMB** and **Autostart UDPFS**, where picking one clears the other two), or use the matching **Start …** item that appears when a device isn't running yet. Starting it on demand does not need a reboot.

### Check the filesystem format

BDM mass devices (USB, MX4SIO, ATA/exFAT) expect a **FAT32** or **exFAT** filesystem. A drive formatted NTFS, HFS+ or ext4 will not mount. Re-format the card or drive as FAT32 (or exFAT for large volumes) and copy your media back. The internal-HDD APA/PFS path is different — that uses a PlayStation 2 PFS partition rather than a PC filesystem; see [Internal HDD](https://nathanneurotic.github.io/Simple-Media-System/hdd.html).

### Rule out a mutual exclusion

Three resource pairs cannot both be active in one boot:

- **MX4SIO and MMCE** both use the SIO2 / memory-card-slot bus. If one is running, the other won't appear — turn off the one you don't need.
- **The internal HDD APA/PFS path and the ATA/exFAT (BDM) path** both own the internal ATA bus. Whichever starts first owns it; pick one.
- **SMB/host and UDPFS** load different stacks for the single Ethernet controller. Reboot, then start only the network path you intend to use.

ℹ How devices are named

 BDM mass devices (USB, MX4SIO, ATA/exFAT, i.LINK) all share the `massN:` namespace, with `N` counting up from 0. MMCE devices are `mmce0:` / `mmce1:`. The internal APA/PFS HDD is `hdd0:`, and the UDPFS folder is `udpfs:`. If a device is present but numbered differently than you expect, that's usually just the mount order — check the path shown on the status line at the bottom of the screen.

## An SMB share won't connect

SMS speaks the older **SMB1 / CIFS** network-share protocol. By far the most common reason a share won't connect is that the server no longer accepts SMB1.

Top cause: SMB1 is disabled on the server

 Modern Windows and Samba ship with **SMB1 turned off by default** for security. SMS needs it enabled to connect. This is a server-side setting, not something SMS can work around from the PS2.

<details><summary>How do I re-enable SMB1 on the server?</summary>

**On Windows:** open *Turn Windows features on or off* and enable **SMB 1.0/CIFS File Sharing Support** (specifically the client/server sub-features), then reboot. Be aware SMB1 is legacy and considered insecure — only enable it on a trusted local network, and consider a dedicated share for the PS2.

**On a Samba (Linux/NAS) server:** allow the older protocol in the server config (for example by setting the minimum protocol to `NT1`) and restart the service. Consult your NAS or Samba documentation for the exact option, as it varies by version.

</details>

### Verify the address, share name and credentials

Once SMB1 is on, re-check the server's IP address, the exact share name, and the username/password — all entered on-screen with the built-in keyboard (see [Network Setup](https://nathanneurotic.github.io/Simple-Media-System/network.html)). A typo in any of these looks identical to "server refused". Case can matter for share names and usernames.

### Check the PS2's own network settings

Make sure the PS2 has a valid static IP on the same subnet as the server, saved through **Device settings → Network settings → Edit IPCONFIG.DAT**, and that the network device is started. If the console can't reach the network at all, no share will connect.

### Check firewalls and the server's file sharing

A firewall on the server (or router) can block the PS2's connection even with everything else correct. Confirm file sharing is enabled on the server and that its firewall allows connections from the PS2's IP on your local network.

SMB1 is a legacy protocol

 The SMB client in SMS is the legacy SMB1 stack, and modernising it to SMB2 is separate work beyond this build. If you can't or won't enable SMB1 on your server, use the [UDPFS network drive](https://nathanneurotic.github.io/Simple-Media-System/network.html#udpfs) or a directly-attached device instead.

## The UDPFS network drive won't appear

- Save the PS2's own static IP through **Edit IPCONFIG.DAT**; a `NOIP` error means this step is missing.
- Run PS2-Servers UDPFSD on a PC on the same LAN, select the folder to serve, and keep **Modulo mode off**.
- Allow UDP port `0xF5F6` (62966) through the PC firewall on the trusted LAN.
- If SMB/host was already started, reboot before choosing **Start UDPFS network drive**. The two stacks cannot share the NIC in one boot.

## Settings don't persist across reboots

If your Autostart toggles, network details or playback preferences reset every time you power on, the config isn't being saved — or isn't being found on the next boot.

### Find the active configuration

Use **Save settings** after changes. On a remountable launch source, `SMS.cfg` normally lives beside the ELF even when a memory card is fitted; `SMS.smb` follows its resolved directory. See [Config Files & Paths](https://nathanneurotic.github.io/Simple-Media-System/configuration.html) for card-slot, network-file and fallback rules.

### This fork can save to the boot device instead

When SMS remounts its boot device, this fork saves the config **next to the SMS ELF on that boot device** instead. So a memory-card-less setup can still keep its settings — as long as the boot device is writable and present at each boot.

### Save after changing settings

Make your changes, then save through the settings menu so they're written out. If a setting only ever holds for the current session, it wasn't committed to storage.

ℹ Resetting to a clean state

 Deleting the config file returns SMS to **clean defaults** — which means **optional Autostart choices are off** and every Autostart toggle is off. If SMS is misbehaving after a lot of tweaking, clearing the config is a reliable way to get back to a known-good baseline; you then turn back on only the devices you use.

## Nothing starts up on the first boot

ℹ This is by design

 On a clean install, **optional Autostart flags start off**. SMS boots to the desktop with the device menu waiting, and you enable what you need. This is deliberate — it avoids forcing a device (such as an internal HDD) that you may not have, and it keeps the first boot fast and predictable.

To bring a device online, either turn on its **Autostart** toggle in settings (so it comes up automatically on future boots) or use the matching **Start <device> now** item to start it immediately without rebooting. Once you save your settings, your chosen devices will auto-start from then on. This is expected behaviour, not a fault.

## The screen looks corrupted in HD video modes

Known issue in HD output modes

 In the two high-definition modes (720p and 1080i) SMS does not draw the desktop wallpaper at all — it paints a plain gradient background instead. This is deliberate: the HD framebuffer leaves no room in VRAM for the full-screen artwork, and forcing it there used to comb the text and leave black bars. Everything else works normally; if you want the artwork, use a standard-definition mode (480i/480p/576p), all of which are unaffected.

HD modes use a much larger framebuffer than standard definition, which leaves little room for the full-screen desktop artwork; the result can be a corrupted or garbled background on the desktop. This is a known, open issue in the current theme at HD resolutions. Standard-definition output (480i) is not affected and is the recommended mode until this is resolved. If you do use an HD mode and the desktop looks wrong, switch the video/output mode back to 480i.

ℹ Still stuck?

 If none of the above matches your symptom, note exactly what you did, which device and file were involved, and what you saw on screen (a photo of the TV helps). That detail is what makes a problem fixable — vague "it doesn't work" reports are the hardest to act on.

## Reporting a freeze

If SMS stops responding during startup and a power cycle is the only way out, there is a build that can tell us exactly where it stopped.

**`Simple-Media-System-DIAG.elf`**, published alongside every release, is the same code with one addition: before each step of startup it prints a short code such as `E44` in the status bar, naming the step it is *about* to perform. If that step never finishes, **the code stays on screen**. So the last code visible is the thing that hung.

It needs no network and no memory card — the codes are drawn directly by the console's graphics hardware, so they still appear when almost nothing else is working. That is the whole point: it reports faults that a log file could not, because nothing was left running to write one.

### How to use it

### Run it

Boot `Simple-Media-System-DIAG.elf` the same way you boot the normal ELF.

### Watch the status bar

Codes appear at the bottom of the screen as startup progresses. On a healthy console they flick past quickly and you end up at the file browser.

### Note where it stops

If it freezes, write down or photograph the last code **and its text label** shown. A recording is even better — it captures the order they appeared in.

### Report it

Open an issue with that code, your console model, and what was connected. Include the label as well as the number; numeric phase values can overlap.

Use the standard ELF for normal playback — the diagnostic build is not faster or better, it simply talks more.

### Include a reproducible comparison

- The bundle's `build-info.json`, standard/packed/DIAG choice and exact release/run.
- Console model, boot method, storage device/format, video mode, cable/display and relevant settings.
- The unchanged original and converted file, or a short failing sample where you can share one; include a SHA-256, codecs, dimensions, frame rate, bitrate, audio tracks and timestamps.
- The exact trigger and time of failure. Record pause/seek/stop separately. Compare with 2.9rev4 where the issue reports that version working.
- Converter settings and logs when applicable. Successful encoding is a host result, not proof of console output. Remove SMB credentials from shared configuration/logs.

For a color fault, compare the same bytes and fixed display settings before applying any matrix/range transform. For stutter, compare sources and record timing before assuming fragmentation or bitrate is the cause. [Current fixes and unconfirmed reports](https://nathanneurotic.github.io/Simple-Media-System/known-issues.html) show which evidence is still missing.

## Quick reference

| Symptom | Most likely cause | Fix |
| --- | --- | --- |
| Video stutters / drops frames | Bitrate or resolution too high for the console (worse on slow devices) | Re-encode lower; use a faster device |
| Video won't open at all | Unsupported codec, or damaged / fragmented file | Re-encode to MPEG-4/XviD; re-copy the file |
| Device not in the menu | Not started, wrong format, or mutual exclusion | Autostart / Start-now; format FAT32/exFAT; free the shared bus |
| MX4SIO and MMCE clash | Both use the SIO2 slot bus | Run only one at a time |
| HDD APA and ATA/exFAT clash | Both own the internal ATA bus | Pick one; first to start owns the bus |
| SMB share won't connect | SMB1 disabled on the server | Re-enable SMB1; then check IP / credentials / firewall |
| UDPFS drive won't appear | No static PS2 IP, no server response, firewall, or SMB already owns the NIC | Save IPCONFIG; run UDPFSD with Modulo off; allow UDP 62966; reboot to switch stacks |
| Settings reset each boot | Wrong active config, failed write/remount, or unsaved changes | Check [the actual boot-folder/fallback path](https://nathanneurotic.github.io/Simple-Media-System/configuration.html), save and compare timestamps |
| Nothing auto-starts | Clean-install default | Enable Autostart for the devices you use |
| No wallpaper in 720p/1080i | Deliberate VRAM-saving gradient | Use an SD mode for skin checks; report actual corruption separately |

---
[Searchable manual](https://nathanneurotic.github.io/Simple-Media-System/troubleshooting.html) · [Edit the maintained source](https://github.com/NathanNeurotic/Simple-Media-System/blob/master/docs-site/content/troubleshooting.html)
