# SMSUdpfs — vendored UDPFS network-drive stack

`smap/`, `ministack/` and `udpfs/` are vendored from **[ps2max32/neutrino](https://github.com/ps2max32/neutrino)** (formerly rickgaiser/neutrino)
(`iop/smap`, `iop/ministack`, `iop/udpfs`), licensed **Academic Free License v3.0** — compatible with
SMS's AFL v2.0. Original authorship and copyright remain with the neutrino project and its
contributors; the UDPRDMA/UDPFS protocol work credits **rickgaiser** and **Maximus32**.

Protocol specs `udpfs/UDPFS.md` and `udpfs/UDPRDMA.md` are carried verbatim from upstream.

## What UDPFS is (and what it is not)

**UDPFS** ("UDP File System", transport **UDPRDMA**, port **0xF5F6**, service id **0xF5F5**) serves a
PC **folder** over UDP. SMS mounts it as the iomanX device **`udpfs:`** and browses/streams it like any
other filesystem — no disk image required.

**UDPBD** (legacy, port **0xBDBD**) is a *block device* protocol needing a disk image. Per the reference
server: *"UDPBD is a subset of UDPFS using block I/O messages (BREAD/BWRITE/INFO)."* SMS previously
shipped a UDPBD client (`smap_udpbd.irx`); it was removed — it was bound to the wrong dev9 ABI and could
never transfer a byte (see below), it cannot talk to a UDPFS server, and its stubbed `smap` export table
would have shadowed the real one and broken UDPFS. If block-device access is ever wanted, the correct
module is neutrino's `udpfs_bd.irx` (`UDPFS_BD=1`), which speaks block mode over this same UDPRDMA
connection and reuses this same smap+ministack.

## Local modifications (all deliberate — preserve them across any upstream re-sync)

1. **dev9 ABI rebind (`smap/include/dev9.h`) — load-bearing.**
   SMS's own v1.1 dev9 header is dropped in to shadow ps2sdk's v1.9 one (neutrino's `Rules.make` puts
   `-Iinclude` first). IOP loadcore binds by library NAME + MAJOR only and resolves stubs **by ordinal**;
   SMS embeds a dev9 **v1.1** clone (`iop/SMSDev9`), so a stock v1.9 build silently lands on the wrong
   ordinals and the SMAP DMA callbacks are never armed — the module loads clean and never transfers a
   byte. This is exactly why the old `smap_udpbd.irx` never worked. smap imports precisely the 7 symbols
   SMS v1.1 exports (ords 4,5,7,8,9,11,12), so no source edit is needed.
   **Gate:** `tools/irx_imports.py --assert dev9=1.1 irx/udpfs_smap.irx` — run by `tools/build_udpfs.sh`.
   Never remove it: this failure mode is invisible (not a load error; nothing reports it).

2. **`udptty_init()` dropped (`ministack/src/main.c`, `ministack/Makefile`, `ministack/src/imports.lst`).**
   Upstream redirects IOP printf over UDP. It does `close(0)`/`close(1)`, DelDrv/AddDrv the tty, then
   `while(1);` **forever** unless the reopened `tty00:` lands on exactly fd 0 and fd 1 (`udptty.c:75-89`)
   — a hard IOP hang with no diagnostic. SMS never reads it. `udptty.c/.h` are kept in-tree, unbuilt, to
   ease upstream re-sync; the `ioman` imports they alone needed are dropped too.

3. **`_retonly` moved after `END_EXPORT_TABLE`** (`smap/`, `ministack/` `src/exports.tab`).
   The ps2dev v2.0.0 `iopfixup` rejects an exported symbol sitting at `.text` offset 0 (indistinguishable
   from null). ps2sdk's own modules define `_retonly` last for this reason.

4. **`stdint.h` shim** (`*/include/stdint.h`). The ps2dev IOP toolchain ships no `<stdint.h>`.
   `uint32_t`/`int32_t` are typed on `long` to match ps2sdk's `_IOP` branch
   (`tamtypes.h:43-49` — `typedef unsigned long u32`); typing them on `int` compiles as the right *width*
   but is a distinct *type*, breaking every `uint32_t*` → `u32*` argument (e.g. `WaitEventFlag`).

5. **Build outputs renamed** to `udpfs_smap.irx` / `udpfs_ministack.irx` / `udpfs_ioman.irx`, mirroring
   wLaunchELF-R3Z and keeping them visually distinct from SMS's own SMSMAP/PS2SMAP. The registered
   library names (`smap`, `mstack`) are baked into the modules and unaffected by the filename.

## Reference implementation

**[saildot4k/wLaunchELF_R3Z](https://github.com/saildot4k/wLaunchELF_R3Z)** is a known-working UDPFS
client and was used to validate this integration. Its `src/init.c:612 load_udpfs_stack()` establishes the
load order reproduced here: DEV9 init → `smap` (no args) → `ministack` (`ip=<PS2 IP>`) → `udpfs_ioman`
(no args), each checked with `ID >= 0 && ret >= 0`. Its prebuilt modules import `mstack v1.0 {4,5,6}` and
`smap v1.0 {7}` — identical to what this tree builds. R3Z's `udpfs_smap.irx` imports dev9 **v1.9** because
R3Z loads *stock* `ps2dev9.irx`; SMS must use v1.1 instead (see modification 1). R3Z ships its udpfs
modules precompiled with no source, so it could not be diffed at source level.

Rebuild with: `sh tools/build_udpfs.sh` (requires Docker + `ps2dev/ps2dev:v2.0.0`).
