# Sable Audit — Simple-Media-System (SMS), Round 4

**Auditor:** Sable. **Mode:** read-only, LESS NOT MORE. No repo file was modified; `git status` clean at the end.
**Pin:** `e65ced39a27eee8d996e9257e21594d1cac1c93d` — **unchanged from Round 3.** Verified this round: clone HEAD == `git ls-remote` master == the round-3 pin, so no delta arose and no revalidation of SMS-050…052 was required.
**Scope (the round-3 still-unread list, verbatim):** (1) SMS_JPEG.c decode-side marker walk end-to-end; (2) AVI + MOV container logic beyond round 2's allocation sites; (3) close round 3 §1.6's one INFERRED link — whether any EE layer caps MPEG dimensions before `_mpeg_alloc_picture`; (4) SMS_IOP.c remainder beyond boot sequencing (device registration, fileXio/devctl surface) plus every src/ file no round had touched, coverage table first, read in size order.
**Result:** **4 new findings (SMS-053…SMS-056), 0 revisions.** Tasks 1 and 3 close with *no* finding — Task 3 resolves round 3's INFERRED link as **capped**, Task 1 clears the largest remaining locally owned parser in full. SMS-035 remains settled (docs-only) and is not re-proposed. Nothing in this round is PS2-build- or hardware-verified; Category 5 statements are structural only, as in rounds 1–3.

---

## 1. Task 3 — the MPEG dimension-cap question is CLOSED: capped (software path), failure-safe (IPU path)

Round 3 §1.6 recorded `_mpeg_alloc_picture`'s unchecked callocs (src/SMS_MPEG.c:42-53) as safe-or-unsafe depending on an INFERRED link: whether anything upstream bounds the width/height that reach it. The full chain was traced and read this round:

**Software path (MPEG-4 / MSMPEG4 from AVI, ASF or MOV):**
container dimensions → `SMS_CodecContext.m_Width/m_Height` → **`SMS_Player.c:1726-1731`** → codec `Init` → `SMS_MPEGContext_Init` (src/SMS_MPEGInit.c:20-75; geometry :25-31) → `SMS_MPEG_FrameStart` (src/SMS_MPEG.c:185) → `_mpeg_alloc_picture` (src/SMS_MPEG.c:30-59).

- **The cap exists, at the player layer:** after `SMS_CodecOpen` and before `Init`, any video stream with `m_Width > 1024 || m_Height > 1024` is refused with `STR_RESOLUTION_TOO_BIG` (src/SMS_Player.c:1726-1731). DXSB subtitle streams are exempt — they allocate nothing dimensioned.
- Consequences, now proven rather than inferred: macroblock stride ≤ 65, the picture-index table ≤ 4,160 entries, and the largest allocations in the `SMS_MPEGContext_Init` family (the motion-value callocs at src/SMS_MPEGInit.c:49-60) are ≈ 66 KB worst case. The callocs are still unchecked — exactly as round 3 §1.6 recorded — but their sizes **cannot be inflated by container data** on this path.
- `MPEG4_CommonInit` (src/SMS_MPEG4.c:488-538) checks its own callocs behind a clean `allocation_error` path — the checked model already exists inside the same codec family.
- One subtlety closed by reading: `_decode_vol_header` (src/SMS_MPEG4.c:825-920) overwrites `g_MPEGCtx.m_Width/m_Height` from the 13-bit VOL fields (:902-903, only when both are non-zero) but does **not** recompute strides, so bitstream-supplied dimensions cannot change allocation geometry after Init; they affect display/GMC only, deliberately (comment at src/SMS_MPEG4.c:709-716).

**IPU path (MPEG-1/2 from MPEG-PS — never reaches `_mpeg_alloc_picture`):** dimensions are parsed from the sequence header by libmpeg.c:272-273 (12-bit) and :498-499 (sequence extension widens to 14-bit), macroblock-rounded at libmpeg.c:148-163, and passed by `SMS_MPEG12.c:146-178` (`_init_cb`) to `SMS_FrameBufferInit` (src/SMS_VideoBuffer.c:275-370), which does checked 64-bit size math — rejects ≤ 0 dimensions and any total > UINT_MAX — and fails cleanly (allocation failure destroys the partial buffer, returns 0, and `m_AllocationFailed` stops the decode). There is no dimension cap here, and none is needed for memory safety: the worst case is a refused open.

**Verdict:** the INFERRED link is resolved — **capped at src/SMS_Player.c:1726-1731 (software path); uncapped but failure-safe (IPU path).** No finding against the allocator. One hole *in* the cap was found while tracing it — it is one-sided — and is filed as SMS-053 below.

---

## 2. Task 1 — SMS_JPEG.c decode-side marker walk: READ IN FULL, CLEARED, no finding

src/SMS_JPEG.c (1,843 lines) read end-to-end. This file is locally maintained, not a pristine import: local hardening commits `a9ac5df` ("validate JPEG layouts and AAC metadata during PR review") and `59d0ed1` ("harden playback timelines, memory ownership and image decoding") are in its history. The walk, mechanism by mechanism:

- **Segment framing:** `_segment` (src/SMS_JPEG.c:113) validates the declared segment length (≥ 2, and contained in the input buffer) before any marker body is consumed; the primitive readers `_get_short_be` / `_get_byte` / `_skip` (src/SMS_JPEG.c:89-111) set `m_Error` on overrun instead of reading past the buffer.
- **SOF (inside the marker switch, src/SMS_JPEG.c:396-632):** requires the exact 15-byte payload; precision must be 8; width ≤ 65520; per-component sampling factors restricted to 1..2; total block sum ≤ 6; duplicate component IDs rejected; components 1/2 forced to 1×1 sampling.
- **SOS:** requires the exact 10-byte payload; Ss = 0, Se = 63, Ah = Al = 0 enforced; quantisation- and Huffman-table existence masks are checked before entropy decode begins.
- **DQT:** ≥ 65 bytes per table required; 16-bit quantisation tables rejected. **DRI:** exact 2-byte payload required.
- **Huffman construction:** `_load_huff` (src/SMS_JPEG.c:788) rejects negative node counts, bounds the symbol count by both the remaining segment length and 256, validates symbol classes, and bounds table cell writes by MAX_CELLS.
- **Entropy decode:** `_unpack` (src/SMS_JPEG.c:915) caps the DC coefficient size at 11, accumulates the predictor in a `long long` with an explicit range check, and guards the HZRL run at i > 48; `_bits` (src/SMS_JPEG.c:883) rejects a marker appearing mid-scan.
- **Allocation geometry:** the decoder's static tables match the header caps exactly — include/SMS_JPEG.h:34,35,37,50: `m_DCTbl[2][64]`, `m_ACTbl[2][384]`, `m_QTable[4][64]`, `m_MCUIdx[6]`. Downstream, src/SMS_Rescale.c gates its input at :263-264 (width ≤ 65520, height ≤ 65535, stripe heights 8/16 — mirroring the SOF caps, so the SOF width/height asymmetry is consistent by design, not a defect) and scales output to ≤ 1024 in the larger dimension with a pixel-count loop bound (W×H ≤ 984,064), so the decoded-bitmap allocation `NewWidth*NewHeight*3` cannot wrap.
- The viewer's demand-paged path (src/SMS_JPEG.c:1733 → `SMS_FileMappingMap`, §4) is the file's only exotic mechanism and is single-mapping by construction in the viewer flow.

**Verdict:** the decode-side marker walk is the most thoroughly validated parser in the repository. No finding, and no revision to round 3 §2.3 (which cleared the container side).

---

## 3. Task 2 — AVI / MOV container logic

### 3.1 Findings

---

#### SMS-053 — The 1024-pixel gate is one-sided: negative AVI/ASF dimensions reach MPEG-4 geometry initialisation

- **Category:** 6 (locally owned defect).
- **Locations:** src/SMS_ContainerAVI.c:570-571; src/SMS_ContainerASF.c:302-303; include/SMS_Codec.h:70-71; src/SMS_Player.c:1726-1731; src/SMS_MPEGInit.c:25-31 and :49-60.
- **Problem:** AVI (`strf`) and ASF (video stream properties) both read width and height with `File_GetUInt` — an unsigned 32-bit read — directly into `SMS_CodecContext.m_Width/m_Height`, which are **int32_t** (include/SMS_Codec.h:70-71). Any value ≥ 2³¹ therefore becomes a *negative* dimension. The player's only dimension gate (src/SMS_Player.c:1726-1731, §1) tests `> 1024`, which a negative value passes. Nothing else sanitises the values: `SMS_CodecOpen` (src/SMS_Codec.c:92-123) is a bare dispatch, the codec Open functions allocate only the codec struct, and there is no `abs()` or range check anywhere in the chain. `SMS_MPEGContext_Init` then computes macroblock counts and strides in signed arithmetic from the negative values: depending on the sign combination, its unchecked callocs (src/SMS_MPEGInit.c:49-60) receive negative counts (→ huge `size_t` → NULL tables that later code walks) or arithmetically positive sizes paired with negative strides (→ the decoder walks frame memory backwards). This is not crafted-only input: a negative `biHeight` is the legitimate top-down encoding in AVI, produced by real Windows capture tools, and those files currently reach this undefined initialisation instead of either playing or being refused.
- **Fix:** Widen the existing gate at src/SMS_Player.c:1726-1731 to two-sided — refuse `m_Width < 1 || m_Height < 1` alongside the existing `> 1024` tests, reusing the existing `STR_RESOLUTION_TOO_BIG` refusal. (`abs()` was considered and rejected: the decoder has no vertical-flip handling, so silently flipping the sign would misrender rather than play the file; refusal is the honest behaviour, and it is what such files effectively get today, minus the undefined state.)
- **Justification if ADD:** Not an addition — one condition widened at the single checkpoint every container and both software codecs already pass through. MOV is immune by construction (16-bit reads, src/SMS_ContainerMOV.c:345) and the IPU path never consumes container dimensions, so the player gate is provably the only site needed.
- **Benefit:** Closes the only uncapped route into the decoder geometry allocator found in four rounds; converts today's top-down-AVI behaviour from undefined initialisation into the existing clean error message.
- **Effort:** S. **Risk:** Low.
- **Confidence:** High on the mechanism (every link READ: container reads, field types, gate, open dispatch, Init arithmetic). The exact failing instruction inside Init is INFERRED — it is sign-combination-dependent, as stated.

---

#### SMS-054 — ODML index entry loop is unbounded and publishes an unchecked `SMS_Realloc` result

- **Category:** 6 (locally owned defect).
- **Locations:** src/SMS_ContainerAVI.c:232 (count read), :274-293 (loop; realloc :281, publish :285, dereference :286); src/SMS_EE.c:45-53 (`SMS_Realloc` contract). Contrasts in the same tree: src/SMS_ContainerAVI.c:169-188 and src/SMS_ContainerMOV.c:913-935.
- **Problem:** `_LoadODMLIdx` takes its entry count straight from the file (`lnEntries = File_GetUInt`, src/SMS_ContainerAVI.c:232) and the type-1 loop that consumes it has **no remaining-file clamp and no EOF test**. At EOF the inline readers return 0 (include/SMS_FileContext.h:93-122), so a truncated or lying `indx` chunk makes the loop append synthesised entries until the count is exhausted — up to ~2³¹ iterations — growing the index one entry per iteration through `SMS_Realloc`. That helper (src/SMS_EE.c:45-53) bumps `*apSize` *before* calling `realloc` and returns `realloc`'s result; the caller publishes it into `m_pIdx` unchecked (:285) and writes the next entry through it (:286). On a 32 MB EE heap the allocation fails quickly, the published pointer is NULL, and opening the file hangs, then crashes, instead of being refused. Both halves of the correct pattern already exist in this codebase: `_Load_idx1`, *in the same file*, clamps its count to "only the entries the file still holds" (:186-188), and MOV's `_add_idx_entry` checks the `SMS_Realloc` result before publishing (src/SMS_ContainerMOV.c:913-935).
- **Fix:** Mirror the in-repo models: clamp `lnEntries` to the entries the `indx` chunk can still hold (or break the loop on `FILE_EOF`, as `_mov_read_stsd` does), and return 0 without publishing when `SMS_Realloc` returns NULL.
- **Benefit:** A truncated OpenDML download — the common real-world case, not just a crafted one — opens-or-refuses cleanly instead of hanging the console.
- **Effort:** S. **Risk:** Low.
- **Confidence:** High (READ — loop, both contrast sites, and the realloc contract all read this round).

---

#### SMS-055 — ODML index-of-indexes recursion has no depth limit

- **Category:** 6 (locally owned defect).
- **Locations:** src/SMS_ContainerAVI.c:294-303 (recursion at :299).
- **Problem:** For type-0 (index-of-indexes) entries, `_LoadODMLIdx` seeks to the entry's offset and calls itself, with no depth parameter, counter, or visited-set anywhere in the function. A self-referential `indx` chain — an entry whose offset points at its own chunk — recurses until the EE stack is exhausted. Real OpenDML files nest exactly one level (master index → standard indexes), so the unbounded depth serves no legitimate file.
- **Fix:** Thread a depth argument through the one recursive call and refuse beyond a small constant (4 is generous against the real-world depth of 1). One parameter, one comparison — no new machinery.
- **Benefit:** Removes a stack-exhaustion crash on a class of corrupt/crafted AVI that the entry loop (SMS-054) does not cover — this one needs no large counts at all.
- **Effort:** S. **Risk:** Low.
- **Confidence:** High (READ).

---

#### SMS-056 — AVI `_ReadPacket`'s movi-boundary test uses the file position captured at function entry

- **Category:** 6 (locally owned logic defect).
- **Locations:** src/SMS_ContainerAVI.c:727 (capture), :779 (the test), :784 and :797 (its uses).
- **Problem:** `_ReadPacket` captures `uint32_t i = lpBuf->m_CurPos` once at entry (:727) and never updates it, while the scan loop consumes chunk after chunk. The guard that decides whether a chunk lies inside the `movi` list — `lCond = lSize >= 0 && i + lSize <= m_MoviEnd` (:779) — therefore weakens with every chunk consumed: a chunk whose real extent crosses `m_MoviEnd` is accepted (as a media packet at :797, or skipped as an inline index at :784) whenever the stale test still passes; even for the first chunk the test is lenient by the 8 header bytes, since `i` points at the header, not the data. Packet *memory* stays safe — `AllocPacket` bounds the allocation and the full read is verified (:804-806) — so the consequence is demux desync: bytes belonging to the index, the next RIFF chunk, or padding are delivered to the decoders as media, with the visible failure modes of garbage frames/audio and lost synchronisation near segment boundaries (including multi-RIFF OpenDML files, where `m_MoviEnd` is re-based per RIFF at :758).
- **Fix:** Delete the stale variable and compute the test from the live cursor, which at that point sits on the first data byte of the chunk: `lCond = lSize >= 0 && lpBuf->m_CurPos + lSize <= lpAVICont->m_MoviEnd`. One expression changed, one variable removed — net negative lines.
- **Benefit:** The `movi` boundary becomes a real boundary for every chunk, not just the first one in each packet call.
- **Effort:** S. **Risk:** Low.
- **Confidence:** Medium-High. The staleness is READ directly; the runtime consequence is INFERRED (it depends on where `m_MoviEnd` falls relative to the chunks that follow, which is file-dependent).

---

**SMS-039 interplay (as briefed):** none of SMS-053…056 touches the `strn`/IAS allocation sites SMS-039 covers, and the logic read does not change SMS-039's severity. No revision.

**Checked and cleared in AVI (not promoted):** `_Load_idx1`'s entry clamp (:186-188); `_Seek`'s backward keyframe scan cannot wrap (its decrement is guarded by `aPos ≥ 1`); `_LocateFrame`'s callers guard `nIdx = 0`. One wart noted in §5 (undersized-header backward seeks).

### 3.2 MOV logic — READ IN FULL, CLEARED, no finding

src/SMS_ContainerMOV.c (1,551 lines) read end-to-end: atom dispatcher, every sample-table reader (stts/stsc/stsz/stco/co64/stss/ctts/elst), the index builder, `_ReadPacket`, and `_Seek`. This is the most thoroughly hardened file in the repository — the 2026 triage has clearly already been through it, and the comments say so. The mechanisms that close the questions this task asked:

- **Count allocations:** every table reader gates its file-supplied count through `_tbl_fits` (declared atom size is a hard ceiling, since all these tables are fixed-width) *and* checks its malloc; stsz additionally verifies the payload read in full before publishing samples. This is SMS-015's territory, confirmed still intact, not re-filed.
- **Count/pointer invariant:** counts are published only after their table is allocated *and* filled (see `_mov_read_stco`'s comment), which is exactly what `_mov_build_index`'s deref-the-pointer walks rely on; `_mov_read_header` then clamps `m_nSamples` to the entries actually indexed.
- **Atom walk:** `_mov_read_dflt` enforces child containment (`lAtom.m_Size > apAtom->m_Size - lTotalSize → break`), re-syncs to each atom's end from wherever its handler left the cursor, refuses stream-child atoms before any `trak` exists (the `m_pStm[-1]` guard), and the m4a probe variant shares the same dispatcher.
- **Index growth:** `_add_idx_entry` (src/SMS_ContainerMOV.c:913-935) caps the entry count *and* checks the `SMS_Realloc` result before publishing — the pattern SMS-054 asks AVI to copy.
- **Packet/seek paths:** `_ReadPacket` bounds the sample cursor against both `m_nSamples` and `m_nIdx`, rejects samples outside the file extent, and verifies the read length; `_Seek` publishes new cursors for all streams only after file positioning has succeeded.

Two micro-observations were examined and are **not** promoted: (a) `_mov_read_dflt`'s trailing skip compares consumed bytes against the *last child's* size (FFmpeg heritage) — gated to < 0x7FFFF, position-only effect, and the dispatcher re-syncs on the next atom header regardless; (b) `_mov_read_stsd`'s unknown-codec-type branch skips the already-consumed amount a second time — the dispatcher's end-of-entry re-sync makes the net effect nil. Neither survives the audit's own bar for a numbered finding.

---

## 4. Task 4 — SMS_IOP.c remainder, and the files no round had touched

### 4.1 SMS_IOP.c: the surface SMS itself exposes

Round 3 read src/SMS_IOP.c (1,880 lines) end-to-end for boot sequencing and failure handling. This round re-read it through the briefed lens — device registration and the fileXio/devctl surface:

- **SMS registers no ioman/iomanX device of its own on the EE side.** There is no `AddDevice`/driver table in SMS_IOP.c; the file is a *consumer* of other modules' devices.
- **What SMS exposes is an RPC client, not a device:** the SMSU (SMS-utils IOP module) client, bound at src/SMS_IOP.c:510, with exactly two commands — 0 = query total free IOP memory (:1839) and 1 = DVDV init (:1849) — plus a 6-slot SIF command handler table (src/SMS_IOP.c:190-202) through which IOP code calls back into EE handlers (e.g. the poweroff callback, :1209-1211).
- **devctl/ioctl surface consumed:** `fileXioDevctl("smb:", SMB_DEVCTL_SMS_CP1251, …)` (src/SMS_IOP.c:1163-1174 — an SMS-private devctl into smbman, argument a 4-byte stack int, correctly sized), the MMCE PING devctl `0x1` (:1052), and PFS/HDD ioctls funnelled through the return-checked `SMS_IOCtl` wrapper (:1856-1880). The poweroff thread (:1178+) performs the standard DEV9 register shutdown ritual under kernel mode — unremarkable PS2 practice.

One wart found here is recorded in §5 rather than promoted (unchecked SMSU RPC replies; the only live consumer is an info-panel display string).

### 4.2 Files read this round (all READ in full unless stated)

| File | Lines | Verdict |
|---|---|---|
| src/SMS_JPEG.c | 1,843 | Cleared — §2 |
| src/SMS_ContainerMOV.c | 1,551 | Cleared — §3.2 |
| src/SMS_ContainerAVI.c | 1,189 | SMS-053/054/055/056; remainder cleared (§3.1) |
| src/SMS_Rescale.c | 371 | Cleared — input gate :263-264 mirrors the JPEG SOF caps; output pixel count loop-bounded |
| src/SMS_MPEGInit.c | 117 | Geometry :25-31 and callocs :49-60 — §1 / SMS-053 |
| src/SMS_IPU.c | 869 | Cleared — GS/IPU display glue; mode indices guarded (`anIdx < 8`), VRAM placement arithmetic commented and bounded; the IPU path's dimension safety is §1 |
| src/SMS_SPU.c | 330 | Cleared — a model file: every RPC wrapper carries a server-NULL guard and a bounded wait, with the wedged-server analysis in comments; no unguarded wait remains |
| src/SMS_FileMapping.c | 337 | Cleared as a mechanism — ps2dev-derived TLB demand-paging for the JPEG viewer (sole caller src/SMS_JPEG.c:1733); one fixed mapping address by design ("implement VAddr allocation here"), one live mapping in the viewer flow; `_file_mapping_remap` cannot represent a short read, which is inherent to demand paging, not a parsing defect |
| src/SMS_CDDA.c | 344 | Cleared — ps2dev libcdvd-heritage CDDA RPC client; TOC copies are fixed-size (1024 / 2048+16 bytes) from the DMA buffer into a caller buffer of the same contract |
| src/SMS_RingBuffer.c | 224 | Cleared with one §5 note — wrap/jump accounting in Alloc/Free/Wait is internally consistent; Unalloc correctly assumes it undoes only the most recent Alloc, which holds at every call site |
| src/SMS_VLC.c | 324 | Vendored FFmpeg VLC builder; its inputs are compiled-in codec tables only — no file data reaches it. Inventory, no finding |
| src/SMS_ContainerAAC.c | 223 | Cleared with one §5 note — ADTS probe requires 5 consecutive consistent frames; the bitrate estimate `lnRB` is bounded below (~114) by the validated sample-rate table and the 13-bit frame length, so the division at :141 is safe |
| src/SMS_ContainerAC3.c | 135 | Cleared — probe requires 8 consecutive valid syncframes; packet sizes come from AC3_SyncInfo's table |
| src/SMS_ContainerOGG.c | 128 | Cleared — probe-only wrapper over vendored libvorbis; delegates packet reading to the default reader |
| src/SMS_PCM.c | 95 | Cleared — trivial codec; the `_Decode` function pointer is set for exactly the two codec IDs `SMS_CodecOpen` routes here |
| src/SMS_DateTime.c | 401 | Pure inline asm (OSD-config / RTC helpers). No C logic to audit |
| src/SMS_SIF.c | 54 | Cleared — `SIF_BindRPC` is bounded (2,000 tries) with the PSX no-LIBSD diagnosis in comments |
| src/SMS_CDVD.c | 132 | Cleared with one §5 note — thin RPC wrappers over the embedded CDVD IRX |
| src/SMS_VSync.c | 151 | Cleared with one §5 note — DMA-based vsync estimation |
| src/SMS_OS.c | 1,980 | Not logic: a 32,768-byte embedded OSDSYS machine-code array (`g_OS`) plus a thin init wrapper. Classified as data |
| src/SMS_Codec.c | 168 | Completed this round (:1-91 tag tables + case-insensitive `SMS_CodecGetID`; :92-168 read in earlier rounds). Cleared |
| src/SMS_DSP.c | 2,166 | **Structure-inventoried, not line-audited:** FFmpeg dsputil heritage — scan tables and the GMC C reference read (:1-420, one §5 anomaly), the remainder is the context-init function table (:371-879) and FFT/MDCT/DCT kernel code (:879-2166). Vendored decode internals, per the standing framing |

Targeted sections read for Task 3 (not whole files): src/SMS_MPEG.c:30-59,185; src/SMS_MPEG4.c:488-538, 684, 709-716, 825-920; src/SMS_MSMPEG4.c:1575; src/libmpeg.c:148-163, 272-273, 498-499; src/SMS_MPEG12.c:146-178; src/SMS_VideoBuffer.c:275-370; src/SMS_Player.c:1720-1735; src/SMS_EE.c:40-80; src/SMS_FileContext.c (STIO_Seek) and include/SMS_FileContext.h:68, 93-122.

---

## 5. Noted, not promoted (with the reason each stays below the bar)

1. **Unchecked SMSU RPC replies** — src/SMS_IOP.c:1839, :1849 ignore `SifCallRpc`'s return and return an uninitialised `int` if the call itself fails. Not promoted: the only live consumer of the query is an info-panel `sprintf` (src/SMS_GUInfoPanel.c:124) — worst case a garbage number on an info screen — and `SMS_IOPDVDVInit` has no callers anywhere in the tree (dead export, include/SMS_IOP.h:98). If either gains a real consumer, this becomes a finding.
2. **RingBuffer refcount decrement outside the mutex** — src/SMS_RingBuffer.c:59 decrements `m_nRef` without holding Sema[2], which `SMS_RingBufferAddRef` (:51-55) does take. Not promoted: concurrent-destroy reachability was not established — teardown at all known call sites is single-threaded.
3. **AAC packet payload read unverified** — src/SMS_ContainerAAC.c:150 does not check the payload `Read` length (MOV's reader does). Not promoted: the packet is AllocPacket-bounded, so a truncated final frame yields a garbage tail in one AAC frame, not a memory error.
4. **GMC chroma index anomaly (vendored)** — src/SMS_DSP.c:290 reads `m_Cr[ lY + 1 ][ lY ]` where the parallel Cb read uses `[ lY + 1 ][ lX ]`. Almost certainly a transcription typo, but it lives in FFmpeg/XviD-derived GMC code, affects pixel accuracy only, and only for GMC-warped content at macroblock right edges. Recorded so it is not lost; not a numbered finding under the vendored-internals framing.
5. **AVI undersized-header backward seeks** — the `avih`/`strh`/`strf` handlers finish with relative seeks of `lSize-28` / `lSize-36` / `lSize-48` / `lSize-40` in unsigned arithmetic (`m_CurPos` is `unsigned int`, include/SMS_FileContext.h:68); an undersized chunk therefore seeks *backwards* and already-consumed header bytes are re-interpreted as chunk tags. Not promoted: termination could not be disproven by reading (a cycle needs a constructed layout), impact is confined to malformed files, and the pattern is FFmpeg-heritage demuxer structure. Flagged here so a future round with a fuzzer can settle it.
6. **Config-indexed table in CDVD_SetSpeed** — src/SMS_CDVD.c indexes a 3-entry speed table with `g_Config.m_CDVDSpeed`. Not promoted: config-domain value, not file input; range discipline belongs to the config loader audited in round 1.
7. **Unchecked fixed-size malloc in vsync estimation** — src/SMS_VSync.c (`SMS_EstimateVSync`) does not check its ~98 KB DMA-buffer malloc. Not promoted: boot-time, fixed size, and the same unchecked-allocation family SMS-045 already covers; re-filing it adds nothing.

---

## 6. Repo-wide coverage table (all four rounds)

src/*.c totals **122,965 lines** (plus headers and .S/.s assembly, inventoried by name only).

**Read in full (named rounds):** SMS_ContainerASF.c, SMS_ContainerMP3.c, SMS_ContainerJPG.c, SMS_ContainerM3U.c, SMS_ContainerAVI.c, SMS_ContainerMOV.c, SMS_IOP.c (R3 boot lens + R4 surface lens), SMS_Config.c (R2, unsafe sites in context), SMS_FileDir.c, SMS_List.c, SMS_DirTree.c, SMS_CopyTree.c (R2) — and this round: SMS_JPEG.c, SMS_Rescale.c, SMS_MPEGInit.c, SMS_IPU.c, SMS_SPU.c, SMS_FileMapping.c, SMS_CDDA.c, SMS_RingBuffer.c, SMS_VLC.c, SMS_ContainerAAC.c, SMS_ContainerAC3.c, SMS_ContainerOGG.c, SMS_PCM.c, SMS_DateTime.c, SMS_SIF.c, SMS_CDVD.c, SMS_VSync.c, SMS_Codec.c.
**Read in all relevant sections / classified sites in context (R1–R3 ledgers):** SMS_Player.c, SMS_Container.c, SMS_ContainerMPEG_PS.c, SMS_FileContext.c, SMS_EE.c, SMS_Locale.c, SMS_MC.c, SMS_History.c, SMS_Spectrum.c, SMS_SubtitleContext.c, SMS_GUI.c, SMS_GUIMenuSMS.c, SMS_GUIFileMenu.c, SMS_GUISMBrowser.c, SMS_GUIFileCtxMenu.c, SMS_GUInfoPanel.c, SMS_GUIDesktop.c, SMS_PlayerControl.c (classification), SMS_MPEG.c, SMS_MPEG4.c / SMS_MSMPEG4.c / libmpeg.c / SMS_MPEG12.c (Task-3 sections), SMS_VideoBuffer.c (frame-buffer init).
**Data, not logic (classified by inspection):** SMS_Data.c (16,728), SMS_OS.c (32 KB OSDSYS blob), SMS_Sounds.c (sound bank, data-dominated), SMS_IconsRGBA.c, SMS_BallRGBA.c, SMS_LoadingRGBA.c, SMS_JPEGData.c, SMS_InverseCodePages.c, SMS_ConfigIcon.c, About.c, SMS_Menu.c.
**Vendored, inventoried (provenance + allocation surfaces per R3 §1; decode internals deliberately not line-audited):** SMS_DTS.c, SMS_AAC.c, SMS_MP123.c, SMS_OGG.c, SMS_AC3.c + SMS_AC3_imdct.c, SMS_WMA.c, SMS_FLAC.c, SMS_H263.c, SMS_DSP.c (structure-inventoried R4), xz_dec_lzma2.c, xz_dec_stream.c, xz_crc32.c, lzma2.c, mbstring.c, libds34bt.c, libds34usb.c, and the iop/ module trees (pristine upstream, R3 §4).

---

## 7. Still unread / unfinished (honest list)

- **GUI logic beyond round 2's classified string sites:** SMS_GUIMenuSMS.c (3,052), SMS_GUIFileMenu.c (1,662), SMS_GUIcons.c (1,546), SMS_GUI.c (1,054, beyond the GUI thread), SMS_GUIDesktop.c (992, beyond splash/redraw), SMS_GUIDevMenu.c (690), SMS_GUIFileCtxMenu.c (540, beyond classified sites), SMS_GUIMenu.c (414), SMS_GUICmdProc.c (405), SMS_GUIClock.c (293), SMS_GUIMiniBrowser.c (198), SMS_PlayerControl.c (1,467, beyond classification), SMS_PlayerMenu.c (349).
- **Rendering/setup logic:** SMS_GSFont.c (2,297), SMS_GS_1.c (717), SMS_GS_2.c (590), SMS_IPU_1.c (324), SMS_Artwork.c (141), SMS_PgInd.c (290), SMS_PlayerBallSim.c (123).
- **Remainders of partially read files:** SMS_FileContext.c's device backends beyond the sections read in R2/R4 (file is 2,208 lines); SMS_EE.c beyond :40-80 and cited sites; SMS_Player.c sections never cited by any round; SMS_SubtitleContext.c (1,001) beyond R2's font path; SMS_Sounds.c (4,447) playback logic within the data; SMS_MC.c (317) beyond R2's RPC struct; SMS_PAD.c (492); SMS_RC.c (583); main.c (105, entry shim).
- **Vendored decode internals, by the audit's standing framing:** SMS_MPEG4.c / SMS_MSMPEG4.c / SMS_H263.c frame parsing beyond the Task-3 sections, SMS_DSP.c kernels (:879-2166), and the codec files listed in §6 — allocation verdicts only, as in R3.
- **Assembly sources** (*.S / *.s: SMS_DSP_MMI.S, SMS_MP123Core.S, SMS_GS_0.S, SMS_IPU_0.s, SMS_RC_0.s, SMS_DMA_0.s, SMS_Timer.s, SMS_Utils.S, libmpeg_core.S, About_Data.s) — inventoried by name only in every round.
- **Category 5 remains structural** — no PS2SDK, build, or hardware exists in this environment; SMS-013's heap behaviour, SMS-025's exact text size, and SMS-050's runtime manifestation remain open for a round with a toolchain, exactly as R3 §8 recorded.

*End of Round 4 — SMS. Findings SMS-053…SMS-056 (4 new, all locally owned), zero revisions, Task 1 cleared with evidence, Task 3's INFERRED link closed (capped / failure-safe), Task 4's IOP surface inventoried and 21 previously untouched files read.*
