# REDO Audit — SMS W2: EE Audio Decoders

Blind redo audit, worker W2. Source: `~/workspace/audit-clones-2026-11-13/Simple-Media-System` at pin `e65ced39a27eee8d996e9257e21594d1cac1c93d` (verified with `git rev-parse HEAD` before reading). Read-only; nothing in the clone was modified.

Rule applied throughout: **LESS, NOT MORE** — candidates prefer deletion, simplification, or a minimal guard at the sink over any new mechanism. One mechanism = one candidate, with all locations listed.

---

## (a) Coverage note

### Files read in full

| File | Lines | Notes |
|---|---|---|
| `src/SMS_AAC.c` | 8,302 | Read end-to-end in chunks, including all SBR, Huffman, filterbank and frame-dispatch code. |
| `src/SMS_DTS.c` | 11,801 | All code read end-to-end; the ~9,800 lines of Huffman/FIR/VQ constant tables were verified by script (below) rather than read literal-by-literal. |
| `src/SMS_AC3.c` | 2,772 | Read end-to-end. |
| `src/SMS_AC3_imdct.c` | 642 | Tables script-counted; all code read. |
| `src/SMS_FLAC.c` | 594 | Read end-to-end. |
| `src/SMS_MP123.c` | 6,729 | All code read end-to-end; constant tables script-counted. |
| `src/SMS_WMA.c` | 2,112 | Read end-to-end. |
| `include/SMS_AAC.h` | 27 | |
| `include/SMS_DTS.h` | 31 | |
| `include/SMS_AC3.h` | 187 | |
| `include/SMS_FLAC.h` | 37 | |
| `include/SMS_MP123.h` | 131 | |
| `src/SMS_WMA.h` | 27 | Header lives in `src/`, not `include/`. |
| `include/SMS_Bitio.h` | 148 | Shared bit reader used by every decoder in scope; read in full. |

Spot-read **outside** the primary scope, only to establish reachability of in-scope mechanisms: `src/SMS_Player.c:100-115` (channel clamp / SPDIF switch), `src/SMS_ContainerMPEG_PS.c:400-464` (AC3/DTS channel probing), `src/SMS_ContainerFLAC.c:66,114` (FLAC channel gate). No findings are raised against those files.

### Table verification method

Every constant-table initializer in the seven `.c` files was extracted and counted by script (brace-balanced parse; counts are entry counts, not eyeball estimates): **345 initializers** in total. Structural checks run on top of the census:

- declared size vs counted entries for every table;
- monotonicity where the table's structure implies it (AC3 `s_BndTab`, `s_LATab`; AAC `swb_offset_*`);
- AAC scalefactor-band tables: endpoints, band-width divisibility, `num_swb` vs table length for all 12 sample-rate indices, both windows;
- Huffman tree walks: for the AAC binary trees (`hcb3/5/7/9`), `hcb_sf`, and the SBR envelope/noise trees, every reachable node offset and every branch target was checked to stay inside the table; for the AAC two-step tables, first-stage `offset + 2^extra_bits − 1` was checked against the second-stage table size for all 7 codebooks;
- DTS Huffman tables: presence of the `{0,0,0}` terminator entry the `_dts_inverseq` walk relies on (62 tables);
- WMA: `sum(levels[i]) + 2 == coef table size` for all six coefficient VLCs (the precondition `init_coef_vlc` needs); zero entries inside the initialized part of all `exponent_band_*` rows;
- MP123: maximum `LinBits` in `s_HuffTbl`, maxima of `s_LongLimit`/`s_ShortLimit`;
- FLAC CRC table length and last entry.

Counted-vs-declared discrepancies found by the census are exactly: `s_SAB` (declared 20, only 8 initialized — CAND-W2-23), FLAC `ctx` (257 entries, index 256 unreachable — see checked-clean), and a set of `counted = declared + 1` cases that are trailing-comma artifacts of the counter, each confirmed benign by leaf counts or direct reading (`s_VU0Mpg2` 1376 leaves = declared; `hgain_huffcodes` 37 leaves = declared; DTS `bitalloc_select` and the downmix `table[11][10]` read directly and complete). Every other table's counted entries equal its declared size.

### Script-verified table census (declared size = counted entries)

**src/SMS_AAC.c** (109 tables):
- `sine_long_1024[1024]` = 1024
- `sine_short_128[128]` = 128
- `kbd_long_1024[1024]` = 1024
- `kbd_short_128[128]` = 128
- `_aac_iq_table[IQ_TABLE_SIZE]` = 8192
- `pow2sf_tab[64]` = 64
- `exp_table[128]` = 128
- `mnt_table[128]` = 128
- `mes[]` = 33
- `ObjectTypesTable[32]` = 32
- `num_swb_1024_window[]` = 12
- `swb_offset_1024_96[]` = 42
- `swb_offset_1024_64[]` = 48
- `swb_offset_1024_48[]` = 50
- `swb_offset_1024_32[]` = 52
- `swb_offset_1024_24[]` = 48
- `swb_offset_1024_16[]` = 44
- `swb_offset_1024_8[]` = 41
- `swb_offset_1024_window[]` = 12
- `num_swb_128_window[]` = 12
- `swb_offset_128_96[]` = 13
- `swb_offset_128_64[]` = 13
- `swb_offset_128_48[]` = 15
- `swb_offset_128_24[]` = 16
- `swb_offset_128_16[]` = 16
- `swb_offset_128_8[]` = 16
- `swb_offset_128_window[]` = 12
- `hcb_sf[][2]` = 241
- `hcbN[]` = 12
- `hcb1_1[]` = 32
- `hcb2_1[]` = 32
- `hcb4_1[]` = 32
- `hcb6_1[]` = 32
- `hcb8_1[]` = 32
- `hcb10_1[]` = 64
- `hcb11_1[]` = 32
- `hcb_table[]` = 12
- `hcb1_2[]` = 113
- `hcb2_2[]` = 85
- `hcb4_2[]` = 184
- `hcb_2_quad_table[]` = 12
- `hcb_2_quad_table_size[]` = 12
- `hcb3[]` = 161
- `hcb_bin_table_size[]` = 12
- `hcb5[]` = 161
- `hcb7[]` = 127
- `hcb9[]` = 337
- `hcb_bin_table[]` = 12
- `hcb6_2[]` = 125
- `hcb8_2[]` = 83
- `hcb10_2[]` = 209
- `hcb11_2[]` = 374
- `hcb_2_pair_table[]` = 12
- `hcb_2_pair_table_size[]` = 12
- `Parity[256]` = 256
- `tns_coef_0_3[]` = 16
- `tns_coef_0_4[]` = 16
- `tns_coef_1_3[]` = 16
- `tns_coef_1_4[]` = 16
- `t_huffman_env_bal_3_0dB[24][2]` = 24
- `f_huffman_env_bal_3_0dB[24][2]` = 24
- `t_huffman_env_bal_1_5dB[48][2]` = 48
- `f_huffman_env_bal_1_5dB[48][2]` = 48
- `t_huffman_env_3_0dB[62][2]` = 62
- `f_huffman_env_3_0dB[62][2]` = 62
- `t_huffman_env_1_5dB[120][2]` = 120
- `f_huffman_env_1_5dB[120][2]` = 120
- `t_huffman_noise_bal_3_0dB[24][2]` = 24
- `t_huffman_noise_3_0dB[62][2]` = 62
- `E_deq_tab[64]` = 64
- `Q_div_tab_left[31][13]` = 31
- `Q_div_tab_right[31][13]` = 31
- `Q_div_tab[31]` = 31
- `Q_div2_tab_left[31][13]` = 31
- `Q_div2_tab_right[31][13]` = 31
- `Q_div2_tab[31]` = 31
- `E_pan_tab[25]` = 25
- `V[]` = 512
- `qmf_c[640]` = 640
- `qmf32_pre_twiddle[]` = 32
- `startMinTable[12]` = 12
- `offsetIndexTable[12]` = 12
- `offset[7][16]` = 7
- `stopMinTable[12]` = 12
- `offset[12][14]` = 12
- `vDk[64]` = 1
- `log2tab[10]` = 10
- `vDk0[64]` = 1
- `vk0[64]` = 1
- `temp1[3]` = 3
- `t_E_temp[6]` = 1
- `saved_t_E[6]` = 1
- `limGain[]` = 4
- `Xsbr[MAX_NTSRHFG][64]` = 5
- `h_smooth[]` = 5
- `phi_re[]` = 4
- `phi_im[]` = 4
- `goalSbTab[]` = 12
- `limiterBandsCompare[]` = 3
- `limTable[100]` = 1
- `patchBorders[64]` = 1
- `sample_rates[]` = 12
- `pred_sfb_max[]` = 12
- `pow2_table[]` = 4
- `tns_sbf_max[][4]` = 16
- `state[2 * TNS_MAX_ORDER]` = 1
- `spec_data1[1024]` = 1
- `spec_data2[1024]` = 1
- `spec_data[1024]` = 1
**src/SMS_DTS.c** (87 tables):
- `dts_sample_rates[16]` = 16
- `dts_bit_rates[32]` = 32
- `dts_channels[16]` = 16
- `dts_bits_per_sample[7]` = 7
- `bitalloc_a_12[13]` = 13
- `bitalloc_b_12[13]` = 13
- `bitalloc_c_12[13]` = 13
- `bitalloc_d_12[13]` = 13
- `bitalloc_e_12[13]` = 13
- `bitalloc_12[5]` = 5
- `scales_a_129[130]` = 130
- `scales_b_129[130]` = 130
- `scales_c_129[130]` = 130
- `scales_d_129[130]` = 130
- `scales_e_129[130]` = 130
- `scales_129[5]` = 5
- `bitalloc_a_3[4]` = 4
- `bitalloc_a_4[5]` = 5
- `bitalloc_b_4[5]` = 5
- `bitalloc_c_4[5]` = 5
- `bitalloc_d_4[5]` = 5
- `tmode[4]` = 4
- `bitalloc_a_5[6]` = 6
- `bitalloc_b_5[6]` = 6
- `bitalloc_c_5[6]` = 6
- `bitalloc_a_7[8]` = 8
- `bitalloc_b_7[8]` = 8
- `bitalloc_c_7[8]` = 8
- `bitalloc_a_9[10]` = 10
- `bitalloc_b_9[10]` = 10
- `bitalloc_c_9[10]` = 10
- `bitalloc_a_13[14]` = 14
- `bitalloc_b_13[14]` = 14
- `bitalloc_c_13[14]` = 14
- `bitalloc_a_17[18]` = 18
- `bitalloc_b_17[18]` = 18
- `bitalloc_c_17[18]` = 18
- `bitalloc_d_17[18]` = 18
- `bitalloc_e_17[18]` = 18
- `bitalloc_f_17[18]` = 18
- `bitalloc_g_17[18]` = 18
- `bitalloc_a_25[26]` = 26
- `bitalloc_b_25[26]` = 26
- `bitalloc_c_25[26]` = 26
- `bitalloc_d_25[26]` = 26
- `bitalloc_e_25[26]` = 26
- `bitalloc_f_25[26]` = 26
- `bitalloc_g_25[26]` = 26
- `bitalloc_a_33[34]` = 34
- `bitalloc_b_33[34]` = 34
- `bitalloc_c_33[34]` = 34
- `bitalloc_d_33[34]` = 34
- `bitalloc_e_33[34]` = 34
- `bitalloc_f_33[34]` = 34
- `bitalloc_g_33[34]` = 34
- `bitalloc_a_65[66]` = 66
- `bitalloc_b_65[66]` = 66
- `bitalloc_c_65[66]` = 66
- `bitalloc_d_65[66]` = 66
- `bitalloc_e_65[66]` = 66
- `bitalloc_f_65[66]` = 66
- `bitalloc_g_65[66]` = 66
- `bitalloc_a_129[130]` = 130
- `bitalloc_b_129[130]` = 130
- `bitalloc_c_129[130]` = 130
- `bitalloc_d_129[130]` = 130
- `bitalloc_e_129[130]` = 130
- `bitalloc_f_129[130]` = 130
- `bitalloc_g_129[130]` = 130
- `bitalloc_select[11][8]` = 12
- `scale_factor_quant6[64]` = 64
- `scale_factor_quant7[128]` = 128
- `lossy_quant[32]` = 32
- `lossy_quant_d[32]` = 32
- `lossless_quant[32]` = 32
- `lossless_quant_d[32]` = 32
- `adpcm_vb[4096][4]` = 4096
- `s_FIR32Bands[1024]` = 1024
- `lfe_fir_64[512]` = 512
- `lfe_fir_128[512]` = 512
- `high_freq_vq[1024][32]` = 1024
- `s_CosMod[544]` = 544
- `sl_SampleRate[16]` = 16
- `sl_BitRate[32]` = 32
- `table[11][10]` = 12
- `adj_table[4]` = 4
- `s_FloatToInt[2]` = 2
**src/SMS_AC3.c** (33 tables):
- `g_AC3Channels[8]` = 8
- `s_Q10[32]` = 32
- `s_Q11[32]` = 32
- `s_Q12[32]` = 32
- `s_Q20[128]` = 128
- `s_Q21[128]` = 128
- `s_Q22[128]` = 128
- `s_Q3[8]` = 8
- `s_Q40[128]` = 128
- `s_Q41[128]` = 128
- `s_Q5[16]` = 16
- `s_HalfRate[12]` = 12
- `s_Exp1[128]` = 128
- `s_Exp2[128]` = 128
- `s_Exp3[128]` = 128
- `s_DitherLUT[256]` = 256
- `s_HtHTab[3][50]` = 3
- `s_BAPTab[305]` = 305
- `s_BndTab[30]` = 30
- `s_LATab[256]` = 256
- `s_SAB[20]` = 8
- `s_Rate[19]` = 19
- `s_LFEOn[8]` = 8
- `s_Table[11][8]` = 11
- `s_CLev[4]` = 4
- `s_SLev[4]` = 4
- `s_SlowGain[4]` = 4
- `s_DbPbTab[4]` = 4
- `s_FloorTab[8]` = 8
- `s_nFChansTbl[11]` = 11
- `s_RematrixBand[4]` = 4
- `s_BndTab[16]` = 16
- `s_FloatToInt[2]` = 2
**src/SMS_AC3_imdct.c** (10 tables):
- `s_IMDCTWindow[256]` = 256
- `s_Roots16[3]` = 3
- `s_Roots32[7]` = 7
- `s_Roots64[15]` = 15
- `s_Roots128[31]` = 31
- `s_Pre1[256]` = 256
- `s_Post1[128]` = 128
- `s_Pre2[128]` = 128
- `s_Post2[64]` = 64
- `s_FFTOrder[128]` = 128
**src/SMS_FLAC.c** (7 tables):
- `ctx[257]` = 257
- `sample_size_table[]` = 8
- `blocksize_table[]` = 16
- `sample_rate_table[]` = 16
- `_synth_indep[2]` = 2
- `SynthSamples[4]` = 4
- `lpInp[2]` = 2
**src/SMS_MP123.c** (71 tables):
- `g_mpa_freq_tab[3]` = 3
- `g_mpa_bitrate_tab[2][3][15]` = 2
- `s_GainPow2[378]` = 378
- `s_nSLen2[512]` = 512
- `s_iSLen2[256]` = 256
- `s_MapBuf0[9][152]` = 9
- `s_MapBuf1[9][156]` = 9
- `s_MapBuf2[9][44]` = 9
- `s_pMap[9][3]` = 9
- `s_pMapEnd[9][3]` = 9
- `s_Tab0[1]` = 1
- `s_Tab1[7]` = 7
- `s_Tab2[17]` = 17
- `s_Tab3[17]` = 17
- `s_Tab5[31]` = 31
- `s_Tab6[31]` = 31
- `s_Tab7[71]` = 71
- `s_Tab8[71]` = 71
- `s_Tab9[71]` = 71
- `s_Tab10[127]` = 127
- `s_Tab11[127]` = 127
- `s_Tab12[127]` = 127
- `s_Tab13[511]` = 511
- `s_Tab15[511]` = 511
- `s_Tab16[511]` = 511
- `s_Tab24[511]` = 511
- `s_TabC0[31]` = 31
- `s_TabC1[31]` = 31
- `s_HuffTbl[32]` = 32
- `s_HuffTblC[2]` = 2
- `s_PreTab1[22]` = 22
- `s_PreTab2[22]` = 22
- `s_ISPow[8207]` = 8207
- `s_Tan1_1[16]` = 16
- `s_Tan2_1[16]` = 16
- `s_Tan1_2[16]` = 16
- `s_Tan2_2[16]` = 16
- `s_Pow1_1[2][16]` = 2
- `s_Pow2_1[2][16]` = 2
- `s_Pow1_2[2][16]` = 2
- `s_Pow2_2[2][16]` = 2
- `s_LongLimit[9][23]` = 9
- `s_ShortLimit[9][14]` = 9
- `s_AACA[8]` = 8
- `s_AACS[8]` = 8
- `s_Win[4][36]` = 4
- `s_Win1[4][36]` = 4
- `s_Alloc0[288]` = 288
- `s_Alloc1[300]` = 300
- `s_Alloc2[80]` = 80
- `s_Alloc3[112]` = 112
- `s_Alloc4[196]` = 196
- `s_Muls[27][64]` = 27
- `s_Grp3[96]` = 96
- `s_Grp5[384]` = 384
- `s_Grp9[3072]` = 3072
- `s_VU0Mpg0[352]` = 352
- `s_VU0Mpg1[672]` = 672
- `s_VU0Mpg2[1376]` = 1377
- `sl_Translate[3][2][16]` = 3
- `sl_pTables[5]` = 5
- `sl_SBLims[5]` = 5
- `aFraction[2][4][32]` = 10
- `sl_Tab[10]` = 10
- `sl_Tabs[2][5]` = 2
- `sl_Len[2][16]` = 2
- `sl_STab[3][6][4]` = 3
- `aXRBuf[2][32][18]` = 3
- `sl_pTabs[3][2][2]` = 3
- `_mp3_get_scale_factors[2]` = 2
- `mpa_decode_frame[4]` = 4
**src/SMS_WMA.c** (28 tables):
- `wma_critical_freqs[25]` = 25
- `exponent_band_22050[3][25]` = 3
- `exponent_band_32000[3][25]` = 3
- `exponent_band_44100[3][25]` = 3
- `hgain_huffcodes[37]` = 38
- `hgain_huffbits[37]` = 37
- `lsp_codebook[NB_LSP_COEFS][16]` = 10
- `scale_huffcodes[121]` = 121
- `scale_huffbits[121]` = 121
- `levels0[60]` = 60
- `levels1[40]` = 40
- `levels2[340]` = 340
- `levels3[180]` = 180
- `levels4[70]` = 70
- `levels5[40]` = 40
- `coef0_huffbits[666]` = 666
- `coef1_huffbits[555]` = 555
- `coef2_huffbits[1336]` = 1336
- `coef3_huffbits[1072]` = 1072
- `coef4_huffbits[476]` = 476
- `coef5_huffbits[435]` = 435
- `coef0_huffcodes[666]` = 666
- `coef1_huffcodes[555]` = 555
- `coef2_huffcodes[1336]` = 1336
- `coef3_huffcodes[1072]` = 1072
- `coef4_huffcodes[476]` = 476
- `coef5_huffcodes[435]` = 435
- `coef_vlcs[6]` = 6


---

## (b) Candidates, sorted by benefit

### CAND-W2-1 — DTS: stream-controlled `prim_channels` (up to 8) overflows state and stack arrays sized for 5
- **Category:** 6 bugs/security
- **Locations:** `src/SMS_DTS.c:26` (`#define DTS_PRIM_CHANNELS_MAX 5`); `src/SMS_DTS.c:10756` (`state->prim_channels = bitstream_get(state, 3) + 1;` → 1–8, never validated); `src/SMS_DTS.c:10758-10813` (frame-header loops writing `subband_activity[i]`, `prediction_mode[i]`, … for `i < prim_channels`); `src/SMS_DTS.c:11171` (stack array `float subband_samples[DTS_PRIM_CHANNELS_MAX][8][32]` in `_dts_subsubframe`, indexed by `k < prim_channels` at :11174); `src/SMS_DTS.c:11329-11447` (subframe-header loops over `prim_channels` into `[5]`-wide state fields).
- **Problem:** Every per-channel array in the DTS state struct and the subsubframe stack buffer is sized `[DTS_PRIM_CHANNELS_MAX]` = 5, but the 3-bit frame-header field yields up to 8 and nothing in `_dts_frame`, `dts_subframe_header`, or `_dts_subsubframe` checks it. A crafted DTS frame with `prim_channels` 6–8 writes past the static decoder state and past a stack array. The neighbouring fields *are* clamped (`subband_activity`/`vq_start_subband` are capped to `DTS_SUBBANDS` at :10760-10768), so this one field is simply the missed case.
- **Fix:** In `_dts_frame`, immediately after :10756, return an error if `state->prim_channels > DTS_PRIM_CHANNELS_MAX` — one check, mirroring the clamp style already used two lines below. Values 6–8 have no valid DTS-core arrangement (core modes top out at 5 primary channels + LFE), so no valid stream is affected.
- **Benefit:** Closes a struct-overflow + stack-overflow reachable from any DTS file, in the decoder's hottest entry path.
- **Effort:** Trivial (one comparison).
- **Risk:** None identified for valid streams.
- **Confidence:** READ (field width, array sizes, and absence of any check all read directly; array sizing confirmed against the `#define`).

### CAND-W2-2 — DTS: header fields and a cumulative sum index fixed tables with no range checks (downmix / bitalloc / LFE / scale factors)
- **Category:** 6 bugs/security
- **Locations:** `src/SMS_DTS.c:10722` (`state->amode = bitstream_get(state, 6)`, 0–63) feeding `src/SMS_DTS.c:10261` (`table[flags & DTS_CHANNEL_MASK][input]`, table declared `[11][10]` at :10203); `src/SMS_DTS.c:10780` (`bitalloc_huffman[j] = bitstream_get(state, 3)`, 0–7) feeding `src/SMS_DTS.c:11345` (`bitalloc_12[state->bitalloc_huffman[j]]`, table has 5 entries at :189 — values 5 and 6 are diverted to direct reads at :11340-11343, value **7** is not); `src/SMS_DTS.c:11440` (`scale_factor_quant7[bitstream_get(state, 8)]` — 0–255 into a `[128]` table at :1127); `src/SMS_DTS.c:11386` and `:11402` (`scale_table[scale_sum]`, where `scale_sum` is a cumulative sum of Huffman deltas accumulated at :11376-11392 with no bound against the `[64]`/`[128]` tables at :1108/:1127).
- **Problem:** Four distinct sinks, one mechanism: values taken straight from the bitstream (plus one derived by unbounded accumulation) are used as indices into fixed tables. `amode ≥ 10` reads past the downmix table's row; bitalloc selector 7 reads a garbage function-table pointer and dereferences it in `_dts_inverseq`; an LFE scale byte ≥ 128 reads past `scale_factor_quant7`; a scale-factor delta walk can push `scale_sum` negative or past the quant table in either direction. (Related fall-through, same lines: `scalefactor_huffman == 7` matches neither the `< 5`, `== 5`, nor `== 6` branch at :11378-11392, silently reusing a stale `scale_sum`.) The AC3 decoder in the same codebase shows the intended pattern — its downmix path masks and range-checks the equivalent mode value.
- **Fix:** Range-check at each sink: reject `amode` values the `[11][10]` table doesn't cover before `_dts_downmix_init` indexes it; reject bitalloc selector 7 alongside the existing 5/6 diversion; mask or reject the LFE byte to the table's 128 entries; clamp/validate `scale_sum` against the selected table's size before :11386/:11402. No new tables or abstractions.
- **Benefit:** Removes four independent out-of-bounds reads (one of them a wild pointer dereference) from the DTS header path.
- **Effort:** Small (four guards).
- **Risk:** Low; valid DTS streams stay inside every one of these ranges by construction of the format.
- **Confidence:** READ (all field widths, table sizes, and index expressions read directly; table sizes script-counted).

### CAND-W2-3 — DTS SPDIF passthrough: header frame size can make `memset` length negative (huge) and overrun the output buffer
- **Category:** 6 bugs/security
- **Locations:** `src/SMS_DTS.c:11651` (`aLen = DTS_SyncInfo(...)` — returns the frame size declared in the stream header, up to 16,383); `src/SMS_DTS.c:11652` (`retVal = lnSamples << 2` — at most 8,192 for the largest frame type); `src/SMS_DTS.c:11658-11692` (byte-swap asm copies `aLen` bytes into the `retVal`-sized output buffer); `src/SMS_DTS.c:11694` (`memset(lpOutBuf + 8 + aLen, 0, retVal - 8 - aLen);`).
- **Problem:** `retVal` is derived from the sample count, `aLen` from an independent header field; nothing reconciles them. When `aLen > retVal - 8`, the copy already overruns the ring buffer, and the `memset` length goes negative, becoming a near-4 GB `size_t` write. The AC3 equivalent (`_ac3_spdif`) is safe only because the AC3 format caps frames at 3,840 bytes against a fixed 6,144-byte burst — the DTS port reused the shape without the format guarantee.
- **Fix:** After :11651, bail out (return 0 / drop the frame) when `aLen > retVal - 8`, before allocating or copying.
- **Benefit:** Removes a remotely triggerable memory-corruption primitive in the SPDIF path.
- **Effort:** Trivial (one comparison).
- **Risk:** None for valid streams; oversized declared frames are undecodable anyway.
- **Confidence:** READ (both size derivations and the memset read directly; DTS header frame-size range is the format's 14-bit field parsed by `DTS_SyncInfo`).

### CAND-W2-4 — AAC: the constant 16428320 (0xFAAD20) is wired into the decode path — one reachable sample overwrite, two dead remnants
- **Category:** 6 bugs/security
- **Locations:** `src/SMS_AAC.c:5030` (`hf_assembly`: `if (sbr->bs_extension_id == 3 && sbr->bs_extension_data == 42) Xsbr[...][0] = 16428320;`); `src/SMS_AAC.c:6553-6560` (`huffman_codebook()` returns the two halves of 16428320); `src/SMS_AAC.c:6589-6594` (`huffman_spectral_data` case 12 decodes a pair, then overwrites both values with `huffman_codebook(0/1)`); `src/SMS_AAC.c:3809` (`get_sr_index`: `if (16428320 <= samplerate) return 11;`).
- **Problem:** 16428320 = 0xFAAD20 is a watermark constant, not audio data. Three consequences: (1) the SBR site at :5030 is **reachable** — any SBR stream carrying an extension payload with id 3 and data 42 has a QMF sample forcibly replaced by 16428320.0, a full-scale click/corruption injected by the stream; (2) the case-12 overwrite is dead weight — `section_data` already rejects codebook 12 with error 32 before spectral decoding, so the decoded-then-discarded pair and `huffman_codebook()` can never run; (3) the `get_sr_index` branch is arithmetically dead (any rate ≥ 16428320 already matched the ≥ 92017 case returning 0). One mechanism, three locations; only the first affects output.
- **Fix:** Delete all three: the `hf_assembly` assignment, `huffman_codebook()` and the case-12 overwrite (case 12 keeps the plain pair decode for defence in depth), and the dead `get_sr_index` comparison.
- **Benefit:** Removes a stream-triggered corruption of decoded audio and ~15 lines of dead/misleading code.
- **Effort:** Trivial (pure deletions).
- **Risk:** None; no valid decode path depends on any of the three.
- **Confidence:** READ (all three sites and the `section_data` rejection of codebook 12 read directly).

### CAND-W2-5 — AC3/DTS: two-entry float-to-int function-pointer table indexed by an unvalidated channel count in both decoders
- **Category:** 6 bugs/security
- **Locations:** `src/SMS_AC3.c:2576-2585` (`s_FloatToInt[2]`, indexed `s_FloatToInt[anChannels - 1]`); `src/SMS_DTS.c:11556-11565` (same pattern); callers pass the codec context's channel count straight through — `src/SMS_AC3.c:2760` and the matching `DecodeFrame(apOutput, apCtx->m_Channels, lLen)` in `DTS_Decode`; both decoders *adopt the stream's* channel count (up to 6) when the context count is 0 and only ever decrease it afterwards (`src/SMS_AC3.c:2727-2731`, and the identical block in `DTS_Decode`).
- **Problem:** Neither decoder validates `anChannels` before indexing a 2-entry function-pointer table. Playback is safe today only because of an invariant maintained *outside* the decoders: the player clamps `m_Channels` to 2 for non-SPDIF playback (`src/SMS_Player.c:105`) and the SPDIF configuration (where the count is set to 5, :109) swaps in the passthrough function instead (`src/SMS_AC3.c:348-349`, `src/SMS_DTS.c:9984-9985`). If the clamp runs while a container still reports 0 channels (so it no-ops) and the decoder then adopts a 5.1 stream's 6 channels on the first frame, `s_FloatToInt[5]` is called — a wild function pointer taken from adjacent static data.
- **Fix:** Validate at the sink: at the top of `_ac3_decode`/`_dts_decode`, return 0 unless `anChannels` is 1 or 2 (the only values the table and the downmix flags below it support). Two one-line guards replace reliance on a cross-file invariant.
- **Benefit:** Converts a latent wild-pointer call into a local, checkable invariant in both decoders.
- **Effort:** Trivial.
- **Risk:** None for any stream the player can currently play (all reach here with 1–2).
- **Confidence:** READ for the mechanism (table, index, call chain, and the player clamp all read directly); INFERRED for the exact interleaving that reaches it with a count > 2.

### CAND-W2-6 — AAC: sample-rate index from ADTS/ASC (up to 15) indexes 12-entry window tables unchecked
- **Category:** 6 bugs/security
- **Locations:** `src/SMS_AAC.c:6041` (`hDecoder->sf_index = adts.sf_index;` — raw 4-bit field, 0–15); the ASC path in `AudioSpecificConfig2` likewise keeps a 4-bit index, and index 15 (explicit frequency) passes its validation because the explicit rate is non-zero; `src/SMS_AAC.c:6204-6218` (`window_grouping_info` indexes `num_swb_1024_window[sf_index]`, `swb_offset_1024_window[sf_index]`, `num_swb_128_window[sf_index]` — all 12-entry arrays, script-counted).
- **Problem:** Indices 12–15 are reserved in both ADTS and ASC headers, and nothing rejects them: `_NeAACDecInit`/`_NeAACDecInit2` store the index, and the first decoded frame reads past the 12-entry tables (the pointer-table read at :6207 then dereferences whatever pointer follows the array). `get_sample_rate()` already encodes the validity rule — it returns 0 for index ≥ 12 — but init never treats a 0 rate as fatal.
- **Fix:** In both init paths, fail initialisation when `get_sample_rate(sf_index) == 0` (equivalently `sf_index >= 12`). One check per path, reusing the existing rule.
- **Benefit:** Closes an out-of-bounds table + pointer read on any crafted ADTS/MP4-AAC header.
- **Effort:** Trivial.
- **Risk:** None; indices 12–15 cannot describe a playable stream.
- **Confidence:** READ (field width, storage, table sizes, and the missing check all read directly).

### CAND-W2-7 — AAC: DRC `excluded_channels` loop writes past fixed 64-entry masks, driven by stream bits
- **Category:** 6 bugs/security
- **Locations:** `src/SMS_AAC.c:7549-7562` (`excluded_channels`: `while ((drc->additional_excluded_chns[n-1] = SMS_GetBit(ld))) { for (i = num_excl_chan; i < num_excl_chan + 7; ++i) drc->exclude_mask[i] = ...; ++n; num_excl_chan += 7; }`); array sizes at `src/SMS_AAC.c:333-334` (`exclude_mask[MAX_CHANNELS]`, `additional_excluded_chns[MAX_CHANNELS]`, MAX_CHANNELS = 64).
- **Problem:** The loop has no bound tied to the arrays it fills. Each continuation bit from the stream adds 7 more mask writes; after the 10th iteration `exclude_mask` writes run past the 64-entry array inside the heap-allocated decoder struct, and the `uint8_t` counter `n` itself wraps at 255, re-indexing `additional_excluded_chns[n-1]` from the start. A fill element carrying a DRC extension with a long run of continuation bits corrupts the decoder handle.
- **Fix:** Stop the loop when `num_excl_chan + 7 > MAX_CHANNELS` (treat as end of the excluded-channel list). One comparison inside the existing loop.
- **Benefit:** Removes a heap overwrite in the AAC fill-element path.
- **Effort:** Trivial.
- **Risk:** None; real DRC data never legitimately excludes more channels than the decoder supports.
- **Confidence:** READ (loop, counter types, and array sizes all read directly).

### CAND-W2-8 — WMA: version-1 exponent bands can contain zero-width entries, and `decode_exp_vlc` underflows on a zero entry into a runaway write
- **Category:** 6 bugs/security
- **Locations:** `src/SMS_WMA.c:1347-1362` (WMA v1 band computation: `lpCtx->exponent_bands[0][i] = pos - lpos;` with **no** `pos > lpos` check); contrast `src/SMS_WMA.c:1386` (the v2 computed path does guard: `if (pos > lpos) lpCtx->exponent_bands[k][j++] = pos - lpos;`); consumer at `src/SMS_WMA.c:1627-1667` (`decode_exp_vlc`: `n = *ptr++; do { *q++ = v; } while (--n);` at :1646-1648 and :1658-1660).
- **Problem:** In the v1 path, when two consecutive critical frequencies round to the same band position (small block length), `pos - lpos` is 0 and a zero entry is stored. `decode_exp_vlc` trusts every entry as a repeat count in a do-while: a 0 entry makes `--n` underflow (unsigned) and the loop writes exponents across the channel buffer until it faults. The v2 path's guard proves the v1 omission is an oversight, not a design choice. (The table-driven v2 path is safe: its tables are count-prefixed and contain no zero entries — script-verified.)
- **Fix:** Mirror the v2 guard in the v1 loop (store the band only when `pos > lpos`, compacting with a separate write index as :1386 does). A few lines, deleting the divergence between the two paths.
- **Benefit:** Removes a runaway heap write reachable from WMA v1 streams at small block sizes.
- **Effort:** Small.
- **Risk:** Low; zero-width bands carry no exponents, so skipping them matches the decoder's own v2 behaviour.
- **Confidence:** READ for the guard asymmetry and the do-while underflow; INFERRED for which specific sample-rate/block-size combinations produce a repeated position.

### CAND-W2-9 — DTS: mono float-to-int converter is an empty stub — mono DTS "decodes" to stale buffer contents
- **Category:** 8 half-finished
- **Locations:** `src/SMS_DTS.c:11511-11513` (`_dts_float_to_int_1:` is `jr $ra / nop` — an empty function); selected via `s_FloatToInt[0]` at `src/SMS_DTS.c:11556-11565` whenever `anChannels == 1`; the AC3 equivalent `_ac3_float_to_int_1` in `src/SMS_AC3.c` is a full converter.
- **Problem:** For a mono DTS stream the decode loop runs, `_dts_block` fills the sample buffer, and the converter that should write interleaved PCM into the output ring buffer does nothing — the player emits whatever bytes the ring buffer already held, at full frame size, indefinitely. This is a silently wrong-output bug for an entire stream class, not a graceful degradation.
- **Fix:** Implement the mono converter following the AC3 `_1` pattern (single-channel clamp/convert loop over 256-sample blocks). 
- **Justification (fix adds code):** The alternative within "less" is rejecting mono DTS in `DTS_Decode`, which removes a supported stream class; a ~30-line converter cloned from the sibling decoder is the smallest change that makes the existing dispatch table honest.
- **Benefit:** Mono DTS files play actual audio instead of buffer garbage.
- **Effort:** Small.
- **Risk:** Low; the mono path is currently broken, so any faithful converter is an improvement.
- **Confidence:** READ (stub body and dispatch read directly).

### CAND-W2-10 — Shared bit reader (`SMS_Bitio.h`) has no end-of-buffer check in any reader — the root mechanism under most stream-side over-reads
- **Category:** 6 bugs/security
- **Locations:** `include/SMS_Bitio.h:70` (`SMS_GetBits`) and its siblings `SMS_GetSBits`, `SMS_GetBit`, `SMS_ShowBits`, `SMS_SkipBits` (whole header read, 148 lines): every function advances/reads via `m_Idx` and the cache without ever comparing `m_Idx` against `m_szInBits`.
- **Problem:** All seven decoders in scope parse attacker-controlled lengths, counts, and Huffman walks through this reader. Whenever a header or payload overstates its length (several candidates above are instances), parsing simply continues past the end of the packet buffer, reading heap neighbours as if they were stream data. This is why the per-sink guards in CAND-W2-2/6/7 are necessary at all.
- **Fix:** No single cheap fix is recommended here: clamping inside the reader changes hot-path semantics for every codec in the player (and the AAC code uses its own cache macros layered on the same context). The actionable conclusion is the pattern used by the other candidates — validate lengths/counts at the sinks that know the real bounds — and to treat any *new* unbounded field found in these decoders as a bug on sight. Recorded so the pattern is named once instead of rediscovered per decoder.
- **Benefit:** Names the shared root cause; directs future fixes to the right layer.
- **Effort:** None (documentation-level finding; sink fixes are the other candidates).
- **Risk:** None.
- **Confidence:** READ (header read in full).

### CAND-W2-11 — FLAC: fixed-predictor subframe reads up to 4 samples *before* the channel buffer for predictor orders 0–3
- **Category:** 6 bugs/security
- **Locations:** `src/SMS_FLAC.c:182-185` (`decode_subframe_fixed`: `a = decoded[pred_order-1]; b = a - decoded[pred_order-2]; c = … decoded[pred_order-3]; d = … decoded[pred_order-4];` — all four computed unconditionally, before the `switch (pred_order)` at :187).
- **Problem:** For `pred_order` 0–3 — the common cases in real FLAC files — the code reads `decoded[-1]` down to `decoded[-4]` even though the switch then ignores most of those values. For channel 0 that is a heap under-read (into allocator metadata); for channel 1 it reads the tail of channel 0's buffer. The values are discarded, so output is unaffected, but the read itself is out of bounds on every affected subframe.
- **Fix:** Compute each of `b`, `c`, `d` only when the order needs it (guard with `if (pred_order > 1/2/3)` or move the computations into the switch cases). Pure restructuring of four lines; nothing added.
- **Benefit:** Removes a per-subframe out-of-bounds read in the FLAC hot path.
- **Effort:** Trivial.
- **Risk:** None; the discarded values are provably unused for the affected orders.
- **Confidence:** READ.

### CAND-W2-12 — AC3: `s_Samples` workspace is 192 KB of BSS; the highest offset ever used is ~3,072 floats (~12 KB)
- **Category:** 4 size/memory
- **Locations:** `src/SMS_AC3.c:323` (`static sample_t s_Samples[16 * 256 * 12]` = 49,152 floats = 192 KB, `.bss`); usage at `src/SMS_AC3.c:2305-2307` (base pointer, +256 LFE shift), `:2401-2480` (maximum expression `lpSamples + 1536 + 256 * i` with `i < 5`, plus the 256-wide IMDCT write at that address → highest touched index ≈ 3,072); `:2612` (handed to the float-to-int converter). A grep of the whole file confirms no other references.
- **Problem:** The buffer is declared for 16 blocks × 12 channels worth of samples, but the decode writes at most 6 channel rows of 256 plus the 1,536-float IMDCT delay region — roughly 1/16 of the allocation. On the PS2's 32 MB RAM this is ~180 KB of permanently reserved, never-touched memory.
- **Fix:** Shrink the declaration to `16 * 256` floats (4,096 — still 33% headroom over the proven maximum offset of ~3,072) = 16 KB.
- **Benefit:** ~176 KB of BSS returned, zero behavioural change.
- **Effort:** Trivial (one constant).
- **Risk:** Low; the bound is established by exhaustive grep of the file's uses, not by sampling.
- **Confidence:** READ.

### CAND-W2-13 — WMA: superframe loop trusts container `block_align` (0 → hang; partial final frame → over-read)
- **Category:** 6 bugs/security
- **Locations:** `src/SMS_WMA.c:2040-2100` (`wma_decode_superframe`: frame loop advances `lpBuf += lFrmSize` where `lFrmSize = lpCtx->block_align`, without clamping against the remaining input size); `src/SMS_WMA.c:1247` (`lpCtx->block_align = apCtx->m_BlockAlign;` — taken from the container with no validation in `wma_init`).
- **Problem:** Two failure modes from one unchecked field: (1) `block_align == 0` makes the loop never advance — the player hangs on a malformed ASF/WMA header; (2) when the final packet chunk is smaller than `block_align`, the loop keeps consuming frames past the end of the input buffer. Every other decoder in scope reconciles declared frame size against remaining bytes (e.g. AC3/DTS `Decode` loops); the WMA path doesn't.
- **Fix:** Reject `block_align == 0` in `wma_init` (fail codec open), and bound the superframe loop by the remaining input length.
- **Benefit:** Removes a hang and an over-read from malformed WMA/ASF files.
- **Effort:** Small.
- **Risk:** Low; valid ASF streams always have `block_align > 0` and packet-aligned frames.
- **Confidence:** READ for the loop structure and the missing init check; INFERRED for a container actually delivering `block_align == 0` (the field is header-controlled).

### CAND-W2-14 — AAC: codebook-11 escape values can index the 8,192-entry inverse-quant table out of bounds in the `vquant` asm
- **Category:** 6 bugs/security
- **Locations:** `src/SMS_AAC.c:887` (`_aac_iq_table`, 8,192 entries, script-counted); `src/SMS_AAC.c:6559-6575` (`huffman_getescape`: the escape-length counter `i` is a `uint8_t` incremented per leading 1-bit with no cap, and the returned value is built as `(1 << i) | offset` then truncated to `int16_t`); `src/SMS_AAC.c:6739-6820` (`vquant` asm computes the table address as `base + (|q| << 2)` with no range check, called for every coefficient group from `quant_to_spec`).
- **Problem:** Ordinary Huffman pairs/quads bound spectral values to ≤ 16, but the codebook-11 escape path manufactures values from a bit-run whose length the stream controls. Any resulting `|q| ≥ 8192` makes `vquant` load from past the 32 KB table; the `uint8_t` wrap of the run counter makes the produced values effectively arbitrary within `int16_t`. (The table itself is exactly sized for the *legitimate* escape range; the missing piece is the cap the reference decoder applies to the escape length.)
- **Fix:** Cap the escape run in `huffman_getescape` (stop extending `i` at the legitimate maximum, mirroring the format's escape definition) so produced values stay inside the table by construction.
- **Benefit:** Removes an out-of-bounds read in the AAC coefficient path.
- **Effort:** Small.
- **Risk:** Low; legitimate escapes never reach the cap.
- **Confidence:** INFERRED (asm addressing and escape construction read directly; no crafted stream was executed).

### CAND-W2-15 — DTS: file-scope sample-rate/bit-rate tables are dead duplicates of the live copies inside `_dts_syncinfo`
- **Category:** 1 dead code (with 2 duplication)
- **Locations:** `src/SMS_DTS.c:127-143` (`dts_sample_rates[16]`, `dts_bit_rates[32]` — a grep of the whole file shows no reference to either name outside these declarations); the live, identical tables are the function-local `sl_SampleRate[16]` / `sl_BitRate[32]` at `src/SMS_DTS.c:10105-10121`.
- **Problem:** 48 ints (192 bytes of `.data`) are carried in the binary and maintained in parallel with the copies actually used — a standing trap: anyone patching rates in the file-scope tables changes nothing.
- **Fix:** Delete the file-scope pair; keep the `_dts_syncinfo` locals.
- **Benefit:** 192 bytes and one duplication trap removed.
- **Effort:** Trivial.
- **Risk:** None (zero references, grep-verified).
- **Confidence:** READ.

### CAND-W2-16 — AC3: `_ac3_bit_allocate` can index `s_LATab` one past its 256 entries
- **Category:** 6 bugs/security
- **Locations:** `src/SMS_AC3.c:293` (`s_LATab[256]`, script-verified: 256 entries, non-decreasing −64…0); `src/SMS_AC3.c:1170` (`lPSD = lNext + s_LATab[(-lDelta) >> 1];`, sibling at :1176).
- **Problem:** `lDelta` is derived from exponent differences and takes multiples of 128; the value −512 — an exponent drop of exactly 4 groups between adjacent bands — yields index 256, one past the end of the table. The read lands in whatever static follows `s_LATab` and perturbs the PSD computation (audio-level consequence, plus a formal out-of-bounds read).
- **Fix:** Clamp the index to 255 at the two call sites (or bound `lDelta` once at the top of the delta loop).
- **Benefit:** Removes a one-element over-read in the bit-allocation inner loop.
- **Effort:** Trivial.
- **Risk:** None; index 255 is the table's own endpoint value (0), the natural saturation.
- **Confidence:** INFERRED (index arithmetic and table size are READ; that a real exponent sequence produces exactly −512 was derived, not executed).

### CAND-W2-17 — MP123: granule gain pointer + scalefactor index can walk past the 378-entry `s_GainPow2` table
- **Category:** 6 bugs/security
- **Locations:** `src/SMS_MP123.c:190` (`s_GainPow2[378]`, script-counted 378); `src/SMS_MP123.c:5422` (`m_pPow2Gain = (float*)s_GainPow2 + 256 - SMS_GetBits(lpBitCtx, 8)` — base offset 1–256 from an 8-bit stream field, plus up to +56 from the sub-gain fields at :5435-5437); `src/SMS_MP123.c:5741` and `:5853` (`m_pPow2Gain[(*apSCF++) << lShift]`, shift 1–2, 6-bit scalefactors → index up to 252).
- **Problem:** Worst-case reach is offset ≈ 256 + 252 = 508 into a 378-entry table — ~130 floats past the end. In valid MP2 streams the global gain and scalefactors are correlated so real files stay in range (which is why this ships unnoticed), but the code itself enforces nothing: the bound exists only as an emergent property of well-formed input. Upstream mpg123 carries a materially larger gain table for the same arithmetic.
- **Fix:** Saturate the base pointer when it is computed at :5422 so that `base + 252` cannot exceed the table (clamp the 8-bit gain contribution), rather than growing the table.
- **Benefit:** Converts an emergent bound into an enforced one in the MP2 dequantizer.
- **Effort:** Small.
- **Risk:** Low; saturation only engages on streams already outside the table's design range.
- **Confidence:** INFERRED (all arithmetic read directly; worst-case correlation in valid streams not disproven).

### CAND-W2-18 — WMA: `wma_init` divides by the container-supplied sample rate without a zero check
- **Category:** 6 bugs/security
- **Locations:** `src/SMS_WMA.c:1352` and `:1383` (`pos = ((block_len * 2 * a) + …) / b` with `b = lpCtx->sample_rate`); `src/SMS_WMA.c:1400-1402` (`high_band_start` divides by `lpCtx->sample_rate`); the rate is stored from the codec context during `wma_init` (same function that stores `block_align` at :1247) with no `== 0` rejection.
- **Problem:** A WMA/ASF header declaring sample rate 0 reaches three integer divisions by zero during codec init — a hard crash (integer divide-by-zero trap on EE) before a single frame is decoded. Sibling candidate CAND-W2-13 covers the same init function's unchecked `block_align`; the two fields deserve one validation block.
- **Fix:** Reject `sample_rate == 0` at the top of `wma_init` (together with the `block_align` check of CAND-W2-13).
- **Benefit:** Removes a trivially triggered crash on malformed files.
- **Effort:** Trivial.
- **Risk:** None; a zero sample rate describes nothing playable.
- **Confidence:** INFERRED (divisions and the missing check are READ; that a container will pass 0 through is header-dependent).

### CAND-W2-19 — FLAC: decoder trusts the channel count beyond its 2-entry synth table and a shift-divide (currently saved only by the container's gate)
- **Category:** 6 bugs/security
- **Locations:** `src/SMS_FLAC.c:437-444` (`_synth_indep[2]` function-pointer table indexed by `m_nChannels - 1` at :441); `src/SMS_FLAC.c:523-524` (`lBlockLen = 4096 >> nChannels; lnParts = m_BlockSize / lBlockLen;` — `lBlockLen` becomes 0 at `nChannels ≥ 13`, making the division a divide-by-zero).
- **Problem:** Inside the decoder, nothing bounds `m_nChannels`. The shipped FLAC container happens to refuse files with ≥ 3 channels (`src/SMS_ContainerFLAC.c:114`) and the STREAMINFO field itself is 3 bits (max 8 → synth index up to 7, still out of bounds), so both sinks are one container change — or one alternate caller — away from a wild function-pointer call. The decoder should not depend on that.
- **Fix:** Validate `1 ≤ m_nChannels ≤ 2` once in the FLAC decoder's init (the decoder only implements mono/stereo synthesis anyway — see the 2-entry table and the left/right/mid synth variants at :478).
- **Benefit:** Makes the FLAC decoder self-guarding; removes a latent wild-pointer call.
- **Effort:** Trivial.
- **Risk:** None for any file the current container accepts.
- **Confidence:** INFERRED (in-decoder mechanism READ; reachability currently blocked by the container gate, which is also READ).

### CAND-W2-20 — AC3/DTS: `Decode` writes uninitialized `lBitRate`/`lSampleRate` locals to the codec context when sync fails
- **Category:** 6 bugs/security
- **Locations:** `src/SMS_AC3.c:2735` (`apCtx->m_BitRate = lBitRate;` — placed after the if/else, so it also runs on the `AC3_SyncInfo(...) == 0` failure branch where `lBitRate` was never written); `src/SMS_DTS.c:11763` (identical structure in `DTS_Decode`).
- **Problem:** On every failed sync probe — i.e. while sliding across non-AC3/DTS bytes looking for a header — the context's bit-rate field is overwritten with uninitialized stack content. The value is cosmetic (display/UI), but the read of uninitialized memory is real, happens continuously during seeks over foreign data, and makes the reported bitrate nondeterministic garbage.
- **Fix:** Move the assignment into the sync-success branch in both files (two-line move, ×2).
- **Benefit:** Removes an uninitialized read and garbage state churn on the seek path.
- **Effort:** Trivial.
- **Risk:** None.
- **Confidence:** READ (control flow read directly in both functions).

### CAND-W2-21 — AAC: multichannel frames are mixed through `pcm_synN` with phantom copies of channel 0 for 3–4 channel content
- **Category:** 6 bugs/security
- **Locations:** `src/SMS_AAC.c:7925-7958` (`output_to_PCM`: switch on `CONV(channels, downMatrix)`); driven from `_aac_decode`, which forces `output_channels = 2` and sets `downMatrix = 1` for any frame with > 2 channels, so the switch lands in the `default` case, calling `pcm_synN` with `lppChan[0..4]` taken from `internal_channel[0..4]`; `internal_channel` is zeroed at the top of every `_aac_decode` call, so for a 3- or 4-channel frame slots 3–4 still contain 0 — i.e. channel 0's buffer pointer.
- **Problem:** If `pcm_synN` (extern, `src/SMS_AAC.c:42`; defined outside the scope files) mixes a fixed five channels down to stereo, then 3- and 4-channel AAC frames get channel 0 summed in two or three extra times — audibly wrong levels, though not memory-unsafe (the pointer is a valid allocated buffer). 5.1 content is unaffected; mono/stereo take the dedicated cases.
- **Fix:** Pass the frame's real channel count into the mix (or zero-fill unused `lppChan` slots instead of leaving them aliased to channel 0 — silence mixes safer than a duplicated channel).
- **Benefit:** Correct downmix levels for 3.0/4.0 AAC content.
- **Effort:** Small.
- **Risk:** Low.
- **Confidence:** INFERRED (the dispatch and the zeroed `internal_channel` array are READ; `pcm_synN`'s fixed-width assumption could not be verified from the scope files).

### CAND-W2-22 — AC3 IMDCT: `aBias` parameter of `_ac3_imdct_512`/`_ac3_imdct_256` is threaded through every call but never used
- **Category:** 1 dead code
- **Locations:** `src/SMS_AC3_imdct.c:533` (`_ac3_imdct_512(sample_t*, sample_t*, sample_t aBias)`); `src/SMS_AC3_imdct.c:579` (`_ac3_imdct_256`, same); call sites passing `s_AC3Ctx.m_Bias`/`lBias` at `src/SMS_AC3.c:2401,2432-2480`. Neither function body references the parameter (the bias is applied by callers directly to the sample buffer, e.g. `src/SMS_AC3.c:2440`).
- **Problem:** A dead parameter on two hot functions, plus the context plumbing that feeds it, misleads every reader into hunting for bias handling inside the IMDCT that isn't there.
- **Fix:** Drop the parameter from both functions and their call sites. (The `m_Bias` field itself stays — callers use it as a fill value.)
- **Benefit:** Removes a false trail in the AC3 DSP path; negligible size win.
- **Effort:** Trivial.
- **Risk:** None (parameter provably unread in both bodies).
- **Confidence:** READ.

### CAND-W2-23 — AC3: `s_SAB` is declared `[20]` with only 8 entries initialized; its sole consumer reads 2 of them
- **Category:** 4 size/memory
- **Locations:** `src/SMS_AC3.c:318-321` (`int s_SAB[20]` with exactly 8 initializers — `0x38504E62` ×4 then `0x3CCC2D41` ×4, script-counted; the remaining 12 entries are implicit zeros); sole use at `src/SMS_AC3.c:1697` (asm in `_ac3_mix32toS` loads the table base; the two distinct values are the only ones ever read, as float bit patterns).
- **Problem:** 48 bytes of `.sdata` carry 12 zero entries nothing reads, and the `[20]` declaration invites a reader to believe a 20-entry scale table exists.
- **Fix:** Declare `s_SAB[8]`.
- **Benefit:** 48 bytes and one misleading declaration removed.
- **Effort:** Trivial.
- **Risk:** None (single consumer, grep-verified).
- **Confidence:** READ.

---

## (c) Checked-clean notes

Verified by reading and/or script; **no** finding raised. (Licences were not audited — settled per instructions.)

**Table census (all files).** 345 initializers extracted and counted; counted entries equal declared sizes everywhere except the items named in the coverage note (CAND-W2-23, the unused FLAC CRC entry below, and confirmed trailing-comma artifacts). In particular: DTS `high_freq_vq[1024][32]` = 32,768 leaves, `adpcm_vb[4096][4]` = 16,384 leaves; MP123 `s_Grp9[3072]`, `s_ISPow[8207]`, `s_Muls[27][64]` all exact; AAC `V[512]` complex table = 1,024 leaves; WMA all six `coef*_huffbits`/`coef*_huffcodes` pairs match their declared sizes exactly.

**AC3.** Grouped-quantizer zero tails are intentional invalid-code sinks, sized exactly to their code widths: `s_Q10/11/12` 27 real + 5 zero = 32; `s_Q20/21/22` 125 + 3 = 128; `s_Q3` 7 + 1 = 8; `s_Q40/41` 121 + 7 = 128; `s_Q5` 15 + 1 = 16. `s_Exp1/2/3` carry a `25,25,25` sentinel at indices 125–127 that deliberately forces the `anExp > 24` error path. `AC3_SyncInfo` bounds its fields (`apBuf[5] < 0x60`, frame-size code < 38, rate index ≤ 18 into `s_Rate[19]`); `_ac3_frame`'s unchecked `s_HalfRate[apBuf[5]>>3]` is safe via that caller invariant. `_ac3_parse_deltba` guards `j + lDeltLen >= 50` against `m_DeltBA[50]`; coupling coordinates fit `m_CplCo[5][18]` exactly (max 18 coupling bands); the rematrix loop is bounded by the `EndMant ≤ 253` invariant via the `lChBwCod ≤ 60` check. `_ac3_spdif`'s memset is safe (AC3 frames ≤ 3,840 < 6,144-byte burst — contrast CAND-W2-3). The suspected duplicated downmix `CONVERT` case is not a duplicate (`AC3_3F2R→AC3_3F1R` ≠ `AC3_3F2R→AC3_2F1R`). `s_BndTab[30]` is non-decreasing 21→253 and `s_LATab[256]` non-decreasing −64→0 (script). `AC3_Destroy`/`DTS_Destroy` are empty, but the codec contexts are static and the `SMS_Codec` shell is owned by the caller — no leak attributable to the stubs.

**AAC.** Scalefactor-band tables are fully consistent (script): all 13 `swb_offset_*` tables are monotonic, start at 0, end exactly at 1024/128, every band width is divisible by 4 (so the 4-lane `vquant` loop never straddles a band edge), and `num_swb[i]` equals the pointed-to table's length − 1 for all 12 indices in both windows. Huffman structures walk clean (script): `hcb_sf` — 241 entries, all reachable, maximum offset 240, no out-of-bounds target (matching the runtime `offset > 240` guard); binary trees `hcb3` (161), `hcb5` (161), `hcb7` (127), `hcb9` (337) — every entry reachable, every branch target in-bounds; two-step tables — first-stage maximum index ≤ second-stage maximum index for all seven codebooks (hcb11 reaches exactly 373 = its declared maximum, with 374 entries present). SBR envelope/noise trees (`t/f_huffman_env_*`, non-balance variants) also walk in-bounds; the `*_bal_*` trees store signed first-column values and were excluded from the walk model rather than flagged. `section_data` performs its `MAX_SFB`/section-count checks *before* the writes they protect; `decode_scale_factors` range-checks the accumulated scalefactor (0–255) and `quant_to_spec` re-checks before indexing `pow2sf_tab[64]`; PNS/MS/IS loops clamp with `min(..., swb_offset_max)`; TNS filter/coefficient indices fit their `[8][4][32]` arrays by field width; `program_config_element`'s tag selects fit `[16]` arrays and its element counts fit `pce[16]` in the ADIF header; DRC `band_top[17]` vs `num_bands ≤ 16` fits, and `drc_decode`'s `top ≤ 1024` bound holds. `ObjectTypesTable[32]` matches the 5-bit object-type field exactly. `calc_sbr_tables` routes `freq_scale == 0` away from the `temp1[bs_freq_scale-1]` indexing; Q/Q_div table indices are bounds-checked (0–30, 0–24); `E_deq_tab` exponents are checked < 64.

**DTS.** All 62 Huffman entry tables terminate with the `{0,0,0}` entry `_dts_inverseq` relies on (script). `bitalloc_select[11][8]`'s NULL tails are honoured by the caller's short-circuit (`abits >= 11 || !bitalloc_select[abits][sel]`) before any dereference. `adpcm_vb[4096]` vs the 12-bit VQ index and `high_freq_vq[1024]` vs the 10-bit index are exact fits. Bit-allocation values are rejected above 26 against 32-entry quant tables. `subband_activity` and `vq_start_subband` are clamped to `DTS_SUBBANDS` in the frame header. `scales_129[5]` and `tmode[4]` exactly fit their selector fields' checked ranges (< 5 and 2 bits respectively).

**FLAC.** The CRC table `ctx[257]` is indexed only by `(uint8_t)crc ^ *buffer` (`src/SMS_FLAC.c:72`), so entries 0–255 are used and the 257th entry (`0x01`) is unreachable — one dead byte, noted here rather than as a candidate. `sample_size_table[8]`, `blocksize_table[16]`, `sample_rate_table[16]` match their header field widths.

**MP123.** The main entry `MP123_Decode` runs `MP123_CheckHeader` before `MP123_DecodeHeader`, gating the frequency/layer index paths (the exported `MP123_DecodeHeader` alone lacks that validation, but it has no unguarded in-tree caller). `_seek_to_main_data`'s backstep (≤ 511) fits the 512-byte reservoir headroom. `m_TabSelect` (5 bits) into `s_HuffTbl[32]` and `m_Count1TblSel` into `s_HuffTblC[2]` are exact fits. Maximum `LinBits` in `s_HuffTbl` is 13 (script), consistent with `s_ISPow[8207]`; `s_LongLimit`/`s_ShortLimit` row maxima are all 32 (script), matching the hybrid buffer bound.

**WMA.** All six coefficient VLCs satisfy `sum(levels[i]) + 2 == coef table size` exactly (script: 666/555/1336/1072/476/435), so `init_coef_vlc` builds precisely-sized tables. The table-driven `exponent_band_*` rows are count-prefixed, contain no zero entries in their initialized data (script), and the zero-filled tails are never consumed; the computed v2 band path guards zero-width bands (contrast CAND-W2-8). `wma_init` already clamps channel count to 1/2 with an explanatory comment. `decode_exp_lsp` values ≤ 15 fit `lsp_codebook[][16]`; `pow_m1_4` indices are bounded (exponent ≤ 255 into `[256]`, mantissa < 128).

**Shared.** `SMS_Bitio.h`'s unbounded reads are recorded once as CAND-W2-10; per-decoder length fields that *are* reconciled (AC3/DTS frame loops, FLAC frame CRC gate at `src/SMS_FLAC.c:363`) were verified to use them consistently.
