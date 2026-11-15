# SMS Round 7 — Vendored Codecs II (DTS / MP123 / MPEG4 family / AC3 / WMA / FLAC / MPEG12 family / PCM / DXSB)

- Repo: `Simple-Media-System`, pin `e65ced39a27eee8d996e9257e21594d1cac1c93d` (verified at session start and again before writing; `git status` clean throughout — read-only audit, nothing edited).
- Scope: `src/SMS_DTS.c`, `SMS_MP123.c`, `SMS_MPEG4.c`, `SMS_MSMPEG4.c`, `SMS_H263.c`, `SMS_VLC.c`, `SMS_AC3.c`, `SMS_AC3_imdct.c`, `SMS_WMA.c`, `SMS_FLAC.c`, `SMS_MPEG12.c`, `SMS_MPEG.c`, `libmpeg.c`, `SMS_PCM.c`, `SMS_DXSB.c`.
- Categories: Cat 6 only (OOB read/write, unchecked size, divide by file-controlled value, integer overflow in size math) at decode entry points reachable with crafted media; Cat 1 only where grep-provable tree-wide dead. Prior rounds covered allocation surfaces only; this round goes inside decode arithmetic.
- Labels are candidates (`DTS-1…`), not numbered findings. **READ** = the defective code and the provenance chain were both read in source. **INFERRED** = code read, but reachability or the exact sink depends on a runtime/container condition not provable from these files alone.
- Nothing here is build- or hardware-verified.

## Systemic note (applies to every bitstream codec below)

`include/SMS_Bitio.h`: `SMS_GetBits`/`SMS_GetBit`/`SMS_UPDATE_CACHE` never check the bit index against `m_szInBits`; the cache refill unconditionally loads an unaligned 32-bit word at `idx >> 3`. Any decode loop that consumes bits without an independent termination condition therefore reads past the input buffer. This is counted as a candidate only where a concrete unbounded consumer was identified (DTS-5); elsewhere it is background risk, not double-counted.

---

## DTS — `src/SMS_DTS.c` — 8 candidates

### DTS-1 — `prim_channels` up to 8 written into state arrays sized 5 — READ
- `src/SMS_DTS.c:10756`: `state->prim_channels = bitstream_get(state, 3) + 1;` → 1–8 from the frame header.
- The state struct (`src/SMS_DTS.c:101-117`) sizes every per-channel array at `DTS_PRIM_CHANNELS_MAX` = 5 (`src/SMS_DTS.c:26`): `subband_activity[5]`, `vq_start_subband[5]`, `joint_intensity[5]`, `scalefactor_adj[5][…]`, `quant_index_huffman[5][…]`, `bitalloc[5][…]`, etc.
- Header parsing loops `for (i = 0; i < prim_channels; ++i)` immediately after `:10756` write those arrays with no clamp against 5. A header claiming 6–8 primary channels writes past each array into adjacent struct fields; later, `_dts_subsubframe` also indexes the stack array `subband_samples[DTS_PRIM_CHANNELS_MAX][8][32]` (`src/SMS_DTS.c:11168`) by channel.
- Guard: none found anywhere between the assignment and the loops.

### DTS-2 — Downmix mode table indexed by two unmasked 6-bit header values — READ
- `src/SMS_DTS.c:10261` (in `_dts_downmix_init`, starts `:10199`): `int output = table[ flags & DTS_CHANNEL_MASK ][ input ];` with `table` declared `[11][10]` (`:10203` approx., static const in the function) and `DTS_CHANNEL_MASK` = `0x3F` (`src/SMS_DTS.c:33`).
- Row index can be 0–63 against 11 rows; `input` is `state->amode`, a 6-bit header field (0–63), against 10 columns. Either dimension walks off the table; the returned `output` is only range-checked by the caller after this read.
- Secondary instance of the same pattern: `src/SMS_DTS.c:11299-11309` indexes `dts_channels[ state->output & DTS_CHANNEL_MASK ]` — a 16-entry table (`:145`) with a 0–63 index.
- Contrast: the AC3 sibling (`_ac3_downmix_init`) does guard this exact pattern — see AC3 checked-clean.

### DTS-3 — Cumulative Huffman scale-factor sum used as unchecked table index — READ chain / INFERRED range
- `src/SMS_DTS.c:11371-11393` (`dts_subframe_header`): `scale_sum = 0;` then per subband `scale_sum += _dts_inverseq(state, scales_129[…])`, and the sum indexes `scale_table[ scale_sum ]` at `:11382` and `:11393`.
- `scale_table` is `scale_factor_quant6[64]` unless `scalefactor_huffman == 6` selects `scale_factor_quant7[128]`. The direct-read paths beside it are safe (`bitstream_get(6)` → quant6, `bitstream_get(7)` → quant7 at `:11379-11392`), but the cumulative path has no clamp: `_dts_inverseq` returns signed `(char)` Huffman values (tables at `:195-375` span roughly −64…+64), accumulated over up to 32 subbands, so the sum can leave [0, 64) in either direction → OOB read of the scale table.
- INFERRED only as to how far a real crafted stream can push the sum; the absent bounds check is READ.

### DTS-4 — LFE scale factor: 8-bit index into a 128-entry table — READ
- `src/SMS_DTS.c:11440`: `scale_factor_quant7[ bitstream_get(state, 8) ]` — index 0–255, table size 128 → OOB read for values 128–255. No mask or check.

### DTS-5 — DTS bit reader has no end pointer; decode loops can consume past the frame — READ
- `src/SMS_DTS.c:10009` (`bitstream_fill_current`): `unsigned int lTmp = *state->buffer_start++;` — unconditional dereference/advance. `dts_state_t` carries no end pointer at all.
- Subframe (≤16) / subsubframe (≤4) / channel / subband (≤32) loops all draw from this reader, so a truncated or inconsistent frame walks the pointer past the end of the frame buffer held in `m_InBuf[24576]`.
- The frame copy itself is bounded (syncinfo `frame_size` max 16384, ≤ ~18724 bytes after 14-bit expansion < 24576) — the defect is purely on the consumption side.

### DTS-6 — Channel-count function-pointer table indexed up to 5 into a 2-entry table — READ code / INFERRED reachability
- `src/SMS_DTS.c:11565`: `s_FloatToInt[ anChannels - 1 ]`, table of 2 (`:11556`).
- `anChannels` arrives via `DTS_Decode` from `apCtx->m_Channels`; when the container leaves it 0 it is filled from `DTS_Channels(flags)` (`:11493`), which returns 2/4/5/6 depending on the header `amode`/flags — index up to 5 → OOB function-pointer read followed by an indirect call through it.
- INFERRED: if the container always pre-sets channels to 1 or 2, the sink is not reached. Same defect pattern as AC3-1.

### DTS-7 — Joint-intensity source channel up to 6 into a 5-entry array — READ
- `src/SMS_DTS.c:11405-11407`: `source_channel = state->joint_intensity[j] - 1;` then `state->subband_activity[ source_channel ]` bounds the subband loop.
- `joint_intensity` is a 3-bit header field (`:10771`, values 0–7) stored per channel in `joint_intensity[5]`; values 6/7 give `source_channel` 5/6 → OOB read of `subband_activity[5]`, and the read value then controls a write loop bound.

### DTS-8 — `bitalloc_huffman == 7` selects a Huffman table past the end of `bitalloc_12[5]` — READ
- `bitalloc_huffman[j]` is a 3-bit header field (0–7). In `dts_subframe_header` (`src/SMS_DTS.c:11340-11345`): value 6 → direct 5-bit read, value 5 → direct 4-bit read, **all other values** (including 7) → `_dts_inverseq(state, bitalloc_12[ state->bitalloc_huffman[j] ])`.
- `bitalloc_12` has 5 entries (`:189`); index 7 reads a wild pointer past the table, which `_dts_inverseq` then dereferences as a Huffman table.

---

## MP123 — `src/SMS_MP123.c` — 2 candidates

### MP3-1 — Region-2 start index can read past `m_LongIdx[23]` — READ (low severity, intra-struct)
- `src/SMS_MP123.c:5462-5463` (`_mp3_get_side_info`): `lR0C = GetBits(4)+1` (1–16), `lR1C = GetBits(3)+1` (1–8); `m_Reg2Start = s_BandInfo[aSFreq].m_LongIdx[ lR0C + lR1C ] >> 1`.
- Sum range 2–24 against `m_LongIdx[23]`; indices 23/24 read into the following `m_LongDiff` field of the same static struct, so the read stays inside the allocation but returns difference-table values as a region start, which then bounds dequantize loops. No clamp on the sum.

### MP3-2 — Gain table base + scalefactor index can exceed `s_GainPow2[378]` on the LSF (MPEG-2/2.5) path — READ reasoning / INFERRED occurrence
- `src/SMS_MP123.c:5422`: `m_pPow2Gain = (float*)s_GainPow2 + 256 - GetBits(8)` → base offset 1–256; per-granule `m_pFullGain[w] = m_pPow2Gain + (GetBits(3) << 3)` adds up to 56 → base up to 312.
- Dequantize indexes the table with `(scalefactor << lShift)`, `lShift = 1 + m_ScaleFacScale` ≤ 2.
- On the `_mp3_get_scale_factors_2` path, the packed length fields in `s_iSLen2[256]`/`s_nSLen2[512]` (`:353`/`:287`) reach 5 bits → scalefactor up to 31 → index up to 312 + (31 << 2) = **436 > 378** (`s_GainPow2` at `:190`) → OOB read into following `.data`.
- The MPEG-1 `_1` path does **not** overflow (max length 4 → 15 << 2 = 60 → 372 ≤ 378); the asymmetry between the two paths is the tell. INFERRED occurrence: requires an LSF stream whose side info simultaneously drives the gain base high and a 5-bit-length scalefactor band to 31.

---

## MPEG4 / MSMPEG4 / H263 — `src/SMS_MPEG4.c`, `src/SMS_MSMPEG4.c`, `src/SMS_MPEG.c` — 4 candidates (label MP4)

### MP4-1 — Sprite trajectory: up to 63 warp points written into a stack `int[4][2]` — READ
- `src/SMS_MPEG4.c:933` (`_decode_vol_header`): `g_MPEGCtx.m_nSpriteWarpPts = SMS_GetBits(apBitCtx, 6);` → 0–63, no clamp.
- `src/SMS_MPEG4.c:1183-1201` (`_mpeg4_decode_sprite_trajectory`): `for (i = 0; i < g_MPEGCtx.m_nSpriteWarpPts; ++i) { … lD[i][0] = lX; lD[i][1] = lY; }` where `lD` is a stack `int[4][2]`. The `switch` at `:1236` only handles 0–3 points, but it runs **after** the write loop.
- Reached from the VOP header for S-type (static/GMC sprite) pictures and from the video-packet header path. Stack OOB write of up to 59 extra point pairs.

### MP4-2 — MSMPEG4 slice height can be 0 → modulo-by-zero in the decode loop — READ
- `src/SMS_MSMPEG4.c:2151` (`_msmpeg4_decode_picture_header`): `g_MPEGCtx.m_SliceHeight = g_MPEGCtx.m_MBH / (lCode - 0x16);` — `lCode` is 5 bits, values < 0x17 are rejected, so the divisor is 1–9, but the quotient is 0 whenever `m_MBH < divisor` (e.g. MBH 8 from a small container height, `lCode` 31).
- The first slice returns harmlessly (its resync test `ResyncMBY + SliceHeight == MBY` holds at MBY 0); the next slice evaluation at `src/SMS_MSMPEG4.c:2332` computes `g_MPEGCtx.m_MBY % g_MPEGCtx.m_SliceHeight` → integer divide-by-zero trap.

### MP4-3 — `SMS_BUG_EDGE` widens motion-compensation clipping from macroblock units to pixel units — READ
- `src/SMS_MPEG4.c:3324` and `:3329`: `SMS_BUG_EDGE` is set from file-controlled encoder-version data (XviD build ≤ 12, DivX version < 500) parsed from the stream.
- `src/SMS_MPEG4.c:2733-2736` (end of every VOP header): under `SMS_BUG_EDGE`, `g_MPEGCtx.m_HEdgePos = g_MPEGCtx.m_Width; g_MPEGCtx.m_VEdgePos = g_MPEGCtx.m_Height;` — pixel counts. The correct values, set at context init (`src/SMS_MPEGInit.c:33-34`), are `m_MBW - 1` / `m_MBH - 1` — macroblock counts.
- The clipping macros `CLIP_MV`/`CLIP_MVC` (`src/SMS_MPEG.c:442-470`) clamp motion-compensation MB coordinates to `m_HEdgePos`/`m_VEdgePos`; `_mpeg_motion`, `_qpel_motion` and the 8×8 path then form `lpMB = apRefPic + lMBX + lMBY * m_LineSize`. With pixel-unit bounds (e.g. width 720 vs MBW 45), a crafted motion vector addresses macroblocks far beyond the reference picture's `(MBW+2)×(MBH+2)` allocation → OOB read in every MC function.
- Aggravator (INFERRED): `_decode_vol_header` (`src/SMS_MPEG4.c` ~`:912-917`) also assigns `m_Width`/`m_Height` from 13-bit VOL fields without recomputing the MB geometry or reallocating (geometry is computed once in `SMS_MPEGContext_Init`, `src/SMS_MPEGInit.c:27-31`, from container dimensions), so the pixel values used as clip bounds need not even correspond to the allocated geometry.

### MP4-4 — Interlaced direct-MV field-time divisor can be 0 — INFERRED
- `src/SMS_MPEG4.c:1582-1588` (`_mpeg4_set_direct_mv`, interlaced branch): `lTimePP = g_MPEGCtx.m_PPFieldTime ± lFieldSelect ± i;` and the following lines divide motion values by `lTimePP`.
- The non-interlaced divisor **is** guarded: B-VOP setup (`:2602-2607`) skips the frame when `m_PPTime <= 0` or `m_PPTime <= m_PBTime`, and `m_TFrame` is forced ≥ 1 before the field times are derived (`:2610-2617`). But `m_PPFieldTime` itself can be 0 (the `SMS_ROUNDED_DIV` difference can cancel), and the ±1 field adjustments can then produce exactly 0 → integer divide-by-zero. INFERRED: no concrete field-time combination was executed; the unguarded arithmetic is READ.

---

## AC3 — `src/SMS_AC3.c` — 1 candidate

### AC3-1 — Channel-count function-pointer table indexed up to 5 into a 2-entry table — READ code / INFERRED reachability
- `src/SMS_AC3.c:2585`: `s_FloatToInt[ anChannels - 1 ]`, table of 2 (`:2576`).
- `AC3_Decode` (`:2669`) sets `apCtx->m_Channels` from `g_AC3Channels[ m_Flags & 7 ]` (table `:38` = {2,1,2,3,3,4,4,5}) plus 1 for LFE → up to 6 channels; `_ac3_decode` then indexes with up to 5 → OOB function-pointer read + indirect call.
- A perfectly valid 5.1 stream is the trigger, unless the container pre-sets the channel count to 1/2 (in which case the value is lowered, not raised). Same defect pattern as DTS-6.

Checked-clean highlights for AC3 are below — notably `_ac3_downmix_init` **does** guard the pattern that DTS-2 gets wrong.

---

## WMA — `src/SMS_WMA.c` — 3 candidates

### WMA-1 — Exponent VLC band length `n == 0` → ~2³²-iteration write loop — READ code / INFERRED trigger combination
- `src/SMS_WMA.c:1635-1660` (`decode_exp_vlc`): `band_ptr = lpCtx->exponent_bands[ frame_len_bits - block_len_bits ];` then `n = *ptr++; do { *q++ = v; } while ( --n );` — `q_end` is only tested at the loop head (`:1651`), never inside the fill loop, so `n == 0` writes through the whole 32-bit countdown starting at `exponents[ch]`.
- Trigger chain: WMA v1 init fills only `exponent_bands[0]` (`:1346-1359` region), while decode indexes the row by `bsize > 0` whenever variable block length is enabled by extradata `flags2 & 0x0004` (read in `wma_decode_init`, ~`:1530`); `use_exp_vlc = flags2 & 0x0001`. Uninitialized/zero band rows supply the `n == 0`.

### WMA-2 — Superframe `bit_offset` never bounded before re-basing the bit reader — READ
- `src/SMS_WMA.c:2059`: `bit_offset = SMS_GetBits(lpBitCtx, lpCtx->byte_offset_bits + 3);` — width is file-influenced via bit rate; value not checked against the packet.
- `src/SMS_WMA.c:2073-2074`: `pos = bit_offset + 4 + 4 + byte_offset_bits + 3; SMS_InitGetBits(lpBitCtx, lpBuf + (pos >> 3), (MAX_CODED_SUPERFRAME_SIZE - (pos >> 3)) * 8);` with `MAX_CODED_SUPERFRAME_SIZE` 16384 (`:39`). If `pos >> 3 > 16384`, the size expression goes negative and is converted to a huge `uint32_t` bit count, with the bitstream pointer already past the packet buffer (`lFrmSize = lpCtx->block_align`, container-controlled). The `len` check at `:2083` guards a different `memcpy`, not this re-base.

### WMA-3 — `sample_rate == 0` → integer divide-by-zero during init — READ
- `src/SMS_WMA.c:1244`: `lpCtx->sample_rate = apCtx->m_SampleRate;` — container/ASF-supplied, no zero check anywhere in `wma_init`.
- Integer divisions by it: `:1352` `pos = (…)/b` with `b = sample_rate` (v1 branch), `:1383` `pos = (…)/(4*b)` (v2 branch), `:1397` `high_band_start = (…)/lpCtx->sample_rate`. The float `bps` division at `:1291` does not trap and is not part of this candidate.

---

## FLAC — `src/SMS_FLAC.c` — 2 candidates

### FLAC-1 — Fixed-predictor warm-up reads `decoded[-1…-4]` for predictor orders 0–3 — READ
- `src/SMS_FLAC.c:180-183` (`decode_subframe_fixed`): before the order switch, the code unconditionally computes `a = decoded[pred_order-1]`, `b = a - decoded[pred_order-2]`, `c = … decoded[pred_order-3]`, `d = … decoded[pred_order-4]`.
- Subframe type 8–12 selects predictor order 0–4 (`_decode_subframe`, `:285` region). For order 0 the reads are `decoded[-1]`, `[-2]`, `[-3]`, `[-4]`; orders 1–3 read progressively fewer negatives. For channel 0 that is before the heap allocation; for channel 1 it reads the tail of channel 0's buffer. The computed values are unused for orders 0/1, but the reads happen regardless.

### FLAC-2 — Wasted-bits count has no lower bound → `m_CurBPS` goes negative into the bit macros — READ code / INFERRED sink
- `src/SMS_FLAC.c:276-278` (`_decode_subframe`): `wasted = 1; while ( !SMS_GetBit(apBitCtx) ) ++wasted; apCtx->m_CurBPS -= wasted;` — a run of zero bits in the subframe header (file-controlled, and the bit reader has no end check) makes `m_CurBPS` arbitrarily negative; there is no `m_CurBPS < 0` rejection (upstream FLAC rejects this case).
- `m_CurBPS` is then passed as the bit count to `SMS_GetSBits`/`SMS_GetBits` (constant and verbatim subframes, `:282-284` region), where it becomes a huge `uint32_t` count in the `SMS_NEG_USR32` shift macros (`include/SMS.h:68`) and in the bit-index advance — corrupting the bit position and driving subsequent reads out of the frame buffer. INFERRED sink mechanics (MIPS shift masking makes the first step garbage rather than a trap); the missing bound is READ.

---

## MPEG12 family — `src/SMS_MPEG12.c`, `src/libmpeg.c`, `src/SMS_MPEG.c` — 0 candidates

No Cat 6 candidate survived checking in this family (see checked-clean). `SMS_MPEG.c` motion compensation is shared with MPEG4 and is the sink for MP4-3, filed there.

## PCM — `src/SMS_PCM.c` — 0 candidates

`PCM_Decode` (`:51`) splits the packet into 4096-byte parts plus a remainder and copies exactly the allocated lengths (part size + 80 header slack). No file-controlled size math.

## DXSB — `src/SMS_DXSB.c` — 1 candidate

### DXSB-1 — Header width drives RLE writes into fixed scratchpad past its end; packet size never validated against the header — READ
- `src/SMS_DXSB.c:199-205` (`_decode`): `lW`/`lH` are the header's `uint16` width/height, used with no sanity bound; `_decode` never checks `aSize >= sizeof(_DXSB_Header)` before the header (including the 27-byte PTS field consumed by `_get_pts`, `:215`) is read from the packet.
- `src/SMS_DXSB.c:274`: `mips_memset( lpTmp + lX, (lVal & 3) + 3, lLen );` with `lpTmp = SMS_SPR_FREE` (fixed scratchpad address `0x70001B30`, `include/SMS.h`) and `lX` ranging over the header width. The scratchpad free region from `0x1B30` to the 16 KB scratchpad end is ~9.4 KB; any subtitle width above that writes RLE runs past the scratchpad, and `dxsb_pack` (`:281`) likewise reads `lRW` bytes from `lpTmp`. Widths are 16-bit, so up to ~64 KB of overrun is expressible.

---

## Candidate count

| Codec | Candidates |
|---|---|
| DTS | 8 (DTS-1…DTS-8) |
| MP123 | 2 (MP3-1, MP3-2) |
| MPEG4/MSMPEG4/H263 | 4 (MP4-1…MP4-4) |
| AC3 | 1 (AC3-1) |
| WMA | 3 (WMA-1…WMA-3) |
| FLAC | 2 (FLAC-1, FLAC-2) |
| MPEG12 family | 0 |
| PCM | 0 |
| DXSB | 1 (DXSB-1) |
| **Total** | **21** |

Cat 1 (tree-wide dead): none identified in scope that is grep-provable; not forced.

## Checked-clean / dissolved (with reasoning)

**DTS** — `bitalloc_select[abits][sel]` in `_dts_subsubframe` (~`:11182`): short-circuited by `abits >= 11 ||`, and `abits > 26` is rejected in the subframe header. `prediction_vq` is 12 bits into `adpcm_vb[4096]`; `high_freq_vq` 10 bits into `[1024]`; LFE data index math fits its 48-float array; `adj_table` indexed by 2 bits fits `[4]`.

**MP123** — `s_ISPow`: `lX = 15 + (mask >> (32 - linbits))` with `linbits` ≤ 13 (from `s_HuffTbl`, `:1090-1123`) → max index 8206 fits `s_ISPow[8207]` (`:1135`) exactly. Header divisions are safe because `MP123_CheckHeader` (`:4790`) rejects samplerate-index 3, bitrate 0xF and reserved layer, and `g_mpa_freq_tab` entries are nonzero. `_seek_to_main_data` (`:5369`): backstep ≤ 511 (8/9-bit field) stays inside the 512-byte backstep area of `m_InBuf[2][2304]`. MP1 `s_Muls[lN+1][scaleidx]` (4/6-bit indices) fits `[27][64]`; MP2 `sl_SCFSI[64]` writes ≤ 60 into a caller `lScale[192]` advancing ≤ 180; `_mp3_hybrid` band count ≤ 32 from `s_LongLimit`/`s_ShortLimit`, pair access `lSB+1 ≤ 32` fits `[32][18]`; `m_TabSelect` 5 bits fits `s_HuffTbl[32]`, `m_Count1TblSel` fits `[2]`.

**MPEG4/MSMPEG4/H263/VLC** — QScale → `m_pRLVLC[QScale]` (`SMS_MPEG4.c:1861/1863`, array `[32]`) **dissolved**: `_mpeg4_decode_slice` (`:3137`) calls `SMS_MPEG_SetQScale(g_MPEGCtx.m_QScale)` before any MB decode, and that routine (`src/SMS_MPEG.c:61-93`) clamps to 1–31 and stores the clamped value back; partitioned and video-packet paths also pass through it. `_mpeg4_decode_block` coefficient index: `i > 62` → `i -= 192; if (i & ~63) return -1;` guards the `apBlock[lpScanTbl[i]]` write. Direct-MV `lTimePP` (non-interlaced) dissolved — guarded at `SMS_MPEG4.c:2602-2607` (see MP4-4 for the interlaced remainder). MSMPEG4 `lLevel / lQMul` division safe (intra `lQMul = 1`; inter QScale checked nonzero in the picture header). `SMS_RL_Init`/VLC construction in `SMS_VLC.c` and the MSMPEG4 init tables operate on static data at init, not file data. H263 motion prediction/update (`SMS_H263.c`) indexes MV tables only by the current MB position under the slice loops. (Noted, not a defect: `SMS_LAST_SKIP_BITS(re, &s->gb, 1)` in MSMPEG4 `decode_block` — the macro discards its `gb` argument, so the undefined `s` is never evaluated.)

**AC3** — `_ac3_downmix_init` (`:416`, `:434-436`) **is** guarded: `lOutput = aFlags & AC3_CHANNEL_MASK; if (lOutput > AC3_DOLBY) return -1;`, column `anInput & 7` — the check DTS-2 lacks. `AC3_SyncInfo` (`:374`) rejects `apBuf[5] >= 0x60` (so `>>3 ≤ 11` fits `s_HalfRate[12]`) and `FrmSizeCod >= 38` (fits `s_Rate[19]`), which also covers `_ac3_frame`'s `s_HalfRate[apBuf[5]>>3]`. `m_ACMmod` stays 0–7 (assigned `:562` from `apBuf[6]>>5` before the local may become `AC3_DOLBY`), so `s_nFChansTbl[11]` in `_ac3_block` (`:1989`) is safe. `s_HtHTab[m_FSCod]` (`_ac3_bit_allocate` ~`:1030`, table `[3]`) is safe because SyncInfo returns 0 for `fscod == 3`. `m_CplCo[5][18]`: `nCplBnd` max = 15+3−0 = 18, fits. `_ac3_parse_exponents` (`:628`) checks `anExp > 24` after every addition; `_ac3_parse_deltba` (`:705`) returns 1 if `j + lDeltLen >= 50`. `SMS_AC3_imdct.c` is fixed-size 512/256 transforms over fixed tables and caller buffers — no file-controlled sizes anywhere.

**WMA** — Channels are clamped in `wma_init` (`:1255`: any value ≠ 1 becomes `MAX_CHANNELS` = 2, with an explanatory comment). The coefficient loop breaks on `ptr >= eptr` before writing. `nb_frames = GetBits(4) - 1 = -1` makes its loop a no-op rather than a runaway. `byte_offset_bits` with bit_rate 0 → `SMS_log2(0)` yields −1, +2 = 1: no trap.

**FLAC** — Residual partitioning dissolves on inspection: per-partition `samples = m_BlockSize >> rice_order`, first partition starts at `pred_order`, total writes = partitions × samples − pred_order ≤ `m_BlockSize`, and `m_BlockSize ≤ m_MaxBlockSz` is enforced in `_decode_frame` before any subframe decode; `m_pDec` is allocated at `m_MaxBlockSz` per channel. LPC: `coeff_prec == 16` rejected, `qlevel < 0` rejected, `pred_order` ≤ 32 fits the stack `coeffs[]`. `_flac_decode`'s bit-buffer `memcpy` (`:497` region) is bounded by `min(lSize, m_MaxFrameSz − m_BSize)`; `lBlockLen = 4096 >> nChannels` is safe because the container probe (`src/SMS_ContainerFLAC.c:114`) requires `m_nChannels < 3`, which also bounds `SynthSamples[m_Decor]` (0–3 into `[4]`) and `_pcm_synth_ind`'s `[nChannels − 1]` into `[2]`. `decode_residuals` returns 0 when `pred_order > samples`.

**MPEG12 family** — `_mpeg12_slice` checks `lMBA >= aMBAMax` before every `_mpeg2_mc` call, and `lMBA` is built from the slice start-code row × `m_MBWidth` + MBA increment. `_mpeg12_get_ref` clamps the reference MB coordinates in inline asm (`pmaxw`/`pminw` against `m_MBWidth`/`m_MBHeight − 1`). Motion slots: max 6 refs per MB (frame DMV forward 4 + backward 2) fits `m_Motion[7]` (`include/libmpeg_internal.h:118`), leaving the zero terminator `_mpeg12_do_next_mc` scans for. Sequence-header frame-rate codes 0 and 9–15 select `s_FrameRate` = 0.0 (`src/libmpeg.c:43-47`) → float division yields +inf, **not** an integer trap; the result only feeds `m_MSPerFrame`/PTS arithmetic in `SMS_MPEG12.c`. `_init_seq` allocation failure is checked and propagates to `MPEG12_Decode`'s hardware-error path. `SMS_MPEG12.c` itself only shuttles frames/PTS. `SMS_MPEG.c` block/DMA paths are bounded by the picture geometry computed in `SMS_MPEGContext_Init`.

**PCM** — see above; nothing to dissolve, the file is size-clean by construction.

**DXSB (remainder)** — The RLE loop terminates: every iteration consumes ≥ 1 nibble and `lIt` is bounded by `aSize << 1`; `lLen` is clamped to the remaining row width `lDiff` (`:272` region). The packed-destination stride arithmetic (`lpDst += lRW` per row pair over two fields) sums to ≈ the allocated `lSize`; not provably over. Only the scratchpad/header-validation issue (DXSB-1) stands.

## Lines-read ledger (honest ranges; "spot" = targeted reads/grep, not line-by-line)

| File | Lines | Read |
|---|---|---|
| `src/SMS_DTS.c` | 11,801 | 1–130; 154–410 spot (tables); 1081–1210; 9950–11801 (all code). Tables ~410–9950 grep/spot only. |
| `src/SMS_MP123.c` | 6,729 | 1–190; 287, 353 spot; 825–1140 partial; 3189–3300 partial; 4746–6729 (all code). Tables 190–4745 not line-read. |
| `src/SMS_MPEG4.c` | 3,443 | 488–770; 825–1463; 1463–1757; 1757–2250; 2250–2554; 2554–2800; 3130–3443. 2785–3130 grep-only. 1–488 tables not line-read. |
| `src/SMS_MSMPEG4.c` | 2,394 | 1397–1560 spot (init); 1650–2394 (all code). 1–1396 static VLC tables not line-read. |
| `src/SMS_H263.c` | 230 | Full. |
| `src/SMS_VLC.c` | 324 | Full. |
| `src/SMS_AC3.c` | 2,772 | 340–760; 761–1210; 1972–2772. 1210–1972 function inventory/partial. 1–340 tables spot. |
| `src/SMS_AC3_imdct.c` | 642 | Function inventory + 1–120 spot; fixed-size transforms (structure scan, not full line read). |
| `src/SMS_WMA.c` | 2,112 | 1127–2112 (all code). 1–1127 tables not line-read. |
| `src/SMS_FLAC.c` | 594 | Full. |
| `src/SMS_MPEG12.c` | 258 | Full. |
| `src/SMS_MPEG.c` | 1,233 | 1–1000; 1000–1233 function inventory + partial. |
| `src/libmpeg.c` | 1,385 | 120–1385 (all code); 1–120 function inventory/spot. |
| `src/SMS_PCM.c` | 95 | Full. |
| `src/SMS_DXSB.c` | 330 | Full. |

Supporting files read for provenance: `include/SMS_Bitio.h` (full), `include/SMS.h` (spot: shift macros, SPR map), `src/SMS_MPEGInit.c` (full, 130 lines), `include/SMS_MP123.h`, `include/SMS_AC3.h`, `include/SMS_VLC.h`, `include/libmpeg_internal.h` (spot), `src/SMS_ContainerFLAC.c` (spot: channel probe).
