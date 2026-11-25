# Verification — SMS RE-SMS-097 … RE-SMS-120 (W5)

Verifier: adversarial, refute-first. Pin: `e65ced39a27eee8d996e9257e21594d1cac1c93d` in
`~/workspace/audit-clones-2026-11-13/Simple-Media-System` (HEAD re-checked, clean tree).
Every "dead / never called" claim was tested with a whole-tree word-boundary grep
(all file types, Makefiles, tools, workflows, docs) before acceptance. All byte/line
counts below were recomputed by the verifier, not copied.

---

## RE-SMS-097 — VERIFIED

- `src/SMS_GUIcons.c:34`: `extern int lzma2_uncompress ( unsigned char*, unsigned int, unsigned char*, unsigned int );` — exact.
- Real prototype `include/lzma2.h:34`: `enum xz_ret lzma2_uncompress(unsigned char *in, size_t size_in, unsigned char *out, size_t size_out);` — return and parameter types both differ as filed.
- Call `src/SMS_GUIcons.c:1406` discards the result, then slices `lpBlob` unconditionally (lines 1408+).
- Other consumers do include the header: `src/SMS_EE.c:28`, `src/SMS_GS_2.c:24`, `src/SMS_IOP.c:44` (`#include <lzma2.h>`), and `SMS_IOP.c:240` even checks `ret`. No defeating evidence.

## RE-SMS-098 — VERIFIED-DRIFT

Core verified, one phrase corrected:

- `wc -l`: `include/libds34bt.h` = 35, `include/libds34usb.h` = 21 — exact. `ls include/*.h | wc -l` = 89; `grep -L "ifndef|pragma once" include/*.h` returns exactly these two files.
- Both are included from two TUs each: `src/SMS_PAD.c:26-27`, `src/SMS_GUIMenuSMS.c:36-37` (plus their own `.c` files). Enum definitions (`eDS34BTStatus`/`eDS34USBStatus`) would hard-error on double inclusion in one TU, as filed.
- **Correction:** "all prototypes use K&R empty parentheses" is overstated. Only the **7 zero-argument** prototypes use `()`: bt — `ds34bt_init()`, `ds34bt_deinit()`, `ds34bt_init_charging()`, `ds34bt_reset()` (4); usb — `ds34usb_init()`, `ds34usb_deinit()`, `ds34usb_reset()` (3). The other 16 prototypes in the two headers take real arguments. No other header in `include/` uses a bare `()` zero-arg prototype (grep for `();` outside the ds34 headers returns only a macro false positive in `SMS_IPU.h:56`); the tree convention is `(void)` / `( void )`. The finding's own fix is already scoped to the zero-argument prototypes, so the fix stands unchanged.

## RE-SMS-099 — VERIFIED

Universality checked reader-by-reader in `include/SMS_Bitio.h` (148 lines, read in full):

- `m_szInBits` occurs exactly twice in the header: the field declaration (:52) and the setter in `SMS_InitGetBits` (:145). No function compares `m_Idx` against it.
- Readers enumerated: `SMS_GetBits` (:70), `SMS_GetSBits` (:80), `SMS_GetBit` (:90, direct `m_pBuf[lIdx>>3]` index), `SMS_ShowBits` (:119), `SMS_SkipBits` (:103), **`SMS_GetXBits` (:127)** — the finding does not name `GetXBits`, `SMS_SkipBit`, or `SMS_AlignBits`, but all three also lack any check (the latter two delegate to `SMS_SkipBits`), so the "any reader" wording survives and is in fact under-scoped, not over-scoped.
- `SMS_UPDATE_CACHE` reads 4 unaligned bytes at `m_Idx>>3` with no bound either.
- Call-site checks exist in a few consumers (`src/SMS_MPEG4.c:1396,1469,2758`, `src/SMS_AAC.c:7882`, `src/SMS_FLAC.c:508`) — these are outside the header and do not refute the claim as filed (the reader itself never checks).

## RE-SMS-100 — WEAKENED

True scope is narrower than filed; one of the two sinks is unreachable even without the container gate:

- Table verified: `src/SMS_FLAC.c:437-444` — `_synth_indep[2] = {_pcm_synth_1, _pcm_synth_2}`, indexed at :441 by `m_nChannels - 1`. No in-decoder bound exists; `_flac_init` (`src/SMS_FLAC.c:385`) copies `FLACData` verbatim from `m_pUserData`.
- Gate verified: the only in-tree path that creates that `FLACData` is `src/SMS_ContainerFLAC.c:114` (`lData.m_Info.m_nChannels < 3`), which then stores it at :138-141. `_decode_frame` (`src/SMS_FLAC.c:322-326`) additionally rejects any frame whose channel-assignment field does not match `m_nChannels`. So with the shipped container the synth index can only be 0 or 1 — this is a defence-in-depth note, not a live bug (the finding itself labels it INFERRED / "currently saved only by the container's gate").
- **The shift-divide sink cannot trigger at all:** `m_nChannels` is parsed at `src/SMS_ContainerFLAC.c:66` as `SMS_GetBits(&lCtx, 3) + 1` — maximum 8 by construction of the STREAMINFO field. `lBlockLen = 4096 >> nChannels` (`src/SMS_FLAC.c:523`) is therefore ≥ 16, never 0; the claimed divide-by-zero at `nChannels ≥ 13` is unreachable from any FLAC stream, gated or not. Only the synth-table sink survives, and only as a latent hazard if a future caller feeds the decoder a 3–8-channel `FLACData` directly.

## RE-SMS-101 — VERIFIED

- `src/SMS_Container.c:198`: `} else s_CCreator[ aPrefCont ] ( retVal );` — no range test. The table `s_CCreator` (`src/SMS_Container.c:45-58`) has 12 creators + NULL terminator; the auto-probe path does bounds-check its hint index (:152-155), as the fix notes.
- Callers enumerated by grep over `src/`: `src/SMS_ContainerM3U.c:186` and `:485`, both passing `m_ContID[i]`, whose 7 entries are initialised at `src/SMS_ContainerM3U.c:469-475` to the `SMS_CONTAINER_*` constants (all valid table indices); `src/SMS_Player.c:1688` passes `0x80000000`, negative as `int`, taking the auto-probe path. No live trigger — latent trap exactly as filed.

## RE-SMS-102 — VERIFIED

- Line counts recomputed: `SMSMB.c` 2,002 + `ps2smb.c` 1,857 + `smsutils.h` 35 = **3,894** (Makefile excluded; 3,926 with it) — exact.
- `iop/SMSSMB/Makefile`: `IOP_OBJS = SMSMB.o`, `IOP_BIN = bin/smb.irx` — `ps2smb.c` is not built. `diff SMSMB.c ps2smb.c | wc -l` = **600** — exact.
- No build path touches the tree: no reference to `SMSSMB` in the root `Makefile`, `tools/*.sh`, or `.github/workflows/*.yml` (grep clean). `tools/build-smbman.sh` builds `iop/smbman-sms`.
- EE liveness: `src/SMS_IOP.c:586-597` loads `smbman_irx` via `SifExecDecompModuleBuffer` (comment states smbman binds to the SMSTCPIP stack); grep for `SMS_SMB_OFFSET` over the whole tree returns only `include/SMS_Data.h:42-43` (its definition and the next offset in the chain) — no EE code loads the packed slot.
- Internal-bug spot checks all landed: FindFirst casts at `SMSMB.c:1127/1131` (`&lPkt[SVAL(lPkt,45)+4]`, `&lPkt[SVAL(lPkt,51)+8]`, no length check); `SMB_Negotiate` `strncpy(apCtx->m_PDom, &lBuff[65 + m_EncrKeyLen], …)` at :813-815; LOGOUT inner loop uses `&lpMountCtx->m_FileCtx[ i ]` inside the `j` loop (shown at ~:1774) — closes the same context repeatedly, as filed.
- Deletion reduces and breaks nothing in-tree. (The packed SMB *bytes* inside `src/SMS_Data.c` are a separate residue — see Candidate C1.)

## RE-SMS-103 — VERIFIED

- Recomputed: `wc -l include/s_iop_image.h` = **3,820**; `wc -c` = **373,784 B** (~373 KB) — exact. `git ls-files` confirms it is tracked, and no build rule generates it (grep for `s_iop_image` over the whole tree returns only the file itself and the two comments below — it is not a build-time generated header).
- Only references: `src/SMS_IOP.c:228` (`//#include "s_iop_image.h"`) and `src/SMS_IOP.c:437` (`//while(!SifIopReset(s_iop_image, 0)){};`) — both commented out, exactly as filed. Deletion is build-safe.

## RE-SMS-104 — VERIFIED

Sizes recomputed by content bytes (Python walk, not `du` blocks):

- `docs/downloads/`: **10,283,188 B** (9.81 MiB) — `SMS-manual.pdf` 9,321,329 B, `Rad-HOST-Client-1.8a.zip` 891,537 B, `SMS_lng09.rar` 39,356 B, `pic2smi.rar` 16,768 B, `smspal.rar` 14,198 B — every per-file figure in the finding is exact; "9.9 MB" holds.
- `docs/investigations/`: **1,920,177 B** (1.83 MiB; `du -sh` reports 2.0M in blocks). Handoff dir `2026-11-11-handoff/` = 1,628,644 B; its `samples/` holds exactly **19** files (`.mp3/.mp2/.wma/.wmv/.asf/.aac/.bin`) totalling 901,501 B. The finding's "1.7 MB for the handoff dir alone" is ~4% high against content bytes (1.63 MB) but within its own ≈ framing.
- Total: **12,203,365 B** = 11.64 MiB / 12.20 MB — the claimed "≈ 11.9 MB" sits between the two unit conventions and holds.
- Premise: `docs/README.md` states Pages publishes `master:/docs`; `docs-site/build.py` generates only top-level HTML/assets/wiki mirror — it never emits `downloads/` or `investigations/`. Caveat that does **not** refute: the downloads are deliberately linked from the generated releases page (`docs-site/content/releases.html:109-130` → `docs/releases.html:132-153`), i.e. they are served site content by design, not stray files — which is exactly why the finding's fix (move + update links, Medium risk for external deep links) is the right shape.

## RE-SMS-105 — VERIFIED

- `src/SMS_AC3.c:323`: `static sample_t s_Samples[16 * 256 * 12]` = 49,152 floats = 196,608 B = 192 KiB BSS — exact.
- Whole-file grep returns only :323 (decl), :360, :2305, :2612 plus the use block :2401-2480. Highest index re-derived independently: `lpSamples = s_Samples + 256` (LFE, :2305-2307); IMDCT calls write `apData[0..255]` and `apDelay[0..127]` (`src/SMS_AC3_imdct.c:533-642`) at `lpSamples + 1536 + 256*i`, `i < lnFChans ≤ 5` → max index 256 + 1536 + 1024 + 255 = **3,071** (≈3,072 floats ≈ 12 KB as filed). `_ac3_float_to_int_2` asm reads up to ~2,048 floats from the base — inside the bound. Proposed 4,096-float (16 KB) shrink keeps ~33% headroom.
- Minor omission in the finding, not a refutation: it does not list the `memset(s_Samples, 0, sizeof(s_Samples))` at :360 — that use is size-agnostic and shrinks with the declaration.

## RE-SMS-106 — VERIFIED

Arithmetic recomputed from `include/SMS_Data.h`:

- UMS.IRX 8,504 (+8 pad) = 8,512; USBD.IRX 9,140 (+12) = 9,152; SIO2MAN.IRX 5,817 (+7) = 5,824; aligned total = **23,488 B** — exact. Full offset chain recomputed gives `SMS_DATA_BUFFER_SIZE` = **266,560** — exact (23,488 = 8.8%).
- Consumers: grep for the three OFFSET macros outside `SMS_Data.h` returns only `src/SMS_IOP.c:504` (SIO2MAN, in the `#else`/non-BDM branch of `SMS_IOPReset`), `:674` (USBD) and `:688` (USB_MASS), both in the `#else` branch of `SMS_IOPStartUSB`. Default build is BDM: `Makefile:3` `BDM ?= 1`, `IOP_OBJS` at :49-57 embeds the `irx/*.irx.xz` blobs, and the BDM branches load `sio2man_irx`/`usbd_irx`/`usbmass_bd_irx` instead. The fix preserves the BDM=0 flavour rather than deleting it — LESS-compliant.

## RE-SMS-107 — VERIFIED

Whole-tree word-boundary census (all types incl. tools, workflows, `.S`/asm) for each of the ten names returns only the header declaration, the definition (plus its end-comment), and mentions in `docs/investigations/` handoff notes — no code caller, no function-pointer table entry, no host-test extraction:

- `IPU_UnpackImage` — `include/SMS_IPU.h:140` / `src/SMS_IPU_1.c:220-324` = **105 lines** exactly.
- `CDDA_Pause` (:220-230), `CDDA_SetMediaMode` (:251-270), `CDDA_GetPicture` (`SMS_FileContext.c:887-905`), `CDDA_GetDiskPicture` (:907-918), `CDVD_QueryDVDV` (`SMS_CDVD.c:114-122`), `PlayerControl_SwitchSubs` (:876-884), `RC_Shutdown` (:454-467), `RCX_Shutdown` (:484-497) — all decl+def only. Summed spans for all ten = 217 lines vs the claimed "≈200" — holds.
- Adversarial check on the tenth: `ReadCombinedPadStatus_raw` (`include/SMS_PAD.h:20` / `src/SMS_PAD.c:118`) is **not** the live pad path — the `PAD_Read` macro (`SMS_PAD.h:38`) maps to the *singular* `ReadPadStatus_raw`, which is live via `src/SMS_GUI.c:211-231`. The combined wrapper itself has no caller. Deleting it does not touch the singular function. No token-pasting construction of any of the ten names exists in the tree.

## RE-SMS-108 — VERIFIED-DRIFT

- The 13-name list is exact: bt (8) `ds34bt_deinit`, `ds34bt_reinit_ports`, `ds34bt_init_charging`, `ds34bt_set_rumble`, `ds34bt_set_led`, `ds34bt_reset`, `ds34bt_get_version`, `ds34bt_get_features`; usb (5) `ds34usb_deinit`, `ds34usb_reinit_ports`, `ds34usb_set_rumble`, `ds34usb_set_led`, `ds34usb_reset`. Each returns exactly 2 word-boundary hits in `src/ include/ tools/` — its declaration and its definition.
- Live set verified from call sites: `src/SMS_PAD.c` (`ds34bt_init`, `ds34usb_init`, both `*_get_status`, both `*_get_data`) and `src/SMS_GUIMenuSMS.c:1659-1661` (`ds34bt_get_bdaddr`, `ds34usb_get_bdaddr`, `ds34usb_set_bdaddr`).
- **Correction:** the fix's "keeping the five used entry points" undercounts. There are **9 concrete live functions** (bt: init/get_status/get_data/get_bdaddr = 4; usb: those four + set_bdaddr = 5). "Five" is only defensible as call-name categories. Deleting exactly the 13 listed stubs is safe; an implementer who literally kept only five functions would break `SMS_PAD.c` / `SMS_GUIMenuSMS.c`.

## RE-SMS-109 — VERIFIED-DRIFT

- Absence verified: grep for `DDrawSrv`, `utils/Loader`, `Loader` over docs, docs-site, README, Makefile, tools, workflows (excluding `utils/` itself) returns nothing. All 4 files are git-tracked.
- **Size correction:** content bytes are 45,056 (`DDrawSrv.exe`) + 746 + 580 + 952 (Loader sources) = **47,334 B ≈ 46 KiB**, not the claimed 56 KB. The deletion case is unaffected.

## RE-SMS-110 — VERIFIED-DRIFT

- Operative claim verified: grep for `bdmfs_vfat`/`vfat` in `src/`, `include/`, `tools/`, `.github/`, `docs-site/content/` returns **zero** matches; root `Makefile` `IOP_OBJS` embeds `bdmfs_fatfs_irx.o`, not vfat; no `tools/build_*.sh` / `compress_irx.py` step names it. Deletion is build-safe. (The only mentions anywhere are in `docs/investigations/` handoff notes, which themselves call it unused — outside the finding's claimed grep scope.)
- **Correction:** "the only `.irx.xz` in `irx/` with no plain `.irx` counterpart" is false as stated — `ds34bt.irx.xz`, `ds34usb.irx.xz`, `libsd.irx.xz` and `smbman.irx.xz` also have no plain `.irx` counterpart. The true distinguishing scope: `bdmfs_vfat.irx.xz` is the only `.xz` blob that is simultaneously counterpart-less, **not in `IOP_OBJS`**, and unreferenced by any tool.

## RE-SMS-111 — VERIFIED

- Sizes exact: `images/jellyfish.jpg.bak-512` 25,390 B; `images/main_bg.png` 230,952 B; `theme/splash_composited.png` 232,705 B; the two PNGs sum to 463,657 B as filed.
- Build embeds exactly three JPEGs: `EE_OBJS` (`Makefile:45-46`) lists `jellyfish_jpg.o`, `splash_jpg.o`, `main_bg_mini_jpg.o` via the `%_jpg.c: %.jpg` bin2c rule with `vpath %.jpg images/`; consumers at `src/SMS_GUIDesktop.c:40-45`.
- Repo-wide grep for all three filenames returns zero references (Makefile, `src/`, `tools/`, docs). The "master artwork" characterisation of the PNGs is labelled INFERRED in the finding and stays that way.

## RE-SMS-112 — VERIFIED

- `md5sum` recomputed: `fonts/Turkish by nethunter/latin1.mtf` and `WinLatin1.mtf` both = `621f497d0a5d0c5c62df649eda78559b`, both 8,328 B — byte-identical, exact. The pack's `ascii.mtf` (6,224 B, md5 `238af38f…`) is distinct, and `fonts/Hebrew/latin1.mtf` exists, supporting the sibling-naming argument for keeping `latin1.mtf`.

## RE-SMS-113 — VERIFIED

Every sub-item re-derived:

- `iop/SMSTCPIP/src/loopif.c` = 121 lines, absent from `iop/SMSTCPIP/Makefile` `IOP_OBJS`; body under `#if LWIP_HAVE_LOOPIF` with `opt.h:303-304` defining it 0; its `AddLoopIF` caller and the `netif_add(…loopif_init…)` in `ps2ip.c` (:68, :270-284, :300) are under the same `#if`.
- `udp_lookup` (`udp.c:84-141`, under `#ifdef LWIP_DEBUG`): both exits return 1 — the found path and the `pcb == NULL` not-found path (:136-139) — and the header-pointer line `(struct udp_hdr *)(u8_t *)iphdr + IPH_HL(iphdr) * 4` does struct-pointer arithmetic, wrong besides, as filed. Its only caller is `ip_lookup` (`ip.c:70`, also `#ifdef LWIP_DEBUG`), which itself has no caller (only the declaration at `include/lwip/ip.h:46`).
- `ps2ip.c`: `#define QUEUE_INPUT 0` (:40); `InputCB` + the queue branch of `ps2ip_input` sit under `#if QUEUE_INPUT` (:194-256).
- `ip_forward()` (`ip.c:129-189`) and `icmp_time_exceeded()` (`icmp.c:161-205`) sit under `#if IP_FORWARD`, defined 0 at `lwipopts.h:117`. Summed dead span ≈430 lines incl. `AddLoopIF` vs the claimed ~450 — holds.

## RE-SMS-114 — WEAKENED

Premise verified, prescribed off-switch does not work:

- Premise: `SO_REUSE 1` at `iop/SMSTCPIP/include/lwip/opt.h:338-340`; reuse machinery present as filed — `tcp_in.c` reuse loop with `p->ref++`/`goto again_1` (:192-216, :349-382), `udp.c` `udp_input` reuse loops (:228-299), reuse branches in `tcp_bind` (`tcp.c:264-358`) and `udp_bind` (`udp.c:531-603`). No in-tree code sets `SO_REUSEADDR`/`SO_REUSEPORT` via setsockopt and nothing deliberately double-binds a port (grep over `src/` + SMSTCPIP `sockets.c` shows only the option-storage cases the finding explicitly excludes).
- **Defeating evidence for the fix:** every single guard is `#ifdef SO_REUSE` / `#ifndef SO_REUSE` — enumerated in `sockets.c` (:911, :999, :1107, :1191), `tcp.c` (:235, :242), `tcp_in.c` (:105, :174, :192, :349, :361, :380), `udp.c` (:161, :198, :228, :257, :279, :345, :365, :531, :549, :603). Setting `SO_REUSE` to **0** still leaves the macro *defined*, so all `#ifdef` branches — including the per-packet reuse loops — remain compiled, and the plain `#ifndef SO_REUSE` path in `tcp_bind` stays unreachable. (`lwipopts.h` is included by `opt.h:36` before the default, so a `0` there would indeed win the value — and change nothing.) To actually disable it the macro must be left undefined / `#undef`'d, or the guards converted to `#if SO_REUSE`. As written, the fix deletes nothing; the finding stands only as "compiled-in machinery with no in-tree user, whose removal requires guard surgery, not a value flip."

## RE-SMS-115 — VERIFIED

- `iop/SMSTCPIP/src/memp.c`: `memp_sizes` includes `sizeof(struct raw_pcb)` (:55), `memp_num` includes `MEMP_NUM_RAW_PCB` (:68), `memp_memory` sizing includes the RAW term (:81); `MEMP_NUM_RAW_PCB` = 4 at `include/lwip/opt.h:78-79`.
- No `raw.c` exists in `iop/SMSTCPIP/src/` or the module Makefile. `LWIP_RAW` is **never defined anywhere** in the tree (grep for its definition returns nothing), so both `#if LWIP_RAW` sites — `ip.c` `ip_input` (:279/:310) and `api_msg.c` `recv_raw` (:42) — compile out, exactly as filed. `include/lwip/raw.h` declares the full PCB/API surface.
- Fix-completeness note (see Candidate C4): deleting `raw.h` also requires dropping its three includes — the finding's "delete the raw member references" only partly covers this.

## RE-SMS-116 — VERIFIED-DRIFT

- Six unimplemented declarations verified in `iop/SMSTCPIP/include/lwip/api.h`: `netbuf_alloc` (:101), `netbuf_free` (:102), `netbuf_chain` (:105), `netbuf_data` (:108), `netbuf_next` (:110), `netconn_err` (:150). Grep for all six over `iop/SMSTCPIP/src/` returns **no definitions** (exit 1); `api_lib.c` implements only `netbuf_new`/`netbuf_delete`/`netbuf_ref`/`netbuf_copy_partial` plus the `netconn_*` set minus `netconn_err`, as filed. Nothing in-tree calls any of the six.
- `ps2ip_ShutDown()` at `iop/SMSTCPIP/src/ps2ip.c:265-269` is `{ return 1; }` — empty stub as filed.
- **Slot-number correction:** enumerating `src/exports.tab` 0-based from `_start` gives ShutDown at index **2** and `ps2ip_Stub` at indices **1, 3, 32, 37**. The finding's "export slot 3" and "slots (2, 4, 32, 37)" mix bases — 3/2/4 are 1-based-from-`_start` while 32/37 are 0-based. Substance (one ShutDown stub + four reserved stub slots) is correct.

## RE-SMS-117 — VERIFIED

Each of the six prototypes is the sole occurrence of its name in the whole tree (word-boundary grep over `src/ include/ tools/ iop/`):

- `SMS_Align` — `include/SMS.h:305` only; distinct from the live `SMS_AlignBits` (`include/SMS_Bitio.h:114`, many callers) — verified separately.
- `SMS_SetSifCmdHandler` — `include/SMS.h:306` only, plus the stale end-comment at `src/SMS_IOP.c:196` (`/* end SMS_SetSifCmdHandler */`). The real function is `SMS_IOPSetSifCmdHandler`: declaration `include/SMS_IOP.h:76`, definition `src/SMS_IOP.c:192`, callers `src/SMS_GUI.c:978-982` and `src/SMS_IOP.c:1718` — rename trap exactly as filed.
- `SMS_StartNetwork` — `SMS.h:308` only. `SMS_ResetIOP` — `SMS.h:309` only. `SMS_HDDUMount` — `include/SMS_IOP.h:95` only. `SMS_MPEG_SCB` — `include/SMS_MPEG.h:286` only.
- Deleting a declaration with no definition cannot break a working call. Fix stands.

## RE-SMS-118 — VERIFIED

- `src/SMS_MPEG.c:904-935`: `SMS_H263_DCTUnquantizeInter` body is entirely `#if 0 … #endif` (:905-934) and references `aQScale` and `aN`, neither of which is a parameter (sole parameter is `apBlock`) or declared — the disabled body cannot compile if re-enabled, as filed.
- Wiring verified: assignments at `src/SMS_MPEG.c:205` and `src/SMS_MPEG4.c:513`; member at `include/SMS_MPEG.h:150`; prototype at `include/SMS_H263.h:27`.
- Never-invoked verified adversarially: grep for `DCT_UnquantizeInter` over `src/ include/` returns only the two assignments above, the MPEG2 assignment (`SMS_MPEG.c:210`), the member, and the prototype — **zero invocations through the pointer**. The only pointer unquantize call in the tree is `DCT_UnquantizeIntra` at `src/SMS_MPEG.c:1196`; the inter path at :1153 calls `SMS_MPEG2_DCTUnquantizeInter` *directly* (and only when `m_MPEGQuant`), bypassing the pointer entirely. So the H263 empty function is unreachable even via its own slot.
- Mechanical note for the fix: if the member is deleted, the MPEG2 assignment at `SMS_MPEG.c:210` must also be removed (the finding's "two assignments" are the two H263 ones).

## RE-SMS-119 — VERIFIED

- Local store: `uint32_t lBitRate[128]` at `src/SMS_ContainerASF.c:140`, zeroed at :149, and **never written again** (grep over the file: :140, :149, :326 is a shadowing local inside the handler, :350 is the read).
- File-data store: STREAM_BITRATE handler at :322-330 writes `lpASF->stream_bitrates[lStmID] = lBitRate` (:328); the field (declared :90) has exactly those two occurrences in the file — written, read nowhere.
- Final loop :346-352 applies the local: `if (!lpCodec->m_BitRate) lpCodec->m_BitRate = lBitRate[i];` (:350) — writes 0 over 0, as filed. The loop index `i` is the ASF stream id (via `asfid2avid[i]`), matching how the handler keyed `stream_bitrates`, so the proposed one-token fix is index-correct. 128 × 4 B = 512 B of stack confirmed.

## RE-SMS-120 — VERIFIED

- `ASFIndex` struct `src/SMS_ContainerASF.c:82-85`; `index_read` field :96; `index_ptr` field :116. Grep over the file returns only those declaration lines for all three names — never allocated, read, or tested.
- Container setup `SMS_GetContainerASF` (:695-723) sets `m_pName`, `ReadPacket`, `Destroy` only — no `Seek`, and never sets `SMS_CONT_FLAGS_SEEKABLE` (contrast `SMS_ContainerAVI.c:211`, `SMS_ContainerMOV.c:1469`, `SMS_ContainerMPEG_PS.c:879`, which do). Dead scaffolding exactly as filed; deletion touches nothing else.
- Not a duplicate of RE-SMS-119: that one is a mis-wired live data path (parsed bitrates dropped); this one is never-populated state for an unbuilt seek feature. Different mechanisms, different fixes.

---

## Duplicates check (within range)

No mechanism-level duplicates found. Pairs examined and cleared: RE-SMS-098/108 (same two headers — header hygiene vs dead stubs; RE-SMS-098's fix explicitly defers to RE-SMS-108's surviving prototypes); RE-SMS-113/114/115/116 (four distinct SMSTCPIP mechanisms: uncompiled/compiled-out code, compiled-in reuse machinery, a phantom memp pool, unimplemented API declarations); RE-SMS-119/120 (see above); RE-SMS-102/106 (disjoint byte sets — SMSSMB source tree vs UMS/USBD/SIO2MAN packed segments; the gap between them is Candidate C1).

## Tally

| Verdict | Count | Findings |
|---|---|---|
| VERIFIED | 17 | 097, 099, 101, 102, 103, 104, 105, 106, 107, 111, 112, 113, 115, 117, 118, 119, 120 |
| VERIFIED-DRIFT | 5 | 098, 108, 109, 110, 116 |
| WEAKENED | 2 | 100, 114 |
| REFUTED | 0 | — |
| BLOCKED-ENV | 0 | — |
| **Total** | **24** | RE-SMS-097 … RE-SMS-120 |

## Candidates (new defects noticed — not new findings)

- **C1 — Packed SMSSMB.IRX segment is dead bytes in every build.** `src/SMS_Data.c` carries a `/* SMSSMB.IRX */` segment of `SMS_SMB_SIZE` 19,641 B (19,648 aligned) inside `g_DataBuffer` (`include/SMS_Data.h:24,42`), yet `SMS_SMB_OFFSET` has no EE consumer (only the header offset chain into `SMS_SMSUTILS_OFFSET`). RE-SMS-106 counts only UMS/USBD/SIO2MAN (23,488 B); RE-SMS-102 defers this slot to "if the packed image is ever regenerated". ~19.6 KB of resident EE RAM unaccounted by either finding.
- **C2 — Second orphan IRX pair of the RE-SMS-110 class.** `irx/mmcedrv.irx` + `irx/mmcedrv.irx.xz` are not in the root Makefile `IOP_OBJS` and are referenced nowhere except a comment at `src/SMS_IOP.c:1023` stating mmcedrv is not required. Not filed in this range.
- **C3 — Explicit-format path also discards the creator's return value.** `src/SMS_Container.c:198`: `s_CCreator[ aPrefCont ] ( retVal );` does not assign `lSts`, so the `lSts < 0` teardown later in `SMS_GetContainer` can never fire for explicit probes (distinct from RE-SMS-101's bounds claim; impact limited to creators that return negative, currently observable only via `m_pName == NULL` handling).
- **C4 — RE-SMS-115's deletion fix is build-incomplete as written.** `include/lwip/raw.h` is included by `iop/SMSTCPIP/src/ip.c:52`, `iop/SMSTCPIP/src/memp.c:37`, and `iop/SMSTCPIP/include/lwip/api.h:41` (which also embeds the `raw` member at api.h:86). Deleting the header without dropping those includes breaks the SMSTCPIP build; the finding's "delete the raw member references" does not name the includes.
