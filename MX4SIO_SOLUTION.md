# MX4SIO support in SMS — developer review document

> Historical engineering report: this preserves the original MX4SIO reproducer,
> driver rationale and earlier EE read strategy. Later streaming changes apply
> different batching to other sources. Use the [current device guide](https://nathanneurotic.github.io/Simple-Media-System/mx4sio.html)
> and [open-issue status](https://nathanneurotic.github.io/Simple-Media-System/known-issues.html)
> for current behavior and acceptance; old file:line references are snapshots.

This document is written for a PS2 developer reviewing the MX4SIO work in SMS
(Simple Media System) for a bounty. It explains **what the bug actually was, why
the fix is the correct one (not a workaround), and how to reproduce the build**.
Every claim below is grounded in the shipped source; file:line references are
given so the reviewer can check each one against the tree.

---

## 1. Summary

MX4SIO is a microSD-over-SIO2 adapter that the modern PS2SDK exposes as a `mass:`
BDM block device. SMS could mount an MX4SIO card and list directories, but
**playback hard-stalled the moment a large file read was issued**. The root cause
is in the `mx4sio_bd` IOP driver: its read path uses unbounded busy-waits and a
no-timeout DMA-completion wait, and it holds the SIO2 bus across a long in-ISR
re-arm chain. Under the heavily-loaded SMS IOP, a single large multi-block read
stalls; the thousands of tiny reads that directory listing and format-probing
perform always survived because their chains are only 1–2 sectors.

The fix has two halves, both shipped:

1. **Driver hardening** (`patches/mx4sio_bd-sms.patch`, applied to ps2sdk's
   `iop/sio/mx4sio_bd`, rebuilt into the embedded `irx/mx4sio_bd.irx`): every
   SIO2 busy-wait is bounded, the no-timeout DMA wait is replaced by a bounded
   poll-based watchdog, and multi-block reads are chopped into small batches
   (`SPISD_MAX_BATCH` = 8 sectors / 4 KiB) with correct sector advance on a
   partial read.
2. **EE-side access pattern** (`src/SMS_FileContext.c`): `mass:` reads are issued
   to the device in `MX4SIO_RD_CHUNK`-sized (4 KiB) pieces — one safe driver
   batch per read, with the iomanX/SIF round-trip between reads giving the SD card
   its inter-command gap. This is the *same* shape as the small-read pattern that
   was always reliable.

Verified on real hardware: a 640×480 XviD/MP3 AVI and an MP3 both play smoothly
off an MX4SIO microSD; file-switch re-launch, settings persistence, and a clean
first boot all work.

Built on the project's pinned CI toolchain `ps2dev/ps2dev:v1.0` against Krah's
BDM base (commit `f716b3a`); the patched `mx4sio_bd.irx` is rebuilt with the
current (`mipsel-none-elf`) IOP toolchain and embedded via `bin2c`.

---

## 2. Background: how MX4SIO appears to SMS

SMS embeds its IOP modules and loads them at boot (`SMS_IOPReset` /
`SMS_IOPInit` in `src/SMS_IOP.c`). For the MX4SIO/BDM build the load order is:

```
iomanx → filexio (+ fileXioInit) → bdm → bdmfs_fatfs
   → [sio2man] → mcman → mcserv → padman
   → usbd → usbmass_bd → mx4sio_bd
```

(`src/SMS_IOP.c:248-260` for the core stack; `usbd`/`usbmass_bd` in
`SMS_IOPStartUSB`, `mx4sio_bd` in `SMS_IOPStartMX4SIO` at `:386-474`.)

In this stack:

- `mx4sio_bd.irx` is the block device (microSD over the SIO2 controller bus);
- `bdm.irx` is the Block Device Manager, `bdmfs_fatfs.irx` the FAT/exFAT layer;
- the card surfaces as `massN:` — the same abstraction OPL / NHDDL / wLaunchELF
  use.

Because MX4SIO shares the SIO2 bus with the controller (`padman`) and memory card
(`mcman`), SMS swaps the legacy `rom0:SIO2MAN` for the PS2SDK `sio2man` so the
driver can hook it; that swap is what forces the libpad/libmc moves discussed in
§7. The card is detected by `checkConnectedMassDev()` (`src/SMS_IOP.c:363-376`),
which `fioDopen`s each `massN:` unit.

**Why this only broke in SMS.** The same driver works in OPL/NHDDL/wLaunchELF.
Those are file *managers*: they read in small pieces and the IOP is otherwise
idle. SMS keeps the IOP continuously busy during reads (SPU/audio, GUI, pad
polling, mcman/padman all resident) and a media player streams large buffers.
The driver defects below only surface under exactly those conditions, which is
why prior attempts got the card mounting but stalled on playback.

---

## 3. Root-cause analysis

### 3.1 Symptom and evidence

The card mounts and directories list, but opening a media file stalls. The
decisive evidence is the **small-read-vs-large-read split**:

- Directory listings and format detection (a few sectors at a time) — **always
  worked.**
- A large single multi-block read (a streaming media buffer) — **stalled.**

That split points squarely at the driver's multi-block read path, not at the EE
file API or the filesystem layer.

### 3.2 Two compounding driver defects

**(a) Unbounded busy-waits, including in the ISR.** The original driver waited
for each SIO2 transfer to finish by spinning on a status bit with no bound —
including inside the DMA-completion interrupt handler `mx_sio2_dma_isr_rx`, where
interrupts are masked:

```c
while ((inl_sio2_stat6c_get() & (1 << 12)) == 0)
    ;
```

If a transfer stalls (bus contention while the host polls the pad/MC), that spin
never returns; in the ISR it strands the whole IOP, and the EE then hangs waiting
on it. The patch replaces every such spin with the bounded `mx_sio2_xfer_done()`
helper (`patches/mx4sio_bd-sms.patch`, `mx4sio.c` hunks): a real transfer
completes in microseconds, far under the cap, so the bound only ever fires on a
genuine stall — turning a permanent freeze into a recoverable, signalled error.

**(b) No-timeout DMA-completion wait over a long in-ISR re-arm chain.** A read is
interrupt-driven: the SIO2 DMA ISR re-arms the next sector from interrupt
context, and the worker thread blocked on `WaitEventFlag` with **no timeout**. A
single large transfer (hundreds of sectors) is therefore one long,
uninterruptible in-ISR re-arm chain; under load a single late or dropped SD data
token strands the worker on that wait forever. Small reads survived because their
chain is only 1–2 sectors. See the in-code rationale at
`refs/ps2sdk/iop/sio/mx4sio_bd/src/spi_sdcard_driver.c:611-639` (the
`SPISD_MAX_BATCH` block) and `:432-442` (the watchdog block).

These two stack: even with the busy-waits bounded, a single large read holds the
SIO2 lock across many internal batches with stale FIFO state and still stalls.
Both have to be addressed.

---

## 4. The fix — and why it is correct, not a workaround

A reviewer's first question is "is this a real fix or a band-aid?" The honest
answer: **both halves are real fixes**, and the EE-side chunking is the *correct
access pattern for this device*, not an avoidance hack.

### 4.1 Driver hardening (`patches/mx4sio_bd-sms.patch`)

Applied to ps2sdk's `iop/sio/mx4sio_bd`; the result is the embedded
`irx/mx4sio_bd.irx`. Three changes:

1. **Bound every SIO2 "transfer complete" busy-wait** with `mx_sio2_xfer_done()`
   (`mx4sio.c`), including the in-ISR ones. On timeout the RX ISR sets
   `cmd.abort = CMD_ABORT_NO_READ_TOKEN` and signals the event flag so the read
   aborts cleanly instead of hanging the IOP.
2. **Replace the no-timeout completion wait with a bounded watchdog.**
   `spisd_read_multi_do` now polls `PollEventFlag` in a loop
   (`READ_WATCHDOG_POLLS` × `READ_WATCHDOG_POLL_US` ≈ 300 ms cap) and clears
   stale signal bits before each transfer; a missing DMA interrupt becomes a
   recoverable `NO_READ_TOKEN` abort routed through the existing card-reset/retry
   path (`spi_sdcard_driver.c:444-516`). Adds `I_PollEventFlag` to `imports.lst`.
3. **Batch multi-block reads.** `spisd_read` splits a request into
   `SPISD_MAX_BATCH`-sector batches, keeping each in-ISR re-arm chain short and
   self-terminating, and — fixing a latent bug — advances the **sector number**
   (not just the buffer pointer) on a partial read
   (`spi_sdcard_driver.c:641-714`, the `b_sector = b_sector + results` line).

`SPISD_MAX_BATCH` is **8 sectors (4 KiB)** in the shipped driver
(`spi_sdcard_driver.c:630-639`). 16 KiB/32 KiB batches still stall under SMS
load (a late token strands the longer chain); 8 sectors matches the size of the
directory/4 KiB reads that were always reliable.

### 4.2 EE-side: the correct `mass:` access pattern (`src/SMS_FileContext.c`)

`mass:` files are opened through `fileXio` (the native BDM/IOMANX API), tracked
by a per-file `m_fXio` flag (`SMS_FileContext.c:1219-1226`, set in
`STIO_InitFileContext` at `:1618-1620`). Every read to a `mass:` fd is then
issued to the driver in `MX4SIO_RD_CHUNK` = 4 KiB pieces:

- `STIO_Fill` chunk loop: `SMS_FileContext.c:1338-1352`.
- `STIO_Stream` (the streaming path) chunk loop: `SMS_FileContext.c:1452-1485`.
- the chunk constant + rationale: `SMS_FileContext.c:14-23`.

This is **the right pattern for the device**, for two reasons:

1. One read = one safe block-transfer the driver reliably services (4 KiB = one
   8-sector batch). There is no oversized single transfer to stall on.
2. The iomanX/SIF processing between reads gives the SD card its natural
   inter-command gap — *exactly* the rhythm of the thousands-of-small-reads
   pattern that was always smooth. The chunk loop reproduces that rhythm on
   purpose rather than fighting it.

4 KiB is the empirically-confirmed reliable ceiling on hardware; 8/16/32 KiB
single reads stall.

---

## 5. Alternatives ruled out

These were tried and rejected — included to answer "did you try X?":

- **fileXio instead of fio.** Switching the EE API alone did **not** fix it; the
  stall lives *below* the EE file API (in bdmfs/fileXio's large-transfer path and
  the driver), so the choice of `fio` vs `fileXio` does not by itself decide it.
  (The shipped code does use `fileXio` for `mass:` — but paired with chunking, as
  the *combination* is what works.)
- **Shrinking the driver batch + per-batch SIO2 (re)locking.** Reducing batch
  size and taking the lock per batch did not make a single large EE read succeed
  on its own; the large transfer still stalled.
- **Larger EE chunk sizes (8/16/32 KiB).** All stalled on hardware. 4 KiB is the
  ceiling.

---

## 6. Additional fixes shipped

Each is small and independently justified.

- **fioSync guard on `mass:` stream reset** (`SMS_FileContext.c:1431-1436`). The
  legacy `STIO_Stream` `anBlocks == 0` reset calls `fioSync(FIO_WAIT,…)` +
  `fioSetBlockMode(FIO_WAIT)`. `mass:` reads are synchronous `fileXio` calls with
  no async `fio` op pending, and that legacy sync could stall after heavy use,
  surfacing as a "Loading indices" hang on file-switch re-launch. The reset now
  skips the fio sync/blockmode for `m_fXio` fds.
- **Settings load/save coherence** (`SMS_Config.c`). Save wrote via `fio` to the
  `mc0:` path, but load read via libmc (`MC_GetDir`/`MC_OpenS`) — incoherent on
  the modern iomanX + mcman stack, so settings appeared to save but never loaded.
  `SMS_LoadConfig` now reads via `fio` from the *same* `mc0:` path it was written
  to (`SMS_Config.c:291-313`), so settings persist.
- **libmc card-detection retry** (`_mc_get_info`, `SMS_Config.c:188-200`). The
  sio2man swap moved the memory card onto libmc, whose `mcGetInfo` returns a
  transient "card changed" status on the first query after init. A single query
  intermittently looked like "no card" and skipped save/load; the code now
  retries `MC_GetInfo`/`MC_Sync` until the status stabilises (used by both
  `SMS_LoadConfig` and `SMS_SaveConfig`).
- **Clean-boot default** (`SMS_Config.c:211`). With no saved settings,
  `m_NetworkFlags = 0`, so **no** device auto-starts at boot (no more forced HDD).
  The user enables HDD/USB/MX4SIO/network from the menu, persisted once saved
  (auto-start gates at `SMS_IOP.c:602-607`).

---

## 7. GUI integration

MX4SIO is a first-class device in the UI, distinct from USB:

- **Enable / autostart entry.** The device menu carries an "Autostart MX4SIO"
  toggle (`SMS_GUIMenuSMS.c:194-208`, handler `_automx4sio_handler` at
  `:1150-1156`, flag `SMS_DF_AUTO_MX4SIO`). The BDM device menu is sized to 8
  items vs 7 in the non-BDM build (`:526-530`).
- **"Start MX4SIO support" now entry.** Appended dynamically when MX4SIO is not
  yet running (`SMS_GUIMenuSMS.c:574-581`), wired to `_startmx4sio_handler` →
  `_start_device(SMS_IOPStartMX4SIO)` (`:1200-1206`, `:1168-1186`).
- **Distinct device icon.** MX4SIO units are told apart from USB by a
  **before/after unit-mask delta**: `SMS_IOPStartMX4SIO` records which `massN:`
  units existed before loading `mx4sio_bd`, then sets `g_Mx4sioMask` only for
  units that appear *after* (`SMS_IOP.c:442-474`). In the device-bar renderer, a
  `mass:` unit whose bit is in `g_Mx4sioMask` is remapped from device-id 0 (USB)
  to device-id 7 (`SMS_GUIDevMenu.c:250-255`), which draws the CDDA glyph rather
  than the USB icon (icon table `s_pBrowserDevIcons[7] = s_IconCDDA`,
  `SMS_GUIcons.c:1348-1356`). Unmount handling treats id-7 as a `mass:` device
  too (`SMS_GUIDevMenu.c:302-308`).

---

## 8. Theme

Cosmetic re-theme, kept cheap:

- **Jellyfish desktop background.** A JPEG (`images/jellyfish.jpg`, embedded via
  `bin2c` as `jellyfish_jpg`, Makefile rule `:70-72` and object at `:40`) is
  decoded **once** through SMS's existing JPEG decoder (`SMS_JPEGInit` /
  `SMS_JPEGLoad`) into a cached PSMCT32 texture (`_DecodeJellyfish`,
  `SMS_GUIDesktop.c:225-278`) and then blitted full-screen exactly like a skin
  image (`_DrawJellyfish`, `:323-333`). The procedural gradient is kept as a
  fallback behind the opaque image.
- **Regenerated credits** in `src/About_Data.s` (linked via `About_Data.o`,
  Makefile `:26`).

---

## 9. SMB status (not part of the bounty)

SMB is **outside the MX4SIO bounty scope**, but it was modernised along the way:

- SMS's SMB was migrated from the legacy custom SMB1 client (`iop/SMSSMB/`,
  `SMSSMB.IRX`) to the **modern ps2sdk `smbman`** module — the same battle-tested
  SMB1 client OPL uses. It is embedded (`smbman_irx` in the Makefile) and loaded
  in `src/SMS_IOP.c` (`SifExecDecompModuleBuffer(&smbman_irx, …)`); it presents an
  iomanX `smb` device reached via `fileXioDevctl`. The legacy `iop/SMSSMB/` tree is
  now **dead code** (not built, not loaded), so the old `SMB_Read` read-loop defect
  noted in earlier drafts is moot.
- SMB is now **fully GUI-configurable** — server, share, user, password and IP are
  entered on-screen with an on-screen keyboard (`src/SMS_GUISMBrowser.c`), with no
  pre-authored config file.
- The remaining real-world limitation is **server-side**: `smbman` speaks **SMB1**,
  which modern Windows/Samba disable by default. True SMB2/3 would be a separate
  client effort, well beyond a "MX4SIO support" bounty.

---

## 10. Build & reproduce

The embedded IRX are committed in `irx/`, so the EE app builds without step 1.

```sh
# 1) (optional) rebuild the patched MX4SIO driver IRX from a ps2sdk checkout:
cd <ps2sdk>
patch -p1 < <repo>/patches/mx4sio_bd-sms.patch
cd iop/sio/mx4sio_bd && PS2SDKSRC=<ps2sdk> make     # -> mx4sio_bd.irx
# copy the result into this repo's irx/mx4sio_bd.irx

# 2) build the EE app on the project's pinned toolchain:
docker run --rm -v "$PWD":/src -w /src ps2dev/ps2dev:v1.0 \
  sh -c 'apk add --no-cache build-base && make all'       # -> bin/SMS.elf
```

`make` embeds the BDM IRX set (`iomanx`, `filexio`, `bdm`, `bdmfs_fatfs`,
`sio2man`, `mcman`, `mcserv`, `padman`, `usbd`, `usbmass_bd`, `mx4sio_bd`) and the
jellyfish JPEG via `bin2c`, and links `-lmc -lpadx -lfileXio` (Makefile
`:42-50`). `BDM` defaults on (`BDM ?= 1`).

---

## 11. Testing performed

On real PS2 hardware (slim, with HDD + network adapter present):

- **AVI playback** — 640×480 XviD video + MP3 audio off an MX4SIO microSD
  (FAT32): smooth.
- **MP3 playback** — smooth.
- **File-switch re-launch** — selecting another file after one finishes no longer
  hangs at "Loading indices" (the §6 fioSync guard).
- **Settings persistence** — save then reboot reloads the saved config
  (the §6 load/save coherence + libmc retry fixes).
- **Clean first boot** — with no saved settings, no device is force-started; the
  card is detected and selectable as a `mass:` device with its own icon, and
  directory browsing works.

Diagnosis used temporary on-screen instrumentation (no serial/IOP-TTY on the
target); that instrumentation is removed — the shipped code contains only the
fixes above.

---

## 12. Limitations & future work

1. **Throughput headroom.** 4 KiB per read is the confirmed-safe ceiling and is
   smooth for the tested media. Raising `SPISD_MAX_BATCH` / `MX4SIO_RD_CHUNK`
   together, once a wider range of cards is confirmed stable, would increase
   throughput; both are single-point constants by design.
2. **Card breadth.** Tested on the author's cards; the bounded watchdog makes a
   misbehaving card fail soft (logged abort + retry) rather than freeze, but a
   broad card-compatibility sweep is future work.
3. **SMB** remains the legacy SMB1 stack (§9); modernizing to SMB2 is separate
   work.

---

## 13. Credits

- **Eugene Plotnikov** — original SMS.
- **Krah (KrahJolito)** — MX4SIO/BDM integration foundation (BDM module loading,
  libpad/libmc switch, device-menu wiring).
- **El_isra**, **Ripto** (submitter), **Berion** — additional work and testing.
- This branch — the driver hardening (busy-wait bounds, completion watchdog, read
  batching) and the correct EE `mass:` access pattern that make playback work,
  the settings/clean-boot fixes, the MX4SIO GUI integration and icon, the theme,
  and this document.
