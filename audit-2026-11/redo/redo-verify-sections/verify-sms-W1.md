# Verification — SMS W1 (RE-SMS-001 … RE-SMS-024)

Verifier: adversarial, refute-first. Clone: `~/workspace/audit-clones-2026-11-13/Simple-Media-System` at `e65ced39a27eee8d996e9257e21594d1cac1c93d` (read-only; `git status` clean after work). Every cited file/line below was read in the clone; every table/count was re-derived by script, not eyeballed.

## RE-SMS-001 — REFUTED
- Read: `src/SMS_Container.c:45-60` (table: 12 creators + NULL = 13 entries), `:149-165`; `src/SMS_FileDir.c:124-185` (`SMS_ContID` returns −1, exact).
- Defeating evidence: the guard is `( lID = SMS_ContID(...) ) && ( lID < sizeof(s_CCreator)/sizeof(s_CCreator[0]) )`. The right operand is `size_t`, so per C's usual arithmetic conversions `lID` (−1) is converted to unsigned (4294967295 on the 32-bit target) and `4294967295 < 13` is **false**. The branch is not taken for −1; execution falls to the `else memcpy(lCC, s_CCreator, …)` default order. `s_CCreator[-1]` is never read or called.
- Proof: compiled the identical expression shape (`int lID=-1; (lID) && (lID < sizeof(arr)/sizeof(arr[0]))`) — result 0 for −1, 1 for 5, 0 for 0. The finding paraphrased the test as `lID < 13` in signed arithmetic, which is not what the code computes. The proposed fix is therefore unnecessary (and its `0 < lID` half would change nothing either, since lID==0 already falls through).

## RE-SMS-002 — VERIFIED
- Read: `src/SMS_MPEG4.c:933` (`m_nSpriteWarpPts = SMS_GetBits(...,6)`, 0–63, no check), `:1167-1201` (`int lD[4][2]`, loop `i < m_nSpriteWarpPts` writing `lD[i][0..1]`).
- Grep for all other uses of `m_nSpriteWarpPts` (`src/SMS_MPEG4.c:1236,1371`, `include/SMS_MPEG.h:200`): no clamp anywhere between parse and use. Mechanism, lines and figures hold. Fix (reject >4 at parse) is one comparison, LESS-compliant.

## RE-SMS-003 — VERIFIED
- Read: `iop/SMSTCPIP/src/ip_frag.c:182-227` (swap at :198-199 `(ip_reasslen<<8)|(ip_reasslen>>8)`, then swapped value to `pbuf_alloc(PBUF_LINK, ip_reasslen, PBUF_POOL)` :213 and copy bound :222); `iop/SMSTCPIP/src/pbuf.c:176-185` (PBUF_POOL returns a single pool pbuf, sets `p->len = p->tot_len = length` blindly); `lwipopts.h:63,121` (`PBUF_POOL_BUFSIZE 1540`, `IP_REASSEMBLY 1`).
- Re-derived example: 300 = 0x012C → swapped 0x2C01 = 11,265 ✓. `ip_reassbuf` is 5780 bytes (`ip_frag.c:88`), pool buffer 1540 — the copy overruns both as claimed. Fix (host-order length for alloc/copy, drop >1540) is sound and LESS-compliant.

## RE-SMS-004 — VERIFIED
- Read: `iop/SMSUMS/src/ums_fat.c:21` (`FAT_MAX_NAME 128`), struct at :124/:137 (`m_Name[128]`), LFN branch :421-441, `_set_dir` `strcpy` at :513.
- Re-derived: `(seq & 0x3F) − 1) * 13` max = (63−1)*13 = **806**, plus 13 chars/entry → writes to ~819 into a 128-byte field ✓. No bound check present. (Strengthening the finding did not note: seq 0 gives lOff = −13, a write *before* the buffer.) Fix is a single range check at the trust point — sound.

## RE-SMS-005 — VERIFIED-DRIFT
- Mechanism holds: `src/SMS_DTS.c:26` (`DTS_PRIM_CHANNELS_MAX 5`); script re-count shows every per-channel state field at :101-117 sized `[DTS_PRIM_CHANNELS_MAX]`; `:10756` `prim_channels = bitstream_get(3)+1` (1–8) with loops at :10758-10813 writing those fields; subframe-header loops :11329+. Upstream defeat hunt: `_dts_syncinfo` (:10103-10160) validates sample-rate/bit-rate/flags only — it never sees `prim_channels` (parsed later in `_dts_frame`), so no earlier validation saves this.
- Drift: the stack array is declared at **:11168** (`float subband_samples[DTS_PRIM_CHANNELS_MAX][8][32]` in `_dts_subsubframe`), not :11171 (:11171 is the `quant_step_table` assignment); its indexing loop is :11174 as cited. Fix (reject >5 after :10756) is sound.

## RE-SMS-006 — VERIFIED
- Read verbatim: `src/SMS_EE.c:946-965` — mismatch branch frees and NULLs `retVal`, then `lzma2_uncompress(..., UNCACHED_SEG(retVal), lSize)` is called unconditionally and its return discarded; `src/lzma2.c:84-122,124-155`. Caller `SMS_InitVU` (:969-978) does check `if (lpData)`, so the proposed `return NULL` fix matches the existing contract. Also holds for the initial-malloc-failure path (retVal already NULL).

## RE-SMS-007 — VERIFIED
- Read: `src/SMS_IOP.c:237-243` (`ret = lzma2_uncompress(...); if (ret > 0) ret = SifExecModuleBuffer(...)`; `memalign(64, irx_size)` at :239 unchecked; `int irx_size` at the function head receiving a `size_t`); `include/xz.h` enum (:112-122: `XZ_OK=0, XZ_STREAM_END=1, XZ_UNSUPPORTED_CHECK=2 … XZ_BUF_ERROR=8`) and the single-call undefined-output note at :209-217.
- Every error value (2–8) satisfies `ret > 0`, so corrupt/truncated blobs are executed. Fix (`ret == XZ_STREAM_END`, NULL check, size_t) is three lines, LESS-compliant.

## RE-SMS-008 — VERIFIED
- Read: `iop/SMSCDVD/SMSCDVD.c:858-863` (`_copy_name`: `strncpy(apName, …, 128)` then `apName[lLen] = 0` with `lLen` = the record's length byte, ≤255); `:1085-1091` (`TocEntryCopy`, same pattern); destination `iop/SMSCDVD/SMSCDVD.h:96` (`char m_Filename[129]`). Joliet branches halve the length (≤127) and are safe as claimed. Walk loops at :525-527/:565-567/:713-715 advance by `m_Length` special-casing only 0, as claimed.

## RE-SMS-009 — VERIFIED-DRIFT
- Mechanism holds: `iop/SMSTCPIP/src/sockets.c:104-115` (`&sockets[aSD]` with no range check, only a `conn` NULL test on the already-OOB pointer); `NUM_SOCKETS = MEMP_NUM_NETCONN = 15` (`sockets.c:52`, `lwipopts.h:45`); `FD_SETSIZE 16` (`include/lwip/sockets.h:197`); `lwip_selscan` (:523-551) loops caller-supplied `maxfdp1` with no clamp in `lwip_select` (:568-606). `lwip_close` writes through the returned pointer, as claimed.
- Drift: call-site count. My grep finds **17 call sites** in `sockets.c` (:152,183,203,235,262,295,325,397,445,540,551,736,816,850,881,1077,1261) plus the definition — not "all 19 call sites". The fix (one bounds check in `get_socket`; all callers already handle NULL — verified at the sites read) is sound.

## RE-SMS-010 — VERIFIED-DRIFT
- Script-counted tables: `bitalloc_12[5]` (`src/SMS_DTS.c:189`), `scale_factor_quant6[64]` (:1108), `scale_factor_quant7[128]` (:1127), downmix `table[11][10]` (:10203).
- Sinks verified: bitalloc selector 7 — :11340-11343 divert 5 and 6 only, :11344-11346 pass `bitalloc_12[7]` (OOB pointer) to `_dts_inverseq`, which dereferences `huff[j]` immediately (:10083-10101) ✓; LFE `scale_factor_quant7[bitstream_get(8)]` (0–255 into [128]) at :11440 ✓; cumulative `scale_sum` (signed Huffman deltas from `scales_129`, accumulated :11374-11392 with no bound, `scalefactor_huffman==7` falling through all branches and reusing a stale sum) ✓.
- Drift/correction: (1) the `scale_table[scale_sum]` reads are at **:11382 and :11393**, not :11386/:11402 (:11386 is the second accumulation, :11402 is the joint-intensity loop head). (2) The downmix description understates the sink: at :10261 `table[flags & DTS_CHANNEL_MASK][input]`, *both* indices are unchecked — `input` is `amode` (0–63 into [10]), and the row index `*flags & 0x3F` can itself reach 63 into [11] on the `_dts_decode` else-branch (`src/SMS_DTS.c:11567-11571`, `lFlags |= DTS_ADJUST_LEVEL` leaves the syncinfo 6-bit field in the low bits). Mechanism and fix direction hold and are, if anything, worse than claimed.

## RE-SMS-011 — VERIFIED-DRIFT
- Mechanism holds: `src/SMS_DTS.c:11644` `aLen = DTS_SyncInfo(...)` (header frame size), `:11645` `retVal = lnSamples << 2`, allocation `retVal + 80` at :11647, byte-swap asm copy of `aLen` bytes at :11670-11691, `memset(lpOutBuf + 8 + aLen, 0, retVal - 8 - aLen)` at **:11693**. Nothing reconciles `aLen` with `retVal`.
- Drift: cited lines are :11651/:11652/:11694 in the finding; actual are :11644/:11645/:11693. Figures: `aLen` max is **16,384** (14-bit field + 1 at `_dts_syncinfo`), not 16,383 — and in non-word mode it is further scaled (`*8/14*2`). `retVal` "at most 8,192" holds only for the largest *valid* frame type (2048 samples); the bitstream field itself allows (127+1)*32 = 4096 samples → 16,384. The negative-`memset` conclusion is unchanged (the two fields are independent). Fix (bail when `aLen > retVal - 8`) is sound.

## RE-SMS-012 — VERIFIED
- Read: `src/SMS_DXSB.c:24-34` (header, packed u16 width/height), `:195-204` (`_decode` dereferences `lpHdr->m_Width/m_Height` and computes the allocation before any use of `aSize`; `aSize` is only decremented at :213, after the header read), `:209` (`lpTmp = SMS_SPR_FREE`), `:274` (actual `mips_memset(lpTmp + lX, …, lLen)`; finding cites :272, the `lLen` clamp two lines above — same statement block), `:278` (`dxsb_pack(lpDst, lpTmp, lRW)`).
- Re-derived capacity: `SMS_SPR_FREE` = 0x70001B30 (`include/SMS.h:120`), scratchpad ends 0x70004000 → 0x24D0 = **9,424** bytes ✓. Width > ~9400 overruns it as claimed. Fix (two entry comparisons) is LESS-compliant.

## RE-SMS-013 — VERIFIED-DRIFT
- Every listed site was opened and shows the claimed unchecked use on the next line(s): `SMS_Container.c:351-352` (malloc → Read into it), `SMS_ContainerAAC.c:197/200/212`, `SMS_ContainerAC3.c:109-112`, `SMS_ContainerASF.c:157-159` and `:212` (stream at :206 *is* checked at :208, codec is not; first codec deref :261), `SMS_ContainerAVI.c:107` (stream checked :104, codec not) and `:1164-1166`, `SMS_ContainerFLAC.c:123-126,138`, `SMS_ContainerJPG.c:64-84`, `SMS_ContainerM3U.c:308-312,366-370`, `SMS_ContainerMP3.c:121-124`, `SMS_ContainerOGG.c:105-108`. The checked pattern in MOV (`SMS_ContainerMOV.c:203-206`) and MPEG-PS (`SMS_ContainerMPEG_PS.c:793-799`) is real, as claimed.
- Drift: AAC user-data write is at **:215** (`*(uint64_t*)lpCodecCtx->m_pUserData = lHdr`), not :218; FLAC struct copy is at **:141** (`*(FLACData*)lpCodecCtx->m_pUserData = lData`), not :146. Mechanism and per-site fix shape hold.

## RE-SMS-014 — VERIFIED-DRIFT
- Mechanism holds: `src/SMS_Config.c:696-702` and `:727-732` (4-byte version check, then 892 raw bytes into `g_Config`); consumers verified — `src/SMS_CDVD.c:124-126` (`sl_Speeds[3]` by `m_CDVDSpeed`, an `unsigned char` at `include/SMS_Config.h:129`, so up to 255), `src/SMS_GUIMenuSMS.c:619,1806` (`s_Speeds[3]`), and `g_Palette[g_Config.m_*Idx - 1]` reads (fields are `unsigned int`, `include/SMS_Config.h:101-122`; index 0 → `g_Palette[-1]`, large → far OOB).
- Drift: (1) the only post-load guards are at **:766** (`m_SkinName` NUL) and **:768** (`m_PlayerAC3RL`), not :773/:775 (:773/:775 are the SMB-flag and EMBEDDED lines). (2) My script counts **22** `g_Palette[g_Config.m_*Idx - 1]` read sites, not 26. (3) **The proposed fix is wrong at the boundary**: `g_Palette` is `unsigned int g_Palette[16]` (`src/SMS_Config.c:51`), and the defaults use indices up to 16 (:602,611-618) — the valid range is **1..16**, so the finding's "clamp colour indices to 1..17" would still permit `g_Palette[16]`, one past the array. Mechanism VERIFIED; fix must be corrected to 1..16.

## RE-SMS-015 — VERIFIED
- Read: `src/SMS_Config.c:69` (`char g_SaveDiag[64]`), `:64` (`char s_pMC0SMC[128]`), `:943,945,1082-1087` (three path-carrying `sprintf`s), TEMP self-description at :66-68/:883, consumer `src/SMS_GUIMenuSMS.c:1007`. Worst case ≈ 19-char prefix + up to 127-char path + digits ≫ 64 ✓. The IPCONFIG `lDiag[48]` comparison site was also checked (`SMS_GUIMenuSMS.c:2031-2090`, no path in format) as claimed. Deletion-or-`snprintf` fix is LESS-compliant.

## RE-SMS-016 — VERIFIED
- Read: `iop/SMSTCPIP/src/ip_frag.c:58-77` (`copy_from_pbuf` loops `for (; len; p = p->next)` with no NULL stop), `:143-159` (`len` from swapped `IPH_LEN` minus header, passed unchecked against the actual chain), `:151` (rejects only `offset + len > IP_REASS_BUFSIZE`), `:174,192,195`; entry `iop/SMSTCPIP/src/ip.c:227-230` + `pbuf.c:242-248` (`pbuf_realloc` returns immediately when `new_len >= p->tot_len` — it cannot grow, exactly as claimed).
- Re-derived bitmap bound by script: `ip_reassbitmap[5760/(8*8)]` = 90 bytes, indices 0–89 (`ip_frag.c:80,89`); at total exactly 5760, `(offset+len)/64` = `ip_reasslen/64` = **90** → one byte OOB at :174 (write) and :195 (read) ✓.

## RE-SMS-017 — WEAKENED
- Main mechanism VERIFIED: `iop/SMSCDVD/SMSCDVD_UDFS.c:293-347` — `lLEA`/`lLAD` are disc u32s (:293-294), `p = 176 + lLEA` (:295), loop reads `UDFAD(&apData[p], …)` with no bound and `p += lLAD` (:345); the `176 + lLEA + lLAD` sum can wrap. `UDF_DOpen` returns 1 at :552 and `UDF_DClose` returns 1L at :619, as claimed.
- Narrowing facts: (1) `apData` does not point into a 2,048-byte buffer — it is `&s_Buffer[12]` with `s_Buffer[9 * 2064]` = 18,576 bytes (`iop/SMSCDVD/SMSCDVD.c:196`); the OOB read needs a larger `lLEA` than claimed, though a u32 `lLEA` still reaches far past it, and reads within the slack still consume wrong-sector data. (2) The `UDFDecode` **write** overflow is not reachable at either real call site: at :138 `aLen` is the constant 128 into `m_VolDesc[128]` and the loops fit exactly (8-bit path writes ≤127 + NUL at [127]; 16-bit path ≤63 + NUL); at :399 `aLen = lLFI` is an `unsigned char` (≤255, :388-391) and the destination at both call sites (:441-442, :581-582) is `s_ScanTemp[2048]` (:69). The function is an unchecked contract, but no current caller can overflow it. What *is* reachable on that path is an OOB **read**: the source offset `38 + lLIU` with disc-controlled u16 `lLIU` (:395,399). True scope: allocation-descriptor OOB read (real) + latent decode contract + OOB source read; no demonstrated name-decode write overflow.

## RE-SMS-018 — VERIFIED
- Read: `iop/SMSUMS/src/ums_fat.c:605-632` — `unsigned char lName[257]` (:607), component copy `lName[lOff++] = apName[i++]` at :623 with no bound, terminator write at :626. A component >256 bytes overruns the stack buffer as claimed. Fix (`-ENAMETOOLONG` at 256) fits the function's existing error style.

## RE-SMS-019 — VERIFIED
- Read: `iop/SMSUMS/src/ums_driver.c:283-298` (`UmsGetMaxLUN` returns the device's 1-byte GET MAX LUN result, 0–255), `:214-257` (`for (i = 0; i <= lnLUN; ++i)` writing `m_LUName[i]`), table `iop/SMSUMS/include/ums.h:54` (`char m_LUName[16][19]`). lnLUN > 15 writes past the table into the rest of `USBMDevice` ✓. Fix (clamp to 15 at the read) is sound.
- Secondary correction: for an all-zero name, `lLen = strlen(...) - 1` = −1 (:234) and the trim loop at :236 already *reads* `m_LUName[i][-1]` (its condition tests `lLen` nonzero before the byte) — the −1 touch is not conditional on the dedup path as the finding states; the dedup writes at :247-248 then use lLen+1 = 0 and are in-bounds.

## RE-SMS-020 — VERIFIED
- Read: `iop/SMSUMS/src/ums_driver.c:300-321` (`UmsGetCapacity` stores `_int_be(&lBuf[4])` into `m_SectorSize` verbatim at :313, no check); divisions by it at `ums_driver.c:395`, `ums_cache.c:40`, `ums_fat.c:226-227` (`_root_dir_sectors`), and divisor forms `m_SectorSize >> 5` at `ums_fat.c:236` and `m_SectorSize / 32` at `ums_fat.c:395` (both zero for a sector size < 32, and the direct divisions trap on 0). BPB whitelist exists only for the BPB's own fields at `ums_fat.c:1048-1061`; `FATFS_Init` takes `lSecSize = apDev->m_SectorSize` at :989 and allocates `sizeof(FATFS) + lSecSize` at :1002 from the unvalidated device value ✓. Fix (whitelist 512/1024/2048/4096 at `UmsGetCapacity`) reuses the existing list — LESS-compliant.

## RE-SMS-021 — VERIFIED
- Read: `src/SMS_InverseCodePages.c:138-185` — loop writes while `length < bufSize` (up to `bufSize` bytes), then unconditionally `*outBuff = '\0'` at :184, i.e. at `outBuff[bufSize]`; callers `src/SMS_SubtitleContext.c:483` (passes `SUB_MAXLEN` = 512 for `char lBuff[SUB_MAXLEN]`, :33,482) and `:657-659` (passes the remaining in-line space); safe caller `DisplayUTF8` at `SMS_InverseCodePages.c:233` passes `aSize - 1`. Grep confirms these are all the callers. The finding's own scoping is accurate: current subtitle inputs are length-bounded elsewhere (`File_GetString(..., SUB_MAXLEN)`), so this is a latent contract defect, not a presently-triggered overflow — and it is claimed as exactly that. Fix (reserve the NUL inside the function) is one convention in one place.

## RE-SMS-022 — WEAKENED
- Tables script-counted: `num_swb_1024_window`, `swb_offset_1024_window`, `num_swb_128_window`, `swb_offset_128_window` each have exactly **12** entries (`src/SMS_AAC.c:2006,2056,2065,2093`); sinks at :6204-6220 index them with the raw `sf_index`; `get_sample_rate` returns 0 for index ≥ 12 (:5763-5770).
- ADTS path VERIFIED: `adts.sf_index` (4 bits, :5953) is stored at :6041 and `_NeAACDecInit` returns 1 unconditionally (:6049-6060) despite `*samplerate` becoming 0 — indices 12–15 reach the tables. (The ADIF path at :6030 has the same unchecked shape, unmentioned by the finding.)
- ASC path NARROWER than claimed: the *main* ASC index does **not** pass — `AudioSpecificConfig2` computes `samplingFrequency = get_sample_rate(index)` at :6095 and returns −2 when it is 0 at :6098, before any explicit-frequency handling, and `_NeAACDecInit2` checks `rc != 0` at :6182. Index 15 as the main ASC index is therefore rejected, contrary to the finding's "passes its validation because the explicit rate is non-zero". What survives is only the SBR sub-index: inside the object-type-5 / sync-extension branches (:6105-6110, :6132-6137) a fresh 4-bit `tmp` (12–15, with 15 taking the explicit 24-bit rate) overwrites `samplingFrequencyIndex` with **no** re-validation, and :6172 copies it to `hDecoder->sf_index` (the :6180 recompute does not fire when `downSampledSBR` is set). True scope: ADTS (and ADIF) fully exposed; ASC exposed only via the SBR extension index. The proposed fix (fail init when `get_sample_rate(sf_index) == 0` in both init paths) still closes all of these and is sound.

## RE-SMS-023 — VERIFIED-DRIFT
- Read: `src/SMS_AAC.c:110` (`MAX_CHANNELS 64`), `:333-334` (`exclude_mask[64]`, `additional_excluded_chns[64]`, inside `drc_info`, embedded in the heap-allocated decoder struct at :503), loop at `:7548-7561` (7 initial writes, then each continuation bit adds 7 more with no bound; `n` is `uint8_t`).
- Re-derived by simulation: continuation iterations write starting at 7,14,…,56 (all in-bounds), and the **9th** continuation iteration writes `exclude_mask[63..69]` — first OOB — not "after the 10th iteration" as claimed. `additional_excluded_chns[n-1]` stays in-bounds until much later; the `n` wrap at 255 also holds as claimed. Mechanism and fix (stop when `num_excl_chan + 7 > MAX_CHANNELS`) hold; only the iteration figure is off by one.

## RE-SMS-024 — VERIFIED
- Read: `src/SMS_WMA.c:1347-1361` (v1: `exponent_bands[0][i] = pos - lpos` with no `pos > lpos` test) vs `:1386` (v2 computed path has exactly that guard, compacting via `j++`); consumer `:1641-1660` (`n = *ptr++; do { *q++ = v; } while (--n);` twice — `n` is `int`, so a 0 entry makes `--n` = −1, −2, … and the loop effectively never terminates on its own).
- The finding's INFERRED reachability is now **proven by my simulation** of the v1 formula over `wma_critical_freqs[25]` (script-extracted: 100…24500): zero-width bands occur at e.g. sample_rate 44100 / block_len 64 (7 zero bands, first at index 2) and 128 (3), 48000/64 (8), 32000/64 (5, including band 0), 22050/64 (3), 16000/64 (1). Table-driven v2 claim also script-verified: all 9 `exponent_band_*` rows are count-prefixed with no zero entries after the prefix, and their entries sum to the block size (128/256/512). Fix (mirror the v2 guard in v1) is LESS-compliant.

---

## Duplicates within the range
None. RE-SMS-003 and RE-SMS-016 share `ip_reass` but are distinct mechanisms (swapped length used for alloc/copy vs chain-length trust + bitmap bound). RE-SMS-005 and RE-SMS-010 share the DTS header area but are distinct fields/sinks. RE-SMS-013 is a single family finding by design.

## Tally (24 findings)
- **VERIFIED = 14**: {002, 003, 004, 006, 007, 008, 012, 015, 016, 018, 019, 020, 021, 024}
- **VERIFIED-DRIFT = 7**: {005, 009, 010, 011, 013, 014, 023}
- **WEAKENED = 2**: {017, 022}
- **REFUTED = 1**: {001}
- **BLOCKED-ENV = 0**
- Total = 24 ✓

## Candidates (new defects noticed while verifying — not findings)
1. **DTS downmix row index** (related to RE-SMS-010, worse than its stated sink): `src/SMS_DTS.c:10261` — `table[flags & DTS_CHANNEL_MASK][input]` row index can reach 63 into an `[11]` table via the `_dts_decode` else-branch (`src/SMS_DTS.c:11567-11571`). Same sink/fix as RE-SMS-010; listed here because the finding attributes the sink to `amode` alone.
2. **UDFDecode 16-bit read past end + OOB source offset**: `iop/SMSCDVD/SMSCDVD_UDFS.c:99` (`apSrc[p += 2]` can read one element past `aLen`) and `:399` (source `&apData[38 + lLIU]`, `lLIU` a disc u16 at :395, into a 4096-byte `s_ScanBuf`) — read-side companion to RE-SMS-017.
3. **WMA v1 exponent rows 1+ never filled**: `src/SMS_WMA.c:1354` — the v1 branch always writes `exponent_bands[0][i]` for every `k`, while `decode_exp_vlc` selects the row by `frame_len_bits - block_len_bits` at `:1635`; for any smaller block size in a v1 stream it reads the zero-initialised rows of the static context, feeding the same zero-entry underflow as RE-SMS-024 without needing a repeated critical frequency.
4. **FAT LFN negative offset**: `iop/SMSUMS/src/ums_fat.c:421` — sequence byte 0 gives `lOff = -13`, writing before `fat_direntry.m_Name` (strengthens RE-SMS-004; same fix site).
