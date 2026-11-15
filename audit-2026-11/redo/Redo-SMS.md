# Blind Redo Audit — Simple-Media-System (SMS)

**Repo:** `NathanNeurotic/Simple-Media-System`, clone `~/workspace/audit-clones-2026-11-13/Simple-Media-System`
**Pin:** `e65ced39a27eee8d996e9257e21594d1cac1c93d` (master) — HEAD verified equal to the pin and `git status` clean before and after the audit. The clone was read-only throughout.
**Method:** Independent blind redo, from the code only — no prior audit material was read, listed, or grepped. Eight scoped passes covered: EE containers/demuxers; EE audio decoders (AC3/DTS/AAC/FLAC/MP123/WMA) with all 345 constant-table initialisers counted by script; EE video decoders/pipeline; EE player/GUI/support; the IOP SMSTCPIP (lwIP) stack; the other IOP modules (SMSCDVD, SMSUMS, USBD, SMSSMB, smbman-sms, SMSUdpfs, SMSDev9, SMSMAP, SMSPWROFF); data tables/binary assets and the local LZMA2/XZ wrapper; and headers/build/CI/docs. The coordinator additionally re-read the primary evidence for seven headline mechanisms in the clone before accepting them (container creator-table index, MPEG-4 sprite warp count, ip_reass length swap, socket-table index, FAT LFN offset, DTS prim_channels, LZMA2 decompress-into-NULL, IOP module execute-on-error) — all confirmed as written.
**Rule applied throughout:** LESS, NOT MORE — remove / simplify / consolidate / fix. No new abstractions, frameworks, config, wrappers, fallbacks, retries, backups, logging, or future-proofing unless a finding's Justification shows a concrete problem and a net win. One mechanism = one finding, all locations listed.
**Totals:** 146 worker candidates consolidated to **144 findings** (two mechanism-level merges: the AVI `SMS_Realloc` call-site finding folded into the `SMS_Realloc` root-cause finding, RE-SMS-069; the legacy-SMB patch-list folded into the SMSSMB deletion finding, RE-SMS-102). Findings are sorted by benefit: memory-safety first, then correctness, then deletion/size wins, then consolidation, performance, and build/CI.

| Category | Count |
|---|---|
| 1 dead code | 16 |
| 2 duplication | 10 |
| 3 overengineering | 2 |
| 4 size/memory | 6 |
| 5 performance (structural) | 2 |
| 6 bugs/security | 97 |
| 7 build/CI/repo clutter | 6 |
| 8 half-finished/redundant features | 5 |
| **Total** | **144** |

Licences were not flagged (settled). Secret-like values: none were encountered in scope beyond CI's standard `GH_TOKEN: ${{ github.token }}` wiring, which carries no literal value. No finding proposes copying another project's code. Where a table value is questioned, the evidence is structural only (counts, monotonicity, simulation), never an invented replacement value.

---

## Findings

### RE-SMS-001 — Unknown extension makes the container probe chain call `s_CCreator[-1]`

> **WITHDRAWN (Stage B):** The guard at SMS_Container.c:152-153 compares `int lID` against a `size_t` sizeof expression, so −1 converts to unsigned 4294967295, the branch is never taken, and `s_CCreator[-1]` is never indexed.
- **Category:** 6 (bugs/security)
- **Locations:** `src/SMS_Container.c:149-167` (test at :152, table read at :158); `SMS_ContID` returns −1 for unrecognised extensions at `src/SMS_FileDir.c:124-185`
- **Problem:** The extension-hint branch in `SMS_GetContainer` is taken when `(lID = SMS_ContID(path))` is non-zero and `lID < 13`. `SMS_ContID` returns **−1** for every unrecognised extension — and −1 is both truthy and `< 13`, so the branch runs with `lCC[0] = s_CCreator[-1]`: an out-of-bounds read of the creator table whose value is then *called* as the first probe. Depending on the word the linker placed before the table, an ordinary file with an unlisted extension (`.wav`, `.ts`, `.mkv`, no extension) either crashes/wild-jumps or silently gets no probes at all. The `lID == 0` (AVI) case already falls through to the default order, which starts with AVI anyway, so excluding non-positive IDs loses nothing.
- **Fix:** Take the reorder branch only for `0 < lID < 12` (the number of real creators); all other values keep the existing default-order `memcpy`.
- **Benefit:** Removes a wild function-pointer call / silent no-probe path reachable by simply opening a file with an unlisted extension — the most ordinary untrusted input there is.
- **Effort:** S. **Risk:** Low. **Confidence:** READ (coordinator re-verified in the clone).

### RE-SMS-002 — MPEG-4 sprite trajectory: bitstream warp-point count overflows a 4-entry stack array
- **Category:** 6 (bugs/security)
- **Locations:** `src/SMS_MPEG4.c:933` (count read), `src/SMS_MPEG4.c:1167-1201` (use)
- **Problem:** `_decode_vol_header` stores `SMS_GetBits(apBitCtx, 6)` (0–63) into `g_MPEGCtx.m_nSpriteWarpPts` with no validation. `_mpeg4_decode_sprite_trajectory` declares `int lD[4][2]` on the stack and loops `for (i = 0; i < g_MPEGCtx.m_nSpriteWarpPts; ++i)` writing `lD[i][0]` / `lD[i][1]`. A stream declaring 5–63 warp points writes up to 59 int-pairs past a stack buffer. MPEG-4 allows at most 4 warping points, so any larger value is by definition a corrupt/hostile stream.
- **Fix:** Reject (or clamp-and-error) `m_nSpriteWarpPts > 4` where it is parsed at `src/SMS_MPEG4.c:933`. One comparison; no new machinery.
- **Benefit:** Removes a remotely triggerable stack smash in the main MPEG-4 path (any .avi/.mp4 with a GMC/static-sprite VOL).
- **Effort:** Trivial. **Risk:** Negligible — values >4 are outside the MPEG-4 spec. **Confidence:** READ (coordinator re-verified in the clone).

### RE-SMS-003 — ip_reass uses the byte-swapped total length to allocate and copy the reassembled packet
- **Category:** 6 (bugs/security)
- **Locations:** `iop/SMSTCPIP/src/ip_frag.c:198-199`, `:213`, `:222`; aggravated by `iop/SMSTCPIP/src/pbuf.c:176-185`
- **Problem:** On completion `ip_reasslen += IP_HLEN; ip_reasslen = swap16(ip_reasslen)` so the value can be stored with `IPH_LEN_SET`. The *swapped* value is then passed to `pbuf_alloc(PBUF_LINK, ip_reasslen, PBUF_POOL)` and used as the copy bound in `mips_memcpy(q->payload, &ip_reassbuf[i], min(q->len, ip_reasslen - i))`. Example: a 300-byte reassembled datagram (0x012C) becomes length 0x2C01 = 11,265. `pbuf_alloc` PBUF_POOL returns a *single* pool pbuf (capacity `PBUF_POOL_BUFSIZE` 1540) and blindly sets `p->len = p->tot_len = length`, so ~11 KB is copied from the 5,780-byte `ip_reassbuf` into a 1,540-byte pool buffer, corrupting the pbuf pool and adjacent memp pools. Any remote host that sends a complete fragmented IPv4 datagram triggers this; fragmentation is compiled in (`IP_REASSEMBLY 1`, lwipopts.h). Even with the swap fixed, any reassembled length >1540 overflows the same way because this port's PBUF_POOL allocation never chains.
- **Fix:** Keep a host-order `totlen` for allocation/copy; use the swapped value only in `IPH_LEN_SET`. Reject (drop) reassembled lengths > `PBUF_POOL_BUFSIZE` at the same point, or chain pool pbufs in `pbuf_alloc` — the drop is the LESS fix and matches the 1540-byte link MTU reality of this stack.
- **Benefit:** Removes a remotely triggerable IOP heap/pool overwrite on ordinary fragmented traffic.
- **Effort:** Small. **Risk:** Low — fragmented datagrams >1540 bytes are dropped instead of corrupting memory. **Confidence:** READ (coordinator re-verified in the clone).

### RE-SMS-004 — FAT long-filename sequence number trusted into a 128-byte name buffer (SMSUMS)
- **Category:** 6 (bugs/security)
- **Locations:** `iop/SMSUMS/src/ums_fat.c:421-441` (`_get_direntry` LFN branch); same trust chain consumed at `iop/SMSUMS/src/ums_fat.c:513` (`_set_dir` `strcpy`); buffer definition `iop/SMSUMS/src/ums_fat.c:21` (`FAT_MAX_NAME 128`), `:124`, `:137`
- **Problem:** For a FAT long-name directory entry, the write offset is computed from the on-disc sequence byte: `lOff = ((apLong->m_EntrySeq & 0x3F) - 1) * 13;` — up to 62×13 = **806** — and 13 name characters per entry are then written at `apDir->m_Name[lOff++]` into `fat_direntry.m_Name[128]`. The sequence number is never checked against the buffer (a legitimate LFN chain needs at most 20 entries, offset ≤ 247 — itself already past 128 for names > 128 chars, which this driver truncates nowhere). A crafted USB stick's directory sector therefore writes hundreds of bytes past a fixed struct during ordinary directory listing. `_set_dir` then `strcpy`s the assembled name onward unbounded. SMSUMS is a live module (packed USB_MASS slot; `src/SMS_IOP.c:688`).
- **Fix:** Clamp in `_get_direntry`: reject/skip an LFN entry whose computed `lOff + 13` exceeds `FAT_MAX_NAME - 1` (treat as end of chain), and bound the `_set_dir` copy to the destination size. No new mechanism — one range check at the point the disc byte is first trusted.
- **Benefit:** Removes a remotely craftable (malicious/corrupt media) IOP memory corruption in a shipped module.
- **Effort:** Small. **Risk:** Low — over-long names are already unrepresentable in a 128-byte field. **Confidence:** READ (coordinator re-verified in the clone).

### RE-SMS-005 — DTS: stream-controlled `prim_channels` (up to 8) overflows state and stack arrays sized for 5
- **Category:** 6 (bugs/security)
- **Locations:** `src/SMS_DTS.c:26` (`#define DTS_PRIM_CHANNELS_MAX 5`); `src/SMS_DTS.c:10756` (`state->prim_channels = bitstream_get(state, 3) + 1;` → 1–8, never validated); `src/SMS_DTS.c:10758-10813` (frame-header loops); `src/SMS_DTS.c:11171` (stack array `float subband_samples[DTS_PRIM_CHANNELS_MAX][8][32]`, indexed at :11174); `src/SMS_DTS.c:11329-11447` (subframe-header loops into `[5]`-wide state fields)
- **Problem:** Every per-channel array in the DTS state struct and the subsubframe stack buffer is sized `[DTS_PRIM_CHANNELS_MAX]` = 5, but the 3-bit frame-header field yields up to 8 and nothing in `_dts_frame`, `dts_subframe_header`, or `_dts_subsubframe` checks it. A crafted DTS frame with `prim_channels` 6–8 writes past the static decoder state and past a stack array. The neighbouring fields *are* clamped (`subband_activity`/`vq_start_subband` are capped to `DTS_SUBBANDS` at :10760-10768), so this one field is simply the missed case.
- **Fix:** In `_dts_frame`, immediately after :10756, return an error if `state->prim_channels > DTS_PRIM_CHANNELS_MAX` — one check, mirroring the clamp style already used two lines below. Values 6–8 have no valid DTS-core arrangement, so no valid stream is affected.
- **Benefit:** Closes a struct-overflow + stack-overflow reachable from any DTS file, in the decoder's hottest entry path.
- **Effort:** Trivial. **Risk:** None identified for valid streams. **Confidence:** READ (coordinator re-verified in the clone).

### RE-SMS-006 — `SMS_LZMA2Decompress` decompresses into the buffer it just freed (NULL) on size mismatch
- **Category:** 6 (bugs/security)
- **Locations:** `src/SMS_EE.c:946-965` (sole decompressor for `g_JPEGData`, `src/SMS_JPEGData.c:316-322`); `src/lzma2.c:84-122`
- **Problem:** Read verbatim: `retVal = SMS_SyncMalloc(m_Size)`; `lSize = lzma2_get_uncompressed_size(...)`; `if (lSize != m_Size) { free(retVal); retVal = NULL; }` — and then `lzma2_uncompress(..., UNCACHED_SEG(retVal), lSize)` is called **unconditionally**, with the NULL/freed pointer as the output buffer and the untrusted `lSize` as the output size. The return value of `lzma2_uncompress` is also discarded, so even on the match path a failed decode returns a buffer of undefined contents to `SMS_InitVU`, which DMA-sends it to a VU. The same path fires if the initial `SMS_SyncMalloc` fails (retVal already NULL).
- **Fix:** `return NULL` inside the mismatch branch (and on malloc failure); check `lzma2_uncompress(...) == XZ_STREAM_END`, freeing and returning NULL otherwise. Net change: a guard, not new machinery.
- **Benefit:** Removes a guaranteed write-through-NULL (via the uncached-segment alias) on any size disagreement between a blob's XZ index and its table entry — the exact failure the mismatch check was written to catch, currently converted from a clean NULL return into a crash/corruption.
- **Effort:** Trivial. **Risk:** Low — callers already handle NULL (`SMS_InitVU` checks `if (lpData)`). **Confidence:** READ (coordinator re-verified in the clone).

### RE-SMS-007 — `SifExecDecompModuleBuffer` executes the output buffer on *any* XZ decode error
- **Category:** 6 (bugs/security)
- **Locations:** `src/SMS_IOP.c:237-243` (the wrapper); `include/xz.h:112-122` (`enum xz_ret`); `src/lzma2.c:124-155` (`lzma2_uncompress`)
- **Problem:** `ret = lzma2_uncompress(...); if (ret > 0) ret = SifExecModuleBuffer(irx_data, irx_size, ...)`. In this vendored enum, `XZ_STREAM_END` is 1 — but so is every error greater than it: `XZ_UNSUPPORTED_CHECK`=2, `XZ_MEM_ERROR`=3, `XZ_MEMLIMIT_ERROR`=4, `XZ_FORMAT_ERROR`=5, `XZ_OPTIONS_ERROR`=6, `XZ_DATA_ERROR`=7, `XZ_BUF_ERROR`=8. In `XZ_SINGLE` mode a successful decode returns exactly `XZ_STREAM_END`; anything else means the output buffer's contents are undefined (stated in `include/xz.h:209-217`). The `ret > 0` test therefore passes corrupt, truncated, or mis-sized IRX blobs straight to `SifExecModuleBuffer` for execution on the IOP. The same function also never checks `memalign(64, irx_size)` (line 239) for NULL before decoding into it, and stores the `size_t` uncompressed size in an `int irx_size` (line 235), truncating any size the (unbounded, RE-SMS-053) index walk invents.
- **Fix:** Require `ret == XZ_STREAM_END` before executing; return an error otherwise. Check `irx_data != NULL`; keep the size in `size_t` (or reject `> INT_MAX`). Three-line change, no addition of mechanism.
- **Benefit:** A single corrupt/truncated embedded module (build-artifact damage, bad media read on the file-loaded paths) currently becomes *executed garbage on the IOP* instead of a clean load failure — for every one of the ~25 call sites of this function.
- **Effort:** Trivial. **Risk:** Low — call sites already treat a negative return as failure; making decode errors return negative matches the contract they assume (e.g. `SMS_IOP.c:724,1032` check `< 0`). **Confidence:** READ (coordinator re-verified in the clone).

### RE-SMS-008 — ISO9660 filename length trusted past a 129-byte TOC buffer (SMSCDVD)
- **Category:** 6 (bugs/security)
- **Locations:** `iop/SMSCDVD/SMSCDVD.c:1085-1091` (`TocEntryCopy`), `iop/SMSCDVD/SMSCDVD.c:858-863` (`_copy_name`); destination `iop/SMSCDVD/SMSCDVD.h:96` (`char m_Filename[129]`)
- **Problem:** In the non-Joliet branch both copies do `strncpy(dst, src, 128)` and then terminate with `dst[lFNameLen] = 0;` where `lFNameLen` is the directory record's own filename-length byte (up to 255). For any name longer than 129 bytes the terminator write lands past the buffer (up to 126 bytes past `TocEntry.m_Filename`, inside the TOC array / RPC buffer). The Joliet branch is safe (length halved, ≤127). Directory records are disc-controlled; SMSCDVD is live (packed CDVD slot, loaded by `src/SMS_CDVD.c:77`). Related: the directory-walk loops (`SMSCDVD.c:525-527`, `:565-567`, `:713-715`) advance by the record's `m_Length` and only special-case 0; a length smaller than the fixed record header is never rejected before `TocEntryCopy` dereferences the fields.
- **Fix:** Terminate at the copied length, not the claimed length: after the 128-byte `strncpy`, write `dst[128] = 0` (and use the truncated length for the `strtok` step). Optionally reject records with `m_Length` below the header size in the three walk loops.
- **Benefit:** Removes a crafted-disc IOP overflow in the module every disc browse goes through.
- **Effort:** Small. **Risk:** Low — names >128 chars are already truncated by the `strncpy`. **Confidence:** READ.

### RE-SMS-009 — get_socket() indexes the fixed socket table with no bounds check
- **Category:** 6 (bugs/security)
- **Locations:** `iop/SMSTCPIP/src/sockets.c:104-115` (definition, `&sockets[aSD]` unchecked); all 19 call sites in `sockets.c` (e.g. :152, :203, :235, :262, :295, :325, :397, :445, :816, :850, :881, :1077, :1261); table size `NUM_SOCKETS = MEMP_NUM_NETCONN = 15` (`sockets.c:52`, lwipopts.h), while `FD_SETSIZE` is 16 (`include/lwip/sockets.h:197`) — even a legitimate `select(FD_SETSIZE, …)` reads slot 15
- **Problem:** Any negative or ≥15 descriptor reads/writes outside `sockets[15]`. Worse than a bad read: `lwip_close` writes `sock->conn = NULL`, `sock->lastdata = NULL` through the returned pointer, so an out-of-range fd corrupts whatever static lies adjacent. Every exported `lwip_*` entry point is reachable with an arbitrary int fd via the IRX export table (`src/exports.tab`). `lwip_selscan` additionally loops `i < maxfdp1` calling `get_socket(i)` with a caller-supplied `maxfdp1`.
- **Fix:** One check in `get_socket`: `if (aSD < 0 || aSD >= NUM_SOCKETS) { set_errno(EBADF); return NULL; }`. All callers already handle NULL. Also clamp `maxfdp1` to `NUM_SOCKETS` in `lwip_select`/`lwip_selscan`.
- **Benefit:** Closes an out-of-bounds read/write primitive on every socket entry point with a 2-line fix.
- **Effort:** Trivial. **Risk:** Low. **Confidence:** READ (coordinator re-verified in the clone).

### RE-SMS-010 — DTS: header fields and a cumulative sum index fixed tables with no range checks (downmix / bitalloc / LFE / scale factors)
- **Category:** 6 (bugs/security)
- **Locations:** `src/SMS_DTS.c:10722` (`state->amode = bitstream_get(state, 6)`, 0–63) feeding `src/SMS_DTS.c:10261` (`table[flags & DTS_CHANNEL_MASK][input]`, table declared `[11][10]` at :10203); `src/SMS_DTS.c:10780` (`bitalloc_huffman[j] = bitstream_get(state, 3)`, 0–7) feeding `src/SMS_DTS.c:11345` (`bitalloc_12[state->bitalloc_huffman[j]]`, table has 5 entries at :189 — values 5 and 6 are diverted to direct reads at :11340-11343, value **7** is not); `src/SMS_DTS.c:11440` (`scale_factor_quant7[bitstream_get(state, 8)]` — 0–255 into a `[128]` table at :1127); `src/SMS_DTS.c:11386` and `:11402` (`scale_table[scale_sum]`, where `scale_sum` is a cumulative sum of Huffman deltas accumulated at :11376-11392 with no bound against the `[64]`/`[128]` tables at :1108/:1127)
- **Problem:** Four distinct sinks, one mechanism: values taken straight from the bitstream (plus one derived by unbounded accumulation) are used as indices into fixed tables. `amode ≥ 10` reads past the downmix table's row; bitalloc selector 7 reads a garbage function-table pointer and dereferences it in `_dts_inverseq`; an LFE scale byte ≥ 128 reads past `scale_factor_quant7`; a scale-factor delta walk can push `scale_sum` negative or past the quant table in either direction. (Related fall-through, same lines: `scalefactor_huffman == 7` matches neither the `< 5`, `== 5`, nor `== 6` branch at :11378-11392, silently reusing a stale `scale_sum`.) The AC3 decoder in the same codebase shows the intended pattern — its downmix path masks and range-checks the equivalent mode value.
- **Fix:** Range-check at each sink: reject `amode` values the `[11][10]` table doesn't cover before `_dts_downmix_init` indexes it; reject bitalloc selector 7 alongside the existing 5/6 diversion; mask or reject the LFE byte to the table's 128 entries; clamp/validate `scale_sum` against the selected table's size before :11386/:11402. No new tables or abstractions.
- **Benefit:** Removes four independent out-of-bounds reads (one of them a wild pointer dereference) from the DTS header path.
- **Effort:** Small. **Risk:** Low; valid DTS streams stay inside every one of these ranges by construction of the format. **Confidence:** READ (table sizes script-counted).

### RE-SMS-011 — DTS SPDIF passthrough: header frame size can make `memset` length negative (huge) and overrun the output buffer
- **Category:** 6 (bugs/security)
- **Locations:** `src/SMS_DTS.c:11651` (`aLen = DTS_SyncInfo(...)` — frame size declared in the stream header, up to 16,383); `src/SMS_DTS.c:11652` (`retVal = lnSamples << 2` — at most 8,192 for the largest frame type); `src/SMS_DTS.c:11658-11692` (byte-swap copies `aLen` bytes into the `retVal`-sized output buffer); `src/SMS_DTS.c:11694` (`memset(lpOutBuf + 8 + aLen, 0, retVal - 8 - aLen);`)
- **Problem:** `retVal` is derived from the sample count, `aLen` from an independent header field; nothing reconciles them. When `aLen > retVal - 8`, the copy already overruns the ring buffer, and the `memset` length goes negative, becoming a near-4 GB `size_t` write. The AC3 equivalent (`_ac3_spdif`) is safe only because the AC3 format caps frames at 3,840 bytes against a fixed 6,144-byte burst — the DTS port reused the shape without the format guarantee.
- **Fix:** After :11651, bail out (return 0 / drop the frame) when `aLen > retVal - 8`, before allocating or copying.
- **Benefit:** Removes a remotely triggerable memory-corruption primitive in the SPDIF path.
- **Effort:** Trivial. **Risk:** None for valid streams; oversized declared frames are undecodable anyway. **Confidence:** READ.

### RE-SMS-012 — DXSB subtitle decoder trusts header dimensions against a fixed scratchpad row buffer; packet size never checked
- **Category:** 6 (bugs/security)
- **Locations:** `src/SMS_DXSB.c:24-34` (header struct), `src/SMS_DXSB.c:195-204` (`_decode` entry), `src/SMS_DXSB.c:209` + `src/SMS_DXSB.c:272` (scratchpad row writes), `include/SMS.h:120` (`SMS_SPR_FREE` = `0x70001B30`)
- **Problem:** `_decode(apData, aSize)` casts `apData` to `_DXSB_Header` and reads packed `unsigned short m_Width/m_Height` (up to 65535 each) without ever checking `aSize >= sizeof(_DXSB_Header)`. The row staging buffer is `lpTmp = SMS_SPR_FREE` at `0x70001B30`; the scratchpad ends at `0x70004000`, leaving 0x24D0 (9,424) bytes. The decode loop does `mips_memset(lpTmp + lX, …, lLen)` with `lX` ranging up to `lW-1` and also `dxsb_pack(lpDst, lpTmp, lRW)` reading `lRW` (width rounded to 32) bytes from that staging area. A subtitle packet claiming a width above ~9400 writes/reads far outside the scratchpad free region. Allocation size is likewise derived purely from the claimed dimensions (`lSize = (lRW>>1) * lRH`, `src/SMS_DXSB.c:201-204`).
- **Fix:** At `_decode` entry, return NULL when `aSize < sizeof(_DXSB_Header)` or when the claimed width exceeds the scratchpad row capacity (or a sane subtitle bound). Two comparisons at one choke point; no new code paths.
- **Benefit:** Removes a stream-controlled scratchpad overwrite from the subtitle path, which runs on every DivX-embedded-subtitle file.
- **Effort:** Small. **Risk:** Low — legitimate DXSB subtitles are a few hundred pixels wide. **Confidence:** READ.

### RE-SMS-013 — Stream/codec/user-data allocations are dereferenced without a NULL check in nine container creators
- **Category:** 6 (bugs/security)
- **Locations:** `src/SMS_Container.c:349-353` (`SMSContainer_GetWAVHeader`: `malloc` at :351, `Read` into it at :352); `src/SMS_ContainerAAC.c:197-218` (callocs :197/:200, `malloc(16)` :212, write through it :218); `src/SMS_ContainerAC3.c:109-112`; `src/SMS_ContainerASF.c:157-159` (context calloc + `memset`), `:212` (codec calloc; stream calloc at :206 *is* checked, codec is not; first codec deref :261); `src/SMS_ContainerAVI.c:102-107` (`_NewStream`: stream checked, codec calloc at :107 not) and `:1164-1166` (container calloc, then `MYCONT(apCont)->m_RiffEnd` written); `src/SMS_ContainerFLAC.c:123-126` and `:138-146` (`malloc` :138, struct copy through it :146); `src/SMS_ContainerJPG.c:64-84` (sub-container calloc :64, fields written from :70); `src/SMS_ContainerM3U.c:308-312` and `:366-370` (entry + path `malloc`s, then `strcpy`); `src/SMS_ContainerMP3.c:121-124`; `src/SMS_ContainerOGG.c:105-108`
- **Problem:** One mechanism, repeated: `calloc`/`malloc` results are used on the next line with no test. On a 32 MB EE heap that also holds ring buffers and (for ASF/AVI) per-entry index growth, allocation failure during probing is a realistic runtime event, and every site converts it into a NULL dereference (or a `Read`/`strcpy` into NULL) instead of a clean probe rejection. The tree already contains the correct pattern twice — MOV checks both allocations (`src/SMS_ContainerMOV.c:202-207`) and MPEG-PS checks both (`src/SMS_ContainerMPEG_PS.c:795-796`) — so this is drift, not style.
- **Fix:** At each site, test the result and fail the probe the way the neighbouring code already does (`return 0`, or ASF's `goto fail`); for the two-step stream+codec sites, free the stream if the codec allocation fails, mirroring `src/SMS_ContainerMOV.c:206`.
- **Benefit:** Converts a whole class of OOM crashes across nine files into ordinary "cannot open" outcomes; also gives the five near-identical audio creators (AAC/AC3/FLAC/MP3/OGG) one consistent failure shape.
- **Effort:** M. **Risk:** Low. **Confidence:** READ.

### RE-SMS-014 — Raw config blob is trusted as array indices with almost no validation
- **Category:** 6 (bugs/security)
- **Locations:** `src/SMS_Config.c:696-702` and `:727-732` (load: 4-byte version check, then 892 raw bytes into `g_Config`); the only post-load guards are `src/SMS_Config.c:773` (`m_SkinName` last byte NUL) and `:775` (`m_PlayerAC3RL` clamp). Consumers: `src/SMS_CDVD.c:126` (`sl_Speeds[g_Config.m_CDVDSpeed]`, table of 3), `src/SMS_GUIMenuSMS.c:619` and `:1806` (`s_Speeds[g_Config.m_CDVDSpeed]`), and 26 script-counted `g_Palette[g_Config.m_*Idx - 1]` reads, e.g. `src/SMS_GUI.c:674-675`, `src/SMS_Player.c:144-147`, `src/SMS_PlayerControl.c:205`, `src/SMS_Spectrum.c:51`
- **Problem:** A corrupt or crafted `SMS.cfg` (memory card / next to the ELF — user-copyable files) sets the unsigned colour indices and `m_CDVDSpeed` arbitrarily. Index `0` reads `g_Palette[-1]`; a large index reads far out of bounds; `m_CDVDSpeed > 2` indexes past a 3-entry table. The same unvalidated blob also supplies display-mode/geometry fields.
- **Fix:** In `SMS_LoadConfig`, after a successful read, clamp/validate the whole field set in one place: colour indices to `1..17`, `m_CDVDSpeed` to `0..2`, display mode and scroll-bar fields to their enum ranges — resetting an out-of-range field to its default, exactly as `m_PlayerAC3RL` already is. No new config version, no new fields.
- **Benefit:** Removes a whole OOB class reachable from a single user-supplied file, at the one choke point every boot passes through.
- **Effort:** Small. **Risk:** Low (defaults already exist in the same function). **Confidence:** READ.

### RE-SMS-015 — `sprintf` into the 64-byte `g_SaveDiag` with a 128-byte path overflows it
- **Category:** 6 (bugs/security)
- **Locations:** `src/SMS_Config.c:69` (`char g_SaveDiag[64]`), `:943` (`sprintf(g_SaveDiag, "Save: CWD write %d %s", …, s_pMC0SMC)`), `:945` (same for open), `:1082-1087` (`"Save: mc open %d %s"` with `s_pMC0SMC`); `s_pMC0SMC` is `char[128]` at `src/SMS_Config.c:64`. The buffer and its messages are self-described TEMP diagnostics at `src/SMS_Config.c:66-68`, `:883`, and consumed at `src/SMS_GUIMenuSMS.c:1007`
- **Problem:** `"Save: CWD open -1 "` (19 chars) plus a config path of up to 127 chars is up to ~146 bytes written into a 64-byte global — a straightforward overflow on exactly the error path (failed save) the diagnostic exists for. The same TEMP pattern appears for IPCONFIG in `src/SMS_GUIMenuSMS.c:2031-2090` into `char lDiag[48]` (there the format carries no path, so it fits — checked).
- **Fix:** Either delete the TEMP diagnostic (preferred under LESS, NOT MORE — it was added to chase a save bug) or switch the three path-carrying sites to `snprintf(g_SaveDiag, sizeof(g_SaveDiag), …)`. Do not enlarge the buffer and keep unbounded `sprintf` elsewhere.
- **Benefit:** Removes a guaranteed overflow on a routine failure path; deleting it also removes a global and its extern.
- **Effort:** Small. **Risk:** Low. **Confidence:** READ.

### RE-SMS-016 — ip_reass trusts IPH_LEN over the actual pbuf chain, and its bitmap index goes one byte out of bounds at exactly 5760 bytes
- **Category:** 6 (bugs/security)
- **Locations:** `iop/SMSTCPIP/src/ip_frag.c:58-77` (`copy_from_pbuf` walks `p = p->next` with no NULL stop); `ip_frag.c:146-159` (`len` from wire `IPH_LEN`, then `copy_from_pbuf(p, &i, …, len)`); `ip_frag.c:174` and `:195` (bitmap index `(offset+len)/64` / `ip_reasslen/64` == 90 when the total is `IP_REASS_BUFSIZE` 5760, but `ip_reassbitmap` has 5760/64 = 90 bytes, indices 0–89); entry gap at `iop/SMSTCPIP/src/ip.c:224-230` (`pbuf_realloc(p, IPH_LEN)` cannot grow, so a header claiming more data than the chain holds passes through with `tot_len` still short)
- **Problem:** A fragment whose IPH_LEN exceeds the received chain length makes `copy_from_pbuf` copy `len` bytes by walking off the end of the chain and dereferencing NULL. A fragment completing exactly 5,760 bytes writes/reads one byte past the static bitmap into adjacent statics.
- **Fix:** In `copy_from_pbuf`, stop at chain end and report the shortfall (drop the datagram in `ip_reass`); change the reassembly bound to `offset + len >= IP_REASS_BUFSIZE` rejection for the final byte, or size the bitmap `IP_REASS_BUFSIZE/64 + 1`. The chain-end stop is the primary fix.
- **Benefit:** Removes a second remotely triggerable crash/corruption in the same reassembly path as RE-SMS-003.
- **Effort:** Small. **Risk:** Low. **Confidence:** READ.

### RE-SMS-017 — UDF descriptor lengths trusted: allocation-descriptor offset and name decode unbounded (SMSCDVD)
- **Category:** 6 (bugs/security)
- **Locations:** `iop/SMSCDVD/SMSCDVD_UDFS.c:293-345` (`UDFFileEntry`), `iop/SMSCDVD/SMSCDVD_UDFS.c:84-105` (`UDFDecode`); callers `:138`, `:399`
- **Problem:** `UDFFileEntry` reads `lLEA`/`lLAD` (u32s from the disc at offsets 168/172) and forms `p = 176 + lLEA`, then reads allocation descriptors at `&apData[p]` with no check that `p` (or the descriptor) lies inside the 2,048-byte sector buffer `apData` points into; a large `lLEA` reads out of bounds (and the `176 + lLEA + lLAD` sum can wrap). `UDFDecode` writes `apDst[i++]` for the full source length `aLen` — the destination size is not a parameter and is never checked; at `:399` `aLen` is the disc-supplied `lLFI` (file identifier length). Secondary inconsistency: `UDF_DOpen` returns `1` (`SMSCDVD_UDFS.c:552`) and `UDF_DClose` returns `1L` (`:619`) where the ISO paths return 0 on success (impact INFERRED — depends on how the EE/ioman layer treats a positive dopen result).
- **Fix:** Bound `p + <AD size> <= 2048` before each `UDFAD` call and pass the destination capacity into `UDFDecode` (or clamp `aLen` at both call sites to the known destination sizes). Align the two return values with the ISO side.
- **Benefit:** Removes crafted-disc OOB read/write in the UDF half of the live CDVD module.
- **Effort:** Small. **Risk:** Low. **Confidence:** READ (returns: READ; impact INFERRED).

### RE-SMS-018 — Path component copied unbounded into a 257-byte stack buffer (SMSUMS)
- **Category:** 6 (bugs/security)
- **Locations:** `iop/SMSUMS/src/ums_fat.c:605-626` (`_file_start_cluster`; buffer at `:607`, copy at `:623`)
- **Problem:** Path components are accumulated with `lName[lOff++] = apName[i++]` and no bound; a single path component longer than 256 bytes (long filename handed down from the EE/browser) overruns the stack buffer `lName[257]`. FAT itself limits a component to 255, but nothing enforces that before the copy.
- **Fix:** Stop with `-ENAMETOOLONG` when `lOff` reaches 256 — the function already returns `-ENOENT`-style errors on failure paths, so this is one comparison in the existing loop.
- **Benefit:** Removes a stack overflow reachable with an ordinary (if long) filename.
- **Effort:** Small. **Risk:** Low. **Confidence:** READ.

### RE-SMS-019 — `UmsWarmUp` loops over a device-supplied LUN count past a 16-entry name table (SMSUMS)
- **Category:** 6 (bugs/security)
- **Locations:** `iop/SMSUMS/src/ums_driver.c:214-216` (loop), `:232-248` (writes); table `iop/SMSUMS/include/ums.h:54` (`char m_LUName[16][19]`)
- **Problem:** `lnLUN = UmsGetMaxLUN(apDev)` is the device's own GET MAX LUN byte (0–255), and the warm-up loop runs `for (i = 0; i <= lnLUN; ++i)` writing `m_LUName[i]` — a 16-row array. A device reporting more than 15 LUNs writes INQUIRY names past the array into the rest of the `USBMDevice` struct. (Secondary: at `:234`, `lLen = strlen(m_LUName[i]) - 1` is −1 for an all-zero name, and the dedup suffix write at `:247-248` would then touch index −1 if a duplicate were found.)
- **Fix:** Clamp `lnLUN` to 15 where it is read (`UmsWarmUp`), matching the fixed table size; no other site uses the unclamped value for indexing.
- **Benefit:** Removes a crafted-device struct overwrite in a live module.
- **Effort:** Small. **Risk:** Low — >16-LUN devices cannot be represented in the existing table anyway. **Confidence:** READ.

### RE-SMS-020 — USB device geometry (sector size / LBA) trusted unchecked, including divide-by-zero (SMSUMS)
- **Category:** 6 (bugs/security)
- **Locations:** `iop/SMSUMS/src/ums_driver.c:312-313` (`UmsGetCapacity`); divisions by the value at `iop/SMSUMS/src/ums_driver.c:395`, `iop/SMSUMS/src/ums_cache.c:40`, `iop/SMSUMS/src/ums_fat.c:226-227` (`_root_dir_sectors`), `iop/SMSUMS/src/ums_fat.c:236` and `:395` (`m_SectorSize >> 5` / `m_SectorSize / 32` as divisors)
- **Problem:** `UmsGetCapacity` stores the READ CAPACITY response verbatim: `m_MaxLBA = _int_be(&lBuf[0])`, `m_SectorSize = _int_be(&lBuf[4])` — no range check at all. A device reporting sector size 0 makes every division above a divide-by-zero (IOP exception); a size below 32 makes the `>> 5` / `/ 32` divisors zero the same way; a huge or non-power-of-two size desynchronises every sector computation downstream. `FATFS_Init` does whitelist the **BPB's** sector/cluster sizes (`ums_fat.c:1048-1061`) but never reconciles them with the device-reported size, and allocates its BPB buffer as `sizeof(FATFS) + lSecSize` from the unvalidated value (`ums_fat.c:1002`).
- **Fix:** Validate once, at the source: in `UmsGetCapacity`, accept only 512/1024/2048/4096 (the same whitelist `FATFS_Init` already uses for the BPB) and fail the capacity call otherwise. One check at the trust boundary; all downstream divisions become safe by construction.
- **Benefit:** Removes a crafted-device IOP crash class and a geometry-desync class in a live module.
- **Effort:** Small. **Risk:** Low — non-standard sector sizes are already rejected at the BPB layer. **Confidence:** READ.

### RE-SMS-021 — `TranslateUTF8` can write `bufSize + 1` bytes into the caller's buffer
- **Category:** 6 (bugs/security)
- **Locations:** `src/SMS_InverseCodePages.c:138-195` (loop at :145, final `*outBuff = '\0'` after the loop); callers `src/SMS_SubtitleContext.c:482-483`, `src/SMS_SubtitleContext.c:657-659`; safe caller `src/SMS_InverseCodePages.c:229-239` (`DisplayUTF8`)
- **Problem:** The loop writes one output byte per iteration while `length < bufSize`, i.e. up to `bufSize` bytes, and then unconditionally writes a terminating NUL at `outBuff[bufSize]` — one byte past the contracted capacity. `DisplayUTF8` compensates by passing `aSize - 1`, but the subtitle callers do not: line 483 passes `SUB_MAXLEN` for `char lBuff[SUB_MAXLEN]`, and line 658 passes the remaining space in `lLine[SUB_MAXLEN]` in-place. Today they are saved only because subtitle input lines are length-bounded elsewhere (`File_GetString(..., SUB_MAXLEN)`), not by this function's contract — any future caller (or a longer-line path) gets a 1-byte stack overwrite.
- **Fix:** Reserve the NUL inside `TranslateUTF8` (loop while `length + 1 < bufSize`, or treat `bufSize` as capacity-including-NUL and update the two subtitle call sites to match `DisplayUTF8`'s convention). One convention, applied in one place.
- **Benefit:** Closes a latent 1-byte stack-buffer overflow in subtitle rendering of user-supplied subtitle files, and removes a contract that each caller currently has to rediscover.
- **Effort:** Trivial. **Risk:** Low — behaviour changes only for inputs that previously filled the buffer exactly, which then lose their last character to the NUL instead of corrupting the stack. **Confidence:** READ.

### RE-SMS-022 — AAC: sample-rate index from ADTS/ASC (up to 15) indexes 12-entry window tables unchecked
- **Category:** 6 (bugs/security)
- **Locations:** `src/SMS_AAC.c:6041` (`hDecoder->sf_index = adts.sf_index;` — raw 4-bit field, 0–15); the ASC path in `AudioSpecificConfig2` likewise keeps a 4-bit index, and index 15 (explicit frequency) passes its validation because the explicit rate is non-zero; `src/SMS_AAC.c:6204-6218` (`window_grouping_info` indexes `num_swb_1024_window[sf_index]`, `swb_offset_1024_window[sf_index]`, `num_swb_128_window[sf_index]` — all 12-entry arrays, script-counted)
- **Problem:** Indices 12–15 are reserved in both ADTS and ASC headers, and nothing rejects them: `_NeAACDecInit`/`_NeAACDecInit2` store the index, and the first decoded frame reads past the 12-entry tables (the pointer-table read at :6207 then dereferences whatever pointer follows the array). `get_sample_rate()` already encodes the validity rule — it returns 0 for index ≥ 12 — but init never treats a 0 rate as fatal.
- **Fix:** In both init paths, fail initialisation when `get_sample_rate(sf_index) == 0` (equivalently `sf_index >= 12`). One check per path, reusing the existing rule.
- **Benefit:** Closes an out-of-bounds table + pointer read on any crafted ADTS/MP4-AAC header.
- **Effort:** Trivial. **Risk:** None; indices 12–15 cannot describe a playable stream. **Confidence:** READ.

### RE-SMS-023 — AAC: DRC `excluded_channels` loop writes past fixed 64-entry masks, driven by stream bits
- **Category:** 6 (bugs/security)
- **Locations:** `src/SMS_AAC.c:7549-7562` (`excluded_channels` loop); array sizes at `src/SMS_AAC.c:333-334` (`exclude_mask[MAX_CHANNELS]`, `additional_excluded_chns[MAX_CHANNELS]`, MAX_CHANNELS = 64)
- **Problem:** The loop has no bound tied to the arrays it fills. Each continuation bit from the stream adds 7 more mask writes; after the 10th iteration `exclude_mask` writes run past the 64-entry array inside the heap-allocated decoder struct, and the `uint8_t` counter `n` itself wraps at 255, re-indexing `additional_excluded_chns[n-1]` from the start. A fill element carrying a DRC extension with a long run of continuation bits corrupts the decoder handle.
- **Fix:** Stop the loop when `num_excl_chan + 7 > MAX_CHANNELS` (treat as end of the excluded-channel list). One comparison inside the existing loop.
- **Benefit:** Removes a heap overwrite in the AAC fill-element path.
- **Effort:** Trivial. **Risk:** None; real DRC data never legitimately excludes more channels than the decoder supports. **Confidence:** READ.

### RE-SMS-024 — WMA: version-1 exponent bands can contain zero-width entries, and `decode_exp_vlc` underflows on a zero entry into a runaway write
- **Category:** 6 (bugs/security)
- **Locations:** `src/SMS_WMA.c:1347-1362` (WMA v1 band computation: `lpCtx->exponent_bands[0][i] = pos - lpos;` with **no** `pos > lpos` check); contrast `src/SMS_WMA.c:1386` (the v2 computed path does guard: `if (pos > lpos) lpCtx->exponent_bands[k][j++] = pos - lpos;`); consumer at `src/SMS_WMA.c:1627-1667` (`decode_exp_vlc`: `n = *ptr++; do { *q++ = v; } while (--n);` at :1646-1648 and :1658-1660)
- **Problem:** In the v1 path, when two consecutive critical frequencies round to the same band position (small block length), `pos - lpos` is 0 and a zero entry is stored. `decode_exp_vlc` trusts every entry as a repeat count in a do-while: a 0 entry makes `--n` underflow (unsigned) and the loop writes exponents across the channel buffer until it faults. The v2 path's guard proves the v1 omission is an oversight, not a design choice. (The table-driven v2 path is safe: its tables are count-prefixed and contain no zero entries — script-verified.)
- **Fix:** Mirror the v2 guard in the v1 loop (store the band only when `pos > lpos`, compacting with a separate write index as :1386 does). A few lines, deleting the divergence between the two paths.
- **Benefit:** Removes a runaway heap write reachable from WMA v1 streams at small block sizes.
- **Effort:** Small. **Risk:** Low; zero-width bands carry no exponents, so skipping them matches the decoder's own v2 behaviour. **Confidence:** READ for the guard asymmetry and the do-while underflow; INFERRED for which specific sample-rate/block-size combinations produce a repeated position.

### RE-SMS-025 — getsockopt/setsockopt read and write TCP PCB fields on UDP sockets and on NULL PCBs
- **Category:** 6 (bugs/security)
- **Locations:** `iop/SMSTCPIP/src/sockets.c:1004` (SOL_SOCKET flags via `pcb.tcp->so_options`), `:1037` (IP_TTL via `pcb.tcp->ttl`), `:1037-1063`, writes at `:1197-1247` (`so_options`, `ttl`, `tos`, TCP flags/keepalive via `pcb.tcp->…`); only the IPPROTO_TCP level checks the conn type (:~985, :~1115) and even it silently `return 0` for non-TCP
- **Problem:** `pcb` is a union (`api.h`): for a UDP socket, `pcb.tcp` overlays `struct udp_pcb`, whose field offsets for `so_options`/`ttl`/`tos`/`flags`/`keepalive` do not match `struct tcp_pcb` — so `getsockopt(udp_fd, IPPROTO_IP, IP_TTL)` returns garbage from the wrong offset and `setsockopt(udp_fd, SOL_SOCKET, SO_KEEPALIVE)` *writes* into the UDP PCB at the TCP offset, corrupting neighbouring UDP state. For a netconn whose PCB was never created (socket() then setsockopt before bind/connect) or was nulled by `err_tcp`, the same lines dereference NULL.
- **Fix:** At the top of the value-processing sections, require `sock->conn->pcb.tcp != NULL`; for SOL_SOCKET/IP levels either dispatch on `conn->type` to the correct PCB struct or return ENOPROTOOPT for non-TCP (LESS: these options are only meaningful for TCP in this stack, and the TCP level already special-cases type).
- **Benefit:** Stops cross-struct memory corruption from two exported, application-facing calls.
- **Effort:** Small. **Risk:** Low — UDP callers currently get garbage/corruption, not behaviour worth preserving. **Confidence:** READ.

### RE-SMS-026 — accept_function() registers callbacks on the new PCB before allocating the netconn's mbox/sem, then frees the netconn on failure, leaving the PCB pointing at freed memory
- **Category:** 6 (bugs/security)
- **Locations:** `iop/SMSTCPIP/src/api_msg.c:225-270` (`setup_tcp(newconn)` at :240, *then* `sys_mbox_new`/`sys_sem_new` at :243-260; failure arms `memp_free(MEMP_NETCONN, newconn)` and return ERR_MEM)
- **Problem:** `setup_tcp` installs `recv_tcp`/`sent_tcp`/`err_tcp` with `arg = newconn` on a live, registered PCB. If mbox/sem allocation fails, the netconn is freed but the PCB is neither closed nor are its callbacks cleared: the next segment on that connection calls `recv_tcp` with a dangling `conn` pointer (use-after-free; trigger INFERRED — requires alloc failure at accept time plus subsequent traffic). The PCB itself also leaks (never returned to any pool).
- **Fix:** Allocate recvmbox/mbox/sem *before* `setup_tcp`; on failure after `setup_tcp`, call `tcp_close`/`tcp_abort` on the PCB before freeing the netconn (mirrors `do_delconn`'s teardown order).
- **Benefit:** Accept under memory pressure fails the one connection instead of arming a UAF in the tcpip thread.
- **Effort:** Small. **Risk:** Low. **Confidence:** READ (UAF trigger INFERRED).

### RE-SMS-027 — tcp_process() SYN_SENT branch dereferences pcb->unacked without a NULL check
- **Category:** 6 (bugs/security)
- **Locations:** `iop/SMSTCPIP/src/tcp_in.c:525-527` (debug print and the accept condition both use `pcb->unacked->tcphdr->seqno`); the same hazard is independently documented in-tree at `iop/SMSTCPIP/src/loopif.c:82-88` (comment describing `pcb->unacked` still being NULL in tcp_process)
- **Problem:** A SYN-SENT PCB whose SYN segment failed to enqueue (`tcp_connect` proceeds to SYN_SENT even when `tcp_enqueue` returns ERR_MEM, `tcp.c` `tcp_connect()`) has `unacked == NULL`; a SYN+ACK from the peer then crashes the tcpip thread inside the state machine. (Trigger path INFERRED — it requires the enqueue-failure state or an equivalent queue drain; the missing guard itself is READ.)
- **Fix:** `if (pcb->unacked == NULL) return ERR_OK;` (drop the segment; retransmit/timeout machinery owns the PCB) at the top of the SYN_SENT case.
- **Benefit:** One malformed internal state stops being a remote-triggerable stack crash.
- **Effort:** Trivial. **Risk:** Low. **Confidence:** READ (trigger INFERRED).

### RE-SMS-028 — ASF packet sync divides by a file-controlled `packet_size` that is never validated non-zero
- **Category:** 6 (bugs/security)
- **Locations:** `src/SMS_ContainerASF.c:192` (`lpASF->packet_size = lpASF->hdr.max_pktsize`), `src/SMS_ContainerASF.c:368` (`(CurPos - data_offset) % apASF->packet_size`)
- **Problem:** `packet_size` comes straight from the ASF file-properties object (`max_pktsize`, a `File_GetUInt` with no range check), and `asf_read_header` never requires a file-properties object to have appeared at all before the data object ends the header walk. A crafted or truncated ASF with `max_pktsize == 0` (or no file header) reaches `_ReadPacket` → `asf_get_packet`, whose first computation is a modulo by `packet_size` — integer divide-by-zero with a file-controlled divisor.
- **Fix:** Reject the header (ASF's existing `goto fail`) when `packet_size == 0` after the header loop; one test at the single place the value is established.
- **Benefit:** Closes a one-field crash on an untrusted format; also rejects a class of header-less ASF files that could only malfunction later.
- **Effort:** S. **Risk:** Low. **Confidence:** READ.

### RE-SMS-029 — AVI ODML super-index loading recurses on file-controlled offsets with no depth bound
- **Category:** 6 (bugs/security)
- **Locations:** `src/SMS_ContainerAVI.c:226-330` (`_LoadODMLIdx`; recursive call at :299, target offset from `File_GetULong` at :291-298)
- **Problem:** For a type-0 (super-index) entry, the loader seeks to a file-supplied 64-bit offset and calls itself. Nothing limits depth and nothing remembers visited offsets, so a crafted ODML index whose entries point at each other (or at themselves) recurses until the EE stack is exhausted. Real ODML nesting is two levels (super-index → index), so the unbounded recursion buys nothing. The per-entry index allocation inside the recursion has the RE-SMS-069 defect as well, but that is listed there; this finding is only the recursion.
- **Fix:** Thread a depth counter through the call chain and stop descending past a small bound (e.g. 4 — twice the legitimate depth); entries beyond it are ignored like other malformed ODML data already is in this function.
- **Justification:** The fix *adds* a depth parameter and one comparison — justified because there is no existing bound of any kind on a recursion whose depth is fully attacker-controlled, and the bound is set well above every legitimate file's depth, so no valid input changes behaviour.
- **Benefit:** A malicious or corrupt AVI index can no longer overflow the stack during load.
- **Effort:** S. **Risk:** Low. **Confidence:** Med (mechanism READ; INFERRED that cyclic offsets are accepted, as no visited-set or EOF check distinguishes them).

### RE-SMS-030 — AAC: codebook-11 escape values can index the 8,192-entry inverse-quant table out of bounds in the `vquant` asm
- **Category:** 6 (bugs/security)
- **Locations:** `src/SMS_AAC.c:887` (`_aac_iq_table`, 8,192 entries, script-counted); `src/SMS_AAC.c:6559-6575` (`huffman_getescape`: the escape-length counter `i` is a `uint8_t` incremented per leading 1-bit with no cap, and the returned value is built as `(1 << i) | offset` then truncated to `int16_t`); `src/SMS_AAC.c:6739-6820` (`vquant` asm computes the table address as `base + (|q| << 2)` with no range check, called for every coefficient group from `quant_to_spec`)
- **Problem:** Ordinary Huffman pairs/quads bound spectral values to ≤ 16, but the codebook-11 escape path manufactures values from a bit-run whose length the stream controls. Any resulting `|q| ≥ 8192` makes `vquant` load from past the 32 KB table; the `uint8_t` wrap of the run counter makes the produced values effectively arbitrary within `int16_t`. (The table itself is exactly sized for the *legitimate* escape range; the missing piece is the cap the reference decoder applies to the escape length.)
- **Fix:** Cap the escape run in `huffman_getescape` (stop extending `i` at the legitimate maximum, mirroring the format's escape definition) so produced values stay inside the table by construction.
- **Benefit:** Removes an out-of-bounds read in the AAC coefficient path.
- **Effort:** Small. **Risk:** Low; legitimate escapes never reach the cap. **Confidence:** INFERRED (asm addressing and escape construction read directly; no crafted stream was executed).

### RE-SMS-031 — AAC: the constant 16428320 (0xFAAD20) is wired into the decode path — one reachable sample overwrite, two dead remnants
- **Category:** 6 (bugs/security)
- **Locations:** `src/SMS_AAC.c:5030` (`hf_assembly`: `if (sbr->bs_extension_id == 3 && sbr->bs_extension_data == 42) Xsbr[...][0] = 16428320;`); `src/SMS_AAC.c:6553-6560` (`huffman_codebook()` returns the two halves of 16428320); `src/SMS_AAC.c:6589-6594` (`huffman_spectral_data` case 12 decodes a pair, then overwrites both values with `huffman_codebook(0/1)`); `src/SMS_AAC.c:3809` (`get_sr_index`: `if (16428320 <= samplerate) return 11;`)
- **Problem:** 16428320 = 0xFAAD20 is a watermark constant, not audio data. Three consequences: (1) the SBR site at :5030 is **reachable** — any SBR stream carrying an extension payload with id 3 and data 42 has a QMF sample forcibly replaced by 16428320.0, a full-scale click/corruption injected by the stream; (2) the case-12 overwrite is dead weight — `section_data` already rejects codebook 12 with error 32 before spectral decoding, so the decoded-then-discarded pair and `huffman_codebook()` can never run; (3) the `get_sr_index` branch is arithmetically dead (any rate ≥ 16428320 already matched the ≥ 92017 case returning 0). One mechanism, three locations; only the first affects output.
- **Fix:** Delete all three: the `hf_assembly` assignment, `huffman_codebook()` and the case-12 overwrite (case 12 keeps the plain pair decode for defence in depth), and the dead `get_sr_index` comparison.
- **Benefit:** Removes a stream-triggered corruption of decoded audio and ~15 lines of dead/misleading code.
- **Effort:** Trivial (pure deletions). **Risk:** None; no valid decode path depends on any of the three. **Confidence:** READ.

### RE-SMS-032 — AC3/DTS: two-entry float-to-int function-pointer table indexed by an unvalidated channel count in both decoders
- **Category:** 6 (bugs/security)
- **Locations:** `src/SMS_AC3.c:2576-2585` (`s_FloatToInt[2]`, indexed `s_FloatToInt[anChannels - 1]`); `src/SMS_DTS.c:11556-11565` (same pattern); callers pass the codec context's channel count straight through — `src/SMS_AC3.c:2760` and the matching `DecodeFrame(apOutput, apCtx->m_Channels, lLen)` in `DTS_Decode`; both decoders *adopt the stream's* channel count (up to 6) when the context count is 0 and only ever decrease it afterwards (`src/SMS_AC3.c:2727-2731`, and the identical block in `DTS_Decode`)
- **Problem:** Neither decoder validates `anChannels` before indexing a 2-entry function-pointer table. Playback is safe today only because of an invariant maintained *outside* the decoders: the player clamps `m_Channels` to 2 for non-SPDIF playback (`src/SMS_Player.c:105`) and the SPDIF configuration (where the count is set to 5, :109) swaps in the passthrough function instead (`src/SMS_AC3.c:348-349`, `src/SMS_DTS.c:9984-9985`). If the clamp runs while a container still reports 0 channels (so it no-ops) and the decoder then adopts a 5.1 stream's 6 channels on the first frame, `s_FloatToInt[5]` is called — a wild function pointer taken from adjacent static data.
- **Fix:** Validate at the sink: at the top of `_ac3_decode`/`_dts_decode`, return 0 unless `anChannels` is 1 or 2 (the only values the table and the downmix flags below it support). Two one-line guards replace reliance on a cross-file invariant.
- **Benefit:** Converts a latent wild-pointer call into a local, checkable invariant in both decoders.
- **Effort:** Trivial. **Risk:** None for any stream the player can currently play (all reach here with 1–2). **Confidence:** READ for the mechanism; INFERRED for the exact interleaving that reaches it with a count > 2.

### RE-SMS-033 — MP123: granule gain pointer + scalefactor index can walk past the 378-entry `s_GainPow2` table
- **Category:** 6 (bugs/security)
- **Locations:** `src/SMS_MP123.c:190` (`s_GainPow2[378]`, script-counted 378); `src/SMS_MP123.c:5422` (`m_pPow2Gain = (float*)s_GainPow2 + 256 - SMS_GetBits(lpBitCtx, 8)` — base offset 1–256 from an 8-bit stream field, plus up to +56 from the sub-gain fields at :5435-5437); `src/SMS_MP123.c:5741` and `:5853` (`m_pPow2Gain[(*apSCF++) << lShift]`, shift 1–2, 6-bit scalefactors → index up to 252)
- **Problem:** Worst-case reach is offset ≈ 256 + 252 = 508 into a 378-entry table — ~130 floats past the end. In valid MP2 streams the global gain and scalefactors are correlated so real files stay in range (which is why this ships unnoticed), but the code itself enforces nothing: the bound exists only as an emergent property of well-formed input.
- **Fix:** Saturate the base pointer when it is computed at :5422 so that `base + 252` cannot exceed the table (clamp the 8-bit gain contribution), rather than growing the table.
- **Benefit:** Converts an emergent bound into an enforced one in the MP2 dequantizer.
- **Effort:** Small. **Risk:** Low; saturation only engages on streams already outside the table's design range. **Confidence:** INFERRED (all arithmetic read directly; worst-case correlation in valid streams not disproven).

### RE-SMS-034 — MPEG picture allocation path assumes success: unchecked callocs and a nullable picture lookup dereferenced unconditionally
- **Category:** 6 (bugs/security)
- **Locations:** `src/SMS_MPEG.c:36-58` (`_mpeg_alloc_picture`), `src/SMS_MPEG.c:130-145` (`SMS_MPEGContext_FindUnusedPic`), `src/SMS_MPEG.c:181-185` (`SMS_MPEG_FrameStart`)
- **Problem:** One mechanism, one path: picture setup never checks failure. `_mpeg_alloc_picture` performs seven `calloc`s (MB-skip, QScale, MB-type, 2× motion-value base, 2× ref-idx) with no NULL check and immediately derives `m_pMotionVal[i] = m_pMotionValBase[i] + 2`. `SMS_MPEGContext_FindUnusedPic` explicitly returns NULL when all `MAX_PICTURE_COUNT` (3) pictures are in use, and `SMS_MPEG_FrameStart` then executes `lpPic->m_Ref = …` and `_mpeg_alloc_picture(lpPic)` on the result with no NULL test. Either failure mode is a NULL dereference mid-stream instead of a dropped frame / codec error.
- **Fix:** Check the `FindUnusedPic` result in `SMS_MPEG_FrameStart` and bail out of the frame; check the calloc group in `_mpeg_alloc_picture` once (single combined test) and propagate the same bail. No new abstraction — reuse the existing error style of the codec Init functions.
- **Benefit:** Turns memory-exhaustion / pool-exhaustion crashes into recoverable decode errors across MPEG-1/2/4 and MSMPEG4 (all share this path).
- **Effort:** Small. **Risk:** Low. **Confidence:** READ.

### RE-SMS-035 — VLC/RL table construction and the DivX bitstream stash use allocation results without NULL checks
- **Category:** 6 (bugs/security)
- **Locations:** `src/SMS_VLC.c:223` and `src/SMS_VLC.c:226` (`SMS_RL_Init`), `src/SMS_VLC.c:268-270` (`SMS_VLC_RL_Init`), `src/SMS_MPEG4.c:3391-3392` (DivX packed-frame stash)
- **Problem:** `SMS_RL_Init` `malloc`s `m_pMaxLevel`/`m_pMaxRun` and `memcpy`s into them on the next line, unchecked. `SMS_VLC_RL_Init` `malloc`s `m_pRLVLC[q]` for q = 0..31 and immediately fills `m_VLC.m_TableSize` entries, unchecked. These run for every RL table at codec init (MPEG-4 and MSMPEG4 both). Separately, `MPEG4_Decode` does `g_MPEGCtx.m_pBSBuf = realloc(g_MPEGCtx.m_pBSBuf, …)` and `memcpy`s into the result unchecked — a failed realloc both loses the old buffer pointer and NULL-dereferences. Contrast inside the same file set: `_SMS_VLC_AllocTable` in `src/SMS_VLC.c` does check its realloc, so the unchecked sites are inconsistencies, not house style.
- **Fix:** Add NULL checks at the three sites; on failure return/propagate the codec-init error (init paths already have an `allocation_error` convention, e.g. `MSMPEG4_Init`), and for the realloc use a temporary pointer so the old stash survives failure.
- **Benefit:** Removes init-time and low-memory NULL derefs in table setup shared by all MPEG-family decoders.
- **Effort:** Small. **Risk:** Low. **Confidence:** READ.

### RE-SMS-036 — List allocation contract is inconsistent: `Push`/`PushBackBuf` unchecked, 18 call sites dereference the result
- **Category:** 6 (bugs/security)
- **Locations:** `src/SMS_List.c:17-21` (`SMS_ListPush`: `calloc` result `strcpy`'d with no NULL check), `:57-71` (`SMS_ListPushBackBuf`: `calloc` result linked and dereferenced with no NULL check — contrast `SMS_ListPushBack` at `:36-40`, which does check), `:319-325` (`SMS_ListAppend` dereferences `SMS_ListPushBack(...) -> m_Param`). Script-counted 18 direct `SMS_ListPushBack (…) -> m_Param = …` dereferences, e.g. `src/SMS_History.c:140`, `src/SMS_Config.c:366`, `src/SMS_FileDir.c:311,340,392,437,453,519,593`, `src/SMS_GUISMBrowser.c:563,571`, `src/SMS_GUI.c:929`, `src/SMS_GUIMenu.c:381`. Also `src/SMS_SubtitleContext.c:278,334` assign through `apList->m_pTail` immediately after a push
- **Problem:** `SMS_ListPushBack` already returns NULL on allocation failure; every `-> m_Param` call site converts ordinary memory exhaustion (large directory, large SMB share list, long playlist) into a NULL-pointer crash instead of a dropped entry. `Push` and `PushBackBuf` are worse: they crash inside the list code itself, so no caller can defend against them.
- **Fix:** Make the contract uniform and minimal: add the same NULL check `PushBack` already has to `Push` and `PushBackBuf` (return without linking / return NULL), and at the 18 dereference sites capture the node and set `m_Param` only if non-NULL. No new API, no wrapper layer.
- **Benefit:** One mechanism fixed everywhere it occurs; turns OOM crashes across browsing, history, SMB and menus into graceful truncation.
- **Effort:** Medium (many small sites, all mechanical). **Risk:** Low. **Confidence:** READ.

### RE-SMS-037 — History load: unbounded, unchecked, and triplicated record loop
- **Category:** 6 (bugs/security)
- **Locations:** `src/SMS_History.c:41-52` (CWD/fio loop), `:78-89` (BDM mc/fio loop — near-identical), `:97-108` (non-BDM `MC_ReadS` loop — third copy); cap `HIST_SIZE 32` at `:20` is enforced only on add (`:141`), never on load
- **Problem:** Each loop reads a file-supplied `unsigned short lSize`, allocates `lSize + 1` via `SMS_ListPushBackBuf`, and ignores the return values of the payload and `m_Param` reads. Nothing limits the number of records: a crafted `SMS.hst` grows the list until memory is exhausted (each record also costs a list node), and `SMS_ListPushBackBuf` itself does not check its `calloc` (RE-SMS-036), so exhaustion becomes a NULL dereference mid-load. Short/truncated reads are silently accepted as entries.
- **Fix:** Collapse the three loops into one loop over a small read-callback (or load once via fio in both BDM and non-BDM paths, as the neighbouring palette/locale code already migrated to fio), stop after `HIST_SIZE` records, require both payload and param reads to return in full, and stop on a failed allocation.
- **Justification:** The callback/helper replaces three copies with one — net deletion.
- **Benefit:** Bounds a user-file-driven allocation loop, removes ~40 duplicated lines, and makes truncated history files stop instead of inventing entries.
- **Effort:** Medium. **Risk:** Medium (three device paths; keep the mc fallback behaviour identical). **Confidence:** READ.

### RE-SMS-038 — Subtitle loaders: unchecked node allocation and unbounded node count from the subtitle file
- **Category:** 6 (bugs/security)
- **Locations:** `src/SMS_SubtitleContext.c:451-452` (`_load_sub`: `lpNode = malloc(sizeof(SubNode)); lpNode->m_pList = SMS_ListInit();` — neither checked), `:592-593` (`_load_srt`: same pattern), `:714-716` (`_produce_packets`: `memalign` and `calloc(m_Cnt, …)` results used unchecked), `:278,334` (`_add_line` via RE-SMS-036)
- **Problem:** Subtitle files are external data (downloaded `.srt`/`.sub` next to media). Both parsers allocate one `SubNode` plus a list per entry with no count cap and no allocation check: a many-line file exhausts EE memory, and the first failed `malloc`/`SMS_ListInit` is dereferenced on the next line. `_produce_packets` then trusts `m_Cnt` and unchecked DMA/packet allocations.
- **Fix:** Check the node and its list before linking (free and fail the load on NULL, as other loaders in this tree already do), and cap accepted entries at a fixed maximum consistent with `SUB_MAXLEN`-era memory (drop further entries, keep playing). No new format support.
- **Benefit:** Removes crash/OOM from a routine untrusted input — subtitles are the most commonly user-supplied file in the player.
- **Effort:** Small–Medium. **Risk:** Low. **Confidence:** READ.

### RE-SMS-039 — Object constructors and menu builders dereference `calloc`/`malloc` results without a check
- **Category:** 6 (bugs/security)
- **Locations:** `src/SMS_GUIMenu.c:405-414` (`GUI_CreateMenu`), `:379` (`GUI_MenuPushState`), `src/SMS_GUIDesktop.c:132` (`GUI_CreateVersion`), `:730`, `:799`, `src/SMS_GUICmdProc.c:85`, `src/SMS_GUIFileMenu.c:1645` (`GUI_CreateFileMenu`), `src/SMS_GUIMenuSMS.c:3027-3029` (consumes `GUI_CreateMenu` result) — all `retVal = calloc(...); retVal->Render = …` unchecked. Same pattern for menu arrays: `src/SMS_GUIMiniBrowser.c:132-133` and `src/SMS_GUICmdProc.c:363-364` (`_playlist_menu`: both `calloc`s unchecked, then indexed; also `&lpItems[lCount - 1]` when `lCount == 0`). Support objects: `src/SMS_RingBuffer.c:21-36` (`SMS_RingBufferInit`: `malloc` result field-written immediately), `src/SMS_PCM.c:24-31` (`SMS_Codec_PCM_Open`: `calloc` result field-written immediately)
- **Problem:** This is the GUI's universal construction idiom, so under memory pressure (the normal state after a large media file) opening a menu crashes instead of failing. The playlist case additionally underflows its item array on an empty selection.
- **Fix:** One idiom, applied mechanically: check the allocation, free any sibling allocation, and return NULL; callers of the `GUI_Create*` family already tolerate NULL objects in several paths — where they do not, the NULL return at least localises the failure to creation. Guard the `lCount == 0` playlist case before `&lpItems[lCount - 1]`.
- **Benefit:** Removes the crash-on-open class across every menu/desktop object at once.
- **Effort:** Medium (many sites, one pattern). **Risk:** Low–Medium (NULL-object handling at a few call sites needs the check, not a redesign). **Confidence:** READ.

### RE-SMS-040 — Directory-tree scan/walk/destroy recurse to filesystem depth, and tree init is unchecked
- **Category:** 6 (bugs/security)
- **Locations:** `src/SMS_DirTree.c:46-65` (`SMS_DirTreeInit`: `calloc` result passed straight to `SMS_DirAdd(&retVal->m_Root, …)`; also VLA `char lRoot[strlen(apRoot) + 2]` and `lRoot[lLen - 1]` read with no empty-string guard), recursion at `:153` (`Scan`), `:224` (`Walk`), `:258` (`Destroy`); callers `src/SMS_CopyTree.c:150` (`SMS_CopyTree`: result used unchecked, `strrchr(apSrc, '/') + 1` at `:151` with no NULL check if the path has no slash) and `:252` (`SMS_DeleteTree`)
- **Problem:** Copy/delete of a deeply nested tree (user media on HDD/USB/SMB) recurses once per directory level in three separate walks, on the EE's small stacks, with no depth bound — a deep-but-legitimate tree crashes the browser action. The unchecked init and `strrchr` are the same "assume success" pattern at the entry point.
- **Fix:** Check `SMS_DirTreeInit`'s result at both callers (and inside init before `SMS_DirAdd`), NULL-check the `strrchr`, and add one depth counter threaded through Scan/Walk/Destroy with a sane ceiling that sets `m_Error` (the tree already has that error channel) instead of recursing further.
- **Benefit:** Bounds a user-data-driven stack exhaustion in the only copy/delete path.
- **Effort:** Medium. **Risk:** Medium (three recursive functions must share the ceiling consistently). **Confidence:** READ for the recursion/unchecked patterns; INFERRED for the exact depth at which a real stack overflows.

### RE-SMS-041 — lwip_close() writes errno through a NULL socket pointer on any bad descriptor
- **Category:** 6 (bugs/security)
- **Locations:** `iop/SMSTCPIP/src/sockets.c:235-250` (`sock_set_errno(sock, lErrNo)` after the `!sock` branch)
- **Problem:** When `get_socket` returns NULL (any in-range but unopened fd — no out-of-range value needed), the error branch sets `retVal = -1`, then unconditionally executes `sock_set_errno(sock, lErrNo)`, whose macro body is `sk->err = (e)` — a NULL-pointer write. Closing an unopened descriptor crashes the stack instead of returning `EBADF`.
- **Fix:** In the `!sock` branch use `set_errno(EBADF)`; call `sock_set_errno` only in the non-NULL branch.
- **Benefit:** Turns a guaranteed crash on a routine error path into the intended −1/EBADF.
- **Effort:** Trivial. **Risk:** Low. **Confidence:** READ.

### RE-SMS-042 — do_bind() uses udp_new()/tcp_new() results without the NULL check its sibling functions have
- **Category:** 6 (bugs/security)
- **Locations:** `iop/SMSTCPIP/src/api_msg.c:393-410` (`udp_new()` at :393/:398/:403 fed straight into `udp_setflags`/`udp_recv`/`udp_bind`; `tcp_new()` at :409-410 fed into `setup_tcp`/`tcp_bind`)
- **Problem:** `do_newconn` (:285-330) and `do_connect` (:455-500) check every allocation and post ERR_MEM; `do_bind` checks none. When the 15-entry UDP/TCP PCB memp pools are exhausted, `bind()` on a fresh netconn dereferences NULL inside the tcpip thread, taking down the whole stack, not just the caller.
- **Fix:** Copy the exact check pattern already in `do_connect` (set `conn->err = ERR_MEM`, post mbox, return). (RE-SMS-133 removes the duplication that produced this divergence.)
- **Benefit:** Pool exhaustion becomes a clean failure instead of a tcpip-thread crash.
- **Effort:** Trivial. **Risk:** Low. **Confidence:** READ.

### RE-SMS-043 — icmp_dest_unreach() dereferences its pbuf_alloc() result unchecked and over-reads the offending packet
- **Category:** 6 (bugs/security)
- **Locations:** `iop/SMSTCPIP/src/icmp.c:136-148` (`q = pbuf_alloc(...)`, then `idur = q->payload` with no NULL check; `mips_memcpy(q->payload + 8, p->payload, IP_HLEN + 8)` with no check that `p->tot_len >= IP_HLEN + 8`); same unchecked-alloc pattern at `icmp.c:169` (`icmp_time_exceeded`, currently compiled out with IP_FORWARD=0); live callers: `iop/SMSTCPIP/src/udp.c` `udp_input` no-match path and `iop/SMSTCPIP/src/ip.c` `ip_input` unknown-protocol path
- **Problem:** Any UDP datagram to a closed port (or unknown-protocol IP packet) reaches this function. When the 12 KB `mem` heap cannot satisfy the RAM allocation, `q` is NULL and the tcpip/driver thread crashes building the error reply; a short offending packet is over-read by up to 28 bytes regardless.
- **Fix:** `if (q == NULL) return;` after the allocation, and require `p->tot_len >= IP_HLEN + 8` (else copy only `p->tot_len` / return). Apply identically at :169 for the day IP_FORWARD is enabled.
- **Benefit:** A routine, remotely triggerable path stops being a crash under memory pressure.
- **Effort:** Trivial. **Risk:** Low. **Confidence:** READ.

### RE-SMS-044 — ip_frag() dereferences both of its pbuf_alloc() results unchecked
- **Category:** 6 (bugs/security)
- **Locations:** `iop/SMSTCPIP/src/ip_frag.c:262-263` (`rambuf = pbuf_alloc(PBUF_LINK, 0, PBUF_REF)` then `rambuf->tot_len = …` immediately); `ip_frag.c:310-312` (`header = pbuf_alloc(PBUF_LINK, 0, PBUF_RAM)` then `pbuf_chain(header, rambuf); netif->output(netif, header, dest)` — `pbuf_cat` tolerates NULL but `etharp_output`/`pbuf_header` on a NULL header does not)
- **Problem:** Fragmenting an outbound datagram larger than the MTU under memp exhaustion crashes in the sending thread instead of returning ERR_MEM. `ip_frag`'s contract already returns `err_t`, so callers (`ip_output_if`) can propagate the failure.
- **Fix:** NULL-check `rambuf` (return ERR_MEM) and `header` (free `rambuf`, return ERR_MEM).
- **Benefit:** Outbound fragmentation fails cleanly under the same pressure that makes RE-SMS-043 fire.
- **Effort:** Trivial. **Risk:** Low. **Confidence:** READ.

### RE-SMS-045 — lwip_accept() dereferences the accepted conn, addr and addrlen without NULL checks
- **Category:** 6 (bugs/security)
- **Locations:** `iop/SMSTCPIP/src/sockets.c:159-186` (`netconn_accept` result used at :161 by `netconn_peer`; `*addrlen` read at :172 and `mips_memcpy(addr, …)` with no NULL check on `addr`/`addrlen`; final `sock_set_errno(sock, 0)` applied to the *new* socket after `sock = get_socket(newsock)` at :183)
- **Problem:** `netconn_accept` returns whatever the acceptmbox delivers — `err_tcp` posts NULL there when a listening PCB errors — so `newconn` can be NULL and `netconn_peer(NULL, …)` / the subsequent code deref it. Passing `addr == NULL` (legal POSIX for "don't care") crashes at `*addrlen`. On the failure path (`alloc_socket` == −1) the errno is set on the listening socket, but on success the listening socket's errno is never cleared — the code clears the new socket's instead.
- **Fix:** Check `newconn == NULL` (return −1/ECONNABORTED), guard the copy with `if (addr && addrlen)`, and set errno on the listening `sock` before reassigning it.
- **Benefit:** accept() survives listener errors and the standard NULL-address call form.
- **Effort:** Small. **Risk:** Low. **Confidence:** READ.

### RE-SMS-046 — netconn_recv() dereferences the PCB and the received buffer without the NULL checks its own TCP branch demonstrates
- **Category:** 6 (bugs/security)
- **Locations:** `iop/SMSTCPIP/src/api_lib.c:403` (`conn->pcb.tcp->state` — `err_tcp` sets `pcb.tcp = NULL`, `api_msg.c:183`, so a recv after a reset/close races into a NULL deref); `api_lib.c:455-456` (UDP arm: `buf->p->tot_len` immediately after `sys_mbox_fetch` — NULL posts and `recv_udp`'s design both allow a NULL/empty delivery to reach here; the TCP arm at :418-440 handles `p == NULL` explicitly, the UDP arm has no equivalent)
- **Problem:** Error/EOF delivery is a normal event (peer RST, abort). For UDP conns the recv path assumes a non-NULL netbuf; for TCP conns it assumes the PCB still exists. Both assumptions are falsified by the stack's own error callbacks in the same files.
- **Fix:** Guard both: `if (conn->pcb.tcp == NULL) { conn->err = ERR_CONN; return NULL; }` before :403, and in the UDP arm treat `buf == NULL` like the TCP arm treats `p == NULL` (free recvmbox, return NULL).
- **Benefit:** Orderly EOF/error return instead of a crash on connection teardown — a path every long-lived UDP/TCP session eventually takes.
- **Effort:** Trivial. **Risk:** Low. **Confidence:** READ.

### RE-SMS-047 — lwip_recvfrom() dereferences netbuf_fromaddr() which is NULL for every TCP buffer
- **Category:** 6 (bugs/security)
- **Locations:** `iop/SMSTCPIP/src/sockets.c:355-363` (`addr = netbuf_fromaddr(buf); … addr->addr`); `iop/SMSTCPIP/src/api_lib.c` `netconn_recv` TCP arm sets `buf->fromaddr = NULL` explicitly
- **Problem:** `recvfrom(tcp_fd, …, &from, &fromlen)` — a legal call — dereferences NULL when filling `from`. UDP buffers always carry a fromaddr; TCP buffers never do, and the sockets layer does not distinguish.
- **Fix:** `if (addr == NULL)` fill `sin` with zeroes / skip the copy, mirroring how the TCP arm already signals "no origin" with `fromport = 0`.
- **Benefit:** Removes a crash on a standard call combination.
- **Effort:** Trivial. **Risk:** Low. **Confidence:** READ.

### RE-SMS-048 — netbuf_ref() dereferences its pbuf_alloc() result unchecked
- **Category:** 6 (bugs/security)
- **Locations:** `iop/SMSTCPIP/src/api_lib.c:71-79` (`buf->p = pbuf_alloc(PBUF_TRANSPORT, 0, PBUF_REF);` then `buf->p->payload = …` unconditionally); sole in-tree caller chain: `lwip_send` UDP arm (`sockets.c`) → `netconn_send` → `do_send`/`udp_send`, which also assume `buf->p` is valid
- **Problem:** When the MEMP_PBUF pool (40 entries) is exhausted, a UDP `lwip_send` crashes in `netbuf_ref` instead of returning ENOBUFS — the caller *does* check `netbuf_new()` one line earlier, so the unchecked second allocation is an inconsistency, not a style.
- **Fix:** In `netbuf_ref`, if the allocation fails leave `buf->p = NULL`; in `lwip_send`'s UDP arm treat `buf->p == NULL` after `netbuf_ref` as the existing ENOBUFS return (the check scaffolding is already there).
- **Benefit:** UDP send under pool pressure returns an error instead of crashing.
- **Effort:** Trivial. **Risk:** Low. **Confidence:** READ.

### RE-SMS-049 — pbuf_header() can underflow len/tot_len, and its main wire-facing caller strips 14 bytes without any length check
- **Category:** 6 (bugs/security)
- **Locations:** `iop/SMSTCPIP/src/pbuf.c:291-315` (RAM/POOL arm adjusts `p->len += header_size` with only a payload-lower-bound check; a negative step larger than `len` wraps the u16 lengths); `iop/SMSTCPIP/src/ps2ip.c:245` (live path, QUEUE_INPUT=0: `pbuf_header(pInput, -14)` after `etharp_ip_input`, with no `pInput->tot_len >= 14` check and the ethertype read at :238 taken from `pInput->payload` before any length validation); related unchecked header readers: `iop/SMSTCPIP/src/etharp.c:391-396` (`etharp_ip_input` casts payload to a 34-byte `ethip_hdr` unchecked), `iop/SMSTCPIP/src/ip.c:202-230` (`ip_input` rejects `iphdrlen > p->len` but never `iphdrlen < IP_HLEN`)
- **Problem:** A short Ethernet frame (runt) from the LAN makes the input path manufacture a pbuf whose lengths claim far more data than arrived; downstream IP/TCP/UDP parsing then runs on stale pool bytes as if they were wire data. The pool buffer is 1540 bytes, so this is primarily a data-integrity / parser-confusion defect rather than a clean segfault (severity INFERRED).
- **Fix:** In `ps2ip_input`, drop (`pbuf_free` + return) when `pInput->tot_len < 14` before touching the header; in `pbuf_header`'s RAM/POOL arm, fail (return 1) when `header_size < 0 && -header_size > p->len`. In `ip_input`, reject `iphdrlen < IP_HLEN` alongside the existing `>` check.
- **Benefit:** Runts stop reaching three protocol parsers with fabricated lengths.
- **Effort:** Small. **Risk:** Low. **Confidence:** READ (severity INFERRED).

### RE-SMS-050 — UDPFS `_request` callers never check the reply actually arrived in full (SMSUdpfs)
- **Category:** 6 (bugs/security)
- **Locations:** `iop/SMSUdpfs/udpfs/src/udpfs_core.c:166-171` (`_path_op`), `:373-383` (open), `:519-532` (lseek), `:622-642` (getstat), `:582-587` (dread name copy); helper at `:72-80`
- **Problem:** `_request()` returns the received byte count, but every caller checks only `ret < 0` and then reads the full reply struct out of the shared static `g_rx_buf`. A short reply (truncated datagram, version-skewed server) leaves the tail of the previous reply in place, and the caller acts on it: open returns a stale `handle`, lseek returns a stale position, getstat copies stale timestamps/sizes to the EE. `_path_op` is worse — it checks neither length nor `msg_type` and returns `reply->result` verbatim for mkdir/remove/rmdir. In dread, `name_len` is checked against the caller's `name_max` but not against the bytes actually received (`ret - sizeof(reply)`), so stale buffer bytes are copied out as a filename; and `name_len >= name_max` silently returns success (1) with an empty name. This is exactly the failure family the same file already fixed for the read path in `_recv_with_result` (`udpfs_core.c:89-136`, with an explanatory comment) — the fix pattern exists in-tree and was not applied here.
- **Fix:** One check in `_request` (or at each call site): fail with `-EIO` when `ret` is smaller than the reply struct the caller is about to read; in dread additionally require `name_len <= ret - sizeof(udpfs_msg_dread_reply_t)`. Reuses the file's own established pattern; adds no new mechanism.
- **Benefit:** Closes the remaining stale-data path in the live network filesystem; a misbehaving server currently produces wrong handles/stats instead of a clean error.
- **Effort:** Small. **Risk:** Low — a conforming server always sends full replies. **Confidence:** READ.

### RE-SMS-051 — smbman-sms: two query replies parsed with none of the validation its sibling functions apply
- **Category:** 6 (bugs/security)
- **Locations:** `iop/smbman-sms/src/smb.c:908-930` (`smb_QueryPathInformation`), `iop/smbman-sms/src/smb.c:799-838` (`smb_QueryInformationDisk`)
- **Problem:** `smb_QueryPathInformation` casts `&SMB_buf.smb.u8buff[QPIRsp->smbTrans.DataOffset]` to `BasicFileInfo_t`/`StandardFileInfo_t` and reads the fields without ever checking `DataOffset`, `DataCount`, or the received length `r` — a short or hostile server reply makes it read stale buffer contents as file times/size and report success. `smb_QueryInformationDisk` likewise checks only `r <= 0` and the status word before reading `TotalUnits`/`BlocksPerUnit`/`BlockSize`/`FreeUnits` from a possibly truncated reply. The same file demonstrates the intended standard elsewhere: `smb_NetShareEnum` validates offsets/counts (`smb.c:739-795`), `smb_FindFirstNext2` validates parameter/data bounds (`smb.c:1540-1581`), `smb_ReadAndX` validates wire length (`smb.c:1127-1151`). These two functions are the unpatched remainder, in the live SMB client.
- **Fix:** Apply the file's own existing pattern: require `r` to cover the header plus the info struct, and `DataOffset + sizeof(*FileInfo) <= r`, before dereferencing. No new helpers.
- **Benefit:** Brings the last two unvalidated reply parsers in the shipped SMB client up to the file's own standard.
- **Effort:** Small. **Risk:** Low. **Confidence:** READ.

### RE-SMS-052 — smbman-sms `smb_Echo` copies an EE-supplied length into the SMB buffer unchecked
- **Category:** 6 (bugs/security)
- **Locations:** `iop/smbman-sms/src/smb.c:1648-1664` (`smb_Echo`); caller `iop/smbman-sms/src/smb_fio.c:1094-1097` (`smb_EchoServer`, devctl `SMB_DEVCTL_ECHO`)
- **Problem:** The devctl path passes `echo->len` from the EE request straight into `smb_Echo`, which does `memcpy(&ER->ByteField[0], echo, (u16)len)` into the shared SMB packet buffer (`MAX_SMB_BUF 4096` + `MAX_SMB_BUF_HDR 128`, `iop/smbman-sms/src/smb.h:892-893`) with no cap. A len above the remaining buffer space overruns `SMB_buf` into adjacent statics; the `(u16)` cast also silently truncates larger values so the sent `ByteCount` and the copied count can disagree with the caller's intent. The internal keepalive caller uses a fixed 18 bytes and is safe — the exposure is only the devctl entry, which validates neither `len` nor the devctl argument length.
- **Fix:** Reject `len` greater than the space actually available in the echo request buffer (one comparison in `smb_Echo`, covering both callers).
- **Benefit:** Removes an EE→IOP overflow in the live SMB module's only arbitrary-length copy.
- **Effort:** Small. **Risk:** Low. **Confidence:** READ.

### RE-SMS-053 — `lzma2_get_uncompressed_size` walks the XZ index with no bounds at all
- **Category:** 6 (bugs/security)
- **Locations:** `src/lzma2.c:51-82` (`lzma2_decode_size`), `src/lzma2.c:84-122` (`lzma2_get_uncompressed_size`), `src/lzma2.c:29-49` (`decode`); callers `src/SMS_IOP.c:237`, `src/SMS_EE.c:950`, `src/SMS_GS_2.c:567`
- **Problem:** Four structural defects in one mechanism: (1) no `size >= 12` check — `footer = (xz_footer_t*)(buf + size - 12)` reads before the buffer for a short input; (2) `footer->backward_size` is trusted unchecked, so `index = (unsigned char*)footer - ((i+1)*4)` can point arbitrarily far *before* the buffer; (3) `lzma2_decode_size` receives no end pointer and sums `records` index records by walking forward without limit; (4) every `buf += decode(...)` ignores `decode()`'s 0-on-error return, so a malformed varint makes the walk re-read the same bytes as successive fields instead of failing. Most inputs are compile-time blobs, but one is a **user-supplied file**: `GSFont_Load` (`src/SMS_GS_2.c:567`) runs this walk over the body of an `.mtf` font file read from disk, and its only validation is equality with a size stored in that same file's header — a crafted font makes the walk read out of bounds and can make the subsequent `malloc(lDataSize)` / decode sizes arbitrary.
- **Fix:** Thread the buffer end through: reject `size < 12`, reject a computed index outside `[buf, footer]`, make `lzma2_decode_size` stop at the footer and fail (return 0) when any `decode()` returns 0. All inside the existing two functions.
- **Benefit:** Bounds the one uncompressed-size oracle every XZ consumer in the tree trusts for allocation sizes (IRX modules, JPEG VU programs, fonts, icons).
- **Effort:** Small. **Risk:** Low — all in-tree blobs script-verified well-formed, so strict bounds change nothing for valid inputs. **Confidence:** READ.

### RE-SMS-054 — Unbounded whole-file loads sized by the file itself (language, font, asset copy, IRX module)
- **Category:** 4 (size/memory)
- **Locations:** `src/SMS_Locale.c:841-845` (CWD `.lng`), `:874-878` (BDM mc `.lng`), `:888-892` (non-BDM mc `.lng`) — all `malloc(lSize + 1); fioRead/MC_ReadS(…, lSize)` with `lSize` from seek-to-end and no cap; `src/SMS_Config.c:286-307` (`_load_font`: `if (lSize > lFontSize) lpBuff = malloc(lSize)` then read the whole `.mtf`); `src/SMS_GUIFileCtxMenu.c:143-196` (`_copy_file_to_mc`: whole browsed file `malloc(lSize)` + read, purely to write it back out); `src/SMS_RC.c:82-138` (`_load_rmman`: `malloc(lSize)` with **no NULL check at all**, unchecked `fioRead`, then the buffer is patched and executed as an IOP module)
- **Problem:** One mechanism in four places: allocation and read length are whatever the file says. A large `.lng`/`.mtf`/asset does not fail cleanly — it consumes all EE RAM (or, in `_load_rmman`, dereferences NULL and then hands a short/garbage buffer to `SifExecModuleBuffer`). `_copy_file_to_mc` is the worst offender functionally: a streaming copy already exists in the same tree (`SMS_CopyFile`, `src/SMS_CopyTree.c`), so the whole-file buffer buys nothing. Contrast `DrawSkin` (`src/SMS_GUIDesktop.c:155-317`), which caps skins at `0xFFFF0` and checks read length — proof the tree already knows the pattern.
- **Fix:** Cap each load at a format-sane maximum (language/font: a fixed ceiling well above any real pack; skin's existing `0xFFFF0` is the in-tree precedent), check the allocation and the read length, and rewrite `_copy_file_to_mc` on top of the existing streaming copy instead of a whole-file buffer (preserving its read-before-truncate self-copy guard).
- **Benefit:** Bounds four file-driven allocations, deletes the duplicate copy implementation, and closes the NULL-deref in the IRX loader.
- **Effort:** Medium. **Risk:** Medium for `_copy_file_to_mc` (self-copy case must stay guarded); Low for the caps. **Confidence:** READ.

### RE-SMS-055 — `SMSContainer_DefReadPacket` dereferences a possibly-NULL packet, computes a clamp it never uses, and unallocates from stream 0 regardless of index
- **Category:** 6 (bugs/security)
- **Locations:** `src/SMS_Container.c:308-335` (allocation :312-315, dead clamp :320-321, `Read` with full `lPackSize` :322, `SMS_RingBufferUnalloc(m_pStm[0] …)` :325-326)
- **Problem:** Three defects in the shared packet reader used by FLAC/MP3/OGG. (1) `AllocPacket` returns NULL when the ring buffer cannot fit `lPackSize + 64` — a routine back-pressure event — and the non-EOF path immediately reads into `lpPkt->m_pData`. The sibling readers (AAC, AC3, ASF, MOV, MPEG-PS) all test their packet pointer; this one doesn't. (2) `lnRead` is computed as the bytes actually remaining in the file and then never used — the `Read` still asks for the full `lPackSize` (behaviour happens to be equivalent because the underlying read returns short, so the variable is pure dead code that invites a future mis-edit). (3) On a zero read it unallocates `lPackSize + 64` from `m_pStm[0]` even though the packet was allocated from `m_pStm[*apIdx]`; today every user sets `m_DefPackIdx = 0`, so this is latent, but the function is written generically and the mismatch corrupts the wrong ring if that ever changes.
- **Fix:** Return −1 when `lpPkt` is NULL; delete the `lnRead` computation and keep the full-size `Read` (or use `lnRead` — one of the two, not both); unallocate from `m_pStm[*apIdx]`.
- **Benefit:** Removes a crash on ordinary ring-buffer back-pressure in three audio formats and a latent wrong-ring corruption.
- **Effort:** S. **Risk:** Low. **Confidence:** READ.

### RE-SMS-056 — ASF fragment reassembly assigns `realloc`'s result and the new capacity before checking either
- **Category:** 6 (bugs/security)
- **Locations:** `src/SMS_ContainerASF.c:537` (in `_ReadPacket`)
- **Problem:** `lpASFStm->pkt.data = realloc(lpASFStm->pkt.data, lpASFStm->pkt.alloc = lpASFStm->pkt.size);` — the capacity field is assigned inside the call, so on failure `pkt.data` becomes NULL (the old buffer is lost) while `pkt.alloc` claims the new size. `pkt.size` here is the file-controlled object size (bounded to < 2^24 by `src/SMS_ContainerASF.c:433`, i.e. up to ~16 MB — a plausible failure on this heap). The later `!pkt.data` guard (`src/SMS_ContainerASF.c:565-568`) prevents a write through NULL, but the stream is left with a lying capacity and a leaked buffer, and any future edit that trusts `pkt.alloc` becomes an overflow.
- **Fix:** Reallocate into a local; only on success store pointer and capacity; on failure drop the fragment (reset `pkt.size`/`frag_offset`, which the surrounding code already does in its other reset paths).
- **Benefit:** OOM during ASF playback degrades to a dropped object instead of heap-state corruption.
- **Effort:** S. **Risk:** Low. **Confidence:** READ.

### RE-SMS-057 — `SMSContainer_SetName` scans backwards for a path separator with no lower bound
- **Category:** 6 (bugs/security)
- **Locations:** `src/SMS_Container.c:284-303` (scan loop :292-296)
- **Problem:** `lpSlash` starts at the last character of `m_pPath` and is decremented until it hits `'\\'` or `'/'`, with no stop at the start of the string (and no empty-string case: `strlen - 1` on an empty path starts one byte *before* the buffer). Any path containing no separator is walked off the front of its allocation, and `lpSlash + 1` is then pushed into the playlist as the display name. Reachability is via sub-containers: M3U opens entries through `_open_file` (`src/SMS_ContainerM3U.c:88-110`), which concatenates `g_CWD` + entry name — an entry opened from a root-level playlist can produce a constructed path whose final component has no separator of its own in the scanned string if the prefix contributes none (INFERRED for that specific path shape; the missing bound itself is READ).
- **Fix:** Stop the scan at `apFileCtx->m_pPath` and treat "no separator" as "whole string is the name" — the outcome the code already produces for a leading separator.
- **Benefit:** Removes an out-of-bounds read (and a wild playlist pointer) on separator-less paths, for every container that calls `SetName` — which is all of them.
- **Effort:** S. **Risk:** Low. **Confidence:** High for the missing bound (READ); Med for reachability via playlist sub-files (INFERRED).

### RE-SMS-058 — MOV `mdhd` handler dereferences the stream that the M4A `hdlr` handler may already have freed
- **Category:** 6 (bugs/security)
- **Locations:** `src/SMS_ContainerMOV.c:297-320` (`_mov_read_hdlr_m4a`: frees codec + stream and NULLs the slot at :311-313, without decrementing `m_nStm`), `src/SMS_ContainerMOV.c:224-240` (`_mov_read_mdhd`: `lpStm = m_pStm[m_nStm-1]`, dereferenced with no NULL test)
- **Problem:** On the audio-only (M4A) path, meeting a video `trak` frees its stream in place and leaves a NULL slot that `_mov_read_header` only compacts away later. Most sibling handlers defend against exactly this — `stsd`, `esds`, `stts`, `stsc`, `stsz`, `stco`, `ctts`, `stss` and `elst` all test the slot (the `esds` comment at `src/SMS_ContainerMOV.c:429-435` documents the hazard explicitly). `mdhd` does not. Atom order inside `mdia` is dispatched in file order by `_mov_read_dflt` and is not enforced, so a crafted file that places `hdlr` before `mdhd` inside the same `mdia` makes `mdhd` write through NULL. (`tkhd` at :215-223 has the same shape but sits at `trak` level before `mdia`, so it cannot follow its own trak's `hdlr`; it is listed for completeness of the pattern, not as a reachable site.)
- **Fix:** Give `_mov_read_mdhd` the same guard its siblings have: if the slot is NULL, skip the atom body and return.
- **Benefit:** Closes the one remaining hole in an otherwise systematically defended pattern, on the format (MP4/M4A) whose files are most likely to be reordered or corrupt.
- **Effort:** S. **Risk:** Low. **Confidence:** Med (free/NULL and missing guard READ; INFERRED that `mdia` child order is attacker-controllable, based on the order-agnostic dispatcher in `_mov_read_dflt`, `src/SMS_ContainerMOV.c:795-860`).

### RE-SMS-059 — FLAC: fixed-predictor subframe reads up to 4 samples *before* the channel buffer for predictor orders 0–3
- **Category:** 6 (bugs/security)
- **Locations:** `src/SMS_FLAC.c:182-185` (`decode_subframe_fixed`: `a = decoded[pred_order-1]; b = a - decoded[pred_order-2]; c = … decoded[pred_order-3]; d = … decoded[pred_order-4];` — all four computed unconditionally, before the `switch (pred_order)` at :187)
- **Problem:** For `pred_order` 0–3 — the common cases in real FLAC files — the code reads `decoded[-1]` down to `decoded[-4]` even though the switch then ignores most of those values. For channel 0 that is a heap under-read (into allocator metadata); for channel 1 it reads the tail of channel 0's buffer. The values are discarded, so output is unaffected, but the read itself is out of bounds on every affected subframe.
- **Fix:** Compute each of `b`, `c`, `d` only when the order needs it (guard with `if (pred_order > 1/2/3)` or move the computations into the switch cases). Pure restructuring of four lines; nothing added.
- **Benefit:** Removes a per-subframe out-of-bounds read in the FLAC hot path.
- **Effort:** Trivial. **Risk:** None; the discarded values are provably unused for the affected orders. **Confidence:** READ.

### RE-SMS-060 — WMA: superframe loop trusts container `block_align` (0 → hang; partial final frame → over-read)
- **Category:** 6 (bugs/security)
- **Locations:** `src/SMS_WMA.c:2040-2100` (`wma_decode_superframe`: frame loop advances `lpBuf += lFrmSize` where `lFrmSize = lpCtx->block_align`, without clamping against the remaining input size); `src/SMS_WMA.c:1247` (`lpCtx->block_align = apCtx->m_BlockAlign;` — taken from the container with no validation in `wma_init`)
- **Problem:** Two failure modes from one unchecked field: (1) `block_align == 0` makes the loop never advance — the player hangs on a malformed ASF/WMA header; (2) when the final packet chunk is smaller than `block_align`, the loop keeps consuming frames past the end of the input buffer. Every other decoder in scope reconciles declared frame size against remaining bytes (e.g. AC3/DTS `Decode` loops); the WMA path doesn't.
- **Fix:** Reject `block_align == 0` in `wma_init` (fail codec open), and bound the superframe loop by the remaining input length.
- **Benefit:** Removes a hang and an over-read from malformed WMA/ASF files.
- **Effort:** Small. **Risk:** Low; valid ASF streams always have `block_align > 0` and packet-aligned frames. **Confidence:** READ for the loop structure and the missing init check; INFERRED for a container actually delivering `block_align == 0`.

### RE-SMS-061 — AC3: `_ac3_bit_allocate` can index `s_LATab` one past its 256 entries
- **Category:** 6 (bugs/security)
- **Locations:** `src/SMS_AC3.c:293` (`s_LATab[256]`, script-verified: 256 entries, non-decreasing −64…0); `src/SMS_AC3.c:1170` (`lPSD = lNext + s_LATab[(-lDelta) >> 1];`, sibling at :1176)
- **Problem:** `lDelta` is derived from exponent differences and takes multiples of 128; the value −512 — an exponent drop of exactly 4 groups between adjacent bands — yields index 256, one past the end of the table. The read lands in whatever static follows `s_LATab` and perturbs the PSD computation (audio-level consequence, plus a formal out-of-bounds read).
- **Fix:** Clamp the index to 255 at the two call sites (or bound `lDelta` once at the top of the delta loop).
- **Benefit:** Removes a one-element over-read in the bit-allocation inner loop.
- **Effort:** Trivial. **Risk:** None; index 255 is the table's own endpoint value (0), the natural saturation. **Confidence:** INFERRED (index arithmetic and table size are READ; that a real exponent sequence produces exactly −512 was derived, not executed).

### RE-SMS-062 — CDDA `_read_string` does `strlen` on an unterminated stack buffer filled from disc data
- **Category:** 6 (bugs/security)
- **Locations:** `src/SMS_FileContext.c:803-822` (`_read_string`: PS2 branch declares VLA `char lBuf[aMaxLen]`, `apCtx->Read(apCtx, lpBuf, aMaxLen)`, then `strlen(lpBuf)`); callers at `:845-846` (64/512 bytes) and in the directory walk (~`:859+`, 32 bytes) — all data from a CDDA filesystem image
- **Problem:** Nothing NUL-terminates `lpBuf`. If the stored string fills its field (no NUL in `aMaxLen` bytes), `strlen` reads past the stack VLA, and the subsequent `malloc(lLen + 1)` / `strcpy` pair copies that over-read length. The read result is also used only as a boolean — a short read is treated as a full string.
- **Fix:** Bound it in place: read at most `aMaxLen - 1`, force `lpBuf[aMaxLen - 1] = '\0'` (or use a `strnlen`-equivalent loop), and treat a short read as failure. No new helper needed beyond this function.
- **Benefit:** Removes a stack over-read driven by disc/image data at the single function all CDDA strings pass through.
- **Effort:** Small. **Risk:** Low. **Confidence:** READ.

### RE-SMS-063 — Fixed-buffer `strcpy` of paths and names that other code allows to be longer
- **Category:** 6 (bugs/security)
- **Locations:** `src/SMS_Player.c:1502-1504` (`char lPath[512]; strcpy(lPath, …->m_pPath)` — `m_pPath` is heap-sized from the full file path, and CWD paths in this tree are up to `SMS_CWD_CAPACITY` 1024, `include/SMS_Sounds.h:24`); `src/SMS_IOP.c:1871-1876` (`SMS_HDDMount`: `strcpy(lMountInfo.m_Path, aPath)` into `char m_Path[124]`, `include/SMS_ioctl.h:74`; caller builds the path from a partition name at `src/SMS_GUIFileMenu.c:1024-1033` and also `strcpy(g_Config.m_Partition /* char[256] */, _STR(apMenu->m_pCurrent))` at `:1038`); `src/SMS_MC.c:145,172` (`strcpy(s_MCmd.m_Name /* char[1024] */, apName)` in `MC_GetDir`/`MC_Open`, non-BDM only); `src/SMS_Config.c:575` (`SMS_ConfigUseFSPath`: `strcpy(s_pMC0SMC /* char[128] */, apPath)` — the header comment at the call sites in `src/SMS_IOP.c:1291-1292` assumes `<= 15` bytes, nothing enforces it)
- **Problem:** One mechanism: a destination size is asserted by a comment or by habit while the source length is controlled elsewhere (file path, partition name from disc, caller-supplied path). Each is a latent stack/global overflow; the player one is on the normal stop/playback-end path (`_Destroy` → history save).
- **Fix:** Length-check at each sink and fail/skip the operation (`sizeof(dst) - 1` bound, `strncpy`+force-NUL or an explicit `strlen` gate like `SMS_ConfigAssetPath` already uses at `src/SMS_Config.c:259-276`). No central string library.
- **Benefit:** Closes four overflow sinks with local, auditable checks.
- **Effort:** Small. **Risk:** Low. **Confidence:** READ for the size mismatches; INFERRED that a >512-char media path / >124-char mount path is reachable in normal use (paths are not length-limited upstream).

### RE-SMS-064 — IOP init failures masked by unconditional success returns (family, all modules)
- **Category:** 6 (bugs/security)
- **Locations:** USBD: `iop/USBD/src/hcd.c:340-341` (`AllocSysMemory` result `memset` with no NULL check), `iop/USBD/src/hcd.c:472,482,491` (`CreateThread` unchecked; `initHubDriver()` return ignored — it does check its own allocation and returns −1, `iop/USBD/src/hub.c:44-48`), `iop/USBD/src/usbd.c:263-265` (`CreateSema` unchecked, `hcdInit()` return ignored, `_start` returns 0), `iop/USBD/src/driver.c:246-247` (callback `CreateThread`/`StartThread` unchecked); SMSUMS: `iop/SMSUMS/src/ums.c:125-133` (`CreateMutex` ×2 and `CreateThread` unchecked, `StartThread` unconditional, `_ums_connect` returns 0); smbman-sms: `iop/smbman-sms/src/smb_fio.c:228-244` (`CreateMutex` ×2 + `CreateThread` unchecked, `StartThread` unconditional, `smb_init` returns 0), `iop/smbman-sms/src/smbman.c:32` (`_start` ignores `smb_initdev()`'s failure return and always returns `MODULE_RESIDENT_END`); SMSUdpfs: `iop/SMSUdpfs/udpfs/src/main.c:28-45` (`udpfs_init()`/`udpfs_bd_init()`/`fhi_init()` returns ignored, always `MODULE_RESIDENT_END`), `iop/SMSUdpfs/udpfs/src/udpfs_ioman.c:444` (`AddDrv` result ignored, returns 0); SMSMAP: `iop/SMSMAP/main.c:338-341` (`CreateEventFlag`/`CreateMutex` unchecked; `StartThread(CreateThread(...))` inline and unchecked); SMSPWROFF: `iop/SMSPWROFF/src/poweroff.c:166` (`StartThread(s_ThreadID = CreateThread(...))` unchecked); SMSCDVD: `iop/SMSCDVD/SMSCDVD.c:1186` (directory-cache `AllocSysMemory` stored with no NULL check before use)
- **Problem:** One mechanism repeated across seven modules: a thread/semaphore/allocation creation result is never tested, the object is used (or started) unconditionally, and the enclosing init reports success / resident-end regardless. On IOP memory exhaustion or thread-table pressure the result is a NULL dereference (hcd.c:341), a `StartThread` on an invalid id, or — worst for diagnosis — a module that loads "successfully" with no worker thread, no device registered, or no hub driver, failing later in unrelated-looking ways. The contrast cases exist in-tree: `hub.c:44-48` checks, `SMSCDVD.c:1224-1234` checks its `CreateThread`, `ums.c`'s `_connect_thread` checks its cache allocation — the checks were simply not propagated to the callers/peers.
- **Fix:** Per site, the smallest change: test the result and return the module's existing failure value (`MODULE_NO_RESIDENT_END`, −1, or the function's error return) instead of proceeding. No new abstractions — just stop discarding results the APIs already return.
- **Benefit:** Converts silent half-dead modules and NULL derefs into clean load failures across the whole IOP set.
- **Effort:** Medium (many sites, each trivial). **Risk:** Low. **Confidence:** READ.

### RE-SMS-065 — mem_free() never checks the block was actually in use, so a double free silently corrupts the heap free-list
- **Category:** 6 (bugs/security)
- **Locations:** `iop/SMSTCPIP/src/mem.c:92-113` (`mem->used = 0; plug_holes(mem);` with only a range check on the pointer; `plug_holes` at :60-84 merges neighbours based on the now-cleared flag)
- **Problem:** Freeing the same block twice re-runs the merge logic on an already-merged block, rewriting `next`/`prev` of neighbours from stale offsets and corrupting the singly-linked heap structure that every `mem_malloc` (pbufs, TCP segments, ICMP replies) walks. No in-tree double free is asserted here — this is the missing seatbelt that turns any present/future double free elsewhere (e.g. the pbuf refcount paths) from a detectable no-op into heap corruption (reachability INFERRED).
- **Fix:** `if (mem->used == 0) return;` before clearing — one comparison, using the flag the allocator already maintains.
- **Benefit:** Cheap hardening of the allocator every other subsystem depends on.
- **Effort:** Trivial. **Risk:** Low. **Confidence:** READ (exploitability/reachability INFERRED).

### RE-SMS-066 — ps2ip_getconfig() strcpy()s an unbounded name into a 4-byte field; netif_find() reads name[2] on any string
- **Category:** 6 (bugs/security)
- **Locations:** `iop/SMSTCPIP/src/ps2ip.c:85-93` (`strcpy(pInfo->netif_name, pszName)`; field is `char netif_name[4]` in `include/ps2ip_internal.h`); `iop/SMSTCPIP/src/netif.c` `netif_find()` (`num = name[2] - '0'` before any validation)
- **Problem:** Both functions take the interface name from the EE-side caller of the exported `ps2ip_getconfig`/`ps2ip_setconfig`. A name longer than 3 chars + NUL overwrites `ipaddr`/`netmask`/`gw` inside the caller's own `t_ip_info` (self-inflicted for a well-behaved caller, but the export gives no contract), and a 1–2 character name makes `netif_find` read past the string.
- **Fix:** Replace `strcpy` with a bounded copy of at most 3 chars + NUL (interface names in this stack are exactly 2 letters + digit, cf. `netif_find`'s own matching), and in `netif_find` reject names whose `name[2]` is not a digit before indexing further.
- **Benefit:** An exported config API stops corrupting its own output struct on malformed input.
- **Effort:** Trivial. **Risk:** Low. **Confidence:** READ.

### RE-SMS-067 — err_tcp() is the only PCB callback that dereferences its netconn argument unchecked
- **Category:** 6 (bugs/security)
- **Locations:** `iop/SMSTCPIP/src/api_msg.c:177-205` (`conn = arg; conn->pcb.tcp = NULL; conn->err = err; …` with no `conn == NULL` test); siblings in the same file all check: `recv_tcp` (:~120), `poll_tcp`, `sent_tcp`, `do_connected` (:~445, `if (conn == NULL) return ERR_VAL;`)
- **Problem:** `err_tcp` is invoked via `TCP_EVENT_ERR` from abort/timeout paths (`tcp_slowtmr`, `tcp_abort`) with whatever was last registered by `tcp_arg`. Any path that leaves a PCB alive after its netconn is gone (cf. RE-SMS-026) turns the error notification itself into a NULL/dangling deref, masking the original failure with a crash.
- **Fix:** Add the same first-line guard the siblings have; if `conn == NULL`, return after no-op.
- **Benefit:** Error handling stops being a crash amplifier.
- **Effort:** Trivial. **Risk:** Low. **Confidence:** READ.

### RE-SMS-068 — ip_output_if() keeps its working IP-header pointer in a function-static variable shared by all callers
- **Category:** 3 (overengineering)
- **Locations:** `iop/SMSTCPIP/src/ip.c:331-332` (`static struct ip_hdr *iphdr; static u16_t ip_id = 0;`)
- **Problem:** `iphdr` gains nothing from being static — it is assigned from `p->payload` on entry and used only within the call — but it makes the function non-reentrant across the stack's two real execution contexts: the tcpip thread (all TCP/UDP output) and the driver-side input thread (`ps2ip_input` calls `ip_input` directly, whose ICMP echo reply re-enters `ip_output_if`). An interleave between the assignment and the `IPH_*_SET` sequence writes one packet's header fields through the other packet's pointer (race INFERRED from the two call contexts; the static itself is READ). `ip_id` being static is necessary; `iphdr` is not.
- **Fix:** Delete `static` from the `iphdr` declaration (keep it for `ip_id`).
- **Benefit:** Removes a cross-thread shared-scratch hazard for zero cost.
- **Effort:** Trivial. **Risk:** Low. **Confidence:** READ (race INFERRED).

### RE-SMS-069 — `SMS_Realloc` publishes the new size even when `realloc` fails; AVI index loaders write through the result unchecked
- **Category:** 6 (bugs/security)
- **Locations:** Root: `src/SMS_EE.c:45-53` (`*apSize = 17 * aMinSize / 16 + 32; return realloc(apData, *apSize);`). Call sites: `src/SMS_ContainerAVI.c:169-176` (`_Load_idx1`), `src/SMS_ContainerAVI.c:281-288` (`_LoadODMLIdx`). Companion oddity: `src/SMS_EE.c:55-89` (`SMS_ReallocWithAlign` shrinks a misaligned result back to the old size with a second `realloc` before copying — correct only because it re-publishes `*apSize = aMinSize` at :85 after the copy)
- **Problem:** On failure `realloc` returns NULL but `*apSize` already claims the larger size; a caller that keeps the returned NULL with the published size writes against a size the buffer does not have. The AVI loaders do exactly that: `lpStm->m_pIdx = lpIndices; lpIndex = &lpIndices[m_nIdx++]; lpIndex->m_fKey = …` — storing NULL over the old table pointer (leaking/losing it) and immediately writing through `&NULL[m_nIdx]`. A large idx1/ODML index on a fragmented 32 MB heap is exactly where `realloc` fails. MOV's index builder already demonstrates the safe pattern (local size variable, test, keep old pointer — `src/SMS_ContainerMOV.c:919-929`).
- **Fix:** In `SMS_Realloc`, realloc into a temporary and publish `*apSize` only on success; on failure return the original pointer with the original size (callers already treat a too-small buffer via their own bounds). At the AVI sites, copy MOV's pattern: grow into a local, return 0 from the loader if it is NULL, and only then publish pointer + count.
- **Benefit:** Removes a size/pointer desynchronisation at the root of every growable index buffer, and a crash-on-load for big-index AVIs when memory is tight (failure degrades to "no index" instead).
- **Effort:** Small. **Risk:** Low. **Confidence:** READ.

### RE-SMS-070 — SMSMAP TX gather bounded by the FIFO budget, not by the gather buffer (INFERRED)
- **Category:** 6 (bugs/security)
- **Locations:** `iop/SMSMAP/smap.c:886-921` (`SMap_Send`); buffer `iop/SMSMAP/smap.c:888`, sizes `iop/SMSMAP/smap.c:17-18` (`SMAP_TXMAXSIZE 1514`, tail pad 4); budget `iop/SMSMAP/main.c:165` / `smap.c:708` (`m_TXFree = 4096`)
- **Problem:** For a chained pbuf, `SMap_Send` gathers the whole chain into the static `sl_TXBuf` (1,518 bytes) with `mips_memcpy` per link (`smap.c:914-919`). The only length gate is `lTXLen > m_TXFree` (`smap.c:908`), where `m_TXFree` is the 4,096-byte TX-FIFO budget — so a chain with `tot_len` between 1,519 and 4,096 passes the gate and overruns the static buffer. Reachability depends on whether the SMSTCPIP stack can hand linkoutput a chain longer than the 1,500 MTU — normal traffic cannot, which is presumably why this has never fired; the bound is nonetheless the wrong constant.
- **Fix:** Gate the gather on `sizeof(sl_TXBuf)` (return `SMap_TX` above it), keeping the FIFO check for the FIFO. One constant change.
- **Benefit:** Makes the gather buffer's bound self-evidently correct instead of incidentally safe.
- **Effort:** Trivial. **Risk:** Low. **Confidence:** INFERRED (code READ; reachability depends on stack behaviour outside this finding's scope).

### RE-SMS-071 — SMSDev9 exported callback registration indexes fixed arrays unchecked (INFERRED)
- **Category:** 6 (bugs/security)
- **Locations:** `iop/SMSDev9/src/ps2dev9.c:170-186` (`dev9RegisterIntrCb` → `dev9_intr_cbs[7]`, `dev9RegisterPreDmaCb`/`PostDmaCb` → arrays of 2), `:301,309` (`dev9DmaTransfer` indexes the DMA callback arrays with its `ctrl` argument)
- **Problem:** These are library exports callable by any loaded IOP module. `intr`, `aDev`, and `ctrl` are used as raw array indices with no range check, so an out-of-range argument writes a function pointer outside the tables (registration) or calls through an out-of-bounds slot (DMA). All in-tree callers pass constants (SMAP registers 2–6 / channel 1), so this is a robustness gap in an exported interface rather than a live in-tree bug.
- **Fix:** Range-check the three registration functions and `ctrl` in `dev9DmaTransfer` (ignore/return on out-of-range) — four comparisons total.
- **Benefit:** Hardens the DEV9 export surface every network/HDD driver depends on.
- **Effort:** Trivial. **Risk:** Low. **Confidence:** INFERRED (code READ; no in-tree caller misuses it).

### RE-SMS-072 — do_connect() discards tcp_connect()/udp_connect() results; a synchronous TCP failure is never posted back, so netconn_connect() blocks
- **Category:** 6 (bugs/security)
- **Locations:** `iop/SMSTCPIP/src/api_msg.c:505-525` (TCP arm at :517-519: `tcp_connect(...)` return dropped, no `sys_mbox_post` in that arm); UDP arm :512-514 (`udp_connect(...)` return dropped, `conn->err` untouched); caller blocks at `iop/SMSTCPIP/src/api_lib.c` `netconn_connect()` (`sys_mbox_fetch(conn->mbox, &mem)`)
- **Problem:** For TCP, the only `conn->mbox` post on the success path is inside the `do_connected` callback, which `tcp_connect` invokes only on SYN-ACK. When `tcp_connect` fails synchronously (e.g. `tcp_enqueue` returns ERR_MEM under pool pressure), nobody posts the mbox and `conn->err` is never set from the return value: the calling thread waits until the SYN retransmission machinery aborts the PCB and `err_tcp` happens to post the mbox — or indefinitely if that path is not taken (duration INFERRED). For UDP, a failed `udp_connect` (bind failure) is reported to the application as success.
- **Fix:** Capture the return in both arms: `msg->conn->err = tcp_connect(...)`; if it is not ERR_OK, `sys_mbox_post(msg->conn->mbox, NULL)` immediately (do_connected still posts on the async success path). Same for `udp_connect`.
- **Benefit:** `connect()` fails fast with the real error instead of hanging the application thread.
- **Effort:** Small. **Risk:** Low — must not double-post on success; post only on synchronous error. **Confidence:** READ (hang-until-timeout duration INFERRED).

### RE-SMS-073 — Client netconns never get a semaphore, but netconn_write() waits on it
- **Category:** 6 (bugs/security)
- **Locations:** `iop/SMSTCPIP/src/api_lib.c:146` (`conn->sem = SYS_SEM_NULL`, never created in `netconn_new_with_proto_and_callback`); waits at `api_lib.c:521` and `api_lib.c:545` (`netconn_write`); contrast `api_msg.c:252` (accepted conns *do* get `sys_sem_new(0)`) and the guarded wait at `api_lib.c:574` (`netconn_close` checks `sem != SYS_SEM_NULL`); signallers `api_msg.c` `poll_tcp`/`sent_tcp` both skip `SYS_SEM_NULL` semaphores, so a client conn can never be woken by design
- **Problem:** The send-flow-control mechanism is half-wired: for every outbound (client) connection — the only kind a media client normally creates — `conn->sem` stays 0. A `netconn_write` larger than `TCP_SND_BUF` (5,840 bytes) reaches `WaitSema(conn->sem)` on semaphore 0, which no one will ever signal; at best it returns an IOP error immediately and the write busy-loops posting zero-progress writes, at worst it waits on an unrelated kernel semaphore (exact IOP behaviour INFERRED). `poll_tcp`/`sent_tcp` were written to signal this semaphore and silently never do for client conns.
- **Fix:** Create the semaphore in `netconn_new_with_proto_and_callback` exactly as `accept_function` does (`conn->sem = sys_sem_new(0)`, fail the constructor on SYS_SEM_NULL); `netconn_delete` already deletes it when non-NULL (`api_lib.c:203`).
- **Justification:** One `sys_sem_new` call restoring the mechanism the surrounding code already assumes; no new abstraction. The alternative (guarding the waits like `netconn_close`) leaves large writes spin-looping.
- **Benefit:** Large TCP sends (SMB writes, uploads) block-and-resume as designed instead of spinning/hanging.
- **Effort:** Small. **Risk:** Medium — semaphore lifecycle must match `netconn_delete` (already handled). **Confidence:** READ (runtime symptom INFERRED).

### RE-SMS-074 — err_to_errno() maps every non-zero lwIP error to EIO
- **Category:** 6 (bugs/security)
- **Locations:** `iop/SMSTCPIP/src/sockets.c:75-91` (table + macro); all users in `sockets.c` (`lwip_bind`, `lwip_connect`, `lwip_listen`, `lwip_send`, getsockopt/setsockopt paths)
- **Problem:** The guard is `(err) < (sizeof(err_to_errno_table)/sizeof(int))`. `sizeof` yields an unsigned type, so the negative `err_t` is converted to a huge unsigned value and the comparison is always false for every real error: ERR_MEM, ERR_USE, ERR_CONN, ERR_RST all surface to applications as `EIO` instead of ENOMEM/EADDRINUSE/ENOTCONN/ECONNRESET. Diagnosis of every socket failure is destroyed (e.g. an address-in-use bind is indistinguishable from an I/O error). Latent second defect: the table has 11 entries (0…−10) but `ERR_IF` is −11 (`include/lwip/err.h`), so a naively fixed guard would index one past the table for ERR_IF.
- **Fix:** Compare in signed space — `(-(err)) < 11` — and extend the table with an entry for ERR_IF (EIO is fine) so the bound and the error range agree.
- **Benefit:** Applications and logs get truthful errnos from every socket call again.
- **Effort:** Trivial. **Risk:** Low — behaviour change is only in reported errno values, toward the documented ones. **Confidence:** READ.

### RE-SMS-075 — tcpip_apimsg() silently frees a dropped API message; every netconn caller then blocks forever on a reply that will never come
- **Category:** 6 (bugs/security)
- **Locations:** `iop/SMSTCPIP/src/tcpip.c:110-122` (`else memp_free(MEMP_API_MSG, apimsg)` when the MEMP_TCPIP_MSG pool — 64 entries, lwipopts.h — is exhausted); every caller in `api_lib.c` follows `api_msg_post(msg)` with `sys_mbox_fetch(conn->mbox, &mem)` (e.g. bind/connect/write/close)
- **Problem:** This is the same discarded-failure shape as RE-SMS-072 but at the transport hop shared by *all* netconn operations: pool exhaustion converts any bind/connect/write/close/delete into a permanent block of the calling thread, with the request neither executed nor errored.
- **Fix:** Do not free-and-drop: retry the `memp_malloc` (the tcpip thread continuously drains the pool via `tcpip_thread`, tcpip.c:56-82, so a bounded spin/yield succeeds), i.e. loop until a slot is available instead of discarding. That is the smallest change that preserves the synchronous API contract.
- **Benefit:** Removes a whole class of hangs under burst load (e.g. many simultaneous connection attempts).
- **Effort:** Small. **Risk:** Medium — a spin here must yield to the tcpip thread; on IOP the mbox post/fetch path already involves semaphore waits, and allocation retry can piggyback on `sys_mbox_post` timing. **Confidence:** READ (that the drop deadlocks the caller is code-provable; pool-exhaustion frequency INFERRED).

### RE-SMS-076 — lwip_sendto() discards both re-connect results and dereferences `to` unchecked
- **Category:** 6 (bugs/security)
- **Locations:** `iop/SMSTCPIP/src/sockets.c:445-472` (`to` dereferenced at :458-459 with no NULL check; `netconn_connect(...)` at :461 and :468 and `netconn_disconnect(...)` at :470 all return-discarded)
- **Problem:** sendto is implemented as connect → send → restore/disconnect. If the first connect fails (ERR_MEM/ERR_USE from the api_msg layer), the send still runs against the *previous* peer (or an unconnected PCB) and its result is returned as if the datagram went to `to`. If the restore connect fails, the socket is silently left connected to the sendto target, changing the semantics of every later `send`/`recvfrom` on it. A NULL `to` crashes outright.
- **Fix:** NULL-check `to`; check the first connect and return −1 with its errno on failure; check the restore and surface its failure the same way (the datagram was sent, so at minimum do not lose the state corruption silently — disconnect on restore failure).
- **Benefit:** Datagrams stop going to the wrong peer with a success return code.
- **Effort:** Small. **Risk:** Low. **Confidence:** READ.

### RE-SMS-077 — lwip_getpeername()/lwip_getsockname() discard the peer/addr result; netconn_addr() itself has no NULL-PCB check
- **Category:** 6 (bugs/security)
- **Locations:** `iop/SMSTCPIP/src/sockets.c:827` (`netconn_peer(...)` result dropped, uninitialised `naddr` then copied to the caller and 0 returned); `sockets.c:861` (same for `netconn_addr`); `iop/SMSTCPIP/src/api_lib.c:236-260` (`netconn_addr` reads `conn->pcb.udp->local_ip` / `conn->pcb.tcp->local_ip` with no NULL check, unlike `netconn_peer` at :212-234 which checks)
- **Problem:** getsockname on a fresh (never bound/connected) socket crashes inside `netconn_addr`; getpeername on an unconnected socket returns success with stack garbage as the "peer address" instead of ENOTCONN.
- **Fix:** Add the same NULL checks to `netconn_addr` as `netconn_peer`; propagate both functions' return values in sockets.c (`if (netconn_peer(...) != ERR_OK) { sock_set_errno(sock, ENOTCONN); return -1; }`).
- **Benefit:** Two exported calls stop crashing / lying on the most common misuse (querying before connect).
- **Effort:** Trivial. **Risk:** Low. **Confidence:** READ.

### RE-SMS-078 — TCP initial sequence numbers are predictable: a counter starting at 6510 advanced only by the 500 ms tick count
- **Category:** 6 (bugs/security)
- **Locations:** `iop/SMSTCPIP/src/tcp.c:909-913` (`tcp_next_iss`: `static u32_t iss = 6510; iss += tcp_ticks;`); used for both directions at `tcp.c:506` (`tcp_connect`) and `tcp.c:822` (`tcp_alloc`, listen path); same pattern, lower stakes: `iop/SMSTCPIP/src/ip.c:332` (`static u16_t ip_id` incremented by 1 per packet)
- **Problem:** `tcp_ticks` increments in `tcp_slowtmr` (every 2×160 ms nominal). An attacker who can observe or guess one connection's ISN — or simply the stack's uptime — can predict the next ISN to within a few hundred, enabling blind sequence guessing against SMS's SMB/FTP-style sessions (exploitability INFERRED; predictability is READ from the code — there is no other entropy input anywhere in the function).
- **Fix:** Seed `iss` once in `tcp_init` from the IOP system clock low word (the port already uses `USec2SysClock`/alarm clocks in `ps2ip.c`/`ip_frag.c`) and advance by a large odd stride per connection plus ticks, instead of ticks alone. No new subsystem — one initialisation read of an existing clock.
- **Justification:** The change adds a single seed read; the alternative (leaving a constant-start counter) keeps blind-injection practical against a stack whose peers are on a LAN the user does not control.
- **Benefit:** Raises blind TCP injection from trivial to impractical for this stack's threat model.
- **Effort:** Small. **Risk:** Low. **Confidence:** READ (exploitability INFERRED).

### RE-SMS-079 — tcp_keepalive() never sets the TCP flags byte; probes go out with whatever flags the recycled heap buffer last held
- **Category:** 6 (bugs/security)
- **Locations:** `iop/SMSTCPIP/src/tcp_out.c:539-579` (header filled at :557-568 — src/dest/seqno/ackno/wnd/urgp/hdrlen/chksum, but no `TCPH_FLAGS_SET` anywhere in the function; every sibling does: `tcp_enqueue` :219, ACK in `tcp_output` :365, `tcp_rst` :492)
- **Problem:** The pbuf is `PBUF_RAM` from `mem_malloc`; `mem` RAM is zeroed only once in `mem_init` and recycled thereafter, so the flags byte is leftover data from a previous packet — a keepalive probe can legitimately leave with SYN, RST or FIN bits set, resetting or confusing the very idle connection it exists to preserve (SO_KEEPALIVE sockets, `tcp_slowtmr` keepalive path in `tcp.c`).
- **Fix:** One line with the siblings: `TCPH_FLAGS_SET(tcphdr, TCP_ACK);`
- **Benefit:** Keepalive probes stop being random-flag packets.
- **Effort:** Trivial. **Risk:** Low. **Confidence:** READ.

### RE-SMS-080 — udp_send() leaks the chained header pbuf on the no-route return
- **Category:** 6 (bugs/security)
- **Locations:** `iop/SMSTCPIP/src/udp.c` `udp_send()` — header-chain allocation at the `pbuf_header` fallback (`q = pbuf_alloc(PBUF_IP, UDP_HLEN, PBUF_RAM); pbuf_chain(q, p);`), then the no-route early return `return ERR_RTE;` at `udp.c:458` skips the `if (q != p) pbuf_free(q);` cleanup that the normal exit performs
- **Problem:** Every UDP send to an unroutable destination (no matching netif and no default) while the header had to be chained leaks one RAM pbuf + its heap block from the 12 KB heap. Repeated failed sends (e.g. streaming to a host on a vanished route) drain the heap until RE-SMS-043/044/048 fire.
- **Fix:** On the route-failure return, `if (q != p) pbuf_free(q);` before returning — or move the route lookup above the header chaining (LESS: one less alloc/free pair on the error path entirely).
- **Benefit:** A repeatable error stops being a slow heap leak.
- **Effort:** Trivial. **Risk:** Low. **Confidence:** READ.

### RE-SMS-081 — netconn_new_with_proto_and_callback()'s error path leaks the mbox it created and the PCB do_newconn created
- **Category:** 6 (bugs/security)
- **Locations:** `iop/SMSTCPIP/src/api_lib.c:138-164` (`error:` label frees only the netconn; reached from three places: mbox-creation failure — nothing to leak, msg-allocation failure at :152 — leaks `conn->mbox`, and `conn->err != ERR_OK` at :162 — leaks `conn->mbox` *and* the PCB that `do_newconn` allocated and registered)
- **Problem:** Under exactly the memory pressure that makes these paths likely, each failed `socket()` call permanently loses a sys mbox (IOP semaphore pair + AllocSysMemory block, `ps2ip.c` `sys_mbox_new`) and, in the third case, a PCB that stays registered with callbacks pointing at a freed netconn (cf. RE-SMS-067).
- **Fix:** In the error path, if `conn->pcb.tcp != NULL` first post a DELCONN (or set a flag so the caller does); always `sys_mbox_free(conn->mbox)` when it is not SYS_MBOX_NULL before `memp_free`.
- **Benefit:** Failed socket creation stops consuming finite IOP resources.
- **Effort:** Small. **Risk:** Low. **Confidence:** READ.

### RE-SMS-082 — Packet-duration and FLAC-duration math divide by file-controlled values that can be zero
- **Category:** 6 (bugs/security)
- **Locations:** `src/SMS_Container.c:239` (`SMSContainer_CalcPktFields`: `(pktSize * 8 * m_SampleRate) / m_BitRate` when `m_FrameSize <= 1`); `src/SMS_ContainerFLAC.c:77` (`apBlockSz[5] = lnSamples / m_SampleRate`)
- **Problem:** One mechanism in two places: a divisor taken from file data is used with no zero test. (1) `m_BitRate` is the WAV-header average-bytes-per-second × 8 (`src/SMS_Container.c:341`, `SMSContainer_GetWAVHeader`) or an esds value — a file that says 0 makes every audio packet whose codec reports `m_FrameSize <= 1` (ASF non-MP3 audio sets `m_FrameSize = 1` at `src/SMS_ContainerASF.c:290-292`; AVI audio never sets it, so it stays 0) divide by zero while computing duration. (2) A FLAC STREAMINFO block encodes the sample rate in 20 bits; 0 is encodable, `_do_probe` computes the duration division at :77 *before* the acceptance test that requires a non-zero rate (`src/SMS_ContainerFLAC.c:96`), so a zero-rate STREAMINFO divides by zero during probing itself. On the EE's MIPS core integer divide-by-zero does not trap — it yields an undefined result — so the observable failure is garbage durations / `m_CurDTS` drift rather than a clean crash (INFERRED impact; the missing guards are READ).
- **Fix:** In `SMSContainer_CalcPktFields`, skip the duration computation when `m_BitRate == 0` (the `lDen && lNum` guard two lines later already establishes the "leave duration unset" outcome for the video case); in `_do_probe`, compute the duration field only when the sample rate is non-zero.
- **Benefit:** Zero-bitrate WAV headers and zero-rate FLAC files stop corrupting every subsequent timestamp of the stream.
- **Effort:** S. **Risk:** Low. **Confidence:** High for the missing guards (READ); Med for impact severity (INFERRED, MIPS divide-by-zero semantics).

### RE-SMS-083 — A Vorbis file with ≥3 channels returns −1 from the OGG creator and aborts the entire probe chain
- **Category:** 6 (bugs/security)
- **Locations:** `src/SMS_ContainerOGG.c:70` (`retVal = lInfo.channels < 3 ? 1 : -1`), `src/SMS_ContainerOGG.c:96-128` (creator returns the probe value verbatim), `src/SMS_Container.c:199-251` (probe loop breaks on any non-zero; :246-251 destroys the container when the status is negative)
- **Problem:** Every other creator returns 0 for "not mine / cannot handle" and a positive value for success. OGG alone returns −1 for a *positively identified* Vorbis stream whose only fault is having 3+ channels. In `SMS_GetContainer` a negative status is treated as fatal: probing stops (later creators — M4A, FLAC, AAC, MP3, AC3, MPEG-PS, JPG, M3U, MOV — never run) and the container is destroyed. The same −1 is harmless in the M3U sub-probe loop, which tests truthiness and moves on (`src/SMS_ContainerM3U.c:485-491`), so the two callers of the same value already disagree about its meaning.
- **Fix:** Return 0 from `SMS_GetContainerOGG` for the too-many-channels case (keep the probe's internal −1 if wanted, translate at the creator boundary, where the chain contract lives).
- **Benefit:** One unsupported-channel-count file can no longer veto every other format's probe; behaviour matches the other eleven creators.
- **Effort:** S. **Risk:** Low. **Confidence:** READ.

### RE-SMS-084 — DTS: mono float-to-int converter is an empty stub — mono DTS "decodes" to stale buffer contents
- **Category:** 8 (half-finished)
- **Locations:** `src/SMS_DTS.c:11511-11513` (`_dts_float_to_int_1:` is `jr $ra / nop` — an empty function); selected via `s_FloatToInt[0]` at `src/SMS_DTS.c:11556-11565` whenever `anChannels == 1`; the AC3 equivalent `_ac3_float_to_int_1` in `src/SMS_AC3.c` is a full converter
- **Problem:** For a mono DTS stream the decode loop runs, `_dts_block` fills the sample buffer, and the converter that should write interleaved PCM into the output ring buffer does nothing — the player emits whatever bytes the ring buffer already held, at full frame size, indefinitely. This is a silently wrong-output bug for an entire stream class, not a graceful degradation.
- **Fix:** Implement the mono converter following the AC3 `_1` pattern (single-channel clamp/convert loop over 256-sample blocks).
- **Justification:** The alternative within "less" is rejecting mono DTS in `DTS_Decode`, which removes a supported stream class; a ~30-line converter cloned from the sibling decoder is the smallest change that makes the existing dispatch table honest.
- **Benefit:** Mono DTS files play actual audio instead of buffer garbage.
- **Effort:** Small. **Risk:** Low; the mono path is currently broken, so any faithful converter is an improvement. **Confidence:** READ.

### RE-SMS-085 — WMA: `wma_init` divides by the container-supplied sample rate without a zero check
- **Category:** 6 (bugs/security)
- **Locations:** `src/SMS_WMA.c:1352` and `:1383` (`pos = ((block_len * 2 * a) + …) / b` with `b = lpCtx->sample_rate`); `src/SMS_WMA.c:1400-1402` (`high_band_start` divides by `lpCtx->sample_rate`); the rate is stored from the codec context during `wma_init` (same function that stores `block_align` at :1247) with no `== 0` rejection
- **Problem:** A WMA/ASF header declaring sample rate 0 reaches three integer divisions by zero during codec init — a hard crash before a single frame is decoded. RE-SMS-060 covers the same init function's unchecked `block_align`; the two fields deserve one validation block.
- **Fix:** Reject `sample_rate == 0` at the top of `wma_init` (together with the `block_align` check of RE-SMS-060).
- **Benefit:** Removes a trivially triggered crash on malformed files.
- **Effort:** Trivial. **Risk:** None; a zero sample rate describes nothing playable. **Confidence:** INFERRED (divisions and the missing check are READ; that a container will pass 0 through is header-dependent).

### RE-SMS-086 — AC3/DTS: `Decode` writes uninitialized `lBitRate`/`lSampleRate` locals to the codec context when sync fails
- **Category:** 6 (bugs/security)
- **Locations:** `src/SMS_AC3.c:2735` (`apCtx->m_BitRate = lBitRate;` — placed after the if/else, so it also runs on the `AC3_SyncInfo(...) == 0` failure branch where `lBitRate` was never written); `src/SMS_DTS.c:11763` (identical structure in `DTS_Decode`)
- **Problem:** On every failed sync probe — i.e. while sliding across non-AC3/DTS bytes looking for a header — the context's bit-rate field is overwritten with uninitialized stack content. The value is cosmetic (display/UI), but the read of uninitialized memory is real, happens continuously during seeks over foreign data, and makes the reported bitrate nondeterministic garbage.
- **Fix:** Move the assignment into the sync-success branch in both files (two-line move, ×2).
- **Benefit:** Removes an uninitialized read and garbage state churn on the seek path.
- **Effort:** Trivial. **Risk:** None. **Confidence:** READ.

### RE-SMS-087 — AAC: multichannel frames are mixed through `pcm_synN` with phantom copies of channel 0 for 3–4 channel content
- **Category:** 6 (bugs/security)
- **Locations:** `src/SMS_AAC.c:7925-7958` (`output_to_PCM`: switch on `CONV(channels, downMatrix)`); driven from `_aac_decode`, which forces `output_channels = 2` and sets `downMatrix = 1` for any frame with > 2 channels, so the switch lands in the `default` case, calling `pcm_synN` with `lppChan[0..4]` taken from `internal_channel[0..4]`; `internal_channel` is zeroed at the top of every `_aac_decode` call, so for a 3- or 4-channel frame slots 3–4 still contain 0 — i.e. channel 0's buffer pointer
- **Problem:** If `pcm_synN` (extern, `src/SMS_AAC.c:42`; defined outside the decoder's files) mixes a fixed five channels down to stereo, then 3- and 4-channel AAC frames get channel 0 summed in two or three extra times — audibly wrong levels, though not memory-unsafe (the pointer is a valid allocated buffer). 5.1 content is unaffected; mono/stereo take the dedicated cases.
- **Fix:** Pass the frame's real channel count into the mix (or zero-fill unused `lppChan` slots instead of leaving them aliased to channel 0 — silence mixes safer than a duplicated channel).
- **Benefit:** Correct downmix levels for 3.0/4.0 AAC content.
- **Effort:** Small. **Risk:** Low. **Confidence:** INFERRED (the dispatch and the zeroed `internal_channel` array are READ; `pcm_synN`'s fixed-width assumption could not be verified from the decoder's files).

### RE-SMS-088 — DivX packed-B start-code scan never advances through the buffer
- **Category:** 6 (bugs/security — functional)
- **Locations:** `src/SMS_MPEG4.c:3362-3379`
- **Problem:** In `MPEG4_Decode`'s DivX packed-frame handling, the scan is `for (i = lCurPos; i < lBufSize - 3; ++i) if (SMS_unaligned32(lpBuf) == 0xB6010000) …`. The pointer is never offset by `i` (nor by `lCurPos`), so the loop tests the same four bytes at the packet start on every iteration. Unless the VOP start code happens to sit at offset 0, `lStartCodeFound` stays 0 and the packed second frame is never stashed into `m_pBSBuf` — packed-B DivX streams lose/desync frames.
- **Fix:** Test `SMS_unaligned32(lpBuf + i)`. One-token change.
- **Benefit:** Restores the intended packed-B handling the surrounding code (stash, `m_BSBufSize`, next-call decode at `src/SMS_MPEG4.c:3283`) was built for.
- **Effort:** Trivial. **Risk:** Negligible. **Confidence:** READ.

### RE-SMS-089 — DXSB frames are emitted with `m_EndPTS` never initialized
- **Category:** 6 (bugs/security)
- **Locations:** `src/SMS_DXSB.c:202-204` (allocation), `src/SMS_DXSB.c:215-232` (fields actually set), `src/SMS_DXSB.c:317-318` (use)
- **Problem:** `_decode` allocates the frame with `memalign` (uninitialized memory) and sets `m_StartPTS`, dimensions, pixmap and crop fields — but never `m_EndPTS`. `SMS_DXSB_Decode` then unconditionally computes `lpFrame->m_EndPTS = SMS_Rescale(lpFrame->m_EndPTS, …)` from heap garbage and posts the frame, so subtitle display timing consumes an arbitrary value.
- **Fix:** Set `retVal->m_EndPTS = retVal->m_StartPTS` in `_decode` next to the other field initializations (or switch the allocation to zeroing). One line.
- **Benefit:** Deterministic subtitle end times; removes an uninitialized-memory read from the output path.
- **Effort:** Trivial. **Risk:** Negligible. **Confidence:** READ.

### RE-SMS-090 — DSP MDCT init: unchecked table allocation and a dead error test
- **Category:** 6 (bugs/security)
- **Locations:** `src/SMS_DSP.c:933-972` (`DSP_MDCTInit`: `s->m_pCos = memalign(...)` at `:945` unchecked, then `s->m_pSin`/`s->m_pTmp` derived by pointer arithmetic and written in the loop at `:965-968`; error test at `:972`: `if (!DSP_iFFTInit(&s->m_FFT, s->m_nBits - 2) < 0)`). Contrast `DSP_iFFTInit` itself at `:879-895`, which does check its `memalign`
- **Problem:** Two defects in one init: (1) a failed `memalign` writes the cosine table through NULL-derived pointers; (2) `!x < 0` is always false (`!x` is 0 or 1, neither is `< 0`), so the FFT-init failure branch — destroy and return 0 — can never execute; a failed FFT init is reported as success with a half-initialised context.
- **Fix:** Check `m_pCos` like `DSP_iFFTInit` checks its table (return 0), and write the test as the intended `if (!DSP_iFFTInit(...))` (its return is 1/0 per `:931`). No other change.
- **Benefit:** Makes audio-codec init fail cleanly instead of corrupting memory or lying about success.
- **Effort:** Small. **Risk:** Low. **Confidence:** READ.

### RE-SMS-091 — `MC_SeekS` can return an uninitialised value
- **Category:** 6 (bugs/security)
- **Locations:** `src/SMS_MC.c:307-316` (`int retVal; MC_Seek(...); MC_Sync(&retVal); return retVal;`); `MC_Sync` at `:75-92` writes `*apRes` only when `s_LastCmd` is set. Non-BDM only (`#ifndef BDM` file). Sibling wrappers initialise theirs: `MC_ReadS` at `:265-274` starts from `0`
- **Problem:** If `MC_Seek` could not queue the RPC (a command already in flight — it returns 0 without setting `s_LastCmd`), `MC_Sync` leaves `retVal` untouched and the caller (config/palette/font seeks on the non-BDM path) acts on stack garbage as a file position/result.
- **Fix:** Initialise `retVal = 0;` exactly as `MC_ReadS` does. One line.
- **Benefit:** Removes an uninitialised return on a path used by every non-BDM asset load.
- **Effort:** Trivial. **Risk:** None. **Confidence:** READ.

### RE-SMS-092 — `SMS_SubContID` can never return FLAC: it compares the wrong suffix
- **Category:** 6 (bugs/security)
- **Locations:** `src/SMS_FileDir.c:88-118` (`SMS_SubContID`: `lpExt = apName + lLen - 4`, then at `:112-114` `if (!stricmp(lpExt, s_pFLAC))` where `s_pFLAC` is `".flac"` — 5 chars compared against a 4-char suffix pointer). The sibling functions get it right: `SMS_ContID` at `:160-176` and `SMS_FileID` at `:214-231` both do `--lpExt` before the 5-char comparisons
- **Problem:** `lpExt` points at `"flac"` (no dot); `stricmp("flac", ".flac")` never matches, so the FLAC sub-container branch is dead and `.flac` files fall through to generic probing instead of the direct sub-container ID the other two classifiers grant them.
- **Fix:** Add the missing `--lpExt` in the `lLen > 5` branch, mirroring the two sibling functions verbatim. One character of mechanism, no consolidation needed (the three classifiers differ deliberately in their tables).
- **Benefit:** Restores an intended, documented classification that is currently unreachable.
- **Effort:** Trivial. **Risk:** Low. **Confidence:** READ.

### RE-SMS-093 — `FindPath` computes a subdirectory's sector count from the root directory's size (SMSCDVD)
- **Category:** 6 (bugs/security)
- **Locations:** `iop/SMSCDVD/SMSCDVD.c:794`
- **Problem:** On descending into a subdirectory, `CachedDirInfo.m_SectorNum = (localTocEntry.m_FileSize >> 11) + ((CDVolDesc.m_RootToc.tocSize & 2047) != 0);` — the round-up term tests the **root** directory's size remainder instead of the subdirectory's own (`localTocEntry.m_FileSize & 2047`). Copy-paste error: subdirectories whose size is an exact 2048 multiple get a phantom extra sector when the root's isn't (cache reads one sector too far), and vice versa lose their final partial sector (last entries invisible).
- **Fix:** Use `localTocEntry.m_FileSize & 2047` in the round-up term.
- **Benefit:** Correct directory caching for all non-root directories; one-word fix.
- **Effort:** Trivial. **Risk:** Low. **Confidence:** READ.

### RE-SMS-094 — UDPFS SMAP PHY-reset retry bound fires on the first poll (INFERRED)
- **Category:** 6 (bugs/security)
- **Locations:** `iop/SMSUdpfs/smap/src/smap.c:137-147` (`InitPHY`)
- **Problem:** The reset-wait loop is `for (i = 0; read & RST; i++) { if (i <= 0) return -1; ... DelayThread(1000); }` — the guard `i <= 0` is true on the very first iteration, so the loop can only succeed if the reset bit has *already* cleared before the first read; it can never actually wait. Either the bit reliably clears that fast (making the check dead weight) or PHY reset spuriously fails on slower hardware. A retry cap was clearly intended (`i` is counted and a delay exists) but written as a bound that cannot function.
- **Fix:** Change the guard to a real cap (e.g. `i > 1000` with the existing 1 ms delay ≈ 1 s), matching the loop's evident intent.
- **Benefit:** PHY init either works as intended on slow-to-reset hardware or the dead check is honest.
- **Effort:** Trivial. **Risk:** Low. **Confidence:** INFERRED (code READ; hardware timing not verifiable in this environment).

### RE-SMS-095 — `g_ASCII` is declared 4,572 in its consumer and defined 5,416 in its data file
- **Category:** 6 (bugs/security)
- **Locations:** `src/SMS_GSFont.c:11` (definition, 5,416 — script-counted exact); `src/SMS_GS_2.c:35` (extern, 4,572); `src/SMS_GS_2.c:50-53` (`s_FontSizes`); `src/SMS_GS_2.c:502-506` (`GSFont_Get`); `src/SMS_Config.c:292` (`_load_font`)
- **Problem:** An extern array declaration with a different bound than the definition is undefined behaviour in C, and here it is load-bearing: `s_FontSizes[0]` is initialised with `sizeof(g_ASCII)` *in SMS_GS_2.c's translation unit*, so `GSFont_Get(0, ...)` reports 4,572 for a 5,416-byte font (the other four fonts' externs match their definitions exactly — script-verified). The one consumer, `_load_font`, uses that number as the embedded buffer's capacity when deciding whether a replacement `ascii.mtf` fits in place: fonts of 4,573–5,416 bytes needlessly take the `malloc` path (and, on malloc failure, are silently not loaded). The error is in the safe direction — it understates, so nothing overflows — which is presumably why it survived.
- **Fix:** Declare the font arrays with unspecified bound (`extern unsigned char g_ASCII[];` etc.) in SMS_GS_2.c and export the sizes from SMS_GSFont.c's TU (or move `s_FontSizes` there). Removes the duplicate source of truth for all five fonts, not just the wrong one.
- **Benefit:** `GSFont_Get` tells the truth for the ASCII font; the class of bug (hand-copied sizes drifting from generated data) is removed for all five fonts.
- **Effort:** Small. **Risk:** Low. **Confidence:** READ (counts script-verified).

### RE-SMS-096 — `UnicodeToCP`'s binary search can never reach table index 0 (proven by simulation)
- **Category:** 6 (bugs/security)
- **Locations:** `src/SMS_InverseCodePages.c:114-136` (`UnicodeToCP`); tables at `src/SMS_InverseCodePages.c:22-110`
- **Problem:** The search starts `index = 64, step = 64` and halves `step` *before* moving (`step >>= 1` inside the loop before the index update). An exact python port of the algorithm, run against all four script-extracted tables, finds **exactly one unreachable entry per table — index 0**: in the 1250/1252/1253 tables U+0081 should yield byte 129 and in the 1251 table U+0098 should yield byte 152; all four return `'?'` (the port returns 63 in each failing case). Every other entry (127/128 per table) is found, both arrays are strictly monotonic, and each index array is a permutation of 0..127 — the tables are perfect; only the walk is wrong (repeatedly stepping left from 64 lands on 1 with `step` already 0, so index 0 is never probed; index 127 *is* reachable, so the walk is asymmetric).
- **Fix:** Replace the hand-rolled walk with a standard half-open binary search over `[0, 128)`. No table data changes — per the table rule, no replacement values are proposed or needed.
- **Benefit:** Small in practice (U+0081/U+0098 are C1 control codes, rare in filenames/subtitles) but the fix makes the function's contract — every table entry findable — actually true, verified by re-running the simulation.
- **Effort:** Trivial. **Risk:** Low — simulation gives a complete before/after test oracle for all 512 entries. **Confidence:** READ (algorithm ported line-for-line and executed against the extracted tables; failure set is exactly `{index 0}` × 4 tables).

### RE-SMS-097 — Icon loader hand-declares `lzma2_uncompress` with the wrong type and ignores its result
- **Category:** 2 (duplication)
- **Locations:** `src/SMS_GUIcons.c:34` (declaration), `src/SMS_GUIcons.c:1405-1406` (call); real prototype `include/lzma2.h:33-34`
- **Problem:** Every other consumer includes `<lzma2.h>` (SMS_EE.c:28, SMS_GS_2.c:24, SMS_IOP.c:44). `SMS_GUIcons.c` instead re-declares the function by hand as `extern int lzma2_uncompress(unsigned char*, unsigned int, unsigned char*, unsigned int)` — the real function returns `enum xz_ret` and takes `size_t` parameters. It happens to be ABI-compatible on this target (enum ≈ int, `size_t` = `unsigned int` on o32), so it links and runs, but it is a second, divergent copy of the truth — and the call discards the result entirely, so a failed decode of `g_RGBA_Blob` would silently slice and display garbage icons instead of failing. (The blob itself is verified good — see Checked-clean — so this is latent.)
- **Fix:** Delete the hand declaration, include `<lzma2.h>`, and check the return against `XZ_STREAM_END` (on failure, leave `s_pIconData` unset). Less code, one prototype.
- **Benefit:** Removes a type-punning declaration that breaks silently if the wrapper's signature ever changes, and gives the icon path the same failure behaviour as the other three consumers.
- **Effort:** Trivial. **Risk:** Low. **Confidence:** READ.

### RE-SMS-098 — Give `libds34bt.h` / `libds34usb.h` include guards and `(void)` prototypes
- **Category:** 6 (bugs/security — header correctness)
- **Locations:** `include/libds34bt.h` (35 lines), `include/libds34usb.h` (21 lines) — the only 2 of 89 headers with no include guard and no `#pragma once`; all prototypes use K&R empty parentheses, e.g. `int ds34bt_init();`, `int ds34usb_reset();`, where the other 87 headers uniformly use `(void)`
- **Problem:** Both headers are included by multiple translation units (`src/SMS_PAD.c:26-27`, `src/SMS_GUIMenuSMS.c:36-37`). Today double inclusion in one TU happens not to bite only because the contents are declarations and an enum; the enum definitions (`eDS34BTStatus`, `eDS34USBStatus`) will hard-error on any future double include. In C, `int f();` declares a function with unspecified parameters, so callers get no argument checking — inconsistent with the tree's own convention and a real footgun for the RPC wrappers that take pointers.
- **Fix:** Add the tree-standard `#ifndef __libds34bt_H`-style guards and change `()` to `(void)` on the zero-argument prototypes in both headers. (Do this for the surviving prototypes if RE-SMS-108 lands first.)
- **Justification:** Adds only the guard idiom every other header in the same directory already has — consolidation onto the existing mechanism, not a new one.
- **Benefit:** Removes the only unguarded headers in `include/`; restores compile-time argument checking for the DS34 API.
- **Effort:** Trivial. **Risk:** Very low. **Confidence:** READ.

### RE-SMS-099 — Shared bit reader (`SMS_Bitio.h`) has no end-of-buffer check in any reader — the root mechanism under most stream-side over-reads
- **Category:** 6 (bugs/security)
- **Locations:** `include/SMS_Bitio.h:70` (`SMS_GetBits`) and its siblings `SMS_GetSBits`, `SMS_GetBit`, `SMS_ShowBits`, `SMS_SkipBits` (whole header read, 148 lines): every function advances/reads via `m_Idx` and the cache without ever comparing `m_Idx` against `m_szInBits`
- **Problem:** All seven EE audio decoders parse attacker-controlled lengths, counts, and Huffman walks through this reader. Whenever a header or payload overstates its length (several findings above are instances), parsing simply continues past the end of the packet buffer, reading heap neighbours as if they were stream data. This is why the per-sink guards in RE-SMS-010/022/023 are necessary at all.
- **Fix:** No single cheap fix is recommended here: clamping inside the reader changes hot-path semantics for every codec in the player (and the AAC code uses its own cache macros layered on the same context). The actionable conclusion is the pattern used by the other findings — validate lengths/counts at the sinks that know the real bounds — and to treat any *new* unbounded field found in these decoders as a bug on sight. Recorded so the pattern is named once instead of rediscovered per decoder.
- **Benefit:** Names the shared root cause; directs future fixes to the right layer.
- **Effort:** None (documentation-level finding; sink fixes are the other findings). **Risk:** None. **Confidence:** READ (header read in full).

### RE-SMS-100 — FLAC: decoder trusts the channel count beyond its 2-entry synth table and a shift-divide (currently saved only by the container's gate)
- **Category:** 6 (bugs/security)
- **Locations:** `src/SMS_FLAC.c:437-444` (`_synth_indep[2]` function-pointer table indexed by `m_nChannels - 1` at :441); `src/SMS_FLAC.c:523-524` (`lBlockLen = 4096 >> nChannels; lnParts = m_BlockSize / lBlockLen;` — `lBlockLen` becomes 0 at `nChannels ≥ 13`, making the division a divide-by-zero)
- **Problem:** Inside the decoder, nothing bounds `m_nChannels`. The shipped FLAC container happens to refuse files with ≥ 3 channels (`src/SMS_ContainerFLAC.c:114`) and the STREAMINFO field itself is 3 bits (max 8 → synth index up to 7, still out of bounds), so both sinks are one container change — or one alternate caller — away from a wild function-pointer call. The decoder should not depend on that.
- **Fix:** Validate `1 ≤ m_nChannels ≤ 2` once in the FLAC decoder's init (the decoder only implements mono/stereo synthesis anyway — see the 2-entry table and the left/right/mid synth variants at :478).
- **Benefit:** Makes the FLAC decoder self-guarding; removes a latent wild-pointer call.
- **Effort:** Trivial. **Risk:** None for any file the current container accepts. **Confidence:** INFERRED (in-decoder mechanism READ; reachability currently blocked by the container gate, which is also READ).

### RE-SMS-101 — `SMS_GetContainer`'s explicit-format path indexes the creator table with an unchecked caller value
- **Category:** 6 (bugs/security)
- **Locations:** `src/SMS_Container.c:198` (`} else s_CCreator[ aPrefCont ] ( retVal );`)
- **Problem:** When `aPrefCont >= 0`, the value is used as a direct index into the function-pointer table with no range test. No current caller passes a bad value — the only in-tree explicit callers are M3U's two loops, which pass the seven initialised `m_ContID` constants, and the player passes `0x80000000`, which is negative as an `int` and therefore takes the auto-probe path (all verified by grep over `src/`) — so this is a latent trap for the next caller, not a live bug. It is listed last among the Cat-6 items for exactly that reason.
- **Fix:** Treat an out-of-range `aPrefCont` like the auto-probe case (or return NULL): one range test beside the existing one at :152-154.
- **Benefit:** The public entry point's contract stops depending on every future caller remembering an undocumented precondition.
- **Effort:** S. **Risk:** Low. **Confidence:** READ (call sites enumerated by grep; no live trigger found).

### RE-SMS-102 — The whole legacy SMSSMB tree is dead and internally duplicated — delete it
- **Category:** 1 (dead code; also 2 duplication)
- **Locations:** `iop/SMSSMB/` (3,894 lines); `iop/SMSSMB/src/SMSMB.c` (2,002) vs `iop/SMSSMB/src/ps2smb.c` (1,857); `iop/SMSSMB/Makefile` (builds only `SMSMB.o` → `bin/smb.irx`); packed-image slot definition `include/SMS_Data.h` (`SMS_SMB_OFFSET`/`SMS_SMB_SIZE`)
- **Problem:** Three layers of waste. (1) `ps2smb.c` is a near-copy of `SMSMB.c` in the same directory (600 diff lines: packed attributes, `mips_memcpy` vs `memcpy`, flag values, one DES/name-conversion delta) and is not referenced by the module's own Makefile — unbuilt duplicate. (2) The built product, `smb.irx`, is not what SMS ships: the EE's SMB path loads smbman-sms's `smbman_irx` (`src/SMS_IOP.c:586-597`, comment states smbman binds to the SMSTCPIP stack instead), `tools/build-smbman.sh` builds iop/smbman-sms, and no EE code references the packed `SMS_SMB_OFFSET` slot anymore. (3) The tree carries its own full set of unpatched SMB1 parsing bugs, so its continued presence is a maintenance and cherry-pick hazard: fixes land in smbman-sms and this copy silently diverges. The unpatched set, recorded here so deletion is understood as the fix for it: FindFirst/FindNext/ShareEnum cast `&lPkt[SVAL(lPkt,45)+4]` / `&lPkt[SVAL(lPkt,51)+8]` to parameter/data structs with no check of server-supplied offsets against the received length (`SMSMB.c:1127,1131,1193,1194,1305,1310`); `SMB_Negotiate` uses the server's `m_EncrKeyLen` (u16) to form a `strncpy` source into `m_PDom[80]` (`:813-815,834-837`); packet assembly uses unbounded `strcpy`/`strcat` into fixed packet buffers from EE-supplied strings (`:964-1000,1041-1046,1117,1232,1400-1404`); `_DrvDOpen` does `strcpy(s_SEnumBuff, apName)` into the 8,192-byte static (`:311`) and reads `s_SEnumBuff[lLen-1]`, which underflows to index −1 for an empty name; and the LOGOUT handler's inner loop at `:1774` indexes `m_FileCtx[i]` with the **mount** index `i` instead of the loop variable `j`, closing the same file context repeatedly and leaking the rest.
- **Fix:** Delete `iop/SMSSMB/` outright (and, if the packed IOP image is ever regenerated, drop the SMB slot from `include/SMS_Data.h`). Patching the list above inside a module nothing loads is the MORE path and is explicitly not proposed.
- **Benefit:** −3,894 lines, one whole SMB client removed from the audit/maintenance surface, no duplicate left to diverge.
- **Effort:** Small (deletion + confirming no build script references it; `tools/` and the top Makefile do not). **Risk:** Low — hinges on the liveness evidence above; if a non-BDM legacy build variant is ever resurrected it can be recovered from git history. **Confidence:** READ.

### RE-SMS-103 — Delete the dead generated header `include/s_iop_image.h` (3,820 lines, ~373 KB)
- **Category:** 1 (dead code; also 7 clutter)
- **Locations:** `include/s_iop_image.h` (whole file); only references in the tree are commented out: `src/SMS_IOP.c:228` (`//#include "s_iop_image.h"`) and `src/SMS_IOP.c:437` (`//while(!SifIopReset(s_iop_image, 0)){};`)
- **Problem:** A 3,820-line hex-array dump of a legacy IOP image (`static unsigned char s_iop_image[]`) is carried in `include/` — 30% of all header lines — but no translation unit includes it; the two historical uses survive only as comments. It inflates every clone, grep, and header audit, and risks being mistaken for the live IOP-load path (the live path is the committed `irx/*.irx.xz` blobs via the root Makefile's bin2c rule).
- **Fix:** Delete `include/s_iop_image.h` and the two commented-out lines in `src/SMS_IOP.c`. No replacement.
- **Benefit:** −3,820 lines / −373 KB; removes the single largest misleading artefact in `include/`.
- **Effort:** Trivial (one `git rm`, two comment lines). **Risk:** Very low — no live includer exists (verified by grep over the whole tree). **Confidence:** READ.

### RE-SMS-104 — Move non-site binaries out of the GitHub Pages tree (`docs/downloads/`, `docs/investigations/` ≈ 11.9 MB)
- **Category:** 7 (build/CI/clutter; also 4 size)
- **Locations:** `docs/downloads/` (9.9 MB: `SMS-manual.pdf` 9,321,329 B, `Rad-HOST-Client-1.8a.zip` 891,537 B, `SMS_lng09.rar`, `pic2smi.rar`, `smspal.rar`); `docs/investigations/` (2.0 MB, incl. `2026-11-11-handoff/samples/` — 19 test-media files: `.mp3/.wma/.wmv/.asf/.aac/.bin`, 1.7 MB for the handoff dir alone)
- **Problem:** `docs/README.md` states GitHub Pages publishes `master:/docs`, and `docs-site/build.py` generates only the top-level HTML/assets/search data — `downloads/` and `investigations/` are not generated site content, yet they sit inside the published tree and in every clone. The investigations handoff is dated working material (2026-11-11) with binary crash/sample media; the downloads are legacy third-party archives that the release system (rolling release assets, already used for the test-media pack per `.github/workflows/test-media.yml`) is designed to carry.
- **Fix:** Move `docs/downloads/*` to release assets (or a single `downloads` release) and update the links that point at them; move `docs/investigations/` out of `docs/` (e.g. repo-root `investigations/` if it must stay tracked, or an archive branch/release). Keep the generated site itself in `docs/` — Pages needs it.
- **Benefit:** ≈11.9 MB out of the Pages tree and out of the docs surface every doc link-checker and clone traverses; published site contains only site content.
- **Effort:** Medium (link updates in `docs-site/content/` + regeneration via `docs-site/build.py`, which CI validates with `--check`). **Risk:** Medium — external deep links to the old `…/docs/downloads/…` URLs will break; mitigate by keeping the filenames identical on the release that replaces them. **Confidence:** READ.

### RE-SMS-105 — AC3: `s_Samples` workspace is 192 KB of BSS; the highest offset ever used is ~3,072 floats (~12 KB)
- **Category:** 4 (size/memory)
- **Locations:** `src/SMS_AC3.c:323` (`static sample_t s_Samples[16 * 256 * 12]` = 49,152 floats = 192 KB, `.bss`); usage at `src/SMS_AC3.c:2305-2307` (base pointer, +256 LFE shift), `:2401-2480` (maximum expression `lpSamples + 1536 + 256 * i` with `i < 5`, plus the 256-wide IMDCT write at that address → highest touched index ≈ 3,072); `:2612` (handed to the float-to-int converter). A grep of the whole file confirms no other references
- **Problem:** The buffer is declared for 16 blocks × 12 channels worth of samples, but the decode writes at most 6 channel rows of 256 plus the 1,536-float IMDCT delay region — roughly 1/16 of the allocation. On the PS2's 32 MB RAM this is ~180 KB of permanently reserved, never-touched memory.
- **Fix:** Shrink the declaration to `16 * 256` floats (4,096 — still 33% headroom over the proven maximum offset of ~3,072) = 16 KB.
- **Benefit:** ~176 KB of BSS returned, zero behavioural change.
- **Effort:** Trivial (one constant). **Risk:** Low; the bound is established by exhaustive grep of the file's uses, not by sampling. **Confidence:** READ.

### RE-SMS-106 — Default (BDM) build carries 23,488 bytes of `g_DataBuffer` segments nothing can reach
- **Category:** 4 (size/memory)
- **Locations:** `src/SMS_Data.c` segments UMS.IRX (8,504 + 8 pad), USBD.IRX (9,140 + 12 pad), SIO2MAN.IRX (5,817 + 7 pad) = 23,488 aligned bytes; consumers `src/SMS_IOP.c:504,674,688`; build switch `Makefile:3` (`BDM ?= 1`), `Makefile:49-57` (`IOP_OBJS`)
- **Problem:** The only references to `SMS_USB_MASS_OFFSET`, `SMS_USBD_OFFSET` and `SMS_SIO2MAN_OFFSET` outside `include/SMS_Data.h` are in the `#else` (non-BDM) branches of `SMS_IOP.c` (lines 504, 674, 688). The default build is `BDM=1`, where those modules load from the `bin2c`'d `irx/*.irx.xz` objects instead (SMS_IOP.c:493,642,646). Because the three segments live inside the single `g_DataBuffer` array, the linker cannot strip them: every default ELF — and the EE RAM it occupies — carries 23,488 dead bytes (~8.8% of the 266,560-byte buffer).
- **Fix:** Move the three legacy segments (with their header size/offset entries) into a separate `SMS_DataLegacy.c` compiled only for `BDM=0`, keeping the non-BDM build bit-identical in layout. Deletion of the flavour is *not* proposed — it is a supported `?=` switch.
- **Benefit:** 23,488 bytes of `.data` and resident RAM back in the default build, on a 32 MB console.
- **Effort:** Medium (segment move + offset chain must stay inside the legacy TU; the remaining segments' offsets are computed from the chain in SMS_Data.h, so the split needs the legacy block kept contiguous at the same relative offsets or its own header chain). **Risk:** Medium — offset arithmetic is header-chained; a careless split shifts every following segment. Mitigated by the per-segment script census (all 17 segments payload == declared), which re-verifies payload/padding equality after any move. **Confidence:** READ.

### RE-SMS-107 — Remove ten defined-but-never-called EE functions (≈200 lines, incl. a 105-line IPU routine)
- **Category:** 1 (dead code)
- **Locations** (declaration / definition; word-boundary census over `src/ include/ tools/` found only these plus the definition's own end-comment):
  - `include/SMS_IPU.h:140` / `src/SMS_IPU_1.c:220-324` — `IPU_UnpackImage` (105 lines)
  - `include/SMS_CDDA.h:83` / `src/SMS_CDDA.c:220` — `CDDA_Pause`
  - `include/SMS_CDDA.h:85` / `src/SMS_CDDA.c:251` — `CDDA_SetMediaMode`
  - `include/SMS_FileContext.h:135` / `src/SMS_FileContext.c:887` — `CDDA_GetPicture`
  - `include/SMS_FileContext.h:136` / `src/SMS_FileContext.c:907` — `CDDA_GetDiskPicture`
  - `include/SMS_CDVD.h:63` / `src/SMS_CDVD.c:114` — `CDVD_QueryDVDV`
  - `include/SMS_PlayerControl.h:39` / `src/SMS_PlayerControl.c:876` — `PlayerControl_SwitchSubs`
  - `include/SMS_RC.h:83` / `src/SMS_RC.c:454` — `RC_Shutdown`
  - `include/SMS_RC.h:84` / `src/SMS_RC.c:484` — `RCX_Shutdown`
  - `include/SMS_PAD.h:20` / `src/SMS_PAD.c:118` — `ReadCombinedPadStatus_raw`
- **Problem:** Each function is compiled and linked into the EE ELF but has no caller anywhere — not in EE code, not via function pointer tables found by name, and not in the host-test harnesses in `tools/`. `IPU_UnpackImage` alone is a full 105-line DMA/IPU routine. They mislead readers about which CDDA/IPU/remote-control paths are live.
- **Fix:** Delete each definition and its header declaration. No replacement.
- **Benefit:** ≈200 fewer lines of EE code; smaller ELF; the live CDDA/IPU/RC surfaces become exactly the called ones.
- **Effort:** Small (ten independent deletions; host tests + PS2 build confirm). **Risk:** Low — census covered name references in C, asm, and Python harnesses; the one residual risk is a caller constructed by token pasting, which this codebase does not use for these names. **Confidence:** READ.

### RE-SMS-108 — Delete the thirteen never-called DS34 RPC client stubs
- **Category:** 1 (dead code)
- **Locations:** declarations `include/libds34bt.h`, `include/libds34usb.h`; definitions `src/libds34bt.c`, `src/libds34usb.c`. Never called (word-boundary census over `src/ include/ tools/`; the live calls are only `*_init`, `*_get_status`, `*_get_data`, `*_get_bdaddr`, `ds34usb_set_bdaddr` from `src/SMS_PAD.c` and `src/SMS_GUIMenuSMS.c`):
  - libds34bt (8): `ds34bt_deinit`, `ds34bt_reinit_ports`, `ds34bt_init_charging`, `ds34bt_set_rumble`, `ds34bt_set_led`, `ds34bt_reset`, `ds34bt_get_version`, `ds34bt_get_features`
  - libds34usb (5): `ds34usb_deinit`, `ds34usb_reinit_ports`, `ds34usb_set_rumble`, `ds34usb_set_led`, `ds34usb_reset`
- **Problem:** Half of the vendored DS3/DS4 client API is dead weight in the ELF: each stub is an SIF RPC wrapper that is compiled, linked, and maintained as if SMS could deinit/reinit/rumble/reset the pad drivers, which no code path does.
- **Fix:** Delete the thirteen stubs and their declarations, keeping the five used entry points. (If upstream-parity with the libds34 source is ever wanted again, the functions are recoverable from git history.)
- **Benefit:** ≈150 lines removed across the two client libs; the DS34 surface becomes exactly what SMS uses.
- **Effort:** Small. **Risk:** Low — no caller exists; risk is only to hypothetical future rumble/LED features, which would re-add one stub on demand. **Confidence:** READ.

### RE-SMS-109 — Delete the unreferenced `utils/` directory (prebuilt Win32 exe + orphan Loader)
- **Category:** 1 (dead code; also 7 clutter)
- **Locations:** `utils/DDrawSrv.exe`, `utils/Loader/Makefile`, `utils/Loader/linkfile`, `utils/Loader/loader.c` (4 files, 56 KB)
- **Problem:** Zero references to `DDrawSrv`, `utils/Loader`, or the Loader anywhere else in the repo (grep over docs, docs-site content, README, Makefile, tools, workflows returned nothing). `DDrawSrv.exe` is a committed prebuilt Windows binary with no documented purpose, source, or build path in the current tree — exactly the kind of artefact that cannot be audited or rebuilt.
- **Fix:** Delete `utils/`. If the Loader is still wanted, it belongs with the code that launches it, documented — neither exists today.
- **Benefit:** Removes an unauditable binary and a dead build from the repo root surface.
- **Effort:** Trivial. **Risk:** Low — no in-repo consumer; external historical use is possible but the files remain in git history. **Confidence:** READ (absence of references: INFERRED from repo-wide grep — no external consumer can be ruled out from inside the tree).

### RE-SMS-110 — Remove the orphan blob `irx/bdmfs_vfat.irx.xz`
- **Category:** 1 (dead code; also 7 clutter)
- **Locations:** `irx/bdmfs_vfat.irx.xz` — the only `.irx.xz` in `irx/` with no plain `.irx` counterpart, and zero references: not in the root Makefile's `IOP_OBJS` (which embeds `bdmfs_fatfs_irx.o`), and no match for `bdmfs_vfat`/`vfat` in `src/`, `include/`, `tools/`, `.github/`, or `docs-site/content/`
- **Problem:** Every other blob in `irx/` is either embedded by the Makefile or is the documented input/output of a `tools/build_*.sh` / `tools/compress_irx.py` step. This one is neither — a superseded FAT driver variant (replaced by `bdmfs_fatfs`) that a reader can easily mistake for a shipped module.
- **Fix:** Delete `irx/bdmfs_vfat.irx.xz`.
- **Benefit:** `irx/` becomes exactly the set of modules the build embeds or the tools regenerate.
- **Effort:** Trivial. **Risk:** Very low — nothing references it; recoverable from history. **Confidence:** READ.

### RE-SMS-111 — `images/` contains an editor backup and two large source PNGs nothing references
- **Category:** 7 (build/CI/clutter)
- **Locations:** `images/jellyfish.jpg.bak-512` (25,390 bytes); `images/main_bg.png` (230,952 bytes); `theme/splash_composited.png` (232,705 bytes)
- **Problem:** The build embeds exactly three JPEGs from `images/` (`Makefile:95,103-104`: `jellyfish.jpg`, `splash.jpg`, `main_bg_mini.jpg` via `bin2c`, consumed at SMS_GUIDesktop.c:40-45). A repo-wide grep (Makefile, `src/`, `tools/`, docs) finds **no reference** to `jellyfish.jpg.bak-512` — it is a resized-variant backup left in the tree — nor to `main_bg.png` / `splash_composited.png`, which by name and size are the INFERRED master artwork for `main_bg_mini.jpg` / `splash.jpg` but are not wired into any bake step (unlike the icon PNGs, which have `tools/bake_*.py`).
- **Fix:** Delete `jellyfish.jpg.bak-512`. For the two PNGs: either wire them into a documented bake step (as the icons are) or move them under `docs/`/artwork with a one-line note — do not leave 463,657 bytes of unreferenced binaries indistinguishable from build inputs.
- **Benefit:** 25,390 bytes removed outright; the remaining two large binaries become discoverable as sources instead of looking like dead build inputs.
- **Effort:** Trivial. **Risk:** Low — the `.bak` has no consumer; the PNGs have no *build* consumer (a human re-editing artwork by hand is the only user, served by the note/move). **Confidence:** READ for the `.bak` and the absence of references (grep over the tree); INFERRED for the two PNGs being the masters of the shipped JPEGs (name/size correspondence only).

### RE-SMS-112 — Turkish font pack ships the same font twice under two names
- **Category:** 2 (duplication)
- **Locations:** `fonts/Turkish by nethunter/latin1.mtf` and `fonts/Turkish by nethunter/WinLatin1.mtf` (both 8,328 bytes, md5 `621f497d0a5d0c5c62df649eda78559b`)
- **Problem:** Byte-identical files in the same folder (the pack's third file, `ascii.mtf` at 6,224 bytes, is distinct, as is every other file in `fonts/` — full-tree md5 census). One of the two names is redundant weight in every clone/ship of the font pack.
- **Fix:** Remove one copy; if both filenames are load-bearing for different tools' expectations, say so in the pack's existing `Readme.txt` and keep both deliberately — otherwise delete `WinLatin1.mtf` (the `latin1.mtf` name matches the sibling packs' naming: `fonts/Hebrew/latin1.mtf`).
- **Benefit:** 8,328 bytes and one confusing either/or choice removed from the pack.
- **Effort:** Trivial. **Risk:** Low — a user who copied the pack and references the removed name in a local config would need the other name; the Readme note covers discovery. **Confidence:** READ (hash equality is exact).

### RE-SMS-113 — Dead code shipped in the SMSTCPIP module: an uncompiled loopif, two debug-only lookup functions that always return 1, a compiled-out input queue, and compiled-out forwarding
- **Category:** 1 (dead code)
- **Locations:** `iop/SMSTCPIP/src/loopif.c` (whole file, 121 lines — absent from `Makefile` IOP_OBJS and `LWIP_HAVE_LOOPIF` is 0 in `include/lwip/opt.h:303-304`; the `AddLoopIF` caller in `ps2ip.c` is likewise compiled out); `iop/SMSTCPIP/src/udp.c:84-141` (`udp_lookup`: every exit returns 1, including the not-found exit at :136-139, and its header-pointer arithmetic is wrong besides; only referenced by `ip_lookup` in `ip.c`, itself `#ifdef LWIP_DEBUG`-only with no live caller); `iop/SMSTCPIP/src/ps2ip.c` `InputCB` + QUEUE_INPUT block (`#define QUEUE_INPUT 0`); `iop/SMSTCPIP/src/ip.c` `ip_forward()` and `iop/SMSTCPIP/src/icmp.c:162-205` (`icmp_time_exceeded`) under `IP_FORWARD 0` (lwipopts.h:117)
- **Problem:** ~450 lines that cannot execute are carried in the source the maintainers must read, and two of the items (loopif's unacked-NULL comment, QUEUE_INPUT's alternate input path) actively mislead debugging of the live paths.
- **Fix:** Delete loopif.c, udp_lookup/ip_lookup, and the QUEUE_INPUT block; keep ip_forward/icmp_time_exceeded only if forwarding is a stated roadmap item, otherwise delete with the rest.
- **Benefit:** Smaller, honest source tree; the live input path (`ps2ip_input` direct) becomes unambiguous.
- **Effort:** Small. **Risk:** Low — all items are provably unreferenced/uncompiled in this build. **Confidence:** READ.

### RE-SMS-114 — SO_REUSE port-sharing machinery is compiled in for a single-application media client that never shares ports
- **Category:** 1 (dead code)
- **Locations:** `iop/SMSTCPIP/include/lwip/opt.h:338-340` (`SO_REUSE 1`); machinery: `iop/SMSTCPIP/src/tcp_in.c` reuse loop with `p->ref++` + `goto again_1`, `iop/SMSTCPIP/src/udp.c` `udp_input` reuse loops (`again_1`/`again_2`, `p->ref++`), reuse branches in `tcp_bind` (`tcp.c`) and `udp_bind` (`udp.c`) — several hundred lines across four files
- **Problem:** SO_REUSEADDR/SO_REUSEPORT fan-out (delivering one datagram/segment to multiple PCBs, with manual pbuf refcount juggling) exists to let multiple processes share a port. SMS is the sole consumer of this stack; nothing in the tree binds two sockets to one port deliberately. The machinery is the most fragile code in both input paths (refcount + goto re-scan) and it runs on every packet.
- **Fix:** Set `SO_REUSE` to 0 in lwipopts.h/opt.h (the `#ifndef` guards already provide the plain paths) and delete the reuse-only branches once the simpler paths are confirmed building. Note the sockets layer's SO_REUSEADDR *option storage* (`so_options` bits in getsockopt/setsockopt) is independent and can stay.
- **Benefit:** Deletes the riskiest per-packet branches in tcp_input/udp_input; bind checks become the simple already-written non-REUSE versions.
- **Effort:** Medium (rebuild + on-hardware smoke test of normal client/server flows). **Risk:** Medium — any hidden reliance on double-binding would surface as ERR_USE; that is precisely the behaviour to learn about in testing rather than in production. **Confidence:** READ.

### RE-SMS-115 — memp reserves a RAW-PCB pool for a protocol that has no implementation file
- **Category:** 4 (size/memory)
- **Locations:** `iop/SMSTCPIP/src/memp.c` (`memp_sizes`/`memp_num`/`memp_memory` include `MEMP_NUM_RAW_PCB` = 4, `include/lwip/opt.h:78-79`); `include/lwip/raw.h` declares the PCB/API; there is no `raw.c` in `src/` or in the Makefile, `LWIP_RAW` is never defined anywhere in the tree, and the two `#if LWIP_RAW` call sites (`ip.c` `ip_input`, `api_msg.c` `recv_raw`) compile out
- **Problem:** Four raw_pcb slots are permanently carved out of `memp_memory`, and the header/API surface advertises a feature that cannot link if used. Small in bytes (~4 × (sizeof(raw_pcb)+header)), but it is pure waste in a 2 MB-RAM-class IOP budget and a trap for anyone reading the API surface.
- **Fix:** Set `MEMP_NUM_RAW_PCB` to 0 in lwipopts.h (or delete the RAW slot from the memp tables) and delete `include/lwip/raw.h` + the `raw` member references if the protocol is not planned.
- **Benefit:** Reclaims pool space and removes a phantom API.
- **Effort:** Trivial. **Risk:** Low. **Confidence:** READ.

### RE-SMS-116 — Half-finished API surface: six api.h functions have no implementation, and the exported shutdown entry point is an empty stub
- **Category:** 8 (half-finished)
- **Locations:** `iop/SMSTCPIP/include/lwip/api.h` declares `netbuf_alloc`, `netbuf_free`, `netbuf_data`, `netbuf_next`, `netbuf_chain`, `netconn_err` — grep of all of `src/` finds no definition for any of them (api_lib.c implements only new/delete/ref/copy_partial and the netconn_* set minus `netconn_err`); `iop/SMSTCPIP/src/ps2ip.c` `ps2ip_ShutDown()` (export slot 3, `src/exports.tab`) is `{ return 1; }` — it stops no thread, alarm, or semaphore, and four further export slots (2, 4, 32, 37) are the empty `ps2ip_Stub`
- **Problem:** Anything linking against the documented netbuf API fails at link time, not at design time; and the module advertises a shutdown that does not shut anything down (TimerThread alarms and the tcpip thread run forever), so "unload/reload the stack" flows leak IOP threads and semaphores.
- **Fix:** LESS option — delete the six unimplemented declarations (nothing in-tree calls them) and either delete the ShutDown export or rename the expectation: implementing a real teardown is the MORE path and should only be taken if a reload flow actually needs it. The reserved `ps2ip_Stub` slots are normal IRX practice and can stay.
- **Benefit:** The declared API becomes exactly the implemented API; no phantom teardown contract.
- **Effort:** Trivial (deletion path). **Risk:** Low. **Confidence:** READ.

### RE-SMS-117 — Remove six declaration-only prototypes with no definition anywhere
- **Category:** 1 (dead code)
- **Locations:** `include/SMS.h:305` — `SMS_Align` (only occurrence in the tree; distinct from the live `SMS_AlignBits`); `include/SMS.h:306` — `SMS_SetSifCmdHandler` (the real function was renamed: declaration `include/SMS_IOP.h:76`, definition `src/SMS_IOP.c:192`, callers `src/SMS_GUI.c:978-982` — all as `SMS_IOPSetSifCmdHandler`; the stale end-comment at `src/SMS_IOP.c:196` still says `/* end SMS_SetSifCmdHandler */`); `include/SMS.h:308` — `SMS_StartNetwork` (only occurrence); `include/SMS.h:309` — `SMS_ResetIOP` (only occurrence); `include/SMS_IOP.h:95` — `SMS_HDDUMount` (only occurrence); `include/SMS_MPEG.h:286` — `SMS_MPEG_SCB` (only occurrence)
- **Problem:** Prototypes promise APIs that do not exist. Anyone calling one gets a link error only after writing code against it; `SMS_SetSifCmdHandler` is worse — it looks like the canonical name while the tree actually exposes the differently-named `SMS_IOPSetSifCmdHandler` from a second header.
- **Fix:** Delete the six prototypes; fix the stale end-comment at `src/SMS_IOP.c:196` to the current function name while there.
- **Benefit:** Header surface matches reality; removes a rename trap in the central `SMS.h`.
- **Effort:** Trivial. **Risk:** Very low — a declaration with no definition cannot be called successfully today. **Confidence:** READ.

### RE-SMS-118 — Dead H.263 inter-unquantize path: empty function, `#if 0` body that cannot compile, wired but never called
- **Category:** 1 (dead code; also 8, half-finished)
- **Locations:** `src/SMS_MPEG.c:904-935` (function), `src/SMS_MPEG.c:205` and `src/SMS_MPEG4.c:513` (assignments), `include/SMS_MPEG.h:150` (pointer member), `include/SMS_H263.h:27` (prototype)
- **Problem:** `SMS_H263_DCTUnquantizeInter` has its entire body inside `#if 0 … #endif` (`src/SMS_MPEG.c:905-934`), and the disabled body references undeclared identifiers (`aQScale`, `aN`), so it cannot simply be switched back on — it is abandoned, not disabled. The resulting empty function is still installed as `g_MPEGCtx.DCT_UnquantizeInter` in two places, yet a tree-wide search shows the pointer is never invoked anywhere (the only unquantize call site, `src/SMS_MPEG.c:1196`, calls `DCT_UnquantizeIntra`; inter unquantization is done inline by the block decoders). The wiring actively misleads: it suggests H.263 inter blocks pass through a no-op.
- **Fix:** Delete the empty function and its `#if 0` body, the two assignments, the prototype, and — if no out-of-scope caller exists — the `DCT_UnquantizeInter` member. Pure removal.
- **Benefit:** ~35 lines and one misleading indirection gone; nobody can "fix" playback by enabling code that never ran.
- **Effort:** Small. **Risk:** Low (verify no IOP-side or out-of-scope caller references the symbol before deleting the member). **Confidence:** READ.

### RE-SMS-119 — ASF stream-bitrate table is parsed into one array and applied from a different, never-written one
- **Category:** 1 (dead code)
- **Locations:** `src/SMS_ContainerASF.c:140` and `:149` (local `lBitRate[128]`, zeroed), `:322-330` (STREAM_BITRATE handler writes `lpASF->stream_bitrates[lStmID]`), `:346-352` (final loop applies `lBitRate[i]`), `:90` (`stream_bitrates` field — written at :328, read nowhere; verified by grep over the file)
- **Problem:** The header reader maintains two bitrate stores. File data lands in `ASFContext.stream_bitrates[]`; the post-header loop that fills in missing codec bitrates reads the local `lBitRate[]`, which nothing ever writes except the initial `memset` to zero — and the assignment is guarded by `if (!lpCodec->m_BitRate)`, so it writes 0 over 0. The whole STREAM_BITRATE path is therefore dead: parsed, stored, never applied. It also costs a 512-byte zeroed array on the EE stack for the duration of the header parse.
- **Fix:** Apply from `lpASF->stream_bitrates[i]` in the final loop (the loop index `i` is already the ASF stream ID, which is how the handler keyed the array) and delete the local `lBitRate[128]` and its `memset`.
- **Benefit:** ASF streams whose WAV header carries no bitrate get the file's declared bitrate (which feeds the duration math of RE-SMS-082), and 512 bytes of stack come back.
- **Effort:** S. **Risk:** Low. **Confidence:** READ + grep-verified write-only/read-only pairing.

### RE-SMS-120 — ASF index support is scaffolding for a feature that was never built
- **Category:** 8 (half-finished features)
- **Locations:** `src/SMS_ContainerASF.c:82-85` (`ASFIndex` struct), `:96` (`index_read`), `:116` (`index_ptr` in `ASFContext`); container setup `src/SMS_ContainerASF.c:695-723` (sets `ReadPacket` and `Destroy` only — no `Seek`, never sets `SMS_CONT_FLAGS_SEEKABLE`)
- **Problem:** The context reserves an index pointer, an index-read flag and an index record type, and nothing in the file (verified by grep) ever allocates, reads, or tests any of them. ASF seeking was apparently planned and abandoned, leaving dead state that suggests to every future reader that an index path exists.
- **Fix:** Delete the struct, both fields, and nothing else. If ASF seeking is ever actually built, it will re-derive what it needs from the spec, not from these three declarations.
- **Benefit:** ~10 lines of misleading state gone; the container's real capability set (sequential read only) becomes obvious from its code.
- **Effort:** S. **Risk:** Low. **Confidence:** READ + grep-verified zero uses.

### RE-SMS-121 — Write-only state: fields and struct members that are stored and never read
- **Category:** 1 (dead code)
- **Locations:** `src/SMS_ContainerAVI.c:43` + `:109` (`_AVIStream.m_Idx` — assigned the stream index in `_NewStream`, read nowhere); `src/SMS_ContainerASF.c:71-72` + `:182/:185` (`ASFMainHeader.create_time`, `play_time` — parsed and stored, read nowhere); `src/SMS_ContainerASF.c:187` (`hdr.ignore`, same); `src/SMS_ContainerASF.c:64` + `:538` (`ASFStream.seq` — written from `packet_seq` on every assembled packet, read nowhere)
- **Problem:** One mechanism: values are computed/stored and never consumed (each verified by grep over its file: the only occurrences are the declaration and the write). They cost struct space (the ASF header fields sit in the heap-allocated context), a per-packet store in ASF's hot path, and — worse — they read as if something depends on them, which discourages touching the surrounding code. Note the `File_Get*` calls that *fill* the ASF header fields must stay (they advance the cursor); only the stores/fields go.
- **Fix:** Delete the four members; replace the ASF header stores with bare `File_GetULong`/`File_GetUInt` calls (return value discarded, as dozens of other skip-reads in the same function already do); delete the `seq` assignment.
- **Benefit:** Small memory/stack saving and, mainly, the structs stop lying about what the demuxers use.
- **Effort:** S. **Risk:** Low. **Confidence:** READ + grep-verified write-only.

### RE-SMS-122 — DTS: file-scope sample-rate/bit-rate tables are dead duplicates of the live copies inside `_dts_syncinfo`
- **Category:** 1 (dead code; with 2 duplication)
- **Locations:** `src/SMS_DTS.c:127-143` (`dts_sample_rates[16]`, `dts_bit_rates[32]` — a grep of the whole file shows no reference to either name outside these declarations); the live, identical tables are the function-local `sl_SampleRate[16]` / `sl_BitRate[32]` at `src/SMS_DTS.c:10105-10121`
- **Problem:** 48 ints (192 bytes of `.data`) are carried in the binary and maintained in parallel with the copies actually used — a standing trap: anyone patching rates in the file-scope tables changes nothing.
- **Fix:** Delete the file-scope pair; keep the `_dts_syncinfo` locals.
- **Benefit:** 192 bytes and one duplication trap removed.
- **Effort:** Trivial. **Risk:** None (zero references, grep-verified). **Confidence:** READ.

### RE-SMS-123 — AC3 IMDCT: `aBias` parameter of `_ac3_imdct_512`/`_ac3_imdct_256` is threaded through every call but never used
- **Category:** 1 (dead code)
- **Locations:** `src/SMS_AC3_imdct.c:533` (`_ac3_imdct_512(sample_t*, sample_t*, sample_t aBias)`); `src/SMS_AC3_imdct.c:579` (`_ac3_imdct_256`, same); call sites passing `s_AC3Ctx.m_Bias`/`lBias` at `src/SMS_AC3.c:2401,2432-2480`. Neither function body references the parameter (the bias is applied by callers directly to the sample buffer, e.g. `src/SMS_AC3.c:2440`)
- **Problem:** A dead parameter on two hot functions, plus the context plumbing that feeds it, misleads every reader into hunting for bias handling inside the IMDCT that isn't there.
- **Fix:** Drop the parameter from both functions and their call sites. (The `m_Bias` field itself stays — callers use it as a fill value.)
- **Benefit:** Removes a false trail in the AC3 DSP path; negligible size win.
- **Effort:** Trivial. **Risk:** None (parameter provably unread in both bodies). **Confidence:** READ.

### RE-SMS-124 — AC3: `s_SAB` is declared `[20]` with only 8 entries initialized; its sole consumer reads 2 of them
- **Category:** 4 (size/memory)
- **Locations:** `src/SMS_AC3.c:318-321` (`int s_SAB[20]` with exactly 8 initializers — `0x38504E62` ×4 then `0x3CCC2D41` ×4, script-counted; the remaining 12 entries are implicit zeros); sole use at `src/SMS_AC3.c:1697` (asm in `_ac3_mix32toS` loads the table base; the two distinct values are the only ones ever read, as float bit patterns)
- **Problem:** 48 bytes of `.sdata` carry 12 zero entries nothing reads, and the `[20]` declaration invites a reader to believe a 20-entry scale table exists.
- **Fix:** Declare `s_SAB[8]`.
- **Benefit:** 48 bytes and one misleading declaration removed.
- **Effort:** Trivial. **Risk:** None (single consumer, grep-verified). **Confidence:** READ.

### RE-SMS-125 — `lzma2.c` exports dead, collision-prone functions named `encode` and `decode`
- **Category:** 1 (dead code)
- **Locations:** `src/lzma2.c:10-27` (`encode`), `src/lzma2.c:29-49` (`decode`)
- **Problem:** Tree-wide grep shows `encode` has **no callers at all** — not even inside `lzma2.c` — and `decode` is used only by `lzma2_decode_size` in the same file. Neither is declared in `include/lzma2.h`. Both have external linkage under the two most generic names imaginable in a monolithically linked ELF that also links AAC/MP3/FLAC codec code: any future object defining `encode`/`decode` collides at link time, and today's dead `encode` is pure surface area.
- **Fix:** Delete `encode`; make `decode` `static`. (If a varint encoder is ever needed for a writer path, it can come back with an `lzma2_`-prefixed name in the header.)
- **Benefit:** −19 lines, two fewer exported symbols, one link-collision hazard gone.
- **Effort:** Trivial. **Risk:** None found — no caller exists to break (grep over `src/` + `include/`). **Confidence:** READ.

### RE-SMS-126 — Generated icon header (and its generator) still say "11 dev" — there are 13 dev slots
- **Category:** 7 (build/CI/clutter)
- **Locations:** `src/SMS_IconsRGBA.c:1-6` (generated header); `tools/bake_icons_rgba.py` (header template string and the `main()` comment, both saying 11); authoritative counts: `tools/bake_icons_rgba.py` `DEV` list (13 names, script-counted), `src/SMS_GUIcons.c` `GUI_LoadIcons` (dev loop `i < 13`, comment says 13)
- **Problem:** The arithmetic proves 13: the blob decompresses to 205,824 bytes = 9×4,096 + 12×4,096 + **13**×9,216; with 11 dev icons it would be 187,392. The generated file's own header — the text a maintainer reads first when touching the icon pipeline — understates the dev set by two slots (MX4SIO/ATA/iLink/MMCE/DVDVID/CDVDERROR were added after the text was written), and because the text lives in the bake tool's template, re-running the tool re-emits the wrong header.
- **Fix:** Change "11 dev" to "13 dev" in the tool's template/comment and in the generated file's header (or regenerate). Two words in two places.
- **Benefit:** The next person extending the device-icon set works from a header that matches the slicing code instead of silently mis-slicing by two icons.
- **Effort:** Trivial. **Risk:** None (comment-only). **Confidence:** READ (slot lists counted by script; raw-size arithmetic verified by actual decompression).

### RE-SMS-127 — `src/SMS_Menu.c` is a dead duplicate stub that is not in the build
- **Category:** 1 (dead code)
- **Locations:** `src/SMS_Menu.c:1-7` (entire file: `GUI_CreateMenu` returning `0`). The real implementation is `src/SMS_GUIMenu.c:403-414`; `SMS_Menu.o` does not appear in `EE_OBJS` in the root `Makefile` (lines 22-47), while `SMS_GUIMenu.o` does
- **Problem:** A second definition of a live symbol sitting in `src/` invites a future build-list edit (or a wildcard object list) to link the stub — which would silently break every menu — and misleads any reader grepping for `GUI_CreateMenu` to a 7-line fake.
- **Fix:** Delete the file. Nothing references it and it is not compiled.
- **Benefit:** Removes a symbol-collision trap and a false reading target for zero functional cost.
- **Effort:** Trivial. **Risk:** None. **Confidence:** READ.

### RE-SMS-128 — USBD exports three stubs that report success (USBD)
- **Category:** 8 (half-finished)
- **Locations:** `iop/USBD/src/interface.c:216-229`
- **Problem:** `UsbRegisterAutoloader`, `UsbUnregisterAutoloader` and `UsbChangeThreadPriority` are exported library functions whose entire body is a `dbg_printf("... stub")` and `return 0` (success). Any driver that registers an autoloader through this USBD build is silently never autoloaded, with no error to diagnose from.
- **Fix:** Either implement registration (the autoloader list mechanism is the driver's own design) or return a negative error so callers can tell the feature is absent. Returning an error is the LESS change and matches the stub's reality.
- **Benefit:** Converts a silent no-op success into an honest failure for dependent drivers.
- **Effort:** Trivial (if error return chosen). **Risk:** Low — a caller that depended on the fake success was already not getting the feature. **Confidence:** READ.

### RE-SMS-129 — File mapping is half-finished: every map returns the same fixed virtual address
- **Category:** 8 (half-finished)
- **Locations:** `src/SMS_FileMapping.c:270-302` (`SMS_FileMappingMap`: `void* retVal = (void*)0x32000000; /* implement VAddr allocation here */`), `:276` (`calloc` unchecked before linking), `:294` (`memalign` unchecked before building TLB entries from it), `:304-337` (`SMS_FileMappingUnMap`: removing the head node leaves the new head's `m_pPrev` pointing at the freed node). Sole consumer is JPEG decode (`src/SMS_JPEG.c`)
- **Problem:** The stub comment is literal: there is no virtual-address allocation, so a second concurrent map would alias the first mapping's address and TLB window. Today that is latent (one JPEG at a time in practice), but the API advertises a general mapping service it does not implement, and its error paths (failed `calloc`/`memalign`) corrupt the map chain instead of failing.
- **Fix:** Prefer removal/simplification over completing it: either (a) document and enforce the single-mapping reality — return NULL if `s_pMapChain` is non-empty, check both allocations, fix the head-removal `m_pPrev` — or (b) if no second mapping is ever planned, fold the single window into the JPEG path and delete the chain entirely. Do not build a general VAddr allocator for one caller.
- **Benefit:** Turns a silent aliasing landmine into an explicit, checked contract (or deletes the mechanism).
- **Effort:** Small for (a). **Risk:** Medium (TLB/refill asm in the same file must not change). **Confidence:** READ.

### RE-SMS-130 — `FATFS_Init` leaks its filesystem allocation on two early returns (SMSUMS)
- **Category:** 4 (size/memory)
- **Locations:** `iop/SMSUMS/src/ums_fat.c:1002` (allocation), `:1043` and `:1044-1046` (early returns), `:1131` (the free the early returns bypass)
- **Problem:** `lpFS` (`sizeof(FATFS) + lSecSize`) is allocated at :1002. The bad-signature return (:1043) and the zero-total-sectors return (:1044-1046) `return retVal` directly, skipping the `if (!retVal) FreeSysMemory(lpFS)` cleanup at :1131 that the other failure paths (BPB whitelist mismatch) do reach. Every mount attempt against a non-FAT or corrupt stick leaks the allocation; media is removable, so attempts repeat.
- **Fix:** Route both returns through the existing cleanup (drop the early `return`s, let control fall to :1131).
- **Benefit:** Removes a per-mount-attempt IOP memory leak.
- **Effort:** Trivial. **Risk:** Low. **Confidence:** READ.

### RE-SMS-131 — MOV carries two-to-three parallel copies of the `hdlr`/`stsd`/`esds` parsers that have already drifted apart
- **Category:** 2 (duplication)
- **Locations:** `src/SMS_ContainerMOV.c:275-295` (`_mov_read_hdlr`), `:297-320` (`_mov_read_hdlr_m4a`), `:705-720` (`_mov_read_hdlr_m4ap`); `:322-427` (`_mov_read_stsd`), `:722-766` (`_mov_read_stsd_m4ap`); `:429-485` (`_mov_read_esds`), `:768-810` (`_mov_read_esds_m4ap`)
- **Problem:** The full demuxer and the M4A probe each implement the same three ISO-BMFF atoms, differing mainly in where results are stored (a real `SMS_Stream` vs a bare `SMS_AudioInfo`). The copies have demonstrably drifted: the probe's `esds` bounds-checks its descriptor length against the atom (`src/SMS_ContainerMOV.c:784-786`) while the demuxer's `esds` needed a separate `_tbl_fits` retrofit (`src/SMS_ContainerMOV.c:460-470`); the demuxer's `stsd` walks child atoms, the probe's doesn't; the probe's AAC-specific config extraction exists only in its copy. Every future fix to one copy must be remembered for the other — the file's own comments show that has already failed once.
- **Fix:** Parameterise the three demuxer handlers by their result sink only where the bodies are truly identical (the `hdlr` subtype test and the `esds` descriptor walk), and leave genuinely different bodies (audio-info extraction) separate; do not build a general atom framework.
- **Justification:** This *consolidates* rather than adds: the net is fewer parser bodies for the same atoms. It is justified only for the identical stretches — a wholesale merge would be a new abstraction and is explicitly not proposed.
- **Benefit:** One place to fix the next `esds`/`stsd` bounds issue instead of two; removes roughly 100 lines of near-duplicate parsing (estimate from the ranges above).
- **Effort:** L. **Risk:** Med. **Confidence:** READ (all seven functions read in full).

### RE-SMS-132 — Whole-file asset loading is triplicated (skin) and triplicated again (language)
- **Category:** 2 (duplication)
- **Locations:** Skin: `src/SMS_GUIDesktop.c:155-317` (`DrawSkin` contains three near-identical open → seek-end → capped `malloc` → read → verify-length blocks: CWD/fio, BDM mc/fio, non-BDM `MC_*`). Language: `src/SMS_Locale.c:826-897` (same three-way split, same block shape, differing only in open/read/close calls). History's three loops are the third instance of the pattern and are counted in RE-SMS-037
- **Problem:** Every fix to the load pattern (size cap, read-length check, allocation check) must be made three times per asset — and the record shows it has been: the skin blocks carry the `0xFFFF0` cap and read checks, the language blocks (RE-SMS-054) do not. The divergence is the bug farm, not the line count.
- **Fix:** One static load-whole-file helper per file (open callback or a fio-only path where the BDM migration already made `MC_*` unnecessary — locale and palette already argue this in their own comments), with the cap/check inside it; the three call sites keep only their path construction.
- **Justification:** The helper is justified by three concrete existing copies per file and is a net deletion in both files.
- **Benefit:** Single place to cap/check every asset load; removes ~120 duplicated lines across the two files.
- **Effort:** Medium. **Risk:** Medium (non-BDM `MC_*` path must keep working or be explicitly retired like its siblings). **Confidence:** READ.

### RE-SMS-133 — The PCB-creation switch is triplicated across do_newconn/do_bind/do_connect — and the copies have already diverged
- **Category:** 2 (duplication)
- **Locations:** `iop/SMSTCPIP/src/api_msg.c:285-330` (do_newconn), `:386-415` (do_bind), `:455-500` (do_connect) — census by grep: `udp_new()` ×9 and `tcp_new()` ×3 in this one file, in three near-identical 4-arm switches (UDPLITE / UDPNOCHKSUM / UDP / TCP)
- **Problem:** Three copies of one mechanism ("create the right PCB for this netconn type and attach recv_udp/setup_tcp"). The copies have already drifted: do_connect and do_newconn NULL-check every allocation, do_bind checks none — RE-SMS-042 is a direct product of this duplication.
- **Fix:** Extract one `create_pcb(msg->conn)` helper returning err_t, called by all three; do_bind keeps only its bind call, do_connect only its connect call. Net deletion of ~60 lines.
- **Benefit:** One place to get allocation checking right; prevents the next divergent copy.
- **Effort:** Small. **Risk:** Low — mechanical, behaviour-preserving except fixing RE-SMS-042 in passing. **Confidence:** READ.

### RE-SMS-134 — Fold the eleven one-declaration `SMS_Container*.h` stub headers into `SMS_Container.h`
- **Category:** 2 (duplication; also 3 overengineering)
- **Locations:** `include/SMS_ContainerAAC.h` (28 lines), `SMS_ContainerAC3.h` (28), `SMS_ContainerASF.h` (28), `SMS_ContainerAVI.h` (27), `SMS_ContainerFLAC.h` (28), `SMS_ContainerJPG.h` (36), `SMS_ContainerM3U.h` (35), `SMS_ContainerMOV.h` (29), `SMS_ContainerMP3.h` (28), `SMS_ContainerMPEG_PS.h` (27), `SMS_ContainerOGG.h` (29) — ≈323 lines total carrying 1–2 prototypes each (e.g. `SMS_ContainerAVI.h` carries only `int SMS_GetContainerAVI ( SMS_Container* );`), each already `#include`-ing `SMS_Container.h`. 48 include sites across 16 source files
- **Problem:** Eleven files exist to declare one function each for the same container registry that `SMS_Container.h` already owns (`SMS_GetContainer`, the `SMS_Container` struct and its function-pointer table). The per-format split buys nothing: every stub includes the central header anyway, so there is no compile-time isolation.
- **Fix:** Move the ≈15 prototypes into `SMS_Container.h`, delete the eleven stubs, and point the 48 include sites at `SMS_Container.h` (most already include it transitively/directly).
- **Benefit:** −11 files / ≈300 lines; one place declares the whole container surface.
- **Effort:** Medium (mechanical include edits in 16 files; build + host container tests in CI cover the fallout). **Risk:** Low-medium — purely mechanical, but touches many files; do it as its own commit. **Confidence:** READ.

### RE-SMS-135 — MOV/M4A creators select their parser by mutating a shared global parse table at runtime
- **Category:** 3 (overengineering)
- **Locations:** `src/SMS_ContainerMOV.c:1529` (`SMS_GetContainerMOV` writes `s_MOVParseTbl[0].m_Func = _mov_read_hdlr`), `src/SMS_ContainerMOV.c:1544` (`SMS_GetContainerM4A` writes `s_MOVParseTbl[0].m_Func = _mov_read_hdlr_m4a`), table definition `src/SMS_ContainerMOV.c:812-837`
- **Problem:** Both creators pass the *same* table to `_GetContainerMOV` after overwriting its first entry to select the `hdlr` behaviour. That forces the table to live in writable RAM (it can never be `const`/ROM), makes the selection depend on whichever creator ran last, and means the table's content at any moment is invisible from its definition. The file already shows the cleaner pattern: the M4A *probe* has its own separate table (`s_M4AProbeTbl`, `src/SMS_ContainerMOV.c:839-851`).
- **Fix:** Give the full M4A demuxer its own complete parse table differing only in the `hdlr` entry (copy the MOV table, change one line), mark both `const`, and delete both runtime writes; `_GetContainerMOV` already takes the table as a parameter.
- **Justification:** The fix adds one static table (~24 entries) — justified because it *removes* a runtime-mutation mechanism, restores const-ness (ROM instead of RAM on the EE), and eliminates an order-dependence between two creators; net mechanism count goes down, not up.
- **Benefit:** Parse tables become link-time facts; a future second container or probe reordering cannot silently inherit the wrong `hdlr`.
- **Effort:** M. **Risk:** Low. **Confidence:** READ.

### RE-SMS-136 — Fold the redundant explicit object rules in the root Makefile
- **Category:** 2 (duplication; also 7)
- **Locations:** `Makefile:128-147`. `SMS_GS_1.o` and `SMS_GS_2.o` have explicit rules byte-identical in effect to the generic `$(EE_OBJ_DIR)%.o : $(EE_SRC_DIR)%.c` rule at `Makefile:125-126` (same command, no extra prerequisites). The `SMS_MPEG4.o` / `SMS_MSMPEG4.o` / `SMS_MPEG.o` / `SMS_H263.o` rules differ from the generic rule only by adding the prerequisite `$(EE_INC_DIR)SMS_MPEG.h`
- **Problem:** Six explicit rules restate the generic rule; two of them add nothing at all. Anyone changing compile flags must check whether the explicit copies shadow the generic rule (they do — that is how make resolves them), so the duplication is a standing flag-drift hazard.
- **Fix:** Delete the `SMS_GS_1.o`/`SMS_GS_2.o` rules outright. Replace the four MPEG rules with one grouped prerequisite-only line: `$(EE_OBJ_DIR)SMS_MPEG4.o $(EE_OBJ_DIR)SMS_MSMPEG4.o $(EE_OBJ_DIR)SMS_MPEG.o $(EE_OBJ_DIR)SMS_H263.o: $(EE_INC_DIR)SMS_MPEG.h` (a prerequisite-only rule merges with the generic pattern rule instead of replacing it).
- **Benefit:** ≈20 lines removed; one compile-command definition for all EE objects.
- **Effort:** Trivial; verify with a clean `make` in the ps2dev container (the CI build does exactly this). **Risk:** Low — prerequisite-only rules are standard make; CI's BDM=0 and BDM=1 builds exercise every object. **Confidence:** READ.

### RE-SMS-137 — `dev9.h` duplicated verbatim in two IOP trees
- **Category:** 2 (duplication)
- **Locations:** `iop/SMSDev9/include/dev9.h` and `iop/SMSUdpfs/smap/include/dev9.h` (53 lines each; `diff` — identical)
- **Problem:** The DEV9 ABI header exists twice with no note in either copy that the other exists; an ABI change made in one tree silently desynchronises the UDPFS SMAP driver from the DEV9 driver it registers callbacks with. Caveat: the duplication is partly load-bearing — the UDPFS build deliberately shadows the SDK's dev9 header with the v1.1 ABI (see `tools/build_udpfs.sh` / `tools/build_ata_bd_dev9v11.sh`), so a naive "delete one copy" breaks the include-order trick.
- **Fix:** Keep both files (the shadowing is intentional) but make the pair self-maintaining: a one-line comment in each copy naming the other and requiring both to change together. LESS than any build-system consolidation.
- **Benefit:** Prevents a silent ABI skew between two drivers that share hardware callbacks.
- **Effort:** Trivial. **Risk:** Low. **Confidence:** READ.

### RE-SMS-138 — Deduplicate `AGENTS.md` / `CLAUDE.md` (byte-identical files)
- **Category:** 2 (duplication)
- **Locations:** `AGENTS.md` and `CLAUDE.md` — `diff` reports no differences; both are the same 1,254-byte GitHub Attribution Policy
- **Problem:** Two root files carry the same policy text; any future edit must be made twice or the two agent-facing files will silently diverge — and divergence here is specifically bad, because the file's whole point is a rule agents follow.
- **Fix:** Keep `AGENTS.md` as the canonical text and reduce `CLAUDE.md` to a one-line pointer (`See AGENTS.md`), or vice versa — whichever the maintainer's tooling reads first.
- **Benefit:** One canonical policy file; no divergence risk.
- **Effort:** Trivial. **Risk:** Very low — some tools read only one of the two filenames; a pointer line is still read by both. **Confidence:** READ.

### RE-SMS-139 — IPU suspend/resume duplicated verbatim as "dummy" variants
- **Category:** 2 (duplication)
- **Locations:** `src/SMS_IPU.c:584-596` (`IPU_Suspend`/`IPU_Resume`), `src/SMS_IPU.c:605-617` (`IPU_DummySuspend`/`IPU_DummyResume`)
- **Problem:** `IPU_DummySuspend` is byte-identical to `IPU_Suspend` (`DisableDmac(DMAC_I_GIF);`) and `IPU_DummyResume` is byte-identical to `IPU_Resume` (`DMAC->m_STAT = 4; EnableDmac(DMAC_I_GIF);`) — including a stale `/* end IPU_Suspend */` comment on the dummy. Two copies of DMA register manipulation will drift.
- **Fix:** Delete the two dummy bodies and point the dummy vtable slots at `IPU_Suspend`/`IPU_Resume` directly.
- **Benefit:** One definition of IPU suspend/resume semantics.
- **Effort:** Trivial. **Risk:** Negligible. **Confidence:** READ.

### RE-SMS-140 — MOV index builder resets every stream's chunk cursor per chunk, making derived chunk sizing quadratic
- **Category:** 5 (performance, structural)
- **Locations:** `src/SMS_ContainerMOV.c:1000-1040` (per-chunk search over all streams' chunk-offset tables; cursor reset loop at :1033-1037)
- **Problem:** In `_mov_build_index`'s per-chunk audio path, when a chunk's size must be derived from neighbouring chunk offsets, the search records where it stopped in each stream's `m_NextChunk` — and the block immediately afterwards resets *every* stream's `m_NextChunk` to 0. The next chunk therefore rescans all offset tables from the beginning: O(chunks² × streams) offset comparisons to build an index that is linear with the cursors kept. The reset also makes the `m_NextChunk` writes inside the search pure waste.
- **Fix:** Delete the reset loop; the cursors are per-stream monotonic because chunks are visited in offset order within this walk (the search itself only ever looks for the first offset *greater* than the current one).
- **Benefit:** Index build for affected audio MP4s (fixed/unknown sample size, no samples-per-frame info) drops from quadratic to linear; on a long file that is the difference between instant and a visible hang at open.
- **Effort:** S. **Risk:** Med (behaviour depends on the monotonic-visit argument above; needs one real-file test per affected sub-case). **Confidence:** Med (reset and search READ; INFERRED reachability — the derived-size branch requires `m_SampleSize <= 1` with no per-sample table and no samples-per-frame data).

### RE-SMS-141 — `smb_GetShareList` spends one full NetShareEnum RPC per share (smbman-sms)
- **Category:** 5 (performance, structural)
- **Locations:** `iop/smbman-sms/src/smb_fio.c:1015-1035`
- **Problem:** Share listing does one NetShareEnum to count shares, then **one additional full NetShareEnum round trip per share** (`smb_NetShareEnum(..., i, 1)` in the loop) to fetch entries one at a time into a single `ShareEntry_t`. Enumeration cost is O(n) network round trips on the IOP↔server path for what the first call's response format already pages through a caller buffer.
- **Fix:** Fetch into a bounded local array in as few enum calls as the existing `smb_NetShareEnum` buffering allows (it already accepts an index/count window), then DMA the entries out. No protocol change.
- **Benefit:** Share browsing on large servers drops from n+1 round trips to a small constant.
- **Effort:** Medium. **Risk:** Medium (touches the EE DMA contract for the list) — worth doing only if share browsing latency is a felt problem. **Confidence:** READ.

### RE-SMS-142 — Replace the hand-maintained 64-line host-test list in CI with discovery
- **Category:** 7 (build/CI/clutter; also 3 overengineering of process)
- **Locations:** `.github/workflows/compilation.yml`, step "Host playback and file I/O regression checks" — ≈64 individually named `python3 tools/test_*.py` invocations
- **Problem:** The list is a second, manual registry of what already exists on disk. It is currently complete — verified by script: all 59 `tools/test_*.py` files are referenced across the workflows — but nothing enforces that: the next `tools/test_*.py` a contributor adds will silently never run in CI unless they also edit this YAML list in the right step. The failure mode is invisible (a missing line is not an error).
- **Fix:** Replace the enumerated lines with a loop over `tools/test_*.py` (keeping the two non-`test_` scripts `tools/check_devmenu.py` / its test explicitly), or add a CI step that fails when a `tools/test_*.py` file is not referenced. The loop is the LESS option: one mechanism, no registry.
- **Benefit:** Test coverage can no longer silently drift from the filesystem; ≈60 lines of YAML deleted.
- **Effort:** Small. **Risk:** Low — a loop changes failure granularity (first failure stops the step, as today with `run:` default shell behaviour); keep per-file echo so logs still name the failing test. **Confidence:** READ.

### RE-SMS-143 — Drop stale `src/lzma2` references from `.gitignore` and the Makefile link path
- **Category:** 7 (build/CI/clutter)
- **Locations:** `.gitignore` (`*.a` exception block: `!/src/lzma2/` and `!/src/lzma2/libxz.a`); `Makefile:13` (`EE_LDFLAGS = … -L$(EE_SRC_DIR)/lzma2`)
- **Problem:** There is no `src/lzma2/` directory and no `libxz.a` in the tree (`git ls-files` shows `src/lzma2.c` plus the `src/xz_*.c` sources, which the Makefile compiles directly as `lzma2.o xz_crc32.o xz_dec_lzma2.o xz_dec_stream.o`). Both references are leftovers from a layout where XZ was a prebuilt library; today they are inert but misleading — the `.gitignore` exception in particular invites a future contributor to commit a prebuilt `.a` there.
- **Fix:** Delete the two `.gitignore` exception lines and the `-L$(EE_SRC_DIR)/lzma2` flag.
- **Benefit:** Build/config describe the layout that actually exists.
- **Effort:** Trivial. **Risk:** Very low — the directory does not exist, so the `-L` path contributes nothing to linking today (verified by the file census, not by a local build). **Confidence:** READ.

### RE-SMS-144 — Gate `test-media.yml` regeneration on media-relevant paths
- **Category:** 7 (build/CI/clutter — CI cost)
- **Locations:** `.github/workflows/test-media.yml:12-15` — triggers on every `push` to `master` with no `paths:` filter, then runs `tools/gen-test-media.sh` (ffmpeg generation) and re-uploads `SMS-test-media.zip` to the `test-media` release
- **Problem:** The media pack depends only on `tools/gen-test-media.sh` and the format support it samples, but a docs-only or GUI-only push still pays a full ffmpeg generation + release upload cycle. Unlike `docs.yml`, which is path-filtered, this workflow has no filter at all.
- **Fix:** Add a `paths:` filter (`tools/gen-test-media.sh`, the workflow itself, and the converter/format sources the pack samples — or conservatively `tools/**` + `src/SMS_Container*`), keeping `workflow_dispatch` for manual regeneration.
- **Benefit:** Fewer pointless CI runs and release-asset churn on unrelated pushes.
- **Effort:** Trivial. **Risk:** Low — a too-narrow filter could skip a needed regeneration; `workflow_dispatch` remains as the manual escape hatch. **Confidence:** INFERRED (trigger behaviour is READ from the YAML; the cost saving is inferred, not measured from Actions logs).

---

## Checked-clean — verified, do not re-flag

Areas examined with the same rigour that produced findings, where the code is already correct. Recorded so a future pass does not re-derive them.

**Containers (EE).** MOV sample-table readers (`stts`/`stsc`/`stsz`/`stco`/`ctts`/`stss`): entry counts cross-checked against declared atom size via `_tbl_fits`, allocations tested before counts are published. MOV `esds` decoder-specific-info is `_tbl_fits`-bounded with a checked `calloc`. MPEG-PS `_fill_audio_parameters`: PES length range-checked, heap allocation tested; MPEG-PS stream creation caps count at `SMS_MAX_STREAMS - 1` and checks both allocations with cleanup. AC3 container: `AC3_SyncInfo` returns only 0 or a frame size ≥ 128, so the `lFrameSize - 8` read-back cannot underflow. AAC ADTS: frame length (13-bit, ≤ 8191) required > 7 before the `lSize - 8` read; rate/channel indexes masked to table sizes, zero entries rejected. ASF stream-header cap is enforced *before* allocating; `ds_span` deinterleave parameters validated at :284-289; ASF frame-header bounds (`packet_obj_size` ∈ (0, 2^24), fragment ≤ remaining) checked at :425-470 and :565-568. AVI `_Load_idx1` count is clamped to what the file's remaining bytes can hold before the loop. M3U `_open_file` length-checks every component against its 1024-byte stack buffer; playlist parsing reads into bounded buffers only. FLAC metadata walk: non-STREAMINFO blocks skipped by declared length, STREAMINFO required to be exactly 34 bytes with a checked read. OGG probe cleanup clears libogg/vorbis state on every exit path and re-seeks to 0. JPG probe tests its negative-pointer convention before dereferencing.

**Audio decoders (EE).** All 345 constant-table initialisers were extracted and counted by script: counted == declared everywhere except RE-SMS-124 (`s_SAB`) and one dead byte (FLAC CRC table `ctx[257]`, indexed only by a `uint8_t`, so entry 256 is unreachable — noted, not counted as a finding). AC3 grouped-quantizer zero tails are intentional invalid-code sinks sized exactly to their code widths; `s_Exp1/2/3` carry a deliberate `25,25,25` sentinel at 125–127 forcing the `anExp > 24` error path; `_ac3_parse_deltba` guards `j + lDeltLen >= 50`; coupling coordinates fit `m_CplCo[5][18]` exactly; `_ac3_spdif`'s memset is safe (AC3 frames ≤ 3,840 < 6,144-byte burst — contrast RE-SMS-011); `s_BndTab`/`s_LATab` monotonic (script). AAC scalefactor-band tables fully consistent (script): all 13 `swb_offset_*` monotonic, start 0, end exactly 1024/128, band widths divisible by 4, `num_swb[i]` == table length − 1 for all 12 indices in both windows; Huffman structures walk in-bounds (script): `hcb_sf` max offset 240 matching the runtime guard, binary trees hcb3/5/7/9 all targets in-bounds, two-step tables' first-stage maxima ≤ second-stage sizes for all 7 codebooks (hcb11 reaches exactly 373 of 374); SBR trees walk in-bounds; `section_data` checks precede the writes they protect; `decode_scale_factors` range-checks 0–255. DTS: all 62 Huffman tables terminate with the `{0,0,0}` entry `_dts_inverseq` relies on (script); `bitalloc_select[11][8]` NULL tails honoured by the caller's short-circuit; `adpcm_vb[4096]` vs 12-bit index and `high_freq_vq[1024]` vs 10-bit index are exact fits; bit-allocation values rejected above 26 against 32-entry quant tables. MP123: `MP123_Decode` runs `MP123_CheckHeader` before `MP123_DecodeHeader`; backstep ≤ 511 fits the 512-byte reservoir; `m_TabSelect` (5 bits) into `s_HuffTbl[32]` exact fit; max `LinBits` 13 (script) consistent with `s_ISPow[8207]`. WMA: all six coefficient VLCs satisfy `sum(levels[i]) + 2 == coef table size` exactly (script); table-driven exponent bands contain no zero entries in initialised data (script); `wma_init` clamps channel count to 1/2. FLAC frame CRC gate at `src/SMS_FLAC.c:363` is used consistently.

**Video (EE).** MSMPEG4 motion-vector and DC VLC tables are size-consistent (script): 1100/1099 counts match declarations and the `m_N + 1` init call; the 1100th code entry is the escape handled via `lCode == lpMV->m_N`. libmpeg motion slots provably ≤ 6 of 7 by call-site counting; `_init_seq` NULL-checks frame allocations and unwinds. The JPEG parser is already hardened (segment walking bounds-checked; SOF/DHT validated; DC/AC/run decoding bounded). `SMS_RescaleInit`/`SMS_VideoBufferInit`/`SMS_FrameBufferInit` reject oversized dimensions, multiply sizes in `unsigned long long`, and NULL-check. `IPU_InitLoadImage` caps width/height at 1024 with 16-alignment. `MSMPEG4_Init` NULL-checks `m_pCodedBlockBase` and routes to `allocation_error`. libmpeg's empty extension handlers are live dispatch targets that deliberately ignore unsupported blocks — intentional, not dead.

**Player/GUI/support (EE).** `src/SMS_Artwork.c` is bounded by design (stem/capacity validation, path-separator rejection, JPEG preflight dimension caps). `File_GetString` is truncation-aware and always NUL-terminates. `SMS_ConfigAssetPath` and `_follow_cfg_pointer` are length-gated and device-validated (the model for RE-SMS-063's fix style). Artwork loading in `SMS_GUIFileMenu.c:141-200` caps file size, checks allocation and full read, and preflights JPEG. `_append_directory_path` is capacity-checked against `SMS_CWD_CAPACITY`. SMB share listing force-NUL-terminates network-supplied names at [255] before any `strlen`/`strcpy`. `SIF_BindRPC` uses a bounded retry (2,000) with a documented hardware diagnosis. `File_Skip` terminates at EOF instead of hanging. `SMS_LoadPalette`/`SMS_LoadXLT` use fixed-size reads into fixed buffers.

**SMSTCPIP (IOP).** TCP output core allocation discipline is good: `tcp_enqueue` checks every `memp_malloc`/`pbuf_alloc` and unwinds via `tcp_segs_free`; `tcp_rst`/`tcp_output` ACK/`tcp_keepalive` NULL-check their allocations; `etharp_query` checks its ARP-request allocation. `tcpip_input` frees the pbuf and returns ERR_MEM when its message allocation fails; the thread loop frees every message after dispatch. `memp_init`'s 11th table entry (`MEMP_SYS_TIMEOUT`) is zero-filled but `sys_timeout` is a no-op macro in this port — no out-of-bounds read. Checksum assembly (`inet.s`, mechanism scan) walks pbufs by their own `len` fields, not by caller-supplied lengths. `udp_input`/`tcp_input` verify pseudo-header checksums before delivery and drop broadcasts/multicasts for TCP. ARP timer/queue ownership is sound (`etharp_tmr` frees on expiry). `mem_malloc`/`mem_realloc`/`plug_holes` maintain the block list correctly for the non-double-free case. `LWIP_STATS 0`, `LWIP_DHCP 0`, `IP_FORWARD 0` are consistently honoured. Linear scans (ARP table 10, socket table 15, PCB lists ≤ 15) are proportionate — no structural performance finding warranted.

**Other IOP modules.** smbman-sms is largely hardened already: `GetSMBServerReply` received-size checks (`smb.c:296-306`); `setStringField` capacity logic (`smb.c:313-337`); `smb_NetShareEnum` full offset/count validation (`smb.c:739-795`); `smb_FindFirstNext2` bounds validation (`smb.c:1540-1581`); `smb_ReadAndX` wire-length validation (`smb.c:1127-1151`); `sms_join_path` bounded with no silent truncation; `smb_dread` −EILSEQ skip-advance handling (`smb_fio.c:713-727`). RE-SMS-051/052 are the remainder, not the pattern. SMSUdpfs is SMS-hardened by design (in-code comments document the fixes): `_recv_with_result` zero-initialised header + result clamp (`udpfs_core.c:89-136`); UDPRDMA wire `hdr_size` clamp-and-drain (`udprdma.c:186-215`); RX mutex serialisation and discover-time RX-state reset (`udprdma.c:488-530`); bounded TX wait replacing an infinite loop (`smap/src/xfer.c:254-287`); `smap_register_rx_callback` header-size clamp (`xfer.c:94-99`); `HandleRxIntr` `hdr_len` clamp (`xfer.c:177-185`). ministack `udptty.c` is deliberately unbuilt, with the reason documented at `ministack/src/main.c:59-65` — intentional. SMSCDVD's `CDVD_Thread` RPC allocation is NULL-checked with a sleep fallback (`SMSCDVD.c:975-977`) and `_start` checks its `CreateThread` (`:1224-1234`) — RE-SMS-064's :1186 site is the exception. USBD `initHubDriver` checks its own allocation (`hub.c:44-48`). SMSUMS `_connect_thread` checks its 128 KiB cache allocation; `FATFS_Init`'s BPB whitelist (`ums_fat.c:1048-1061`) and fall-through free (`:1131`) exist — RE-SMS-020/130 are about the gaps around them. SMSSMB `_smb_file_info_dos2ps2`'s name copy is **not** a bug (`m_NameLen` is `unsigned char`, so the copy stays inside `io_dirent_t.name[256]`) — checked and cleared. SMSMAP RX is allocation-guarded (`smap.c:1014-1022`); the TX gather (RE-SMS-070) is the asymmetric case. SMSMAP and SMSUdpfs SMAP are *different* drivers (~1,691 diff lines) — not duplication. SMSDev9's PCMCIA tuple walk (`ps2dev9.c:613-663`) is end-checked.

**Data/tables/assets.** `g_DataBuffer`: all 17 segments' payloads equal their `include/SMS_Data.h` declared sizes and payload+padding equals the 16-byte-aligned size (script) — exact. All five JPEG VU blobs are valid XZ and decompress to exactly the raw sizes declared in `g_JPEGData[5]` (script decompression). The icon pipeline is reproducible: re-running `tools/bake_icons_rgba.py` and `tools/bake_loading_rgba.py` produces byte-identical output to the committed files; alpha ≤ 0x80 and transparent-RGB-zeroed invariants hold in all three RGBA blobs (script). `g_SMSound` partition is exact (no overlaps, uniform 17-byte gaps). Inverse-codepage tables themselves are flawless (4 × 128 Unicode entries strictly monotonic; 4 × 128 index entries each a permutation of 0..127 — script); only the search walk is wrong (RE-SMS-096). The `About` obfuscation round-trips: simulated `UnHash` decodes exactly 60 readable records. `lng/` files are uniformly shaped (19/19 at 269 lines / 268 non-empty, no duplicates; no untranslated-content claim is made — identical lines can be proper nouns/format strings). Theme duplicate PNGs (`s_IconCDROM` == `s_IconDVD`, `s_IconHost` == `s_IconSMB`) occupy *distinct* fixed-offset slots in the bake order — deliberate reuse the fixed-offset format cannot express more cheaply; not counted. `irx/` plain+`.xz` pairs are consistent (every `.xz` decompresses to its sibling `.irx` with equal md5, where a sibling exists). Vendored XZ core (`xz_dec_lzma2.c`, `xz_dec_stream.c`, `xz_crc32.c`): no local modifications, dead config, or licence issues found (mechanism scan); CRC32-only configuration matches what the in-tree producers emit.

**Headers/build/docs.** Include guards: 87 of 89 headers have matching guards; the only exceptions are RE-SMS-098's two. No duplicated declarations across the 649 parsed prototypes (apparent duplicates were parser artefacts, re-verified by grep). Apparent dead declarations that are alive: the `DSP_*`/`DMA_*`/`GS_*`/`IPU_*`/`_MPEG_*` families resolve to definitions in `.s`/`.S` assembly, to inline-asm definitions inside `.c` files (e.g. `SMS_SetExecPath` at `src/SMS_EE.c:742`), or to header-inline bodies (e.g. `GSFont_CharWidth` at `include/SMS_GS.h:1307`) — spot-verified, none flagged. Vendored XZ surface: `xz_crc64`/`xz_crc64_init` have no compiled definition in the default build, but this is the vendored configuration working as intended (both `tools/compress_irx.py` and `tools/bake_icons_rgba.py:86` document CRC32-only) — not flagged. Host tests are all wired up: all 59 `tools/test_*.py` are referenced in `.github/workflows/` (script) — RE-SMS-142 is about the manual mechanism, not a current gap. Plain `.irx` + `.irx.xz` pairs are intentional (compress_irx inputs/outputs). `vu/*.o` are provenance for the XZ dumps embedded in `src/SMS_JPEGData.c`, regenerated by `vu/build.sh`, explicitly excepted in `.gitignore` — not build outputs. `lng/` is a deliberate archive (`lng/README.txt` states the 269-entry packs are the original SMS 2.9 packs kept unchanged; live packs are `docs/translation/` with 311 entries). Docs generation is disciplined: `docs/` HTML/assets and `wiki/` are generated from `docs-site/` by `docs-site/build.py`, CI enforces freshness with `--check`, and `docs-site/static/` → `docs/` copies are byte-identical (21/21 MD5-checked). Doc claims spot-verified against code with **no contradiction found**: `SPISD_MAX_BATCH 8` is at `patches/mx4sio_bd-sms.patch:173-174` and the patch does add the claimed symbols; `MX4SIO_RD_CHUNK 4096` is at `src/SMS_FileContext.c:26`; the clean-boot network default is at `src/SMS_Config.c:604`; `BDM ?= 1` at `Makefile:3`; README's "311 strings" matches `docs/translation/SMS.lng` exactly (311 lines, counted). Two doc file:line citations have drifted (the doc's own header banner declares its references snapshots; the substance matches) — not counted as findings.

---

## Files NOT read

Explicit non-coverage, so the boundary of this audit is honest. Everything not listed here was read in full or verified by script as described in the findings and checked-clean notes.

- **EE assembly sources — structure/entry-point scan only, not line-audited:** `src/libmpeg_core.S` (2,324 lines; entry points, globals, DMA packet layout), `src/SMS_IPU_0.s` (108), `src/SMS_DSP_FFT.S`, `src/SMS_DSP_MMI.S`, `src/SMS_DSP_QPel.S`, `src/SMS_GS_0.S`, `src/SMS_RC_0.s`, `src/SMS_Timer.s`, `src/SMS_Utils.S`, `src/SMS_DMA_0.s`, `src/SMS_MP123Core.S`. `src/About_Data.s` is data: verified by exact simulation of its decoder (60 records), not line-read.
- **SMSTCPIP `src/inet.s`** (667 lines, MIPS asm checksums + `inet_aton`/`inet_ntoa`) — mechanism scan only (bounds/length-trust paths), not line-audited.
- **GS rendering internals:** the body of `include/SMS_GS.h` (1,314 lines) was composition-sampled (241 `#define`s, 13 struct typedefs, 1 header-inline function) but not line-audited; `src/SMS_GS_1.c` was not line-audited, and `src/SMS_GS_2.c` was read at the ranges cited in RE-SMS-053/095/096 (font size table, `GSFont_Get`, `GSFont_Load`) rather than end-to-end. (`src/SMS_GSFont.c` is pure data and was script-counted in full.)
- **IOP items structure-scanned or grep-verified only:** `iop/SMSUMS/include/ums.h` (via grep); USBD headers, exports, and ~888 lines of `iop/USBD/src/` beyond the files read in full; `iop/smbman-sms/src/smb.h` (895 lines, grepped for constants/structs), `iop/smbman-sms/src/auth.c`, `src/des.c`, `src/md4.c`, `src/sms_cp1251.h` (crypto/auth — structure only); `iop/SMSMAP/smap.c` init/PHY sections and `smap.h` (TX/RX paths read); per-directory IOP Makefiles, exports and imports files (~66–243 lines each) listed/counted only.
- **Vendored header bodies beyond guard/declaration checks:** `include/xz.h`, `include/xz_config.h` (config read; API body not line-audited), `include/lzma2.h`, `include/mbstring.h`.
- **Tools:** `tools/sms_converter/*.py` bodies, `tools/mpeg_color_probe/` and `tools/mpeg_motion_probe/` bodies, `tools/libsd/*.c` bodies — censused and reference-checked only. Other `tools/` files were read at cited lines (bake/compress/build scripts) rather than end-to-end.
- **Docs prose:** `docs-site/content/*.html` full text (roles established via `docs-site/build.py`, `docs-site/manifest.json`, `docs/README.md`); `docs/*.html` generated output text; `docs/src/**` DocBook XML bodies (censused as historical material); `CHANGELOG.md` beyond the head sections and a full heading census; `docs/investigations/**/*.md` bodies (censused by size/files only). `README.md`, `MX4SIO_SOLUTION.md`, `AGENTS.md`/`CLAUDE.md` were read in full.
- **Build files listed/counted only:** the 11 per-directory IOP Makefiles, `docs/Makefile` beyond its head, `utils/Loader/Makefile` body, `.clang-format`/`.editorconfig` (listed only), `.github/actions/setup-ffmpeg-windows/action.yml` beyond a structural skim.
- **Binary asset contents beyond hash/format/decompression checks:** font `.mtf` glyph data, theme/ship PNG and `.icn` pixel content, `ship/` files beyond census — verified by md5, byte-identical re-bakes, and format invariants, not by visual inspection.
- **Out of scope by design:** `src/SMS_Data.c`'s 16,728 lines of hex were verified by script (per-segment payload/padding census), not read as text; the PS2SDK and other external dependencies; any file under `~/workspace/Sable-audit/` (blind-redo rule — never read, listed, or grepped).

---

*End of redo audit. Worker section files: `~/workspace/Sable-audit-redo/redo-sections/redo-sms-W1.md` … `redo-sms-W8.md`. Nothing in this audit was executed on PS2 hardware or in an emulator; every claim is code-read (READ) or explicitly labelled INFERRED. No build was run.*
