# Sable Audit — Simple-Media-System, ROUND 3

- **Auditor:** Sable
- **Repo:** NathanNeurotic/Simple-Media-System
- **Pin (READ, verified in clone at start and end of this round):** `master` @ `e65ced39a27eee8d996e9257e21594d1cac1c93d` — same pin as Round 2; `git status` clean throughout.
- **Scope of this round:** (1) Category 6 allocation audit of the vendored codec internals (Vorbis codebook path first, then AAC/FAAD, WMA, MP3, FLAC, MPEG family, MPEG-PS demux); (2) container parsing **logic** line-audit for ASF, MP3/ID3, JPG (Round 2 did allocations only); (3) SMS_IOP.c boot sequencing read end-to-end — module load order and per-module failure handling; (4) inventory pass over the five IOP upstream snapshots Round 2 verdicted but did not line-read (SMSTCPIP, SMSMAP, SMSUMS, SMSPWROFF, USBD).
- **Mode:** READ ONLY. No build was run (no PS2 toolchain in this VM); every performance/runtime statement is structural and labelled as such.
- **Rule:** LESS, NOT MORE. No secret values are printed (SMS-035 already covers the SMB password topic and is not repeated).
- **Framing applied throughout:** a vendored upstream snapshot is **inventory + risk note**, never a numbered finding — even when the inherited code has real hostile-input weaknesses — unless a hunk is locally modified / locally owned. Ownership was decided from file headers and `git log` (the whole codec layer arrived in the SMS 2.9 import `d704063` and has received only mechanical commits since: assembler-register fixes, `long`-type removal, DOS→Unix newlines).

---

## 1. Task 1 — Vendored codec internals, Category 6 allocation audit

### 1.1 Vorbis (src/SMS_OGG.c, 5,533 lines) — the named surface, read in full around every file-driven allocation

**Provenance (READ):** header at SMS_OGG.c:1-10 — "THE OggVorbis SOURCE CODE IS (C) COPYRIGHT 1994-2002 by the XIPHOPHORUS Company … Adopted for SMS in 2007 by Eugene Plotnikov". This is a libvorbis ~1.0-era single-file amalgamation (codebook.c / info.c / floor / residue / mapping), imported at `d704063`; `git log --follow` shows no semantic local edits since. **Verdict: upstream snapshot. Everything below is inventory + risk notes, not findings.**

Setup-header counts are bit-width-bounded and consistent with the port's own tables — checked and cleared: `books` = 8 bits + 1 ≤ 256 = `book_param[256]`, `floors`/`residues`/`maps`/`modes` = 6 bits + 1 ≤ 64 = the `[64]` arrays (include/SMS_OGG.h:196-203, identical sizes to upstream). No count can index past its array.

The codebook path (the surface Round 2 named, SMS_OGG.c:3170-3220) — every file-supplied count/length that reaches an allocation:

| Site | File-supplied source | Reaches | Verdict |
|---|---|---|---|
| SMS_OGG.c:3170, :3190 | `entries` = 24-bit field (:3167), up to 16,777,215 | `lengthlist = malloc(sizeof(int) * entries)` — up to ~64 MB requested on a 32 MB machine; **result never NULL-checked** before the fill loops write through it | Upstream risk note: hostile .ogg header → failed allocation → NULL write / crash during setup. Inherited from libvorbis of this vintage; later upstream releases added the checks this snapshot predates |
| SMS_OGG.c:3220 | `quantvals`: maptype 1 = derived by `_book_maptype1_quantvals` (:3128); maptype 2 = `entries * dim` in 32-bit `int` (:3214) — dim is a 16-bit field (:3166), so the product can **wrap** (e.g. entries 2²⁴ × dim 2¹⁶) | `quantlist = malloc(sizeof(int) * quantvals)`, unchecked, then filled for `quantvals` entries | Upstream risk note: the wrap is self-consistent inside unpack (fill loop uses the same wrapped count), but the decode side below recomputes the un-wrapped product — the two disagree exactly when the header lies |
| SMS_OGG.c:3795 (`_book_unquantize`) | `aN * dim` with `aN` = entries | `calloc(aN * dim, sizeof(float))`, then maptype-2 loop writes `quantlist[j * dim + k]` and `r[count * dim + k]` for all entries (:3824-3843) | Upstream risk note: with a wrapped `quantvals` (§above) the quantlist is smaller than `entries * dim`, so decode reads past it; with an un-wrapped huge product the calloc simply fails and returns NULL into a dereferencing caller. Same inherited class |
| SMS_OGG.c:3852-3953 (`vorbis_book_init_decode`) | `entries`, code lengths | `codelist` / `dec_index` / `dec_codelengths` mallocs sized from `used_entries` (≤ entries), `dec_firsttable = calloc(1 << dec_firsttablen)` | Upstream risk note: all sized by the same header values; no independent SMS-side bound exists anywhere in the chain |
| floor0/floor1/residue0/mapping0 unpack (:3262-3340 and following) | 6-bit counts + per-type fields, each validated against its `VI_*B` bound and `ci->books` before use (e.g. floor0 books checked at the unpack site) | Fixed-size `calloc(1, sizeof(*info))` per item | **Bounded — the well-behaved part of the header parser.** Counts cannot exceed the §1.1 table sizes |

**Net Task-1 Vorbis verdict:** the one genuinely dangerous inherited surface in the codec layer is the static-codebook unpack/decode pair above (unchecked, overflowable, header-driven). It is upstream code the user did not write and has not locally modified; the LESS-NOT-MORE-correct disposition is this risk note, not a local patch programme against a 2002 snapshot. If the owner ever touches it, the whole fix is upstream's own later codebook.c, taken whole — not hand-added checks.

### 1.2 AAC / FAAD (src/SMS_AAC.c, 8,302 lines)

**Provenance (READ):** header — FAAD2, "Copyright (C) 2002 M. Bakker", `$Id: decode.c,v 1.16 2004/04/03`, "Adopted for SMS and optimized for R5900 CPU by Eugene Plotnikov". **Upstream snapshot.**

Allocation population (grep, 27 sites, all eyeballed in context): every site is one of (a) fixed `sizeof(struct)` (codec/handle), (b) a constant product (`1024 * sizeof(pred_state)` at :6703/:7444/:7448, `64 * sizeof(float)` SBR — already SMS-016), or (c) a channel-count product (`2 * channels * 10/20 * sizeof(float)` at :3671/:3679; `mul * 1024` at :6718/:7461) where `channels` comes from FAAD's own configuration layer, not from a raw file field used unvalidated. **No file-supplied count or length reaches a malloc/memcpy in this file beyond what SMS-016 already covers. No new item.**

### 1.3 WMA (src/SMS_WMA.c, 2,112 lines)

**Provenance (READ):** header — "Copyright (c) 2002 The FFmpeg Project" + Plotnikov PS2 code (2007). **Upstream snapshot (early FFmpeg WMA decoder).**

Only two heap sites besides the codec struct: `run_table = malloc(n * sizeof(uint16_t) * 3)` (:1206), where `n` is the VLC table size built from the decoder's static code tables during init; and `window = malloc(sizeof(float) * n)` (:1419), where `n` is the frame length selected from the header's frame-length exponent via the codec's fixed block-size ladder. Neither is a raw file count. **Inventory only.**

### 1.4 MP3 (src/SMS_MP123.c, 6,729 lines)

**Provenance (READ):** header — mpg123 project, "initially written by Michael Hipp", "Adopted for SMS in 2006 by Eugene Plotnikov". **Upstream snapshot.**

The file contains exactly **one** heap allocation: `calloc(1, sizeof(SMS_Codec))` at :4753. Decoding runs on fixed internal tables/buffers and the SMS ring buffer. **There is no file-supplied allocation surface in the MP3 decoder. Inventory only — cleanest codec in the tree by this measure.**

### 1.5 FLAC (src/SMS_FLAC.c)

**Provenance (READ):** header — "(c) 2005-2009 Eugene Plotnikov … Don't remember where the original source code comes from." Provenance is honestly unknown *in the file itself* (the SMS-048 pattern, inside a source header).

Allocation population: exactly one `calloc(1, sizeof(SMS_Codec))` at :587; no other malloc/calloc in the file — frame/subframe decoding works out of the codec struct's fixed buffers. **No file-supplied size reaches an allocation. Inventory only.** (The unknown provenance is recorded here for completeness; SMS-048 already covers the repo-level version of this problem and is not repeated.)

### 1.6 MPEG video family (src/SMS_MPEG.c, SMS_MPEG12.c, SMS_MPEG4.c, SMS_H263.c, SMS_MSMPEG4.c, libmpeg.c + libmpeg_core.S)

**Provenance (READ):** libmpeg.c header — "Based on reference software of MSSG" (the MPEG Software Simulation Group reference decoder), Plotnikov 2006-2007; the SMS_MPEG.c context layer is FFmpeg-mpeg12-shaped. **Upstream-derived snapshot family.**

The one file-driven allocation cluster: `_mpeg_alloc_picture` (SMS_MPEG.c:42-53) callocs six per-picture tables sized from bitstream dimensions (`m_MBStride × m_MBH`, `m_B8Stride × m_MBH × 2` — macroblock geometry out of the sequence header). **None of the six results is checked**, and :49 immediately does pointer arithmetic on one (`m_pMotionVal = m_pMotionValBase + 2`), so an allocation failure under memory pressure is a wild-pointer crash in the decode loop, not a decode error. Whether any earlier layer caps the sequence-header dimensions was **not traced** (INFERRED reachability: a crafted sequence header inflates the calloc requests; legitimate DVD/VCD geometry is small). Upstream-shaped code — **risk note, not a finding.** This is the MPEG family's analogue of SMS-016, in vendored code.

### 1.7 MPEG-PS demux (src/SMS_ContainerMPEG_PS.c) — locally owned, logic now read

Unlike the decoders, this demuxer is **actively locally maintained**: `git log` shows `39d762f` "fix(mpeg): retry codec probes and reject incomplete packets", `3db9048` "Harden MPEG-PS probing, failed seeks and playlist cleanup", `94c4fd8` (2026-11 triage). Round 2 read its PES allocation site (:372-378 — malloc bounded by the 16-bit PES length, checked, read-back verified) and called it the model site. This round read the header-length logic end-to-end: `_read_header` (:156-296) decrements the 16-bit packet length field-by-field with a `goto redo` on **every** underflow (:195, :206, :217, :224, :238, :274, :281), and the unbounded-length video case (PES length 0, sentinel `0x80000000` at :519) is handled by streamed reads into the fixed container buffer, not by an allocation. **Verdict: logic bounded at every step read; no new finding.** The allocation-only gap Round 2 left for this file is closed.

---

## 2. Task 2 — Container parsing logic (ASF, MP3/ID3, JPG)

### 2.1 ASF (src/SMS_ContainerASF.c, 723 lines) — read in full

**Ownership:** FFmpeg-2007 asf.c lineage (header credits Bellard 2000/2001, Plotnikov adoption 2007), but **locally maintained today** — `git log` shows `dfe8995` "Containers: guard MOV/ASF stream-array against crafted/corrupt files" and `94c4fd8` (2026-11 triage fixes), and the file carries the same modern hardening-comment style as the MPEG-PS work (e.g. the SMS_MAX_STREAMS cap at :238). Findings here are therefore legitimate under this audit's framing.

What is sound (READ, recorded so it is not re-chased): the header loop rejects any sub-object with `lGSize < 24` (:175); stream count is capped before allocation (:238); stream IDs are masked to 0x7F before indexing the 128-entry tables (:259, and in the bitrate object); `packet_obj_size` is rejected unless `0 < size < 2²⁴` (:524); the fragment copy is preceded by a complete bounds check against the accumulation buffer (:601-605) and the realloc'd buffer is NULL-guarded by that same check; `packet_length ≥ 2²⁹` and `padsize ≥ packet_length` are rejected in `asf_get_packet` (:450-451).

**One real hole remains — SMS-050 below.** Noted but **not** promoted (READ, with reasons): the double `DO_2BITS` into `padsize` (:446-447) looks like a bug but is functionally harmless — the second read is the true padding-length field, the discarded first read is the sequence field this port tracks elsewhere, and `rsize` correctly accounts for both reads; the stream-bitrate loop (:401-410) trusts the file's stream count and can read past its object into following bytes (parser desync only — every value lands in a masked 128-entry table or is discarded, and the outer loop then fails on GUID mismatch); negative `File_Skip` amounts cast to `uint32_t` at :372/:413 on malformed stream objects (seek-to-EOF → clean fail, not corruption); the `realloc` at :583 assigns `pkt.alloc` before the result is known, so a failed realloc leaks the old buffer — but the :601 NULL guard prevents any write through the NULL, and the next fragment retries the allocation.

### 2.2 MP3 probe + ID3 (src/SMS_ContainerMP3.c, 145 lines; src/SMS_Container.c:360-393)

Read in full. **The ID3v2 skip is textbook-correct:** `_id3_match` (SMS_Container.c:360-367) validates the syncsafe property (all four size bytes < 0x80, version bytes ≠ 0xFF) *before* the size is trusted; the size is assembled from 7-bit groups (:381-384, maximum 0x0FFFFFFF — no overflow is possible); `File_Skip` consumes it. The probe itself (SMS_ContainerMP3.c:26-107) requires a second valid frame header at exactly `m_FrameSize` before accepting (the `g_Config.m_MP3AutoPar` loop), treats a lone header-like word at EOF as not-audio (:61-66), and byte-slides with a hard 16,384-iteration cap. **No finding — this is the best-written probe in the container set.**

Noted, not promoted (INFERRED, marginal): an ID3v2.4 **footer** (10 extra bytes, footer flag) is not skipped, since the skip is header-size only. For MP3 the probe's sync scan absorbs it. For FLAC (SMS_ContainerFLAC.c:24 calls the same skip before its magic check) a footer-bearing ID3 tag would misalign the `fLaC` check by 10 bytes and the file would not be recognised — a false-negative on an exotic tagging combination, not a safety issue.

### 2.3 JPG (src/SMS_ContainerJPG.c, 94 lines)

Read in full. The container is glue only: the entire header validation is one `File_GetUShort == 0xD8FF` (SOI, big-endian read) at :51; all marker/SOF parsing lives in SMS_JPEG.c (of which Round 2 read the caption/artwork paths only — the decode-side marker walk remains partially unread, see §6). Two structural notes, neither promoted: the "negative FileContext pointer is actually a tagged SMS_List pointer" convention at :42-49 dereferences the tagged pointer's head before any check beyond the container-ID of its first string — an internal caller convention, fragile if a future caller passes a genuine error value, sound for the current callers (INFERRED); and `_Destroy` (:25-38) destroys the file list but the viewer pointer is used unguarded if `SMS_JPEGViewerInit` had failed — the init result at :84 is not checked, the same unchecked-small-alloc family as SMS-045 but in a destroy path that only runs after a successful open. **No new finding in the container itself.**

### 2.4 SMS-052 — a second hole found while tracing the ASF audio path

The ASF audio branch calls `SMSContainer_GetWAVHeader` (SMS_ContainerASF.c:265), as does AVI (SMS_ContainerAVI.c:599). Reading that shared helper's logic (not just its allocation, which Round 2 covered) exposed an exact-one-value defect — filed as SMS-052 below.

---

## 3. Task 3 — SMS_IOP.c boot sequencing, read end-to-end (1,880 lines)

### 3.1 Load order (READ)

**Phase 0 — `SMS_IOPReset` (:337-536), called from main.c at boot (afExit=0), on exec-exit (afExit=1), and on the in-place network switch (afExit=2/SWITCH):** SIF teardown → `SifIopReset` (empty-arg on exit/switch, `rom0:UDNL rom0:EELOADCNF` on boot, `:425`) → `SifIopSync` spin (`:431`, `:447`) → sbv patches → RCX remote-receiver load/start/open → **SMSUTILS** blob (`:472`) → BDM only: **iomanx → filexio → fileXioInit → bdm → bdmfs_fatfs → sio2man** (skipped only on exec-exit, `:498`) **→ mcman → mcserv → padman** (`:475-506`); non-BDM: SIO2MAN blob + rom0 PADMAN/MCMAN/MCSERV (`:511-515`) → `SIF_BindRPC(SMSUTILS)` (`:525`).

**Phase 1 — `SMS_IOPInit` (:1429-1790):** `_read_ipconfig` (early attempt) → **DEV9** (`_load_module(1)`, hoisted) → config-device re-mount dance (MMCE / USB-then-MX4SIO / HDD / UDPFS by config-path prefix, each with a probe and an `lFSGone` degrade-to-memory-card path, `:1479-1610`) → config reload + locale + ipconfig second attempt → **LIBSD** (`SifLoadModule("rom0:LIBSD")`, with the embedded freesd fallback under BDM) → `SMS_IOPDVDVInit` (SMSU RPC) → **AUDSRV** (`_load_module(0)`, only if LIBSD) → **POWEROFF** (`_load_module(2)`) → DEV9 keep/shutdown decision → `SPU_Initialize` (only if LIBSD) → AUTO starters in fixed order: **HDD (PFS) → network (exactly one of HOST/SMB via `SMS_IOPStartNet`, or UDPFS) → USB → MX4SIO → ATA-BDM → iLink → MMCE** → poweroff thread + SIF command handlers → `SPU_LoadData` → RC/RMMAN.

**Lazy starters (menu/auto later):** `SMS_IOPStartDS34USB/BT`, `SMS_IOPRefreshMass`, `SMS_IOPNetSwitch` (full reset + rebuild, :1794-1880 area — order: save config → quiesce DS34 poller → unmount PFS → reset → forget all residency state → restart requested stack; read in full, no item).

### 3.2 Per-module failure handling (READ)

| Module / step | Load site | Result checked? | On failure |
|---|---|---|---|
| IOP reset + sync | SMS_IOP.c:425, 431, 447 | n/a — unbounded spins `while(!SifIopReset(...)){}` / `while(!SifIopSync()){}` | **Hang by design** (the reset protocol has no timeout); the SMS_DIAG breadcrumb system (E04-E14) exists precisely so a wedge here names its statement on screen. Documented, measured history in the :245-335 comments |
| SMSUTILS blob | :472 | **No** (`&i` discarded) | Silent degrade: `SIF_BindRPC` at :525 and the SMSU RPCs (DVDVInit, total-free-mem) run against a missing server — bounded per the :1694 comment ("SIF_BindRPC is separately bounded now"), so degrade-not-hang is the current intent |
| iomanx / filexio / bdm / bdmfs_fatfs | :475-490 | **No** | Silent; would be catastrophic (no iomanX devices at all) but these are embedded IRX whose load is deterministic absent IOP-heap exhaustion |
| sio2man / mcman / mcserv / padman | :498-506 (BDM); :511-515 (rom0) | **No** | Silent: memory card and/or controller simply never appear |
| DEV9 (blob index 1) | `_load_module` :204-216, called :1463 | **Yes** — side effect: `SMS_IOPF_DEV9_IS` set only when `lRes >= 0` (:213) | Correct degrade: network, HDD, UDPFS starters all gate on DEV9_IS and refuse cleanly |
| LIBSD | :1663 + freesd fallback :1680 (BDM) | **Yes** — `s_fLIBSD` | **The model row:** no LIBSD → no AUDSRV → no SPU bind → silence instead of the DESR boot hang (diagnosis preserved in the :1648-1662 comment) |
| AUDSRV (index 0) | :1713 | By proxy (gated on `s_fLIBSD`) | No audio; everything else boots |
| POWEROFF (index 2) | :1722 | **No** | Silent: the power-button thread (`:1240-1280`) still runs, its ioctls just fail. The E51/E52 crumbs bracketing this load show a hang *inside* this module's start was chased on DESR-7100 hardware |
| PS2ATAD / PS2HDD / PS2FS (via `SMS_IOPStartHDD`) | :1150 loads; :1155 status ioctl | **Loads no; outcome yes** — `SMS_IOPF_HDD` set only if `PS2HDD_IOCTL_STATUS` returns 0/1 | Degrade for the flag — but `s_AtaBusOwner = 1` is assigned at :1152 *before* the status check, so a failed load still claims the ATA bus against ata_bd for the whole boot (see SMS-051) |
| PS2IP → PS2SMAP → smbman / PS2HOST (`SMS_IOPStartNet`) | :601-628 | **Yes**, chained: each `i >= 0` gates the next | Correct staged degrade; `s_NetOwner = 1` is claimed *before* the loads **deliberately** (comment at SMS_IOPNetLiveMode: the claim must stand even on partial failure so UDPFS never lands on a half-built NIC) — a documented trade-off, not an oversight |
| UDPFS smap / ministack / ioman (`SMS_IOPStartUDPFS`) | :946-985 | **Yes**, each stage, with a per-stage `GUI_Error` (lE2/lE3/lE4) and residency flags set only on success | Correct degrade + retryable (the probe failure lE5 leaves the device registered and the menu row offered) |
| MX4SIO / MMCE / DS34USB / DS34BT | :767, :1037, :1407, :1445 | **Yes** — load result gates the residency flag, with the rule stated in comments ("Flag RESIDENCY, not the attempt") | Correct degrade; the file's own modern standard |
| **USB (usbd / usbmass_bd), ATA-BDM (ata_bd), iLink (iLinkman / IEEE1394_bd)** | :642-647, :835-845, :792-802 | **No** | **SMS-051** — flags/ownership claimed on the attempt, not the residency |

**Phase summary:** the boot path never hangs on a *module* failure except at the two reset spins (inherent to the IOP reset protocol and now instrumented); everything else degrades. The quality split is generational: every starter written or rewritten recently checks its load result and degrades with a named error; the three oldest starters (USB, ATA-BDM, iLink) and the Phase-0 core loads do not check at all. Of those, only the three starters turn a failed load into a *false residency claim* with boot-long consequences — that subset is SMS-051. The Phase-0 unchecked loads (SMSUTILS, iomanx chain, mcman chain) are recorded in the table above as known-silent, not promoted: their failure modes are total-and-obvious (no card, no devices) rather than misleading, and they have no retry path a check could feed.

---

## 4. Task 4 — IOP upstream snapshots: inventory pass

Method: tree listings, file headers, and full `git log` per tree. **Headline result: all five trees are pristine upstream snapshots at their vintage. There are no local C-source patches in any of them.** Every tree arrived in the SMS 2.9 import (`d704063`) and was afterwards touched *only* by the DOS→Unix newline commit (`2097680`) — except SMSUMS and USBD, which also received `2d36c01` "Fixes to make SMS work with new PS2SDK (#1)" (2017, contributor belek666). That commit was read in full: it changes **only the two Makefiles** (adds `$(PS2SDK)/common/include` to the include path, switches the blob compression from `lzma` to `xz --lzma2`, updates the Rules.make include path) — build compatibility, zero source lines. (It is also the historical evidence that these trees once produced the SMS_Data.c blobs via `bin2c … UMS.dmp s_UMS` / `USBD.dmp s_USBD` — the build path SMS-048 records as missing today; SMS-048 is not repeated here.)

| Tree | Files / LOC (Round 2) | Provenance (READ from headers) | Local patches | Live? (per Round 2 / SMS-037) |
|---|---|---|---|---|
| `iop/SMSTCPIP/` | 55 files, 14,016 LOC | ps2dev PS2IP + lwIP snapshot; `src/ps2ip.c` header: "Copyright 2001-2004, ps2dev", `$Id: ps2ip.c 577 2004-09-14 pixel $` | None (import + newlines only) | **Live in both configs** (PS2IP blob, 67,061 B) |
| `iop/SMSMAP/` | 8 files, 1,530 LOC | ps2dev smap clone (Makefile/Rules.make/imports.lst layout, `smap.c` 1,050 lines, `main.c` 345); no `$Id` stamp in the files checked | None (import + newlines only) | Live via blob in the legacy network mode |
| `iop/SMSUMS/` | 11 files, 2,410 LOC | ps2dev USB mass storage (`ums.c`, `ums_cache.c`, `ums_driver.c`, `ums_fat.c`) | **Makefile only** (`2d36c01`, above); C sources pristine | Blob live only in BDM=0; dead in the shipped build (SMS-037) |
| `iop/SMSPWROFF/` | 6 files, 224 LOC | ps2dev poweroff; `src/poweroff.c:10`: `$Id: poweroff.c 1377 2007-02-01 Herben $` | None (import + newlines only) | Live via blob (POWEROFF, loaded every boot — §3) |
| `iop/USBD/` | 19 files, 3,371 LOC | ps2dev USB driver; header `$Id: usbd_macro.h 1034 2005-04-23 tyranid $` | **Makefile only** (`2d36c01`, above); C sources pristine | Blob live only in BDM=0; dead in the shipped build (SMS-037) |

**Disposition:** inventory only, per the task's own split — pristine upstream is not a finding, and there are no locally patched hunks to promote. Combined with Round 2's ledger, the entire `iop/` tree is now accounted for: locally owned and line-audited (smbman-sms, SMSUdpfs local mods, SMSCDVD), dead (SMSSMB, SMS-036), or pristine-upstream inventory (these five + SMSDev9 as ABI reference).

---

## 5. New findings (SMS-050…, sorted by benefit)

### SMS-050 / ASF: a missing or zero file-properties object leaves `packet_size` at 0, and the packet reader divides by it / Category 6
- **Location(s):** Simple-Media-System/src/SMS_ContainerASF.c:192 (the **only** assignment to `lpASF->packet_size` in the file — grep-verified — from the file-properties object's own `max_pktsize` field); Simple-Media-System/src/SMS_ContainerASF.c:165-173 (the header loop breaks on the data object *wherever it appears*; nothing requires the file-properties object to have been seen first); Simple-Media-System/src/SMS_ContainerASF.c:368 (`(m_CurPos - data_offset) % apASF->packet_size` — the first arithmetic in `asf_get_packet`, before any check)
- **Problem:** Two triggers, one missing check. (a) A crafted ASF whose header contains a stream-properties object followed by the data object — a legal order for this parser's loop, which processes objects in file order until the data object — is accepted with `packet_size` still at its calloc zero. (b) A perfectly ordinary file whose file-properties object declares `max_pktsize = 0` does the same. The first packet read then computes a modulo by zero on a 64-bit operand (an EE libcall path, not a single instruction), and the resync scan that follows it works from whatever that produces. In the best case playback of the file wedges the player UI on the read call; nothing in the path recovers, because the value can never become non-zero later — it is set once, in the header, or never.
- **Fix:** At the data-object break (:165-173), fail the header if `lpASF->packet_size` is still 0 — one condition on the existing `goto fail`, covering both triggers, in the same style as this file's existing crafted-file guards (`lGSize < 24` at :175, the stream cap at :238).
- **Justification (adds a check):** concrete crafted-file freeze on a flagship format path (WMA/WMV), cited above; the check is one comparison at a point where the parser already bails on malformed headers, and it adds no state, no option, and no new code path.
- **Benefit:** The crafted-ASF freeze class is closed at the only unguarded arithmetic consumer of a header-supplied size in the ASF reader.
- **Effort:** S. **Risk:** Low (a legitimate ASF always carries a file-properties object with max_pktsize > 0 — it is required by the format; files that play today are unaffected). **Confidence:** High on the code path (READ — sole assignment site grep-verified, loop break semantics and the modulo site read in context); Med on the exact runtime manifestation (INFERRED — 64-bit modulo-by-zero behaviour on the EE runtime was not executed here; on classic MIPS the 32-bit divide does not trap, so "undefined resync / effective freeze" is the honest claim, not "crash").

### SMS-051 / Three IOP starters claim residency/ownership without checking whether the module loaded — the exact failure the file's newer starters were written to prevent / Category 6
- **Location(s):** Simple-Media-System/src/SMS_IOP.c:642-647 (`SMS_IOPStartUSB`, BDM arm: `SMS_IOPF_USB` set after the usbd load, `SMS_IOPF_UMS` set after the usbmass_bd load — neither `SifExecDecompModuleBuffer` result is read; the function's own early-return guard at :639 keys on `SMS_IOPF_UMS`); Simple-Media-System/src/SMS_IOP.c:835-845 (`SMS_IOPStartATA`: `s_AtaBusOwner = 2` at :836 and `SMS_IOPF_ATA` at :845, both after the unchecked ata_bd load at :835); Simple-Media-System/src/SMS_IOP.c:792-802 (`SMS_IOPStartILINK`: `SMS_IOPF_ILINK` at :802 after two unchecked loads); Simple-Media-System/src/SMS_IOP.c:1150-1155 (`SMS_IOPStartHDD`: `s_AtaBusOwner = 1` at :1152 assigned *before* the status-ioctl verification at :1155 that correctly gates the HDD flag itself); secondary: the non-BDM arm of `SMS_IOPStartUSB` sets `SMS_IOPF_USB` unconditionally at :676 (the BDM=0 build is unshipped per SMS-009, hence secondary)
- **Problem:** This file has a stated, modern rule — "Flag RESIDENCY, not the attempt" (SMS_IOPStartMMCE's comment), implemented by MX4SIO (load `< 0` → return 0, no flag, no bus claim) and by DS34USB ("module didn't load -> DON'T flag it, so the EE skips ds34usb_init entirely"). The three oldest starters predate the rule and violate it, and the consequences are boot-long and silent: if the usbmass_bd load fails (the realistic cause is IOP heap pressure at that moment), `SMS_IOPF_UMS` is set anyway, so every later `SMS_IOPStartUSB` call early-returns at :639 and USB mass storage can never be retried or recovered this boot — with no error shown. If the ata_bd load fails, the ATA bus is marked owned by ata_bd, so `SMS_IOPStartHDD` refuses (`s_AtaBusOwner == 2`) for the whole boot and the internal HDD disappears; the mirrored HDD case claims the bus at :1152 even when its own status check at :1155 then (correctly) refuses the HDD flag. iLink's flag has no competing claimant, so its cost is only a device-strip entry for hardware that never came up.
- **Fix:** Apply the file's own existing pattern at the three starters: capture each `SifExecDecompModuleBuffer` result and set the flag / bus owner only on success, exactly as `SMS_IOPStartMX4SIO` and `SMS_IOPStartMMCE` already do in this same file; in `SMS_IOPStartHDD`, move the `s_AtaBusOwner = 1` assignment below the status check so it stands only when the HDD actually answered. No new mechanism, no new state — the asymmetry is what gets deleted.
- **Benefit:** A transient IOP load failure stops being a silent, boot-long loss of USB mass or the internal HDD; the residency rule becomes single-pattern across all starters in the file.
- **Effort:** S. **Risk:** Low (success paths are byte-identical; only the failure branches change, from "claim and wedge" to "don't claim, stay retryable" — the behaviour the newer starters already ship). **Confidence:** High (READ — both the violating sites and the rule's statement + correct implementations are in the same file, read in full this round).

### SMS-052 / WAV-header helper: a format size of exactly 17 turns its own clamp negative — `malloc(0)` followed by a 4 GiB `Read` into it / Category 6
- **Location(s):** Simple-Media-System/src/SMS_Container.c:347-356 (`SMSContainer_GetWAVHeader`: the extradata branch runs when `aSize > 16`; `lLen` = the file's cbSize field; the clamp at :350 is `if (lLen > aSize - 18) lLen = aSize - 18;` — for `aSize == 17` this assigns **−1**; :351 then allocates `(lLen + 15) & ~15` = `malloc(0)`, and :352 reads `lLen` bytes into it); Simple-Media-System/include/SMS_FileContext.h:79 (the `Read` function pointer takes an **`unsigned int`** length, so −1 arrives as 4,294,967,295); callers with file-supplied `aSize`: Simple-Media-System/src/SMS_ContainerASF.c:265 (`type_specific_size`, a raw uint32 from the stream-properties object) and Simple-Media-System/src/SMS_ContainerAVI.c:599 (the `strf` chunk size)
- **Problem:** Round 2 cited this clamp (:349-351) as "the model" for bounded extradata handling — and for every size except one it is. A WAVEFORMAT-style header is 16 bytes fixed + 2 bytes cbSize, so the extradata branch only makes sense for `aSize >= 18`; the gate admits 17. A crafted ASF or AVI whose audio format block is exactly 17 bytes with cbSize ≥ 1 drives `lLen` to −1, allocates zero bytes, and issues a device read of effectively unbounded length into that allocation — the read delivers real file bytes (that is the Read contract), so the heap takes whatever the file contains until EOF or exhaustion. Narrow trigger (one exact size), severe primitive (heap overwrite with file content at a known offset from a 0-byte chunk).
- **Fix:** Raise the branch gate from `aSize > 16` to `aSize >= 18` — the sizes below 18 have no cbSize field to read, so nothing legitimate is lost; one comparison changed in the single shared helper, covering both container callers at once.
- **Justification (changes a bound):** this is a correction to an existing bound, not an added check or layer; it removes the one value for which the existing clamp inverts.
- **Benefit:** A crafted-file heap-corruption primitive in the shared WAV-header path (ASF + AVI audio) is closed; the "model site" Round 2 praised becomes actually sound.
- **Effort:** S. **Risk:** Low (no valid WAVEFORMATEX block is 17 bytes — the fixed part plus cbSize is 18 minimum). **Confidence:** Med — the arithmetic chain, both call sites, and the unsigned Read signature are all READ; the one INFERRED link is that a device `Read` implementation streams up to the requested count rather than failing fast on an absurd length (the STIO read loop's exact behaviour on a 4 GiB request was not traced line-by-line this round).

---

## 6. Revisions to prior findings

**None.** Nothing read this round contradicts SMS-001…SMS-049 or revisions R2-1…R2-4. Two adjacent notes, neither of which changes a prior finding: SMS-052 corrects the *praise* Round 2 gave the SMS_Container.c:349 clamp (the clamp itself was reported as bounded, which remains true for all sizes except 17 — the finding is about the gate, not a reversal of Round 2's classification); and §3.2's table extends, but does not alter, Round 2's SMS-037 guard analysis of SMS_IOP.c.

## 7. Coverage ledger (this round)

**Read in full:** SMS_ContainerASF.c (723, incl. packet/frame-header state machine), SMS_ContainerMP3.c (145), SMS_ContainerJPG.c (94), SMS_IOP.c (1,880, end-to-end), SMSContainer_SkipID3 + GetWAVHeader in SMS_Container.c, the ASF/AVI call sites of GetWAVHeader.
**Read in all relevant sections:** SMS_OGG.c (staticbook unpack :3128-3260, unpack-books :3260-3340, _book_unquantize :3786-3852, init_decode :3852-3953, allocation grep over the whole file), SMS_ContainerMPEG_PS.c (_read_header :156-296, PES buffer :300-390, packet path :489-560), SMS_MPEG.c (_mpeg_alloc_picture :29-60), SMS_AAC.c / SMS_WMA.c / SMS_MP123.c / SMS_FLAC.c (headers + complete malloc/calloc populations in context), iop/{SMSTCPIP,SMSMAP,SMSUMS,SMSPWROFF,USBD} (listings, headers, full git logs; `2d36c01` diff read in full).
**Ownership evidence:** `git log --follow` / per-tree `git log` for every codec and IOP tree cited; import commit `d704063` is the common origin.

## 8. Still unread / unfinished

- **SMS_JPEG.c decode-side marker walk** (SOF/DHT parsing, dimensions → viewer allocations): the container (§2.3) is cleared, the decoder's header logic is not line-audited — it is first-party code and the largest remaining locally-owned parsing surface after this round.
- **SMS_ContainerAVI.c logic** beyond the GetWAVHeader call site and Round 2's chunk-allocation sites (index/superindex parsing was allocation-swept in Round 2, not logic-read), and **SMS_ContainerMOV.c logic** likewise (SMS-015 covers its count allocations; the atom-walk state machine is unread).
- **Vendored codec decode paths beyond their allocation surfaces** (Vorbis residue/floor *decode* arithmetic, FAAD spectral paths, MPEG4/H263/FLAC frame parsing): deliberately not line-audited — upstream snapshots, per this round's framing; §1 records their allocation verdicts only.
- **IOP upstream snapshots' internals** (SMSTCPIP lwIP options, USBD hub logic, SMSUMS FAT cache, SMSMAP register code): inventoried for provenance/patches only (§4), by design — no line audit was attempted or is recommended against pristine upstream.
- **Whether an EE-side layer caps MPEG sequence dimensions** before `_mpeg_alloc_picture` (§1.6) — the one INFERRED link left open in Task 1; resolvable by tracing SMS_MPEG12.c's sequence-header path, not by hardware.
- **No timings, no build, no map** — as in Rounds 1-2: Category 5 magnitudes, SMS-013's heap behaviour and SMS-025's exact text size remain open for a round with a toolchain/hardware. SMS-050's runtime manifestation (freeze vs. undefined resync) likewise awaits hardware or an emulator run to state exactly.

*End of Round 3 — SMS. Findings SMS-050…SMS-052 (3 new, all locally owned code), zero revisions to prior rounds, Task 1 closed as inventory + risk notes for six vendored decoder families (one dangerous inherited surface documented: Vorbis codebook unpack), Task 2 closed for ASF/MP3/JPG logic, Task 3 closed with a full boot-order + failure-handling ledger, Task 4 closed: all five IOP snapshots are pristine upstream, zero local C patches.*
