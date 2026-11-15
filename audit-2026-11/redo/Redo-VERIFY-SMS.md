# Adversarial Verification — Redo Audit SMS Report

**Subject:** `~/workspace/Sable-audit-redo/Redo-SMS.md` — 144 findings, RE-SMS-001…144.
**Clone:** `~/workspace/audit-clones-2026-11-13/Simple-Media-System` at pin `e65ced39a27eee8d996e9257e21594d1cac1c93d` (HEAD verified before work; working tree clean after work; read-only throughout).
**Method:** refute-first. Six verifiers (24 findings each) opened every cited file at the pin and re-derived each mechanism from the code, its types, and its callers, hunting for the defeating guard/caller/type before accepting anything. Every table/count/byte claim was re-derived by script (initialiser counts, segment sums, maximum reachable index vs true table extent). Fixes were checked for sanity and LESS-NOT-MORE compliance; bodies were read for mechanism-level duplicates.
**Coordinator spot-checks (first-hand, in the clone):** the RE-SMS-001 refutation was re-verified directly — `src/SMS_Container.c:148-158` shows `int lID` tested as `(lID = SMS_ContID(...)) && (lID < sizeof(s_CCreator)/sizeof(s_CCreator[0]))`; the `size_t` right operand converts −1 to 4294967295, the comparison is false, and `s_CCreator[-1]` is never indexed. The RE-SMS-025 narrowing was likewise re-verified — `iop/SMSTCPIP/include/lwip/ip.h:75-82` (`IP_PCB` common prefix in both PCB types) and the IPPROTO_TCP early return at `iop/SMSTCPIP/src/sockets.c:955`.
**Section files:** `redo-verify-sections/verify-sms-W1.md` … `verify-sms-W6.md` carry the full per-finding evidence trails behind the rows below.

**Verdict key:** VERIFIED — mechanism, citations and figures hold · VERIFIED-DRIFT — mechanism holds, a citation/line/figure is wrong (corrected value given) · WEAKENED — defect real but narrower/lower-impact/more conditional than claimed (true scope stated) · REFUTED — defeating evidence · BLOCKED-ENV — not settleable in this VM.

---

## RE-SMS-001 … 024

## RE-SMS-001 — REFUTED
Unknown-extension `s_CCreator[-1]` call. Defeating evidence: the guard at `src/SMS_Container.c:152-153` compares `int lID` against `sizeof(s_CCreator)/sizeof(s_CCreator[0])`, a `size_t`; −1 converts to unsigned 4294967295 and `4294967295 < 13` is false, so the branch is never taken for unknown extensions and execution falls to the `else memcpy` default order. `SMS_ContID` does return −1 exactly (`src/SMS_FileDir.c:124-185`), and the table has 12 creators + NULL (`SMS_Container.c:45-60`), but the call at :158 is unreachable for −1. Proven with a compiled test of the identical expression shape (0 for −1, 1 for 5, 0 for 0). The finding paraphrased the test as signed `lID < 13`, which is not what the code computes; the proposed fix is unnecessary.

## RE-SMS-002 — VERIFIED
MPEG-4 sprite warp count: `m_nSpriteWarpPts = SMS_GetBits(...,6)` (0–63, unchecked) at `src/SMS_MPEG4.c:933`; loop writes `int lD[4][2]` at :1167-1201. No clamp at any other use (:1236, :1371). Fix (reject >4 at parse) is one comparison.

## RE-SMS-003 — VERIFIED
`ip_reass` byte-swapped length: swap at `iop/SMSTCPIP/src/ip_frag.c:198-199`, swapped value feeds `pbuf_alloc(PBUF_LINK, …, PBUF_POOL)` (:213) and the copy bound (:222); PBUF_POOL returns a single 1540-byte pool pbuf with `len` set blindly (`pbuf.c:176-185`; `lwipopts.h:63`). Example re-derived: 300 → 0x2C01 = 11,265. Copy overruns the 1540-byte pool buffer and the 5780-byte `ip_reassbuf`.

## RE-SMS-004 — VERIFIED
SMSUMS FAT LFN sequence trusted into `m_Name[128]` (`iop/SMSUMS/src/ums_fat.c:21,124,137`, LFN branch :421-441): max offset (63−1)*13 = 806, writes to ~819. No bound check. (Verifier strengthening: sequence byte 0 gives `lOff = −13`, a write *before* the buffer — see Candidates.)

## RE-SMS-005 — VERIFIED-DRIFT
DTS `prim_channels` (3-bit +1 → 1–8, `src/SMS_DTS.c:10756`) loops over state fields all sized `[DTS_PRIM_CHANNELS_MAX]` = 5 (:26, script-counted :101-117); `_dts_syncinfo` never sees `prim_channels`, so no upstream validation defeats it. **Drift:** the stack array `subband_samples` is declared at **:11168**, not :11171 (:11171 is the `quant_step_table` assignment); its indexing loop is :11174 as cited.

## RE-SMS-006 — VERIFIED
`SMS_LZMA2Decompress` (`src/SMS_EE.c:946-965`): the size-mismatch branch frees and NULLs `retVal`, then `lzma2_uncompress(..., UNCACHED_SEG(retVal), lSize)` runs unconditionally with its return discarded. Caller `SMS_InitVU` checks for NULL, so the proposed `return NULL` matches the existing contract.

## RE-SMS-007 — VERIFIED
`SifExecDecompModuleBuffer` (`src/SMS_IOP.c:237-243`): `if (ret > 0) SifExecModuleBuffer(...)`; the XZ enum (`include/xz.h:112-122`) makes every error value 2–8 satisfy `ret > 0`, so corrupt/truncated blobs execute. The `memalign` at :239 is also unchecked. Fix (`ret == XZ_STREAM_END` + NULL check) is three lines.

## RE-SMS-008 — VERIFIED
ISO9660 name length trusted past `m_Filename[129]` (`iop/SMSCDVD/SMSCDVD.h:96`): `_copy_name` (`SMSCDVD.c:858-863`) does `strncpy(...,128)` then `apName[lLen]=0` with `lLen` up to 255; same pattern in `TocEntryCopy` (:1085-1091). Joliet branches halve the length and are safe, as filed.

## RE-SMS-009 — VERIFIED-DRIFT
`get_socket()` (`iop/SMSTCPIP/src/sockets.c:104-115`) indexes `sockets[aSD]` with no range check (NUM_SOCKETS 15; FD_SETSIZE 16; `lwip_selscan` loops caller-supplied `maxfdp1` unclamped). **Drift:** there are **17 call sites** in sockets.c, not 19. All callers already handle NULL, so the one-check fix is sound.

## RE-SMS-010 — VERIFIED-DRIFT
DTS unchecked table indices. Script-counted tables: `bitalloc_12[5]` (:189), `scale_factor_quant6[64]` (:1108), `scale_factor_quant7[128]` (:1127), downmix `table[11][10]` (:10203). Sinks hold: `bitalloc_12[7]` passed to `_dts_inverseq` (:11344-11346); LFE `scale_factor_quant7[bitstream_get(8)]` (0–255 into [128], :11440); unbounded cumulative `scale_sum`. **Drift:** the `scale_table[scale_sum]` reads are at **:11382 and :11393**, not :11386/:11402. **Understatement:** at :10261 *both* downmix indices are unchecked — the row index `*flags & 0x3F` can itself reach 63 into [11] via the `_dts_decode` else-branch (:11567-11571), not just `amode` into [10] (see Candidates).

## RE-SMS-011 — VERIFIED-DRIFT
DTS SPDIF passthrough: header `aLen` and sample-derived `retVal` are never reconciled; `memset(lpOutBuf + 8 + aLen, 0, retVal - 8 - aLen)` at :11693 goes negative/huge when `aLen > retVal - 8`. **Drift:** actual lines are :11644/:11645/:11693 (filed :11651/:11652/:11694); `aLen` max is **16,384** (14-bit field +1), not 16,383; `retVal` "at most 8,192" holds only for the largest valid frame type — the field allows 4096 samples → 16,384. The negative-memset conclusion is unchanged.

## RE-SMS-012 — VERIFIED
DXSB decoder trusts header width/height against the scratchpad row buffer; packet size never checked before use (`src/SMS_DXSB.c:195-204`; `aSize` only decremented at :213, after the header read). Capacity re-derived: `SMS_SPR_FREE` 0x70001B30 to scratchpad end 0x70004000 = 9,424 bytes. Width > ~9400 overruns it.

## RE-SMS-013 — VERIFIED-DRIFT
Unchecked stream/codec/user-data allocations dereferenced in nine container creators — every cited site opened and confirmed (Container.c:351-352; AAC :197/200/212; AC3 :109-112; ASF :157-159 and codec at :212; AVI stream checked :104 but codec not, :107, :1164-1166; FLAC :123-126,138; JPG :64-84; M3U :308-312,366-370; MP3 :121-124; OGG :105-108). The checked pattern in MOV/MPEG-PS is real. **Drift:** AAC user-data write is at **:215**, not :218; FLAC struct copy is at **:141**, not :146.

## RE-SMS-014 — VERIFIED-DRIFT
Raw config blob trusted as indices: 892 raw bytes loaded into `g_Config` after only a 4-byte version check (`src/SMS_Config.c:696-702,727-732`); consumers index `sl_Speeds[3]`/`s_Speeds[3]` by an unsigned-char field (to 255) and `g_Palette[g_Config.m_*Idx - 1]` by unsigned-int fields (0 → `g_Palette[-1]`). **Drift:** the only post-load guards are at **:766 and :768**, not :773/:775; there are **22** `g_Palette[...]` read sites, not 26. **Fix correction (important):** `g_Palette` is `unsigned int g_Palette[16]` (`SMS_Config.c:51`) and defaults use indices up to 16 — the valid clamp is **1..16**, not the finding's 1..17, which would still permit `g_Palette[16]`, one past the array.

## RE-SMS-015 — VERIFIED
`sprintf` into `g_SaveDiag[64]` (`src/SMS_Config.c:69`) with paths from `s_pMC0SMC[128]` (:64) at :943,945,1082-1087; worst case ≫ 64 bytes. Consumer at `SMS_GUIMenuSMS.c:1007`.

## RE-SMS-016 — VERIFIED
`ip_reass` trusts `IPH_LEN` over the actual pbuf chain (`ip_frag.c:143-159`; `copy_from_pbuf` :58-77 has no NULL stop; `pbuf_realloc` cannot grow, `pbuf.c:242-248`). Bitmap bound re-derived by script: `ip_reassbitmap` is 90 bytes; at total exactly 5760 the index `(offset+len)/64` = 90 → one byte OOB at :174 (write) and :195 (read).

## RE-SMS-017 — WEAKENED
UDF descriptor lengths. The allocation-descriptor OOB **read** is real: `iop/SMSCDVD/SMSCDVD_UDFS.c:293-347` — disc u32 `lLEA`/`lLAD`, `p = 176 + lLEA`, loop reads `UDFAD(&apData[p],…)` unbounded with `p += lLAD`, and the sum can wrap. **Narrowing facts:** (1) `apData` points into `s_Buffer[9*2064]` = 18,576 bytes (`SMSCDVD.c:196`), not a 2,048-byte buffer — the OOB read needs a larger `lLEA` than claimed, though a u32 `lLEA` still reaches far past it. (2) The claimed `UDFDecode` **write** overflow is unreachable at both real call sites: at :138 `aLen` is the constant 128 into `m_VolDesc[128]` and the loops fit exactly; at :399 `aLen = lLFI` is a u8 ≤255 into `s_ScanTemp[2048]`. What is reachable on that path is an OOB **read** via source offset `38 + lLIU`. True scope: allocation-descriptor OOB read (real) + latent decode contract + OOB source read; no demonstrated name-decode write overflow.

## RE-SMS-018 — VERIFIED
Path component copied unbounded into `unsigned char lName[257]` (`iop/SMSUMS/src/ums_fat.c:607`, copy at :623, terminator at :626). A component >256 bytes overruns the stack buffer.

## RE-SMS-019 — VERIFIED
`UmsWarmUp` loops over a device-supplied LUN count (`ums_driver.c:283-298`, 0–255) writing `m_LUName[16][19]` (`ums.h:54`) at :214-257; lnLUN > 15 writes past the table. Verifier secondary correction: the −1 trim read at :234-236 is not conditional on the dedup path as filed; the dedup writes at :247-248 use lLen+1 = 0 and are in-bounds.

## RE-SMS-020 — VERIFIED
USB device geometry trusted unchecked: `UmsGetCapacity` stores the device sector size verbatim (`ums_driver.c:313`); divisions by it at `ums_driver.c:395`, `ums_cache.c:40`, `ums_fat.c:226-227`, and `>>5` / `/32` divisor forms at `ums_fat.c:236,395` are zero for sector size 0 or <32. The BPB whitelist covers only the BPB's own fields; `FATFS_Init` allocates from the unvalidated value (:989, :1002). Fix reuses the existing whitelist — LESS-compliant.

## RE-SMS-021 — VERIFIED
`TranslateUTF8` writes up to `bufSize` bytes then unconditionally `*outBuff = '\0'` at outBuff[bufSize] (`src/SMS_InverseCodePages.c:138-185`). Callers at `SMS_SubtitleContext.c:483,657-659` pass full buffer sizes; the safe caller (`DisplayUTF8`, :233) passes `aSize - 1`. The finding's own scoping (latent contract defect; current subtitle inputs are length-bounded elsewhere) is accurate.

## RE-SMS-022 — WEAKENED
AAC sample-rate index into 12-entry window tables (script-counted exactly 12 entries each, `src/SMS_AAC.c:2006,2056,2065,2093`; sinks :6204-6220). **ADTS path verified** (also ADIF, unmentioned by the finding): `sf_index` 12–15 stored at :6041, `_NeAACDecInit` returns 1 unconditionally despite `get_sample_rate()` returning 0. **ASC path narrower than claimed:** the main ASC index *is* rejected — `AudioSpecificConfig2` returns −2 when `get_sample_rate()==0` (:6095-6098) and `_NeAACDecInit2` checks `rc != 0` (:6182), contrary to the finding's claim that it passes via the explicit rate. ASC exposure survives only via the unvalidated SBR-extension sub-index (12–15) at :6105-6110/:6132-6137, copied to `hDecoder->sf_index` at :6172. True scope: ADTS/ADIF fully exposed; ASC only via the SBR extension index. The proposed fix (fail init when `get_sample_rate(sf_index)==0` in both paths) still closes all of it.

## RE-SMS-023 — VERIFIED-DRIFT
AAC DRC `excluded_channels` loop writes past `exclude_mask[64]`/`additional_excluded_chns[64]` (`src/SMS_AAC.c:333-334`, loop :7548-7561, `n` is uint8_t). **Drift:** by simulation, the first OOB write is at the **9th** continuation iteration (`exclude_mask[63..69]`), not "after the 10th". The `n` wrap at 255 also holds. Fix (stop when `num_excl_chan + 7 > MAX_CHANNELS`) holds.

## RE-SMS-024 — VERIFIED
WMA v1 exponent bands can be zero-width (`src/SMS_WMA.c:1347-1361`, no `pos > lpos` test; the v2 path at :1386 has exactly that guard) and `decode_exp_vlc`'s `do { *q++ = v; } while (--n)` underflows on a 0 entry (:1641-1660, `n` is int). The finding's INFERRED reachability is now **proven**: simulation over the extracted `wma_critical_freqs[25]` shows zero-width v1 bands at 44100 Hz/block 64 (7), 48000/64 (8), 32000/64 (5, incl. band 0), and others. Table-driven v2 rows script-verified zero-free.

---

## RE-SMS-025 … 048

## RE-SMS-025 — WEAKENED
getsockopt/setsockopt cross-PCB claims. **The headline UDP cross-struct corruption is refuted:** `so_options`/`tos`/`ttl` live in the common `IP_PCB` prefix (`iop/SMSTCPIP/include/lwip/ip.h:75-82`) at identical offsets in `tcp_pcb` (`tcp.h:205`) and `udp_pcb` (`udp.h:56`) — accessing them via the union member is lwIP's intended idiom, not corruption. TCP-only fields are unreachable for UDP: both functions `return 0` in validation when `conn->type != NETCONN_TCP` (`sockets.c:955` and the setsockopt switch). The "PCB never created before bind/connect" premise is false: `do_newconn` allocates the PCB at socket creation. **Surviving scope:** after `err_tcp` (`api_msg.c:183`) sets `pcb.tcp = NULL`, a subsequent getsockopt/setsockopt at SOL_SOCKET/IPPROTO_IP level dereferences NULL with no guard (unlike `netconn_peer`, `api_lib.c:219,226`). A post-error TCP-only crash — the fix's NULL check is sound; its ENOPROTOOPT dispatch is unnecessary for SOL_SOCKET/IP.

## RE-SMS-026 — VERIFIED
`accept_function`/`do_newconn` path (`api_msg.c:220-272`): `setup_tcp` installs PCB callbacks with `arg=newconn` (:240) *before* `sys_mbox_new` (:241,246) and `sys_sem_new` (:252); each failure arm frees only the netconn and returns ERR_MEM — the PCB is never closed and its callbacks never cleared, leaving it pointing at freed memory. Trigger (alloc failure + later traffic) is INFERRED, as filed.

## RE-SMS-027 — VERIFIED
`tcp_process` SYN_SENT branch dereferences `pcb->unacked` unconditionally at `tcp_in.c:526-527`; `tcp_connect` sets SYN_SENT *before* `tcp_enqueue` and the state persists on enqueue error (`tcp.c:516,525-529`); `do_connect` discards the return (`api_msg.c:518-519`). The in-tree loopif comment (`loopif.c:86-95`) documents `unacked` still NULL here. (The debug print at :524-525 is inert in this config but does not save the mechanism — the accept condition itself derefs.)

## RE-SMS-028 — VERIFIED
ASF packet sync divides by file-controlled `packet_size` (`src/SMS_ContainerASF.c:368`, `(CurPos - data_offset) % packet_size`): `packet_size = hdr.max_pktsize` at :190-192 with no range check, and the header walk (:167-172) can end without any file-properties object. Zero → modulo-by-zero on first packet read.

## RE-SMS-029 — VERIFIED
AVI ODML super-index loading recurses on file-controlled offsets with no depth bound (`src/SMS_ContainerAVI.c:226-302`; self-call at :299 on a file-supplied 64-bit offset). Legitimate nesting terminates at type 1; nothing distinguishes a self-referential super-index. Depth-bound fix (4) is justified.

## RE-SMS-030 — VERIFIED
AAC codebook-11 escape values: `huffman_getescape` (:6559-6575) increments a uint8_t counter per leading 1-bit with no cap; the result truncated to int16_t already reaches |q| 8192–16383 at i=13, and the `vquant` asm (:6745-6767) indexes the script-counted 8,192-entry `_aac_iq_table` (:887) with no range check. No caller-side clamp. Trigger INFERRED, as labelled.

## RE-SMS-031 — VERIFIED
Constant 16428320 (0xFAAD20) wired into the decode path: one reachable overwrite (`hf_assembly`, `src/SMS_AAC.c:5030`, on SBR extension payload id 3/data 42), two dead remnants — `huffman_codebook` case 12 (:6552-6557,6589-6594) is unreachable because `section_data` rejects `sect_cb==12` with error 32 (:6304), and the `get_sr_index` branch (:3809) is arithmetically dead. Pure-deletion fix.

## RE-SMS-032 — VERIFIED
Two-entry float-to-int pointer table indexed by unvalidated channel count in both decoders (`src/SMS_AC3.c:2576-2585`, `src/SMS_DTS.c:11556-11565`, indexed `anChannels-1` before the 1/2/other branching); the player clamp (`SMS_Player.c:100-110`) no-ops at count 0, after which the decoder adopts up to 6 and `s_FloatToInt[5]` is called. The exact interleaving is INFERRED, as the finding labels it.

## RE-SMS-033 — VERIFIED-DRIFT
MP123 gain pointer + scalefactor index vs `s_GainPow2[378]` (script-counted exactly 378, `src/SMS_MP123.c:190`). **Drift:** scalefactors are **not** 6-bit/63 — `_1` uses at most 4-bit fields (max 15) and `_2`'s length tables max at 4/5 bits, so the maximum scalefactor is **31**. Corrected worst cases: direct reads (:5741/:5853) 258 + (31<<2) = **382 — 4 entries past**, not ≈508/~130 past; via `m_pFullGain` (+56) **438 (60 past)**; via pretab (max 3) 394. The mechanism holds on every path; the finding also omits the +2 MS-stereo term in the base (max 258). The fix must be sized for the FullGain path (438).

## RE-SMS-034 — VERIFIED
MPEG picture allocation assumes success: `_mpeg_alloc_picture` (`src/SMS_MPEG.c:36-58`) leaves seven callocs unchecked (a NULL motion-val base becomes a 0x2-based array at :49); `FindUnusedPic` explicitly returns NULL (:130-145) and `SMS_MPEG_FrameStart` dereferences the result unconditionally (:181-185).

## RE-SMS-035 — VERIFIED
VLC/RL table construction and the DivX bitstream stash use allocation results unchecked: `SMS_VLC.c:223-227,268-272` (malloc then immediate memcpy/fill); `SMS_MPEG4.c:3391-3392` (realloc assigned directly to `g_MPEGCtx.m_pBSBuf` then memcpy'd — failure loses the old buffer and NULL-derefs). The same VLC file's `_SMS_VLC_AllocTable` does check its realloc, as filed.

## RE-SMS-036 — VERIFIED-DRIFT
List allocation contract inconsistent: `SMS_ListPush`/`SMS_ListPushBackBuf` unchecked (`src/SMS_List.c:17-21,57-71`) while `SMS_ListPushBack` checks (:36-40); `SMS_ListAppend` dereferences its result (:319-325). **Drift (understatement):** the direct-deref pattern `SMS_ListPushBack(...)->m_Param =` occurs at **21 sites, not 18** — the finding's list misses M3U ×3, GUIDevMenu, GUIFileCtxMenu, and GUIFileMenu ×2. Plus `SMS_SubtitleContext.c:278,334` assign through `m_pTail` immediately after a push.

## RE-SMS-037 — VERIFIED
History load: three near-identical load loops (`src/SMS_History.c:41-52,78-89,97-108`; two compile per build), file-supplied `lSize`, payload/`m_Param` read results ignored, `HIST_SIZE 32` enforced only in `SMS_HistoryAdd` (:141), never on load — a crafted `SMS.hst` grows the list until the unchecked `PushBackBuf` calloc (RE-SMS-036) NULL-derefs. Fix (one loop, cap, require full reads) is a net deletion.

## RE-SMS-038 — VERIFIED
Subtitle loaders: `_load_sub`/`_load_srt` (`src/SMS_SubtitleContext.c:451-453,592-594`) dereference `malloc`'d SubNode and `SMS_ListInit()` results unchecked, per entry, with no count cap; `_produce_packets` (:714-716) offsets a `memalign` result immediately and leaves a `calloc` unchecked. (The cap value in the fix is a judgement call, not code-proven — noted, not a defect in the finding.)

## RE-SMS-039 — VERIFIED
Object constructors and menu builders dereference calloc/malloc results unchecked — every cited site re-read (GUIMenu :405-412, :377-381; GUIDesktop :132-135,730-734,799-802; GUICmdProc :85-89, :363-364 menu-array callocs indexed at :369-374, :385 playlist tail forming `lpItems[-1]` on an empty list; GUIMenuSMS :3027-3035; GUIFileMenu :1645-1655; MiniBrowser :132-133; RingBuffer :23-36; PCM :26-31).

## RE-SMS-040 — VERIFIED
Directory-tree scan/walk/destroy recurse to filesystem depth with no bound (`src/SMS_DirTree.c:153,224,258`); `SMS_DirTreeInit` passes an unchecked calloc result to `SMS_DirAdd` (:46-65) and reads `lRoot[lLen-1]` at index −1 for an empty root string (:53); callers `SMS_CopyTree.c:150-151,252-255` use the init result unchecked. Overflow depth INFERRED, as labelled.

## RE-SMS-041 — VERIFIED
`lwip_close` (`sockets.c:226-255`): the `!sock` branch sets EBADF (:237-239), then :250 unconditionally runs `sock_set_errno(sock, …)` — a NULL-pointer write for any in-range but unopened descriptor.

## RE-SMS-042 — VERIFIED
`do_bind` (`api_msg.c:386-435`) feeds `udp_new()`/`tcp_new()` results (:393,398,403,409) straight into `udp_setflags`/`udp_recv`/`setup_tcp`/`*_bind` with no NULL check, in the branch that runs precisely when the PCB is NULL (e.g. after `err_tcp`). Siblings `do_newconn`/`do_connect` check every allocation and post ERR_MEM. Fix copies the sibling pattern.

## RE-SMS-043 — VERIFIED
`icmp_dest_unreach` (`icmp.c:136-148`) dereferences its `pbuf_alloc` result unchecked (:141) and copies `IP_HLEN+8` from the offending packet with no `p->tot_len` check (:145); the `ip.c:306` unknown-protocol caller can present a packet over-read by the full 8 bytes. The same pattern in `icmp_time_exceeded` (:169) is compiled out, as filed.

## RE-SMS-044 — VERIFIED
`ip_frag` dereferences both `pbuf_alloc` results unchecked (`ip_frag.c:262-264` — `rambuf->tot_len` written on the next line; :310-312 header). Nuance: `pbuf_cat` tolerates a NULL head; the crash lands in `netif->output`/`pbuf_header` on the NULL header, as filed.

## RE-SMS-045 — VERIFIED
`lwip_accept` (`sockets.c:143-193`): `netconn_accept` can return NULL (err_tcp posts NULL to the acceptmbox) and :161 calls `netconn_peer`, which dereferences `conn->type` (`api_lib.c:215`) guarding only `pcb.*==NULL`; :170 dereferences `*addrlen` and :172 memcpys to `addr` with no NULL check on either (addr==NULL is the legal POSIX form). The errno asymmetry (failure sets the listener's errno; success clears the *new* socket's) is also as filed.

## RE-SMS-046 — WEAKENED
`netconn_recv` (`api_lib.c:384-462`). **Narrowing evidence:** (1) A *sequential* post-reset TCP recv never reaches the :403 deref — `err_tcp` sets `conn->err != ERR_OK` and :399 returns NULL first. The deref needs a genuine cross-thread race: recv passes :399 while `err==ERR_OK`, then the tcpip thread runs `err_tcp` (which NULLs `pcb.tcp` at `api_msg.c:183` *before* setting `conn->err`) before :403 executes. Real, but a narrow in-flight window, not "a recv after a reset" generally. (2) The UDP arm's NULL-buf claim has **no in-tree poster**: `recv_udp` posts only successfully allocated bufs, and the only NULL recvmbox post in the tree is `err_tcp` (:191), which is TCP-only. True scope: one racy TCP guard worth adding (the proposed fix is cheap and correct); the UDP half is a latent missing guard with no demonstrated delivery path on this pin.

## RE-SMS-047 — VERIFIED
`lwip_recvfrom` (`sockets.c:355-365`) dereferences `netbuf_fromaddr(buf)`, which is a bare `b->fromaddr` macro (`api.h:115`); the TCP arm of `netconn_recv` explicitly sets `buf->fromaddr = NULL` (`api_lib.c:431-434`), so recvfrom with a `from` buffer on any TCP socket NULL-derefs. UDP buffers always carry a fromaddr.

## RE-SMS-048 — VERIFIED
`netbuf_ref` (`api_lib.c:70-79`) dereferences its `pbuf_alloc` result at :76 unchecked. Pool size re-derived: `MEMP_NUM_PBUF 40` (`lwipopts.h:18`). Caller `lwip_send` UDP arm checks `netbuf_new()` (:409) but not `netbuf_ref`'s result (:413).

**Split assessment (W2 range):** RE-SMS-041…048 are eight distinct functions/triggers/guards, not one mechanism split to inflate; RE-SMS-034…040 likewise. The only overlaps are the findings' own cross-references (037→036, 038→036), which do not double-count.

---

## RE-SMS-049 … 072

## RE-SMS-049 — VERIFIED
`pbuf_header` (`pbuf.c:291-315`): the RAM/POOL arm checks only the lower bound, so a negative `header_size` passes trivially and `p->len += header_size; p->tot_len += header_size` wrap the u16 fields (`pbuf.h:66-84`). The wire-facing caller `ps2ip_input` (`ps2ip.c:238-246`, QUEUE_INPUT 0) calls `pbuf_header(pInput, -14)` with no `tot_len >= 14` check; `etharp_ip_input` reads a 34-byte header struct unchecked (`etharp.c:391-402`), and `ip_input` rejects `iphdrlen > p->len` but not `iphdrlen < IP_HLEN` (`ip.c:202-230`). Pool size re-derived from `lwipopts.h:63` (1540, overriding opt.h). Severity framing (data-integrity, INFERRED) is the finding's own and is right.

## RE-SMS-050 — VERIFIED
UDPFS `_request` (`udpfs_core.c:72-80`) returns the raw received count into a shared static 312-byte buffer, so an unreceived tail is the previous reply; callers check only `ret < 0` — `_path_op` (:166-171, neither length nor msg_type), open (:373-383), lseek (:519-532), getstat (:622-642); dread (:559-587) compares `name_len` only with `name_max`, never with `ret - sizeof(reply)`, and still returns success on the clamp path. The in-file contrast is real: `_recv_with_result` (:89-136) clamps a wire count to bytes actually received, with an explanatory comment.

## RE-SMS-051 — VERIFIED
smbman-sms: `smb_QueryPathInformation` (`smb.c` ~:908-930) and `smb_QueryInformationDisk` (:799-838) check only `r <= 0` + magic + status, then read fields from the response overlay without relating `DataOffset`/`DataCount` to `r`. The sibling standard is confirmed in the same file: `smb_NetShareEnum`, `smb_FindFirstNext2` (:1540-1581) and `smb_ReadAndX` (:1127-1151) all validate offsets/counts against `r`. These two are exactly the unpatched remainder claimed.

## RE-SMS-052 — VERIFIED
smbman-sms `smb_Echo` (`smb.c:1648-1664`) memcpys an EE-supplied `(u16)len` into the 4,224-byte static SMB buffer (extent re-derived: 4096+128, `smb.h:892-893`) with no cap, and `memcmp`s with the untruncated `int len`. Source struct is `smbEcho_in_t { char echo[256]; int len; }` (`ps2smb.h:65-70`), reached via devctl `SMB_DEVCTL_ECHO` (`smb_fio.c:1147-1148,1094-1097`). The finding's scoping is accurate: internal callers use fixed 18/4-byte echoes; the exposure is the devctl entry itself.

## RE-SMS-053 — VERIFIED
`lzma2_get_uncompressed_size` walks the XZ index with no bounds (`src/lzma2.c:84-122`): `footer = buf + size - 12` is formed before any check (size < 12 points before the buffer), the header loop reads 6 bytes regardless of size, and `index = footer - ((i+1)*4)` is never compared with `buf`; `backward_size` is a plain unsigned int from the file (`lzma2.h:16`). `lzma2_decode_size` (:51-82) discards `decode()`'s 0-on-error, so a malformed varint leaves `buf` unmoved and bytes are re-read. Callers include the user-file path `GSFont_Load` (`SMS_GS_2.c:567`), which calls it *before* its equality check, so the check cannot protect the walk.

## RE-SMS-054 — VERIFIED
Unbounded whole-file loads sized by the file itself: three `.lng` loads (`SMS_Locale.c:841-845,874-878,888-892`), `_load_font` (`SMS_Config.c:286-307`), `_copy_file_to_mc` whole-file malloc+read (`SMS_GUIFileCtxMenu.c:143-196`, where a streaming `SMS_CopyFile` exists in-tree), and `_load_rmman` (`SMS_RC.c:82-138`) — the last with **no NULL check at all** before `SifExecModuleBuffer`. Contrast confirmed: `DrawSkin` caps at 0xFFFF0 and checks read length.

## RE-SMS-055 — VERIFIED
`SMSContainer_DefReadPacket` (`SMS_Container.c:308-335`): (1) dereferences a possibly-NULL packet — `_AllocPacket` (:62-82) returns NULL on ring-buffer failure for non-zero sizes, and FLAC/MP3/OGG defaults (1024/4096) make the NULL path operative; siblings AAC (:152-155) and AC3 (:67-70) wrap the identical call in `if (lpPkt)`. (2) `lnRead` computed at :320-321 is never used; the Read at :322 still requests the full `lPackSize` — dead code as filed. (3) Unallocates from `m_pStm[0]` (:325-326) regardless of the allocating index — latent, since every current setter writes `m_DefPackIdx = 0`, exactly as the finding labels it.

## RE-SMS-056 — VERIFIED
ASF fragment reassembly (`SMS_ContainerASF.c:537`): `realloc(pkt.data, pkt.alloc = pkt.size)` assigns the new capacity as an argument to the same call — on failure `pkt.data` becomes NULL (old buffer leaked) while `pkt.alloc` claims the new size. `pkt.size` is bounded to < 2^24 (:433), a plausible realloc failure on this heap. The downstream guard (:565-568) prevents a write through NULL today; the finding claims only the lying capacity + leak + future-edit hazard, correctly.

## RE-SMS-057 — VERIFIED
`SMSContainer_SetName` (`SMS_Container.c:284-303`) scans backwards for a path separator with no lower bound; a separator-less path walks off the front of the allocation, and an empty path starts one byte *before* the buffer and dereferences it on the first test. The M3U reachability shape is INFERRED, as labelled; the missing bound needs no inference.

## RE-SMS-058 — VERIFIED
MOV `mdhd`/`hdlr` use-after-free: `_mov_read_hdlr_m4a` (`SMS_ContainerMOV.c:297-320`) frees the stream and NULLs the slot without decrementing `m_nStm`; `_mov_read_mdhd` (:224-240) dereferences the NULL slot's `m_pCtx` with no test. The verifier proved the precondition the finding only inferred: `SMS_GetContainerM4A` (:1538-1547) swaps `hdlr` into the *same* `s_MOVParseTbl` that contains `mdhd`, and `_mov_read_dflt` dispatches children in file order with compaction only later (:1072-1082). Sibling defences exist (`esds` comment :429-435; `elst` NULL check). Stronger than filed: the mechanism is fully code-proven.

## RE-SMS-059 — VERIFIED
FLAC fixed-predictor subframe (`src/SMS_FLAC.c:168-196`): the a/b/c/d computation at :182-185 is unconditional, reading `decoded[pred_order-1]` … `decoded[pred_order-4]` — for orders 0–3 that is up to 4 samples *before* the channel buffer — and the switch at :187-195 then uses only the values its case needs, so the reads are both out of bounds and discarded. Caller passes `pred_order = type & ~0x8` for type 8–12 (:286-287).

## RE-SMS-060 — WEAKENED
WMA superframe loop trusts container `block_align` (`src/SMS_WMA.c:1247` in `wma_init`, no validation; loop :2040-2100). **Proven:** the bit-context size is taken from `block_align` and never reconciled with the remaining input — on a final chunk smaller than `block_align` the decoder is handed a bit length larger than the data present (and the shared bit reader has no end check), so the partial-final-frame over-read stands; with `block_align == 0` the loop advance is provably zero. **Not proven:** the filed flat claim "0 → hang; the player hangs" additionally requires `wma_decode_frame` to keep *succeeding* on a zero-length bit context; if it errors first, the function returns. True scope: over-read VERIFIED; zero-`block_align` non-advance VERIFIED as a loop property; hang conditional on the frame decode not erroring first. The proposed fix (reject 0 at init + bound by remaining length) remains sound for both halves.

## RE-SMS-061 — WEAKENED
AC3 `_ac3_bit_allocate` indexes `s_LATab[256]` (script-counted exactly 256 entries) at `src/SMS_AC3.c:1170,1176` with a plain unclamped int index — **but the filed mechanism ("exponent drop of exactly 4 → index 256") is disproven for parsed data.** All `m_Exp` arrays come from `_ac3_parse_exponents` (:628-703), whose per-step deltas are script-verified to [−2,+2] (the three poison entries of 25 abort the frame via the `anExp > 24` return). Adjacent bins therefore differ by at most 2. Inside the delta loop the invariant `lPSD ≤ 128 * lpExp[prev]` caps `lDelta ≥ −256`, so the case-−1 index is ≤ 128 and case-0 is ≤ 255 by the switch range. −512 is unreachable from any freshly parsed or whole-array reused exponent sequence. Residual scope the finding does not argue: an `EXP_REUSE` boundary where a later frame's ranges grow into never-written (statically zero) slots could in principle create an artificial 4→0 adjacency — no evidence was produced that such a boundary lands inside a bit-allocation band with `lPSD` on the high value. True scope: an unclamped index whose only proven bound is the producer's ±2 step — a latent one-element over-read contingent on stale-reuse boundary data, not the demonstrated mechanism as filed. The clamp fix is harmless and cheap.

## RE-SMS-062 — VERIFIED
CDDA `_read_string` (`src/SMS_FileContext.c:803-822`): VLA filled from disc, only the boolean read result tested, then `strlen` on the unterminated buffer; the follow-on malloc/strcpy copies whatever length the over-read produced. Callers at :845-846 and in the directory walk (32-byte field), all fed from CDDA image data. (A distinct empty-field defect in the same function is listed under Candidates.)

## RE-SMS-063 — VERIFIED
Fixed-buffer `strcpy` of paths/names other code allows to be longer, sink by sink: `SMS_Player.c:1502-1504` (512-byte stack buffer vs `m_pPath` built under a 1024 cap, `SMS_Sounds.h:24`); `SMS_IOP.c:1871-1876` (`m_Path[124]`, `SMS_ioctl.h:74`; the GUI caller imposes no 124 bound) and `SMS_GUIFileMenu.c:1038` (`g_Config.m_Partition[256]`); `SMS_MC.c:145,172` (`m_Name[1024]` in the file-local struct, unsized source); `SMS_Config.c:575` (`s_pMC0SMC[128]`, callers assume ≤15 bytes, nothing enforces it). The cited in-tree bounded precedent (`SMS_ConfigAssetPath`, :259-276) exists. The finding's READ/INFERRED split is honest.

## RE-SMS-064 — VERIFIED (per module, not as a lump)
IOP init failures masked by unconditional success returns — each module checked individually: **USBD** (`hcd.c:340-341` AllocSysMemory result memset unchecked — a genuine NULL deref; :472 CreateThread tested but StartThread/initHubDriver unconditional; `usbd.c:263-265`; `driver.c:246-247`; contrast `hub.c:44-48` checks and its caller ignores the return). **SMSUMS** (`ums.c:125-133`, two mutexes + thread unchecked). **smbman-sms** (`smb_fio.c:228-244` unchecked; `smbman.c:32` `_start` returns RESIDENT_END unconditionally while `smb_initdev` genuinely can fail at :249-267). **SMSUdpfs** (`main.c:28-45` three init returns discarded; `udpfs_ioman.c:444` AddDrv result ignored, with an in-file comment at :103-105 documenting the hazard). **SMSMAP** (`main.c:338-341`). **SMSPWROFF** (`poweroff.c:166`). **SMSCDVD** (`SMSCDVD.c:1186` cache allocation never tested after AllocSysMemory, dereferenced by the directory code; contrast: the same `_start` *does* check CreateThread at :1224-1234). No module in the family propagates its init failure.

## RE-SMS-065 — VERIFIED
`mem_free` (`iop/SMSTCPIP/src/mem.c:92-113`) never checks the block was in use: it clears `used` (written but never read on this path) and runs `plug_holes`, so a double free re-runs the merge on stale offsets and corrupts the free list. The finding asserts no in-tree double free and labels reachability INFERRED — accurate. The one-line guard fix sits inside the existing interrupt-suspended section.

## RE-SMS-066 — VERIFIED
`ps2ip_getconfig` strcpy's an unbounded name into `char netif_name[4]` (`ps2ip.c:85-93`; `ps2ip_internal.h:22`). Reachability is narrower than a bare reading suggests but real: `netif_find` matches only name[0], name[1], name[2]−'0', so `"et0anything…"` passes the find and the strcpy then writes over the caller's ipaddr/netmask/gw. Second half confirmed: `netif_find` (`netif.c:116-131`) reads `name[2]` before any length validation, so a 0–2-char name is read past its end. Both functions are library exports (imports 20/21).

## RE-SMS-067 — VERIFIED
`err_tcp` (`api_msg.c:177-205`) is the only PCB callback that dereferences its netconn argument unchecked (`conn->pcb.tcp = NULL; conn->err = err;` then recvmbox/mbox/acceptmbox/sem) — every sibling checks (`recv_tcp`, `poll_tcp`, `sent_tcp`, `do_connected` :438-445). It is registered via `tcp_err` and invoked from abort/timeout paths — precisely the callback most likely to be handed a stale/NULL arg.

## RE-SMS-068 — VERIFIED
`ip_output_if` keeps its working IP-header pointer in a function-static (`ip.c:331-332`; `ip_id` legitimately needs static, `iphdr` does not). Both execution contexts are code-proven: the tcpip thread, and the driver thread — with QUEUE_INPUT 0, `ps2ip_input` runs in the SMAP receive context and its ICMP echo path (`icmp.c:76-119`) calls `ip_output_if` synchronously. No lock exists in `ip_output_if`. The interleave is INFERRED, as labelled; the shared-scratch mechanism is READ. Fix deletes one keyword.

## RE-SMS-069 — VERIFIED
`SMS_Realloc` (`src/SMS_EE.c:45-53`) publishes `*apSize = 17*aMinSize/16 + 32` *before* calling realloc and never rolls back; on failure the caller holds NULL + an inflated size and the old pointer is lost. AVI call sites do the dangerous thing (`SMS_ContainerAVI.c:169-176,281-288`: store NULL over the old table, write through `&NULL[m_nIdx]`). The safe in-tree pattern exists and is self-documenting (`SMS_ContainerMOV.c:919-929`, with the comment "SMS_Realloc grows by 17/16 and publishes its size even if realloc fails"). The `SMS_ReallocWithAlign` companion oddity (:55-89) is also as filed.

## RE-SMS-070 — WEAKENED
SMSMAP TX gather (`iop/SMSMAP/smap.c:886-921`): the only length gate is the 4096-byte FIFO budget (`m_TXFree = 4096`) while the gather buffer is the static TX buffer — constants re-derived: SMAP_TXMAXSIZE 1514 + tail pad 4 = 1518 requested, but the u32 array rounds the allocation to **1520 bytes** (2-byte drift from the filed 1518, immaterial). The bound mismatch is real in code. **Why weakened:** everything reaching `linkoutput` in this stack is a single Ethernet frame (≤1500 MTU + 14 = 1514), and `ip_output_if` fragments anything larger before the driver — no in-tree path hands `SMap_Send` a chain with `tot_len` in (1520, 4096]. The finding is titled and labelled INFERRED and concedes normal traffic cannot produce such a chain. True scope: a wrong-constant latent bound, incidentally safe today because of the MTU — not a demonstrated overflow. The one-constant fix is sound hardening.

## RE-SMS-071 — WEAKENED
SMSDev9 exported callback registration indexes fixed arrays unchecked (`iop/SMSDev9/src/ps2dev9.c:85-87,170-186`); `dev9DmaTransfer` (:286-313) masks `ctrl` for the hardware register but uses it **raw** at :301,309 for the callback arrays — an out-of-range `ctrl` reads and calls through an OOB slot. These are genuine library exports (`dev9.h:26-48`). **Why weakened:** every in-tree caller passes constants (intr 2–6, DMA channel 1), so — as the finding itself states — there is no live in-tree bug, only an unchecked exported interface. It does not present inference as fact (INFERRED in title and confidence). True scope: export-surface hardening gap, no demonstrated in-tree reachability.

## RE-SMS-072 — VERIFIED
`do_connect` (`api_msg.c:505-525`) discards connect results in both arms: UDP arm (:512-514) discards `udp_connect`'s return and posts success to the mbox unconditionally (its bind-failure return is real, `udp.c:662-670`); TCP arm (:517-519) discards `tcp_connect`'s return, sets no `conn->err`, and posts nothing — a synchronous failure (e.g. ERR_MEM from `tcp_enqueue`) leaves `netconn_connect` (`api_lib.c:287-316`) waiting until the SYN retransmission machinery aborts the PCB and `err_tcp` happens to post. The "hangs / duration INFERRED" framing is accurate, including its hedge.

---

## RE-SMS-073 … 096

## RE-SMS-073 — VERIFIED
Client netconns never get a semaphore but `netconn_write` waits on it: `netconn_new_with_proto_and_callback` sets `conn->sem = SYS_SEM_NULL` (`api_lib.c:146`) and no `sys_sem_new` exists anywhere in api_lib.c; SYS_SEM_NULL is 0 and never a valid semaphore (`sys_arch.h:10`, `ps2ip.c:429-440`); unguarded `WaitSema(conn->sem)` at both `netconn_write` sites (~:521/545), contrasted with the guarded wait in `netconn_close` (:573) and with accepted conns, which do get one (`api_msg.c:252`). `TCP_SND_BUF = 5,840` re-derived as filed. The runtime symptom is INFERRED, as labelled; the half-wired mechanism is code-proven.

## RE-SMS-074 — VERIFIED
`err_to_errno` macro (`sockets.c:75-91`): the 11-entry table (0…−10) is indexed under a `size_t` comparison, so every non-zero error converts to a huge unsigned and the test is false — **reproduced by compiling the exact macro**: 0→0, −1…−11→EIO in all cases. Latent second defect confirmed: `ERR_IF` is −11 but the table has no index 11, so fixing only the signedness creates a one-past read; the finding's fix (signed bound + table entry) addresses both.

## RE-SMS-075 — VERIFIED
`tcpip_apimsg` (`tcpip.c:110-122`) silently frees a dropped API message on `MEMP_TCPIP_MSG` exhaustion (pool 64, `lwipopts.h:55`); every api_lib.c caller then blocks on `sys_mbox_fetch` for a reply only the `do_*` handlers post — permanently, since the fetch has no timeout. Fix caveat (does not defeat the finding): the proposed unbounded retry-spin must yield/sleep or it can starve the tcpip thread on the single-core IOP; a bounded sleep-retry is the safer form (see Candidates).

## RE-SMS-076 — VERIFIED
`lwip_sendto` (`sockets.c:445-472`) dereferences `to` (~:458-459) with no NULL check and discards both `netconn_connect` results (:461, :468) and the `netconn_disconnect` (:470) — a failed first connect sends to the previous peer with a success-shaped return; a failed restore silently leaves the socket connected to the sendto target. This is the exported entry point itself; no caller-side guard exists.

## RE-SMS-077 — VERIFIED
`lwip_getpeername`/`lwip_getsockname` (~:827/:861) discard the `netconn_peer`/`netconn_addr` result and copy an uninitialised stack `naddr` to the caller as success; `netconn_addr` (`api_lib.c:236-260`) itself dereferences `conn->pcb.udp/tcp->local_ip` with no NULL-PCB check in either branch, while its sibling `netconn_peer` checks in both. A never-bound/connected socket crashes inside `netconn_addr`.

## RE-SMS-078 — VERIFIED
TCP ISNs predictable: `tcp.c:909-913` — `static u32_t iss = 6510; iss += tcp_ticks;` with no other entropy input; used at :506 and :822. Secondary claim exact: `ip_id` starts at 0 (`ip.c:332`). Minor imprecision, immaterial: the "500 ms tick" cadence wording doesn't match this port's timer plumbing (`tcp.c:106`); predictability (constant start + public uptime counter) is unaffected. Exploitability INFERRED, as labelled.

## RE-SMS-079 — VERIFIED
`tcp_keepalive` (`tcp_out.c` ~:539-579) never sets the TCP flags byte: no `TCPH_FLAGS_SET`/`TCPH_SET_FLAG` in the function (grep: only :219, :365, :492, all siblings); `TCPH_HDRLEN_SET` (`tcp.h:179`) *preserves* existing flags bits rather than clearing them, and the PBUF_RAM buffer comes from `mem_malloc`, which does not zero (`mem.c` zeroes the heap only in `mem_init`). A recycled keepalive header carries the previous packet's flags (SYN/RST/FIN possible).

## RE-SMS-080 — VERIFIED
`udp_send` leaks the chained header pbuf on the no-route return (`udp.c:458`, immediately after `ip_route(...) == NULL`, no free); the normal exit frees it (~:505-510). Correctly scoped to the chained case (`q != p`); when the header fits in-place nothing leaks.

## RE-SMS-081 — WEAKENED
`netconn_new_with_proto_and_callback` error path (`api_lib.c:138-164`) leaks the mbox it created — verified on two paths: `MEMP_API_MSG` allocation failure (:152) and `conn->err != ERR_OK` (:162); the mbox is a semaphore pair + AllocSysMemory block (`ps2ip.c:330-345`). **Defeated sub-claim:** the finding's orphaned-PCB half cannot occur — `do_newconn` (`api_msg.c:277`) sets `conn->err` only in the branches where `udp_new()`/`tcp_new()` returned NULL (pcb stays NULL), and `setup_tcp` on the success path never sets `err`; there is no path where a PCB exists *and* `err != ERR_OK` at the :162 check, so no registered PCB is orphaned. True scope: mbox leak on two paths; the fix should be scoped to `sys_mbox_free(conn->mbox)` — the DELCONN-posting half of the proposed fix is unnecessary.

## RE-SMS-082 — VERIFIED
Divide-by-file-controlled-zero in duration math: (1) `SMS_Container.c:239` `(m_Size * 8 * m_SampleRate) / m_BitRate` with `m_BitRate` = WAV `avg_bytes_per_sec * 8` (:341), so a 0 header value divides by zero; preconditions confirmed (ASF non-MP3 sets `m_FrameSize = 1`; no AVI code assigns `m_FrameSize`). (2) FLAC probe `SMS_ContainerFLAC.c:77` divides by the 20-bit STREAMINFO sample rate *before* the `if (apInfo->m_SampleRate)` acceptance test at :96. MIPS divide-by-zero impact INFERRED, as labelled; the missing guards are READ-proven.

## RE-SMS-083 — VERIFIED
A Vorbis file with ≥3 channels returns −1 from the OGG creator (`SMS_ContainerOGG.c:70`, returned verbatim by `SMS_GetContainerOGG`) and aborts the entire probe chain: the creator table order puts OGG at index 2, the loop treats −1 as terminal, and the tail destroys the container — the nine later creators never run for a positively-identified ≥3-channel Vorbis file. The M3U sub-probe disagreement is also confirmed. Returning 0 at the creator boundary matches the other eleven creators' contract.

## RE-SMS-084 — VERIFIED
DTS mono float-to-int converter is an empty stub (`src/SMS_DTS.c:11511-11513`, asm body is `jr $ra / nop`), dispatched via `s_FloatToInt[anChannels - 1]` (:11556-11565) per block; the output region comes from `SMS_RingBufferAlloc` (recycled, not zeroed) and `_dts_decode` returns a non-zero byte count for mono, so the player emits stale ring-buffer bytes at full frame size. The AC3 sibling is a full converter, confirming an unfinished stub.

## RE-SMS-085 — VERIFIED
`wma_init` divides by the container-supplied sample rate with no zero check (`src/SMS_WMA.c` ~:1247 store; divisions ~:1352, :1383, :1400); a zero rate also falls into the `<= 16000` branch and proceeds into the divisions. ASF reads the rate straight from the file with no zero validation. Container pass-through is INFERRED, as the finding labels it; the divisions and missing guard are READ-proven. (Distinct from RE-SMS-060 — different field/mechanism in the same function.)

## RE-SMS-086 — VERIFIED-DRIFT
AC3/DTS `Decode` writes an uninitialised local to the codec context on sync failure — **drift:** only `m_BitRate` is written from the uninitialised local (AC3 `SMS_AC3.c:2735`, DTS `SMS_DTS.c:11763`, both outside the if/else); `m_SampleRate` is correctly assigned inside the success branch in both files (:2720 / :11750). The title's "lBitRate/lSampleRate" overstates it by one field per decoder. Mechanism, locations and the two-line-move fix are otherwise exact.

## RE-SMS-087 — VERIFIED
AAC multichannel frames mixed through `pcm_synN` with phantom channel-0 copies for 3–4 channel content (`src/SMS_AAC.c:7925-7958, ~:8167-8243`): slots 3/4 of `internal_channel` are never written for such frames and remain 0, i.e. channel 0's buffer. The finding's INFERRED premise is in fact verifiable in-tree and true: `pcm_synN` (`src/SMS_Utils.S:556`) unconditionally loads all five channel pointers and mixes with fixed weights — no channel-count parameter. **Fix flagged:** the proposed "zero-fill unused slots" is unsafe as written — a NULL slot would be dereferenced by the asm; unused slots must point at a zeroed scratch buffer, or the real channel count must be threaded through.

## RE-SMS-088 — VERIFIED
DivX packed-B start-code scan never advances (`src/SMS_MPEG4.c:3362-3379`): the loop re-tests `SMS_unaligned32(lpBuf)` — the same four bytes at offset 0 — every iteration; `lCurPos` is used only as the loop's start value, never as a pointer offset. Unless the start code sits at offset 0 the stash never happens. One-token fix (`lpBuf + i`) is correct.

## RE-SMS-089 — VERIFIED
DXSB frames emitted with `m_EndPTS` never initialised: grep over `src/SMS_DXSB.c` shows `m_EndPTS` written nowhere except the use site, where `SMS_DXSB_Decode` (:317-318) rescales it from heap garbage (the frame block is `memalign`'d, uninitialised) and posts the frame. One-line fix (initialise from `m_StartPTS`) is sound.

## RE-SMS-090 — VERIFIED
DSP MDCT init (`src/SMS_DSP.c` `DSP_MDCTInit` :933-): `m_pCos = memalign(...)` (~:945) never checked, with `m_pSin`/`m_pTmp` derived by pointer arithmetic and written through — contrast `DSP_iFFTInit` (:879-895) in the same file, which does check. Second defect exact: `if (!DSP_iFFTInit(...) < 0)` (:972) is dead by operator precedence (`!x` is 0 or 1, never < 0), so FFT-init failure is reported as success.

## RE-SMS-091 — VERIFIED
`MC_SeekS` can return an uninitialised value (`src/SMS_MC.c:307-316`): `int retVal;` with `MC_Sync` (:75-92) writing `*apRes` only `if (s_LastCmd)`, and `MC_Seek` returning 0 without setting `s_LastCmd` when a command is in flight or the RPC fails. Sibling `MC_ReadS` (:265-274) starts `int retVal = 0;`. One-line fix identical to the sibling.

## RE-SMS-092 — VERIFIED
`SMS_SubContID` can never return FLAC (`src/SMS_FileDir.c:88-118`): in the `lLen > 5` branch it compares `stricmp(lpExt, ".flac")` where `lpExt` points at `"flac"` (no dot), so the comparison can never be equal. Both siblings (`SMS_ContID` :168, `SMS_FileID` :224) do `stricmp(--lpExt, ...)` first, shifting onto the dot. The missing `--` is exactly the defect.

## RE-SMS-093 — VERIFIED
`FindPath` computes a subdirectory's sector count from the root directory's size (`iop/SMSCDVD/SMSCDVD.c:794`): the quotient uses the subdirectory's `m_FileSize` but the round-up remainder tests `CDVolDesc.m_RootToc.tocSize`; the root initialisation at :674 uses the root size in both terms, proving a copy-paste slip. One-word fix.

## RE-SMS-094 — VERIFIED
UDPFS SMAP PHY-reset retry bound fires on the first poll (`iop/SMSUdpfs/smap/src/smap.c` `InitPHY` ~:137-147): `if (i <= 0)` inside the loop returns −1 on the first iteration if the RST bit hasn't already cleared, before a single `DelayThread` — the loop can never wait. The hardware-timing consequence is INFERRED, as labelled; the code defect is READ-proven (hence VERIFIED, not BLOCKED-ENV).

## RE-SMS-095 — VERIFIED
`g_ASCII` declared 4,572 in its consumer and defined 5,416 in its data file — script recount exact: all five definitions' declared bounds equal their element counts (5416/7736/7856/7732/7588); only the `g_ASCII` extern (`src/SMS_GS_2.c:35-39`) mismatches, by −844. The load-bearing path is confirmed: `s_FontSizes[0] = sizeof(g_ASCII)` in the consumer's TU yields 4,572, so 4,573–5,416-byte replacement fonts take the malloc path and are silently skipped on malloc failure. Direction is safe (understate), as filed.

## RE-SMS-096 — VERIFIED
`UnicodeToCP`'s binary search can never reach table index 0 — independently re-simulated: the verifier ported the search (`src/SMS_InverseCodePages.c:114-136`) line-for-line and ran it against all four script-extracted tables; exactly index 0 is unreachable in each (expected 129/152/129/129, all return '?'), and all other 508 mappings are correct. The claimed proof reproduces; the standard half-open fix preserves all currently-working mappings.

---

## RE-SMS-097 … 120

## RE-SMS-097 — VERIFIED
Icon loader hand-declares `lzma2_uncompress` with the wrong type (`src/SMS_GUIcons.c:34`: `extern int lzma2_uncompress(unsigned char*, unsigned int, unsigned char*, unsigned int)`) vs the real prototype (`include/lzma2.h:34`: `enum xz_ret …(…, size_t, …, size_t)`) and ignores its result at the call (:1406), slicing `lpBlob` unconditionally. Other consumers include the header and check the result (`SMS_IOP.c:240`).

## RE-SMS-098 — VERIFIED-DRIFT
`libds34bt.h`/`libds34usb.h` lack include guards (whole-`include/` grep: exactly these two of 89 headers) and use K&R `()` prototypes. **Drift:** only the **7 zero-argument** prototypes use `()` (bt 4, usb 3) — not "all prototypes"; the other 16 take real arguments. Line counts exact (35/21). The finding's own fix is already scoped to the zero-arg prototypes, so it stands unchanged.

## RE-SMS-099 — VERIFIED
Shared bit reader (`include/SMS_Bitio.h`, read in full) has no end-of-buffer check in any reader: `m_szInBits` occurs only at its declaration (:52) and setter (:145); no function compares `m_Idx` against it. All readers checked — `SMS_GetBits`, `SMS_GetSBits`, `SMS_GetBit`, `SMS_ShowBits`, `SMS_SkipBits`, plus `SMS_GetXBits` (:127), which the finding does not name but which also lacks a check (the "any reader" wording is under-scoped, not over-scoped). A few consumers check at call sites; the reader itself never does, as filed.

## RE-SMS-100 — WEAKENED
FLAC decoder trusts the channel count beyond its 2-entry synth table (`src/SMS_FLAC.c:437-444`, `_synth_indep[2]` indexed by `m_nChannels - 1` at :441; `_flac_init` :385 copies `FLACData` verbatim). **Narrowing:** the only in-tree path creating that `FLACData` is the container gate (`SMS_ContainerFLAC.c:114`, `< 3`), and `_decode_frame` (:322-326) additionally rejects channel-assignment mismatches — so with the shipped container the synth index can only be 0 or 1: a defence-in-depth note, not a live bug (the finding labels it INFERRED / "saved only by the container's gate"). **The claimed divide-by-zero is unreachable outright:** channels come from a 3-bit STREAMINFO field (`SMS_ContainerFLAC.c:66`, max 8), so `4096 >> nChannels` (`SMS_FLAC.c:523`) is ≥ 16, never 0 at "≥13". Only the synth-table sink survives, and only as a latent hazard if a future caller feeds the decoder a 3–8-channel `FLACData` directly.

## RE-SMS-101 — VERIFIED
`SMS_GetContainer`'s explicit-format path indexes the creator table with an unchecked caller value (`src/SMS_Container.c:198`: `s_CCreator[aPrefCont](retVal)`, no range test; the auto-probe path does bounds-check its hint at :152-155). Callers enumerated: `SMS_ContainerM3U.c:186,485` pass table-valid constants; `SMS_Player.c:1688` passes a negative sentinel taking the auto-probe path. No live trigger — latent trap exactly as filed.

## RE-SMS-102 — VERIFIED
The legacy SMSSMB tree is dead and internally duplicated: line counts recomputed exactly (SMSMB.c 2,002 + ps2smb.c 1,857 + smsutils.h 35 = 3,894); `ps2smb.c` is not built (the Makefile builds only SMSMB.o); `diff` = 600 lines as filed; no build path, workflow, or tool references the tree; no EE code loads its packed slot (`SMS_SMB_OFFSET` has no consumer outside `SMS_Data.h`). Internal-bug spot checks landed (FindFirst casts `SMSMB.c:1127/1131`; LOGOUT loop closing the same context repeatedly ~:1774). Deletion breaks nothing in-tree. (The packed SMB *bytes* in `SMS_Data.c` are a separate residue — see Candidates.)

## RE-SMS-103 — VERIFIED
Dead generated header `include/s_iop_image.h`: recomputed exactly 3,820 lines / 373,784 B, git-tracked, not generated by any build rule; its only references are two commented-out lines (`src/SMS_IOP.c:228,437`). Deletion is build-safe.

## RE-SMS-104 — VERIFIED
Non-site binaries in the GitHub Pages tree: sizes recomputed by content bytes — `docs/downloads/` 10,283,188 B (every per-file figure exact), `docs/investigations/` 1,920,177 B (handoff dir 1,628,644 B; its samples/ holds exactly 19 files, 901,501 B); total 12,203,365 B — the claimed ≈11.9 MB sits between the MiB/MB conventions and holds. `docs/README.md` states Pages publishes `master:/docs`; the site generator never emits these dirs. Caveat that does not refute: the downloads are deliberately linked from the generated releases page — served content by design — which is why the finding's fix (move + update links) is the right shape.

## RE-SMS-105 — VERIFIED
AC3 `s_Samples` workspace: `static sample_t s_Samples[16*256*12]` = 192 KiB BSS (`src/SMS_AC3.c:323`, exact). Highest used index independently re-derived: 256 + 1536 + 1024 + 255 = **3,071** (~12 KB as filed). The proposed 4,096-float (16 KB) shrink keeps ~33% headroom. Minor omission in the finding: the size-agnostic `memset` at :360 is not listed; it shrinks with the declaration.

## RE-SMS-106 — VERIFIED
Default (BDM) build carries unreachable `g_DataBuffer` segments: aligned sizes recomputed exactly (UMS 8,512 + USBD 9,152 + SIO2MAN 5,824 = **23,488 B**; full chain `SMS_DATA_BUFFER_SIZE` = 266,560). The three offsets are consumed only in `#else`/non-BDM branches (`SMS_IOP.c:504,674,688`); the default build is BDM (`Makefile:3`, `BDM ?= 1`) and loads the `.irx.xz` blobs instead. The fix preserves the BDM=0 flavour — LESS-compliant.

## RE-SMS-107 — VERIFIED
Ten defined-but-never-called EE functions (~200 lines): whole-tree word-boundary census for each name returns only declaration + definition (+ handoff-note mentions) — no caller, no pointer-table entry. `IPU_UnpackImage` is exactly 105 lines (`SMS_IPU_1.c:220-324`); summed spans for all ten = 217 lines vs the claimed ≈200. Adversarial check on the tenth: `ReadCombinedPadStatus_raw` is genuinely dead — the live `PAD_Read` macro targets the *singular* `ReadPadStatus_raw`.

## RE-SMS-108 — VERIFIED-DRIFT
Thirteen never-called DS34 RPC client stubs: the 13-name list is exact (bt 8, usb 5), each returning exactly 2 word-boundary hits (declaration + definition). **Drift:** the fix's "keeping the five used entry points" undercounts — there are **9 concrete live functions** (bt: init/get_status/get_data/get_bdaddr; usb: those four + set_bdaddr), called from `SMS_PAD.c` and `SMS_GUIMenuSMS.c:1659-1661`. Deleting exactly the 13 listed stubs is safe; an implementer who literally kept only five functions would break the build.

## RE-SMS-109 — VERIFIED-DRIFT
Unreferenced `utils/` directory: absence verified (no reference from docs, docs-site, README, Makefile, tools, or workflows; all 4 files git-tracked). **Drift:** content bytes are 45,056 + 746 + 580 + 952 = **47,334 B ≈ 46 KiB**, not the claimed 56 KB. The deletion case is unaffected.

## RE-SMS-110 — VERIFIED-DRIFT
Orphan blob `irx/bdmfs_vfat.irx.xz`: unreferenced and unembedded confirmed (zero matches in src/include/tools/.github/docs-site; `IOP_OBJS` embeds `bdmfs_fatfs_irx.o`, not vfat). **Drift:** the claim "the only `.irx.xz` with no plain `.irx` counterpart" is false — ds34bt/ds34usb/libsd/smbman `.irx.xz` also lack plain counterparts. True distinguishing scope: it is the only `.xz` blob simultaneously counterpart-less, **not in IOP_OBJS**, and unreferenced by any tool.

## RE-SMS-111 — VERIFIED
`images/` editor backup and two large source PNGs: sizes exact (25,390 B; 230,952 B; 232,705 B; PNGs sum 463,657 B as filed). The build embeds exactly three JPEGs via the bin2c rule; repo-wide grep for all three filenames returns zero references. The "master artwork" characterisation is INFERRED in the finding and stays that way.

## RE-SMS-112 — VERIFIED
Turkish font pack ships the same font twice: md5 recomputed — `latin1.mtf` and `WinLatin1.mtf` both `621f497d…`, both 8,328 B, byte-identical. The pack's `ascii.mtf` is distinct, and the sibling-naming argument for keeping `latin1.mtf` holds.

## RE-SMS-113 — VERIFIED
Dead code shipped in SMSTCPIP, every sub-item re-derived: uncompiled `loopif.c` (121 lines, absent from IOP_OBJS, under `#if LWIP_HAVE_LOOPIF` = 0); `udp_lookup`/`ip_lookup` debug-only and callerless, both exits returning 1; the `QUEUE_INPUT 0` compiled-out input queue (`ps2ip.c:40,194-256`); `ip_forward`/`icmp_time_exceeded` under `#if IP_FORWARD` = 0. Summed dead span ≈430 lines vs the claimed ~450 — holds.

## RE-SMS-114 — WEAKENED
SO_REUSE port-sharing machinery is compiled in with no in-tree user — premise verified (`SO_REUSE 1` at `opt.h:338-340`; reuse loops/branches in tcp_in.c, udp.c, tcp_bind, udp_bind; nothing in-tree sets SO_REUSEADDR/PORT or deliberately double-binds). **Defeating evidence for the fix:** every guard is `#ifdef SO_REUSE` / `#ifndef SO_REUSE` (enumerated across sockets.c, tcp.c, tcp_in.c, udp.c) — setting `SO_REUSE` to **0** leaves the macro *defined*, so all `#ifdef` branches remain compiled and the `#ifndef` path in tcp_bind stays unreachable. As written, the fix deletes nothing. True scope: compiled-in machinery with no in-tree user, whose removal requires leaving the macro undefined/`#undef`'d or converting the guards to `#if` — guard surgery, not a value flip.

## RE-SMS-115 — VERIFIED
memp reserves a RAW-PCB pool for a protocol with no implementation file: `memp.c` includes `sizeof(struct raw_pcb)` (:55), `MEMP_NUM_RAW_PCB` (:68, = 4 at `opt.h:78-79`) in sizing (:81); no `raw.c` exists; `LWIP_RAW` is never defined anywhere, so both `#if LWIP_RAW` sites compile out. Fix-completeness note: deleting `raw.h` also requires dropping its three includes (see Candidates).

## RE-SMS-116 — VERIFIED-DRIFT
Half-finished API surface: six api.h functions have no implementation (`netbuf_alloc`, `netbuf_free`, `netbuf_chain`, `netbuf_data`, `netbuf_next`, `netconn_err` — grep over SMSTCPIP/src returns no definitions; nothing calls them), and `ps2ip_ShutDown()` (`ps2ip.c:265-269`) is `{ return 1; }`. **Drift:** the export slot numbers mix bases — enumerating `exports.tab` 0-based from `_start` gives ShutDown at **2** and the stub slots at **1, 3, 32, 37** (the finding's 3 / 2,4,32,37 mixes 1-based and 0-based). Substance is correct.

## RE-SMS-117 — VERIFIED
Six declaration-only prototypes with no definition anywhere — each name is the sole occurrence in the whole tree (word-boundary grep). Includes the rename trap exactly as filed: `SMS_SetSifCmdHandler` (`SMS.h:306`) vs the real `SMS_IOPSetSifCmdHandler` (declared `SMS_IOP.h:76`, defined `SMS_IOP.c:192`, live callers). Deleting a declaration with no definition cannot break a working call.

## RE-SMS-118 — VERIFIED
Dead H.263 inter-unquantize path: `SMS_H263_DCTUnquantizeInter` body is entirely `#if 0` (`SMS_MPEG.c:904-935`) and references undeclared `aQScale`/`aN` — it cannot compile if re-enabled. Wired but never invoked: grep returns only the two assignments, the member, and the prototype — zero invocations through the pointer; the inter path at :1153 calls the MPEG2 function directly. Mechanical note: if the member is deleted, the MPEG2 assignment at :210 must also be removed.

## RE-SMS-119 — VERIFIED
ASF stream-bitrate table parsed into one array and applied from a different, never-written one: local `lBitRate[128]` (`SMS_ContainerASF.c:140`, zeroed :149, never written again); the handler writes `lpASF->stream_bitrates[…]` (:322-330, the field's only other occurrence — written, read nowhere); the final loop applies the local (:346-352), writing 0 over 0. The loop index matches the handler's keying, so the one-token fix is index-correct. 512 B of stack confirmed.

## RE-SMS-120 — VERIFIED
ASF index support is scaffolding for a feature never built: `ASFIndex` struct (:82-85), `index_read` (:96), `index_ptr` (:116) occur only at their declarations — never allocated, read, or tested; `SMS_GetContainerASF` sets no `Seek` and never sets `SMS_CONT_FLAGS_SEEKABLE` (contrast AVI/MOV/MPEG-PS, which do). Not a duplicate of RE-SMS-119 (mis-wired live data path vs never-populated seek state — different mechanisms, different fixes).

---

## RE-SMS-121 … 144

## RE-SMS-121 — VERIFIED-DRIFT
Write-only state: AVI `m_Idx` (`SMS_ContainerAVI.c:43`, sole write :109, no read anywhere); ASF header `create_time`/`play_time`/`ignore` (:71,72,75; writes :182,185,187; the only fields ever read are flags/preroll/send_time/min_pktsize); ASF `seq` written at :538 and never read. **Drift:** the `seq` declaration is at **:59**, not :64 (:64 is `packet_pos`). The fix must keep the `File_Get*` calls as cursor-advancing skip-reads, as filed.

## RE-SMS-122 — VERIFIED
DTS file-scope sample-rate/bit-rate tables are dead duplicates of the live copies inside `_dts_syncinfo`: the file-scope names occur only at their declarations (:127, :134); script extraction shows all four initialiser lists value-identical (16 sample rates, 32 bit rates). 48 ints = 192 bytes as filed. Deletion is safe.

## RE-SMS-123 — VERIFIED
AC3 IMDCT `aBias` parameter threaded through every call but never used: script count of `aBias` inside `_ac3_imdct_512`/`_ac3_imdct_256` (`SMS_AC3_imdct.c:533,579`) = 1 each — the signature only. Callers apply bias directly (`SMS_AC3.c:2440,2426`). Dropping the parameter touches no behaviour; the `m_Bias` field correctly stays.

## RE-SMS-124 — VERIFIED
AC3 `s_SAB` declared `[20]` with only 8 entries initialised (script count = 8 exactly, `SMS_AC3.c:318-321`); its sole consumer (`:1697`, `_ac3_mix32toS` asm) does exactly two `lq` loads covering entries 0–7 — all 8 initialised entries, carrying the 2 distinct values the finding describes. `[20]`→`[8]` preserves both loads; saving 48 bytes as filed. (Caution: shrinking below [8] would break the `lq` at offset 16.)

## RE-SMS-125 — VERIFIED
`lzma2.c` exports dead, collision-prone `encode`/`decode` (`src/lzma2.c:10-26,29-49`): whole-tree word grep — `encode` occurs only at its definition (zero callers); `decode` only at its definition and its 3 internal calls in `lzma2_decode_size`; neither is in `include/lzma2.h`. Fix accounting checks out (−19 lines deleting `encode`; making `decode` static removes the second export).

## RE-SMS-126 — VERIFIED
Generated icon header and generator still say "11 dev": script counts from the bake tool give DEV = **13** names; the consumer agrees (`SMS_GUIcons.c:1372` array [13], :1435 loop to 13). Arithmetic re-derived: 9×4096 + 12×4096 + 13×9216 = 205,824 = `g_RGBA_RawSize` (`SMS_IconsRGBA.c:1748`); with 11 dev icons it would be 187,392. Comment-only fix in template + generated file is correct.

## RE-SMS-127 — VERIFIED
`src/SMS_Menu.c` is a dead duplicate stub: 7 lines total, `GUI_CreateMenu(void) { return 0; }`; the real implementation is `SMS_GUIMenu.c:403-414`, the Makefile's EE_OBJS contains `SMS_GUIMenu.o` and no `SMS_Menu.o`, and all live callers resolve to the GUIMenu version. Deleting the stub removes a symbol-collision trap at zero cost.

## RE-SMS-128 — VERIFIED
USBD exports three stubs that report success (`iop/USBD/src/interface.c:216-229`: `UsbRegisterAutoloader`, `UsbUnregisterAutoloader`, `UsbChangeThreadPriority` — each body is a debug print + `return 0`). They are genuinely exported (exports.tab slots 14, 15, 16; declared `usbd.h:249-251`), not internal dead code. Returning a negative error is the LESS fix as filed.

## RE-SMS-129 — VERIFIED
File mapping is half-finished: `SMS_FileMappingMap` returns the same fixed virtual address 0x32000000 for every map (`src/SMS_FileMapping.c:270-272`, with the "implement VAddr allocation here" comment); the `calloc` result (:276) and `memalign` result (:294) are dereferenced unchecked; `SMS_FileMappingUnMap` head removal (:315-316) never clears the new head's `m_pPrev`, which still points at the node freed at :334. Sole consumer confirmed: `SMS_JPEG.c:1733,1749`. Fix option (a) (single-mapping + checks + head fix) is the LESS path.

## RE-SMS-130 — VERIFIED
`FATFS_Init` leaks its filesystem allocation on two early returns (`iop/SMSUMS/src/ums_fat.c:1043,1044-1046` — both return while `retVal` is still 0, skipping the cleanup at :1131 that the BPB-whitelist path does reach). Routing both returns to :1131 is exactly the filed fix. (The pre-allocation return at :996 leaks nothing.)

## RE-SMS-131 — WEAKENED
MOV carries parallel `hdlr`/`stsd`/`esds` parser copies that have drifted — **core claim stands:** three `hdlr` bodies (`SMS_ContainerMOV.c:275-295,297-320,705-720`), two `stsd` (:322-382 demuxer, :722-766 probe), two `esds` (:429-469, :768-809), storing into a real `SMS_Stream` vs a bare `SMS_AudioInfo`; real drift exists (esds bounds strategy — explicit test + `return -1` at probe :784-785 vs the `_tbl_fits` retrofit at demuxer :460; probe-only AAC AudioSpecificConfig extraction :789-808; three genuinely different `hdlr` stream dispositions). **Why weakened:** (1) one filed drift example is false — "the demuxer's stsd walks child atoms, the probe's doesn't" is refuted by the code: probe `_mov_read_stsd_m4ap` at :758-765 contains the identical `_mov_read_dflt` child-walk (demuxer equivalent :375-381); both walk children. (2) Two cited ranges are inflated: demuxer `stsd` is :322-382, not :322-427 (:384-427 are shared `_mp4_read_descr*` helpers used by both copies), and `esds` is :429-469, not :429-485. True scope: duplication + drift are real, but the stsd copies are *more* alike than filed — the ~100-line saving is an upper bound, and any consolidation must preserve the probe's existing child-walk.

## RE-SMS-132 — VERIFIED
Whole-file asset loading triplicated (skin) and triplicated again (language): `DrawSkin` (`SMS_GUIDesktop.c:155-317`) contains three open→seek-end→capped-malloc→read→verify blocks; `SMS_LocaleInit` (`SMS_Locale.c:813-`) repeats the same three-way split. Divergence as filed: the locale blocks have **no size cap and discard the read result** (`fioRead` return ignored at :844/:877; `0xFFFF0` occurs only in SMS_GUIDesktop.c). (Nuance: BDM/non-BDM are `#ifdef` alternatives — two of three blocks compile per build; the triplication is at source level, where the fix-drift happens.) Per-file helper is a net deletion in both files.

## RE-SMS-133 — VERIFIED
The PCB-creation switch is triplicated across do_newconn/do_bind/do_connect (`api_msg.c` — script census: `udp_new()` ×9, `tcp_new()` ×3 in the one file) and the copies have diverged: do_newconn NULL-checks every allocation (:293-330), do_connect NULL-checks every allocation (:461-500), **do_bind checks none** (:393-410) — exactly the RE-SMS-042 hole, produced by the triplication. One `create_pcb()` helper (~35 lines replacing ~129) is a genuine net deletion of roughly the filed ~60 lines.

## RE-SMS-134 — VERIFIED-DRIFT
Eleven one-declaration `SMS_Container*.h` stub headers: all eleven exist with exactly the filed line counts (323 lines total), each `#include`s `SMS_Container.h`, included from 16 distinct source files. **Drift:** script extraction finds **19 prototypes, not ≈15** (MOV carries 3, not 1–2); only **31** include sites pull one of the eleven stubs, not 48 (48 conflates central-header includes). **Fix incompleteness:** two stubs are not prototype-only — `SMS_ContainerJPG.h:18-25` defines `struct SMS_ContainerJPEG` (used in SMS_ContainerJPG.c and SMS_JPEG.c:1836) and `SMS_ContainerM3U.h:18-24` defines the seven `SMS_SUBCONTAINER_*` macros (used across SMS_ContainerM3U.c and SMS_FileDir.c) — both must move to `SMS_Container.h` too or the filed deletion breaks the build. With those corrections the consolidation is still a net deletion (−11 files, ~300 lines).

## RE-SMS-135 — VERIFIED
MOV/M4A creators select their parser by mutating a shared global parse table at runtime: `s_MOVParseTbl[0].m_Func` written at `SMS_ContainerMOV.c:1529` (MOV) and :1544 (M4A), both then calling `_GetContainerMOV(apCont, s_MOVParseTbl)` — which already takes the table as a parameter, and the separate-table pattern already exists in-file (`s_M4AProbeTbl` :838-851). The array is non-const precisely because of the two writes. **Fix-sanity flag:** the filed fix *adds more than it removes* (+~25 lines for a second table vs −2 write lines) — justified only by const/ROM placement and order-independence, not as a deletion.

## RE-SMS-136 — VERIFIED-DRIFT
Redundant explicit object rules in the root Makefile: six explicit rules restate the generic rule (four MPEG rules add only a header prerequisite; the two GS rules add nothing). **Drift:** the generic pattern rule is at **Makefile:110-111**, not :125-126 (:125-126 is the libmpeg grouped-prerequisite rule the fix should mirror); the explicit rules span **:113-132**, not :128-147. Net saving with corrected ranges is still ≈20 lines as filed.

## RE-SMS-137 — VERIFIED
`dev9.h` duplicated verbatim in two IOP trees: `diff` clean, md5 identical (`01ad1498…`), 53 lines each. The load-bearing caveat is real: `tools/build_udpfs.sh:12-22` documents the deliberate SDK-header shadowing (dev9 v1.1 ABI vs the v1.9 ordinal trap). Comment-in-each-copy is the correct LESS fix; deleting a copy would break the include-order trick.

## RE-SMS-138 — VERIFIED
`AGENTS.md`/`CLAUDE.md` byte-identical: `diff` clean, md5 identical (`f93ecc98…`), 1,254 bytes each, both the GitHub Attribution Policy. Pointer-file fix is sound.

## RE-SMS-139 — VERIFIED
IPU suspend/resume duplicated verbatim as "dummy" variants (`src/SMS_IPU.c`): body extraction shows `IPU_DummySuspend` (:605) identical to `IPU_Suspend` (:584) and `IPU_DummyResume` (:611) identical to `IPU_Resume` (:590), including the stale `/* end IPU_Suspend */` comment at :609. The dummy slots are live (:844-845). Pointing them at the real functions is behaviour-identical.

## RE-SMS-140 — WEAKENED
MOV index builder resets every stream's chunk cursor per chunk (`src/SMS_ContainerMOV.c` `_mov_build_index` :936-1057; reset loop :1034-1037, brace-verified inside the derived-size branch) — **the quadratic behaviour is real:** each chunk rescans from 0, O(chunks² × streams) on the derived-size audio path, and the in-search cursor writes are pure waste. **Why weakened — the filed fix ("delete the reset loop") is unsafe as stated:** (1) `_mov_read_header` calls `_mov_build_index` once per stream (:1124) and there is *no* per-call `m_NextChunk` initialisation (the only writes in the tree are :1023 and :1036; streams start zeroed only by calloc) — today the per-chunk reset is what guarantees all cursors are 0 between calls. Deleting it leaves stream 2's search starting mid-table, producing oversized merged chunk sizes. The fix must move the reset to per-build (still a net deletion). (2) Monotonicity is assumed, not enforced: nothing sorts or validates `m_pChunkOffsets` (read raw at :614-619); the linear version is correct only for offset-ordered tables — the normal ISO-BMFF layout, but it must be stated as the fix's precondition. True scope: quadratic behaviour real; remedy is "move the reset from per-chunk to per-build (and document the offset-order assumption)", not a pure deletion.

## RE-SMS-141 — VERIFIED
`smb_GetShareList` spends one full NetShareEnum RPC per share (`smb_fio.c:991-1046`: count call at :1016, then one call per share at :1024-1035). Each call is a full transaction that parses **all** entries in the reply and copies only the requested window (`smb.c:684,~792-802`) — the batched data already arrives in the first reply and is discarded n times. n+1 round trips as filed; the bounded-array fetch uses the existing window parameters, no protocol change.

## RE-SMS-142 — VERIFIED-DRIFT
Hand-maintained host-test list in CI: the step (`.github/workflows/compilation.yml:61-120`) is a hand-enumerated `run:` list with no discovery; nothing fails if a new `tools/test_*.py` is unreferenced. Script census: 59 `tools/test_*.py` files, all 59 referenced across workflows (complete, as filed). **Drift:** the step's list is **58 lines / 57 test invocations**, not ≈64 (lines 63–120 = 57 tests + `check_devmenu.py`); benefit ≈58 YAML lines, consistent with the filed "≈60 deleted". **Fix caveat:** a naive glob loop would also pull `test_converter_publication.py` into the EE workflow and run `test_rolling_release.py` twice (it has its own step at :58-59) — the loop must exclude, or the separate steps be folded in deliberately.

## RE-SMS-143 — VERIFIED
Stale `src/lzma2` references: `.gitignore:10-11` and `Makefile:13` (`-L$(EE_SRC_DIR)/lzma2`) point at a directory that does not exist; the sources are compiled directly (`Makefile:45`: lzma2.o, xz_crc32.o, xz_dec_lzma2.o, xz_dec_stream.o) and no `libxz.a` exists in the tree. Both references are inert; deleting them changes no link result.

## RE-SMS-144 — VERIFIED-DRIFT
`test-media.yml` regeneration is not path-gated: the workflow triggers on push to master with **no `paths:` filter** (present in docs.yml/wiki.yml, absent here), then regenerates media with ffmpeg (:37) and uploads with `--clobber`; `workflow_dispatch` is present, so the escape hatch for a filtered version already exists. **Drift:** the trigger block is at **:9-12**, not :12-15 (:14-15 are the `permissions:` block). The cost claim remains INFERRED from the YAML — no Actions-log measurement was made by the finding or by this verification.

---

## Fix-sanity / LESS-NOT-MORE flags (mechanism verified; fix needs correction)

- **RE-SMS-014** — filed clamp 1..17 is wrong; `g_Palette[16]` makes the valid range **1..16**. Applying the filed fix would still permit a one-past read.
- **RE-SMS-033** — fix must be sized for the FullGain path (worst index 438), not just the direct path (382).
- **RE-SMS-075** — unbounded retry-spin could starve the tcpip thread on the single-core IOP; use a bounded sleep-retry.
- **RE-SMS-087** — "zero-fill unused slots" is unsafe as written (NULL slot dereferenced by the asm); use a silent scratch buffer or thread the real channel count.
- **RE-SMS-108** — "keep the five used entry points" undercounts: 9 concrete live functions must survive.
- **RE-SMS-114** — filed value-flip fix deletes nothing (all guards are `#ifdef`); requires `#undef`/guard conversion.
- **RE-SMS-115** — deletion fix is build-incomplete: `raw.h` is included by `ip.c:52`, `memp.c:37`, `api.h:41` (plus the api.h:86 member); those includes must be dropped too.
- **RE-SMS-118** — if the member is deleted, the MPEG2 assignment at `SMS_MPEG.c:210` must also be removed.
- **RE-SMS-131** — the ~100-line saving is an upper bound; consolidation must preserve the probe stsd's existing child-walk.
- **RE-SMS-134** — filed fix omits the JPG struct and M3U macros; both must move to `SMS_Container.h` or the build breaks.
- **RE-SMS-135** — fix adds more than it removes (+~25 lines vs −2); justified only by const-ness/mechanism removal, not as a deletion.
- **RE-SMS-140** — filed pure deletion is unsafe; move the cursor reset to per-build.
- **RE-SMS-142** — naive glob loop duplicates two test runs; exclude or fold in the separate steps deliberately.

## Mechanism-level duplicates

None across the 144 findings. Pairs explicitly examined and cleared by body: 003/016 (same function, distinct mechanisms), 005/010, 025/046 (different call paths despite both involving `err_tcp`), 034–040 (distinct subsystems), 041–048 (eight distinct functions/triggers/guards — not one mechanism split), 055/069 (different allocators), 060/085 (same function, different fields), 063/066 (same verb, different destinations/contracts), 073/075, 076/077, 082/085, 084/086, 098/108, 102/106 (disjoint byte sets), 113/114/115/116 (four distinct SMSTCPIP mechanisms), 119/120, 131/135 (related — 135's table mutation forces 131's second `hdlr` body — but independently fixable and best landed together), 137/138 (unrelated byte-identity pairs). RE-SMS-133's divergence *is* RE-SMS-042 by the finding's own cross-reference — noted there, not double-counted.

## Candidates (new defects noticed while verifying — not findings)

1. **DTS downmix row-index overflow** (companion to RE-SMS-010, worse than its stated sink): `src/SMS_DTS.c:10261` — `table[flags & DTS_CHANNEL_MASK][input]` row index can reach 63 into an [11] table via the `_dts_decode` else-branch (:11567-11571).
2. **UDFDecode read-side defects** (companion to RE-SMS-017): `iop/SMSCDVD/SMSCDVD_UDFS.c:99` 16-bit read one element past `aLen`; :399 OOB source via `38 + lLIU` (disc u16) into a 4096-byte `s_ScanBuf`.
3. **WMA v1 exponent rows 1+ never filled**: `src/SMS_WMA.c:1354` always writes row 0 while `decode_exp_vlc` selects the row by block size (:1635) — zero rows trigger RE-SMS-024's underflow without a repeated critical frequency.
4. **FAT LFN negative offset** (strengthens RE-SMS-004): `iop/SMSUMS/src/ums_fat.c:421` — sequence byte 0 gives `lOff = −13`, a write before `m_Name`.
5. **ASFContext calloc unchecked**: `src/SMS_ContainerASF.c:157-159` — dereferenced by memset on the next line; same family as RE-SMS-039 but not cited there.
6. **Dead defensive check in netconn_recv TCP arm**: `iop/SMSTCPIP/src/api_lib.c:444-446` — `buf` already dereferenced at :431-434 and NULL-checked at :410; deletion-only, harmless.
7. **CDDA empty-field uninitialised name** (distinct from RE-SMS-062): `src/SMS_FileContext.c:803-822` — on an empty field `*apBuf` is never assigned; per-node `m_pName` is uninitialised malloc memory.
8. **pbuf_header REF/ROM mirror underflow** (companion to RE-SMS-049): `iop/SMSTCPIP/src/pbuf.c:306-307` — `header_size - p->len <= 0` passes for almost any negative size, after which the shared `len += header_size` still wraps.
9. **lzma2_decode_size counter type mismatch** (companion to RE-SMS-053): `src/lzma2.c:51-82` — `int i` looped against an `unsigned long records` from the file.
10. **getpeername/getsockname NULL namelen deref** (independent of RE-SMS-077): `iop/SMSTCPIP/src/sockets.c` ~:815-875 — both dereference `*namelen` with no NULL check.
11. **netconn_addr missing default**: `iop/SMSTCPIP/src/api_lib.c:236-260` — returns ERR_OK with `*addr`/`*port` unset for non-UDP/TCP conn types.
12. **Packed SMSSMB.IRX segment is dead bytes in every build**: `src/SMS_Data.c` / `include/SMS_Data.h:24,42` — 19,641 B (19,648 aligned) in `g_DataBuffer` with no EE consumer; counted by neither RE-SMS-102 nor RE-SMS-106 (~19.6 KB resident EE RAM).
13. **Second orphan IRX pair of the RE-SMS-110 class**: `irx/mmcedrv.irx` + `irx/mmcedrv.irx.xz` — not in IOP_OBJS; only mention is a comment at `src/SMS_IOP.c:1023` saying mmcedrv is not required.
14. **Explicit-format path discards the creator's return**: `src/SMS_Container.c:198` does not assign `lSts`, so the `lSts < 0` teardown can never fire for explicit probes (distinct from RE-SMS-101's bounds claim).
15. **FLAC include-guard typo**: `include/SMS_ContainerFLAC.h:11-12,28` — guard spelled `__SMS_CountainerFLAC_H`; internally consistent so it works, but fragile.
16. **MOV cursor cross-call invariant**: `src/SMS_ContainerMOV.c` — `m_NextChunk` correctness between per-stream `_mov_build_index` calls depends entirely on the per-chunk reset at :1034-1037 executing in the prior call; a per-build initialisation at entry (:936) would make the invariant explicit regardless of the RE-SMS-140 decision.

## Closing tally

| Verdict | Count | Findings |
|---|---|---|
| VERIFIED | **110** | 002–004, 006–008, 012, 015, 016, 018–021, 024, 026–032, 034, 035, 037–045, 047–059, 062–069, 072–080, 082–085, 087–095, 096, 097, 099, 101–107, 111–113, 115, 117–120, 122–130, 132, 133, 135, 137–139, 141, 143 |
| VERIFIED-DRIFT | **20** | 005, 009, 010, 011, 013, 014, 033, 036, 086, 098, 108, 109, 110, 116, 121, 134, 136, 142, 144 |
| WEAKENED | **13** | 017, 022, 025, 046, 060, 061, 070, 071, 081, 100, 114, 131, 140 |
| REFUTED | **1** | 001 |
| BLOCKED-ENV | **0** | — |
| **Total** | **144** | RE-SMS-001 … RE-SMS-144 |

Headline: 130 of 144 findings (90%) stand as filed or with corrected figures; 13 are real but narrower than claimed; exactly one — RE-SMS-001, the report's #1 finding — is refuted outright, defeated by the unsigned conversion in the very guard the finding paraphrased as signed. No finding was left unsettled for environment reasons. The clone was left clean and unmodified at the pin.
