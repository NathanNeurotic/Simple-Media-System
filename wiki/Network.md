# Network Setup

SMS has two ways to browse media over wired Ethernet: the established **SMB1/CIFS share** client and this fork's **UDPFS network drive**. SMB connects to a configured Windows or Samba share. UDPFS discovers a PC folder served by PS2-Servers UDPFSD and mounts it as `udpfs:`. Both use settings entered from the pad; no hand-edited network file is required.

Choose SMB/host or UDPFS for each boot

 The two paths load different network stacks for the PS2's one EMAC3 Ethernet controller. Whichever starts first owns the NIC until reboot. The menu now enforces this: **Autostart HOST**, **Autostart SMB** and **Autostart UDPFS** are a single choice — turning one on turns the other two off, and selecting the one already lit switches networking off entirely. This does not conflict with MX4SIO or MMCE because those use SIO2, not Ethernet.

## Requirements

- A fat PS2 with a **Network Adapter**, or a slim PS2 with built-in Ethernet.
- A wired Ethernet connection to the same LAN as the server PC. The PS2 has no built-in Wi-Fi.
- For SMB: a Windows, Samba or NAS host offering an **SMB1/CIFS** share.
- For UDPFS: [PS2-Servers](https://github.com/NathanNeurotic/PS2-Servers) running its **UDPFSD** folder server.

## Set the PS2's static IP on-screen

Open **Device settings → Network settings → Edit IPCONFIG.DAT**. Enter the PS2's own IP address, network mask and gateway one octet at a time, then choose **Save/Update IPCONFIG.DAT**. With a memory card present SMS writes `mc0:/SYS-CONF/IPCONFIG.DAT`, the same place the PS2 browser and other software expect it. **With no memory card fitted** it writes `IPCONFIG.DAT` beside `SMS.ELF` instead, and reads it back from there on the next boot — so a console with no card can still be given an address. UDPFS requires a valid static PS2 IP; SMB uses the same network configuration.

### Choose an address

Give the PS2 an unused static address on the same subnet as the server, such as `192.168.1.20` when the PC is `192.168.1.10`.

### Enter mask and gateway

For a typical home LAN the mask is `255.255.255.0`; use your router's address as the gateway. Match your actual network.

### Save

Choose **Save/Update IPCONFIG.DAT**. With a memory card fitted the file goes to the card, where the PS2 browser and other titles expect it; with no card fitted SMS writes it beside the ELF instead, and reads it back from there next boot — so a card-less console can still be networked.

## Choosing a network mode

SMS can use the network in three different ways, and the console can only do **one of them per boot**. They all drive the same single network adapter, and an IOP module cannot be unloaded once it has claimed it — so the choice is settled the moment one starts.

Because of that, the three autostart entries in **Device settings** behave as one setting rather than three switches:

| Mode | What it is for |
| --- | --- |
| **Autostart HOST** | `host:` access from a PC running ps2client — mainly a development convenience. |
| **Autostart SMB** | A Windows / Samba / NAS file share. See [SMB](https://nathanneurotic.github.io/Simple-Media-System/smb.html). |
| **Autostart UDPFS** | A folder served over UDP from a PC. See [UDPFS](https://nathanneurotic.github.io/Simple-Media-System/network.html#udpfs) below. |

Turning one on turns the other two off automatically. Selecting the one that is already lit switches networking off. Previously two could be enabled at the same time, and the second one simply failed with no message — that is no longer possible.

If you change the mode while a network stack is already running, SMS offers to restart the network there and then. Accepting reboots the console's I/O processor in place, which takes a moment and leaves SMS running; declining is safe — the choice is saved either way and applies next launch.

## SMB network shares

The SMB browser stores a server address, share, user, password, client name and port. Add or edit the server under **Device settings → Network settings → SMB server…** using the on-screen keyboard; see [SMB Network](https://nathanneurotic.github.io/Simple-Media-System/smb.html) for the complete field reference. Start it with **Start SMB support**, or set **Autostart SMB** to have it come up on every boot.

SMB1 is required

 SMS speaks SMB1/CIFS. Modern Windows disables SMB1 by default, so a Samba/NAS configuration that permits SMB1 is usually safer than enabling an obsolete protocol broadly on a general-purpose PC. Keep this service on a trusted LAN.

## UDPFS network drive

UDPFS serves a real PC directory over the UDPFS/UDPRDMA protocol. It is a **file-level** service: the folder mounts directly as `udpfs:`. It is not UDPBD, does not require a disk image, and does not appear as `massN:`.

### Configure the PS2 IP

Complete the static-IP steps above and save `IPCONFIG.DAT`.

### Serve the folder

Run the UDPFSD server from [PS2-Servers](https://github.com/NathanNeurotic/PS2-Servers), select the folder to expose, and keep **Modulo mode off**. The PC and PS2 must be on the same LAN; SMS discovers the server by broadcast, so there is no server IP to enter in SMS.

### Start the drive

Choose **Start UDPFS network drive**, or enable **Autostart UDPFS** and save settings. When discovery succeeds, browse the `udpfs:` device like any other source.

Earlier project hardware report

 Earlier project documentation reports UDPFS boot, browsing, playback and clean exit on real PS2 hardware. The current documentation audit checked the source/configuration and did not repeat those console runs.

## Troubleshooting

- **Check link and subnet.** Confirm Ethernet link lights and put the PS2 and server on the same subnet.
- **UDPFS says NOIP.** Save the PS2's static IP through **Edit IPCONFIG.DAT** first.
- **UDPFS discovery fails.** Start UDPFSD before retrying, keep Modulo mode off, and allow UDP port `0xF5F6` (62966) through the PC firewall on the trusted LAN.
- **The other network option disappeared.** One network stack already owns the NIC. Reboot, then start only the path you intend to use.
- **SMB reaches the host but cannot list or open shares.** Check SMB1 availability and the configured credentials. Administrative shares ending in `$` are intentionally hidden.

- [SMB Network Share fields and login](https://nathanneurotic.github.io/Simple-Media-System/smb.html)
- [Settings Reference Autostart and Start-now controls](https://nathanneurotic.github.io/Simple-Media-System/settings.html)
- [Devices Overview All sources and exclusion rules](https://nathanneurotic.github.io/Simple-Media-System/devices.html)

---
[Searchable manual](https://nathanneurotic.github.io/Simple-Media-System/network.html) · [Edit the maintained source](https://github.com/NathanNeurotic/Simple-Media-System/blob/master/docs-site/content/network.html)
