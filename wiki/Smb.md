# SMB Network Share

SMS has a built-in **SMB (CIFS) client**: point it at a shared folder on a PC or NAS over wired Ethernet and it appears in the device menu like any other storage, so you can browse the share and play your video, audio and pictures straight off the network — no need to copy files onto a memory card or USB stick first. In this fork you enter the server address, share, and login **entirely on-screen** with an on-screen keyboard; there are no config files to hand-edit.

Your server must offer SMB1 / CIFS — this is the #1 gotcha

 SMS speaks the original **SMB1 (CIFS)** protocol only. Modern Windows and modern Samba **disable SMBv1 by default** and lean on SMB2/SMB3, which SMS does **not** speak. If your share is on an up-to-date Windows PC or NAS, you will almost certainly have to **turn the SMB1 feature back on** (or use a device that still serves SMB1) before SMS can see it. If SMS refuses to connect and everything else looks right, assume SMBv1 is the cause first. See [Troubleshooting](https://nathanneurotic.github.io/Simple-Media-System/smb.html#troubleshooting) and the [limitation](https://nathanneurotic.github.io/Simple-Media-System/smb.html#limitation) below.

## Overview

An SMB (Server Message Block) share is just a folder on another computer that has been made available over the local network. Windows calls this "File Sharing"; Linux and NAS boxes usually run **Samba**, an open-source implementation of the same protocol. SMS includes a small SMB1/CIFS client that connects to one of these shares over the PS2's Ethernet port and lets you walk its folders exactly as if it were a local disc — pick a movie and press play, and SMS streams it over the wire.

On the device menu the share shows up as a **network device** alongside your other sources. Once connected, navigation, playback, subtitles and playlists all behave the same as they do for a local device — the only difference is that the bytes are arriving over Ethernet instead of off a disc or card.

ℹ SMB needs the network up first

 The SMB client rides on top of the PS2's TCP/IP stack, so the console needs a working wired network connection — an IP address and a link to your router or switch — before it can reach a share. Configure that on the [Network Setup](https://nathanneurotic.github.io/Simple-Media-System/network.html) page first; this page assumes the PS2 is already on your LAN.

SMB/host and UDPFS require separate IOP sessions

 SMS's ordinary SMB/host stack and the [UDPFS network drive](https://nathanneurotic.github.io/Simple-Media-System/network.html#udpfs) use different drivers for the one Ethernet controller. Whichever starts first owns it until reboot. Enable **Autostart SMB** — picking it automatically clears **Autostart HOST ( ps2dev )** and **Autostart UDPFS**, so the three can never fight.

## Requirements

- A PS2 **network adapter** and a **wired** Ethernet connection to the same LAN as your PC or NAS. SMS's network support is wired only — there is no built-in Wi-Fi.
- A PC, NAS, or other device on that LAN that is **sharing a folder over SMB1 / CIFS**. Because modern Windows and Samba ship with SMBv1 switched off, you generally have to **enable the SMB1 feature** on the server (or use a device/NAS that still serves it) — this is the single most common reason a share is invisible.
- The server's **IP address** on the LAN, the exact **share name**, and a set of **credentials** — a username and password, or a guest / anonymous login if the share allows it.
- The PS2 already on the network with an IP address (see [Network Setup](https://nathanneurotic.github.io/Simple-Media-System/network.html)).

## Setting up the share on your PC or NAS

This part happens on the computer, not on the PS2. The goal is a shared folder that answers to SMB1 and that you can reach with a known username and password (or as a guest). The exact steps differ by operating system, but the shape is always the same:

### Create and share a folder

Make a folder to hold your media — for example a "Movies" folder — and turn on file sharing for it. On Windows this is Properties → Sharing (or "Give access to"); on a NAS it is the box's Shared Folders / SMB service; on Linux it is a Samba `[share]` entry. **Write down the exact share name** you give it — SMS needs it spelled the same way, case included.

### Decide on access: user or guest

Either grant a specific **username** access to the share and set a **password** for it, or (if your OS allows it and you don't mind) enable **guest / anonymous** access so no password is needed. Note down whichever you choose — you'll type it into SMS.

### Make sure SMB1 is being served

On a modern Windows PC, enable the **"SMB 1.0/CIFS File Sharing Support"** Windows feature; on a modern Samba server, allow the old protocol (an `SMB1`-friendly `min protocol` in the Samba config). Many NAS units have a checkbox to allow SMBv1 as well. Without this step the share exists but SMS cannot speak to it.

### Find the server's IP address

Note the PC / NAS **IP address** on your LAN (for example `192.168.1.50`). SMS connects by IP. Give the machine a fixed / reserved address if you can, so it doesn't change between sessions.

## Configuring the SMB share in SMS

Everything below is done **on the console** with the pad. This fork replaces the old fiddly per-character picker with a real **on-screen keyboard**, so you type the server address, share and credentials directly — there is no config file to prepare on a PC beforehand. You add a share once; SMS remembers it.

ℹ The on-screen keyboard

 When you open one of the text fields, SMS pops up a full on-screen keyboard: move the D-pad to a character and press **Cross** to type it, use **Circle** to backspace and **Square** for a space, and commit the field with **Triangle** (or the OK key). It covers digits, upper- and lower-case letters and the common symbols (`. _ - / @ :` and more), so full server names, usernames and passwords are all typeable from the pad.

Open the SMB / network configuration from SMS's settings, then choose **Add server…** to create a new entry (or **Edit server…** to change an existing one). The add/edit form walks top-to-bottom through these fields:

| Field | What to enter |
| --- | --- |
| **IP address** | The server's LAN address, entered as four numbers. Each octet is set with an on-screen spinner rather than the keyboard — e.g. `192` · `168` · `1` · `50`. |
| **Server name** | A short name for the share/host. Keep it brief — SMS is happiest with short server names. |
| **Username** | The account allowed to access the share. Leave it as your guest login if the share permits anonymous access. |
| **Password** | The password for that account (blank / guest if the share is open). |
| **Client name** | The name the PS2 announces itself as to the server. A default is fine; you can set something recognisable if you like. |
| **Port** | The TCP port SMS connects to. A normal SMB server listens on `445` (or `139` for the older NetBIOS transport); leave the pre-filled default in place unless your server uses a non-standard port, and set it to `445` for a standard Windows / Samba share. |
| **Share (optional)** | The share name you noted on the server. You can leave this blank to browse the server's list of shares, or fill it in to jump straight into one named share. |

When the fields are filled in, choose the **Save Settings** item at the bottom of the form to store the entry. SMS keeps the server list in a separate `SMS.smb` file in the same directory as the general `SMS.cfg`, so it follows the configuration location and you do not have to re-type it next time.

ℹ Turn the network device on

 As with the other sources, SMS only brings up the network stack when you ask it to. The settings menu has an **Autostart SMB** toggle so the share is available automatically on boot, and — when networking isn't running yet and a server is configured — a **Start SMB server support** action so you can bring it up on demand without rebooting. On a clean install optional Autostart choices are off; you enable networking when you want it.

## Connecting & browsing

### Bring up the network

With the network configured (and Autostart SMB on, or started on demand), SMS connects to your LAN and then to the SMB server you saved.

### Select the network device

In the top row **Device menu**, use **Left / Right** to land on the network / SMB device, then press **Down** to drop into the file list.

### Browse the share

The middle **File menu** now lists the contents of the share. Move with **Up / Down** (use **L1 / L2** to page through long folders), press **Cross** to open a folder or start a file, and **Triangle** to go up one level. The current path shows in the status line at the bottom left.

### Play

Pick a video, track or picture and press **Cross**. Playback, subtitles and playlists work just as they do from a local device — during playback **Select** pauses, **Start** resumes, and **Triangle** stops and returns you to the browser.

Streaming over the network

 Playback quality depends on your LAN and the file's bitrate. A wired connection to a modern router handles typical SMS video fine, but very high-bitrate files may benefit from copying to a local device (USB, MX4SIO, HDD) instead. If a file stutters over SMB but plays cleanly from a local source, the network is the limiting factor.

## Troubleshooting

| Symptom | Most likely cause & fix |
| --- | --- |
| The share never appears / SMS can't connect at all | **SMBv1 is disabled on the server** — the number-one cause. Enable "SMB 1.0/CIFS File Sharing Support" on Windows, allow the old protocol in Samba, or use a device that still serves SMB1. |
| Can't reach the server, but the LAN seems fine | Check the **IP address** you typed matches the server's current address (it may have changed if it isn't reserved). Confirm the PS2 itself has a valid IP — see [Network Setup](https://nathanneurotic.github.io/Simple-Media-System/network.html). |
| Connects, but access is refused | Wrong **credentials**. Re-check the username and password, and confirm that account actually has permission to the share. If you meant to use guest access, make sure the share allows it. |
| Server is unreachable / times out | A **firewall** on the PC is often blocking SMB. Allow file sharing (ports `445` / `139`) through the firewall, and make sure the sharing service is actually running. |
| Right server, but the folder is empty or wrong | The **share name** is misspelled or points at the wrong folder. Re-open **Edit server…** and correct the Share field (or clear it to browse the server's share list). |

ℹ A quick sanity check

 Before blaming SMS, confirm another computer on the same network can open the share with the same IP, username and password. If a second PC can't reach it either, the problem is on the server (usually SMBv1 or the firewall), not on the PS2.

## Limitation: SMB1 only

SMS's network share support is built on the original **SMB1 (CIFS)** protocol. It does **not** speak **SMB2 or SMB3**, the newer versions that modern operating systems prefer. That is why a stock, up-to-date Windows PC or NAS won't be visible until you re-enable the legacy SMB1 feature on it. Speaking the newer protocols would require a substantial rewrite of the client, so for now the rule of thumb is simple: **if the server serves SMB1, SMS can use it; if it only serves SMB2/3, it can't.**

<details><summary>Why is SMB1 a security concern, and is enabling it safe?</summary>

SMBv1 is an old protocol with known weaknesses, which is exactly why Microsoft and the Samba project switched it off by default. Do not expose it to the internet. If you choose to use it, isolate the service to a trusted LAN or a dedicated media host. If you would rather not enable SMB1, use the [UDPFS network drive](https://nathanneurotic.github.io/Simple-Media-System/network.html#udpfs) or a local source such as [USB](https://nathanneurotic.github.io/Simple-Media-System/usb.html), [MX4SIO](https://nathanneurotic.github.io/Simple-Media-System/mx4sio.html) or the [internal HDD](https://nathanneurotic.github.io/Simple-Media-System/hdd.html).

</details>

## See also

- [Network Setup Get the PS2 on your LAN first](https://nathanneurotic.github.io/Simple-Media-System/network.html)
- [Settings Reference Autostart SMB & where config lives](https://nathanneurotic.github.io/Simple-Media-System/settings.html)
- [Devices Overview How all the sources compare](https://nathanneurotic.github.io/Simple-Media-System/devices.html)
- [Troubleshooting When something won't play](https://nathanneurotic.github.io/Simple-Media-System/troubleshooting.html)

Changing an already running network personality requires the explicit IOP restart prompt or relaunching SMS. An IOP restart also invalidates other mounted drivers; start the required storage again. See [Network Setup](https://nathanneurotic.github.io/Simple-Media-System/network.html).

---
[Searchable manual](https://nathanneurotic.github.io/Simple-Media-System/smb.html) · [Edit the maintained source](https://github.com/NathanNeurotic/Simple-Media-System/blob/master/docs-site/content/smb.html)
