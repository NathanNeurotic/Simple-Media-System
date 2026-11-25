# Sable Audit — Simple-Media-System, ROUND 2

- **Auditor:** Sable
- **Repo:** NathanNeurotic/Simple-Media-System
- **Pin (READ, verified in clone):** `master` @ `e65ced39a27eee8d996e9257e21594d1cac1c93d` (round-2 HEAD). Delta from round-1 pin `b9a5c882d6d6` is docs CSS only (PINS-ROUND2.md) — the code tree is unchanged, so round-1 findings stand except where this file explicitly revises one.
- **Scope of this round:** (1) exhaustive classification of every `strcpy`/`strcat`/`sprintf` call site in the EE tree; (2) enumeration/classification of malloc-with-file-supplied-size sites beyond MOV; (3) line-audit of the IOP tree with per-module ownership verdicts; (4) Category 5 structural trace of the main/GUI/player/browser paths; (5) size-estimate firming for SMS-001/013/025.
- **Mode:** READ ONLY. Clone verified `git status` clean at the end of this round. No build was run (no PS2 toolchain in this VM); every performance statement is structural and labelled as such.
- **Rule:** LESS, NOT MORE. No secret values are printed (SMS-035 already covers the SMB password topic and is not repeated).

---

## 1. Category 6 exhaustive classification — unsafe string calls, EE tree

### 1.0 Correction to round 1's counts

Round 1 (SMS-014) quoted `strcpy` 176 / `strcat` 62 / `sprintf` 87 for "src/*.c". Those numbers do **not** reproduce for the EE tree at either pin; they approximate the *whole-repo* `.c` counts (measured now: 178 / 62 / 83 including `iop/`). The verified EE-tree population, and the population classified below, is:

**Method (reproducible):** `grep -rn -E '\b(strcpy|strcat|sprintf)\s*\(' src --include='*.c'` at `e65ced39a27e` = **238 call lines** (117 `strcpy`, 51 `strcat`, 71 `sprintf`; four lines in SMS_Config.c carry two calls each, so call count is 242 — the table classifies by line). The remaining word occurrences in `src/` are four comment mentions (SMS_EE.c:765, SMS_FileDir.c:75, SMS_FileDir.c:374, SMS_GUIDevMenu.c:57), not calls.

**Classes:**
- **A — Real risk:** file / network / disc-supplied data reaches a fixed buffer with no bound found on the path. **7 lines.**
- **B — Bounded by caller/field, fragile:** the destination is safe only because a field size, a caller-side cap, or a cross-file constant currently lines up. **48 lines.**
- **C — Safe by construction:** fixed internal strings, numeric-only formatting, allocation-sized destinations (`malloc(strlen+1)` / calloc-node / VLA-sized), or an explicit length guard at the site. **183 lines.**

The headline of this round is that the EE tree is **far better bounded than round 1 implied**: most of the scary-looking calls sit on allocation-sized buffers (SMS_List, SMS_DirTree, SMS_CopyTree, SMS_FileContext all allocate `strlen+1` before copying) or behind explicit guards that round 1 did not credit. Round 1's SMS-014 is therefore **narrowed** by §1.2 below.

### 1.1 Classification table (per file; A/B/C line counts)

| File | Sites | A | B | C | Dominant pattern (READ) |
|---|---|---|---|---|---|
| SMS_Config.c | 29 | **3** | 5 | 21 | Fixed names into sized statics; two guarded boot-dir concats (193-194 guard at :191, 267-268 guard at :265); A = the three `g_SaveDiag` `%s`-path sprintfs — see SMS-040 |
| SMS_FileDir.c | 18 | **2** | 7 | 9 | Guarded `_append_directory_path` (:242-261, capacity-checked, memcpy-based); exact-malloc SMB share build (:388-390, after forced NUL at :374-378); A = CDDA strcat bypass — see SMS-044 |
| SMS_Player.c | 4 | **2** | 1 | 1 | A = SMS-041 (buffering-name sprintf :1060; path strcpy :1504) |
| SMS_GUIMenuSMS.c | 40 | 0 | 11 | 29 | Numeric `%d` sprintfs into static buffers (C); B = IP octet assembly into the 16-byte `g_pDefIP/Mask/GW` globals (:1954-1988) and unguarded boot-dir+IPCONFIG.DAT concats into 128-byte buffers (:2014-2015, safe only via the 119-char boot-dir cap in SMS_Config.c); skin/MBF names (2121, 2579) are C — the EE scan guard (SMS_EE.c:765, name < 68, 4-char extension stripped) bounds them to ≤63 chars for the 64-byte fields, verified against all six `SMS_EEScanDir` callers (only `.smi`/`.mbf`, both 4 chars, SMS_Locale.c:56-57) |
| SMS_GUIFileCtxMenu.c | 17 | 0 | 5 | 12 | Exact-malloc path builds (97-99, 379); B = `_copy_cwd_or_mc` (:230-231, safe only because its two callers pass the literals `"SMS.lng"`/`"SMS.pal"`) and `_copy_bim_handler` FS branch (:324-325). This file also documents a *previously fixed* stack smash of exactly this shape (:201, buffer enlarged 32 → 1024 with the story in the comment) |
| SMS_GUISMBrowser.c | 18 | 0 | 1 | 17 | Octet fields are `char[4]` and are rewritten via `atoi`/`sprintf("%d")` (:436), so the `lIP[16]` build at :511-512 cannot exceed 15 chars; server-name length is explicitly rejected at ≥14 before the :551 copy |
| SMS_GUIFileMenu.c | 13 | 0 | 3 | 10 | `_make_path` is capacity-checked (:860-870, memcpy); B = :936 (name into 1024-byte buffer, caller-capped), :1038 (device-supplied partition name into `m_Partition[256]`), :793 |
| SMS_GUInfoPanel.c | 15 | 0 | 3 | 12 | B = the three codec-tag sprintfs (:110, :118, :121) — the "string" is a 4-byte FOURCC inside the codec struct, read as `char*`; bounded in practice by struct layout, not by a terminator |
| SMS_IOP.c | 13 | 0 | 2 | 11 | :961 `ip=%s` into `lArg[24]` is C — `g_pDefIP` is a 16-byte global (SMS_Locale.c:33), written only via `strncpy(...,15)` (SMS_IOP.c:1425) or the UI octet path, so the result is ≤19 bytes; fragile invariant, currently sound. B = :663, :1876 |
| SMS_CopyTree.c | 9 | 0 | 0 | 9 | Every destination is `malloc`'d from the source lengths (:48, :85-91, :163-184), including the per-directory status string |
| SMS_DirTree.c | 8 | 0 | 0 | 8 | Node strings calloc'd `strlen+1` (:24); walk buffer `realloc`-grown (:202-213) |
| SMS_ContainerM3U.c | 4 | 0 | 0 | 4 | **Bounded — see revision R2-1.** `File_GetString` (SMS_FileContext.c:2176-2208) always NUL-terminates and caps at the caller's length; `lBuf`/`lTitle` are both 512 and entries are `malloc(strlen+1)` |
| SMS_FileContext.c | 4 | 0 | 0 | 4 | The `malloc(lLen+1)` + strcpy pattern (:816, :1238, :1978, :2069) — safe as a copy, unsafe as an *unchecked allocation* (SMS-045) |
| SMS_SubtitleContext.c | 6 | 0 | 0 | 6 | Both buffers are VLAs sized from the exact source lengths (:939-941) |
| SMS_JPEG.c | 9 | 0 | 1 | 8 | Caption path pre-bounds the filename via `DisplayUTF8` into a fixed buffer and truncates from the front on purpose (:1560-1566) |
| SMS_History.c | 4 | 0 | 2 | 2 | B = :152-153 — boot-dir (≤119) + `"SMS.hst"` into `lPath[128]`: fits with 2 bytes to spare, only because of the cap in another file |
| SMS_MC.c | 2 | 0 | 2 | 0 | Paths into the RPC struct's 1024-byte `m_Name` (SMS_MC.c:36); callers pass path-capped strings |
| SMS_PlayerControl.c | 8 | 0 | 1 | 7 | Time/percent sprintfs are numeric (C); B = :1445 audio-language name into `lBuf[128]` |
| SMS_GUIDevMenu.c | 3 | 0 | 1 | 2 | :292 fixed stage strings + ints (C); B = :643 server IP from config |
| SMS_GUIMiniBrowser.c | 3 | 0 | 1 | 2 | B = :76 option (directory) name appended to the CWD path |
| SMS_Artwork.c | 1 | 0 | 0 | 1 | Extension appended inside a size-checked helper; caller checks the return (SMS_GUIFileMenu.c:152) |
| SMS_ContainerAVI.c | 1 | 0 | 1 | 0 | B = :269 — language-pack string used as the sprintf *format*, numeric argument, 256-byte buffer (see SMS-046 for the format-string pattern) |
| SMS_GUI.c / SMS_GUIClock.c / SMS_GUIDesktop.c / SMS_List.c / SMS_Locale.c / SMS_PlayerMenu.c / SMS_RC.c | 9 | 0 | 1 | 8 | Fixed strings and numeric formats; SMS_List's two copies are into calloc'd `strlen+1` nodes; B = SMS_RC.c:91-92 (boot-dir + `"RMMAN.IRX"` relies on the same 119-char cap) |
| **Total** | **238** | **7** | **48** | **183** | |

### 1.2 Revisions to round 1 forced by this classification

- **R2-1 — SMS-014 is narrowed.** Its M3U example (SMS_ContainerM3U.c:351,371,390) is **not** an overflow: the input line is capped at 511 chars by `File_GetString`'s contract, both destinations are 512 bytes or exact allocations. Its DirTree example is likewise allocation-sized. What survives of SMS-014 is: the FileDir CDDA bypass (SMS-044), the GUInfoPanel FOURCC reads (class B), and the general B-class fragility — 48 sites that are safe today only because a cap in another file holds. The fix guidance in SMS-014 ("fix the ones reachable from file content") still stands, but the reachable-and-unbounded set is the 7 A-class lines, not "10–15 sites".
- **R2-2 — SMS-028 is narrowed.** `SMS_HistorySave` is called only from player teardown (SMS_Player.c:1559-1560), once per playback end — not per track event mid-playback. On the natural-end path the write already happens only when an entry was actually removed (`SMS_HistoryRemove` returns true); the user-stop path (SMS-041's neighbour, :1555-1558) always rewrites. The remaining fix is the small one: skip the save on user-stop when `SMS_HistoryAdd` changed nothing.
- **R2-3 — SMS-027 is firmed and replaced by SMS-038** (exact gating lines below).
- **R2-4 — SMS-001 is firmed by SMS-037** (exact dead bytes below). SMS-013 and SMS-025 could **not** be firmed: see §4.

### 1.3 malloc-with-file-supplied-size, beyond MOV (SMS-015)

Enumerated across the first-party containers (vendored codec internals — OGG/Vorbis, WMA, AAC/FAAD — are upstream code and are classified vendored in §3's ledger, not promoted):

| Site | Size source | Verdict |
|---|---|---|
| SMS_ContainerMOV.c:495,546,576,614,651,680 | File-supplied entry counts | Already SMS-015 (fix proposed there); additionally all six results are unchecked (SMS-045) |
| SMS_ContainerAVI.c:635 (`strn` name), :653 (`IAS*` strings) | Chunk size `lSize` read from the file (SMS_ContainerAVI.c:334), **not clamped to remaining file/atom size, result unchecked** | **Real — SMS-039** |
| SMS_Container.c:351 (WAV extradata) | `lLen` from file, **clamped** to `aSize-18` before allocation (:349) | Bounded (the pattern SMS-039's fix should copy); result unchecked → SMS-045 |
| SMS_ContainerMPEG_PS.c:372 | Caller length, explicitly rejected if `< lMinLen` or `> 65535` (:368-371, with the reasoning in the comment), result checked | Bounded + checked — the model site |
| SMS_ContainerM3U.c:309,367 | `strlen` of an already-capped line | Bounded; unchecked → SMS-045 |
| SMS_FileContext.c:1238,1978,2069 (CDDA/media paths) | `strlen` of a caller string | Bounded; unchecked → SMS-045 |
| SMS_AAC.c:3729-3743 (SBR) | Constant 64×float | Not file-sized; already SMS-016 |

---

## 2. IOP tree — per-module ownership ledger and line-audit

Pass 1 inventoried `iop/`; this round read the provenance docs, Makefiles, build scripts and the locally-owned sources. **The single most important structural fact: almost none of `iop/` is built by anything in this repo.** The shipped IRX come from `irx/*.irx.xz`; the only IOP trees with an in-repo build path are `smbman-sms` (`tools/build-smbman.sh` + `smbman.yml`) and `SMSUdpfs` (`tools/build_udpfs.sh`). The other trees are the *source of the hex-dump blobs* in `src/SMS_Data.c` (SMS-001) — kept, but with no documented rebuild path (SMS-048).

| Module | Files / LOC | Ownership verdict | Line-audit result |
|---|---|---|---|
| `iop/smbman-sms/` | 30 files, 3,851 LOC in `src/` | **Locally owned.** Vendored ps2sdk smbman @ `329e2bb` + documented SMS changes (README.md in-tree: CP1251 conversion, bounded string writes, READ_ANDX/RAP validation, path-length rejection), host regression tests incl. ASan/UBSan CI | Line-read the remaining unsafe sites: `smb_fio.c:672-675` diropen copy is length-checked first (`strlen+3 > sizeof(fh->name)` → `-ENAMETOOLONG`, :665-670); `smb_fio.c:1005` tree string into `tree_str[64]` from a 16-byte `ServerIP[16]` field (smb.h:274) — max 23 bytes; `smb_fio.c:1062` into `tree_str[274]` — 2+15+1+255+1 exactly fits by design; `smb.c:380` dialect name is a fixed protocol constant. **No new findings. This is the best-hardened module in the tree.** |
| `iop/SMSUdpfs/` (smap + ministack + udpfs) | 59 files, 5,321 LOC | **Upstream snapshot (neutrino, ps2max32/rickgaiser) with 5 documented local modifications** (ATTRIBUTION.md in-tree: dev9 v1.1 header shadow — load-bearing, `udptty_init` dropped, `_retonly` placement, `stdint.h` shim, output renames) | Zero `strcpy/strcat/sprintf` in the whole tree. The one network-data copy, `udpfs_core.c:583-586` (directory name from a server reply), checks `name_len < name_max` but not against the received packet length — **checked and cleared, not promoted**: the reply header is 48 bytes (udpfs_packet.h:164-178), `name_len` < 256, and `g_rx_buf` is 312 bytes (udpfs_core.c:35), so even a lying server cannot push the copy outside the static buffer; worst case is stale buffer bytes shown as a filename. Path sends are length-checked (`path_len > UDPFS_MAX_PATH` → `-ENAMETOOLONG`, udpfs_core.c:158-160). `ministack/src/udptty.c` (89 lines) is kept unbuilt *deliberately* for upstream re-sync (ATTRIBUTION.md item 2) — recorded, not flagged. |
| `iop/SMSSMB/` | 4 files, 3,859 LOC | **First-party legacy (Plotnikov) — DEAD.** Replaced by smbman-sms; see SMS-036 | Its own Makefile builds only `SMSMB.o`; `ps2smb.c` (1,857 lines) is a second, near-duplicate implementation of the same client (diff vs SMSMB.c ≈ 600 lines, mostly the header/include and naming) that not even that Makefile builds |
| `iop/SMSTCPIP/` | 55 files, 14,016 LOC | **Upstream snapshot** (ps2sdk SMSTCPIP / lwIP) — not yours to shrink | Inventoried + grep-swept only. Its blob (PS2IP, 67,061 B in SMS_Data.c) is live in both build configs (SMS_IOP.c:588 comment). One unsafe site noted for the record: `src/ps2ip.c:93` strcpy of the netif name — caller is internal/fixed |
| `iop/SMSCDVD/` | 6 files, 2,130 LOC | **First-party legacy (Plotnikov), live via blob** (CDVD 20,093 B, SMS_Data.h:29) | Read: ISO9660 + UDF parsers (`SMSCDVD_UDFS.c` is `#include`d at SMSCDVD.c:985 — one module, two filesystems, legitimate). One residual: SMS-049 |
| `iop/SMSDev9/` | 6 files, 958 LOC | **First-party clone, live via blob + load-bearing as an ABI reference** (the v1.1 header is what ata_bd and SMSUdpfs/smap bind against — build_ata_bd_dev9v11.sh:6) | Inventoried; no unsafe-string sites of consequence found by sweep |
| `iop/SMSMAP/` | 8 files, 1,530 LOC | **ps2sdk smap clone, live via blob** in the legacy network mode | Inventoried/grep-swept; upstream-shaped |
| `iop/SMSUMS/` | 11 files, 2,410 LOC | **ps2sdk-derived USB mass, live via blob only in BDM=0** (its blob is dead in the default build — SMS-037) | Inventoried/grep-swept |
| `iop/USBD/` | 19 files, 3,371 LOC | **Upstream snapshot** (ps2sdk usbd), live via blob only in BDM=0 | Inventoried/grep-swept |
| `iop/SMSPWROFF/` | 6 files, 224 LOC | **ps2sdk-derived, live via blob** | Inventoried; trivial |

IOP unsafe-string population for scale: 54 `strcpy` / 10 `strcat` / 12 `sprintf` occurrences across `iop/**/*.c`, concentrated in SMSSMB's two duplicate clients (dead, SMS-036) and SMSCDVD (live, §SMS-049). The live, locally-owned IOP surface after this round is: smbman-sms (clean), SMSUdpfs local mods (clean), SMSCDVD (one residual).

---

## 3. Category 5 — structural trace (no hardware; structure only, labelled)

Paths traced end-to-end by reading: boot → `SMS_GUI.c` GUI thread → desktop → file browser (`SMS_FileDirInit`) → player (`_sms_play` + the four player threads, SMS_Player.c:323,1844-1865) → teardown.

**Checked and cleared (recorded so round 3 doesn't re-chase them):**
- The GUI thread is event-driven: `_gui_thread` (SMS_GUI.c:368) sleeps (`SleepThread`) and drains a message queue; there is no busy-poll main loop. Its device-event merging hazard is already documented in a long comment at SMS_GUI.c:374-385.
- Playback decode runs on its own threads (video render/decode, audio render/decode), not on the GUI thread. The browser is not blocked by decode; it *is* blocked by its own scan/artwork work (SMS-042, SMS-043).
- Splash fades are VSync-paced frame loops (SMS_GUIDesktop.c:427-445, :545-580) — deliberate, except the fixed hold (SMS-047).

**Promoted:** SMS-038 (per-frame FFT/simulation in the audio-only renderer), SMS-042 (directory-scan probe + quadratic subtitle matching), SMS-043 (synchronous artwork decode in the redraw path), SMS-047 (1-second splash hold). Plus revision R2-2 (history save).

---

## 4. Size firming for SMS-001 / SMS-013 / SMS-025

- **No map artefact exists in the tree.** `git ls-files` contains no linker map; the Makefile *generates* one at link time (`-Xlinker -Map -Xlinker ./obj/SMS.map`, Makefile:142) and `obj/` is untracked. Deriving one requires a build, which this read-only round did not run. SMS-025's DTS text size therefore remains an estimate from source size (535,335 B file), as round 1 stated.
- **SMS-001 firmed — by constants, not by map:** SMS-037 below gives the exact dead subset of `g_DataBuffer` (37,312 padded bytes) with zero consumers in the shipped configuration. The rest of SMS-001 (modules duplicated between blob and IRX set but *live* on one path each) still needs the boot-path decision round 1 described.
- **SMS-013 cannot be firmed by a map at all:** the 2 MB + 512 KB rings are heap allocations at runtime (`SMS_SyncMalloc`, SMS_Player.c:1741-1777, allocated per stream type inside the stream loop), not static sections. A map would show nothing; only a hardware heap trace would firm it. The allocation *sites* were re-read and are as round 1 described.

---

## 5. New findings (SMS-036…, sorted by benefit)

### SMS-036 / The whole legacy SMB IOP source tree is built by nothing — and contains the same client twice / Category 1
- **Location(s):** Simple-Media-System/iop/SMSSMB/ (4 files, 3,859 lines: src/SMSMB.c 2,002 lines / 55,686 B, src/ps2smb.c 1,857 lines / 50,897 B, src/smsutils.h, Makefile); Simple-Media-System/iop/SMSSMB/Makefile (IOP_OBJS = SMSMB.o only); migration documented at Simple-Media-System/MX4SIO_SOLUTION.md:282-286
- **Problem:** SMS's SMB now ships as smbman-sms (Makefile EE IOP_OBJS embeds `smbman_irx.o`; tools/build-smbman.sh builds it from iop/smbman-sms). Nothing in the Makefile, tools, or workflows references iop/SMSSMB. Inside the dead tree, `ps2smb.c` is a second near-duplicate of the same SMB1 client that even the tree's own Makefile doesn't build (diff vs SMSMB.c ≈ 600 lines). The tree's only remaining function is to be the historical source of the dead blob in SMS-037.
- **Fix:** Delete `iop/SMSSMB/` from the repo (it remains in git history at this pin). If the blob in SMS-037 is deleted in the same pass, nothing anywhere references this tree.
- **Benefit:** 106,583 bytes and 3,859 lines of repo; one entire phantom SMB implementation — the one with 40+ unbounded string calls (see §2) — stops being mistaken for the shipped client by future editors and agents.
- **Effort:** S. **Risk:** Low (build inputs verified: main Makefile, tools/*.sh, workflows — no reference). **Confidence:** High (READ).

### SMS-037 / 37,312 bytes of `g_DataBuffer` are dead in the shipped build — exact figures that firm SMS-001 / Category 4
- **Location(s):** Simple-Media-System/include/SMS_Data.h:24 (`SMS_SMB_SIZE 19641`), :17-18 (`SMS_USB_MASS_SIZE 8504`, `SMS_USBD_SIZE 9140`), :42-43 (SMS_SMB_OFFSET/SMS_SMSUTILS_OFFSET chain); Simple-Media-System/src/SMS_Data.c:12412 (`/* SMSSMB.IRX */`), :5480 (`/* UMS.IRX */`), :6015 (`/* USBD.IRX */`); Simple-Media-System/src/SMS_IOP.c:590-596 (BDM loads `smbman_irx`; non-BDM sets `i = -1; /* SMB unavailable */` — neither path touches SMS_SMB_OFFSET), :674,688 (USBD/USB_MASS blob loads, inside the `#else` non-BDM arm delimited by SMS_IOP.c:660-693); Simple-Media-System/Makefile (`BDM ?= 1`)
- **Problem:** `SMS_SMB_OFFSET`/`SMS_SMB_SIZE` have **no consumer anywhere** — grep over `src/` + `include/` returns only the macro chain itself. The SMSSMB image (19,641 B) is dead in *every* configuration. The UMS (8,504 B) and USBD (9,140 B) images are consumed only in the BDM=0 arm, so they are also dead in the default, shipped BDM=1 build. With the header's 16-byte offset alignment the three occupy 19,648 + 8,512 + 9,152 = **37,312 bytes** of the 266,425-byte blob, in every shipped ELF and resident in EE RAM.
- **Fix:** Delete the SMSSMB block from SMS_Data.c and its SIZE/OFFSET pair (the offset chain is macro-computed, so the following offsets recompute). For UMS/USBD, wrap their blocks in the same `#ifndef BDM` condition their loader already uses, exactly mirroring SMS_IOP.c:660. This is the first, mechanical slice of SMS-001 and needs no boot-path decision.
- **Benefit:** 37,312 bytes off the ELF `.data` and off resident RAM in the shipped build (19,648 in BDM=0 builds); ~5,800 lines of hex-dump source. Exact figures, READ from the size constants — not estimates.
- **Effort:** M. **Risk:** Med (the audio ring buffer lives after this blob in the same array — SMS_Data.h's layout comments must be respected; the change is checkable by comparing every surviving module's bytes at its new offset on the host, no hardware needed for that check). **Confidence:** High (READ — consumers, guards and build default all read).

### SMS-038 / The audio-only renderer runs an FFT and a ball simulation every frame, even when both visuals are off / Category 5
- **Location(s):** Simple-Media-System/src/SMS_Player.c:393 (`_sms_dummy_video_renderer`, the audio-only display thread), :447 (`SMS_SpectrumCalc(lpSamples)` — unconditional, every loop iteration; SMS_Spectrum.c:71-88 runs DSP_FFTInit/Run/Get), :484 (`SMS_PlayerBallSim_Update(...)` — unconditional), :486 (`SMS_SpectrumUpdate()` — gated only by `!SMS_PF_SPDIF || SMS_PF_PDW22`); contrast Simple-Media-System/src/SMS_Player.c:416-422 (the ball packet joins the DMA chain only under `SMS_PF_ANIM`) and :438-444 (the spectrum packet joins only under `SMS_PF_ASD`); flags at Simple-Media-System/include/SMS_Config.h:72,77
- **Problem:** This firms round 1's SMS-027, whose open question was whether the spectrum call is visibility-gated. It is not: with ANIM and ASD both off, neither packet is ever displayed, yet the FFT over the audio samples, the spectrum packet rebuild, and the ball simulation still run on every iteration of the display loop for the entire playback. The cost is paid by exactly the users who turned the visuals off.
- **Fix:** Gate the three calls with the flags that already gate their packets: `SMS_SpectrumCalc` + `SMS_SpectrumUpdate` under the ASD condition, `SMS_PlayerBallSim_Update` under ANIM. No new state, no new option — the config flags exist.
- **Benefit:** One 1024-point FFT (assembly, SMS_DSP_FFT.S) plus two packet builds removed from every frame of audio playback when visuals are off *(structural — no timing measured, no hardware in this round)*.
- **Effort:** S. **Risk:** Low (the guarded packets are already conditional; the first frame after enabling a visual mid-play repopulates from live samples). **Confidence:** High (READ — call sites, packet gating and flag definitions all read).

### SMS-039 / AVI stream-name chunks: file-supplied size goes straight to `malloc` — unclamped and unchecked / Category 6
- **Location(s):** Simple-Media-System/src/SMS_ContainerAVI.c:635 (`malloc((lSize += lSize & 1) + 1)` for the `strn` stream name, then `Read` into it and `lpStm->m_pName[lSize] = '\0'`), :653 (same pattern for `IAS1`–`IAS9` strings); `lSize` is the raw chunk size from the file (read at SMS_ContainerAVI.c:334)
- **Problem:** A crafted AVI whose `strn` chunk claims, say, 100 MB drives a 100 MB allocation attempt on a 32 MB heap; when it fails (or for a merely large claim under memory pressure from the player buffers), the unchecked NULL is written to and read into immediately. The same file format's WAV sibling already shows the intended pattern: clamp to the containing size first (SMS_Container.c:349).
- **Fix:** Clamp `lSize` to the bytes remaining in the file (`m_pFileCtx->m_Size - m_CurPos`, both already used in this parser) before allocating, and return/skip on a NULL allocation, as SMS_ContainerMPEG_PS.c:372-373 already does. Two sites, same two lines each.
- **Justification (adds checks):** concrete, cited, file-reachable crash/corruption path; the fix copies two in-tree patterns and adds no abstraction.
- **Benefit:** Crafted-AVI crash class closed at the only two unclamped chunk allocations found outside MOV (SMS-015).
- **Effort:** S. **Risk:** Low. **Confidence:** High (READ).

### SMS-040 / The TEMP save diagnostic can itself overflow a 64-byte global on the failure path it exists to debug / Category 6
- **Location(s):** Simple-Media-System/src/SMS_Config.c:69 (`char g_SaveDiag[64]`), :943 (`sprintf(g_SaveDiag, "Save: CWD write %d %s", lWr, s_pMC0SMC)`), :945 (same shape, open failure), :1082-1087 (same shape, mc open failure); `s_pMC0SMC` is `char[128]` (:64) and `SMS_ConfigSetCWD` fills it with an ELF path capped only at `sizeof(s_pMC0SMC)-9` (:157-164) — a CWD path of ~119 chars is legitimate (deep USB folder)
- **Problem:** On a failed config save from a filesystem boot with a long ELF path, the diagnostic writes ~17 bytes of format + a number + up to ~119 bytes of path into a 64-byte `.bss` global, overwriting its neighbours (`s_CfgOnFS`, `s_CfgFallback`, `g_pBootDir` are declared immediately around it in the same file). The failure path corrupts the state the error message is about. This is round 1's SMS-011 "TEMP diagnostic" with a memory-safety reason to act, not just tidiness.
- **Fix:** The SMS-011 fix, now urgent: delete `g_SaveDiag` and its sprintfs (the generic `STR_ERROR` remains). If the staged message is kept instead, drop the `%s` path argument from the three formats — the stage and the return code are the diagnostic value; the path is already on screen as the browser's CWD.
- **Benefit:** A .bss corruption on every long-path save failure, removed; ~30 lines and 64 B if deleted outright (as SMS-011 proposed).
- **Effort:** S. **Risk:** Low. **Confidence:** High (READ — buffer size, formats, and the path-length cap all read).

### SMS-041 / Player thread: media name and path are sprintf/strcpy'd into 128- and 512-byte stack buffers / Category 6
- **Location(s):** Simple-Media-System/src/SMS_Player.c:965 (`char lBuff[128]` in `_sms_play`), :1060 (`sprintf(lBuff, STR_BUFFERING_FILE.m_pStr, s_Player.m_pCont->m_pName)` — the container name is the directory-supplied file name), :1502 (`char lPath[512]`), :1504 (`strcpy(lPath, s_Player.m_pCont->m_pFileCtx->m_pPath)` — full paths elsewhere in this codebase are built to the 1024-byte `SMS_CWD_CAPACITY`, SMS_Sounds.h:24)
- **Problem:** A file name longer than ~110 chars (legal on USB/SMB shares, and the browser happily lists 255-byte names) overflows the player thread's stack buffer at :1060 while the "Buffering" message is being formatted. At :1504 a full path longer than 511 bytes — reachable when the CWD is deep, since the browser's own path capacity is 1024 — overflows a second stack buffer during teardown/history handling.
- **Fix:** Bound both with the `snprintf` the tree already uses (4 existing sites) or truncate the name argument before the call; size `lPath` to the same 1024 the rest of the browser uses. No new helper.
- **Justification (touches working code):** file-name/path data is the classic hostile input for a media player; the destinations are the two smallest buffers on their respective data paths.
- **Benefit:** Two stack-overflow inputs removed from the playback path.
- **Effort:** S. **Risk:** Low. **Confidence:** High for :1060 (READ — buffer, format, and name source); Med for :1504 (buffer and source read; a >511-byte real-world path is INFERRED from the 1024 capacity, not observed).

### SMS-042 / Directory scans pay a per-entry device probe and a quadratic subtitle match / Category 5
- **Location(s):** Simple-Media-System/src/SMS_FileDir.c:543-570 (scan loop; entries whose driver reports no `stat.mode` are probed individually: `strcpy(lpPtr, lEntry.name)` at :553, then `fioOpen` at :556 and, on failure, `fioDopen` — per entry), :610-650 (after both sorts, every AVI node triggers up to three linear `SMS_ListFindI` scans of the whole file list at :631, :640, :644 to find its `.srt`/`.sub`/`.txt` partner); related: Simple-Media-System/src/SMS_CopyTree.c:48-55 (the size-calculation walk allocates a status string and posts a GUI status + `GS_VSync` **per directory node**)
- **Problem:** On devices that omit entry types (the comment at :548 says some do — SMB/USB stacks are the usual suspects), opening a folder costs one extra open *and* one extra directory-open per file, each a network round trip on SMB. Independently, subtitle matching is O(AVI files × all files × 3) string compares on every navigation. Both costs are paid on the GUI thread before the folder can be drawn, in the exact "UI must never freeze" path. *(Structural — magnitudes are INFERRED; no timing exists in this round.)*
- **Fix:** Two small, separate changes. (1) Probe only entries whose name has a media/directory-plausible extension or no extension at all — a `*.txt` data file does not need an open to be listed as a file when the driver already failed to type it; treat untyped-and-unprobed entries as files, which is what the probe concludes for most of them anyway. (2) The subtitle probe runs *after* `SMS_ListSort` (:610-611): walk the sorted file list once with a merge pointer instead of a fresh linear find per candidate name. No new data structure — the sort already happened.
- **Benefit:** Folder-open round trips on type-less devices drop from ~2 per entry to ~0 for non-media names; subtitle matching drops from quadratic to linear in list size.
- **Effort:** M. **Risk:** Med (an untyped true *directory* with an unusual name would list as a file until entered — the current probe's only real win; keep the probe for extension-less names, which covers directories). **Confidence:** High on the code paths (READ); Med on the user-visible magnitude (INFERRED, needs hardware timing).

### SMS-043 / Grid/page redraw synchronously reads and JPEG-decodes every missing artwork thumbnail before drawing / Category 5
- **Location(s):** Simple-Media-System/src/SMS_GUIFileMenu.c:140-190 (`_artwork_load`: opens the file, reads it whole, size-checks and decodes the JPEG), :428-429 (selection redraw loads the selected item's artwork via `SMS_ArtworkGet(..., _artwork_load, ...)`), :516-520 (page draw: a `for` loop calls `SMS_ArtworkGet` for **every** slot on the page before any display packet is built — the comment at :518-520 explains the ordering constraint: no packet may be submitted during decoding)
- **Problem:** On a cold cache, drawing one grid page costs a full read + decode of every thumbnail on it, on the GUI thread, from USB/SMB. The cache (`SMS_ArtworkPage`/`SMS_ArtworkGet`) makes *repeat* draws cheap, so the stall is exactly at navigation — the moment responsiveness matters. The decode-before-packets constraint in the comment is real, but it constrains ordering *within* a draw, not the number of decodes per draw.
- **Fix:** Cap cold loads per draw (e.g. the selected item plus the first row), draw with placeholders for the rest, and let the next redraw — the machinery already redraws on cache drain (`_artwork_drain`, :427) — fill in the remainder. No new thread, no new cache.
- **Benefit:** Page navigation stall becomes one row of decodes instead of a full page *(structural; magnitude INFERRED — a network-sourced thumbnail page is the worst case)*.
- **Effort:** M. **Risk:** Med (placeholder state must not fight the existing drain/redraw logic). **Confidence:** High on the structure (READ); Med on magnitude (INFERRED).

### SMS-044 / One CDDA path append bypasses the capacity-checked helper every other append uses / Category 6
- **Location(s):** Simple-Media-System/src/SMS_FileDir.c:465-466 (`strcpy(g_CWD, g_pCDDAFS); strcat(g_CWD, apPath);`) vs Simple-Media-System/src/SMS_FileDir.c:242-261 (`_append_directory_path`: explicit `SMS_CWD_CAPACITY` checks, memcpy-based, used by the normal navigation path); `g_CWD` capacity is 1024 (SMS_Sounds.h:24-25 — the CWD lives inside the sound-data array at offset 512)
- **Problem:** The CDDA branch appends a disc-supplied directory name to the CWD with no length check at all, in the same function whose other path was carefully rewritten to check. CD audio directory names are short in practice, which is why this is Low rather than a headline — but it is the one unguarded append left in the browser's core function.
- **Fix:** Route the CDDA append through `_append_directory_path` like every other navigation, deleting the two unbounded lines.
- **Benefit:** The last unguarded CWD append in SMS_FileDir.c is gone; the capacity invariant becomes single-site.
- **Effort:** S. **Risk:** Low. **Confidence:** High on the code (READ); Low on exploitability (CDDA name lengths are INFERRED short).

### SMS-045 / Small allocation results are unchecked across the container/open paths — NULL is then written through / Category 6
- **Location(s):** Simple-Media-System/src/SMS_ContainerM3U.c:308,366-367 (`lpEntry` and `m_pPath` dereferenced/strcpy'd immediately), Simple-Media-System/src/SMS_FileContext.c:1238,1978,2069 (`retVal->m_pPath = malloc(...)` then `strcpy` at the same site), Simple-Media-System/src/SMS_Container.c:351 (extradata `malloc` then `Read` into it), and the six SMS-015 sites at Simple-Media-System/src/SMS_ContainerMOV.c:495,546,576,614,651,680
- **Problem:** These are the mirror of SMS-039: the *sizes* are bounded (see §1.3), but a failed allocation — most plausible exactly when the 2.5 MB player buffers of SMS-013 are held — is a NULL dereference / NULL write instead of a clean "cannot open". The codebase already has the correct idiom in the same files (e.g. SMS_ContainerMPEG_PS.c:372-373 checks; SMS_ListPushBack checks at SMS_List.c:41).
- **Fix:** Check each result and fail the open with the file's existing error return. Site-by-site one-line guards, no helper, no new error machinery.
- **Justification (adds checks):** cited NULL-deref-on-OOM in the media-open path on a 32 MB machine; the guard pattern already exists in-tree.
- **Benefit:** OOM during media open degrades to an error dialog instead of a crash, at ~10 sites.
- **Effort:** S. **Risk:** Low. **Confidence:** High (READ).

### SMS-046 / Language-pack strings are used as `sprintf` format strings throughout — the pack test is the only gate, and it doesn't check `%` sequences / Category 6
- **Location(s):** Pattern sites (READ): Simple-Media-System/src/SMS_Player.c:1003,1060; Simple-Media-System/src/SMS_ContainerAVI.c:269; Simple-Media-System/src/SMS_CopyTree.c:52; Simple-Media-System/src/SMS_IOP.c:210 — all pass a `STR_*.m_pStr` (loaded from a `.lng` pack file) as the format argument; pack loading at Simple-Media-System/src/SMS_Locale.c (SMS_LocaleSet) and round 1's SMS-022 (exact-count packs); existing gate: Simple-Media-System/tools/test_language_packs.py
- **Problem:** A language pack is a downloadable, user-editable text file. Any `%n`, stray extra `%s`, or missing argument in a translation turns every one of these call sites into a crash or memory write the day that string is displayed. Verified for the record: **no `%n` exists in any current pack** (grep over `docs/translation/` and `lng/`), and the English pack uses 13 `%` sequences — so this is a loaded-but-unfired gun, not a live bug.
- **Fix:** Extend the *existing* host test (`test_language_packs.py`, which already parses every pack) to require each pack string's `%` conversion sequence to match the English template's, position by position. Test-only change; zero runtime code, zero new tooling.
- **Justification (adds a check):** concrete crash vector via a user-suppliable file, and the smallest possible gate — one assertion family inside a test that already runs in CI.
- **Benefit:** The entire class is closed at pack-commit time for all present and future sprintf-with-pack-format sites.
- **Effort:** S. **Risk:** Low. **Confidence:** High (READ for the pattern and the current packs' cleanliness).

### SMS-047 / Boot splash holds a static frame for a full second, between two 17-frame fades / Category 5
- **Location(s):** Simple-Media-System/src/SMS_GUIDesktop.c:545-580 (fade-in loop, `lA` 0x80→0 in steps of 8 = 17 VSync-paced frames), :558 (`SMS_TimerWait(1000); /* hold at full brightness */`), followed by the symmetric 17-frame fade-out; a second, shorter fade path at :427-445
- **Problem:** Startup cost that buys nothing functional: ~34 vsync frames of fades plus a hard 1,000 ms hold on a static splash, on every boot, before the browser is usable. At 60 Hz the fades alone are ~0.57 s; the hold is a full second of deliberate wait.
- **Fix:** Delete the `SMS_TimerWait(1000)` hold (the fade in/out already reads as a transition). Keep the fades.
- **Benefit:** ~1 s off every cold boot *(the hold is a literal 1,000 ms constant, READ; frame-time arithmetic assumes 50/60 Hz VSync, INFERRED)*.
- **Effort:** S. **Risk:** Low. **Confidence:** High (READ).

### SMS-048 / The still-live legacy IOP blob sources have no build path, no workflow, and no provenance note / Category 7
- **Location(s):** Simple-Media-System/iop/SMSCDVD/, iop/SMSDev9/, iop/SMSMAP/, iop/SMSPWROFF/, iop/SMSUMS/, iop/USBD/, iop/SMSTCPIP/ (2,130 + 958 + 1,530 + 224 + 2,410 + 3,371 + 14,016 LOC); contrast Simple-Media-System/tools/build-smbman.sh, tools/build_udpfs.sh, tools/build_ata_bd_dev9v11.sh, tools/build_libsd.sh (which build from *ps2sdk checkouts*, not from these trees) and Simple-Media-System/iop/smbman-sms/README.md + iop/SMSUdpfs/ATTRIBUTION.md (the two trees that do document provenance)
- **Problem:** These trees are the only source for blobs that still load on every boot (CDVD, DEV9, SMAP, PS2IP, POWROFF — §2), yet nothing in `tools/`, the Makefile, or any workflow builds them, and none carries a README saying which blob it produced, with what toolchain, or whether the blob in SMS_Data.c still matches it. This is round 1's SMS-033 problem (source-less VU objects) at module scale: if a blob module ever needs a one-line fix, the first task is archaeology. (This is also why SMS-036's tree stood out — it *looked* identical in status to these live ones.)
- **Fix:** Documentation only, and less of it than it sounds: one `README` line per tree — blob name in SMS_Data.c, historical toolchain, "matches blob as of <commit>" or "provenance unknown". Where even that can't be stated honestly, say "provenance unknown" — that is itself the useful information. Do not create build scripts for modules nobody is changing.
- **Benefit:** No bytes. The live/dead line in `iop/` becomes discoverable in minutes instead of a research project — the exact confusion this round had to spend its first hour on.
- **Effort:** S. **Risk:** Low. **Confidence:** High (READ — build scripts and workflows enumerated).

### SMS-049 / IOP CDVD directory cache appends disc-supplied names to a 1024-byte pathname with no length check / Category 6
- **Location(s):** Simple-Media-System/iop/SMSCDVD/SMSCDVD.c:168 (`char m_Pathname[1024]` in the cache struct), :772-773 (`strcat(CachedDirInfo.m_Pathname, "/"); strcat(CachedDirInfo.m_Pathname, lDirName);` as the cache descends), :657,685 (`strcpy(s_DirName, apPath...)` into the 1024-byte `s_DirName` at :195)
- **Problem:** The cached pathname is built incrementally as directories are entered, one component per call, and nothing checks the accumulated length. Real discs won't reach 1024 bytes of path; a crafted ISO/UDF image with deep nesting of long names (each component can be up to 255 bytes from the directory record) can — the cache is persistent IOP memory, and the overflow lands in the CDVD driver's own state. This is the one residual in the live first-party IOP code; everything around it (SMSCDVD_UDFS.c included at SMSCDVD.c:985) was swept.
- **Fix:** One bound in the descend step: if `strlen(m_Pathname) + strlen(lDirName) + 2` would exceed the buffer, fail the cache fill (the caller already handles a failed `FindPath`/cache as "not found"). No new mechanism.
- **Justification (adds a check):** disc images are untrusted input to an IOP driver; a single arithmetic check at the only append site is the smallest fix.
- **Benefit:** Crafted-disc IOP memory corruption closed in the CD/DVD path.
- **Effort:** S. **Risk:** Low. **Confidence:** Med (mechanism READ; reachability via a real crafted image is INFERRED — no disc was built in this round).

---

## 6. Coverage ledger (this round)

**Read in full or in all relevant sections:** SMS_Config.c (all unsafe sites + CWD caps), SMS_FileDir.c (scan, SMB share build, CDDA branch, subtitle probe), SMS_ContainerM3U.c, SMS_ContainerAVI.c (chunk handlers), SMS_Container.c (WAV header), SMS_ContainerMPEG_PS.c (PES buffer), SMS_FileContext.c (GetString, _read_string, path sites), SMS_List.c, SMS_DirTree.c, SMS_CopyTree.c, SMS_SubtitleContext.c (font path), SMS_Player.c (renderer loop, buffering/history sites, ring allocations), SMS_Spectrum.c, SMS_History.c (save paths), SMS_GUIMenuSMS.c / SMS_GUISMBrowser.c / SMS_GUIFileCtxMenu.c / SMS_GUIFileMenu.c / SMS_GUInfoPanel.c / SMS_IOP.c (all classified sites in context), SMS_EE.c (scan guard), SMS_Locale.c (globals), SMS_MC.c (RPC struct), SMS_GUIDesktop.c (splash + redraw), SMS_GUI.c (GUI thread), iop/smbman-sms (README + all unsafe sites in context), iop/SMSUdpfs (ATTRIBUTION + udpfs_core dread/path paths + packet header), iop/SMSCDVD (cache path logic), iop/SMSSMB (Makefile + diff sample), Makefile (map rule, IOP_OBJS, BDM default).

**Classified by dominant pattern after sampling the file's sites in context:** the numeric-sprintf majorities in SMS_GUIMenuSMS.c, SMS_PlayerControl.c, SMS_GUInfoPanel.c (the B/C boundary in the table follows the source of the *string argument*, as stated in §1.1's notes).

**Checked and cleared (not promoted):** UDPFS dread name copy (bounded by buffer geometry, §2); smbman-sms remaining string sites (bounded, §2); M3U copies (File_GetString contract, R2-1); EE skin-scan guard (sound for both current extensions, §1.1); IPCONFIG loader (`strncpy(...,15)` into 16-byte globals, SMS_IOP.c:1425); GUI thread (event-driven, §3); `udptty.c` kept unbuilt (documented upstream-sync decision, §2).

## 7. Still unread / unfinished

- **Vendored codec internals** (SMS_OGG/Vorbis, SMS_WMA, SMS_AAC/FAAD, SMS_DTS, MPEG family, FLAC): allocation and string sites inside them were **not** individually classified — they are upstream ports, and §1's table covers the first-party EE tree only. A hostile-media audit of the Vorbis codebook path (`SMS_OGG.c:3170-3220`, entries-driven allocations) remains the largest unexamined Cat 6 surface.
- **IOP upstream snapshots line-by-line**: SMSTCPIP (lwIP, 14,016 LOC), USBD, SMSMAP, SMSUMS were inventoried/grep-swept, not line-audited — per the task's own split, only locally-owned code was promoted, and their findings would not survive a re-sync.
- **SMS_IOP.c boot sequencing** beyond the classified sites and the SMS-037 guards (e.g. the full netmode decision tree) was not re-traced this round.
- **No timings, no build, no map** — Category 5 magnitudes and SMS-025's exact text size remain open for a round with a toolchain/hardware. SMS-013's runtime heap behaviour likewise.
- `SMS_ContainerASF.c` / `SMS_ContainerMP3.c` / `SMS_ContainerJPG.c` container parsers: allocation sweep found no `malloc` sites beyond includes (grep, READ for that fact), but their parsing logic was not line-audited this round.

*End of Round 2 — SMS. Findings SMS-036…SMS-049 (14 new), revisions R2-1…R2-4 to round-1 findings, one full Cat 6 classification (238 sites: 7 A / 48 B / 183 C), one full IOP ownership ledger (10 modules).*
