# SMS Round 7 — Vendored Codecs: SMS_OGG.c (Vorbis) + SMS_AAC.c (FAAD2)

- Repo: Simple-Media-System, pin `e65ced39a27eee8d996e9257e21594d1cac1c93d` (verified by `git rev-parse HEAD`; working tree clean before and after — read-only throughout).
- Scope honoured: Cat 6 only (OOB read/write, unchecked sizes, divides by file-controlled values, integer overflow in size math) at decode entry points reachable with crafted media, plus Cat 1 only where grep-provable tree-wide dead. No style/duplication findings. Candidates are **unnumbered** (coordinator numbers them); labels below are local (`OGG-n` / `AAC-n`).
- Relation to prior rounds: round 3's unnumbered allocation note (codebook `entries` 24-bit → unchecked `malloc` at SMS_OGG.c:3170/:3190 region) and SMS-016 are **not** re-counted here. Every candidate below is an arithmetic/index/divide defect *inside* unpack or decode, not a bare unchecked-`malloc`.
- Marking: **READ** = the defective code path was read line-by-line in this round. **INFERRED** = the sink was read, but full trigger reachability depends on a chain not executed here (no build/hardware in this VM; nothing in this section is executed-proven).

---

## Lines-read ledger (honest, per file)

### `src/SMS_OGG.c` — 5,533 lines
Deep-read (line-by-line):
- 1–225 — header comment, all in-file struct definitions (`vorbis_info_floor0/1`, `mapping0`, `residue0`, look structs, `private_state`), defines (`VI_*B`, `VIF_POSIT 63`, `VIF_CLASS 16`, `VIF_PARTS 31`), `mask[33]` table.
- 2543–3692 — `ilog/icount`, ogg sync/stream, `oggpack_read`, comment/info/codebook unpack, `_vorbis_unpack_books`, all floor/mapping/residue unpack + free + look functions (`res0_look` re-read separately at 3631–3680).
- 3693–4442 — `vorbis_synthesis_headerin`, `_make_words`, `_book_unquantize`, `vorbis_book_init_decode`, `_vds_shared_init`, `vorbis_synthesis`, `mapping0_inverse`, `oggpack_look/adv`, `decode_packed_entry_number`, `vorbis_book_decodev_set`, `floor0_inverse1`.
- 4443–5092 — `floor0_map_lazy_init`, cos/fromdB/invsq lookups, `vorbis_lsp_to_curve`, `floor0_inverse2`, `render_point/line`, `floor1_inverse1/2`, `_01inverse`, all `vorbis_book_decodev*` variants, `res0/1/2_inverse`, `vorbis_synthesis_blockin/pcmout/read`.
- 5093–5533 — SMS glue: `OGGV_Init/Destroy`, `_decode_start`, `_decode_hdr`, `_decode_pcm`, `OGGV_Decode` (the `_pcm_syn1/2` inline-asm bodies in this range were skimmed, not audited — assembly is out of this round's stated scope).
- Plus `include/SMS_OGG.h` read in full (275 lines: `codec_setup_info` array sizes — `mode_param[64]`, `book_param[256]`, etc. — are load-bearing for several candidates).

Not deep-read: 226–2448 (constant lookup/window tables: COS, FROMdB, INVSQ, `vwin64…vwin8192` float data — constants only; the `vwin[8]` pointer table itself was verified at 2449 via grep + script), 2449–2542 (function-pointer bundle declarations/prototypes, listed via grep only).

Approx. deep-read total: ~3,540 lines of code in SMS_OGG.c + full SMS_OGG.h.

### `src/SMS_AAC.c` — 8,302 lines
Deep-read (line-by-line):
- 1–586 — FAAD header, all defines (`MAX_CHANNELS 64`, `MAX_SFB 51`, `TNS_MAX_ORDER 20`, etc.) and every struct (`sbr_info`, `program_config`, `ic_stream`, `drc_info`, `_NeAACDecStruct`, hcb types, ADIF/ADTS headers).
- 2006–2105 — `num_swb_*` / `swb_offset_*` tables and head of `hcb_sf` (also machine-checked, see checked-clean A-C5).
- 3583–4282 — SBR init/end/reset, `sbr_header`, `qmf_start/stop_channel`, `master_frequency_table_fs0`, `find_bands/find_initial_power`, `derived_frequency_table`, `master_frequency_table`, `envelope_time_border_vector`, `middleBorder`, `sbr_grid`, `sbr_dtdf`, `invf_mode`, `sbr_huff_dec`, envelope extraction start.
- 4283–5082 — `sbr_envelope/noise`, `calc_Q_div/2`, `envelope_noise_dequantisation`, `sinusoidal_coding`, `sbr_single_channel_element`, `unmap_envelope_noise`, `calc_sbr_tables`, `sbr_channel_pair_element`, `sbr_data`, `sbr_extension_data`, `sbr_save_matrix/prev_data`, `estimate_current_envelope`, `get_S_mapped`, `calculate_gain`, `hf_assembly`, `hf_adjustment`.
- 5083–5742 — `mapNewBw`, `calc_chirp_factors`, `patch_construction`, `auto_correlation`, `calc_prediction_coef`, `limiter_frequency_table`, `hf_generation`, `sbr_qmf_analysis_32`, `sbr_process_channel`, `sbr_qmf_synthesis_32/64`, `sbrDecodeSingleFrame/CoupleFrame`.
- 5744–6443 — `drc_init`, `_NeAACDecOpen/Close`, `program_config_element`, `get_adif_header`, ADTS header functions, `filter_bank_init`, `_NeAACDecInit`, `GASpecificConfig`, `AudioSpecificConfig2`, `_NeAACDecInit2`, `window_grouping_info`, `ics_info`, `section_data`, `huffman_scale_factor`, `decode_scale_factors`, `pulse_data`, `tns_data`, `side_info`.
- 6444–7143 — all Huffman spectral decoders, `huffman_getescape`, `spectral_data`, `pulse_decode`, `individual_channel_stream`, `allocate_single_channel`, `quant_to_spec` (+ `vquant` asm skimmed), `pns_decode`, `ic_predict/ic_prediction`, `tns_decode_coef`, `tns_ar_filter`.
- 7144–7868 — `tns_decode_frame`, `drc_decode`, `ifilter_bank` (+ overlap asm skimmed), `reconstruct_single_channel`, `allocate_channel_pair`, `ms_decode`, `is_decode`, `data_stream_element`, `excluded_channels`, `dynamic_range_info`, `extension_payload`, `fill_element`, `reconstruct_channel_pair`, `channel_pair_element`, `decode_cpe`, `single_lfe_channel_element`, `decode_sce_lfe`.
- 7869–8302 — `raw_data_block`, `output_to_PCM`, `create_channel_config`, `_aac_decode`, `_aac_init/destroy`, `SMS_Codec_AAC_Open`.
- Plus `include/SMS_Bitio.h` read in full (152 lines — the bit reader is load-bearing for AAC-1/AAC-6) and `include/SMS.h:68–69` (`SMS_NEG_USR32/SSR32` macros).

Not deep-read: 587–2005 and 2106–3582 — constant tables only (window/KBD tables, `_aac_iq_table`, Huffman code tables, TNS coefficients, SBR Huffman/QMF tables), with spot reads of the table-size arrays at 2307, 2355, 2746 and a script count of table entries vs. declared sizes (see AAC-2 note).

Approx. deep-read total: ~5,400 lines of code in SMS_AAC.c + full SMS_Bitio.h.

---

# VORBIS — `src/SMS_OGG.c` (12 candidates)

## OGG-1 — `oggpack_read` / `oggpack_look` read up to 4 bytes past the end of a packet that is *not* long enough to trigger the overflow exit — READ
- Sinks: `src/SMS_OGG.c:2939–2975` (`oggpack_read`), mirrored at `:4302–4329` (`oggpack_look`).
- Provenance: every header field and every audio-packet field in the file flows through these two functions; `apBuf->storage` is the packet byte count (`oggpack_readinit`, :2932–2937, from `ogg_packet.bytes`, itself file-controlled via ogg lacing).
- Defect: the guard is
  `if (apBuf->endbyte + 4 >= apBuf->storage) { ret = -1; if (apBuf->endbyte*8 + anBits > apBuf->storage*8) goto overflow; }`
  When the first condition holds but the second does not (reader within the last 4 bytes, requesting few bits), execution **falls through** and unconditionally reads `ptr[0]`, and for `anBits > 8/16/24` also `ptr[1]`, `ptr[2]`, `ptr[3]` (and `ptr[4]` when `anBits > 32 && endbit`). Those bytes are past the packet buffer. There is no guard that keeps the 4-byte lookahead window inside `storage`.
- Guard: partial only (the total-bits check above); the fall-through path is unguarded.

## OGG-2 — Codebook `dim == 0` never rejected: float divide-by-zero + non-terminating quantvals search in setup — READ
- Source: `src/SMS_OGG.c:3160–3161` — `apBook->dim = oggpack_read(apBuf,16)`, `apBook->entries = oggpack_read(apBuf,24)` in `vorbis_staticbook_unpack`. The only check is `entries == -1` (:3163, the oggpack overflow sentinel). No `dim < 1` and no `entries < 1` check anywhere in the function (:3153–3239).
- Sink 1: `_book_maptype1_quantvals`, `:3128–3151`: `pow((float)entries, 1.0F / apBook->dim)` divides by `dim == 0`; the accumulation loop `for (i = 0; i < dim; ++i)` then never runs, so `acc == acc1 == 1` permanently and the `while (1)` search increments `vals` forever for any `entries >= 1` — setup never returns (hang on a crafted setup header, maptype 1).
- Sink 2 (same root, audio/setup cross-ref): `floor0_inverse1`'s `for (j = 0; j < look->m; j += b->dim)` (:4420) never advances if the floor book's `dim == 0`.
- Guard: none for `dim == 0`.

## OGG-3 — Codebook `entries * dim` (maptype 2) and maptype-1 accumulation overflow int size math — READ
- Source: same `dim` (16-bit, ≤ 65535) and `entries` (24-bit, ≤ 16777215) fields at :3160–3161.
- Sink 1: `:3219` `case 2: quantvals = apBook->entries * apBook->dim;` — signed `int` multiplication; `dim >= 129` already overflows 2^31. The wrapped (small or negative) `quantvals` sizes the `quantlist` allocation at :3223 and its fill loop, but `_book_unquantize` maptype 2 (:3822–3840) indexes `apBook->quantlist[j * apBook->dim + k]` for **all** `j < entries` — OOB read far past the undersized allocation.
- Sink 2: `_book_maptype1_quantvals` (:3135–3143): `acc *= vals` / `acc1 *= vals+1` repeated `dim` times with no overflow check; for large `dim` the products overflow signed `int` (UB) and the termination predicate `acc <= entries && acc1 > entries` is evaluated on wrapped values. `_book_unquantize` maptype 1 (:3808) similarly accumulates `indexdiv *= quantvals` unchecked.
- Guard: none on either product.

## OGG-4 — Floor1 total post count `count` unbounded: `postlist` heap overwrite in unpack, stack/heap overwrite in look — READ
- Source: `floor1_unpack`, `:3413–3462`. `partitions = oggpack_read(apBuf,5)` (≤ 31, :3420); per class `class_dim[j] = oggpack_read(apBuf,3) + 1` (≤ 8, :3432).
- Sink 1 (unpack): `:3451–3458` — `count += class_dim[partitionclass[j]]` per partition, then `info->postlist[k + 2] = oggpack_read(apBuf, rangebits)` for all `k < count`. `count` can reach 31 × 8 = **248**, but `postlist` is `int postlist[VIF_POSIT + 2]` = 65 entries (struct at :64). No `count > VIF_POSIT` check exists. Writes run up to index 249 — through the rest of the `calloc`'d `vorbis_info_floor1` and past the allocation.
- Sink 2 (look): `floor1_look`, `:3570–3630` recomputes the same total into local `n` (:3583–3588) and writes `sortpointer[i]` for `i < n` into a **stack** array `int *sortpointer[VIF_POSIT + 2]` (:3574), plus `look->forward_index/reverse_index/sorted_index[i]` (arrays of 65 in the look struct, :110–112). Same unguarded `count` drives all of them.
- Guard: none. (The per-class arrays themselves are correctly bounded — see checked-clean V-C4.)

## OGG-5 — Residue `booklist` overwrite: cascade popcount sum `acc` can reach 512 into a 256-entry array — READ
- Source: `res0_unpack`, `:3517–3549`. `partitions = oggpack_read(apBuf,6) + 1` (≤ 64, :3527); per partition `cascade = oggpack_read(apBuf,3)`, optionally `cascade |= oggpack_read(apBuf,5) << 3` (:3532–3534) — up to 8 bits set.
- Sink: `:3536` `acc += icount(cascade)` accumulates up to 64 × 8 = **512**; `:3540` `for (j = 0; j < acc; ++j) info->booklist[j] = oggpack_read(apBuf,8);` writes into `int booklist[256]` (struct at :90). Indices 256–511 overwrite `secondstages[64]`, `classmetric1/2[64+64]`, then `begin/end/grouping/partitions/groupbook` themselves (struct order at :89–99), and the final ~59 writes pass the end of the `calloc`'d struct into the heap.
- Secondary sink (same corrupted state): `res0_look` (:3648–3654) consumes `acc++` entries from `booklist` again with the same unbounded total.
- Guard: none. The later `booklist[j] >= ci->books` validation loop (:3545–3546) runs only **after** the overflowing fill loop and validates values, not the count.

## OGG-6 — Ordered codebook lengths grow past 32: `_make_words` indexes `marker[33]` by length, `oggpack_look` indexes `mask[33]` by `dec_maxlength` — READ
- Source: `vorbis_staticbook_unpack` case 1, `:3178–3195`: `length` starts at `oggpack_read(apBuf,5) + 1` and is incremented once per run (`++length`, :3193); the number of runs is bounded only by `entries` (24-bit). Resulting `lengthlist[]` entries can be arbitrarily large (≫ 32).
- Sink 1: `_make_words`, `:3721–3768`: `unsigned int entry = marker[length];` at :3730 reads `marker[33]` (stack array, :3724) **before** the `length < 32` test at :3731; the subsequent loops write `marker[j]` for `j` up to `length` (:3735–3743). Reached from `vorbis_book_init_decode` (:3868) during `vorbis_synthesis_init`.
- Sink 2: `vorbis_book_init_decode` sets `dec_maxlength` to the largest lengthlist entry (:3930–3932); `decode_packed_entry_number` then calls `oggpack_look(apBuf, read)` with `read = dec_maxlength` (:4342, :4366), and `oggpack_look` indexes `mask[anBits]` (`mask` has 33 entries, :148–156) and shifts by `32 - anBits`-family amounts — both out of range for `anBits > 32`. Same width reaches `oggpack_read` callers via `q_quant`-bounded reads only — not claimed for those.
- Guard: the `length < 32` test exists but only after the first `marker[length]` read, and nothing caps `length` at unpack time.

## OGG-7 — Blocksize field permits window-table index −3…9 into `vwin[8]` — READ
- Source: `_vorbis_unpack_info`, `:3104–3118`: `blocksizes[0] = 1 << oggpack_read(apBuf,4)`, `blocksizes[1] = 1 << oggpack_read(apBuf,4)` — values 8…32768 are all reachable. The only guards are `blocksizes[0] < 8` and `blocksizes[1] < blocksizes[0]` (:3115–3116). No lower bound of 64 and no upper bound of 8192.
- Chain: `_vds_shared_init`, `:3989–3990`: `window[i] = ilog(blocksizes[i] - 1) - 6` yields −3 (bs 8), −2 (16), −1 (32), 8 (16384), 9 (32768). `_vorbis_window_get(aN)` (:4921–4923) returns `vwin[aN]`; `vwin` has exactly 8 entries (`vwin64…vwin8192`, :2449–2453, verified).
- Sink: `vorbis_synthesis_blockin` calls `_vorbis_window_get(b->window[1] - hs)` / `(b->window[0] - hs)` (:4980–5013) and dereferences the returned pointer as the window table — OOB read of a function-scope-external pointer, then reads through it.
- Guard: none tying blocksize to the 64…8192 window-table range.

## OGG-8 — Audio packet `mode` field never checked against `modes`: NULL/out-of-setup `mode_param` dereference — READ
- Source: `vorbis_synthesis`, `:4157–4204`. `mode = oggpack_read(opb, b->modebits)` (:4177), where `modebits = ilog(ci->modes - 1)` (:3968). The only check is `mode == -1` (:4179).
- Defect: `modebits` bits can encode values `>= modes` whenever `modes` is not a power of two (e.g. `modes = 5` → `modebits = 3` → packet values 5, 6, 7). `:4182` immediately dereferences `ci->mode_param[mode]->blockflag`; `codec_setup_info` is `calloc`'d, so unallocated slots are NULL — NULL dereference (and for the SMS context struct, a wild read through a zero pointer). Values stay inside the `mode_param[64]` array bounds, so this is a NULL/unallocated-slot deref, not an array overflow — stated as such.
- Guard: none (`mapping` inside each *valid* mode is validated at unpack, :3299–3301 — that check does not cover an out-of-range `mode`).

## OGG-9 — Residue `begin`/`end`/`grouping` (24-bit each) never validated against the block size: residue decode writes at file-controlled offsets into the per-channel PCM buffer — READ (sink) / INFERRED (full trigger)
- Source: `res0_unpack`, `:3524–3526`: `begin = oggpack_read(apBuf,24)`, `end = oggpack_read(apBuf,24)`, `grouping = oggpack_read(apBuf,24) + 1`. No check relates any of them to `ci->blocksizes[]` (max 32768, PCM channel buffers are `pcmend = blocksizes[W]` floats, :4196–4200) at unpack, look, or decode time.
- Sink 1: `_01inverse`, `:4715–4755`: `n = end - begin`; `samples_per_partition = grouping`; `offset = info->begin + i * samples_per_partition` (:4741) is passed to the decode-part function as `appIn[j] + offset`, and `vorbis_book_decodev_add` / `vorbis_book_decodevs_add` write `samples_per_partition` floats starting there (:4768–4777, :4802–4831). A `begin` of e.g. 1,000,000 with a set stage bit writes ~16 MB past a ≤ 8192-float channel buffer.
- Sink 2: `res2_inverse`, `:4883–4919` computes the same values and calls `vorbis_book_decodevv_add` with `i * samples_per_partition + info->begin` (:4908–4911).
- Also: `end < begin` makes `n` and `partvals = n / samples_per_partition` negative (:4719), feeding `(partvals + partitions_per_word - 1) / partitions_per_word` and, in `res2_inverse`, `_vorbis_block_alloc(apBlock, partwords * sizeof(...))` (:4893) with a negative byte count that the allocator's `anBytes + localtop > localalloc` arithmetic was not written for.
- Guard: none found in `res0_unpack`, `res0_look`, `_01inverse`, or `res2_inverse`.
- Trigger note (INFERRED): a write requires an audio packet whose classification selects a stage book for an affected partition; the offset arithmetic itself is fully read and unchecked.

## OGG-10 — `res0_look` `partvals = pow(parts, dim)` overflows `int` / wraps the decodemap allocation size — READ
- Source: `res0_look`, `:3631–3675`. `parts = info->partitions` (≤ 64, from `res0_unpack` :3527); `dim = look->phrasebook->dim` (:3646) — the group book's 16-bit `dim` field (see OGG-2/OGG-3 for its lack of validation).
- Sink: `:3657` `look->partvals = rint(pow((float)parts, (float)dim));` — `parts = 64, dim = 6` already gives 2^36; conversion of the out-of-range float to `int` is UB and in practice yields a negative or truncated value. `:3659` `malloc(look->partvals * sizeof(*look->decodemap))` then either wraps to a small allocation on the 32-bit target (e.g. `dim = 5, parts = 64` → partvals 2^30 × 4 B wraps to 0) or is negative-as-`size_t` huge, while the fill loops at :3661–3672 and later consumers trust `partvals` independently — allocation size and logical size are decoupled by the overflow.
- Guard: none; no relation is ever checked between `parts^dim`, the allocation, and the phrasebook's `entries`.

## OGG-11 — `decodemap[temp]` indexed by a phrasebook decode result with no `temp < partvals` check — READ
- Chain: audio packet → `vorbis_book_decode(look->phrasebook, &opb)` returns `dec_index[packed_entry]`, an index into the phrasebook's entry space (`entries` is the 24-bit setup field, up to 16,777,215, independent of `partvals`).
- Sink 1: `_01inverse`, `:4734`: `partword[j][l] = look->decodemap[temp];` — `decodemap` has `partvals` entries (OGG-10). A small legitimate-shaped configuration already overflows: `partitions = 2, dim = 1` → `partvals = 2`, while a phrasebook with e.g. 100 entries returns `temp` up to 99.
- Sink 2: `res2_inverse`, `:4900`: identical `partword[l] = look->decodemap[temp];`.
- The result is not just read: the fetched (out-of-bounds) pointer is dereferenced at :4742/:4904 as `partword[…][k]`.
- Guard: only `temp == -1` and `decodemap[temp] == NULL` are checked (:4733–4735, :4899–4901); the NULL test itself dereferences the unchecked index first.

## OGG-12 — Floor0 `order == 1` passes unpack, then `vorbis_lsp_to_curve`'s do-while decrements its counter through zero and walks `lsp` out of bounds — READ (sink) / INFERRED (trigger)
- Source: `floor0_unpack`, `:3392` `info->order = oggpack_read(apBuf,8)`; the guard at :3399 is only `order < 1`. `order == 1` is accepted and becomes `look->m` in `floor0_look` (:3558).
- Sink: `vorbis_lsp_to_curve`, `:4486–4528`, called from `floor0_inverse2` (:4543–4548) with `aM = look->m`. At :4505–4512: `int c = aM >> 1;` (= 0 for `aM == 1`), then `do { q *= ftmp[0] - w; p *= ftmp[1] - w; ftmp += 2; } while (--c);` — the do-while body executes once unconditionally, `--c` yields −1 (truthy), and the loop repeats indefinitely, reading `ftmp[0]/ftmp[1]` ever further past the `lsp` block allocation (`m + dim + 1` floats, allocated at :4415).
- Guard: none on `aM < 2` (or on evenness of the LSP order) in unpack, look, or inverse2.
- Trigger note (INFERRED): requires a floor0 packet for which `floor0_inverse1` returns an `lsp` memo (ampraw > 0 and a successful book decode) — the counter defect itself is fully read.

---

# AAC — `src/SMS_AAC.c` (7 candidates)

## AAC-1 — The bit reader has no end-of-buffer check at all: every `SMS_GetBits`/`SMS_GetBit` reads through a 4-byte window at the current index regardless of `m_szInBits` — READ
- Sink: `include/SMS_Bitio.h:26–40` (`SMS_UPDATE_CACHE` loads `SMS_unaligned32_be(m_pBuf + (index >> 3))` — an unconditional 4-byte read at the current byte position), `:70–78` (`SMS_GetBits`), `:90–97` (`SMS_GetBit` reads `m_pBuf[lIdx >> 3]`). No function in the header ever compares `m_Idx` against `m_szInBits`.
- Provenance: `SMS_InitGetBits(&ld, buffer, buffer_size << 3)` in `_aac_decode` (:8183) with `buffer_size` from the container packet; all element parsers (section/scale-factor/Huffman/SBR/DRC) consume from `ld` without any per-parser budget.
- The one apparent guard: `raw_data_block`'s loop condition `ld->m_Idx < ld->m_szInBits` (:7884) is evaluated only *between* syntax elements, after an element id is read; a single element's internal parsing (e.g. one CPE's Huffman spectral data, one SBR grid + envelope) can run arbitrarily far past the buffer end before control returns to that check. Near the end of any well-formed frame the 4-byte cache load already reads up to 3 bytes past the allocation; a truncated/corrupt element reads unboundedly far.
- Guard: none in the reader; the `raw_data_block` check does not bound intra-element consumption.

## AAC-2 — Two-step Huffman decoders use the file-influenced `offset` to index the code table *before* the size check — READ
- Sinks: `huffman_2step_quad`, `:6452–6473`, and `huffman_2step_pair`, `:6519–6538` (identical structure).
- Chain: `cw` = next `hcbN[cb]` bits of the file stream → `offset = hcb_table[cb][cw].offset`, `extra_bits = hcb_table[cb][cw].extra_bits` (:6459–6460). If `extra_bits`, the file's next `extra_bits` bits are **added** to `offset` (:6463). `offset` is then used at :6464 / :6531 — `hcb_2_quad_table[cb][offset].bits` (resp. pair) as the argument to `SMS_SKIP_BITS` — **before** the only range check, `if (offset > hcb_2_*_table_size[cb]) return 10;`, at :6467 / :6534. The subsequent `sp[]` reads at :6468–6471 / :6535–6536 also use the unchecked value when the check is not taken.
- Why reachable: the first-stage table assigns each `cw` a segment base + width; nothing constrains the file's extra bits to the valid codeword sub-range, so `offset` can be driven past the segment and past the table end (declared sizes read at :2307/:2746, e.g. 114/185 quad, 126/210/373 pair).
- Note on a dissolved sub-claim: an earlier suspicion that the check is also off-by-one (`>` vs `>=`) was **withdrawn** — a script count of table entries vs. the declared sizes is consistent with the sizes being maximum valid indices, so only the check-after-use defect is claimed.
- Guard: present but mis-ordered (after the first dereference).

## AAC-3 — `section_data`: zero-length sections increment `i` without advancing `k`; the per-section arrays are written at `i == 120` *before* the `i` bound check fires (short windows) — READ
- Sink: `section_data`, `:6288–6330`.
- Chain (EIGHT_SHORT_SEQUENCE): `sect_len` is built from file bits at :6305–6312 and can be **0** (a codebook nibble plus a zero length increment). The loop `while (k < ics->max_sfb)` (:6296) then does not advance `k`, but `i` increments every iteration (:6322–6323). Per iteration the writes come first: `ics->sect_cb[g][i]` (:6300), `ics->sect_start[g][i]` / `sect_end[g][i]` (:6314–6315); the bounds tests `k + sect_len > 8 * 15` and `i >= 8 * 15` are only evaluated at :6317–6319, i.e. after the writes for that `i`.
- Result: after 120 zero-length sections (all bits suppliable inside a normally-sized packet: each iteration consumes ≥ 7 bits), iteration `i == 120` writes one element past `sect_cb[8][120]`, `sect_start[8][120]` and `sect_end[8][120]` (`ic_stream` layout at SMS_AAC.c:395–412 — these land in the following struct fields, an intra-struct overwrite) before the function returns 15.
- Guard: present but mis-ordered for the array writes. (The long-window branch was checked and is *not* claimed: its `i >= MAX_SFB (51)` test also fires after the write, but index 51 is still inside the 120-wide arrays — see checked-clean A-C2.)

## AAC-4 — PCE channel counts: the `channels > MAX_CHANNELS` check runs after construction and its result is ignored by two of three callers; downstream `internal_channel[]` / `channel_position[]` writes are indexed by the unchecked totals — READ
- Source: `program_config_element`, `:5812–5909`. Element counts are file bits: front/side/back 4 bits each (≤ 15 elements, ≤ 30 channels each), LFE 2 bits (:5821–5826). `pce->channels` can reach 30 + 30 + 30 + 3 = **93**; per-element it stores `pce->cpe_channel[tag] = pce->channels` / `sce_channel[tag] = pce->channels` with the running total (:5845–5890). The only validation, `if (pce->channels > MAX_CHANNELS) return 22;`, is at :5905 — after all of it.
- Ignored returns: `get_adif_header` calls it at :5931 with no return check (and `_NeAACDecInit` then copies `adif.pce[0]` into `hDecoder->pce` and sets `pce_set`, :6030–6038); `raw_data_block` case `ID_PCE` calls it at :7917 with no return check. (Contrast: `GASpecificConfig` at :6072 *does* check and is not claimed.)
- Sink 1: `decode_cpe`, `:7808–7809`: `hDecoder->internal_channel[hDecoder->pce.cpe_channel[tag]]` and `[… + 1]` — `internal_channel` is `uint8_t[MAX_CHANNELS]` = 64 entries (`_NeAACDecStruct`, :488ff in-file struct; MAX_CHANNELS at :115), the index can be up to 92.
- Sink 2: `create_channel_config`, `:7997–8036` (pce_set branch): `hInfo->channel_position[chpos++]` is written once per front/side/back/LFE channel counted from the PCE (up to ~93) into `channel_position[MAX_CHANNELS]` in the stack-resident `_NeAACDecFrameInfo` in `_aac_decode`.
- Guard: the :5905 check exists but is post-construction and unenforced at two call sites; neither sink re-checks.

## AAC-5 — `excluded_channels`: the additional-excluded-channels loop is bounded only by a file bit — `exclude_mask[64]` / `additional_excluded_chns[64]` walked without limit — READ
- Sink: `excluded_channels`, `:7548–7562`.
- Chain: fill element → `extension_payload` case `EXT_DYNAMIC_RANGE` (:7610–7613) → `dynamic_range_info` with `excluded_chns_present == 1` (:7576) → here. `while ((drc->additional_excluded_chns[n - 1] = SMS_GetBit(ld)))` — each iteration whose file-supplied bit is 1 writes 7 more entries `drc->exclude_mask[i]` for `i` in `[num_excl_chan, num_excl_chan + 7)` and increments both `n` and `num_excl_chan` by 7 (:7553–7560). Both arrays are `uint8_t[MAX_CHANNELS]` = 64 (`drc_info`, in-file struct at :300–318). After 9 continuation rounds the mask index passes 63; `n - 1` itself passes 63 at the 10th. Nothing caps the loop except the stream bit turning 0.
- Guard: none (no `n`/index bound anywhere in the function).

## AAC-6 — `huffman_getescape`: the escape-length counter `i` is incremented by a file-controlled run of 1-bits with no cap, then used as a bit-read width and a shift count — READ
- Sink: `huffman_getescape`, `:6559–6575`, reached for codebook-11 spectral values of ±16 (`huffman_spectral_data` case 11, :6598–6602).
- Chain: `for (i = 4; ; ++i) if (!SMS_GetBit(ld)) break;` (:6570) — a crafted run of `1` bits makes `i` arbitrarily large. Then `off = SMS_GetBits(ld, i)` (:6571) and `j = off | (1 << i)` (:6572).
- Defects at `i >= 32`: `SMS_GetBits` computes `SMS_NEG_USR32(cache, aN)` = `cache >> (32 - aN)` (`include/SMS.h:68`) — a negative shift count, UB; `1 << i` at :6572 is likewise a ≥ 32 shift of a 32-bit `int`, UB. Independently, the reader index has by then advanced far past the buffer (AAC-1), but the width/shift defect is distinct: even a reader with buffer bounds would still be handed an illegal width.
- Guard: none on the run length (the AAC escape sequence is spec-bounded in valid streams; nothing here enforces it).

## AAC-7 — ADTS/ADIF `sf_index` (4 bits, values 12–15) stored unchecked; `window_grouping_info` indexes 12-entry sample-rate tables by it — READ (tables/guard asymmetry) / INFERRED (secondary ASC path)
- Sources: ADTS `sf_index = SMS_GetBits(ld,4)` (:5953) stored verbatim at `_NeAACDecInit` :6041 (`hDecoder->sf_index = adts.sf_index;`); ADIF path stores `adif.pce[0].sf_index` (itself a 4-bit PCE field, :5821) at :6030. Neither init path rejects 12–15: `get_sample_rate` returns 0 for them (:5763–5770 guards `sr_index < 12`) but that 0 is only reported as the sample rate, not treated as an error.
- Sink: `window_grouping_info`, `:6194–6249`: `ics->num_swb = num_swb_1024_window[sf_index]` (:6205) and `swb_offset_1024_window[sf_index][i]` (:6207–6209), and the 128-window equivalents (:6220–6223) — all four tables have exactly 12 entries (:2006–2100). `sf_index` 12–15 reads past the count table and past the pointer table, then dereferences the wild pointer in the copy loop.
- Guard asymmetry (evidence this is an omission, not a design): neighbouring consumers of the same field *do* guard — `get_sample_rate` (`< 12`), `max_pred_sfb` (`< 12`, :6249–6255). `window_grouping_info` does not.
- Secondary path (INFERRED): `AudioSpecificConfig2` can leave `samplingFrequencyIndex == 15` (explicit-frequency form) after the SBR branch at :6107/:6134, stored to `hDecoder->sf_index` at :6172; the :6180 recompute only replaces it when SBR/upsampling applies.

---

## Cat 1 (tree-wide dead) — none found in either file

Method: every `static` function defined in `src/SMS_OGG.c` and `src/SMS_AAC.c` was extracted and its name counted across the whole tree (`src/` + `include/`). Every name occurs at least 3 times (declaration/prototype or function-pointer bundle registration + definition + at least one call/registration use) — e.g. the Vorbis floor/residue/mapping functions are reachable through the `_floor_P[]` / `_residue_P[]` / `_mapping_P[]` bundles, and the AAC Huffman variants through the `huffman_spectral_data` switch. No grep-provable tree-wide dead static function exists in either file, so no Cat 1 candidate is claimed.

---

## Checked-clean list (candidates examined and dissolved, with reasoning — no filler)

### Vorbis
- **V-C1 — `_vorbis_unpack_books` count fields.** `books = read(8)+1 ≤ 256` vs `book_param[256]`; `floors/residues/maps/modes = read(6)+1 ≤ 64` vs the `[64]` arrays in `codec_setup_info` (SMS_OGG.h:189–206); every type field is range-checked against `VI_TIMEB/VI_FLOORB/VI_RESB/VI_MAPB` (:3259–3287) and each mode's `mapping` against `ci->maps` (:3301). Dissolved: counts and types are all bounded before use.
- **V-C2 — `mapping0_unpack` arrays.** `coupling_steps = read(8)+1 ≤ 256` vs `coupling_mag/ang[256]`; `submaps = read(4)+1 ≤ 16` vs `floorsubmap/residuesubmap[16]`; `chmuxlist` loop bounded by `channels ≤ 255` vs `chmuxlist[256]`; coupling indices checked `< channels` and `mag != ang` (:3493–3497); submap indices checked against `ci->floors/ci->residues` (:3512–3515). Dissolved.
- **V-C3 — `floor0_unpack` book list.** `numbooks = read(4)+1 ≤ 16` vs `books[16]`, each book index checked `< ci->books` (:3404–3407); `order/rate/barkmap` all required `>= 1` (:3399–3402). Dissolved for the unpack itself (the surviving `order == 1` problem is downstream in the LSP curve — OGG-12).
- **V-C4 — `floor1_unpack` per-class arrays.** `partitions ≤ 31` vs `partitionclass[VIF_PARTS = 31]`; `maxclass ≤ 15` vs the `[VIF_CLASS = 16]` class arrays; subbook loop `k < (1 << class_subs) ≤ 8` vs `class_subbook[][8]`; `class_book`/`class_subbook` values checked against `ci->books` (:3436–3445). Dissolved — only the *summed* post count is unguarded (OGG-4).
- **V-C5 — `res0_unpack` `secondstages` array.** `partitions ≤ 64` vs `secondstages[64]`, indexed strictly by the partition loop (:3531–3537). Dissolved for that array (the popcount *sum* over it is OGG-5).
- **V-C6 — Ogg lacing/body path (`ogg_stream_pagein`, `_ogg_packetout`).** `segments ≤ 255` (header byte) and `_os_lacing_expand(apSS, segments + 1)` runs before any `lacing_vals` write (:2817); body storage is expanded by exactly `bodysize` before the `memcpy` (:2865–2869); `_ogg_packetout`'s 255-chain walk is bounded by `lacing_packet/lacing_fill` bookkeeping. No new Cat 6 arithmetic found on this path.
- **V-C7 — `decode_packed_entry_number` binary search.** `lo/hi` stay within `used_entries`; `dec_codelengths[lo]` is compared `<= read` before `oggpack_adv` consumes it (:4376–4381). Dissolved apart from the `read > 32` width problem, which is charged to its root in OGG-6.
- **V-C8 — Comment unpack lengths.** `_vorbis_unpack_comment` (:3046–3078) checks each 32-bit length `< 0` after reading (oggpack overflow sentinel) before allocating/copying. The remaining exposure there is unchecked-allocation (NULL) handling, which is the prior rounds' allocation surface, not new Cat 6 arithmetic — not re-counted.

### AAC / FAAD2
- **A-C1 — `window_grouping_info` `max_sfb` validation.** Both branches test `ics->max_sfb > ics->num_swb` and return 32 (:6204, :6221), and `ics_info` re-tests it (:6269). Dissolved for `max_sfb`; the unguarded input to the same lines is `sf_index` itself (AAC-7).
- **A-C2 — `section_data` long-window `i` bound.** As noted in AAC-3, the `i >= MAX_SFB` test fires after the write, but the arrays are 120 wide while the test trips at 51 — the write at `i == 51` is in-bounds. Dissolved for long windows; only the short-window `i == 120` case (AAC-3) overflows.
- **A-C3 — Pulse path.** `pulse_data` rejects `pulse_start_sfb > num_swb` (:6394); `number_pulse ≤ 3` vs `pulse_offset/amp[4]`; `pulse_decode` re-checks `k >= 1024` and returns 15 **before** each `spec_data[k]` access (:6650–6655), with `k` starting from a `min(..., swb_offset_max)` clamp. Dissolved.
- **A-C4 — TNS order.** `tns_data` stores `order` up to 31 (5-bit field, long windows), but `tns_decode_frame` clamps `tns_order = min(order, TNS_MAX_ORDER = 20)` at :7144 before `tns_decode_coef` indexes its `[TNS_MAX_ORDER + 1]` work arrays; `n_filt` is bit-bounded (≤ 3 long / ≤ 1 short) vs `length/order[8][4]` second index ≤ 3 in practice by the same field widths, and `coef[…][32]` holds the ≤ 31 stored coefficients. Dissolved.
- **A-C5 — `quant_to_spec` 4-wide asm step vs band widths.** The loop steps `bin += 4` and `vquant`/`sqc2` always move 4 values, which would straddle a band end if any scalefactor-band width were not a multiple of 4. Machine-checked **all 13** `swb_offset_*` tables (1024 and 128 families, :2010–2100): every successive difference is divisible by 4, and the totals are exactly 1024/128 — the 4-wide step never crosses a band or buffer end. Dissolved (verified computationally, not by eyeball).
- **A-C6 — `huffman_scale_factor` table walk.** `offset += hcb_sf[offset][b]` is followed **inside** the loop by `if (offset > 240) return -1;` (:6336–6339) before the next `hcb_sf[offset][1]` test read. Dissolved.
- **A-C7 — Binary Huffman tree walks (`huffman_binary_quad` :6475–6487, `huffman_binary_pair` :6507–6517).** The walk offset is advanced only by `data[b]` values taken from the static tree tables themselves; by construction of those tables every non-leaf step stays inside the tree, and the post-loop `offset > hcb_bin_table_size[cb]` check is a backstop. Unlike AAC-2, no file-controlled quantity is *added* to the offset. Dissolved.
- **A-C8 — `dynamic_range_info` band count (the non-DRC part of AAC-5's neighbourhood).** `num_bands = 1 + band_incr ≤ 16` vs `band_top[17]`, `dyn_rng_sgn/ctl[17]` (:7581–7599). Dissolved; only `excluded_channels` in the same call chain is unbounded.
- **A-C9 — ADIF PCE loop and PCE internal construction.** `get_adif_header` loops `i <= num_program_config_elements ≤ 15` into `adif->pce[16]` (:5927–5933) — exactly in bounds; inside `program_config_element`, all per-element arrays are indexed by the loop counter (≤ 15) or by 4-bit tag values (≤ 15) against `[16]` arrays. Dissolved for construction; the defect is the unchecked *totals* and their downstream use (AAC-4).
- **A-C10 — SBR `master_frequency_table` `temp1[bs_freq_scale - 1]` index.** `bs_freq_scale == 0` looks like an index −1, but the sole caller `calc_sbr_tables` (:4546–4552) routes `freq_scale == 0` to `master_frequency_table_fs0` and calls `master_frequency_table` only with 1–3, so the index is 0–2 into `temp1[3]`. Dissolved.
- **A-C11 — SBR `derived_frequency_table` noise-band division.** The interpolation `i + (N_low - i) / (N_Q + 1 - k)` loops `k ≤ N_Q`, so the denominator is ≥ 1 at every step (:3952–3958); `find_bands`' `a0 = kx` argument is ≥ the `qmf_start_channel` minimum for every sample-rate row, so no zero divisor was provable. Dissolved.
- **A-C12 — SBR grid border arithmetic.** `envelope_time_border_vector` returns 1 on both border underflow (`border < bs_rel_bord`, FIXVAR/VARVAR) and overflow past `numTimeSlotsRate + tHFGen` (VARFIX/VARVAR) (:4083–4125), and `sbr_grid` restores the saved `L_E/L_Q/frame_class` on that error (:4240–4246); `t_E_temp[6]` is indexed at most by `L_E ≤ 5`. `estimate_current_envelope` guards `div == 0.0F → 1.0F` before both divisions (:4770, :4809). Dissolved.
- **A-C13 — SBR `patch_construction` `f_master` scan.** The `for (i = 0; f_master[i] < goalSb; ++i)` loop (:5131) looks unbounded, but it only runs in the `goalSb < kx + M` branch, where `f_master[N_master] == k2 == kx + M > goalSb` is a guaranteed sentinel, so the scan stops at or before `N_master ≤ 64`. Dissolved.
- **A-C14 — `sbr_extension_data` bit-count reconciliation.** Consumed SBR bits are measured from `ld->m_Idx` before/after `sbr_data` and, if the element overran its declared `8 * cnt` budget, the index is rewound and an error returned (:4713–4719). This bounds the *element as a whole* (it does not fix intra-element reads — AAC-1 — but the accounting itself is sound). Dissolved as an accounting candidate.

---

## Summary counts

- **Vorbis (SMS_OGG.c): 12 candidates** — OGG-1 oggpack near-end OOB read; OGG-2 codebook `dim == 0`; OGG-3 `entries × dim` overflow; OGG-4 floor1 post-count overflow (unpack + look); OGG-5 residue `booklist` popcount overflow; OGG-6 codeword length > 32 (`_make_words` + `mask[]`); OGG-7 blocksize → `vwin[]` index −3…9; OGG-8 unchecked packet `mode`; OGG-9 residue `begin/end/grouping` vs block size; OGG-10 `partvals = pow(parts, dim)` overflow; OGG-11 `decodemap[temp]` unchecked; OGG-12 floor0 `order == 1` LSP loop.
- **AAC (SMS_AAC.c): 7 candidates** — AAC-1 unbounded bit reader; AAC-2 two-step Huffman check-after-use; AAC-3 `section_data` zero-length-section write-before-check; AAC-4 PCE channel totals ignored/unchecked downstream; AAC-5 `excluded_channels` unbounded loop; AAC-6 `huffman_getescape` unbounded escape width; AAC-7 `sf_index` 12–15 into 12-entry tables.
- **Cat 1: 0** in either file (method in the Cat 1 section above).
- **Checked-clean: 8 Vorbis + 14 AAC** dissolved items, listed with reasoning above.
- Repository untouched: no edits, no builds, `git status` clean at pin `e65ced39a27eee8d996e9257e21594d1cac1c93d`.
