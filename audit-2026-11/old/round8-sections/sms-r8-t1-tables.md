# SMS Round 8 — Task 1: Constant tables (declared sizes, duplication, invariants)

Repo: Simple-Media-System @ e65ced39a27eee8d996e9257e21594d1cac1c93d (verified; `git status` clean before and after — read-only throughout).
Spans audited: SMS_OGG.c:226–2542, SMS_AAC.c:587–2005 + 2106–3582, SMS_DTS.c:410–9950, SMS_MP123.c:190–4745, SMS_MPEG4.c:1–488, SMS_MSMPEG4.c:1–1396, SMS_WMA.c:1–1127, SMS_AC3.c:1–340 + 1210–1972.
Method: script enumeration/counting of every initializer table in the spans (two independent counters for the disputed AAC tables), float-reinterpretation of `unsigned int` tables that store IEEE-754 bit patterns, VLC `(code, bits)` validity (`code < 2^bits`), monotonic/range/symmetry tests per table family, cross-file duplicate diffing over the whole `src/` tree, and line reads of the primary consumer for each major table. Existing SMS-001…157 were not repeated; overlaps are flagged explicitly where they occur.

**Headline result: 313 tables enumerated; every declared-size table in the spans matches its actual element count except the cases below. Three candidates.**

---

## CAND-T1-1 / DTS scale-factor quantizer tables end in zeros: `scale_factor_quant6[63] = 0` and `scale_factor_quant7[125..127] = 0` break a strictly increasing 63/125-entry run — the largest scale factors decode as silence / Category 6

**Locations:** `src/SMS_DTS.c:1108-1125` (`scale_factor_quant6[64]`), `src/SMS_DTS.c:1127-1160` (`scale_factor_quant7[128]`); consumers `src/SMS_DTS.c:11369-11370` (table select), `:11383`/`:11397` (`scale_table[scale_sum]`), `:11440` (LFE).

**Problem:** Script-verified structure (READ): `scale_factor_quant6` is strictly increasing over indices 0–62 (1 … 8,317,638 = 0x7EEAC6) and then index 63 is `0`. `scale_factor_quant7` is strictly increasing over indices 0–124 (1 … 8,317,638, the same maximum) and then indices 125, 126, 127 are `0, 0, 0`. A quantizer table whose last entries collapse to 0 has no legitimate reading: every other entry in both tables is a positive, monotonically increasing step size. The zero indices are directly reachable from the bitstream, not just via the cumulative-sum path already covered by SMS-138:
- Mode 5 (`scalefactor_huffman == 5`): `scale_sum = bitstream_get(state, 6)` (SMS_DTS.c:11380) → index 63 is a legal 6-bit value → `scale_factor[j][k][0] = scale_table[63] = 0`.
- Mode 6: `scale_sum = bitstream_get(state, 7)` (SMS_DTS.c:11382) → indices 125–127 are legal 7-bit values → scale factor 0.
- LFE: `lfe_scale_factor = scale_factor_quant7[bitstream_get(state, 8)]` (SMS_DTS.c:11440) → indices 125–127 are in-bounds but give `lfe_scale = 0.035F * 0 = 0`, silencing the LFE channel for the three largest LFE scale-factor codes. (The 8-bit index overrunning the 128-entry table at indices 128–255 is SMS-139, already filed — this candidate is the separate in-bounds wrong-value defect at 125–127.)

Effect: a subband (or the LFE channel) whose scale factor should be the *largest* in the table is instead multiplied by 0 — silent subband/LFE on otherwise valid streams that use the top scale-factor codes. This is distinct from SMS-138 (cumulative Huffman sum used as an unchecked index — a bounds defect) and SMS-139 (8-bit LFE index — a bounds defect): here the indices are in-bounds and the *stored values* are wrong.

**Fix (in words):** Restore the missing tail values of both tables from the DTS reference tables (the entries after 8,317,638 in each table's step sequence); do not invent values locally — the correct continuation must come from the spec/upstream table, which is why the replacement values are INFERRED (see Confidence). As a guard, the strictly-increasing property of both tables can be asserted by a host-side table test.

**Benefit:** Correct decoding of the loudest scale-factor steps instead of silent subbands/LFE; removes a wrong-constant that no bounds fix (SMS-138/139) would catch.

**Effort:** S (four values across two tables, once the reference values are in hand). **Risk:** Low — changes only entries that currently produce 0 (silence); no valid decode today depends on them being 0. **Confidence:** READ for the zero tails, the strictly-increasing structure of the preceding 63/125 entries, and the direct bitstream reachability of indices 63 and 125–127 (all script-counted and line-read in this pass). INFERRED for what the correct replacement values are — upstream/spec comparison is explicitly required and was not available in-repo; no in-repo copy of either table exists to diff against (whole-tree duplicate scan found no second copy).

---

## CAND-T1-2 / AAC two-step Huffman size arrays disagree with the actual second-stage table counts in both directions — 5 of 7 non-trivial entries are wrong, so the bound the decoders check against is not the bound that exists / Category 6

**Locations:** size arrays `src/SMS_AAC.c:2307-2309` (`hcb_2_quad_table_size`), `:2355-2357` (`hcb_bin_table_size`), `:2746-2748` (`hcb_2_pair_table_size`); second-stage tables `:2197-2227` (`hcb1_2`), `:2229-2252` (`hcb2_2`), `:2254-2301` (`hcb4_2`), `:2530-2563` (`hcb6_2`), `:2565-2587` (`hcb8_2`), `:2589-2643` (`hcb10_2`), `:2645-2740` (`hcb11_2`); consumers `src/SMS_AAC.c:6467`, `:6481`, `:6513`, `:6534` (`if (offset > hcb_*_table_size[cb]) return 10;`).

**Problem:** Counts were verified by two independent script methods (initializer-element counter and per-entry brace counter; both agree). Declared size vs actual entry count:

| Family | Codebook | Declared size | Actual entries | Delta |
|---|---|---|---|---|
| quad | 1 (`hcb1_2`) | 114 | 113 | +1 |
| quad | 2 (`hcb2_2`) | 86 | 85 | +1 |
| quad | 4 (`hcb4_2`) | 185 | 184 | +1 |
| pair | 6 (`hcb6_2`) | 126 | 125 | +1 |
| pair | 8 (`hcb8_2`) | 83 | 83 | 0 |
| pair | 10 (`hcb10_2`) | 210 | 209 | +1 |
| pair | 11 (`hcb11_2`) | 373 | 374 | −1 |
| binary | 3 (`hcb3`) | 161 | 161 | 0 |
| binary | 5 (`hcb5`) | 161 | 161 | 0 |
| binary | 7 (`hcb7`) | 127 | 127 | 0 |
| binary | 9 (`hcb9`) | 337 | 337 | 0 |

The tables themselves are internally consistent — script-verified (READ): for every codebook, `max(first_stage.offset + 2^extra_bits − 1)` over `hcb*_1` equals exactly the last valid second-stage index (quad: 112/84/183; pair: 124/82/208/373). So the second-stage tables are the right length; it is the size arrays that misstate them, in both directions, while the sibling binary family is exact — i.e. these are hand-maintained constants that have drifted, not a convention.

Consequence, stated precisely: today the practical exposure is bounded, because the checks at SMS_AAC.c:6467/6534 run *after* the table has already been indexed (that placement defect is SMS-123, already filed — not repeated here), and offsets constructible from the first-stage tables cannot exceed the last valid index. The new defect is that the bound values themselves are wrong: any fix that moves the SMS-123 check before the read — the obvious remediation — would still admit offsets `count` and `count+1` (1–2 entries past the end) for codebooks 1, 2, 4, 6, 10, and the −1 for codebook 11 sits exactly at its maximum reachable offset (373), so tightening the comparison to `>=` during such a fix would falsely reject valid codebook-11 streams. A bounds fix built on these arrays is a bounds fix that doesn't bound.

**Fix (in words):** Replace the hand-written size constants with values derived from the tables themselves (entry-count expressions / `sizeof` division at the pointer-table definition site), or correct the five wrong entries (114→113, 86→85, 185→184, 126→125, 210→209, 373→374) and add a host-side assertion that each size equals its table's entry count.

**Benefit:** The only bounds the two-step decoders have become true bounds; unblocks a correct SMS-123 remediation instead of one that preserves a 1–2 entry over-read.

**Effort:** S. **Risk:** Low — the corrected values only reject offsets that are unreachable from valid first-stage data (quad/pair maxima verified above); codebook 11's value moves in the permissive direction to its true count. **Confidence:** READ — all counts double-verified by script, first-stage reachability computed from the tables themselves, consumers line-read. No upstream comparison needed: the inconsistency is internal to this file.

---

## CAND-T1-3 / DTS integer quantizer tables `lossy_quant` / `lossless_quant` are dead: zero references repo-wide; only their float `_d` twins are ever read (256 bytes of `.data`) / Category 1

**Locations:** `src/SMS_DTS.c:1162-1171` (`lossy_quant[32]`), `src/SMS_DTS.c:1184-1193` (`lossless_quant[32]`); the live twins at `:1173-1182` and `:1195-1204`, consumed at `src/SMS_DTS.c:11171-11172`.

**Problem:** A whole-repo grep (`*.c`/`*.h`, all directories) for `lossy_quant` / `lossless_quant` returns only the two definitions; every consumer uses the `_d` float versions: `quant_step_table = (float*)lossless_quant_d; else quant_step_table = (float*)lossy_quant_d;` (SMS_DTS.c:11171-11172), indexed by `abits` at :11180. The integer pair is a leftover of the fixed-point variant of the same step tables. 2 tables × 32 entries × 4 bytes = 256 bytes. Both carry an explicit `section(".data")` attribute, which is the mechanism that keeps such tables emitted even when unreferenced (whether the linker in fact retains them in the shipped ELF is INFERRED — no ELF was inspected in this pass).

**Fix (in words):** Delete the two integer tables; the `_d` float tables already carry the same information in the representation the decoder uses.

**Benefit:** 256 bytes of dead `.data` and one less near-identical table pair to mistake for the live one during maintenance (the pairs differ in representation, so editing the wrong one is a silent no-op).

**Effort:** S. **Risk:** Low — zero references exist to break; the risk is only the INFERRED ELF-retention point, which affects the size claim, not the deadness claim. **Confidence:** READ for zero references and for the `_d` twins being the sole consumers' tables (grep + line read); INFERRED for the 256 bytes actually reaching the shipped ELF (section-attribute reasoning, no binary inspected).

---

## Coverage ledger

Tables enumerated and size-verified by script (declared dimension vs counted initializers, including per-row counts for 2-D tables). "OK" = declared == actual at every level. Consumers checked = primary consumer line-read and its assumed invariant tested.

### SMS_OGG.c:226–2542 — 17 tables enumerated, 17 size-OK, consumers checked
- `FROMdB_LOOKUP[35]` (261–271, declared via `FROMdB_LOOKUP_SZ`=35), `FROMdB2_LOOKUP[32]` (273–282): strictly decreasing (float), verified. Consumer SMS_OGG.c:4472 composes them as `FROMdB_LOOKUP[i >> FROMdB_SHIFT] * FROMdB2_LOOKUP[i & FROMdB2_MASK]` — assumes both monotonic-decaying; holds.
- `INVSQ_LOOKUP[33]` (284–294, declared `INVSQ_LOOKUP_SZ + 1` = 33), `INVSQ2EXP_LOOKUP[65]` (296–316, declared `MAX − MIN + 1` = 65): strictly decreasing, verified; macro-dimension resolution confirmed, no off-by-one.
- `FLOOR1_fromdB_LOOKUP[256]` (318–383): strictly increasing 1.06e-07 → 1.0, verified.
- `vwin64/128/256/512/1024/2048/4096/8192` (385–2447; declared 32/64/128/256/512/1024/2048/4096 = N/2 each): all counts exact; all monotonic increasing ≈0 → 1.0 (the Vorbis half-window shape the consumer assumes). Consumer: `vwin[aN]` returned at SMS_OGG.c:4922. (The `aN` index-domain defect is SMS-121, already filed — not repeated.)
- `_mapping_P[1]`, `_floor_P[2]`, `_residue_P[3]` (2528–2541, unsized pointer bundles): counts match the three export bundles defined directly above them.
- Span head note: lines 226–260 are the tail of a cosine table whose declaration sits above the assigned span (line <226); its visible tail is exactly antisymmetric (+1.0 → 0 → −1.0 mirror values, spot-verified by line read). Not counted above; flagged here so the ledger has no silent gap.

### SMS_AAC.c:587–2005 — 62 tables total across both AAC spans (with 2106–3582), sizes OK except CAND-T1-2's size arrays
- Windows (float bit patterns in `unsigned int`, reinterpreted before testing): `sine_long_1024[1024]` (587–716), `sine_short_128[128]` (718–735), `kbd_long_1024[1024]` (737–866), `kbd_short_128[128]` (868–885): counts exact; all monotonic increasing as floats ≈0 → 1.0 (an integer-domain test produces false violations on these — the values are IEEE-754 patterns; verified clean after reinterpretation).
- `_aac_iq_table[8192]` (887–1912, declared `IQ_TABLE_SIZE`): count exact; monotonic increasing as float.
- `pow2sf_tab[64]` (1914–1923): strictly increasing as float. `exp_table[128]` (1925–1942) / `mnt_table[128]` (1944–1961): consumers SMS_AAC.c:6984/6993 multiply them as floats (`(float*)exp_table)[j] * ((float*)mnt_table)[i]`); `exp_table` tail reaches exactly 0 via an arithmetic descent (…16777216, 8388608, 0, 0 at indices 124–127) — see Checked-clean.
- `ObjectTypesTable[32]` (1971–2004): count exact.
- Huffman two-step family (2136–2748): `hcbN[12]`, first-stage `hcb1_1/2_1/4_1/6_1/8_1[32]`, `hcb10_1[64]`, `hcb11_1[32]` — all counts exact, first-stage offsets monotonic non-decreasing, and max-reachable-offset == last valid second-stage index for every codebook (see CAND-T1-2). Second-stage and binary tables' counts are the subject of CAND-T1-2 (tables right, size arrays wrong). `Parity[256]` (2750–2759): verified exhaustively — `Parity[i] == popcount(i) % 2` for all 256 entries, zero mismatches.
- SBR/PS family (2761–3582): `tns_coef_0_3/0_4/1_3/1_4[16]` counts exact; ten `*_huffman_*` tables `[24|48|62|120][2]` all declared == actual with uniform row length 2; `E_deq_tab[64]`, `Q_div_tab_left/right` and `Q_div2_tab_left/right[31][13]` (all rows exactly 13), `Q_div_tab[31]`, `Q_div2_tab[31]`, `E_pan_tab[25]`: counts exact. `V[512]` complex pairs (3142–3399) and `qmf32_pre_twiddle[32]` pairs (3564–3581): counts exact. `qmf_c[640]` (3401–3562): count exact (prototype-filter symmetry tested — see Checked-clean).

### SMS_DTS.c:410–9950 — 60 tables enumerated, sizes OK except CAND-T1-1 (values) and CAND-T1-3 (dead pair)
- Huffman `bitalloc_{a..g}_{5,7,9,13,17,25,33,65,129}` family (410–1080): 47 tables, every declared count exact (6/8/10/14/18/26/34/66/130), every entry satisfies `code < 2^length` for its `{length, code, value}` triple (`huff_entry_t`, SMS_DTS.c:61–65), and every table ends with the `{0,0,0}` sentinel the `_dts_inverseq` walk assumes. Zero violations across the family.
- `bitalloc_select[11][8]` (1081–1106): rows are short (1/2/4/8 pointers, zero-padded to 8) — by design; consumer SMS_DTS.c:11185 tests `!bitalloc_select[abits][sel]` before use. See Checked-clean.
- `scale_factor_quant6[64]` / `scale_factor_quant7[128]` (1108–1160): counts exact; values are CAND-T1-1.
- `lossy_quant` / `lossless_quant` / `lossy_quant_d` / `lossless_quant_d[32]` (1162–1204): counts exact; zero tails at indices 27–31 are unreachable padding (see Checked-clean); the integer pair is CAND-T1-3.
- `adpcm_vb[4096][4]` (1206–3255): all 4096 rows exactly 4. Consumer SMS_DTS.c:11265 indexes `[prediction_vq][n−1]` with the 12-bit VQ index (SMS_DTS.c:11336) — domain matches the 4096 rows.
- `s_FIR32Bands[1024]` (3257–3514), `lfe_fir_64[512]` (3516–3645), `lfe_fir_128[512]` (3647–3776): counts exact; both LFE FIRs verified float-symmetric (mirror pairs equal); consumer SMS_DTS.c:11147–11148 selects by decimation factor.
- `high_freq_vq[1024][32]` (3778–9923): all 1024 rows exactly 32. Consumer SMS_DTS.c:11280 indexes by the 10-bit `high_freq_vq` code (SMS_DTS.c:11432) — domain matches 1024 rows; second index `subsubframe*8+m` < 32 by loop construction.
- `s_CosMod[544]` (9925–9960): count exact; consumed as the QMF modulation table at SMS_DTS.c:11124. No monotonic/symmetry invariant is assumed by that consumer, and none was asserted (see Checked-clean).

### SMS_MP123.c:190–4745 — 57 tables enumerated, 57 size-OK, consumers checked
- `s_GainPow2[378]` (190–286): count exact; strictly decreasing as float (2896.3 → ~0), the direction the gain consumer assumes. (Index-domain exposure is SMS-145, already filed — not repeated.)
- `s_nSLen2[512]` / `s_iSLen2[256]` (287–386): counts exact (packed-nibble scalefactor-length tables; nibble domain is structural to the `unsigned short` packing).
- `s_MapBuf0[9][152]`, `s_MapBuf1[9][156]`, `s_MapBuf2[9][44]` (387–824): every row exactly its declared width; `s_pMap[9][3]` / `s_pMapEnd[9][3]` (825–846) rows exactly 3.
- Huffman pointer tables `s_Tab0[1]`, `s_Tab1[7]`, `s_Tab2/3[17]`, `s_Tab5/6/7/8/9[31|71]`, `s_Tab10/11/12[127]`, `s_Tab13/15/16/24[511]`, `s_TabC0/C1[31]` (847–1089) plus `s_HuffTbl[32]` / `s_HuffTblC[2]` (1090–1127): all counts exact.
- `s_PreTab1/2[22]` (1128–1133): exact. `s_ISPow[8207]` (1135–3188): count exact; monotonic increasing as float (consumer SMS_MP123.c:5772 reads it as `(float*)s_ISPow`).
- `s_Tan1_1/Tan2_1/Tan1_2/Tan2_2[16]`, `s_Pow1_1/Pow2_1/Pow1_2/Pow2_2[2][16]` (3189–3260): exact. `s_LongLimit[9][23]` / `s_ShortLimit[9][14]` (3261–3327): every row exactly its declared width. `s_AACA/s_AACS[8]`, `s_Win[4][36]` / `s_Win1[4][36]` (3328–3419): exact; the two window tables differ in 60 of 144 entries (they are different windows, not copies — see Checked-clean).
- Layer-II allocation `s_Alloc0[288]`, `s_Alloc1[300]`, `s_Alloc2[80]`, `s_Alloc3[112]`, `s_Alloc4[196]` (3420–3673): counts exact, uniform 2-field rows. `s_Muls[27][64]` (3674–4134): all 27 rows exactly 64.
- `s_Grp3[96]` / `s_Grp5[384]` / `s_Grp9[3072]` (4135–4584): counts exact; value ranges verified structural — max values 2 / 4 / 8 respectively, i.e. every entry is a valid base-3 / base-5 / base-9 digit as the block-decode consumer assumes.
- `s_VU0Mpg0[352]`, `s_VU0Mpg1[672]`, `s_VU0Mpg2[1376]` (4585–4740): counts exact (VU microcode blobs; no numeric invariant applies).

### SMS_MPEG4.c:1–488 — 31 tables enumerated, 31 size-OK, consumers checked
- VLC pairs, all `(code, bits)` with `code < 2^bits` verified for every entry, zero violations: `g_IntraMCBPC_bits/code[9]`, `g_InterMCBPC_bits/code[28]`, `g_cbpy_tab[16][2]`, `g_mvtab[33][2]`, `g_inter_vlc[103][2]` + `g_inter_run[102]` + `g_inter_level[102]`, `g_intra_vlc[103][2]` + `g_intra_run[102]` + `g_intra_level[102]`, `s_inter_rvlc[170][2]` + run/level `[169]`, `s_intra_rvlc[170][2]` + run/level `[169]`, `g_DCtab_lum/chrom[13][2]`, `s_sprite_trajectory_tab[15][2]`, `s_mb_type_b_tab[4][2]`. The vlc/run/level triples are length-consistent in the pattern the `SMS_RLTable` descriptors (`s_rl_inter` :140, `s_rl_intra` :201, `s_rvlc_rl_inter` :297, `s_rvlc_rl_intra` :393) assume: n+1 VLC entries (incl. escape/last) over n run/level entries. Consumers initialise via `SMS_RL_Init`/`SMS_VLC_RL_Init` (SMS_MPEG4.c:653/658).
- `s_pixel_aspect[16]` (413–418): 16 rational pairs, exact (it is a `{num, den}` table — not a VLC pair; a code/bits test on it is a category error, noted to prevent a false filing).
- `s_default_intra_matrix[64]` / `s_default_non_intra_matrix[64]` (420–431): counts exact, all values in 1–255.
- `s_dc_threshold[8]` (442–444) = {99, 13, 15, 17, 19, 21, 23, 0}: consumer SMS_MPEG4.c:2659 indexes it with a 3-bit field; the leading 99 and trailing 0 are the field's boundary codes, not a monotonic run — see Checked-clean.
- `s_y_dc_scale_table[32]` / `s_c_dc_scale_table[32]` (446–454) and `g_chroma_qscale_table[32]` (456–459): counts exact; dc tables monotonic non-decreasing, chroma table strictly increasing 0–31. Consumers wired at SMS_MPEG4.c:2730-2731. Duplication vs SMS_MSMPEG4.c copies: see Checked-clean (tail divergence).
- `s_mb_type_b_map[4]`, `s_mpeg4_resync_prefix[8]` (461–470): exact.

### SMS_MSMPEG4.c:1–1396 — 37 tables enumerated, 37 size-OK, consumers checked
- `s_y_dc_scale_table[32]` / `s_c_dc_scale_table[32]` (77–84): exact; consumers SMS_MSMPEG4.c:1584-1585. Duplication diff vs MPEG4 copies in Checked-clean.
- RL families `s_table0_vlc[133][2]`+run/level`[132]`, `s_table1_vlc[149][2]`+`[148]`, `s_table2_vlc[186][2]`+`[185]`, `s_table4_vlc[169][2]`+`[168]` (85–354): counts exact, uniform rows, `code < 2^bits` for every entry, zero violations; `s_rl_table[6]` descriptor rows uniform (declared via `NB_RL_TABLES`=6).
- MV families `s_table0_mv_code/bits[1100]` + `s_table0_mvx/mvy[1099]`, `s_table1_mv_code/bits[1100]` + `s_table1_mvx/mvy[1099]` (363–1068): counts exact — the 1100 vs 1099 split is structural (index 0 of mvx/mvy is the unused zero-motion slot in the `MVTable` descriptors `s_mv_tables[2]`, :1069); every `(code, bits)` pair satisfies `code < 2^bits`, no zero bit-lengths.
- DC families `s_table0_dc_lum/chroma[120][2]`, `s_table1_dc_lum/chroma[120][2]` (1073–1201): counts exact; `code < 2^bits` holds for all entries including the 25/26-bit tail entries of `s_table1_dc_*` (those tails are long sequential code runs, a generated-table shape — see Checked-clean).
- `s_v2_intra_cbpc[4][2]`, `s_v2_mb_type[8][2]`, `s_table_mb_intra[64][2]`, `s_table_inter_intra[4][2]`, `s_table_mb_non_intra{,2,3,4}[128][2]` (1202–1368): counts exact, `code < 2^bits` throughout; `s_wmv2_inter_table[4]` pointer table (1369) references exactly the four non-intra tables above it.

### SMS_WMA.c:1–1127 — 28 tables enumerated, 28 size-OK, consumers checked
- `wma_critical_freqs[25]` (56–61): strictly increasing 100–24500, verified (consumer SMS_WMA.c:1354/1386ff walks it assuming ascending band edges).
- `exponent_band_22050/32000/44100[3][25]` (63–79): rows are variable-length (11/15/24, 12/16/17, 13/16/18 initializers, zero-padded to 25) — by design; consumer SMS_WMA.c:1373-1375 does `n = *table++; for (i < n) bands[i] = table[i]`, i.e. first entry is the band count. Verified structurally: for every row, count field == number of following entries, and the following entries sum to exactly 128 / 256 / 512 (rows 0/1/2) — the three block lengths. Zero violations.
- Huffman pairs — every `(code, bits)` pair in all eight pairs satisfies `code < 2^bits`, zero violations: `hgain_huffcodes/bits[37]`, `scale_huffcodes/bits[121]`, `coef0…coef5_huffcodes/bits[666/555/1336/1072/476/435]` (81–1115); code and bits arrays are pairwise equal in length. Consumer: `coef_vlcs[6]` descriptor table (1116–1127) pairs them; selected at SMS_WMA.c:1447-1448.
- `lsp_codebook[8][16]` (95–127, declared `[NB_LSP_COEFS][16]`): 8 rows of exactly 16.
- `levels0…levels5[60/40/340/180/70/40]` (158–223): counts exact (run-level symbol tables paired with the coef Huffman tables above).

### SMS_AC3.c:1–340 — 21 tables enumerated in span (incl. 2 function-local tables at 1957–1962 in the second span), sizes OK except the noted s_SAB padding (Checked-clean)
- Quantizer families `s_Q10/Q11/Q12[32]`, `s_Q20/Q21/Q22[128]`, `s_Q3[8]`, `s_Q40/Q41[128]`, `s_Q5[16]` (46–172): counts exact. These are macro-generated (`Q0/Q1/Q2` ternary patterns, SMS_AC3.c:40–44) with deliberate zero padding to the power-of-two index domain (e.g. `s_Q10` carries 27 ternary entries + 5 zeros) — structural, not truncation.
- `g_AC3Channels[8]` = {2,1,2,3,3,4,4,5} (38): exact. `s_HalfRate[12]` (177–179): exact.
- `s_Exp1/s_Exp2/s_Exp3[128]` (181–206): counts exact; all values in [−2, 25]; consumers SMS_AC3.c:636/657/678 add them as exponent deltas from a 7-bit field — domain matches 128. The three tables are related decompositions of the same 7-bit exponent code, not copies (pairwise different, as designed).
- `s_DitherLUT[256]` (208–241): exact (pseudo-random dither — no invariant applies or was asserted). `s_HtHTab[3][50]` (243–262): all rows exactly 50.
- `s_BAPTab[305]` (264–285): exact; consumer SMS_AC3.c:1067/1095 indexes it as `(s_BAPTab + 156)[lMask + 4*lpExp[i]]` — the 305 length is the union of the two sub-tables the +156 split assumes. Values in [−3, 16].
- `s_BndTab[30]` (287–291): strictly increasing 21–253, verified. `s_LATab[256]` (293–316): exact; consumer SMS_AC3.c:1170/1176 indexes by `lDelta >> 1` — a log-approximation table, monotonic in its segments as designed.
- `s_SAB[20]` (318–321): 8 initializers — see Checked-clean (consumer reads only 8).
- Second span SMS_AC3.c:1210–1972 contains **no constant tables** — it is the downmix coefficient code (`_ac3_downmix_init` switch and related functions); the only tables in it are the function-local `s_nFChansTbl[11]` and `s_RematrixBand[4]` at :1957-1962, both counted above and exact. The task brief's "1210–1972 table spans" description does not match the file at this pin; the span was read and this is the discrepancy, recorded rather than silently skipped.

---

## Checked-clean (suspicions tested and dissolved, with reasoning)

1. **WMA `exponent_band_*` short rows (declared `[3][25]`, rows of 11–24 initializers).** Dissolved: the consumer treats entry 0 as a count (`n = *table++`, SMS_WMA.c:1373) and reads exactly `n` further entries; every row's count matches its entry count and the widths sum to exactly the block length (128/256/512). The remaining slots are C zero-padding that is never read.
2. **DTS `bitalloc_select[11][8]` short rows (1/2/4/8 pointers per row).** Dissolved: rows are NULL-padded pointer sets and the only indexing consumer null-tests the entry before use (`if (abits >= 11 || !bitalloc_select[abits][sel])`, SMS_DTS.c:11185).
3. **DTS `lossy/lossless_quant{,_d}` zero tails (indices 27–31 = 0).** Dissolved as a value defect: the index is `abits`, and the subframe header rejects `bitalloc > 26` (`if (state->bitalloc[j][k] > 26) return -1;`, SMS_DTS.c:11347) before `_dts_subsubframe` reads `quant_step_table[abits]` (:11180). Indices 27–31 are unreachable padding to 32. (The integer pair's separate deadness is CAND-T1-3.)
4. **AC3 `s_SAB[20]` with only 8 initializers.** Dissolved as a bounds defect: the sole consumer is the inline-asm `_ac3_mix32toS`, which loads only offsets 0 and 16 bytes from `s_SAB` (SMS_AC3.c:1697ff) — exactly the 8 initialized `int`s. Entries 8–19 are zero-filled and never read from C or asm (whole-file grep: two references total, definition + the one `la`). At most 48 bytes of slack; below the filing bar on its own.
5. **AAC `exp_table[126..127] = 0`.** Dissolved: the tail is an arithmetic descent in steps of 2^23 (…, 25165824, 16777216, 8388608 at indices 123–125) that reaches exactly 0 at index 126; 0 is the sequence's own continuation, not a truncation break.
6. **Duplicated DC-scale tables, MPEG4 vs MSMPEG4 (task item b).** Diffed byte-for-byte: `s_y_dc_scale_table` copies agree on indices 0–24 and diverge at 25–31 (MPEG4: 34,36,38,40,42,44,46; MSMPEG4: 33,34,35,36,37,38,39); `s_c_dc_scale_table` copies agree on 0–25 and diverge at 26–31 (MPEG4: 20,21,22,23,24,25; MSMPEG4: 19,20,20,21,21,22). **Not filed**: both copies remain smooth and monotonic, and the divergence is a clean tail-shape difference (steps of +2 vs +1), which is the signature of two different standardised DC-scaling curves (MPEG-4 Part 2's non-linear tail vs the linear MPEG-1/2-style tail the MS-MPEG4 decoder uses), not of copy corruption — a corrupted copy breaks smoothness, as CAND-T1-1's tables do. Which curve belongs to which standard is INFERRED (upstream/spec comparison would be needed to confirm absolutely); the in-repo evidence shows two deliberate, internally consistent tables feeding two different decoders (SMS_MPEG4.c:2730-2731 vs SMS_MSMPEG4.c:1584-1585), so there is no internal inconsistency to file.
7. **AAC first-stage Huffman tables (`hcb*_1`) under a `(code, bits)` test.** Category error, dissolved: `hcb` is `{uint8_t offset; uint8_t extra_bits}` (SMS_AAC.c:525–528), not a VLC pair; `offset ≥ 2^extra_bits` is expected. Tested instead on their real invariants (offset monotonicity, reachable-offset closure) — all pass; the failure found in that family is the size arrays (CAND-T1-2).
8. **MSMPEG4 `s_table1_dc_lum/chroma` 25/26-bit tail entries.** Dissolved: every entry satisfies `code < 2^bits` (e.g. 0x0F34B7 < 2^25, 0x1E6940 < 2^26); the tails are long runs of sequential codes, the normal shape of these generated WMV DC tables. An early script pass flagged them only because of an arbitrary `bits > 24` heuristic in the test itself — recorded here so the false positive is not re-filed.
9. **AAC window / MP123 power tables under integer monotonic tests.** Dissolved as test artefacts: `sine_*`, `kbd_*`, `_aac_iq_table`, `s_ISPow`, `s_GainPow2` store IEEE-754 bit patterns in integer arrays (their consumers cast to `float*`); after float reinterpretation every one is monotonic in its assumed direction with zero violations. (An initial pass also produced false violations from a script bug that stripped trailing hex `F` digits; the parser was fixed and all counts/invariants re-run — the numbers in this section are from the fixed pass, and the disputed AAC counts were re-verified with a second, independent counter.)
10. **MP123 `s_Win` vs `s_Win1`.** Diffed: 60 of 144 entries differ. Not a duplication defect — the names, and the decode paths that select between them, treat them as two distinct windows; no internal inconsistency.
11. **AAC `qmf_c[640]` symmetry.** Tested both mirror hypotheses: neither exact symmetry nor antisymmetry holds (max deviation 0.011 / 1.71 respectively), but the SBR QMF consumer imposes no symmetry invariant on the stored prototype (it indexes the table directly in the analysis/synthesis filters), so there is no consumer assumption to file against. No filing without an invariant — per the task's own rule (d).
12. **DTS `s_CosMod[544]` / `s_FIR32Bands[1024]`.** No monotonic or symmetry invariant is assumed by their consumers (QMF modulation / 32-band synthesis at SMS_DTS.c:11124 and the FIR path), and none was fabricated for filing purposes. Counts exact; listed here as enumerated-and-consumer-checked with no invariant claimed.
13. **AC3 quantizer zero padding (`s_Q10` etc.).** The zeros sit beyond the ternary-pattern domain (27 of 32, etc.) in macro-generated tables whose consumers index by the grouped-bit fields the patterns enumerate; padding to the power-of-two size is the table's own structure. No filing.

## Method notes / limitations
- All counting was scripted (Python, `/tmp` scratch) and the headline counts (AAC second-stage family, DTS quant tails) were each produced by two independent methods or by direct line read. One script bug (trailing-hex-`F` stripping) and one test-design error (arbitrary 24-bit VLC cap) were caught and corrected mid-pass; both false-positive families are documented in Checked-clean items 8–9 so they are not re-derived as findings later.
- No PS2 build or hardware verification applies to this section (table contents are host-verifiable); nothing here is hardware-verified, consistent with the audit's standing caveat.
- The one place upstream comparison is genuinely required is CAND-T1-1's replacement values; it is marked INFERRED there, per instructions.
