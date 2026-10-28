# Settings Reference

This is the full reference for the SMS settings menu — every option, grouped exactly the way SMS groups them, with what each one does and the values it accepts. If you only want to turn a storage device on, jump to [Device settings](https://nathanneurotic.github.io/Simple-Media-System/settings.html#device-settings); if you're tuning subtitles or the picture, see [Player settings](https://nathanneurotic.github.io/Simple-Media-System/settings.html#player-settings) and [Display settings](https://nathanneurotic.github.io/Simple-Media-System/settings.html#display-settings).

ℹ How to reach the settings menu

 The settings menu is the same **SMS menu** you use for saving and quitting. Open it from the browser, then pick one of its top-level entries. Each entry opens a sub-menu of options. On a toggle, **Cross** flips it between `on` and `off`; on a value item, **Left/Right** cycles the value. **Triangle** goes back up a level. Nothing is committed to storage until you run **Save settings** (see [Saving your settings](https://nathanneurotic.github.io/Simple-Media-System/settings.html#saving)) — so you can experiment freely and just not save.

## The settings menu at a glance

The top level of the SMS menu has these entries. The first four open the option sub-menus documented on this page; the last four are actions.

| Top-level entry | What it opens / does |
| --- | --- |
| **Display settings…** | TV system, character set, language, image position, and an Advanced sub-menu for resolution & sync — see [below](https://nathanneurotic.github.io/Simple-Media-System/settings.html#display-settings). |
| **Device settings…** | Every storage/network **Autostart** toggle, the **Start … support** on-demand items, CD/DVD options and controller options — see [below](https://nathanneurotic.github.io/Simple-Media-System/settings.html#device-settings). |
| **Browser settings…** | Appearance of the file browser: background image, sound FX, sorting/filtering, panel colours, the reset-button action and Exit-to target — see [below](https://nathanneurotic.github.io/Simple-Media-System/settings.html#browser-settings). |
| **Player settings…** | Everything about playback: subtitles, audio/SPDIF, volume, colour resolution, scroll bar, and an MP3 sub-menu — see [below](https://nathanneurotic.github.io/Simple-Media-System/settings.html#player-settings). |
| Quick help | An on-screen list of the button shortcuts (browser and player). |
| Save settings | Writes your configuration to storage — see [Saving your settings](https://nathanneurotic.github.io/Simple-Media-System/settings.html#saving). |
| Shutdown console | Powers the PS2 off. |
| Exit SMS | Leaves SMS and returns to the boot browser it was launched from. |

Labels come straight from the app

 Every option name and value on this page is taken from the SMS build's own on-screen strings, so it should match what you see on the TV. Wording can differ slightly if you load a translated language file, but the layout and behaviour are the same.

## Device settings — storage & network

This is the menu you care about most on this fork: it is where you turn storage devices on. Each device has an **Autostart** toggle (whether it starts automatically at boot), and — when the device is *not* currently running — a matching **Start … support** item appears at the bottom of the menu so you can bring it up right now without rebooting.

### Autostart toggles

Each toggle shows `on` or `off`. On a clean install (no saved settings) every one of these is **off** — SMS auto-starts *nothing*, so you enable only the devices you actually use. Once you [save](https://nathanneurotic.github.io/Simple-Media-System/settings.html#saving), your choices persist across reboots.

| Option | Device it starts | Values |
| --- | --- | --- |
| **Autostart HOST ( ps2dev )** | The ps2dev `host:` stack (ps2client on a PC). One of three mutually exclusive network rows. | on / off |
| **Autostart SMB** | The SMB1/CIFS share client. One of three mutually exclusive network rows. See [Network Setup](https://nathanneurotic.github.io/Simple-Media-System/network.html). | on / off |
| **Autostart USB** | USB mass storage (`massN:`). See [USB](https://nathanneurotic.github.io/Simple-Media-System/usb.html). | on / off |
| **Autostart HDD APA** | The internal HDD via the classic APA/PFS path (`hdd0:`); SMS lists the drive's PFS partitions so you can pick one. See [Internal HDD](https://nathanneurotic.github.io/Simple-Media-System/hdd.html). | on / off |
| **Autostart HDD BDM** | The internal HDD via ATA as a BDM `massN:` device — this is the **exFAT** path added by this fork. See [Internal HDD](https://nathanneurotic.github.io/Simple-Media-System/hdd.html). | on / off |
| **Autostart MX4SIO** | The MX4SIO microSD adapter over SIO2 (`massN:`). The headline feature of this fork. See [MX4SIO](https://nathanneurotic.github.io/Simple-Media-System/mx4sio.html). | on / off |
| **Autostart i.LINK** | An i.LINK (IEEE 1394 / FireWire) drive as a BDM `massN:` device. See [i.LINK](https://nathanneurotic.github.io/Simple-Media-System/ilink.html). | on / off |
| **Autostart MMCE** | An MMCE device (SD2PSX / MemCard PRO2) in the memory-card slot (`mmce0:` / `mmce1:`). See [MMCE](https://nathanneurotic.github.io/Simple-Media-System/mmce.html). | on / off |
| **Autostart UDPFS** | The folder-served UDPFS network drive (`udpfs:`). Mutually exclusive with SMB/host for that boot. See [UDPFS](https://nathanneurotic.github.io/Simple-Media-System/network.html#udpfs). | on / off |

Shared resources — choose one member of each pair

- **MX4SIO and MMCE** both use the SIO2 / memory-card-slot bus. Only one can be active at a time — enable the one you're using, not both.
- **HDD APA and HDD BDM** both own the internal ATA bus. Pick one; whichever starts first owns the bus. Use **HDD APA** for a classic APA/PFS drive, or **HDD BDM** for a modern exFAT drive.
- **Network (SMB/host) and UDPFS** load different stacks for the one Ethernet controller. Whichever starts first owns it until reboot. Enable one, not both.

 See [Devices Overview](https://nathanneurotic.github.io/Simple-Media-System/devices.html) for the full picture.

### "Start … support" — bring a device up now

When a device isn't running yet, a matching item appears at the bottom of the Device menu. Selecting it starts that device immediately, so it shows up in the device row without a reboot. Once the device is up, its item disappears from the list. The wording matches each device:

- **Start SMB server support** (shown when an SMB server is configured)
- **Start HOST ( ps2dev ) support**
- **Start USB support**
- **Start HDD support** (APA)
- **Start HDD BDM** (ATA / exFAT)
- **Start MX4SIO support**
- **Start i.LINK support**
- **Start MMCE support**
- **Start UDPFS network drive**

The Device menu also ends with a **Refresh connections** item, which re-probes the running devices — handy after hot-swapping a card or drive so its contents are re-read.

ℹ Autostart vs. Start-now

 **Autostart** decides what comes up automatically at every boot (and is remembered once saved). **Start … support** is a one-shot "turn it on for this session" that doesn't change the Autostart toggle. A common flow: leave everything off, use **Start MX4SIO support** to test, then — once happy — flip **Autostart MX4SIO** on and [save](https://nathanneurotic.github.io/Simple-Media-System/settings.html#saving).

### CD/DVD and controller options

The rest of the Device menu covers the optical drive and controller behaviour, plus the network settings entry that links to the on-screen network setup.

| Option | What it does | Values |
| --- | --- | --- |
| **Network settings…** | Opens **Edit IPCONFIG.DAT** (the PS2's static IP, mask and gateway), the **Network protocol** selector, **SMB server…** — where share entries are added, edited and deleted — and the link parameters: **Operating mode**, plus **Duplex mode** and **Standard** when the mode is manual. See [Network Setup](https://nathanneurotic.github.io/Simple-Media-System/network.html). | opens sub-menu |
| **Controller port 2** | Tells SMS what is plugged into controller port 2 so it reads it correctly. | gamepad / remote control / none |
| **Pair Bluetooth controller** | Pairs a DualShock 3 or DualShock 4 to the connected USB Bluetooth dongle. The row appears only when the Bluetooth driver is available: connect the dongle, plug the controller into USB, run this command, then unplug the cable and power on the controller. | action |
| **Disable CD/DVD** | Turns the optical (CD/DVD) source on or off. Disable it if you never use discs, so SMS doesn't wait on the drive. | on / off |
| **CD/DVD speed** | Read speed for the optical drive. | low / medium / high |
| **Directional buttons** | Chooses how the D-pad is read — as a standard gamepad or as a remote control. | gamepad / remote control |

## Display settings — TV output & language

Everything about how SMS drives the screen, plus the interface language and character set. Most people set the TV system once and never touch the rest.

| Option | What it does | Values |
| --- | --- | --- |
| **TV system** | Chooses the video standard SMS outputs. | NTSC / PAL |
| **Character set** | The code page used when drawing text (menus, file names, subtitles) — pick the one that matches your language/filenames. | cycles through the built-in character sets |
| **Language** | English or the installed external `SMS.lng` pack; this row does not list every translation in the download ZIP. See [Languages & Fonts](https://nathanneurotic.github.io/Simple-Media-System/languages.html). | English / installed external pack |
| **Adjust image left / right / up / down** | Nudge the whole picture to centre it on your TV. Four separate items, one per direction. | press to nudge |
| **Advanced settings…** | Opens the advanced display sub-menu below (resolution, colour depth, sync parameters). Only needed for HD/VESA modes or fine tuning. | opens sub-menu |

### Advanced display settings

These control the raw output resolution and timing. They are for advanced users chasing a specific mode or fixing a picture that doesn't sync — the defaults are fine for standard NTSC/PAL. Changes here only take effect when you run **Apply settings** at the bottom of the sub-menu.

| Option | What it does |
| --- | --- |
| **Display height** | The output framebuffer height in scanlines. |
| **Display width** | The output framebuffer width in pixels. |
| **Color resolution** | Framebuffer colour depth (also selectable from the Player menu). See [Color resolution](https://nathanneurotic.github.io/Simple-Media-System/settings.html#colour-res). |
| **Synch. parameter 1 (video)** | Fine video-sync timing for the current mode. Adjust only if the picture rolls or is off-centre. |
| **Synch. parameter 2 (GUI)** | The equivalent sync tuning for the on-screen GUI layer. |
| **Synch. parameter 3 (Audio)** | The equivalent sync tuning for audio timing. |
| **Apply settings** | Commits the advanced display changes to the live output so you can see the result. |

HD modes and the background

 The 720p/1080i modes deliberately use a gradient instead of the desktop wallpaper because of VRAM use. A gradient in those modes is not the saved-skin restart report. See [HD background behavior](https://nathanneurotic.github.io/Simple-Media-System/themes.html#hd-known-issue) and [the open skin report](https://nathanneurotic.github.io/Simple-Media-System/known-issues.html#skin).

## Browser settings — the file browser's look

These change how the file browser looks and behaves, and what SMS does on the reset button / on exit. Colour items open a small palette picker; the rest are toggles or values.

| Option | What it does | Values |
| --- | --- | --- |
| **Skin** (shown at the top) | The active `.smi` skin. Shows the skin name, or `none`. See [Themes & Skins](https://nathanneurotic.github.io/Simple-Media-System/themes.html). | cycles installed skins / none |
| **Use background image** | Whether the desktop background image (skin/artwork) is drawn behind the browser. | on / off |
| **Sound FX** | Enables the browser's navigation sound effects. | on / off |
| **Sort filesystem objects** | Whether folders/files are shown sorted rather than in raw filesystem order. | on / off |
| **Filter media files** | Whether the browser hides files SMS doesn't recognise as playable media, so the list shows only openable files. | on / off |
| **Display HDL partitions** | Whether HD Loader-style partitions on the internal HDD are shown in the browser. | on / off |
| **Hide system partitions** | Whether the PS2's system partitions on the HDD are hidden from the list. | on / off |
| **Use exception handler** | Installs SMS's exception handler (a debugging/robustness aid). | on / off |
| **Active border color** | Palette colour of the highlighted (active) panel's border. | palette index |
| **Inactive border color** | Palette colour of the inactive panels' borders / info bar. | palette index |
| **Text color** | Palette colour of the browser's file-list text. | palette index |
| **Status line text color** | Palette colour of the bottom status line (path + clock). | palette index |
| **Selection bar color** | Palette colour of the highlight bar on the currently selected item. | palette index |
| **Reset button action** | What the PS2 reset button does while SMS is running. | cycles actions (incl. exit) |
| **Exit to** | Where **Exit SMS** sends you — the target program SMS returns to on exit. | cycles exit targets |

ℹ Palette / colour items

 The colour options (**Active border color**, **Text color**, **Selection bar color**, and the subtitle colours in the Player menu) pick from SMS's 16-entry palette. Cycle the value to preview each colour live in the panel it affects, so you can match your skin. See [Themes & Skins](https://nathanneurotic.github.io/Simple-Media-System/themes.html) for the palette itself.

## Player settings — playback, subtitles & audio

The biggest sub-menu. It covers subtitles, audio/SPDIF, volume, the scroll bar, colour resolution, an auto power-off timer, and an **MP3 settings** sub-menu for audio playback. Toggles are on/off; value items cycle with Left/Right; colour items pick a palette index.

### Subtitles

SMS shows external subtitle files over video. These options control loading, look and placement. Some (offset, font width/height) show a live **Sample** while you adjust them. See the dedicated [Subtitles](https://nathanneurotic.github.io/Simple-Media-System/subtitles.html) page for the full walkthrough.

| Option | What it does | Values |
| --- | --- | --- |
| **Autoload subtitles** | Automatically load a matching subtitle file when a video starts. | on / off |
| **Opaque subtitles** | Draw subtitles on a solid backing rather than transparent, for readability over busy scenes. | on / off |
| **Subtitle color** | Palette colour of normal subtitle text. | palette index |
| **Subtitle bold color** | Palette colour used for bold subtitle text. | palette index |
| **Subtitle italic color** | Palette colour used for italic subtitle text. | palette index |
| **Subtitle underline color** | Palette colour used for underlined subtitle text. | palette index |
| **Subtitle alignment** | Horizontal placement of the subtitle text. | left / center / right / center-right |
| **Subtitle offset** | Vertical position of subtitles on screen (shows a live Sample). | value in points |
| **Subtitle font width** | Horizontal size of the subtitle font (shows a live Sample). | value |
| **Subtitle font height** | Vertical size of the subtitle font (shows a live Sample). | value |
| **Subtitle MBCS** | Multi-byte character-set handling for subtitles (for languages that need it). | on / off |

### Audio & video output

| Option | What it does | Values |
| --- | --- | --- |
| **S/PDIF - Dolby Digital/DTS** | Passes AC3/DTS out over the optical S/PDIF port as a bitstream to an external decoder, instead of decoding on the PS2. | on / off |
| **Video auto 2:2 pulldown** | Applies 2:2 pulldown handling for correct playback of certain video cadences. | on / off |
| **Default volume** | The starting playback volume level. | value |
| **Color resolution** | Framebuffer colour depth used during playback / GUI. Higher looks better; lower saves video memory. | 16 bit / 32 bit |

### On-screen extras & scroll bar

| Option | What it does | Values |
| --- | --- | --- |
| **Display scroll bar time** | Whether the elapsed/remaining time is shown on the player's scroll (progress) bar. | on / off |
| **Auto power-off** | Automatically powers the console off after a period of inactivity. | off, or a number of minutes |
| **Scroll bar length** | The on-screen length of the player's scroll bar. | value |
| **Scroll bar position** | Where the scroll bar sits on screen. | top / bottom / off |
| **Scroll bar color** | Palette colour of the scroll bar. | palette index |
| **Volume bar color** | Palette colour of the on-screen volume bar. | palette index |

### MP3 settings (audio sub-menu)

Opened from the Player settings menu via **MP3 settings…**, this covers audio and playlist behaviour, including the order of video folders played with **Play all → Video**.

| Option | What it does | Values |
| --- | --- | --- |
| **Audio animation display** | Whether a visual animation is shown while audio-only files play. See [Audio & Playlists](https://nathanneurotic.github.io/Simple-Media-System/audio.html). | on / off |
| **Randomize playlist** | Shuffle playlists and Play all folders, including [Play all → Video](https://nathanneurotic.github.io/Simple-Media-System/video.html#folder-playback). Turn it off to keep the normal play order. | on / off |
| **Repeat mode** | Repeat the current playlist/track. | on / off |
| **Audio spectrum display** | Show a spectrum-analyser visual during audio playback. | on / off |
| **Autodetection parameter** | A tuning value for MP3 format auto-detection. | value |

## Saving your settings — and where they go

Nothing you change in the menus is permanent until you run **Save settings** from the top level of the SMS menu. Until then you can change anything and simply not save. There is also a button shortcut on some builds (`Select`+`Square`) that saves settings from the browser.

### Make your changes

Set your Autostart toggles, colours, subtitle look, and so on in the sub-menus. Try devices with **Start … support** first if you like.

### Run Save settings

Back out to the top of the SMS menu (Triangle), select **Save settings**, and confirm.

### Reboot to confirm

Your Autostart choices and preferences come back on the next boot.

Configuration location

Use **Save settings** after changes. On a remountable launch source, `SMS.cfg` normally lives beside the ELF even when a memory card is fitted; `SMS.smb` follows its resolved directory. See [Config Files & Paths](https://nathanneurotic.github.io/Simple-Media-System/configuration.html) for card-slot, network-file and fallback rules.

`IPCONFIG.DAT` has separate card/boot-folder rules. Back up the active config before renaming it to test clean defaults.

## See also

- [Devices Overview Which device is which, and the bus rules](https://nathanneurotic.github.io/Simple-Media-System/devices.html)
- [Network Setup On-screen IP, SMB & UDPFS](https://nathanneurotic.github.io/Simple-Media-System/network.html)
- [Subtitles Loading, styling & placement](https://nathanneurotic.github.io/Simple-Media-System/subtitles.html)
- [Themes & Skins Skins, palette & artwork](https://nathanneurotic.github.io/Simple-Media-System/themes.html)
- [MX4SIO Enable & play from microSD](https://nathanneurotic.github.io/Simple-Media-System/mx4sio.html)
- [Troubleshooting When a device or file won't behave](https://nathanneurotic.github.io/Simple-Media-System/troubleshooting.html)

<details><summary>Why is there both &quot;HDD APA&quot; and &quot;HDD BDM&quot; — aren&#x27;t they the same drive?</summary>

They are the same physical internal HDD, but reached two different ways. **HDD APA** uses the PS2's classic APA/PFS filesystem and reads media from the PFS partitions on `hdd0:` — the traditional SMS route. **HDD BDM** reads the drive as a modern **exFAT** volume over ATA, appearing as a `massN:` device like USB or MX4SIO — the path this fork adds so you can use a normally-formatted drive. They share the internal ATA bus, so only one can be active; whichever starts first owns it. Choose the one that matches how your drive is formatted. See [Internal HDD](https://nathanneurotic.github.io/Simple-Media-System/hdd.html).

</details>

<details><summary>I enabled Autostart MX4SIO but MMCE stopped working (or vice-versa)</summary>

That's expected. MX4SIO and MMCE both drive the SIO2 / memory-card-slot bus, so SMS can only run one of them at a time. Turn off the Autostart for the one you're not using, enable the one you are, and [save](https://nathanneurotic.github.io/Simple-Media-System/settings.html#saving). If you need to switch after one has claimed SIO2, save the intended choice and relaunch SMS. Disabling Autostart does not unload an already running driver. See [Devices Overview](https://nathanneurotic.github.io/Simple-Media-System/devices.html).

</details>

**Boot-source exception:** optional Autostart flags default off, but SMS may start/remount the device holding its ELF/config during startup. A source already visible after first boot does not mean another Autostart choice was saved. See [configuration discovery](https://nathanneurotic.github.io/Simple-Media-System/configuration.html).

---
[Searchable manual](https://nathanneurotic.github.io/Simple-Media-System/settings.html) · [Edit the maintained source](https://github.com/NathanNeurotic/Simple-Media-System/blob/master/docs-site/content/settings.html)
