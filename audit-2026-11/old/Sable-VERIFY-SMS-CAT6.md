# Sable — Verification Pass: SMS Category 6 (adversarial re-verification)

**Scope:** every SMS finding listed in §3(b) of `Sable-MASTER-INDEX.md` — the original Cat-6 top-tier rows (SMS-015, SMS-052, SMS-053, SMS-057, SMS-060) plus the Rounds 7–9 extension SMS rows (SMS-103…155, SMS-158…171, SMS-179, SMS-180, SMS-182). **75 findings.** (SMS-037 appears in §3(b) only in the excluded-by-filter note as Cat 4 — not a Cat-6 row, out of scope.)

**Pin:** Simple-Media-System `e65ced39a27eee8d996e9257e21594d1cac1c93d` — clone HEAD verified at start and end by every pass; read-only held throughout; clone git-clean at end.

**Method:** per finding — (a) cited locations re-opened in the clone, claimed vs actual lines recorded; (b) mechanism re-derived from code, with independent arithmetic (field widths, array sizes, index maxima) and script recounts for numeric claims; (c) reachability traced from real input paths, with an active hunt for defeating guards elsewhere. Verdicts: **VERIFIED** / **VERIFIED-DRIFT** (holds, corrected lines given) / **WEAKENED** (real but narrower — conditions stated) / **REFUTED** (defeating evidence at path:line). Full per-finding reasoning for all 75 lives in `verify-sections/verify-sms-{A,B,C,D}.md`; the reasoning for every non-VERIFIED finding is reproduced in full below.

---

## Summary counts

| Verdict | Count | IDs |
|---|---|---|
| VERIFIED | 50 | SMS-052, 053, 057, 060, 103, 104, 105, 107, 109, 110, 112, 113, 114, 119, 120, 122, 123, 125, 126, 129, 130, 131, 132, 137, 138, 139, 141, 143, 144, 145, 146, 149, 150, 152, 154, 155, 158, 159, 160, 162, 163, 165, 166, 167, 168, 169, 170, 171, 179, 182 |
| VERIFIED-DRIFT | 15 | SMS-106, 108, 111, 118, 121, 124, 127, 128, 134, 135, 136, 147, 148, 161, 180 |
| WEAKENED | 9 | SMS-115, 116, 117, 133, 140, 142, 151, 153, 164 |
| REFUTED | 1 | SMS-015 |
| **Total** | **75** | |

**Headline outcomes:**

- **SMS-015 is REFUTED.** A pass-1 Top-25 finding does not hold at the pin: the bound it proposed (`_tbl_fits`, `src/SMS_ContainerMOV.c:143-146`) already runs before every cited malloc, and atom sizes are bounded by the parent chain rooted at the real file size. Worst-case allocation is ~1× file size. The original read quoted the malloc lines without the guard calls on the immediately preceding lines. Parent spot-check independently confirmed the guard and all six call sites.
- **SMS-115 / SMS-116 are WEAKENED.** The sinks are exact, but the round-7 headline "a valid 5.1 stream is the trigger" is defeated in the shipped Player flow: `_check_ac3` (`src/SMS_Player.c:98-107`) clamps channels to 2 in non-SPDIF mode, and SPDIF mode routes to `_ac3_spdif`/`_dts_spdif` instead of the vulnerable decode functions (`src/SMS_AC3.c:347-349`). Parent spot-check confirmed both sites. Surviving triggers: non-SPDIF playback where the container leaves `m_Channels == 0` so the codec self-fills 5/6 from the frame header, or a non-Player codec caller.
- **SMS-161 is VERIFIED-DRIFT on wording, exact on substance** — confirmed by two independent script recounts (worker's and parent's): the DTS tables are monotonically *non-decreasing* (early duplicate steps), not strictly increasing; entry counts (64/128), the zero tails (quant6[63], quant7[125..127]) and the shared maximum `0x7EEAC6` immediately before them are exact, as is bitstream reachability.
- **SMS-160 VERIFIED with a full per-codebook recount**, and the SMS-123 interlock sharpened: SMS-123's core defect stands, but a reorder-only fix still admits 1–2 entries past end for codebooks 1/2/4/6/10, and a `>=`-on-declared-count fix would falsely reject codebook 11's valid offset 373. The -160 bounds must be corrected before/with the -123 reorder.
- **Two findings were strengthened past their original claims:** SMS-158's hang is proven *permanent* (the failed-enqueue PCB can never retransmit, time out, or error out), and SMS-168's INFERRED label is conservative — a full-tree census shows no code path ever clears the LINK flag, so the mechanism is code-provable and only PHY behaviour remains inferred.
- **Line drift is the audit's recurring minor defect, confirmed again:** 15 findings hold in mechanism with citations off by 1–10 lines (worst: SMS-180's supporting ranges ~8–10 lines; SMS-128's `backward_size` is also a 32-bit field, not a byte field — the reachable range is *larger* than claimed).
- **2 new candidate findings** filed (SMS-230, SMS-231), both parent spot-checked at their cited sites.

---

## Verdict table

| ID | Verdict | One-line evidence |
|---|---|---|
| SMS-015 | **REFUTED** | `_tbl_fits` (`src/SMS_ContainerMOV.c:143-146`) runs before every cited malloc and bounds `count × on-disk width` against the atom's remaining bytes; atom sizes are bounded by the parent chain (`:883`) rooted at the real file size (`:1063`); every malloc result is NULL-checked before the count is published. |
| SMS-052 | VERIFIED | `src/SMS_Container.c:347-356` exact; script-confirmed `aSize==17, cbSize≥1 → lLen=-1 → malloc(0)`; `Read` takes `unsigned int` (`include/SMS_FileContext.h:79`) so −1 arrives as 4,294,967,295; `STIO_Read` (`src/SMS_FileContext.c:1337-1376`) streams until EOF into the 0-byte allocation. |
| SMS-053 | VERIFIED | AVI `src/SMS_ContainerAVI.c:570-571` / ASF `src/SMS_ContainerASF.c:302-303` store `File_GetUInt` into `int32_t` fields (`include/SMS_Codec.h:70-71`); the only gate (`src/SMS_Player.c:1726-1731`) tests only `> 1024`; the ≤0 guards in `SMS_VideoBufferInit`/`SMS_FrameBufferInit` run under HWCtl *after* codec `Init` consumed the values in `SMS_MPEGContext_Init` (`src/SMS_MPEGInit.c:25-31,48-60`). |
| SMS-057 | VERIFIED | All four rotate copies (`src/SMS_GUIMenuSMS.c:2190-2200, 2219-2239, 2241-2258, 2348-2362`) wrap at 17 but test `lVal < 0`, so `1 + (−1) = 0` is stored; original sinks, inline sinks at `:2231/:2253`, and R6-REV-1 sinks (`src/SMS_Spectrum.c:51,55`, `src/SMS_GUIDesktop.c:756`, `src/SMS_GUInfoPanel.c:141`, `src/SMS_PlayerControl.c:431`) all read `g_Palette[idx − 1]` with `idx` unsigned — index `UINT_MAX` ≡ address `g_Palette − 4`. |
| SMS-060 | VERIFIED | `src/SMS_EE.c:952-962` exact: on `lSize != apData->m_Size` the buffer is freed and set NULL, then `lzma2_uncompress(..., UNCACHED_SEG(retVal), lSize)` runs unconditionally — with `retVal == NULL` the destination is the uncached alias of physical address 0 (in-tree convention `include/SMS.h:112`); sole caller `SMS_InitVU` (`:967-979`) already handles a NULL return. |
| SMS-103 | VERIFIED | `ums_fat.c:421` `lOff=((seq&0x3F)-1)*13`, no check; script range −13…806 into `m_Name[128]` (`:124`), stack `lDir` at `:555`/`:638`. |
| SMS-104 | VERIFIED | `SMS_DTS.c:10756` 3-bit field +1 = 1–8 vs arrays sized `DTS_PRIM_CHANNELS_MAX`=5 (`:26`, `:101-117`); loops `:10758+` write per channel, only values clamped. |
| SMS-105 | VERIFIED | `SMS_MPEG4.c:933` 6-bit count 0–63 unclamped; loop `:1183-1201` writes `lD[i]` into stack `int lD[4][2]` (`:1174`); 0–3 switch only at `:1236`. |
| SMS-106 | VERIFIED-DRIFT | Mechanism exact (acc ≤ 64×8 = 512 into `booklist[256]`); fill is at `:3536`, validation at `:3540-3541` — not `:3540`/`:3545-3546` as cited. |
| SMS-107 | VERIFIED | `floor1_unpack` count ≤ 31×8 = 248 into `postlist[VIF_POSIT+2]`=65 (`SMS_OGG.c:36,64`); `floor1_look` stack `sortpointer[65]` (`:3574`) written for `i<n` at `:3588`. |
| SMS-108 | VERIFIED-DRIFT | 24-bit `begin/end/grouping` at `:3523-3525`, never validated; sinks at `:4740` / `:4908` (cited `:4741`); PCM bound is 32768 floats max, not 8192 (begin up to 2²⁴ dwarfs either). |
| SMS-109 | VERIFIED | `ip.c:227-230` claimed length, `pbuf_realloc` shrink-only (`pbuf.c:248`); `ip_frag.c:151` checks destination only; `copy_from_pbuf` loop `:67-72` has no `p != NULL`; `IP_REASSEMBLY 1` (`lwipopts.h:121`). |
| SMS-110 | VERIFIED | `ums_driver.c:312-313` verbatim store, division `:395`, `ums_cache.c:40`; mount order `ums.c:200` FATFS_Init before `:202` CacheInit; cache 131072 B (`ums.c:66`); BPB whitelist contrast `ums_fat.c:1048-1052`. |
| SMS-111 | VERIFIED-DRIFT | Totals ≤ 15·2·3+3 = 93 > `MAX_CHANNELS` 64; late check `:5905`, ignored returns `:5931`/`:7917`, checked caller `:6072`; `MAX_CHANNELS` is defined at `:110`, not `:115`. |
| SMS-112 | VERIFIED | `SMS_OGG.c:4734` and `:4900` read `decodemap[temp]` (and NULL-test it) before any `temp < partvals` check; `partvals` from `res0_look` `:3657`, phrasebook `entries` independent 24-bit field. |
| SMS-113 | VERIFIED | `SMS_MPEG4.c:2733-2736` stores pixel `m_Width/m_Height` in `m_HEdgePos/m_VEdgePos` vs macroblock init `SMS_MPEGInit.c:33-34`; flag set `:3324`/`:3329`; VOL aggravator at `:895-903`. |
| SMS-114 | VERIFIED | `SMS_DTS.c:10261` `table[flags & 0x3F][input]`, table `[11][10]` (`:10203`); both indices are 6-bit fields (`amode` `:10722`, sync flags `:10134`); `dts_channels[16]` (`:145`) indexed by `output & 0x3F` at `:11299-11309`. |
| SMS-115 | WEAKENED | Sink exact (`SMS_AC3.c:2585`, table of 2 at `:2576`, index up to 5), but the shipped player flow defeats the "valid 5.1 suffices" headline — `_check_ac3` clamp + SPDIF routing; surviving triggers in reasoning below. |
| SMS-116 | WEAKENED | Sink exact (`SMS_DTS.c:11565`, table of 2 at `:11556`, `DTS_Channels` 2/4/5/6 at `:11493-11502`), same player-flow guard chain as SMS-115. |
| SMS-117 | WEAKENED | `exclude_mask[64]` walk verified (passes 63 at the 9th round), but `n` advances by 1 per round (`SMS_AAC.c:7557`), not 7 — `additional_excluded_chns[64]` needs ~64 rounds, not 10; struct citation drifted (`drc_info` at `:324-337`). |
| SMS-118 | VERIFIED-DRIFT | `entries*dim` at `:3217` (cited `:3219`), allocation `:3220` (cited `:3223`), fields `:3162-3163`; overflow threshold `dim ≥ 129` at max entries recomputed exactly. |
| SMS-119 | VERIFIED | `SMS_OGG.c:3657` `rint(pow(parts,dim))`, allocation `:3659`; script: 64⁵ = 2³⁰, ×4 B wraps to 0 on 32-bit `size_t`; 64⁶ = 2³⁶ out of `int` range. |
| SMS-120 | VERIFIED | Ordered lengths grow unbounded (`:3189-3197`); `_make_words` reads `marker[length]` at `:3732` before the `length < 32` test at `:3733`, `marker[33]` at `:3724`; `oggpack_look` indexes `mask[anBits]` at `:4305`, `mask` has 33 entries (`:148-156`). |
| SMS-121 | VERIFIED-DRIFT | Blocksizes 8…32768 accepted (`:3109-3116`); window index computed at `:3982-3983` (cited `:3989-3990`); script ilog range −3…9 into `vwin[8]` (`:2449-2452`), dereferenced `:4978+`. |
| SMS-122 | VERIFIED | `sf_index` from ADTS `:5953` / PCE `:5821`, stored `:6041`/`:6030` unchecked; `window_grouping_info` reads 12-entry tables at `:6204-6208`, `:6218-6220`; sibling guards at `:5768` and `:6253`. |
| SMS-123 | VERIFIED | Use-before-check at both sites: `hcb_2_*_table[cb][offset].bits` read at `:6464`/`:6531`, range check only at `:6467`/`:6534`; `offset` inflated by file bits at `:6463`/`:6530`. (Fix-ordering interlock with SMS-160 sharpened — see SMS-160's note in the summary.) |
| SMS-124 | VERIFIED-DRIFT | Writes precede the bound test: `sect_cb` write at `:6303` (cited `:6300`), `sect_start/end` at `:6313-6314`, check at `:6316-6317`; zero-length sections keep `k` fixed while `i` increments. Nuance: for groups g<7 the `[g][120]` write lands in the next row; only the last group writes past the array. |
| SMS-125 | VERIFIED | `SMS_WMA.c:2059` `bit_offset` unchecked; re-base at `:2073-2074` passes `(16384-(pos>>3))*8` to `SMS_InitGetBits(..., uint32_t)` (`SMS_Bitio.h:143`) — negative int converts huge; `:2083` check guards a different `memcpy`. |
| SMS-126 | VERIFIED | `bitstream_fill_current` dereferences `*state->buffer_start++` unconditionally at `SMS_DTS.c:10009`; `dts_state_t` (`:67-125`) carries `buffer_start` and no end pointer. |
| SMS-127 | VERIFIED-DRIFT | Reader never compares `m_Idx` to `m_szInBits` (`SMS_Bitio.h:28` macro, `:70-78`, `:90-97`); the only length test is between elements at `SMS_AAC.c:7882` (cited `:7884`); AAC init site is `:8171`, not `:8183`. |
| SMS-128 | VERIFIED-DRIFT | Oracle at `src/lzma2.c:84-122` (cited 80-122), `lzma2_decode_size` at `:51-81`, varint `decode` at `:29-49`; callers exact (`SMS_EE.c:950`, `SMS_GS_2.c:567`, `SMS_IOP.c:237`); `backward_size` is a 32-bit field (`include/lzma2.h:16`), not a byte field — reachable pointer range is larger than claimed. |
| SMS-129 | VERIFIED | `tcp_in.c:157-158` ignores `pbuf_header` return; POOL branch (`pbuf.c:295-312`) has no forward-move length check, `len`/`tot_len` wrap as u16. (Illustrative wrapped value corrected to 65,476, not ≈65,492 — verdict unaffected.) |
| SMS-130 | VERIFIED | `udp.c:172` strip of 28 B on a `<28` B pbuf cannot fail for POOL pbufs; lengths wrap to 65528+u; checksum gate restores `tot_len` to `u` before testing. |
| SMS-131 | VERIFIED | `smap.c:1010` BD length unclamped; `pbuf_alloc` POOL is single-pbuf (`pbuf.c:182-185`); FIFO copy is sized by `tot_len` (asm `smap.c:825`) into a 1540 B slot. |
| SMS-132 | VERIFIED | `hcd.c:190-195` walk checks only `bLength >= 2`; consumers (`hub.c:636-651`, `hcd.c:75-123`) read fixed struct offsets from returned descriptors. |
| SMS-133 | WEAKENED | The `:2948`/`:4310` total-bits test algebraically bounds every fall-through read of `ptr[1..4]`; only a 0-bit read at `endbyte == storage` still touches `ptr[0]` one byte past — the claimed up-to-4-byte over-read cannot occur. |
| SMS-134 | VERIFIED-DRIFT | `dim==0` hang confirmed (`SMS_OGG.c:3128-3151` + `:3216`); field/check lines drifted +2 (actual `:3162`/`:3163`/`:3165`), second sink actual `:4413` not `:4420`. |
| SMS-135 | VERIFIED-DRIFT | `mode >= modes` reaches NULL `mode_param[]` deref; actual lines `:4171`/`:4173`/`:4176`, `modebits` at `:3972` (claimed 4177/4179/4182/3968). |
| SMS-136 | VERIFIED-DRIFT | `order==1` → `c=0`, `do{}while(--c)` never terminates; actual guard `:3395`, sink `:4504-4510`, caller `:4540-4544` (claimed 3399/4505-4512/4543-4548). |
| SMS-137 | VERIFIED | 3-bit `bitalloc_huffman` (`SMS_DTS.c:10780`); value 7 falls to `bitalloc_12[7]` past the 5-entry table (`:189`, `:11340-11346`). |
| SMS-138 | VERIFIED | Script: all five `scales_*_129` tables return −64…+64; ≤32 accumulations index `quant6[64]`/`quant7[128]` unclamped at `:11382`/`:11393`. |
| SMS-139 | VERIFIED | `scale_factor_quant7[bitstream_get(state, 8)]` at `:11440` — 0–255 into a 128-entry table (`:1127`). |
| SMS-140 | WEAKENED | Source 5/6 OOB read of `subband_activity[5]` stands (intra-struct), but the −1 arm is defeated by the `joint_intensity[j] > 0` guard at `SMS_DTS.c:11404`, and the loop it bounds only consumes/discards bits — it writes nothing. |
| SMS-141 | VERIFIED | `n==0` do-while fill (`SMS_WMA.c:1645-1648`, `:1657-1660`); v1 init fills row 0 only (`:1347-1361`), zero rows selected via checked `v < nb_block_sizes` (`:1727-1737`). |
| SMS-142 | WEAKENED | No zero check and integer divisions by `sample_rate` exist at `SMS_WMA.c:1352`/`:1383`, but `:1397` is a *float* division (not a third integer div-by-zero), and MIPS integer div-by-zero does not trap — quotient is UNPREDICTABLE, not a crash per se. |
| SMS-143 | VERIFIED | `m_SliceHeight = m_MBH/(lCode-0x16)` is 0 for `m_MBH < divisor` (`SMS_MSMPEG4.c:2151`); `% m_SliceHeight` at `:2332` is evaluated on the first while-test (slice returns immediately via `:2205`). Carries the MIPS no-trap caveat (see reasoning in section file). |
| SMS-144 | VERIFIED | `m_PPFieldTime==0` is constructible under the `:2602-2605` guard's own inequalities (worked example in section file); interlaced `lTimePP` can be 0 and divides at `SMS_MPEG4.c:1592-1597`. Carries the MIPS no-trap caveat. |
| SMS-145 | VERIFIED | Script: `s_iSLen2` length fields reach 5 (scf ≤31), `s_nSLen2` max 4; recomputed max index 382 (Pow2Gain) / 438 (FullGain) > `s_GainPow2[378]`. |
| SMS-146 | VERIFIED | Unbounded `wasted` run drives `m_CurBPS` negative (`SMS_FLAC.c:275-278`); negative count reaches `SMS_GetSBits`/`SMS_GetBits` and the `SMS_NEG_USR32` shift (`include/SMS.h:68`). |
| SMS-147 | VERIFIED-DRIFT | Unconditional `decoded[-1…-4]` reads at `SMS_FLAC.c:180-183` for orders 0–3; order selection is at `:287` (claimed ":285 region"); `m_pDec[1]` layout (`:391-392`) matches the claim. |
| SMS-148 | VERIFIED-DRIFT | No `aSize >= sizeof(_DXSB_Header)` (53 B) check; RLE writes at `SMS_DXSB.c:274` span header `lW` from `0x70001B30` (9424 B of scratchpad left); read-back `dxsb_pack` is at `:278` (claimed :281). |
| SMS-149 | VERIFIED | `m_LongIdx[23]` (`SMS_MP123.c:27`); `lR0C+lR1C` ∈ 2–24 at `:5462-5463` reads `m_LongDiff[0..1]` — intra-struct, as filed. |
| SMS-150 | VERIFIED | Walkers return −1 (`ums_fat.c:270/312/343`); `_set_dir_chain` handles only `==0` (`:484-487`), loop runs zero times, `:500` reads `m_ClusterBuf[-1]`; sibling guard at `:403`. |
| SMS-151 | WEAKENED | Return ignored at `ums_fat.c:566` and `:652` and `lDirSec`/`lDirSector` are uninitialised — but `lStartSec`/`lStartSector` *are* written at `ums_fat.c:400` before the failing chain call; only the loop extent is garbage, not the start sector. |
| SMS-152 | VERIFIED | Bitmap is 90 B (`5760/64`, `ip_frag.c:89`); guard `:151` admits `offset+len == 5760`; accesses at `:174` and `:195` index byte 90 (OR operand is 0 for this exact length). |
| SMS-153 | WEAKENED | No cap and the UB window is real, but `i` is `uint8_t` (`SMS_AAC.c:6560`) and wraps at 256 — UB requires exit `i ∈ [32,255]`, not an "arbitrarily large" counter. |
| SMS-154 | VERIFIED | `SMS_DSP_FFT.S:363` `addiu $v0,$v0,32` is the first `$v0` touch in `DSP_FFTGet` (entry `:337`); sole caller `SMS_Spectrum.c:84`, `void` prototype `SMS_DSP.h:82`; `DSP_FFTRun` happens to exit with `$v0==0` — luck, not contract. |
| SMS-155 | VERIFIED | Script-compared layouts: asm inserts 576P at offset 44, C has no such field — every PAR from 44 sits one slot early; latency holds because the four divergent-region words are all `0x3F800000` (correction to the original: not all seven — NTSC/PAL/480P differ, at aligned offsets; latency conclusion unchanged). |
| SMS-158 | VERIFIED | `api_msg.c:518-519` discards `tcp_connect`'s return; a failed `tcp_enqueue` (`tcp_out.c:296-308` → `ERR_MEM`) leaves a SYN_SENT PCB with `unacked == NULL` and `nrtx == 0`, so `tcp_slowtmr` (`tcp.c:557-575`) never retransmits, removes, or calls `err_tcp` — the `sys_mbox_fetch` at `api_lib.c:312` is never posted to. Hang proven permanent (strengthened). |
| SMS-159 | VERIFIED | `sockets.c:104-115`: first statement is `&sockets[aSD]`, no range test in `get_socket`, any caller, or `ps2ip.c`; `NUM_SOCKETS = MEMP_NUM_NETCONN = 15` (`sockets.c:46`, `lwipopts.h:45`); `lwip_selscan:535-551` feeds caller `maxfdp1` into the same index. |
| SMS-160 | VERIFIED | Script recount: quad actual 113/85/184 vs declared 114/86/185; pair actual 125/83/209/374 vs declared 126/83/210/373; binary actual = declared 161/161/127/337; first-stage max-reachable = actual−1 for all seven codebooks. |
| SMS-161 | VERIFIED-DRIFT | Zero tails, counts (64/128), max value `0x7EEAC6` and reachability all script-verified (twice, independently); but consumer lines drifted 1–4 (actual scale_table uses `:11382`/`:11393`, bitstream reads `:11379`/`:11381`) and the tables are **non-decreasing, not strictly increasing** (repeated small values, e.g. quant6 2,2 / 3,3). |
| SMS-162 | VERIFIED | `hcd.c:340-341` `AllocSysMemory` → unconditional `memset(memBuf, …)`; `hcdInit` ignores `initHcdStructs` at `:488` and returns 0 at `:496`; `usbd.c:265-268` `_start` ignores `hcdInit` and returns 0. |
| SMS-163 | VERIFIED | Census of every `get_socket` caller: all return on `!sock` except `lwip_close` (`sockets.c:226-255`), which falls through to `sock_set_errno(sock, lErrNo)` at `:250`; the macro (`:99-102`) writes `sk->err` through the NULL pointer. |
| SMS-164 | WEAKENED | Missing check is real and `do_bind` is the only unchecked PCB allocator (full handler census confirms), but only the **TCP** branch is reachable in-tree (via `err_tcp` NULLing the PCB at `api_msg.c:183`, then re-bind); the UDP branches have no in-tree entry path with `pcb.udp == NULL` — latent, not live. |
| SMS-165 | VERIFIED | `usbd.c:106` clears `hcIsoTdToIoReqLUT[arg − hcIsoTdBuf]` inside the `curTd` loop instead of `curTd`'s slot; generic twin at `:183` clears `tdListPos`'s slot correctly; stale entry is dereferenced/freed by `killEndpoint`'s LUT sweep (`hub.c:216-237`). |
| SMS-166 | VERIFIED | `err_tcp` posts NULL to `acceptmbox` (`api_msg.c:196-201`); `netconn_accept` returns it; `lwip_accept:159-161` passes it to `netconn_peer`, which switches on `conn->type` (`api_lib.c:215`); `addr`/`addrlen` dereferenced unconditionally at `sockets.c:170-172` vs the `from && fromlen` guard at `:355`. |
| SMS-167 | VERIFIED | `api_lib.c:74-78`: `buf->p = pbuf_alloc(PBUF_TRANSPORT, 0, PBUF_REF)` then three writes through `buf->p`; `pbuf.c:205-207` returns NULL on `memp_malloc(MEMP_PBUF)` failure (pool 40, `lwipopts.h:18`); sole caller `lwip_send` UDP path (`sockets.c:413`) checks `netbuf_new`, not this. |
| SMS-168 | VERIFIED | Only two stores to `m_Flags` offset 0 exist in the tree — `_smap_phy_reset`'s OR of bit 1 (`smap.c:510-513`) and `SMap_Start`'s OR of bit 0 (`:731-732`) — neither clears bit 1; `SMap_Reset` (`:692-697`) discards the `int` result (`:311`); consumer at `:904`. The original INFERRED label is conservative: the mechanism is code-provable; only PHY behaviour remains inferred. |
| SMS-169 | VERIFIED | `main.c:338-343`: `CreateEventFlag`, `CreateMutex`, and `StartThread(CreateThread(…))` at `:341` (thread id not even stored) all unchecked; `return MODULE_RESIDENT_END` at `:343` unconditional; the IO thread created there is the only trigger of `smap_module_init` (`main.c:289-303`). |
| SMS-170 | VERIFIED | `netconn_peer` guards PCB-NULL and returns `ERR_CONN` (`api_lib.c:219, 226-227`); `netconn_addr` (`:235-252`) has no guard and no `conn->err` check, dereferencing at `:243/:247`; producer `err_tcp` at `api_msg.c:183`; caller `lwip_getsockname` at `sockets.c:861, 868`. (See new candidate SMS-230: fixing -170 alone relocates a crash.) |
| SMS-171 | VERIFIED | `sl_TXBuf` is `(1514+4+3)/4 = 380` u32 = 1,520 B (`smap.c:888`, `SMAP_TXMAXSIZE` 1514 at `:17`); the only gate compares rounded length to `m_TXFree` = 4,096 (`:908`, set `:708`); the chained gather (`:914-919`) has no bound against the buffer; MTU 1500 at `main.c:250`. |
| SMS-179 | VERIFIED | `err_t` = `s8_t` = `signed char` (`err.h:39`, `arch/cc.h:7`); macro at `sockets.c:89-91` compares against a `sizeof` (`size_t`, unsigned) operand; host execution of the exact macro: `ERR_OK` → `table[0]`, every error −1…−10 → EIO branch. All four `err_to_errno` call sites confirmed (`:216, :280, :305, :426`). One citation correction: error codes span `err.h:43-59`, not 43-53. |
| SMS-180 | VERIFIED-DRIFT | Core site exact: `driver.c:239-249` (`CreateThread` at `:246`, `StartThread` at `:247`, `return 0` at `:249`), sole caller `hcd.c:485` ignores the result; supporting delivery-path ranges drifted ~8–10 lines (actual `signalCallbackThreadFunc` `:184-200`, `callbackThreadFunc` `:202-237`). |
| SMS-182 | VERIFIED | `tcp.c:909-913`: `static u32_t iss = 6510; iss += tcp_ticks;`; `tcp_ticks` starts at 0 (`tcp_init:92`) and increments only in `tcp_slowtmr` (`:546`); no `rand`/seed/entropy input exists anywhere in SMSTCPIP (grep); call sites `:506` and `:822` confirmed. Refinements make it *more* predictable than claimed (cumulative iss; pre-first-tick connections all get exactly 6510). |

---

## Full reasoning — non-VERIFIED findings (25)

Reproduced from the verification section files (`verify-sections/verify-sms-{A,B,C,D}.md`), which also carry the full reasoning for the 50 VERIFIED findings.

## SMS-015 — MOV container trusts file-supplied entry counts for malloc size

**Verdict: REFUTED** (defeating guard present at the pin, at every cited site).

### Locations checked (claimed vs actual)

| Claimed | Actual at pin | Result |
|---|---|---|
| `src/SMS_ContainerMOV.c:495,546,576,614,651,680` — `malloc(lnEntries * sizeof(...))` for stts/stsc/stsz/stco/ctts/stss | The six mallocs are at exactly those lines: `:495` stts, `:546` stsc, `:576` stsz, `:614` stco/co64, `:651` ctts, `:680` stss; `lnEntries` is read from the file at `:490,542,566,604,647,673` (`_read_int_be`) | Locations exact — but the finding stopped reading one line too early at each site (see Mechanism) |
| `src/SMS_ContainerMOV.c:129,454,952` — comments describing a count that "asks malloc for hundreds of megabytes on a 32 MB heap" and tables that must be NULL-checked | `:128-142` is a comment block that begins with that historical description and then states, in the present tense, "Both halves are closed here: the count is cross-checked against the space the atom actually declares … and each allocation is checked." `:454`-area and `:947-956` comments are the same genre (invariant documentation for `_mov_build_index`) | The comments describe a *closed* bug class, not a live one |

### Mechanism re-derivation

The proposed fix in the finding — "reject a table whose `count × entry size` exceeds the containing atom's remaining bytes" — **already exists, verbatim in effect**, as a shared helper:

```c
/* src/SMS_ContainerMOV.c:143-146 */
static int _tbl_fits ( const MOVAtom* apAtom, unsigned int anEntries, unsigned int aWidth, unsigned int aConsumed ) {
 int64_t lAvail = apAtom -> m_Size - aConsumed;
 return lAvail >= 0 && ( int64_t )anEntries * aWidth <= lAvail;
}
```

Every cited allocation is preceded by it, with the correct on-disk width and consumed count:

- stts `:494` — `!_tbl_fits(apAtom, lnEntries, 8, 8)` → `return 0` before the `:495` malloc;
- stsc `:545` — width 12, consumed 8, before `:546`;
- stsz `:572-575` — width 4, consumed 12, before `:576` (variable-size path only; the fixed-size path at `:569-570` returns before any allocation);
- stco/co64 `:609-613` — width 8 for `co64`, 4 for `stco`, consumed 8, before `:614`;
- ctts `:650` — width 8, consumed 8, before `:651`;
- stss `:679` — width 4, consumed 8, before `:680`.

Two further layers the finding did not credit:

1. **Multiply-overflow guard** immediately before each `_tbl_fits` call: `if (lnEntries >= UINT_MAX / sizeof(<table type>)) return -1;` (`:491,543,567,605,648,674`).
2. **NULL check + publish-after-fill:** each malloc result is tested (`if (!lpMyStm->m_pStts) return 0;` etc. at `:496,547,577-580,615,652,681`) and the count field is only assigned after the allocation succeeds — the exact invariant the `:947-956` comment warns maintainers to preserve.

### Reachability / guard hunt (the decisive part)

A sceptic's next move is: "_tbl_fits trusts `apAtom->m_Size`, which is also file-supplied — declare a giant atom in a tiny file and the bound is meaningless." That escape is closed by the dispatcher:

- `_mov_read_header` roots the parse at the real file: `MOVAtom lAtom = { 0, 0, lpFileCtx->m_Size };` (`src/SMS_ContainerMOV.c:1063`).
- `_mov_read_dflt` rejects any child atom that overruns its parent: `if (lAtom.m_Size < 0 || lAtom.m_Size > apAtom->m_Size - lTotalSize) break;` (`:883`), applied recursively down moov → trak → stbl → the six table atoms.

So a table atom's declared size cannot exceed the bytes its ancestors declare, which cannot exceed the actual file size. Worst case at the pin, per table, in-memory allocation vs on-disk width: stts 8/8, stsc 12/12, stsz 4/4, ctts 8/8, stss 4/4 — ratio 1×; stco is the worst at 8 in-memory / 4 on-disk = 2×. A crafted file can therefore ask for at most roughly **its own size (×2 for stco)** in table memory — not "hundreds of megabytes" from a small file, and any allocation failure is handled by the NULL checks above. The claimed primitive (huge allocation attempt before any validation against actual file size) does not exist at this pin.

**Why this is REFUTED rather than WEAKENED:** the finding's Problem sentence is categorical ("A crafted MP4 can still drive a huge allocation attempt before any sample is validated against the actual file size") and its Fix is precisely the `_tbl_fits` check that is already implemented at all six sites. Nothing narrower survives: the residual behaviour (allocation proportional to file size, failure-checked) is ordinary parser behaviour, not the filed defect. (Historical note, not part of the verdict: the round-1 pin and the round-2 pin differ only by docs/CSS commits per `PINS-ROUND2.md`, so the guard was already present when the finding was written — the original read appears to have quoted the malloc lines and the historical comments without reading the `_tbl_fits` calls on the immediately preceding lines.)

---

---

### SMS-106 — VERIFIED-DRIFT

Claimed vs actual: `res0_unpack` spans `:3517-3549` — exact range. Struct `vorbis_info_residue0` at `:89-99`, `booklist[256]` at `:90`, `secondstages[64]` at `:91` — exact. **Drift:** the fill loop is at `:3536` (`for (j = 0; j < acc; ++j) info->booklist[j] = oggpack_read(apBuf, 8);`), not `:3540`; `:3540-3541` is the post-fill validation loop (finding cites the validation at `:3545-3546` — those lines are the `errout:` label/free). The groupbook check is at `:3538`.
Arithmetic (script): `partitions = oggpack_read(apBuf, 6) + 1` (`:3526`) ≤ 64. `cascade` = 3 bits, optionally OR'd with 5 bits << 3 (`:3530-3531`) → an 8-bit value; `icount` (`:2552-2559`) counts set bits, ≤ 8. `acc += icount(cascade)` (`:3533`) ≤ 64 × 8 = **512**. Fill writes `booklist[0…511]` into a 256-entry array; struct order (`booklist`, `secondstages[64]`, `classmetric1/2[64+64]`, then `begin/end/grouping/partitions/groupbook`) matches the claimed walk-through, with the tail passing the `calloc`'d struct.
Guard hunt: validation of `booklist[j] >= ci->books` runs only after the fill and checks values, not count — as claimed. `res0_look` re-consumes `booklist[acc++]` at `:3653` with the same unbounded total. No cap anywhere.

---

### SMS-108 — VERIFIED-DRIFT

Claimed vs actual: field reads `:3523-3525` exact (24 bits each; `grouping` is read + 1). **Drift:** `_01inverse` spans `:4707-4757` (cited `:4715-4777`); the offset is computed at `:4740` (`int offset = info->begin + i * samples_per_partition;`, cited `:4741` — `:4741` is the `secondstages` test that gates the write). `res2_inverse` spans `:4876-4919`; the offset argument is at `:4908` as claimed; its `_vorbis_block_alloc` is at `:4889`, not `:4893` (`:4893` is the all-zero-channel early return).
Arithmetic: `begin`/`end`/`grouping` ∈ [0, 2²⁴). No check relates them to `blocksizes[]` at unpack, `res0_look`, or either inverse. `decodepart(stagebook, appIn[j] + offset, …, samples_per_partition)` (`:4744-4747`) writes `grouping` floats starting at `begin + i·grouping` (`vorbis_book_decodev_add`, `:4796-4832`, writes `aN` floats). **Correction to the finding's bound:** the PCM channel buffers are `calloc(pcm_storage)` with `pcm_storage = blocksizes[1]` (`:3994-3999`), and `blocksizes[1]` can be up to 32768 under the current guards (SMS-121's domain), not "at most 8,192 floats" — the overflow magnitude is smaller than claimed at the top end but `begin` up to 16,777,215 still dwarfs any legal blocksize, so the mechanism is unchanged. `end < begin` gives negative `n`/`partvals` at `:4718-4719` (and `:4886-4888`), feeding the alloc at `:4889`.
Guard hunt: none found in any consumer. Trigger still needs a partition whose stage book is selected (the original's INFERRED flag on the full trigger stands).

---

### SMS-111 — VERIFIED-DRIFT

Claimed vs actual: `program_config_element` spans `:5812-5909` exact; late check at `:5905` exact. Ignored returns: `get_adif_header` at `:5931` exact; `raw_data_block` ID_PCE at `:7917` exact. Checking caller `GASpecificConfig` at `:6072` exact. Sinks: `decode_cpe` at `:7808-7809` exact; `create_channel_config` writes span `:7992-8020` (claimed `:7997-8036` — the range covers the front/side/back loops; the first write is `:7992`). **Drift:** `MAX_CHANNELS 64` is defined at `:110`, not `:115` (`:115` is `FAAD_NO_SBR`). Arrays: `internal_channel[MAX_CHANNELS]` at `:512`, `channel_position[MAX_CHANNELS]` at `:480`.
Arithmetic (script): front/side/back counts are 4-bit fields ≤ 15 elements each, each ≤ 2 channels → 90; LFE is a 2-bit field ≤ 3 → total ≤ **93** > 64. During construction (`:5841-5888`) the running total is stored as the *value* in `cpe_channel[tag]`/`sce_channel[tag]` (tag is 4-bit, arrays `[16]` at `:373-374` — those stores are in-bounds; the poisoned values up to 92 are what the downstream sinks index with). The `:5905` check therefore cannot prevent the downstream OOB, and two of three callers ignore it anyway.

---

### SMS-115 — WEAKENED

Sink arithmetic — fully verified: `_ac3_decode` at `SMS_AC3.c:2574`; table `s_FloatToInt[2]` at `:2576-2578`; selection `s_FloatToInt[anChannels - 1]` at `:2585` with no clamp. Channel derivation: `g_AC3Channels[8] = {2,1,2,3,3,4,4,5}` at `:38`, +1 for LFE (`:2723`) → up to 6, so the index can reach 5 — exactly as claimed, and `AC3_Decode`'s own update logic (`:2725-2731`) only lowers `m_Channels`, or fills it when it is 0.
**Defeating guard the finding understates:** the finding's parenthetical ("unless the container pre-sets the channel count to 1/2") misses the player layer. `SMS_Player.c:_check_ac3` (`:100-115`), called before codec Init/Decode at `:732`, `:837` and `:1766`, clamps `m_Channels > 2` to 2 whenever the non-SPDIF path is in use (`:104-105`); and in SPDIF mode `SMS_Codec_AC3_Open` selects `_ac3_spdif`, not `_ac3_decode`, as `DecodeFrame` (`SMS_AC3.c:347-349`), and `_check_ac3` sets `m_Channels = 5` only in that SPDIF branch (`SMS_Player.c:109`). Every container that reports channels from the same header (raw AC3 probe `SMS_ContainerAC3.c:34-36`, MPEG-PS `SMS_ContainerMPEG_PS.c:412-414`) therefore arrives at `_ac3_decode` with `anChannels ≤ 2`. A perfectly valid 5.1 file played normally does **not** trigger this.
Exact surviving conditions: (a) non-SPDIF mode, **and** (b) the container leaves `m_Channels == 0` at setup — e.g. a WAV-header-derived stream (`SMSContainer_GetWAVHeader`, `SMS_Container.c:340`, reads `nChannels` verbatim from an AVI/ASF header; 0 survives `_check_ac3`, which only reduces values > 2) — whereupon `AC3_Decode` self-fills `m_Channels` to 5/6 from the frame header at `:2725-2727` and the next `DecodeFrame` call indexes `s_FloatToInt[4]`/`[5]`; or (c) any caller of the codec API that bypasses `SMS_Player`'s `_check_ac3`. Real defect, narrower trigger than claimed.

---

### SMS-116 — WEAKENED

Sink arithmetic — fully verified: `_dts_decode` at `SMS_DTS.c:11554`; table `s_FloatToInt[2]` at `:11556-11558`; selection at `:11565`. `DTS_Channels` (`:11493-11502`) returns 6 (LFE), 5, 4, or 2 — index up to 5, as claimed. `DTS_Decode`'s update logic (`:11753-11759`) mirrors AC3: fill when 0, otherwise only lower.
Guard chain — identical to SMS-115 and equally defeating for the normal flow: `_check_ac3` covers `SMS_CodecID_DTS` as well as AC3 (`SMS_Player.c:101-102`) and clamps to 2 in the non-SPDIF path; in SPDIF mode `SMS_Codec_DTS_Open` selects `_dts_spdif` (`SMS_DTS.c:9983-9985`). MPEG-PS sets DTS channels from `DTS_Channels(lFlags)` (`SMS_ContainerMPEG_PS.c:430`) and is clamped at setup.
Exact surviving conditions: non-SPDIF mode plus a container leaving `m_Channels == 0` (WAV-header path, `SMS_Container.c:340`) so `DTS_Decode` self-fills 4/5/6 at `:11753-11755`, or a non-Player codec caller.

---

### SMS-117 — WEAKENED

Claimed vs actual: `excluded_channels` at `SMS_AAC.c:7548-7561` exact (finding's range `:7548-7562`). Arrays `exclude_mask[MAX_CHANNELS]` / `additional_excluded_chns[MAX_CHANNELS]` at `:333-334` in `drc_info` (`:324-337`; the finding cites the struct at `:300-318` — that range is `cfft_info`/`mdct_info`/`fb_info`, drift in the citation only). Entry chain: `fill_element` → `extension_payload` EXT_DYNAMIC_RANGE at `:7609-7612` → `dynamic_range_info` at `:7573` → `excluded_channels` — confirmed.
Arithmetic — half verified, half refuted: initial fill writes indices 0…6, `num_excl_chan = 7`. Each continuation round writes 7 more (`:7555-7556`) and does `num_excl_chan += 7` (`:7558`). Rounds start at 7, 14, …, 63: the round starting at 63 writes indices 63…69 — **`exclude_mask` passes 63 at the 9th round, as claimed**, and nothing caps the loop except a 0 continuation bit. But `n` is advanced by `++n` once per round (`:7557`), **not** by 7 as the finding states ("advances num_excl_chan and n by 7" / "n − 1 itself passes 63 at the 10th"). `additional_excluded_chns[n-1]` is written in the loop condition (`:7554`); `n-1` reaches 64 only after ~64 continuation rounds (each consuming 8 stream bits — suppliable, but a much larger crafted element than claimed). `n` and `num_excl_chan` are `uint8_t`, so both also wrap eventually; the `exclude_mask` overflow fires long before either wrap matters.
Verdict follows the effective behaviour: the unbounded-walk defect is real via `exclude_mask`; the `additional_excluded_chns` half is real only under the corrected, narrower arithmetic.

---

### SMS-118 — VERIFIED-DRIFT

Claimed vs actual: fields read at `:3162-3163` (`dim` 16-bit, `entries` 24-bit; cited `:3160-3161` — those lines are the `0x564342` magic check / blank). Maptype-2 `quantvals = apBook->entries * apBook->dim` at `:3217` (cited sink `:3219`); allocation at `:3220` (cited `:3223`); fill loop `:3222-3223`. `_book_unquantize` maptype-2 read `quantlist[j * dim + k]` at `:3825`, within the claimed `:3822-3840`. Maptype-1 accumulation at `:3139-3140` (`acc *= vals; acc1 *= vals + 1;`, claimed `:3128-3151` — the function spans `:3128-3151` exact) and `indexdiv *= quantvals` at `:3813` (claimed `:3808` region).
Arithmetic: `entries` ≤ 2²⁴−1 = 16,777,215; 16,777,215 × 129 = 2,164,258,215 > 2³¹−1 — the claimed `dim ≥ 129` overflow threshold at maximum entries recomputes exactly. A wrapped-negative `quantvals` makes `malloc` fail/undersize and the fill loop skip, while `_book_unquantize` still indexes `quantlist[j*dim + k]` for all `j < entries`; a wrapped-small-positive value undersizes the allocation for the same reads. Only check present is `entries == -1` (`:3165`).

---

### SMS-121 — VERIFIED-DRIFT

Claimed vs actual: source `_vorbis_unpack_info` reads at `:3109-3110` (`1 << oggpack_read(apBuf, 4)`), guards at `:3115-3116` — inside the claimed `:3104-3118`. Table `vwin[8]` at `:2449-2452` exact. **Drift:** the window index computation is at `:3982-3983` (`b->window[i] = ilog(blocksizes[i] - 1) - 6`), not `:3989-3990` (those lines are inside the `fullbooks` calloc loop). Sink `_vorbis_window_get` at `:4921-4923`, dereferenced in `vorbis_synthesis_blockin` from `:4978` onward (claimed `:4980-5013`).
Arithmetic (script, using the clone's `ilog` at `:2543-2550`): blocksizes ∈ {8,16,32,64,…,32768} → `ilog(bs-1) - 6` = **−3, −2, −1, 0, …, 9** — indices −3…−1 and 8…9 fall outside `vwin[8]`. Guards reject only `blocksizes[0] < 8` and `blocksizes[1] < blocksizes[0]`. The use sites additionally subtract `hs` (`window[1] - hs` etc.), which can only push a negative index further out. `_vorbis_window_get` returns the OOB pointer and `blockin` reads through it as the window table.

---

### SMS-124 — VERIFIED-DRIFT

Claimed vs actual: `section_data` spans `:6288-6330` exact. **Drift:** the first write is `ics->sect_cb[g][i]` at `:6303`, not `:6300` (`:6300` declares `sect_len_incr`); `sect_start/sect_end` writes at `:6313-6314` (claimed `:6314-6315`); the short-window bound tests are at `:6316-6317` (claimed `:6317-6319` — `:6318-6320` is the long-window branch).
Arithmetic/mechanism: in the EIGHT_SHORT_SEQUENCE branch `sect_bits = 3`; a section with codebook nibble + `sect_len_incr` summing to `sect_len == 0` leaves `k` unchanged (`:6323` adds 0) while `i` increments (`:6324`). The writes at `:6303`/`:6313-6314` execute **before** the `i >= 8*15` test at `:6317`, so the iteration with `i == 120` writes index 120 into `sect_cb[8][120]`, `sect_start[8][120]`, `sect_end[8][120]` (ic_stream fields at `:413-415`) before returning 15. Each iteration consumes ≥ 7 bits (4 + 3), so 121 iterations fit in a normal packet. Nuance: for `g < 7`, index `[g][120]` lands in row `g+1` of the same 2-D array (intra-struct, later overwritten); only for the last group is it past the array proper — the OOB *index* and the check-after-write defect are as claimed in all cases. The long-window branch was re-checked: its test trips at `i >= MAX_SFB` (51), inside the 120-wide arrays — not claimed, correctly.

---

### SMS-127 — VERIFIED-DRIFT

Claimed vs actual: `include/SMS_Bitio.h` — `SMS_UPDATE_CACHE` macro at `:28-32` (claimed `:26-40` range covers it; it performs an unconditional 4-byte `SMS_unaligned32_be` load at `m_pBuf + (index >> 3)`), `SMS_GetBits` at `:70-78` exact, `SMS_GetBit` at `:90-97` exact (direct `m_pBuf[lIdx >> 3]` byte read). `SMS_BitContext` carries `m_szInBits` (`:52`), set by `SMS_InitGetBits` (`:143-147`) — and a grep of the header confirms **no function in the reader ever compares `m_Idx` against `m_szInBits`**. **Drift:** the AAC init site is `SMS_AAC.c:8171` (`SMS_InitGetBits(&ld, buffer, buffer_size << 3)` in the decode entry), not `:8183` (`:8183` is the post-`raw_data_block` error test); the between-elements length test is the `while` condition at `:7882` (`ld->m_Idx < ld->m_szInBits`), not `:7884` (`:7884` is `case ID_SCE:`).
Mechanism: within one syntax element (one CPE's Huffman spectral data, one DRC/fill element), parsing can advance `m_Idx` arbitrarily far past `m_szInBits` before control returns to the `:7882` test; and even a well-formed frame's final cache load reads up to 3 bytes past the buffer end. This is the shared root under SMS-117/122/123/124 as claimed.

---

### SMS-128 — VERIFIED-DRIFT

Claimed vs actual — **line drift throughout** (file is tab-indented; the coordinator's ranges were approximate): `lzma2_get_uncompressed_size` spans `src/lzma2.c:84-122` (claimed 80-122); `lzma2_decode_size` spans `:51-81` (claimed 54-78); the varint reader `decode` spans `:29-49` (claimed 25-52 — `:25` is the tail of `encode`). Callers exact: `SMS_EE.c:950`, `SMS_GS_2.c:567`, `SMS_IOP.c:237`.
Mechanism, re-derived: (1) `footer = (xz_footer_t *)(buf + size - 12)` at `:98` is computed before any size validation; the header magic loop (`:100-106`) runs first, then the footer magic dereference (`:108-114`) — a `size < 12` call whose buffer carries a valid 6-byte XZ header magic therefore dereferences a pointer below the buffer. No caller checks `size >= 12` either (GS_2's `lBodySize = lFileSize - 20` can itself go negative for a short file). (2) `index = (unsigned char *)footer - ((i + 1) * 4)` at `:118`, where `i = footer->backward_size` (`:116`). **Correction to the finding's description:** `backward_size` is a 32-bit `unsigned int` field (`include/lzma2.h:16`), not a "raw footer byte-field" — the reachable pointer range is far larger than a byte field implies ((2³²−1+1)·4 wraps 32-bit int arithmetic at `:118`'s `(i + 1) * 4` with `i` an `int`), and `lzma2_decode_size` dereferences it immediately (`*buf != 0`, `:59`). (3) `lzma2_decode_size` receives no length: its per-varint `decode(buf, 9, …)` calls are capped at 9 bytes each, but the record loop (`:70-78`) advances `buf` by attacker-controlled varints for a `records` count (an `unsigned long` compared against an `int i`, `:70`) with no bound against the stream end, and accumulates into `size_t uncompressed_size` (`:56`) — 32-bit on the PS2 target, so the wrap-to-small consequence holds on target. All three callers trust the result as an allocation size or equality gate as claimed. The XZ Embedded core was not re-audited here (out of this batch's claim scope beyond the oracle).

---

### SMS-133 — Vorbis `oggpack_read`/`oggpack_look` near-end over-read — **WEAKENED**

- Claimed vs actual: both functions at the claimed lines (`SMS_OGG.c:2939-2974`, `:4302-4329`), guard text quoted accurately (`:2946-2949`, `:4309-4311`).
- Refutation of the main mechanism, by algebra: let `E = endbyte`, `S = storage`, `A = anBits` *after* `A += endbit` (`:2944`). Fall-through requires `E + 4 >= S` **and** `E*8 + A <= S*8`. The reads touch `ptr[i]` for `i = 1` iff `A > 8`, `i = 2` iff `A > 16`, `i = 3` iff `A > 24`, `i = 4` iff `A > 32 && endbit`. For any `i >= 1`, `A > 8i` gives `A >= 8i + 1`, and the fall-through condition gives `A <= (S − E)*8`, hence `8i + 1 <= 8(S − E)`, i.e. `i <= S − E − 1`, i.e. `E + i <= S − 1` — **in bounds**. When the guard is not entered (`E + 4 < S`), `ptr[4]` is the highest possible read and `E + 4 <= S − 1` — in bounds. Once `E > S`, the total-bits test always fires and no read happens. So the claimed "within the last 4 bytes, few bits requested → reads of `ptr[1..3]` (and `ptr[4]`) past the packet buffer" cannot occur: the second condition the finding quotes is precisely the bound that prevents it.
- Surviving residue (the exact narrower condition): `ptr[0]` is read unconditionally. With `A == 0` (a 0-bit request with `endbit == 0`) at `E == S` exactly, the total-bits test is `S*8 + 0 > S*8` = false, execution falls through, and `ptr[0]` reads **one byte** one past the packet (the value is then masked to 0 by `mask[0]`). Reachability is code-level only: a 0-width field (e.g. `modebits == 0` when `modes == 1`, `SMS_OGG.c:3972` + `:4171`; `ilog(...)`-derived widths elsewhere) requested at the exact moment the packet is fully consumed. No ≥1-bit or multi-byte over-read survives.

---

### SMS-134 — Vorbis codebook `dim == 0` — **VERIFIED-DRIFT**

- Location drift: fields are read at `SMS_OGG.c:3162` (`dim`, 16 bits) and `:3163` (`entries`, 24 bits), not :3160-3161 (that's the `0x564342` magic read); the `entries == -1` check the fix anchors to is at `:3165`, not :3163. Sink 1 exact (`_book_maptype1_quantvals`, `:3128-3151`). Sink 2 is at `:4413-4414` (`for (j = 0; j < look->m; j += b->dim)` in `floor0_inverse1`), not :4420.
- Mechanism re-derived: with `dim == 0`, the accumulation loop at `:3138` never runs, so `acc == acc1 == 1` permanently; the `while (1)` search's exit needs `acc <= entries && acc1 > entries`, impossible for `entries >= 1` (and for `entries == 0` it decrements forever instead) — the loop can never return regardless of the initial `vals`, so the UB in `(int)floor(pow(entries, 1.0F/0))` (float divide-by-zero → +inf) doesn't even matter. Called from unpack at `:3216` (maptype 1) and again from book-look at `:3800`. No `dim` validation exists in `vorbis_staticbook_unpack` — only `entries == -1` (`:3165`).
- Sink 2 nuance: in `floor0_inverse1` the step-0 loop at `:4413` calls `vorbis_book_decodev_set` each iteration; it hangs unless that call returns −1, and with `dim == 0` there is no reason it must. The setup-hang (sink 1) is unconditional and sufficient.

---

### SMS-135 — Vorbis audio packet `mode` unchecked — **VERIFIED-DRIFT**

- Location drift (+6 in the function, −4 for the derivation): mode read at `SMS_OGG.c:4171`, only check (`mode == -1`) at `:4173`, dereference at `:4176`; `modebits` derivation at `:3972`. Claimed 4177/4179/4182/3968.
- Mechanism: `ci->modes = oggpack_read(apBuf, 6) + 1` (`:3290`, 1–64); `mode_param[64]` (`SMS_OGG.h:196`) slots are `calloc`'d only for `i < modes` (`:3291-3293`). `modebits = ilog(modes − 1)` (`:3972`): for any non-power-of-two `modes` (e.g. 5 → 3 bits → packet values 5, 6, 7) the audio packet can name an unallocated slot, and `:4176` dereferences `ci->mode_param[mode]->blockflag` through NULL; `:4198`/`:4201` deref again. Values stay inside the 64-entry array (max `modebits` is 6), so this is a NULL-slot deref, exactly as the finding states — not an array overflow. No `mode >= ci->modes` check exists.

---

### SMS-136 — Vorbis floor0 `order == 1` — **VERIFIED-DRIFT**

- Location drift: `order` read at `SMS_OGG.c:3388` (claimed :3392, which is `ampdB`); guard `order < 1` at `:3395` (claimed :3399); `look->m = info->order` at `:3557` (claimed :3558); sink function `:4486-4528` with counter at `:4504-4510` (claimed :4505-4512); caller at `:4540-4544` (claimed :4543-4548).
- Mechanism: `order == 1` passes the `< 1` guard and becomes `aM = 1` in `vorbis_lsp_to_curve`. `c = aM >> 1 = 0`; the `do { q *= ftmp[0] − w; p *= ftmp[1] − w; ftmp += 2; } while (--c);` body executes unconditionally, `--c` yields −1 (truthy) and decrements away from 0 forever — an unbounded OOB read walk over `lsp` (allocated `m + dim + 1` floats at `:4412`). No `aM < 2` guard exists in unpack, `floor0_look`, or `floor0_inverse2`.
- Reachability: `floor0_inverse1` returns a memo for `m == 1` with any book `dim >= 1` (one decodev_set iteration fills `lsp[0]`, `lsp[1] = amp` at `:4419`), and `floor0_inverse2` then calls the curve function with `aN = blocksizes[W]/2 >= 4` (blocksizes validated ≥ 8 at `:3115`). Trigger INFERRED in the finding; the chain above is fully code-supported.

---

### SMS-140 — DTS joint-intensity source channel — **WEAKENED**

- Locations exact: field stored per channel from a 3-bit read at `:10771`; sink at `:11405-11408`.
- What stands: for `joint_intensity[j]` ∈ {6, 7}, `source_channel` ∈ {5, 6} and `state->subband_activity[source_channel]` reads past the 5-entry array (`SMS_DTS.c:109`). In struct order the bytes read are `vq_start_subband[0]`/`[1]` (intra-struct, values 1–32), and the read value bounds the loop at `:11406-11408`.
- What falls: (1) the −1 arm — `source_channel == −1` requires `joint_intensity[j] == 0`, but the sink is guarded by `if (state->joint_intensity[j] > 0)` at `:11404`, so −1 is unreachable on this path (the finding's "used unguarded on this path" is wrong). (2) "the read value bounds a **write** loop" — the loop body (`:11410-11417`) only calls `_dts_inverseq`/`bitstream_get` and **discards the results**; it consumes bits, it writes nothing. The real consequence is bit-stream desynchronisation steered by an intra-struct OOB read, not a steered write.
- Narrower restatement: crafted `joint_intensity` 6/7 → intra-struct OOB read of `subband_activity[5..6]` whose value controls how many scale-factor codes are skipped for channel `j`.

---

### SMS-142 — WMA `sample_rate == 0` — **WEAKENED**

- Locations: store at `SMS_WMA.c:1244` exact; no zero check exists anywhere in `wma_init` (full `sample_rate` grep: comparisons are `<= 16000`, `>= 44100`, etc. — none reject 0). The rate is container-supplied (`SMS_ContainerASF.c:675` reads it with `File_GetUInt`, unchecked).
- What stands: two **integer** divisions use it as divisor — `:1352` (v1 band positions, `b = sample_rate`) and `:1383` (v2 fallback, `/ (4 * b)`). Both are reached with rate 0: v1 directly; v2 because rate 0 selects no band table (`:1366-1371` all false) and falls into the computed branch.
- What falls: (1) the third claimed division, `:1397`, is **floating-point** — `(block_len * 2 * high_freq) / sample_rate` with `high_freq` a `float` (`:1236`) — float division by zero yields inf, and the `(int)` cast is UB but is not an integer divide; the finding's "divides by zero three times" overcounts. (2) "traps during decoder init" is an x86 intuition: on the MIPS target (EE/IOP) integer `div`/`divu` by zero does **not** raise an exception — the quotient/remainder are UNPREDICTABLE. The honest consequence is garbage band positions / `high_band_start` propagating into the band tables, not a guaranteed init trap. (The finding correctly excludes the float `bps` division at `:1291`.)
- Narrower restatement: rate 0 is unvalidated and reaches two integer divisions by zero (UB/unpredictable results on MIPS, trap only on trapping ISAs) plus float inf propagation at `:1291`/`:1397`.

---

### SMS-147 — FLAC fixed-predictor warm-up — **VERIFIED-DRIFT**

- Locations: reads exact at `SMS_FLAC.c:180-183`; order selection is at `:286-287` (`type >= 8 && type <= 12` → `decode_subframe_fixed(..., type & ~0x8)`, orders 0–4) — claimed ":285 region", off by 2.
- Mechanism: `a/b/c/d` are computed unconditionally before the order switch: order 0 reads `decoded[-1], [-2], [-3], [-4]`; order 1 reads `[-2], [-3], [-4]` (in `b`, `c`, `d`); order 2 reads `[-3], [-4]`; order 3 reads `[-4]`; only order 4 is fully in-bounds. The warm-up loop at `:176` writes `decoded[0..pred_order)` first but never touches negative indices. Allocation layout confirmed at `:391-392`: one `realloc`'d block, `m_pDec[1] = m_pDec[0] + MaxBlockSz` — so channel 0's negative reads precede the heap block and channel 1's land on channel 0's tail, exactly as filed. Values are unused for orders 0/1 but the reads happen regardless.

---

### SMS-148 — DXSB scratchpad — **VERIFIED-DRIFT**

- Locations: header use `:199-205` exact, PTS consumed `:215` exact, sink `mips_memset(lpTmp + lX, …, lLen)` at `:274` exact; `SMS_SPR_FREE = 0x70001B30` at `include/SMS.h:120`. Drift: the `dxsb_pack` read-back is at `:278`, not :281 (:281 is `lpDst += lRW`).
- Mechanism: `_decode` (`SMS_DXSB.c:195`, called from `SMS_DXSB_Decode:309` with the raw packet size) never checks `aSize >= sizeof(_DXSB_Header)` — the packed header struct is 53 bytes (`:24-34`: 27 + 7×2 + 4×3) — so a short packet is read as a header and `aSize -= 53` (`:213`) can go negative before any decode. `lW`/`lH` are unbounded u16s. Per row, `lLen` is clamped to the remaining row width (`:272`, checked — the finding credits this), so writes span exactly `[lpTmp, lpTmp + lW)`; the scratchpad free region from 0x1B30 to the 16 KB scratchpad top (0x70004000) is 0x24D0 = 9,424 B, so any `lW > 9424` writes RLE runs past scratchpad RAM, and `dxsb_pack(lpDst, lpTmp, lRW)` (`:278`, `lRW` = `lW` aligned up to 32) reads the same overrun region back into the heap pixmap. A moderate `lW` (e.g. 10,000) keeps the `memalign` at `:202-204` small enough to succeed, so the NULL-check on `retVal` does not save it.

---

### SMS-151 — SMSUMS `_get_direntry_start_cluster` ignored return — **WEAKENED**

- Locations exact: function `ums_fat.c:551-603`, discarded call at `:566`, locals declared `:556-557`, `CacheRead` at `:573`; second site `_next_direntry` call at `:652`. These are the **only two** call sites of `_get_direntry_sector_data` in the file (grep), so "only family" holds.
- Callee check (as tasked): `_get_direntry_sector_data` returns −1 at `:403` on chain failure. But it does **not** return "without writing" both outputs: `*apStartSec` is written at `:400` (`cluster2sector` of the start cluster) *before* the chain call at `:401`, on every path that can fail (non-FAT32 root returns 0 with both outputs written at `:394-395`; FAT32 root also passes through `:400`). Only `*apDirSec` is left unwritten on the −1 path.
- Consequence, narrowed: in `_get_direntry_start_cluster`, `lDirSec` (int) is genuinely uninitialised and controls the loop at `:568` — negative garbage ends the search benignly (−ENOENT), positive garbage scans sectors starting from the *legitimate* `lStartSec`, with cluster-boundary sector jumps derived from the stale/partial `m_ClusterBuf` at `:571`. In `_next_direntry`, `lDirSector` (unsigned) is uninitialised with the same extent effect from a legitimate `lStartSector`. The finding's "loops over uninitialised locals … from unintended disc locations when the garbage start is positive" overstates the start-sector half: the start is the directory's real first sector, not garbage. Core defect (ignored error return + uninitialised loop bound on the device-error path) stands.

---

### SMS-153 — AAC `huffman_getescape` — **WEAKENED**

- Locations exact: function `SMS_AAC.c:6559-6575`, loop `:6570`, uses `:6571-6572`; reached from codebook-11 spectral values ±16 at `:6597-6598` (claimed :6598-6602).
- What stands: there is no cap on the 1-bit run; the escape width `i` is used as the `SMS_GetBits` width and in `1 << i`. For exit `i >= 32`, `SMS_GetBits` reaches `SMS_NEG_USR32`'s `>> (32 − aN)` with a negative/wrapped shift count (UB; `include/SMS.h:68`) and `1 << i` is a ≥32 shift of a 32-bit int (UB) — independently of SMS-127's missing end check, as filed.
- What falls: "`i` is incremented … makes `i` arbitrarily large" — `i` is declared **`uint8_t`** at `:6560` (`uint8_t neg, i;`). It wraps modulo 256: a run of length `r` exits with `i = (4 + r) mod 256`, so the UB window is exactly exit `i ∈ [32, 255]` (run lengths 28–251, 284–507, …); a run of ≥ 252 ones wraps `i` back below 32 and produces a wrong-but-ordinary escape value instead of an ever-larger shift. The defect and its fix (cap the run) are unchanged; the counter's range is bounded, not arbitrary.

---

### SMS-161 — DTS scale-factor quantizer zero tails — VERIFIED-DRIFT

Script extraction (brace-matched, hex literals parsed as integers):

| Table | Declared | Actual count | Index 62 / 124 value | Tail values | Monotonicity before tail |
|---|---|---|---|---|---|
| `scale_factor_quant6` (`SMS_DTS.c:1108-1125`) | [64] | **64** | [62] = 8,317,638 = `0x007EEAC6` | [63] = **0** | non-decreasing over 0–62; only violations of strict increase are [1]=[2]=2 and [3]=[4]=3 |
| `scale_factor_quant7` (`SMS_DTS.c:1127-1160`) | [128] | **128** | [124] = 8,317,638 = `0x007EEAC6` | [125]=[126]=[127] = **0, 0, 0** | non-decreasing over 0–124; strict-increase violations are the repeated small values at indices 0–1, 2–4, 6–7, 9, 13 |

- **Drift 1 (wording):** the original claims both tables are "strictly increasing" over 0–62 / 0–124. They are not: both contain repeated values among the small entries (quant6: 2,2 and 3,3; quant7: 1,1; 2,2,2; 3,3; 4,4; 7,7). The correct invariant is **non-decreasing**, and the zero tails are still the *only* decreases in either table — so the defect inference (tails break the table's own monotonic structure) survives with the weaker, correct invariant.
- **Drift 2 (lines):** table locations :1108-1125 and :1127-1160 are exact, and the LFE consumer at :11440 is exact. The subband consumers are at **:11382** (`scale_table[scale_sum]` for `[0]`) and **:11393** (for `[1]`), not :11383/:11397 as cited; the direct bitstream reads are at **:11379** (`bitstream_get(state, 6)`, mode 5) and **:11381** (`bitstream_get(state, 7)`, mode 6), not :11380/:11382.
- **Reachability re-checked:** mode 5's 6-bit read yields 0–63, so index 63 is a legal direct value; mode 6's 7-bit read yields 0–127, so 125–127 are legal; table select at :11369-11370 exact (mode 6 → quant7, else quant6 — note modes <5 use quant6 via the cumulative path, SMS-138's territory, not this finding's). LFE at :11440 indexes quant7 with an 8-bit read: 125–127 are the in-bounds wrong-value cases; 128–255 is SMS-139's overrun, correctly not re-claimed.
- **Duplicate hunt:** whole-tree grep for `scale_factor_quant` returns only the two definitions and three uses, all in `src/SMS_DTS.c` — no second in-repo copy with different tails exists.
- **Verdict:** VERIFIED-DRIFT — defect, counts, tail values, maximum, and reachability exact; "strictly increasing" corrected to non-decreasing and four consumer/bitstream lines corrected by −1 to −4.

---

### SMS-164 — `do_bind` missing PCB NULL check — WEAKENED

- **Locations claimed vs actual:** `api_msg.c:389-435` exact: UDP branches :393-405, TCP branch :409-410, first dereferences `udp_setflags` (macro `udp.h:90`: `((pcb)->flags = (f))`) at :394/:399, `udp_recv` at :395/:400/:404, `setup_tcp` → `tcp_arg` (macro `tcp.h:62`: `pcb->callback_arg = (arg)`) at :410 via :213, and `udp_bind`/`tcp_bind` at :423/:428. Pool sizes exact: `MEMP_NUM_TCP_PCB`/`MEMP_NUM_UDP_PCB` = 15 each (`lwipopts.h:22, 26`).
- **"Only handler" census (full):** `udp_new()`/`tcp_new()` are called in exactly three API handlers — `do_newconn` (:293-326), `do_bind` (:393-410), `do_connect` (:461-496). `do_newconn` NULL-checks all four allocation branches (:294, :303, :312, :322); `do_connect` NULL-checks all four (:462, :472, :482, :493); `do_bind` checks **none** of its four. The asymmetry claim is exactly true.
- **Reachability — where the original overstates:** the allocation branch is entered only when `msg->conn->pcb.tcp == NULL` (:389). Tracing every `netconn` construction: (i) `netconn_new_with_proto_and_callback` (`api_lib.c:121-166`) sets `pcb.tcp = NULL` at :136 but immediately posts `API_MSG_NEWCONN`, whose checked `do_newconn` allocates the PCB — and if that allocation fails, `conn->err != ERR_OK` sends the constructor to its `error:` path (:162-141), freeing the conn, so no live conn with a NULL PCB and no error reaches `netconn_bind`; (ii) `accept_function` (`api_msg.c:220-273`) constructs only TCP conns with `pcb.tcp = newpcb` already set. The **only** in-tree producer of a live NULL-PCB conn is `err_tcp` at `api_msg.c:183` (`conn->pcb.tcp = NULL`), which exists only for TCP. Hence: the **TCP branch is reachable** exactly as the original's INFERRED note says (abort → PCB NULLed → re-bind via `lwip_bind`/`netconn_bind` → `tcp_new()` fails under exhaustion → `setup_tcp` writes through NULL). The **UDP branches (:393-405) are not reachable by any in-tree lifecycle**: no code ever NULLs `pcb.udp` on a live conn (grep: the only `pcb.* = NULL` writes in the stack are `api_lib.c:136` and `api_msg.c:183`, both TCP), and a fresh UDP conn always arrives at bind already allocated by `do_newconn`.
- **Verdict:** WEAKENED — narrow to: the missing-check defect and the crash are proven for the TCP re-bind-after-`err_tcp` path; the UDP branches carry the identical unchecked code but are latent/unreachable in-tree, not a live crash path. The fix (copy the sibling guard into all branches) is unchanged and still correct as defence in depth.

---

### SMS-180 — USBD `initCallbackThread` unchecked, result discarded — VERIFIED-DRIFT

- **Locations claimed vs actual:** core site exact — `driver.c:239-249`: `callbackTid = CreateThread(&thread)` at :246, `StartThread(callbackTid, NULL)` at :247, `return 0` at :249, no test anywhere. Sole caller exact: grep returns only `hcd.c:485` (`initCallbackThread();`, result discarded, inside the `hcdInit` that returns 0 at :496).
- **Drift (supporting citations only):** the delivery path is cited as `driver.c:192-210` (`signalCallbackThreadFunc`) and `:212-237` (`callbackThreadFunc`). Actual: `signalCallbackThreadFunc` is **:184-200** (the `WakeupThread(callbackTid)` is at :199), and `callbackThreadFunc` is **:202-237** (the sole `userCallbackProc` invocation at :232-233). Both functions' roles are exactly as described — `signalCallbackThreadFunc` is passed as the completion callback at `interface.c:184, 189`, and `callbackThreadFunc` is the only code that invokes `userCallbackProc` (grep over USBD: only `driver.c:232-233` calls it; `interface.c:171` only stores it).
- **Verdict:** VERIFIED-DRIFT — defect, sole-caller, and sole-delivery-route claims all reproduced; two supporting ranges corrected (−8/−10 lines).

---


## New candidate findings (SMS-183 upward)

Two candidates filed, both from the Round 8–9 verification batch, both parent spot-checked at their cited sites. Reserved ranges SMS-183…189, SMS-190…209 and SMS-210…229 were left unused (no candidates in those batches; dissolved shapes are recorded in the section files).

### SMS-230 — SMSTCPIP `lwip_getpeername` / `lwip_getsockname` ignore `netconn_peer` / `netconn_addr`'s return: a guarded error becomes a successful garbage result (and SMS-170's fix alone does not save `lwip_getsockname`) / Category 6
- **Locations:** `iop/SMSTCPIP/src/sockets.c:827` (`lwip_getpeername` discards `netconn_peer`'s return), `:834` (uses uninitialised `naddr.addr`), `:840-841` (returns 0); `iop/SMSTCPIP/src/sockets.c:861` (`lwip_getsockname` discards `netconn_addr`'s return), `:868` (dereferences `naddr`), `:874-875` (returns 0); guarded callees `iop/SMSTCPIP/src/api_lib.c:219, 226-227` (`netconn_peer` returns `ERR_CONN` without writing its outputs when the PCB is NULL).
- **Problem:** `netconn_peer`'s PCB-NULL guard (the sibling guard SMS-170 is measured against) protects the callee, but its only sockets-layer caller throws the result away. In `lwip_getpeername`, `naddr` is an uninitialised stack `struct ip_addr` (:814); when the PCB is NULL (post-`err_tcp` abort, `api_msg.c:183`) or a UDP conn is unconnected, `netconn_peer` returns `ERR_CONN` without writing `*addr`/`*port`, and the caller proceeds to copy the uninitialised `naddr.addr` into the result sockaddr and return success with `sock_set_errno(sock, 0)`. The mirror defect in `lwip_getsockname` matters for SMS-170's remediation: even after `netconn_addr` gains the proposed guard and returns `ERR_CONN` without setting `*addr`, `lwip_getsockname` ignores that return and dereferences its uninitialised `naddr` pointer at :868 — the crash moves, it does not close. (The third `netconn_peer` caller, `lwip_sendto` at `sockets.c:452`, does check `== ERR_OK` — the correct shape exists in the same file.) Distinct from SMS-170 (missing guard inside `netconn_addr`, `api_lib.c`) and SMS-166 (`lwip_accept`'s NULL `newconn`): this is the caller-side ignored-return defect in `sockets.c`.
- **Fix:** Check both returns: on `!= ERR_OK`, `sock_set_errno(sock, ENOTCONN)` (or the recorded `conn->err`) and return −1, mirroring `lwip_sendto:452`'s existing test. For `lwip_getsockname` this check is required *together with* SMS-170's guard, not instead of it.
- **Benefit:** Post-abort / unconnected `getpeername`/`getsockname` return an error instead of success-with-garbage (or, after a partial SMS-170 fix, a relocated crash).
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ — callee guard, both call sites, and the checking sibling all line-read; INFERRED only for the abort-then-query ordering, as in SMS-170).

### SMS-231 — USBD `hcdInit` never checks the HCD IRQ thread's creation: `hcdTid` failure leaves USB interrupts waking a nonexistent thread while `hcdInit` returns 0 / Category 6
- **Locations:** `iop/USBD/src/hcd.c:464-482` (`static int hcdTid;` :464, `hcdTid = CreateThread(&thread)` :472 unchecked, `RegisterIntrHandler(…, iWakeupThread, (void*)hcdTid)` :476, `StartThread(hcdTid, NULL)` :482 unchecked), unconditional `return 0` at `hcd.c:496`.
- **Problem:** This is a third unchecked-thread site in `hcdInit`, distinct from SMS-180's callback thread (`driver.c:246`, called at `hcd.c:485`) and from SMS-162's arena allocation (`hcd.c:340`): the thread created here is `hcdIrqThread`, the thread the USB interrupt handler wakes to process HCD interrupts. If `CreateThread` fails (IOP thread exhaustion), the negative error id is stored in `hcdTid` — which also defeats the `if (!hcdTid)` retry guard at :466, since a negative id is truthy — `RegisterIntrHandler` registers a wakeup target that does not exist, `StartThread` is called on the invalid id with its result ignored, and `hcdInit` still returns 0. USBD loads resident with interrupts enabled (:494) and no IRQ thread: no interrupt is ever serviced. Neither SMS-162 (allocation + return-value masking in general) nor SMS-180 (callback thread specifically) cites this site or this thread.
- **Fix:** Test `hcdTid` after `CreateThread` (and `StartThread`'s result) in `hcdInit` and return an error on failure, so `_start` can fail the module load — the same propagation SMS-162/SMS-180 already require of this function.
- **Benefit:** Thread exhaustion at USBD load becomes a visible load failure instead of a resident USB stack whose interrupt path is dead on arrival.
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ for the missing checks, the truthy-negative retry-guard interaction, and the unconditional return; INFERRED only for kernel-call failure modes, as in SMS-180).

---

**Verification close-out:** 75 §3(b) SMS Category 6 findings re-verified at pin `e65ced39a27eee8d996e9257e21594d1cac1c93d` — VERIFIED 50 · VERIFIED-DRIFT 15 · WEAKENED 9 · REFUTED 1 — plus 2 new candidates (SMS-230, SMS-231). Clone git-clean at end; read-only held throughout. Full reasoning for the 50 VERIFIED findings: `verify-sections/verify-sms-{A,B,C,D}.md`. Recommended index actions (for the parent, not applied here): SMS-015 → REFUTED status; SMS-115/-116/-117/-133/-140/-142/-151/-153/-164 → WEAKENED annotations with the narrowed conditions above; the 15 VERIFIED-DRIFT findings' citations corrected to the lines given in their reasoning blocks.