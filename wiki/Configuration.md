# Config Files & Paths

The active configuration follows the launch path when SMS can remount it. A memory card being present does not automatically move a USB-launched player's settings onto that card.

## Where SMS.cfg and SMS.smb live

| Launch source | Normal configuration location |
| --- | --- |
| Memory card, USB/MX4SIO/i.LINK/ATA mass volume, MMCE, PFS/HDD, writable UDPFS | `SMS.cfg` beside the launched ELF. `SMS.smb` follows the same resolved config directory. |
| SMB, developer `host:`, optical launch, or a boot mass device that SMS cannot remount after IOP reset | Fallback resolution: an available memory card can retain the card config; otherwise SMS searches writable USB/MMCE roots for an existing `SMS.cfg`, then prefers a USB target, then MMCE. The card path is the final fallback. |
| No usable launch directory | The configured memory-card default, normally `mc0:/SMS/SMS.cfg` (the selected card slot can differ). |

Examples: `mass0:/APPS/SMS/SMS.elf` normally uses `mass0:/APPS/SMS/SMS.cfg`; `mc1:/APPS/SMS.elf` normally uses `mc1:/APPS/SMS.cfg`. BDM mass numbering can change across boots. A UDPFS server must permit writes for config saved there; an unreachable/read-only boot source can cause fallback.

SMS remounts filesystem boot devices before its second settings load. That is why a saved skin or display setting may load later than the first boot screen. The October repaint fix addresses that sequence; [the reported restart case still needs acceptance](https://nathanneurotic.github.io/Simple-Media-System/known-issues.html#skin).

## IPCONFIG.DAT is separate

The network editor saves the PS2's static address/mask/gateway to `mc?:/SYS-CONF/IPCONFIG.DAT` when the configured memory card is usable. Without a card it uses the ELF's folder. Startup probes the boot-folder copy and the card fallback; a stale boot-folder copy can therefore matter. This file does not simply follow `SMS.cfg` into every fallback directory.

`SMS.smb` stores SMB server settings, including credentials, beside the resolved `SMS.cfg`. Keep your personal configuration out of public bug-report attachments or remove credentials before sharing it.

## Language, skin, font and palette assets

| Asset | Lookup |
| --- | --- |
| `SMS.lng` | Boot-folder copy first, then `mc?:/SMS/SMS.lng`. Enable the external language after installing a pack matching the build. |
| `Skins/Name.smi` | Boot-folder `Skins/` first, then `mc?:/SMS/Skins/`. Names are selected and saved in config. |
| `SMS.pal` | Boot-folder copy first, then the configured memory-card SMS directory. |
| `.mbf` fonts | Scanned in the SMS card/boot locations; select the desired font in the relevant menu. See [languages](https://nathanneurotic.github.io/Simple-Media-System/languages.html) and [subtitles](https://nathanneurotic.github.io/Simple-Media-System/subtitles.html). |

## Save, compare and reset

1. Use **Save settings** after changing autostart, language, skin or playback preferences.
2. If a setting reverts, check the exact launch directory and file timestamp; do not assume another copy on `mc0:` is active.
3. Before resetting, back up the active `SMS.cfg` and `SMS.smb`. Rename the config and relaunch to test defaults. A fresh config disables optional device autostart.
4. When comparing builds, keep a separate config copy for each and record which file each used.

Source: [main.c](https://github.com/NathanNeurotic/Simple-Media-System/blob/master/src/main.c), [SMS_Config.c](https://github.com/NathanNeurotic/Simple-Media-System/blob/master/src/SMS_Config.c), [SMS_IOP.c](https://github.com/NathanNeurotic/Simple-Media-System/blob/master/src/SMS_IOP.c) and [SMS_GUIDesktop.c](https://github.com/NathanNeurotic/Simple-Media-System/blob/master/src/SMS_GUIDesktop.c). The active target depends on successful remount/probe results.

---
[Searchable manual](https://nathanneurotic.github.io/Simple-Media-System/configuration.html) · [Edit the maintained source](https://github.com/NathanNeurotic/Simple-Media-System/blob/master/docs-site/content/configuration.html)
