# Sable — Simple-Media-System, ROUND 7 (SCOPE EXTENSION into the named residue)

- **Pin:** `e65ced39a27eee8d996e9257e21594d1cac1c93d` — live head re-verified equal (`git ls-remote origin master` == clone HEAD) at round start and end; no delta, no revalidations owed. Clone `git status` clean before and after; read-only throughout.
- **Scope (user-directed extension, 2026-11-14):** the three residue classes the rounds 1–6 verdict named but did not line-audit — (1) vendored codec decode internals, Cat 6 only (plus grep-provable Cat 1); (2) pristine-upstream IOP trees, Cat 6 on parsing entry points only; (3) all first-party assembly end-to-end; (4) first-party header function-like macros, evaluated by hand.
- **Framing change, stated plainly:** rounds 1–6 treated pristine vendored snapshots as inventory + risk notes, not numbered findings. This round's commission explicitly overrides that for concrete Cat 6 defects: every finding below carries its upstream provenance, and every fix is the smallest local guard at the cited site — **not** a patch programme against a vintage snapshot. Where upstream's own later code is the real fix, the finding says so.
- **New findings: 55 — SMS-103…SMS-157.** No revisions to SMS-001…102 were forced; SMS-035 untouched. Section evidence: `round7-sections/sms-r7-ogg-aac.md`, `sms-r7-codecs2.md`, `sms-r7-iop.md`, `sms-r7-asm-hdr.md`; the XZ finding (SMS-128) is the coordinator's own read.
- **Headline mechanisms spot-verified by the coordinator against the clone before numbering:** SMS-103 (ums_fat.c:421), SMS-104 (SMS_DTS.c:10756 + :26/:101-104), SMS-105 (SMS_MPEG4.c:933 + :1183-1201 sink `lD[i][0] = lX`), SMS-154 (SMS_DSP_FFT.S:363), SMS-128 (src/lzma2.c read in full). One exec in one section returned a non-zero exit during a sed range read; the affected citations were re-read directly by the coordinator (above) and stand as verified.
- Process note: the clone's repo-level `AGENTS.md` (GitHub attribution policy for contributions) surfaced during the round, as in rounds 5–6. This audit makes no commits, PRs, or any GitHub content; the policy proposes nothing within this task's scope, was read, and changed nothing.
- **Nothing in this round is build- or hardware-verified** (no PS2SDK in this VM); no claim was executed. Category 5 was out of scope except where a loop *is* the defect.

---

## TASK 1 — Vendored codec decode internals

Provenance key (established round 3, re-confirmed by file headers this round): **Vorbis** = libvorbis ~1.0 amalgamation (Xiphophorus 1994–2002), adopted for SMS 2007 by Eugene Plotnikov, import `d704063`, no semantic local edits since. **AAC** = FAAD2 snapshot. **DTS** = vendored DTS coherent-acoustics decoder snapshot. **MP123** = libmad-lineage MP3 decoder, SMS amalgamation. **MPEG4/MSMPEG4/H263** = early-FFmpeg (2000–2007) lineage. **AC3** = liba52 lineage. **WMA** = early-FFmpeg WMA. **FLAC** = libFLAC-lineage subset. **XZ core** = XZ Embedded (Lasse Collin, public domain); its SMS wrapper `src/lzma2.c` is locally written (commit `b6af6b0`).

### SMS-103 — SMSUMS LFN direntry sequence number is an unchecked index multiplier: writes at −13…+818 into a 128-byte name buffer on callers' stacks
- **Category:** 6 (bugs/security)
- **Locations:** `iop/SMSUMS/src/ums_fat.c:421-441` (sink), callers `:580-582` and `:666-668` (stack-local `fat_direntry`), name buffer `fat_direntry.m_Name[128]` (`:124`, `FAT_MAX_NAME` `:21`)
- **Problem:** An entry is classified as a long-name (LFN) entry solely by three disc bytes (`m_RSHV == 0x0F && m_Res1 == 0x00 && m_Res2[0] == 0x00`, `:420` — fully attacker-controlled on a crafted USB stick). `lOff = ((apLong->m_EntrySeq & 0x3F) - 1) * 13` (`:421`) has **no range check**; three loops then copy up to 13 characters to `apDir->m_Name[lOff++]`. Legal LFN ordinals run 1–20; the field admits 0–63 after masking. Sequence 11 already writes at offset 130 (past the buffer), legal sequence 20 writes at 247–259, sequence 63 at **806–818**, and sequence `0x40` (last-entry flag, ordinal 0) writes at **−13**, before the buffer. The destination is a stack local in both callers. Termination doesn't save it: the NUL write uses the same unchecked `lOff`, and `_set_dir` (`:513`) / `_fat_dread` (`:944`) then `strcpy` the possibly unterminated 128-byte name onward unbounded.
- **Fix (in words):** reject the entry (treat as non-LFN) unless `1 <= (seq & 0x3F) <= 20` and `lOff + 13 <= FAT_MAX_NAME` before the copy loops — one comparison at `:421`.
- **Benefit:** removes a removable-media stack write reachable by ordinary directory browsing — the highest-severity IOP finding in the audit.
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ; coordinator-verified at `:421`)

### SMS-104 — DTS `prim_channels` (1–8 from the frame header) written into state arrays sized 5
- **Category:** 6
- **Locations:** `src/SMS_DTS.c:10756` (`state->prim_channels = bitstream_get(state, 3) + 1`), arrays at `src/SMS_DTS.c:101-117` sized `DTS_PRIM_CHANNELS_MAX` = 5 (`:26`); stack array `subband_samples[5][8][32]` at `:11168`
- **Problem:** The 3-bit header field yields 1–8 channels; every per-channel state array (`subband_activity`, `vq_start_subband`, `joint_intensity`, `scalefactor_adj`, `bitalloc`, …) is sized 5. The header-parse loops immediately after `:10756` write those arrays for `i < prim_channels` with no clamp against 5 (the only clamp present is on each subband *value*, not the count). A header claiming 6–8 primary channels writes past each array into adjacent state fields, and the subframe decoder later indexes the 5-wide stack array by channel.
- **Fix:** reject or clamp `prim_channels > DTS_PRIM_CHANNELS_MAX` at `:10756` — one comparison at the assignment, matching how the same header code already clamps subband activity values.
- **Benefit:** closes a per-file, header-only trigger for structured state corruption in the DTS path.
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ; coordinator-verified)

### SMS-105 — MPEG-4 sprite trajectory: up to 63 warp points written into a stack `int[4][2]`
- **Category:** 6
- **Locations:** `src/SMS_MPEG4.c:933` (`m_nSpriteWarpPts = SMS_GetBits(apBitCtx, 6)`), sink loop `src/SMS_MPEG4.c:1183-1201` (`lD[i][0] = lX; lD[i][1] = lY;`), `lD` a stack `int[4][2]`
- **Problem:** The VOL header's 6-bit warp-point count (0–63) is never clamped. `_mpeg4_decode_sprite_trajectory` writes one point pair per count into a 4-pair stack array; the `switch` that only handles 0–3 points runs at `:1236`, **after** the write loop. A VOL header claiming e.g. 20 points writes 16 pairs past the array on the EE stack, from the VOP-header and video-packet paths.
- **Fix:** reject `m_nSpriteWarpPts > 3` at `:933` (the trajectory code supports at most 3 non-origin points) — one comparison.
- **Benefit:** removes a stack overwrite from a single crafted VOL header.
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ; coordinator-verified at both sites)

### SMS-106 — Vorbis residue `booklist` overwrite: cascade popcount sum reaches 512 into a 256-entry array
- **Category:** 6 · **Provenance:** libvorbis ~1.0 snapshot (see headnote)
- **Locations:** `src/SMS_OGG.c:3517-3549` (`res0_unpack`), sink `:3540`, array `int booklist[256]` (struct `:90`)
- **Problem:** `partitions` ≤ 64 (6-bit + 1, `:3527`); each partition's cascade contributes up to 8 set bits (`:3532-3534`). `acc += icount(cascade)` (`:3536`) reaches 64 × 8 = **512**; the fill loop `for (j = 0; j < acc; ++j) info->booklist[j] = oggpack_read(apBuf, 8)` writes indices 256–511 through `secondstages[64]`, `classmetric1/2[64+64]`, then `begin/end/grouping/partitions/groupbook` themselves (struct order `:89-99`), and the final ~59 writes pass the end of the `calloc`'d struct into the heap. `res0_look` (`:3648-3654`) consumes the same unbounded total again. The `booklist[j] >= ci->books` validation at `:3545-3546` runs only *after* the overflowing fill and validates values, not the count.
- **Fix:** cap `acc` at 256 (array size) during accumulation at `:3536`, failing setup if exceeded — upstream's later codebook/residue checks do the equivalent.
- **Benefit:** closes a setup-header heap overwrite in the most common audio format the player handles.
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ)

### SMS-107 — Vorbis floor1 total post count unbounded: `postlist` heap overwrite in unpack, stack/heap overwrite in look
- **Category:** 6 · **Provenance:** libvorbis ~1.0 snapshot
- **Locations:** `src/SMS_OGG.c:3413-3462` (`floor1_unpack`, sink `:3451-3458`), `src/SMS_OGG.c:3570-3630` (`floor1_look`, stack sink `:3574`), `postlist` = `int[VIF_POSIT + 2]` = 65 (struct `:64`)
- **Problem:** `partitions` ≤ 31 (5 bits) × `class_dim` ≤ 8 (3 bits + 1) gives a summed post `count` up to **248**, written into 65-entry arrays in both unpack (`info->postlist[k + 2]`, heap) and look (stack `sortpointer[65]`, plus three 65-entry look arrays). No `count > VIF_POSIT` check exists anywhere. (The per-class arrays themselves are correctly bounded — round 7 checked-clean V-C4.)
- **Fix:** fail setup when the summed count exceeds `VIF_POSIT` at `:3451` — one comparison on the accumulated total.
- **Benefit:** closes a second setup-header overflow in the Vorbis path, heap and stack variants with one guard.
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ)

### SMS-108 — Vorbis residue `begin`/`end`/`grouping` (24-bit each) never validated against block size: decode writes at file-controlled offsets into the PCM buffer
- **Category:** 6 · **Provenance:** libvorbis ~1.0 snapshot
- **Locations:** `src/SMS_OGG.c:3524-3526` (`res0_unpack` reads the three 24-bit fields), sinks `src/SMS_OGG.c:4715-4777` (`_01inverse`, offset at `:4741`) and `:4883-4919` (`res2_inverse`, `:4908-4911`)
- **Problem:** No check relates `begin`/`end`/`grouping` to `blocksizes[]` (PCM channel buffers are at most 8,192 floats) at unpack, look, or decode. `offset = info->begin + i * samples_per_partition` is passed to the book-decode functions, which write `grouping` floats starting there: a `begin` of 1,000,000 writes ~16 MB past an ≤8,192-float buffer. `end < begin` additionally makes `partvals` negative (`:4719`), feeding a negative byte count into `_vorbis_block_alloc` in the res2 path (`:4893`).
- **Fix:** validate `begin <= end <= blocksizes[1]` (and `grouping` sane) once in `res0_unpack` — three comparisons at the single point all consumers share.
- **Benefit:** removes the largest-magnitude write primitive in the Vorbis decoder (file-controlled offset + length).
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ sink arithmetic; full trigger INFERRED — needs an audio packet selecting a stage book for an affected partition)

### SMS-109 — lwIP `ip_reass` trusts the header's total length over the received chain: `copy_from_pbuf` walks off the chain end and dereferences NULL
- **Category:** 6 · **Provenance:** pristine upstream lwIP snapshot (iop/SMSTCPIP)
- **Locations:** `iop/SMSTCPIP/src/ip.c:227-230` (claimed length; `pbuf_realloc` is shrink-only, `iop/SMSTCPIP/src/pbuf.c:248`), `iop/SMSTCPIP/src/ip_frag.c:143-159` (guard checks destination capacity only), sink `iop/SMSTCPIP/src/ip_frag.c:67-72`
- **Problem:** A fragment whose IP header claims more bytes than were received keeps its short real `tot_len` (realloc shrinks only). `ip_reass` checks the claimed length against the 5,760-byte reassembly buffer — never against `p->tot_len` — and `copy_from_pbuf` loops on the claimed `len` with **no `p != NULL` check**: `for (; len; p = p->next) { l = len < p->len ? len : p->len; memcpy(...); }`. A single-pbuf fragment claiming 1,500 bytes but carrying 100 exhausts the chain and dereferences `p->len` through NULL. Trigger is any fragmented IPv4 packet addressed to the console; the header checksum is computed over the header as sent, so it is no obstacle.
- **Fix:** in `copy_from_pbuf`, stop at chain end (`p != NULL` in the loop condition) and/or validate `len <= p->tot_len - IPH_HL*4` in `ip_reass` before the copy — the chain-end check is one condition.
- **Benefit:** removes a one-packet IOP network-stack crash (and the over-read feeding it).
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ chain; exact IOP fault manifestation INFERRED)

### SMS-110 — SMSUMS READ CAPACITY sector size stored verbatim: 0 divides by zero, >128 KiB overflows the cache buffer on the first read
- **Category:** 6 · **Provenance:** pristine upstream ps2dev USB mass-storage snapshot
- **Locations:** `iop/SMSUMS/src/ums_driver.c:312-313` (store), `:393-420` (`UmsRead` division + bulk transfer), `iop/SMSUMS/src/ums_cache.c:40` (second division), cache buffer 131,072 B at `iop/SMSUMS/src/ums.c:66`; first use `iop/SMSUMS/src/ums_fat.c:989,996`
- **Problem:** `m_SectorSize = _int_be(&lBuf[4])` from the device's READ CAPACITY response is stored with **no validation** — no zero check, no range check (contrast the same tree's BPB parser, which whitelists its sector-size field against {512, 1024, 2048, 4096} at `ums_fat.c:1048-1052`). Mount order runs `FATFS_Init` before `CacheInit`: `m_SectorSize == 0` divides by zero in `UmsRead`'s sector count; a large claimed size with a large claimed `m_MaxLBA` passes the `:398` clamp and bulk-writes `aSize` device bytes into the 131,072-byte cache buffer. The bad geometry then sizes every subsequent `CacheRead`.
- **Fix:** validate at the store (`:312-313`): power-of-two within 512…131,072 (the BPB whitelist's shape, widened to the cache size) or refuse the device — one check at the single store site.
- **Benefit:** closes a device-controlled divide-by-zero and heap overflow that fire before any filesystem validation runs.
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ)

### SMS-111 — AAC PCE channel totals: the `> MAX_CHANNELS` check runs after construction and its return is ignored by two of three callers; unchecked totals index `internal_channel[]` / `channel_position[]`
- **Category:** 6 · **Provenance:** FAAD2 snapshot
- **Locations:** `src/SMS_AAC.c:5812-5909` (`program_config_element`; late check `:5905`), ignored returns at `:5931` (`get_adif_header`) and `:7917` (`raw_data_block` ID_PCE), sinks `src/SMS_AAC.c:7808-7809` (`decode_cpe`) and `:7997-8036` (`create_channel_config`); `MAX_CHANNELS` 64 (`:115`)
- **Problem:** PCE element counts are file bits (front/side/back ≤ 15 elements each, LFE ≤ 3): `pce->channels` can reach **93**, and the per-element channel indices are stored from the running total *during* construction (`:5845-5890`). The only validation, `if (pce->channels > MAX_CHANNELS) return 22`, sits at `:5905` — after all stores — and two of three callers ignore the return entirely (the third, `GASpecificConfig` `:6072`, checks it — the asymmetry shows the omission). Downstream, `decode_cpe` indexes `internal_channel[64]` with `pce.cpe_channel[tag]` up to 92, and `create_channel_config` writes `channel_position[chpos++]` up to ~93 times into a 64-entry array in a stack-resident frame-info struct.
- **Fix:** enforce the total inside `program_config_element` as the counts accumulate (fail the element when the running total would exceed `MAX_CHANNELS`), so ignored returns no longer matter — the check moves, it does not multiply.
- **Benefit:** closes an ADIF/raw-stream header path to intra-struct and stack-adjacent overwrites.
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ)

### SMS-112 — Vorbis `decodemap[temp]` indexed by a phrasebook decode result with no `temp < partvals` check; the fetched pointer is dereferenced
- **Category:** 6 · **Provenance:** libvorbis ~1.0 snapshot
- **Locations:** `src/SMS_OGG.c:4734` (`_01inverse`), `src/SMS_OGG.c:4900` (`res2_inverse`); `decodemap` sized by `partvals` in `res0_look` (`:3659`)
- **Problem:** `vorbis_book_decode(phrasebook)` returns an index into the phrasebook's entry space (`entries` is an independent 24-bit setup field, up to 16,777,215). Nothing relates it to `partvals` (the decodemap's length). A small legitimate-shaped configuration already overflows: `partitions = 2, dim = 1` gives `partvals = 2`, while a 100-entry phrasebook returns `temp` up to 99. The only checks are `temp == -1` and `decodemap[temp] == NULL` — the NULL test itself dereferences the unchecked index first — and the fetched out-of-bounds pointer is then dereferenced as the partition's book pointer at `:4742` / `:4904`.
- **Fix:** reject `temp < 0 || temp >= partvals` before the decodemap read at both sites (or once in a shared decode-step guard) — two comparisons.
- **Benefit:** removes an audio-packet-driven OOB read + wild-pointer dereference in normal Vorbis playback.
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ)

### SMS-113 — MPEG-4 `SMS_BUG_EDGE` widens motion-compensation clipping from macroblock units to pixel units
- **Category:** 6 · **Provenance:** early-FFmpeg lineage
- **Locations:** `src/SMS_MPEG4.c:3324,3329` (flag set from file-controlled encoder-version data), `src/SMS_MPEG4.c:2733-2736` (per-VOP clip bounds), correct values at `src/SMS_MPEGInit.c:33-34`; sinks are the `CLIP_MV`/`CLIP_MVC` macros at `src/SMS_MPEG.c:442-470` and every MC function using them
- **Problem:** Under `SMS_BUG_EDGE` (set when the stream's DivX/XviD version fields match old buggy encoders), every VOP header ends by setting `m_HEdgePos = m_Width; m_VEdgePos = m_Height` — **pixel** counts — where context init set `m_MBW - 1` / `m_MBH - 1` — **macroblock** counts. `CLIP_MV` clamps MC macroblock coordinates against those bounds, so with pixel bounds (e.g. width 720 vs MBW 45) a crafted motion vector addresses macroblocks far beyond the reference picture's `(MBW+2)×(MBH+2)` allocation in every motion-compensation function. Aggravator (INFERRED): the VOL header also assigns `m_Width`/`m_Height` from 13-bit fields (`:912-917` region) without recomputing the MB geometry allocated once at context init, so the pixel bounds need not correspond to the allocated geometry at all.
- **Fix:** convert at the assignment (`:2733-2736`): store `m_MBW - 1` / `m_MBH - 1` exactly as init does — the BUG_EDGE workaround only needs the *rounding* behaviour, not different units. Two expressions changed, no new code.
- **Benefit:** removes an OOB-read primitive from the most common video path (DivX/XviD files are SMS's core use).
- **Effort:** S · **Risk:** Med (behavioural workaround for genuinely buggy old encoders; the unit conversion preserves the workaround's intent but should be hardware-checked against a known BUG_EDGE sample) · **Confidence:** High (READ)

### SMS-114 — DTS downmix table indexed by two unmasked 6-bit header values into an `[11][10]` table
- **Category:** 6 · **Provenance:** vendored DTS snapshot
- **Locations:** `src/SMS_DTS.c:10261` (`table[flags & DTS_CHANNEL_MASK][input]`, `DTS_CHANNEL_MASK` = `0x3F` at `:33`, table `[11][10]`); second instance `src/SMS_DTS.c:11299-11309` (`dts_channels[16]` indexed by `output & 0x3F`)
- **Problem:** Both the row index (`flags & 0x3F`, 0–63) and the column index (`amode`, a 6-bit header field, 0–63) exceed the table's 11×10 shape; the returned `output` is range-checked by the caller only *after* this read. The second instance reads a 16-entry table at indices up to 63. The AC3 sibling function guards exactly this pattern (SMS section checked-clean: `_ac3_downmix_init` masks and range-checks) — the guard exists in-family and was not carried into the DTS copy.
- **Fix:** transpose the AC3 sibling's guard shape: reject row > 10 / column > 9 (and mask the `:11299` index to the 16-entry table) at the two read sites.
- **Benefit:** closes two header-driven OOB reads whose results steer the downmix/output configuration.
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ)

### SMS-115 — AC3 channel-count function-pointer table indexed up to 5 into a 2-entry table — a valid 5.1 stream is the trigger
- **Category:** 6 · **Provenance:** liba52 lineage
- **Locations:** `src/SMS_AC3.c:2585` (`s_FloatToInt[anChannels - 1]`, table of 2 at `:2576`); channel derivation `src/SMS_AC3.c:2669` region (`g_AC3Channels[flags & 7]` = {2,1,2,3,3,4,4,5}, plus 1 for LFE → up to 6)
- **Problem:** `_ac3_decode` indexes the 2-entry FloatToInt function-pointer table with `channels - 1` up to 5 — an OOB function-pointer read followed by an indirect call through it. No crafted stream is needed: a perfectly valid 5.1 AC3 file suffices, unless the container pre-sets the channel count to 1/2 (in which case the value is lowered, not raised).
- **Fix:** clamp/select at `:2585`: use the stereo entry for any channel count the table doesn't cover (the decode output is stereo PCM regardless) — one conditional at the single call site.
- **Benefit:** removes an indirect-call-through-garbage on the most standard surround format.
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ code; reachability INFERRED on the container's channel pre-set behaviour)

### SMS-116 — DTS channel-count function-pointer table indexed up to 5 into a 2-entry table
- **Category:** 6 · **Provenance:** vendored DTS snapshot
- **Locations:** `src/SMS_DTS.c:11565` (`s_FloatToInt[anChannels - 1]`, table of 2 at `:11556`); channel derivation `:11493` (`DTS_Channels(flags)` returns 2/4/5/6)
- **Problem:** Identical pattern to SMS-115 in the DTS decoder: when the container leaves `m_Channels` 0, it is filled from the header flags (2/4/5/6), and the FloatToInt selection indexes a 2-entry table with up to 5 → OOB function-pointer read + indirect call.
- **Fix:** same shape as SMS-115 — one conditional at `:11565`.
- **Benefit:** removes the second instance of the pattern (filed separately: different decoder, different call site, independent fix).
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ code; reachability INFERRED as in SMS-115)

### SMS-117 — AAC `excluded_channels`: continuation-bit loop writes 7 mask entries per round with no cap — `exclude_mask[64]` walked without limit
- **Category:** 6 · **Provenance:** FAAD2 snapshot
- **Locations:** `src/SMS_AAC.c:7548-7562`; arrays `exclude_mask[64]` / `additional_excluded_chns[64]` in `drc_info` (`:300-318`); entry chain `:7576` → `:7610-7613`
- **Problem:** `while ((drc->additional_excluded_chns[n - 1] = SMS_GetBit(ld)))` — each round whose file-supplied continuation bit is 1 writes 7 more `exclude_mask` entries and advances `num_excl_chan` and `n` by 7. After 9 rounds the mask index passes 63; `n - 1` itself passes 63 at the 10th. Nothing caps the loop except the stream bit turning 0 — a fill element full of 1-bits walks both arrays arbitrarily far.
- **Fix:** bound the loop by the array size (`num_excl_chan + 7 <= MAX_CHANNELS`) at `:7548` — one condition.
- **Benefit:** closes an unbounded intra-struct overwrite in the DRC extension path.
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ)

### SMS-118 — Vorbis codebook `entries * dim` (maptype 2) and maptype-1 accumulation overflow int size math
- **Category:** 6 · **Provenance:** libvorbis ~1.0 snapshot
- **Locations:** fields `src/SMS_OGG.c:3160-3161` (`dim` 16-bit, `entries` 24-bit); sink 1 `:3219` + allocation `:3223` + use `src/SMS_OGG.c:3822-3840`; sink 2 `:3128-3151` and `:3808`
- **Problem:** Maptype 2: `quantvals = entries * dim` is a signed `int` product; `dim >= 129` already overflows 2³¹ at maximum `entries`. The wrapped (small/negative) value sizes the `quantlist` allocation and its fill loop, but `_book_unquantize` indexes `quantlist[j * dim + k]` for **all** `j < entries` — OOB read far past the undersized allocation. Maptype 1: `_book_maptype1_quantvals` accumulates `acc *= vals` / `acc1 *= vals + 1` `dim` times with no overflow check, evaluating its termination predicate on wrapped values (signed-overflow UB), and `_book_unquantize` accumulates `indexdiv *= quantvals` the same way.
- **Fix:** validate the product once in `vorbis_staticbook_unpack` (fail setup if `entries * dim` exceeds a sane bound computed in 64-bit) — the single point both maptypes pass through.
- **Benefit:** closes the size-math root under the codebook quantisation path.
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ)

### SMS-119 — Vorbis `res0_look`: `partvals = pow(parts, dim)` overflows int and wraps the decodemap allocation size
- **Category:** 6 · **Provenance:** libvorbis ~1.0 snapshot
- **Locations:** `src/SMS_OGG.c:3631-3675` (`res0_look`); sink `:3657` (conversion), `:3659` (allocation), fill `:3661-3672`
- **Problem:** `parts` ≤ 64 (setup field), `dim` is the phrasebook's unvalidated 16-bit field. `partvals = rint(pow((float)parts, (float)dim))` — `parts = 64, dim = 6` is already 2³⁶; converting the out-of-range float to `int` is UB and in practice yields a negative or truncated value. `malloc(partvals * sizeof(*decodemap))` then wraps to a small allocation on the 32-bit target (e.g. `dim = 5, parts = 64` → 2³⁰ × 4 B wraps to 0) or a huge negative-as-`size_t` request, while the fill loops and later consumers (SMS-112) trust the logical `partvals` — allocation size and logical size are decoupled by the overflow.
- **Fix:** compute in 64-bit and fail look if `parts^dim` exceeds the phrasebook's `entries` (the only legitimate bound — a decodemap larger than the phrasebook is meaningless) at `:3657`.
- **Benefit:** removes the allocation-size half of the decodemap defect family (SMS-112 is the index half).
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ)

### SMS-120 — Vorbis ordered codebook lengths grow past 32: `_make_words` indexes `marker[33]` by length, `oggpack_look` indexes `mask[33]` by `dec_maxlength`
- **Category:** 6 · **Provenance:** libvorbis ~1.0 snapshot
- **Locations:** source `src/SMS_OGG.c:3178-3195` (ordered-length runs); sink 1 `src/SMS_OGG.c:3721-3768` (`_make_words`, first read `:3730`, `length < 32` test `:3731`); sink 2 `:3930-3932` → `:4342,4366` → `oggpack_look` mask index (table `:148-156`, 33 entries)
- **Problem:** In ordered unpack, `length` starts at a 5-bit value + 1 and is incremented once per run; the number of runs is bounded only by `entries` (24-bit), so `lengthlist[]` entries can be arbitrarily large. `_make_words` reads `marker[length]` **before** its `length < 32` test and then writes `marker[j]` for `j` up to `length` — stack array of 33. The same widths become `dec_maxlength`, and `decode_packed_entry_number` calls `oggpack_look(apBuf, dec_maxlength)`, which indexes `mask[anBits]` and shifts by `32 - anBits`-family amounts — both out of range for `anBits > 32`.
- **Fix:** reject any ordered length > 32 at unpack (`:3193` run increment) — the one point all downstream consumers trust.
- **Benefit:** closes a stack overwrite in setup plus an OOB table/shift in the decode hot path with one guard.
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ)

### SMS-121 — Vorbis blocksize field permits window-table index −3…9 into `vwin[8]`
- **Category:** 6 · **Provenance:** libvorbis ~1.0 snapshot
- **Locations:** source `src/SMS_OGG.c:3104-3118` (`_vorbis_unpack_info`); index computation `:3989-3990` (`_vds_shared_init`); table `:2449-2453` (8 entries); sink `:4980-5013` (`vorbis_synthesis_blockin`)
- **Problem:** `blocksizes[i] = 1 << oggpack_read(apBuf, 4)` accepts 8…32768; the only guards are `blocksizes[0] < 8` and `blocksizes[1] < blocksizes[0]`. `_vds_shared_init` computes `window[i] = ilog(blocksizes[i] - 1) - 6` → −3 (bs 8), −2, −1, 8 (16384), 9 (32768). `_vorbis_window_get` returns `vwin[window]` from an 8-entry table and `vorbis_synthesis_blockin` dereferences the result as the window table — an OOB pointer read followed by a read through it.
- **Fix:** require both blocksizes in 64…8192 at `:3115` (the window table's exact domain) — the guards already exist there in weaker form; this completes them.
- **Benefit:** closes a header-only path to a wild window pointer in synthesis.
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ)

### SMS-122 — AAC ADTS/ADIF `sf_index` (12–15) stored unchecked; `window_grouping_info` indexes 12-entry sample-rate tables by it
- **Category:** 6 · **Provenance:** FAAD2 snapshot
- **Locations:** sources `src/SMS_AAC.c:5953` (ADTS), `:5821` (ADIF PCE field), stores `:6041`, `:6030`; sink `src/SMS_AAC.c:6194-6249` (`window_grouping_info`, reads `:6205-6209`, `:6220-6223`); tables `:2006-2100` (12 entries each)
- **Problem:** Neither init path rejects `sf_index` 12–15. `window_grouping_info` reads `num_swb_1024_window[sf_index]` and `swb_offset_1024_window[sf_index][i]` (and the 128-window pair) from 12-entry tables, then dereferences the wild offset-table pointer in its copy loop. The neighbouring consumers of the same field *do* guard (`get_sample_rate` `< 12` at `:5763-5770`; `max_pred_sfb` `< 12` at `:6249-6255`) — the omission is local to this one function. A secondary ASC path can leave `samplingFrequencyIndex == 15` stored at `:6172` (INFERRED).
- **Fix:** reject `sf_index > 11` once in `window_grouping_info` (mirroring `max_pred_sfb`'s existing guard) — or at both init stores; the function-local guard covers all three sources.
- **Benefit:** closes a header-field path to a wild-pointer dereference in every AAC frame's setup.
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ; ASC sub-path INFERRED)

### SMS-123 — AAC two-step Huffman decoders use the file-influenced `offset` to index the code table *before* the size check
**Verification note (2026-11-14): REVISED — the withdrawal rationale in the Problem text below ("a script count shows the declared sizes are maximum valid indices") is falsified by SMS-160's verified counts (declared = actual + 1 for codebooks 1/2/4/6/10; declared = actual − 1 for codebook 11) and is withdrawn: the off-by-one question reopens, and any fix must pair the check-reorder with SMS-160's corrected bounds; see Sable-VERIFY-SMS-CAT6.md and census item C-3 in Sable-VERIFY-CONSISTENCY.md. The check-after-use defect itself is VERIFIED and stands.**
**Verification note (2026-11-14, full verification): WITHDRAWN — the reopened off-by-one question is now settled the other way: two independent script derivations prove the first-stage Huffman segments tile each second-stage table exactly, so the maximum reachable offset equals the last valid index on all seven codebooks (declared/actual/max-reachable: cb1 114/113/112, cb2 86/85/84, cb4 185/184/183, cb6 126/125/124, cb8 83/83/82, cb10 210/209/208, cb11 373/374/373; excess past end 0 everywhere) — no reachable out-of-bounds read exists, and the check-after-use ordering survives only as a latent dead guard. Full three-step history (in-text withdrawal at filing → phase-1 reopening on the falsified rationale → phase-2 refutation) in index §4; the SMS-160 bounds-pairing survives as fix guidance if the reorder is ever done as defence-in-depth; see Sable-VERIFY2-SMS.md §7. Do not act on this finding as a live defect.**
- **Category:** 6 · **Provenance:** FAAD2 snapshot
- **Locations:** `src/SMS_AAC.c:6452-6473` (`huffman_2step_quad`, use `:6464`, check `:6467`), `src/SMS_AAC.c:6519-6538` (`huffman_2step_pair`, use `:6531`, check `:6534`)
- **Problem:** `offset = hcb_table[cb][cw].offset`, then the file's next `extra_bits` bits are **added** to it (`:6463`). Nothing constrains those bits to the valid codeword sub-range, so `offset` can be driven past the segment and the table end — and it is used (`hcb_2_*_table[cb][offset].bits` as the `SMS_SKIP_BITS` argument, and the `sp[]` reads) **before** the only range check `if (offset > hcb_2_*_table_size[cb]) return 10`. (A suspected `>` vs `>=` off-by-one in that check was examined and **withdrawn** during the round: a script count shows the declared sizes are maximum valid indices. Only the check-after-use defect is claimed.)
- **Fix:** move the existing check above the first use at both sites — a reorder of lines that already exist, no new logic.
- **Benefit:** closes an OOB table read in the spectral-data hot path of every AAC frame.
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ)

### SMS-124 — AAC `section_data`: zero-length sections write the per-section arrays at `i == 120` *before* the `i` bound check fires (short windows)
- **Category:** 6 · **Provenance:** FAAD2 snapshot
- **Locations:** `src/SMS_AAC.c:6288-6330`; writes `:6300`, `:6314-6315`; checks `:6317-6319`; arrays in `ic_stream` (`:395-412`, 120 wide)
- **Problem:** In the EIGHT_SHORT_SEQUENCE branch, `sect_len` can be **0** (codebook nibble + zero length increment). The `while (k < max_sfb)` loop then doesn't advance `k` but increments `i` every iteration — and the writes (`sect_cb[g][i]`, `sect_start/end[g][i]`) come before the `i >= 8 * 15` test. After 120 zero-length sections (each iteration consumes ≥ 7 file bits, suppliable inside a normally-sized packet), iteration `i == 120` writes one element past all three arrays into the following struct fields — an intra-struct overwrite — before the function returns its error. The long-window branch was checked and is **not** claimed (its post-write test trips at 51, still inside the 120-wide arrays).
- **Fix:** move the `i` bound test above the writes at `:6300` — same reorder shape as SMS-123.
- **Benefit:** closes an intra-struct overwrite reachable with a small crafted packet.
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ)

### SMS-125 — WMA superframe `bit_offset` never bounded before re-basing the bit reader: negative size becomes a huge bit count past the packet
- **Category:** 6 · **Provenance:** early-FFmpeg WMA snapshot
- **Locations:** `src/SMS_WMA.c:2059` (field read), sink `src/SMS_WMA.c:2073-2074`; `MAX_CODED_SUPERFRAME_SIZE` 16384 (`:39`)
- **Problem:** `bit_offset = SMS_GetBits(lpBitCtx, lpCtx->byte_offset_bits + 3)` is never checked against the packet. `pos = bit_offset + 4 + 4 + byte_offset_bits + 3; SMS_InitGetBits(lpBitCtx, lpBuf + (pos >> 3), (16384 - (pos >> 3)) * 8)` — if `pos >> 3 > 16384`, the size goes negative and converts to a huge `uint32_t` bit count, with the reader pointer already past the packet buffer (`lFrmSize` = container-controlled `block_align`). The `len` check at `:2083` guards a different `memcpy`, not this re-base.
- **Fix:** reject `pos >> 3 >= MAX_CODED_SUPERFRAME_SIZE` before the `SMS_InitGetBits` at `:2074` — one comparison.
- **Benefit:** closes an unbounded-read setup in WMA superframe parsing.
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ)

### SMS-126 — DTS bit reader has no end pointer at all: decode loops can consume past the frame buffer
- **Category:** 6 · **Provenance:** vendored DTS snapshot
- **Locations:** `src/SMS_DTS.c:10009` (`bitstream_fill_current`: `*state->buffer_start++` unconditional); `dts_state_t` carries no end pointer
- **Problem:** The frame copy is bounded (syncinfo `frame_size` ≤ 16384, ≤ ~18,724 B after 14-bit expansion < the 24,576 B `m_InBuf`) — but consumption is not: subframe (≤16) / subsubframe (≤4) / channel / subband (≤32) loops all draw from a reader that cannot know where the frame ends, so a truncated or internally inconsistent frame walks the pointer past the buffer indefinitely.
- **Fix:** add the frame end to the state (the copy site knows it) and stop the fill at it — a small, single-struct change; this is the one DTS fix that is a (tiny) addition rather than a guard reorder, justified because no existing field carries the bound.
- **Benefit:** bounds every DTS decode loop at once (the systemic DTS analogue of SMS-127).
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ)

### SMS-127 — AAC bit reader has no end-of-buffer check: every `SMS_GetBits`/`SMS_GetBit` reads through a 4-byte window at the cursor regardless of `m_szInBits`
- **Category:** 6 · **Provenance:** locally owned reader (`include/SMS_Bitio.h`) under a FAAD2 snapshot
- **Locations:** `include/SMS_Bitio.h:26-40` (`SMS_UPDATE_CACHE` unconditional 4-byte load), `:70-78`, `:90-97`; init `src/SMS_AAC.c:8183`; the only length check is between elements at `src/SMS_AAC.c:7884`
- **Problem:** No function in the reader ever compares `m_Idx` to `m_szInBits`. `raw_data_block`'s loop condition runs only *between* syntax elements; a single element's internal parsing (one CPE's Huffman spectral data, one SBR grid + envelope) can run arbitrarily far past the buffer before control returns to that check. Even at the end of a well-formed frame, the 4-byte cache load reads up to 3 bytes past the allocation; a truncated element reads unboundedly far. This is the root under several round-7 AAC findings (SMS-117/122/123/124 are the concrete consumers with independent defects; SMS-153 is the width/shift consequence).
- **Fix:** bound the reader itself: `SMS_UPDATE_CACHE`/`SMS_GetBits` stop advancing at `m_szInBits` (return 0 bits past end) — one place fixes the class for every consumer, and is a net simplification versus per-parser budgets.
- **Benefit:** converts an unbounded-read class into end-of-stream behaviour for the whole AAC path (and every other Bitio consumer).
- **Effort:** S · **Risk:** Med (touches the hot reader used tree-wide; behaviour at legitimate frame ends must stay bit-exact — the change only alters reads that are already past the end) · **Confidence:** High (READ)

### SMS-128 — XZ size oracle `lzma2_get_uncompressed_size` performs unbounded, unvalidated index parsing: size < 12 underflows the footer pointer, `backward_size` drives the index pointer before the buffer, and the record loop walks without any length
- **Category:** 6 · **Provenance:** locally written SMS glue (commit `b6af6b0`, "Replace libxz binary with sources") around XZ Embedded (Lasse Collin, public domain) — the oracle has no upstream counterpart
- **Locations:** `src/lzma2.c:80-122` (`lzma2_get_uncompressed_size`), `src/lzma2.c:54-78` (`lzma2_decode_size`), varint reader `src/lzma2.c:25-52`; callers `src/SMS_EE.c:950`, `src/SMS_GS_2.c:567`, `src/SMS_IOP.c:237`
- **Problem:** (1) `footer = (xz_footer_t *)(buf + size - 12)` — for `size < 12` the pointer underflows below the buffer before the magic checks read through it. (2) `index = (unsigned char *)footer - ((backward_size + 1) * 4)` where `backward_size` is a raw footer byte-field: a crafted footer points the index arbitrarily far *before* the buffer, and `lzma2_decode_size` dereferences it immediately (`*buf != 0`). (3) `lzma2_decode_size` receives **no length at all**: its record loop advances `buf += decode(...)` for a varint `records` count with no bound against the stream end, reading sizes from whatever memory follows, and accumulates them into a 32-bit `size_t` that can wrap — returning a small "uncompressed size" for a stream whose real size is huge. All three callers trust the result as an allocation size / equality gate (`SMS_GS_2.c:567` compares it against the font's claimed data size; `SMS_IOP.c:237` allocates the IRX buffer by it; `SMS_EE.c:950` is SMS-060's call site — SMS-060 covers that function's NULL handling, not this oracle). The write side is bounded by `xz_dec_run`'s `out_size`, so the proven defects are the OOB **reads** and the wrap; downstream allocation consequences follow the callers' trust. The XZ Embedded core itself (`xz_dec_stream.c`, `xz_dec_lzma2.c`) was read and is bounded — including the dictionary path: in `XZ_SINGLE` mode the dictionary is the caller's output buffer, so the uncapped `dict_max = 0` at `src/lzma2.c:142` allocates nothing (checked, dissolved).
- **Fix:** validate in the oracle: require `size >= 12 + STREAM_HEADER_SIZE`, require the computed index pointer `>= buf` and the record walk to stay within `[buf, buf + size)` (pass the end pointer into `lzma2_decode_size` — it is the only signature change), and accumulate in 64-bit with a sanity ceiling. Still a net simplification: three callers stop trusting an unbounded function.
- **Benefit:** closes OOB reads on every XZ-blob load path (fonts, IRX modules, VU data) — all locally owned code, no upstream-framing complication.
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ — full file read by the coordinator)

### SMS-129 — lwIP TCP data-offset never validated: ignored `pbuf_header` return underflows `len`/`tot_len` to ~65 KB
- **Category:** 6 · **Provenance:** pristine upstream lwIP snapshot
- **Locations:** `iop/SMSTCPIP/src/tcp_in.c:157-158` (use), root helper `iop/SMSTCPIP/src/pbuf.c:291-317` (pool branch has no forward-move length check)
- **Problem:** For POOL/RAM pbufs, `pbuf_header(p, -n)` checks only that the payload doesn't move before the pbuf struct — never against `p->len` — and returns 0 while `p->len += header_size` wraps as u16 (the REF/ROM branch at `:304-308` does check; the receive path's branch doesn't). `tcp_input` reads `hdrlen = TCPH_HDRLEN(tcphdr)` — the wire data-offset nibble, 0–15, never checked against 5 or the remaining segment length — and calls `pbuf_header(p, -(hdrlen * 4))` **ignoring the return** (the same function honours the return for the IP strip at `:121`). A 20-byte segment with data-offset 15 sets `len`/`tot_len` to ≈ 65,492; `inseg.len` takes the wrapped value (`:284`) and `tcp_process`/`tcp_receive` treat the segment as ~64 KB of in-window data. `tcp_parseopt` (`:1161-1201`) walks options bounded only by the same claimed hdrlen — a consequence surface, counted here not separately. The TCP checksum is computed *before* the bad move over the true length, so a correctly-checksummed crafted segment passes cleanly.
- **Fix:** validate at `:157` (`hdrlen >= 5 && hdrlen * 4 <= p->tot_len`) and honour the `pbuf_header` return as `:121` already does — no helper change needed.
- **Benefit:** closes a wire-driven length forgery in the TCP receive path.
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ chain; downstream delivery extent INFERRED)

### SMS-130 — lwIP UDP: a datagram shorter than the UDP header makes the "drop short packets" branch dead and can be delivered with a ~65 KB length
- **Category:** 6 · **Provenance:** pristine upstream lwIP snapshot
- **Locations:** `iop/SMSTCPIP/src/udp.c:172` (strip + dead drop branch), `:309`/`:341` (restore/strip), `:342-344` (delivery); root helper as SMS-129 (`pbuf.c:291-317`); `udphdr->len` never consulted on input
- **Problem:** `udp_input`'s short-packet guard relies on `pbuf_header` failing for a forward move — for pool pbufs it cannot fail (SMS-129 root), so the branch is dead for exactly the packets it names. With IP payload < 8 bytes, `len`/`tot_len` wrap to ≈ 65,528 + u, the payload points past the datagram, and the ports are read from attacker bytes. If a PCB matches (SMS binds UDP sockets in normal network use) and the checksum gate passes (checksum field 0, or solved over the truncated window — all input words except the checksum are sender-controlled), `pcb->recv` is called with `tot_len` ≈ 65,528 + u and the sockets layer copies by the wrapped per-pbuf lengths (`sockets.c:346-353`, `api_lib.c:82-119`) — handing the application bytes past the 1,540-byte pool buffer.
- **Fix:** same root as SMS-129 — an explicit `p->tot_len >= UDP_HLEN` check before the strip at `:172` makes the existing drop branch real. (Filed separately: different protocol path, different delivery consequence, independently fixable.)
- **Benefit:** closes a wire-to-application over-read delivery path.
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ underflow; end-to-end delivery INFERRED on the bound-PCB + checksum conditions stated)

### SMS-131 — SMAP received-frame length from the buffer descriptor is never clamped to the 1,540-byte pool slot before alloc + FIFO copy
- **Category:** 6 · **Provenance:** pristine upstream ps2dev SMAP driver
- **Locations:** `iop/SMSMAP/smap.c:1010` (length from hardware BD; BD struct `iop/SMSMAP/smap.h:39`), `:1014` (`pbuf_alloc` single-pool-pbuf behaviour), `:1018` (FIFO copy sized by `tot_len`, asm at `smap.c:819-884`, count loaded `:825`); pool size `PBUF_POOL_BUFSIZE` 1540 (`iop/SMSTCPIP/include/lwipopts.h:63`)
- **Problem:** `lLen = (lpRXBD->m_Len + 3) & ~3` — the only gate before use is the RX error-bit test (`:1012`); the length itself is compared to nothing. `pbuf_alloc(PBUF_RAW, lLen, PBUF_POOL)` assigns a **single** pool pbuf with `len = tot_len = lLen` (it neither chains nor rejects > 1540), and `SMAP_CopyFromFIFO` copies exactly `tot_len` bytes into that one 1,540-byte slot — an over-long BD length writes into the adjacent pool pbufs, and the pbuf is then handed to `ps2ip_input` as-is. The TX path in the same file does check its length (`:908`) — the RX path lacks the mirror check.
- **Fix:** drop the frame when `lLen > PBUF_POOL_BUFSIZE` at `:1012`, beside the existing error-bit test — one comparison.
- **Benefit:** converts a potential pool overwrite into a dropped frame at the exact point a hardware/wire length sizes a copy.
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ chain; whether a real SMAC EMAC can present `m_Len` > 1540 without an error bit is a hardware question — INFERRED; standard framing suggests not, no jumbo enable exists in the tree)

### SMS-132 — USBD descriptor walk validates only `bLength >= 2`: truncated descriptors are returned as full structs and read at fixed offsets
- **Category:** 6 · **Provenance:** pristine upstream ps2dev USBD snapshot
- **Locations:** walk primitive `iop/USBD/src/hcd.c:176-196` (checks `:190-195`); consumers `iop/USBD/src/hub.c:636,640,647-651` (`hubDrvConnect`), `iop/USBD/src/hub.c:392,397` (`wTotalLength` read), `iop/USBD/src/hcd.c:75-76,93,123` (`openDeviceEndpoint`)
- **Problem:** The walk checks `descBuf < staticDeviceDescEndPtr` and `bLength >= 2` — never `bLength >= sizeof(<requested struct>)`, and never that the requested struct's bytes lie before the end pointer before returning it. A 2- or 4-byte descriptor whose type matches is returned as a complete config/interface/endpoint struct, and consumers immediately read fixed offsets beyond it (`bNumInterfaces`, `bNumEndpoints`, `bEndpointAddress`, `wMaxPacketSize`, …) — bytes from whatever follows in the static buffer, driving connect/probe decisions and endpoint programming (`hcEd.maxPacketSize`, direction, type) from data the device never sent as part of that descriptor. The aggregate size **is** genuinely capped (512 B per device, `hub.c:401-404` — checked and credited), and the hub-descriptor copy is clamped (`hub.c:671-675`); what is missing is the per-descriptor minimum. Client drivers compound it by walking with no contract at all (SMSUMS `_ums_connect`, counted here not there).
- **Fix:** in `doGetDeviceStaticDescriptor`, require `bLength >= sizeof` of the requested descriptor type before returning a match — one check in the single primitive every consumer shares.
- **Benefit:** closes the standard USB descriptor-parser failure mode for every client driver at once.
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ reads; specific corruption INFERRED)

### SMS-133 — Vorbis `oggpack_read`/`oggpack_look` read up to 4 bytes past the end of a packet that is not long enough to trigger the overflow exit
- **Category:** 6 · **Provenance:** libvorbis ~1.0 snapshot
- **Locations:** `src/SMS_OGG.c:2939-2975` (`oggpack_read`), mirrored `:4302-4329` (`oggpack_look`)
- **Problem:** The guard is `if (endbyte + 4 >= storage) { ret = -1; if (total requested bits > storage bits) goto overflow; }` — when the reader is within the last 4 bytes but requests few bits, the second condition fails and execution **falls through** to unconditional reads of `ptr[0]`, and for larger widths `ptr[1..3]` (and `ptr[4]` when `anBits > 32 && endbit`) — all past the packet buffer. Every header and audio field in the decoder flows through these two functions.
- **Fix:** make the near-end branch return the overflow result instead of falling through when the 4-byte window crosses `storage` — the branch already computes the condition; it just doesn't act on it.
- **Benefit:** bounds the single most-called read primitive in the Vorbis path.
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ)

### SMS-134 — Vorbis codebook `dim == 0` never rejected: float divide-by-zero + non-terminating quantvals search in setup
- **Category:** 6 · **Provenance:** libvorbis ~1.0 snapshot
- **Locations:** fields `src/SMS_OGG.c:3160-3161`; sink 1 `src/SMS_OGG.c:3128-3151` (`_book_maptype1_quantvals`); sink 2 `:4420` (`floor0_inverse1` loop step)
- **Problem:** `vorbis_staticbook_unpack` checks only `entries == -1` (the oggpack overflow sentinel). With `dim == 0`: `pow(entries, 1.0F / dim)` divides by zero, the accumulation loop `for (i = 0; i < dim; ++i)` never runs so `acc == acc1 == 1` permanently, and the `while (1)` quantvals search increments `vals` forever for any `entries >= 1` — setup never returns (hang on a crafted setup header, maptype 1). The same zero `dim` makes `floor0_inverse1`'s `for (j = 0; j < look->m; j += b->dim)` never advance.
- **Fix:** reject `dim < 1 || entries < 1` beside the existing `entries == -1` check at `:3163`.
- **Benefit:** removes a setup-time hang + divide-by-zero from two header fields.
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ)

### SMS-135 — Vorbis audio packet `mode` never checked against `modes`: NULL mode_param dereference
- **Category:** 6 · **Provenance:** libvorbis ~1.0 snapshot
- **Locations:** `src/SMS_OGG.c:4157-4204` (`vorbis_synthesis`); field read `:4177`, only check `:4179`, dereference `:4182`; `modebits` derivation `:3968`
- **Problem:** `mode = oggpack_read(opb, modebits)` where `modebits = ilog(modes - 1)` — whenever `modes` is not a power of two, the bit field encodes values `>= modes` (e.g. `modes = 5` → 3 bits → packet values 5, 6, 7). `:4182` immediately dereferences `ci->mode_param[mode]->blockflag`; the setup struct is `calloc`'d, so unallocated slots are NULL — a NULL dereference on an ordinary audio packet. (Values stay inside the `mode_param[64]` array; this is a NULL/unallocated-slot deref, stated as such, not an array overflow. Each *valid* mode's mapping is validated at unpack — that check doesn't cover an out-of-range mode.)
- **Fix:** reject `mode >= ci->modes` beside the `mode == -1` check at `:4179`.
- **Benefit:** removes a per-packet crash in the Vorbis audio path.
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ)

### SMS-136 — Vorbis floor0 `order == 1` passes unpack, then `vorbis_lsp_to_curve`'s do-while decrements its counter through zero and walks `lsp` out of bounds
- **Category:** 6 · **Provenance:** libvorbis ~1.0 snapshot
- **Locations:** source `src/SMS_OGG.c:3392` (field), weak guard `:3399` (`order < 1` only), `look->m` at `:3558`; sink `src/SMS_OGG.c:4486-4528` (`vorbis_lsp_to_curve`, counter `:4505-4512`), caller `:4543-4548`
- **Problem:** `order == 1` is accepted and becomes `aM = 1` in the LSP curve: `int c = aM >> 1` = 0, then `do { q *= ftmp[0] - w; p *= ftmp[1] - w; ftmp += 2; } while (--c);` — the body executes once unconditionally, `--c` yields −1 (truthy), and the loop repeats indefinitely, reading ever further past the `lsp` block allocation (`m + dim + 1` floats, `:4415`). No guard on `aM < 2` (or on even order) exists in unpack, look, or inverse2.
- **Fix:** reject `order < 2` at `:3399` (the guard exists; its bound is wrong by one) — or equivalently guard `aM < 2` at the curve entry; the unpack site is the single point.
- **Benefit:** closes an unbounded OOB read from a two-field floor0 setup.
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ sink; trigger INFERRED — needs a floor0 packet whose inverse1 returns an lsp memo)

### SMS-137 — DTS `bitalloc_huffman == 7` selects a Huffman table past the end of `bitalloc_12[5]`
- **Category:** 6 · **Provenance:** vendored DTS snapshot
- **Locations:** `src/SMS_DTS.c:11340-11345` (`dts_subframe_header` dispatch), table `bitalloc_12[5]` at `:189`
- **Problem:** `bitalloc_huffman[j]` is a 3-bit header field (0–7). Values 6 and 5 take direct-read branches; **all other values** — including 7 — fall through to `_dts_inverseq(state, bitalloc_12[state->bitalloc_huffman[j]])`. Index 7 reads a wild pointer past the 5-entry table, which `_dts_inverseq` then dereferences as a Huffman table.
- **Fix:** treat 7 like the direct branches or reject it at the dispatch (`:11340`) — the field's valid set is 0–6 by the decoder's own structure.
- **Benefit:** closes a wild-pointer dereference from a 3-bit header field.
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ)

### SMS-138 — DTS cumulative Huffman scale-factor sum used as an unchecked scale-table index
- **Category:** 6 · **Provenance:** vendored DTS snapshot
- **Locations:** `src/SMS_DTS.c:11371-11393` (`dts_subframe_header`); tables `scale_factor_quant6[64]` / `scale_factor_quant7[128]`
- **Problem:** The direct paths beside it are safe (6-bit read → quant6, 7-bit → quant7), but the cumulative path accumulates `scale_sum += _dts_inverseq(...)` — signed `(char)` Huffman values spanning roughly −64…+64 — over up to 32 subbands, then indexes `scale_table[scale_sum]` with **no clamp**. The sum can leave [0, 64) in either direction → OOB read of the scale table.
- **Fix:** clamp/validate `scale_sum` into the selected table's range before the two index sites (`:11382`, `:11393`) — the table is already selected by then.
- **Benefit:** closes a subband-count-amplified OOB read in every DTS subframe header.
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ absent check; how far a real crafted stream can push the sum is INFERRED)

### SMS-139 — DTS LFE scale factor: 8-bit index into a 128-entry table
- **Category:** 6 · **Provenance:** vendored DTS snapshot
- **Locations:** `src/SMS_DTS.c:11440`
- **Problem:** `scale_factor_quant7[bitstream_get(state, 8)]` — index 0–255 into a 128-entry table; values 128–255 read past it. No mask or check. (The neighbouring subband paths use correctly-sized 6/7-bit reads — SMS-138's context — making this 8-bit read a plain width error.)
- **Fix:** read 7 bits (the table's domain) or mask to 0x7F at `:11440` — one token.
- **Benefit:** closes a one-field OOB read in the LFE path.
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ)

### SMS-140 — DTS joint-intensity source channel up to 6 indexes a 5-entry array, and the read value bounds a write loop
- **Category:** 6 · **Provenance:** vendored DTS snapshot
- **Locations:** `src/SMS_DTS.c:11405-11407`; field stored `:10771` (3-bit, per channel, into `joint_intensity[5]`)
- **Problem:** `source_channel = state->joint_intensity[j] - 1` (values 0–7 → source −1…6), then `state->subband_activity[source_channel]` bounds the subband loop. Sources 5/6 read past `subband_activity[5]` (and −1 reads before it when the field is 0 and used unguarded on this path), and the out-of-bounds value then controls a write loop's bound.
- **Fix:** reject `joint_intensity` values that don't name a valid *other* channel (1…prim_channels) where the field is read at `:10771` — one range check at the store.
- **Benefit:** closes an OOB read whose result steers writes.
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ)

### SMS-141 — WMA exponent VLC band length `n == 0` produces a ~2³²-iteration write loop
- **Category:** 6 · **Provenance:** early-FFmpeg WMA snapshot
- **Locations:** `src/SMS_WMA.c:1635-1660` (`decode_exp_vlc`, fill loop `:1646-1648` region); band-table init `:1346-1359` (v1 fills row 0 only); row selection by `block_len_bits` under variable block length (extradata `flags2 & 0x0004`, init ~`:1530`)
- **Problem:** `n = *ptr++; do { *q++ = v; } while (--n);` — `q_end` is tested only at the loop head (`:1651`), never inside the fill. With `n == 0` the do-while writes through the whole 32-bit countdown starting at `exponents[ch]`. The zero band entry is reachable in combination: WMA v1 init fills only `exponent_bands[0]`, while decode indexes higher rows whenever variable block length is enabled (`flags2 & 0x0004`) and `use_exp_vlc` is set (`flags2 & 0x0001`) — uninitialized/zero rows supply `n == 0`.
- **Fix:** treat `n == 0` as a decode error before the fill (one comparison at the band read) — zero-length bands are invalid in the format's own terms.
- **Benefit:** removes a runaway write from the WMA exponent path.
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ code; the exact extradata combination is INFERRED)

### SMS-142 — WMA `sample_rate == 0` divides by zero three times during init
- **Category:** 6 · **Provenance:** early-FFmpeg WMA snapshot
- **Locations:** store `src/SMS_WMA.c:1244` (`lpCtx->sample_rate = apCtx->m_SampleRate`, container/ASF-supplied); divisions `:1352`, `:1383`, `:1397`
- **Problem:** No zero check exists anywhere in `wma_init` for the container-supplied sample rate, and three integer divisions use it as the divisor (v1 band positions, v2 band positions, `high_band_start`). A stream/container declaring rate 0 traps during decoder init, before any audio is decoded. (The float `bps` division at `:1291` doesn't trap and is not claimed.)
- **Fix:** reject `m_SampleRate == 0` at the store (`:1244`) — one comparison at the single entry.
- **Benefit:** removes an init-time trap from a single container field.
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ)

### SMS-143 — MSMPEG4 slice height can be 0 → modulo-by-zero in the decode loop
- **Category:** 6 · **Provenance:** early-FFmpeg lineage
- **Locations:** computation `src/SMS_MSMPEG4.c:2151` (`m_SliceHeight = m_MBH / (lCode - 0x16)`); sink `src/SMS_MSMPEG4.c:2332` (`m_MBY % m_SliceHeight`)
- **Problem:** `lCode` is 5 bits; values < 0x17 are rejected, so the divisor is 1–9 — but the quotient is 0 whenever `m_MBH < divisor` (e.g. MBH 8 from a small container height with `lCode` 31). The first slice returns harmlessly (its resync test holds at MBY 0); the next slice evaluation computes `MBY % SliceHeight` with a zero divisor → integer divide-by-zero trap.
- **Fix:** clamp `m_SliceHeight` to at least 1 at `:2151` (a slice height of 0 is meaningless in the format) — one max operation.
- **Benefit:** removes a trap reachable from a small-height MSMPEG4 stream.
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ)

### SMS-144 — MPEG-4 interlaced direct-MV field-time divisor can be 0
- **Category:** 6 · **Provenance:** early-FFmpeg lineage
- **Locations:** `src/SMS_MPEG4.c:1582-1588` (`_mpeg4_set_direct_mv`, interlaced branch); guarded non-interlaced sibling `:2602-2617`
- **Problem:** The non-interlaced divisor **is** guarded: B-VOP setup skips the frame when `m_PPTime <= 0 || m_PPTime <= m_PBTime` and forces `m_TFrame >= 1` before deriving field times. But `m_PPFieldTime` itself can be 0 (the `SMS_ROUNDED_DIV` difference can cancel), and the interlaced branch's `lTimePP = m_PPFieldTime ± lFieldSelect ± i` can then produce exactly 0 — and the following lines divide motion values by `lTimePP` → integer divide-by-zero. The guard pattern exists 1,000 lines away for the sibling path and wasn't applied here.
- **Fix:** mirror the `:2602` guard shape for the field-time divisor at `:1582` (skip direct-MV / treat as 1 when `lTimePP == 0`).
- **Benefit:** closes the remaining divide-by-zero in direct-MV computation.
- **Effort:** S · **Risk:** Low · **Confidence:** Med (READ unguarded arithmetic; a concrete field-time combination producing 0 is INFERRED — none was executed)

### SMS-145 — MP123 gain table base + scalefactor index can exceed `s_GainPow2[378]` on the LSF (MPEG-2/2.5) path
- **Category:** 6 · **Provenance:** libmad-lineage MP3 snapshot
- **Locations:** base `src/SMS_MP123.c:5422` (`m_pPow2Gain = s_GainPow2 + 256 - GetBits(8)`, + up to 56 per granule); length tables `:353`, `:287`; table `s_GainPow2[378]` at `:190`
- **Problem:** Base offset reaches 312 (256 − 1 + 56 + … per the granule addition `(GetBits(3) << 3)`). On the `_mp3_get_scale_factors_2` (LSF) path, packed length fields reach 5 bits → scalefactor up to 31 → dequantize index up to 312 + (31 << 2) = **436 > 378** — an OOB read into following `.data`. The MPEG-1 path does **not** overflow (max length 4 → 15 << 2 = 60 → 372 ≤ 378): the asymmetry between the two sibling paths is the tell that the LSF bound was never re-derived when the wider length fields were added.
- **Fix:** clamp the LSF scalefactor (or the computed index) to the table at the dequantize read — the MPEG-1 path's implicit bound, made explicit for LSF.
- **Benefit:** closes an LSF-only OOB read in MP3 dequantisation.
- **Effort:** S · **Risk:** Low · **Confidence:** Med (READ reasoning end-to-end; occurrence INFERRED — needs an LSF stream simultaneously driving the gain base high and a 5-bit-length band to 31)

### SMS-146 — FLAC wasted-bits count has no lower bound: `m_CurBPS` goes negative into the bit macros
- **Category:** 6 · **Provenance:** libFLAC-lineage subset
- **Locations:** `src/SMS_FLAC.c:276-278` (`_decode_subframe`); consumers `:282-284` region; shift macros `include/SMS.h:68`
- **Problem:** `wasted = 1; while (!SMS_GetBit(apBitCtx)) ++wasted; apCtx->m_CurBPS -= wasted;` — a run of zero bits in the subframe header (file-controlled; the bit reader has no end check, SMS-127's class) makes `m_CurBPS` arbitrarily negative. There is no `m_CurBPS < 0` rejection (upstream FLAC rejects this case). The negative count is then passed as a bit count to `SMS_GetSBits`/`SMS_GetBits`, becoming a huge `uint32_t` count in the `SMS_NEG_USR32` shift macros and in the bit-index advance — corrupting the bit position and driving subsequent reads out of the frame buffer.
- **Fix:** adopt upstream's own disposition: reject the subframe when `m_CurBPS` would go negative at `:278` — one comparison.
- **Benefit:** closes a bit-position corruption primitive in FLAC subframe parsing.
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ missing bound; sink mechanics INFERRED — MIPS shift masking makes the first step garbage rather than a trap)

### SMS-147 — FLAC fixed-predictor warm-up reads `decoded[-1…-4]` for predictor orders 0–3
- **Category:** 6 · **Provenance:** libFLAC-lineage subset
- **Locations:** `src/SMS_FLAC.c:180-183` (`decode_subframe_fixed`); order selection `:285` region
- **Problem:** Before the order switch, the code unconditionally computes `a = decoded[pred_order-1]`, `b = a - decoded[pred_order-2]`, `c = … decoded[pred_order-3]`, `d = … decoded[pred_order-4]`. Subframe types select orders 0–4: for order 0 the reads are `decoded[-1]`, `[-2]`, `[-3]`, `[-4]`; orders 1–3 read progressively fewer negatives. For channel 0 that's before the heap allocation; for channel 1 it reads the tail of channel 0's buffer. The computed values are unused for orders 0/1 — the reads happen regardless.
- **Fix:** compute the warm-up terms inside the order cases that use them (orders 2–4), where the indices are non-negative by construction — a move of existing lines, no new logic.
- **Benefit:** removes a per-subframe OOB read (small, but unconditional for the most common low orders).
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ)

### SMS-148 — DXSB header width drives RLE writes past the scratchpad free region; packet size never validated against the header size
- **Category:** 6 · **Provenance:** locally owned subtitle decoder
- **Locations:** `src/SMS_DXSB.c:199-205` (`_decode` header use, no `aSize >= sizeof(_DXSB_Header)` check; PTS field consumed `:215`); sink `src/SMS_DXSB.c:274` (`mips_memset(lpTmp + lX, …, lLen)`), `lpTmp = SMS_SPR_FREE` = `0x70001B30` (`include/SMS.h`); read-back `src/SMS_DXSB.c:281`
- **Problem:** `lW`/`lH` are the header's uint16 width/height with no sanity bound, and `_decode` never checks the packet is even as large as the header struct it reads. The RLE loop writes runs at `lpTmp + lX` for `lX` ranging over the header width; the scratchpad free region from `0x1B30` to the 16 KB scratchpad end is ~9.4 KB, so any subtitle width above that writes RLE runs past the scratchpad (widths are 16-bit — up to ~64 KB of overrun is expressible), and `dxsb_pack` reads `lRW` bytes back from the same overrun region. (The loop itself terminates and clamps `lLen` to the remaining row width — checked; only the missing width/packet validation stands.)
- **Fix:** validate at `_decode` entry: `aSize >= sizeof(_DXSB_Header)` and `lW <=` the scratchpad free size — two comparisons against constants the file already knows.
- **Benefit:** closes a scratchpad overwrite from a subtitle packet header (XSUB subtitles ride inside crafted video files).
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ)

### SMS-149 — MP123 region-2 start index can read past `m_LongIdx[23]` (low severity, intra-struct)
- **Category:** 6 · **Provenance:** libmad-lineage MP3 snapshot
- **Locations:** `src/SMS_MP123.c:5462-5463` (`_mp3_get_side_info`)
- **Problem:** `lR0C = GetBits(4) + 1` (1–16), `lR1C = GetBits(3) + 1` (1–8); `m_Reg2Start = s_BandInfo[aSFreq].m_LongIdx[lR0C + lR1C] >> 1` — sum range 2–24 against `m_LongIdx[23]`. Indices 23/24 read into the following `m_LongDiff` field of the same static struct: the read stays inside the allocation, but returns difference-table values as a region start, which then bounds dequantize loops. No clamp on the sum.
- **Fix:** clamp `lR0C + lR1C` to 22 at `:5463` — one comparison.
- **Benefit:** removes a wrong-bounds input to the dequantize loops (filed at its real weight: intra-struct read, not a wild access).
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ)

### SMS-150 — SMSUMS `_set_dir_chain` treats a −1 chain error as a completed chain and reads `m_ClusterBuf[-1]`
- **Category:** 6 · **Provenance:** pristine upstream ps2dev snapshot
- **Locations:** `iop/SMSUMS/src/ums_fat.c:482-501`; walkers return −1 at `:270`, `:312`, `:343`; the guarded sibling call site is `:401-403`
- **Problem:** The chain walkers return **−1** when a FAT sector read fails. `_set_dir_chain` handles only the documented `0` = "buffer full, continue" case, so −1 exits the loop as if the chain were complete; the following `for` runs zero times with `i == 0`; `:500` then unconditionally executes `apDir->m_LastCluster = lpFS->m_ClusterBuf[i - 1]` = `m_ClusterBuf[-1]` — a 4-byte read immediately before the 512-entry cluster array inside the heap `FATFS`, latched as the file's last cluster and used by later seek/read positioning. The sibling caller `_get_direntry_sector_data` **does** check (`if (lChainSize <= 0) return -1`) — the check exists in the file; this call site missed it.
- **Fix:** mirror the sibling: bail when `lChainSize < 0` at `:483` — the file's own pattern, one condition.
- **Benefit:** closes an in-struct-adjacent over-read + garbage cluster latch on the media-error path.
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ)

### SMS-151 — SMSUMS `_get_direntry_start_cluster` ignores the sector-data error return and loops over uninitialised locals
- **Category:** 6 · **Provenance:** pristine upstream ps2dev snapshot
- **Locations:** `iop/SMSUMS/src/ums_fat.c:551-599` (call `:566`, locals `:557`); second site `:652` (`_next_direntry`, same pattern)
- **Problem:** `_get_direntry_sector_data` returns −1 on chain failure *without* writing `*apStartSec`/`*apDirSec`. The caller at `:566` discards the return and proceeds into `for (i = 0; i < lDirSec && lCont; ++i)` with `lDirSec` and `lStartSec` uninitialised stack values, calling `CacheRead(apDev, lStartSec + i)` at `:573` — feeding `_get_direntry` (SMS-103's parser) and the name comparisons from unintended disc locations when the garbage start is positive. This is the only family of call sites that doesn't consume the result.
- **Fix:** check the return at `:566` (and `:652`) and bail with the error — the function's own contract, applied.
- **Benefit:** removes uninitialised-value control flow on the device-error path.
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ ignored return; loop extent INFERRED — depends on stack garbage)

### SMS-152 — lwIP reassembly bitmap is indexed at 90 (of 0–89) when the reassembled length is exactly 5,760
- **Category:** 6 · **Provenance:** pristine upstream lwIP snapshot
- **Locations:** `iop/SMSTCPIP/src/ip_frag.c:89` (bitmap = 90 bytes), `:151` (guard admits equality), accesses `:174` and `:195`
- **Problem:** `ip_reassbitmap[IP_REASS_BUFSIZE / 64]` = 90 bytes, indices 0–89. A fragment ending exactly at the buffer limit passes the `:151` guard (`offset + len > 5760` is false for equality). The cross-block branch at `:174` then does `ip_reassbitmap[(offset + len) / 64] |= …` → index **90**, a read-modify-write one byte past the array (the OR operand happens to be 0 for this exact length, so the stored value is unchanged — the access itself is still out of bounds). If that fragment completes the packet, the completion test at `:195` reads `ip_reassbitmap[90]` and compares it against 0 — reassembly completion for a maximum-size datagram is decided by an adjacent static byte, not the bitmap.
- **Fix:** make the `:151` guard `offset + len >= IP_REASS_BUFSIZE`-strict for the final-block path, or clamp the two endpoint accesses — the guard change is one character of operator.
- **Benefit:** closes a 1-byte static-adjacent OOB access and a never-completes/wrongly-completes edge in reassembly (filed at its real, bounded weight).
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ)

### SMS-153 — AAC `huffman_getescape`: the escape-length counter is incremented by a file-controlled run of 1-bits with no cap, then used as a bit-read width and a shift count
- **Category:** 6 · **Provenance:** FAAD2 snapshot (sink); shift macros locally owned (`include/SMS.h:68`)
- **Locations:** `src/SMS_AAC.c:6559-6575`; reached from `:6598-6602` (codebook-11 spectral values of ±16)
- **Problem:** `for (i = 4; ; ++i) if (!SMS_GetBit(ld)) break;` — a crafted run of 1-bits makes `i` arbitrarily large. Then `off = SMS_GetBits(ld, i)` and `j = off | (1 << i)`. At `i >= 32`: `SMS_GetBits` computes `cache >> (32 - aN)` — a negative shift count, UB — and `1 << i` is a ≥32 shift of a 32-bit `int`, likewise UB. Independently of SMS-127 (a bounds-fixed reader would still be handed an illegal width here): the width/shift defect is distinct from the reader's missing end check. The AAC escape sequence is spec-bounded in valid streams; nothing here enforces it.
- **Fix:** cap the run (the spec's escape lengths are bounded; reject `i` beyond the format maximum, 31 at the absolute limit for a 32-bit reader) in the loop at `:6570`.
- **Benefit:** closes a UB width/shift primitive in spectral decoding.
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ)

### SMS-154 — `DSP_FFTGet` loop count is built from an uninitialised register (`addiu $v0, $v0, 32` before `$v0` is ever set)
- **Category:** 6 · **Provenance:** first-party SMS assembly
- **Locations:** `src/SMS_DSP_FFT.S:337` (entry), `:363` (the `addiu`), loop `:364-515` (decrement `:512`, branch `:514`); prototype `include/SMS_DSP.h:82`; sole caller `src/SMS_Spectrum.c:82-84`
- **Problem:** The first loop-count operation in `DSP_FFTGet` adds 32 to `$v0` **before `$v0` is set anywhere in the routine**. The prototype is `void DSP_FFTGet(void *)` — no return value, so `$v0` is not a register the caller has any reason to set, and the sole caller invokes `DSP_FFTInit` / `DSP_FFTRun` / `DSP_FFTGet` as three separate C calls; nothing in the ABI guarantees `$v0` on entry to the third. The intended count is evidently 32 (32 × 128 B = the 4,096-byte FFT workspace). With garbage in `$v0` the loop under-runs (stale spectrum data) or over-runs, writing past `g_FFTWsp` (which sits at `&g_SMSounds[64032]`, `include/SMS_Sounds.h:39`). It works in practice because the preceding call chain happens to leave a benign `$v0` — luck, not a contract.
- **Fix:** `li $v0, 32` instead of the `addiu` — one instruction, the smallest fix in the round.
- **Benefit:** removes a register-luck dependency from the spectrum path that runs during audio playback.
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ mechanism, coordinator-verified at `:363`; consequence INFERRED)

### SMS-155 — Asm `s_GSParams` layout diverges from C `GSParams` after the 576P slot: every PAR word from offset 44 sits one slot early (latent today)
- **Category:** 6 (latent structural divergence)
- **Locations:** `src/SMS_GS_0.S:21-29` (asm table: NTSC 32, PAL 36, 480P 40, **576P 44**, VESA 48, 720P 52, 1080I 56) vs `include/SMS_GS.h:1173-1189` (C struct: `m_PARNTSC` 32, `m_PARPAL` 36, `m_PAR480P` 40, **`m_PARVESA` 44, `m_PAR720P` 48, `m_PAR1080I` 52** — no 576P field)
- **Problem:** The asm table inserts a 576P PAR word the C struct doesn't have, so from offset 44 onward every asm PAR word sits one slot earlier than the C field of the same name, and asm's 1080I word at 56 lies beyond the C struct's PAR fields entirely. It is latent **only** because all seven asm PAR words are `0x3F800000` (1.0f), the only C writes are `m_PARNTSC`/`m_PARPAL` (`src/SMS_GS_1.c:94-95` — offsets 32/36, which match), and no C code reads `m_PAR480P/VESA/720P/1080I`. The first future C read or per-mode write of `m_PARVESA` or later will silently hit the wrong mode's value.
- **Fix:** align the two definitions — delete the asm 576P slot **or** add `m_PAR576P` to the C struct at offset 44. Which side is canonical is a (small) design call: the C struct is the one config code touches (`g_Config.m_PAR[12]`, `include/SMS_Config.h:99`), so adding the field there is the less invasive alignment. Either way it is a one-line change on one side.
- **Benefit:** removes a silent mode-mismatch landmine at the C/asm boundary of the display path.
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ both sides; latency reasoning READ)

### SMS-156 — Three fully dead assembly routines: `GS_XYZF`, `SMS_iTimerReset`, `inet_ntoa` (Cat 1)
- **Category:** 1 (dead code)
- **Locations:** `src/SMS_GS_0.S:52` (`.globl`), body `:449`, declaration `include/SMS_GS.h`; `src/SMS_Timer.s:28`, body `:154`, declaration `include/SMS_Timer.h`; `iop/SMSTCPIP/src/inet.s:18`, body `:618-667` (~49 lines), declaration `iop/SMSTCPIP/include/lwip/inet.h`
- **Problem:** A scripted census over all `.c/.h/.S/.s` in the tree counted references to every exported asm symbol; a header declaration alone does not count as a caller. These three routines have **no C caller and no asm-internal caller** — declaration-only references. (Contrast `SMS_TimerReset` in the same file as `SMS_iTimerReset`, which has callers in `SMS_GUI.c`/`SMS_Player.c`/`SMS_PlayerControl.c`; and `inet_ntoa` is the only fully dead routine in `inet.s`.) Dead routines in EE asm still cost ELF size and invite future callers into unmaintained code.
- **Fix:** delete each routine with its declaration — three small deletions, each independently safe by the census.
- **Benefit:** removes ~100+ lines of dead assembly (exact ELF saving INFERRED — no build here).
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ, census-verified)

### SMS-157 — Four dead assembly exports whose bodies are live under a second name: `GS_RRV`, `_MPEG_AlignBits`, `ip_cksum`, `inet_aton` (Cat 1)
- **Category:** 1 (dead code — export/declaration only)
- **Locations:** `src/SMS_GS_0.S:60` (`GS_RRV:` `:828` shares the body with `_gs_rrv:` `:829`, live via `GS_RenderRoundRect_asm` `:1037`); `src/libmpeg_core.S:23` (`_MPEG_AlignBits` label `_ipu_align_bits:` `:501-502`, live via `src/libmpeg_core.S:524`); `iop/SMSTCPIP/src/inet.s:12` (`ip_cksum:` `:53` shares with `_ip_cksum:` `:54`, live via `:227/:266/:356`); `iop/SMSTCPIP/src/inet.s:16` (`inet_aton:` `:397` shares with `_inet_aton:` `:398`, live via `inet_addr` `:607`)
- **Problem:** Each name is exported (`.globl`) and declared in a header, but no caller anywhere uses that name — the body is reached exclusively through its sibling label. The dead half is the export + declaration: they advertise an API nobody uses and double the surface a reader must census. **The bodies must NOT be deleted** — this is the export-level analogue of the round-4 `aes_reference.c` lesson, filed explicitly so nobody "cleans up" the live code by mistake.
- **Fix:** delete the four `.globl` exports and their header declarations; keep every body byte-identical.
- **Benefit:** shrinks the advertised asm API to the used API at zero runtime risk.
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ, census-verified)

---

## TASK 2 — IOP trees: parse / no-parse dispositions (the pass behind SMS-103/109/110/130-132/150-152)

| Tree | Parses external input? | Disposition |
|---|---|---|
| `iop/SMSTCPIP/` (14,016 LOC) | **YES** — unauthenticated wire packets | Cat 6 pass on `ps2ip_input`, `etharp_*_input`, `ip_input`/`ip_reass`, `udp_input`, `tcp_input`/`tcp_parseopt`, `icmp_input` + the `pbuf` helpers they rely on → SMS-109/129/130/152. **DHCP/DNS: no implementation exists in this tree** (no `dhcp.c`/`dns.c`, not in the Makefile object list; `LWIP_DHCP` is 1 only under `-DPS2IP_DHCP`, defined nowhere in tree or Makefile) — the one unguarded DHCP peek at `ip.c:243-252` is **compiled out of this build**; dissolved for this pin, recorded so a future `-DPS2IP_DHCP` build re-opens it. |
| `iop/SMSMAP/` (1,530 LOC) | **YES (one entry point)** — `SMap_HandleRXInt` consumes hardware BD lengths | Pass on the RX path + `SMAP_CopyFromFIFO` only → SMS-131. Rest is TX/init/register code. |
| `iop/SMSUMS/` (2,410 LOC) | **YES** — disc MBR/BPB/FAT/direntries + device SCSI responses | Pass on `FATFS_Init`, chain/direntry parsers, `_fat_read`, `UmsGetCapacity`/`UmsRead`/`CacheRead`, probe/connect → SMS-103/110/150/151. |
| `iop/USBD/` (3,371 LOC) | **YES** — device-supplied descriptors for all client drivers | Pass on `fetchConfigDescriptors`, `requestDevDescrptCb`, `doGetDeviceStaticDescriptor`, `hubDrvConnect`, endpoint open → SMS-132. |
| `iop/SMSPWROFF/` (224 LOC) | **NO** | CDVD interrupt hook + fixed 4-slot internal callback table; no external buffer/length/structure exists. Read in full (196/224 lines incl. header); **inventoried, no pass** — reasoning stated, per the task's split. |
| `iop/SMSDev9/` (958 LOC) | **LIMITED — parsed, checked clean** | Only external touches: the PCMCIA CIS tuple walk (`card_find_manfid`, `ps2dev9.c:613-663`) — guarded at every step (0x1000 bound, link/next/tuple-length checks all present) — and the fixed-count EEPROM bit-bang. Ioctls are 3 fixed commands. Spot-pass performed; **no candidate**. |

Checked-clean highlights (full lists in the section files): ARP fixed-layout parsing + `tot_len` check; ICMP input checks (the 28-byte dest-unreach over-read stays inside the 1,540-byte pool allocation — dissolved under the containment rule stated in the section: header reads a few bytes past a packet's *logical* length inside its allocation are not promoted; only lengths trusted for an unbounded walk/copy/delivery are); UMS BPB whitelists, cycle-safe bounded chain walkers, short-name construction; USBD aggregate 512-byte descriptor cap, hub port-count check, hub-descriptor copy clamp; SMAP error-frame skip.

## TASK 3 — Assembly ledger (14,785 lines)

| File | Lines | Coverage |
|---|---|---|
| `src/SMS_GS_0.S` | 1,180 | end-to-end |
| `src/SMS_Utils.S` | 1,307 | end-to-end |
| `src/SMS_DSP_MMI.S` | 2,067 | end-to-end (all 37 exports live via `SMS_DSPContextInit` tables; 384 B MacroBlock stride + chroma offsets cross-checked against `include/SMS.h:198-204`) |
| `src/SMS_DSP_QPel.S` | 4,728 | helpers `:1-855` end-to-end; `:855-2344` control flow/offsets/loops read in full (parallel-SIMD arithmetic lines elided from view); all 90 wrappers `:2345-4728` signature-clustered by script + representative bodies end-to-end — no divergent wrapper |
| `src/SMS_DSP_FFT.S` | 617 | end-to-end → SMS-154 |
| `src/SMS_MP123Core.S` | 1,045 | end-to-end (constants cross-checked, no mismatch) |
| `src/libmpeg_core.S` | 2,324 | `:1-1000` end-to-end; `:1000-2324` same treatment as QPel (`s_IPUState` 32 B exact, `s_DMAPack` 128 B = 4 clamped entries × 32 B — verified) |
| `src/SMS_DMA_0.s` / `SMS_IPU_0.s` / `SMS_RC_0.s` / `SMS_Timer.s` | 47 / 108 / 142 / 277 | end-to-end → SMS-156 (Timer) |
| `src/About_Data.s` | 189 | end-to-end — **data only** (`.byte` table under `g_AboutData`); disposition: no code, nothing to audit |
| `iop/SMSMAP/smap_opt.s` | 87 | end-to-end |
| `iop/SMSTCPIP/src/inet.s` | 667 | end-to-end → SMS-156/157 (pbuf struct offsets +0/+4/+8/+10 cross-checked against lwIP `struct pbuf`) |

Every other exported symbol in these files has a real caller (census in the section file). `_MPEG_Set16` (`libmpeg_core.S:2300`) is deliberate self-modifying code (patches instruction bytes, then `FlushCache`) — by design, called from `SMS_MPEG12.c`; noted, not a defect.

## TASK 4 — Header macros: **zero candidates**

Every function-like macro in first-party `include/SMS_*.h` (~130 definitions across 23 headers) was extracted and chased to its call sites. None meets the bar (overflow in an allocation/index context, a side-effect argument double-evaluated at a real call site, or missing parentheses that change meaning at a real call site). The classic hazards are present but **latent only**, verified by grep at their call sites: unparenthesised `DMA_SET_CTRL` (`include/SMS_DMA.h:74`) / `GIF_TAG` (`include/SMS_GS.h:134`) — all call sites are plain assignments; multi-statement `IPU_RESET()` (`include/SMS_IPU.h:56`, no `do{}while(0)`) — all call sites standalone statements; double-evaluating `FILE_EOF` (`include/SMS_FileContext.h:26`), `SMS_MAX`/`SMS_MIN`/`SMS_RSHIFT` (`include/SMS.h:61-68`) — no call site anywhere passes a side-effect argument. Recorded as checked-clean by family in the section file, explicitly not as findings. **Not verified, no claim made:** `SMS_DXSB_DP_*` fixed offsets (`include/SMS_DXSB.h:66-77`) — re-deriving them needs every `GSReg*` packed size; object-like size chains in `include/SMS_Data.h:33-56` are outside the function-like extraction. Vendored headers (`xz*.h`, `lzma2.h`, `libmpeg*.h`) and the generated `s_iop_image.h` were noted and skipped per scope; boundary headers `libds34bt.h`/`libds34usb.h`/`mbstring.h` (first-party but not `SMS_*.h`) were outside the extraction pattern.

## Per-decoder ledger summary (Task 1)

| Decoder / file | Lines | Read this round | Candidates |
|---|---|---|---|
| Vorbis `src/SMS_OGG.c` (+ `SMS_OGG.h` full) | 5,533 | ~3,540 deep (1–225, 2543–5533 in five ranges); 226–2542 constant tables + prototypes via grep/script | 12 → SMS-106/107/108/112/118/119/120/121/133/134/135/136 |
| AAC `src/SMS_AAC.c` (+ `SMS_Bitio.h` full) | 8,302 | ~5,400 deep (1–586, 2006–2105, 3583–8302 in ranges); 587–2005, 2106–3582 constant tables, spot + script size counts | 7 → SMS-111/117/122/123/124/127/153 |
| DTS `src/SMS_DTS.c` | 11,801 | 9950–11801 all code + header/struct ranges; ~410–9950 tables grep/spot | 8 → SMS-104/114/116/126/137/138/139/140 |
| MP123 `src/SMS_MP123.c` | 6,729 | 4746–6729 all code + partials; tables 190–4745 not line-read | 2 → SMS-145/149 |
| MPEG4 `src/SMS_MPEG4.c` | 3,443 | 488–2800, 3130–3443; 2785–3130 grep-only; 1–488 tables | (with below) 4 → SMS-105/113/144 + MP4-2 in MSMPEG4 |
| MSMPEG4 `src/SMS_MSMPEG4.c` | 2,394 | 1650–2394 all code; 1–1396 VLC tables | → SMS-143 |
| H263 `src/SMS_H263.c` / VLC `src/SMS_VLC.c` | 230 / 324 | full / full | 0 |
| AC3 `src/SMS_AC3.c` | 2,772 | 340–1210, 1972–2772; 1210–1972 inventory/partial | 1 → SMS-115 |
| AC3 imdct `src/SMS_AC3_imdct.c` | 642 | structure scan + spot (fixed-size transforms, no file-controlled sizes) | 0 |
| WMA `src/SMS_WMA.c` | 2,112 | 1127–2112 all code; 1–1127 tables | 3 → SMS-125/141/142 |
| FLAC `src/SMS_FLAC.c` | 594 | full | 2 → SMS-146/147 |
| MPEG12 `src/SMS_MPEG12.c` / `libmpeg.c` / `SMS_MPEG.c` | 258 / 1,385 / 1,233 | full / 120–1385 / 1–1000 + inventory | 0 (SMS_MPEG.c is SMS-113's sink, filed there) |
| PCM `src/SMS_PCM.c` | 95 | full — size-clean by construction | 0 |
| DXSB `src/SMS_DXSB.c` | 330 | full | 1 → SMS-148 |
| XZ core `src/xz_dec_stream.c` + `xz_dec_lzma2.c` + wrapper `src/lzma2.c` | 847 / 1,175 / 163 | wrapper full (coordinator); core targeted (stream state machine + lzma2 dict/reset paths) | 1 → SMS-128 (wrapper only; core checked clean) |

**Checked-clean counts:** Vorbis 8 dissolved, AAC 14 dissolved, plus per-codec dissolutions in the codecs-II section (notably: MPEG4 QScale→`m_pRLVLC[32]` dissolved — `SMS_MPEG_SetQScale` clamps 1–31 and stores back before any MB decode; AC3 downmix guard — the in-family model SMS-114 lacks; FLAC residual partition math — total writes = BlockSize − pred_order with BlockSize capped; MP123 `s_ISPow` exact fit 8206 ≤ 8207; libmpeg slice MBA bound + asm MV clamp). **Cat 1 in codec C files: none** — a script census found every static function in SMS_OGG.c/SMS_AAC.c has ≥3 tree occurrences (decl/bundle + def + call); the round's only Cat 1 items are the assembly ones (SMS-156/157).

---

## UPDATED RESIDUE STATEMENT (supersedes the rounds 1–6 residue for SMS)

SMS at pin `e65ced39a27eee8d996e9257e21594d1cac1c93d` has now been audited, at source level in this environment, across: all first-party EE C (rounds 1–6), all locally owned IOP C (rounds 1–6), vendored codec decode arithmetic (round 7, ledger above), pristine-upstream IOP parsing entry points (round 7), all assembly (round 7), and first-party header macros (round 7, zero candidates). **What remains, exactly:**

1. **Constant-table regions never line-read** (data, not code — listed so the boundary is explicit): SMS_OGG.c:226–2542 (window/lookup tables), SMS_AAC.c:587–2005 and :2106–3582 (window/Huffman/SBR tables — table *sizes* script-verified against declarations), SMS_DTS.c:~410–9950 (Huffman/quantisation tables), SMS_MP123.c:190–4745 (tables), SMS_MPEG4.c:1–488, SMS_MSMPEG4.c:1–1396 (VLC tables), SMS_WMA.c:1–1127, SMS_AC3.c:1–340 + :1210–1972 (partial/inventory). A defect in these regions would be a wrong constant, not a parsing flaw; no round has claimed otherwise.
2. **Structure-scan only:** `src/SMS_AC3_imdct.c` (642 lines, fixed-size transforms — no file-controlled size exists in it, verified by inventory, not line-by-line).
3. **IOP non-parsing internals not line-read:** SMSTCPIP output paths (`tcp.c`, `tcp_out.c`, most of `sockets.c`/`api_*.c` beyond delivery accounting) and `mem.c`/`memp.c` internals (~10,100 of 14,016 lines not read — internal/EE-facing, no wire parsing); USBD OHCI register/TD-list internals in `hcd.c` beyond `:40-230` (~2,600 lines not read — hardware queue management, no descriptor/length parsing); SMSMAP PHY/reset/init register sequences (~1,080 lines not read — no external data).
4. **SIMD arithmetic lines elided:** SMS_DSP_QPel.S:855–2344 and libmpeg_core.S:1000–2324 were read for control flow, offsets and loop structure with parallel-SIMD arithmetic lines elided from view; a wrong arithmetic *constant* inside those spans would not have been caught (loop bounds and offsets were).
5. **Not independently re-derived (no claim either way):** `DSP_PackAddMB`'s VU-side block offsets (no C-side structure defines them); `SMS_DXSB_DP_*` header offsets; object-like size chains in `include/SMS_Data.h:33-56`; boundary headers `libds34bt.h`/`libds34usb.h`/`mbstring.h` (outside the macro extraction pattern — the ds34 *drivers* for other repos were censused in round 5 cross-repo; SMS ships its ds34 as precompiled IRX, no source in tree).
6. **Binaries with no source in tree:** `vu/*.o` VU microcode objects (build scripts only — no `.vsm` source exists to audit), the `irx/` blobs, `utils/DDrawSrv.exe`.
7. **Un-auditable in this environment (unchanged in every round):** no PS2 build (no PS2SDK), no hardware behaviour, no executed proof for any finding in any round; Category 5 remains structural; runtime consequences labelled INFERRED stay INFERRED.

**Verdict update:** the rounds 1–6 verdict ("exhausted at source level," residue = vendored internals / upstream IOP / assembly) is now extended through that residue per the ledgers above. A round 8 on this pin would be reading constant tables and non-parsing internals — the report does not recommend it on LESS-NOT-MORE grounds. Productive continuations are unchanged: a pin move, or a PS2 build/hardware rig to convert READ findings into executed ones (SMS-154 and SMS-113 are the two whose fixes most warrant a hardware check).

*End of Round 7 — SMS. 55 new findings (SMS-103…157), zero revisions, SMS six/seven-round total: 157 findings. All clones git-clean; read-only held.*
