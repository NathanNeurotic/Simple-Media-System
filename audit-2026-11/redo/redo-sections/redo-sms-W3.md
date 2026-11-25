# Blind Redo Audit — W3: Simple-Media-System, EE Video Decoders & Video Pipeline

Pin: `e65ced39a27eee8d996e9257e21594d1cac1c93d` (verified via `git rev-parse HEAD`).
Clone read-only; nothing under `~/workspace/Sable-audit/` was touched.
Rule applied throughout: LESS, NOT MORE — remove / simplify / fix only, one mechanism per candidate.

## (a) Coverage note

Read fully (line counts by `wc -l`):

| File | Lines |
|---|---|
| src/SMS_MPEG.c | 1233 |
| src/SMS_MPEG12.c | 258 |
| src/SMS_MPEG4.c | 3443 |
| src/SMS_MSMPEG4.c | 2394 (code read fully; large constant tables read + script-counted) |
| src/SMS_H263.c | 230 |
| src/SMS_IPU.c | 869 |
| src/SMS_IPU_1.c | 324 |
| src/SMS_JPEG.c | 1843 |
| src/SMS_DXSB.c | 330 |
| src/SMS_VLC.c | 324 |
| src/libmpeg.c | 1385 |
| src/SMS_Rescale.c | 371 |
| src/SMS_VideoBuffer.c | 384 |
| include/SMS_MPEG.h | 301 |
| include/SMS_VLC.h | 132 |
| include/SMS_IPU.h | 145 |
| include/SMS_JPEG.h | 127 |
| include/SMS_MPEG4.h | 43 |
| include/SMS_Bitio.h | 148 |
| include/SMS_DXSB.h | 109 |
| include/SMS_Rescale.h | 65 |
| include/SMS_VideoBuffer.h | 66 |
| include/libmpeg.h | 69 |
| include/libmpeg_internal.h | 210 |
| include/SMS_H263.h | 32 |
| include/SMS_MPEG12.h | 33 |
| include/SMS_MSMPEG4.h | 29 |

Structure-scan only (assembly): `src/libmpeg_core.S` (2324 lines — entry points, globals, DMA packet layout), `src/SMS_IPU_0.s` (108 lines — IPU register routines at fixed `0x10002000`).

Table sizes were verified by script (counting initializer elements), never eyeballed.

## (b) Candidates (sorted by benefit)

### CAND-W3-1 — MPEG-4 sprite trajectory: bitstream warp-point count overflows a 4-entry stack array
- Category: 6 (bugs/security)
- Locations: `src/SMS_MPEG4.c:933` (count read), `src/SMS_MPEG4.c:1167-1201` (use)
- Problem: `_decode_vol_header` stores `SMS_GetBits(apBitCtx, 6)` (0–63) into `g_MPEGCtx.m_nSpriteWarpPts` with no validation. `_mpeg4_decode_sprite_trajectory` declares `int lD[4][2]` on the stack and loops `for (i = 0; i < g_MPEGCtx.m_nSpriteWarpPts; ++i)` writing `lD[i][0]` / `lD[i][1]`. A stream declaring 5–63 warp points writes up to 59 int-pairs past a stack buffer. MPEG-4 allows at most 4 warping points, so any larger value is by definition a corrupt/hostile stream.
- Fix: Reject (or clamp-and-error) `m_nSpriteWarpPts > 4` where it is parsed at `src/SMS_MPEG4.c:933`. One comparison; no new machinery.
- Benefit: Removes a remotely triggerable stack smash in the main MPEG-4 path (any .avi/.mp4 with a GMC/static-sprite VOL).
- Effort: Trivial (one guard).
- Risk: Negligible — values >4 are outside the MPEG-4 spec.
- Confidence: READ

### CAND-W3-2 — DXSB subtitle decoder trusts header dimensions against a fixed scratchpad row buffer; packet size never checked
- Category: 6 (bugs/security)
- Locations: `src/SMS_DXSB.c:24-34` (header struct), `src/SMS_DXSB.c:195-204` (`_decode` entry), `src/SMS_DXSB.c:209` + `src/SMS_DXSB.c:272` (scratchpad row writes), `include/SMS.h:120` (`SMS_SPR_FREE` = `0x70001B30`)
- Problem: `_decode(apData, aSize)` casts `apData` to `_DXSB_Header` and reads packed `unsigned short m_Width/m_Height` (up to 65535 each) without ever checking `aSize >= sizeof(_DXSB_Header)`. The row staging buffer is `lpTmp = SMS_SPR_FREE` at `0x70001B30`; the scratchpad ends at `0x70004000`, leaving 0x24D0 (9424) bytes. The decode loop does `mips_memset(lpTmp + lX, …, lLen)` with `lX` ranging up to `lW-1` and also `dxsb_pack(lpDst, lpTmp, lRW)` reading `lRW` (width rounded to 32) bytes from that staging area. A subtitle packet claiming a width above ~9400 writes/reads far outside the scratchpad free region. Allocation size is likewise derived purely from the claimed dimensions (`lSize = (lRW>>1) * lRH`, `src/SMS_DXSB.c:201-204`).
- Fix: At `_decode` entry, return NULL when `aSize < sizeof(_DXSB_Header)` or when the claimed width exceeds the scratchpad row capacity (or a sane subtitle bound). Two comparisons at one choke point; no new code paths.
- Benefit: Removes a stream-controlled scratchpad overwrite from the subtitle path, which runs on every DivX-embedded-subtitle file.
- Effort: Small.
- Risk: Low — legitimate DXSB subtitles are a few hundred pixels wide.
- Confidence: READ

### CAND-W3-3 — MPEG picture allocation path assumes success: unchecked callocs and a nullable picture lookup dereferenced unconditionally
- Category: 6 (bugs/security)
- Locations: `src/SMS_MPEG.c:36-58` (`_mpeg_alloc_picture`), `src/SMS_MPEG.c:130-145` (`SMS_MPEGContext_FindUnusedPic`), `src/SMS_MPEG.c:181-185` (`SMS_MPEG_FrameStart`)
- Problem: One mechanism, one path: picture setup never checks failure. `_mpeg_alloc_picture` performs seven `calloc`s (MB-skip, QScale, MB-type, 2× motion-value base, 2× ref-idx) with no NULL check and immediately derives `m_pMotionVal[i] = m_pMotionValBase[i] + 2`. `SMS_MPEGContext_FindUnusedPic` explicitly returns NULL when all `MAX_PICTURE_COUNT` (3) pictures are in use, and `SMS_MPEG_FrameStart` then executes `lpPic->m_Ref = …` and `_mpeg_alloc_picture(lpPic)` on the result with no NULL test. Either failure mode is a NULL dereference mid-stream instead of a dropped frame / codec error.
- Fix: Check the `FindUnusedPic` result in `SMS_MPEG_FrameStart` and bail out of the frame; check the calloc group in `_mpeg_alloc_picture` once (single combined test) and propagate the same bail. No new abstraction — reuse the existing error style of the codec Init functions.
- Benefit: Turns memory-exhaustion / pool-exhaustion crashes into recoverable decode errors across MPEG-1/2/4 and MSMPEG4 (all share this path).
- Effort: Small.
- Risk: Low.
- Confidence: READ

### CAND-W3-4 — VLC/RL table construction and the DivX bitstream stash use allocation results without NULL checks
- Category: 6 (bugs/security)
- Locations: `src/SMS_VLC.c:223` and `src/SMS_VLC.c:226` (`SMS_RL_Init`), `src/SMS_VLC.c:268-270` (`SMS_VLC_RL_Init`), `src/SMS_MPEG4.c:3391-3392` (DivX packed-frame stash)
- Problem: `SMS_RL_Init` `malloc`s `m_pMaxLevel`/`m_pMaxRun` and `memcpy`s into them on the next line, unchecked. `SMS_VLC_RL_Init` `malloc`s `m_pRLVLC[q]` for q = 0..31 and immediately fills `m_VLC.m_TableSize` entries, unchecked. These run for every RL table at codec init (MPEG-4 and MSMPEG4 both). Separately, `MPEG4_Decode` does `g_MPEGCtx.m_pBSBuf = realloc(g_MPEGCtx.m_pBSBuf, …)` and `memcpy`s into the result unchecked — a failed realloc both loses the old buffer pointer and NULL-dereferences. Contrast inside the same file set: `_SMS_VLC_AllocTable` in `src/SMS_VLC.c` does check its realloc, so the unchecked sites are inconsistencies, not house style.
- Fix: Add NULL checks at the three sites; on failure return/propagate the codec-init error (init paths already have an `allocation_error` convention, e.g. `MSMPEG4_Init`), and for the realloc use a temporary pointer so the old stash survives failure.
- Benefit: Removes init-time and low-memory NULL derefs in table setup shared by all MPEG-family decoders.
- Effort: Small.
- Risk: Low.
- Confidence: READ

### CAND-W3-5 — DivX packed-B start-code scan never advances through the buffer
- Category: 6 (bugs/security — functional)
- Locations: `src/SMS_MPEG4.c:3362-3379`
- Problem: In `MPEG4_Decode`'s DivX packed-frame handling, the scan is `for (i = lCurPos; i < lBufSize - 3; ++i) if (SMS_unaligned32(lpBuf) == 0xB6010000) …`. The pointer is never offset by `i` (nor by `lCurPos`), so the loop tests the same four bytes at the packet start on every iteration. Unless the VOP start code happens to sit at offset 0, `lStartCodeFound` stays 0 and the packed second frame is never stashed into `m_pBSBuf` — packed-B DivX streams lose/desync frames.
- Fix: Test `SMS_unaligned32(lpBuf + i)`. One-token change.
- Benefit: Restores the intended packed-B handling the surrounding code (stash, `m_BSBufSize`, next-call decode at `src/SMS_MPEG4.c:3283`) was built for.
- Effort: Trivial.
- Risk: Negligible.
- Confidence: READ

### CAND-W3-6 — DXSB frames are emitted with `m_EndPTS` never initialized
- Category: 6 (bugs/security)
- Locations: `src/SMS_DXSB.c:202-204` (allocation), `src/SMS_DXSB.c:215-232` (fields actually set), `src/SMS_DXSB.c:317-318` (use)
- Problem: `_decode` allocates the frame with `memalign` (uninitialized memory) and sets `m_StartPTS`, dimensions, pixmap and crop fields — but never `m_EndPTS`. `SMS_DXSB_Decode` then unconditionally computes `lpFrame->m_EndPTS = SMS_Rescale(lpFrame->m_EndPTS, …)` from heap garbage and posts the frame, so subtitle display timing consumes an arbitrary value.
- Fix: Set `retVal->m_EndPTS = retVal->m_StartPTS` in `_decode` next to the other field initializations (or switch the allocation to zeroing). One line.
- Benefit: Deterministic subtitle end times; removes an uninitialized-memory read from the output path.
- Effort: Trivial.
- Risk: Negligible.
- Confidence: READ

### CAND-W3-7 — Dead H.263 inter-unquantize path: empty function, `#if 0` body that cannot compile, wired but never called
- Category: 1 (dead code; also 8, half-finished)
- Locations: `src/SMS_MPEG.c:904-935` (function), `src/SMS_MPEG.c:205` and `src/SMS_MPEG4.c:513` (assignments), `include/SMS_MPEG.h:150` (pointer member), `include/SMS_H263.h:27` (prototype)
- Problem: `SMS_H263_DCTUnquantizeInter` has its entire body inside `#if 0 … #endif` (`src/SMS_MPEG.c:905-934`), and the disabled body references undeclared identifiers (`aQScale`, `aN`), so it cannot simply be switched back on — it is abandoned, not disabled. The resulting empty function is still installed as `g_MPEGCtx.DCT_UnquantizeInter` in two places, yet a tree-wide search shows the pointer is never invoked anywhere (the only unquantize call site, `src/SMS_MPEG.c:1196`, calls `DCT_UnquantizeIntra`; inter unquantization is done inline by the block decoders). The wiring actively misleads: it suggests H.263 inter blocks pass through a no-op.
- Fix: Delete the empty function and its `#if 0` body, the two assignments, the prototype, and — if no out-of-scope caller exists — the `DCT_UnquantizeInter` member. Pure removal.
- Benefit: ~35 lines and one misleading indirection gone; nobody can "fix" playback by enabling code that never ran.
- Effort: Small.
- Risk: Low (verify no IOP-side or out-of-scope caller references the symbol before deleting the member).
- Confidence: READ

### CAND-W3-8 — IPU suspend/resume duplicated verbatim as "dummy" variants
- Category: 2 (duplication)
- Locations: `src/SMS_IPU.c:584-596` (`IPU_Suspend`/`IPU_Resume`), `src/SMS_IPU.c:605-617` (`IPU_DummySuspend`/`IPU_DummyResume`)
- Problem: `IPU_DummySuspend` is byte-identical to `IPU_Suspend` (`DisableDmac(DMAC_I_GIF);`) and `IPU_DummyResume` is byte-identical to `IPU_Resume` (`DMAC->m_STAT = 4; EnableDmac(DMAC_I_GIF);`) — including a stale `/* end IPU_Suspend */` comment on the dummy. Two copies of DMA register manipulation will drift.
- Fix: Delete the two dummy bodies and point the dummy vtable slots at `IPU_Suspend`/`IPU_Resume` directly.
- Benefit: One definition of IPU suspend/resume semantics.
- Effort: Trivial.
- Risk: Negligible.
- Confidence: READ

## (c) Checked-clean notes

- **MSMPEG4 motion-vector tables are consistent (script-verified).** Initializer element counts: `s_table0_mv_code`/`s_table1_mv_code` = 1100, `s_table0_mv_bits`/`s_table1_mv_bits` = 1100, `s_table0_mvx`/`s_table0_mvy`/`s_table1_mvx`/`s_table1_mvy` = 1099 each — matching the declared sizes, `s_mv_tables[].m_N = 1099`, and the `SMS_VLC_Init(…, lpMV->m_N + 1, …)` call in `MSMPEG4_Init`. The 1100th code entry is the escape handled via `lCode == lpMV->m_N` in `_msmpeg4_decode_motion`. No off-by-one.
- **MSMPEG4 DC VLC sizes match.** The four `s_table*_dc_*` tables are declared and initialised with 120 entries and initialised with count 120; the v2 DC tables are 512 entries and initialised with count 512.
- **libmpeg motion slots are provably within bounds.** `_mpeg12_get_ref` appends to `m_Motion[m_nMotions++]` with no explicit check against `m_Motion[7]` (`include/libmpeg_internal.h:118`), but counting the call sites in `_mpeg12_get_refs` (`src/libmpeg.c:1062-1241`): the forward branch contributes at most 4 (frame-picture dual-prime), the backward branch at most 2 — worst case 6 of 7. Safe by construction (INFERRED from control flow; an assert would be optional hardening, not a fix).
- **libmpeg sequence init handles allocation failure.** `_init_seq` NULL-checks its frame allocations and unwinds through `_destroy_seq`; the 4-bit frame-rate code indexes a 16-entry table. No finding.
- **JPEG parser is already hardened.** `src/SMS_JPEG.c` bounds-checks segment walking (`_get_short_be`/`_skip`/`_segment` with `m_Error`), validates SOF (3 components, sampling factors 1–2, quant table < 4, total blocks ≤ 6, width ≤ 65520), validates DHT (class/id, symbol counts, leaf/node bounds), and bounds DC/AC/run decoding in `_unpack`. No candidate raised.
- **Rescale / VideoBuffer dimension handling is already hardened.** `SMS_RescaleInit` rejects width > 65520 / height > 65535 and non-8/16 stripes and preserves the old context on allocation failure; `SMS_VideoBufferInit` and `SMS_FrameBufferInit` reject > 65504 dimensions and perform the size multiplication in `unsigned long long` with NULL checks. No candidate raised.
- **IPU image load caps exist.** `IPU_InitLoadImage` in `src/SMS_IPU_1.c` rejects non-16-aligned widths, width > 1024 and height > 1024, with a comment tying the cap to DMA overrun. No candidate raised.
- **MSMPEG4 coded-block allocation is checked.** `MSMPEG4_Init` NULL-checks `m_pCodedBlockBase` and routes to `allocation_error`; contrast with CAND-W3-4 is what makes those sites stand out.
- **libmpeg empty extension handlers are intentional, not dead.** `_ext_unknown`/`_ext_seq_scl`/`_ext_pic_ssc`/`_ext_pic_tsc` (`src/libmpeg.c:428-431, 556-558, 621-627`) are live dispatch targets that deliberately ignore unsupported extension blocks. Left alone.
- **Harmless port leftover, not a candidate.** `src/SMS_MSMPEG4.c:1926` passes `&s->gb` (an FFmpeg-ism; no such variable exists) to `SMS_LAST_SKIP_BITS`, whose macro definition (`include/SMS_Bitio.h:37`) ignores the `gb` argument entirely, so it compiles and behaves correctly. Worth a one-token tidy (`lpBitCtx`) only if the file is touched for another reason.
- **Bit-reader overread is a design assumption, noted not flagged.** The `SMS_Bitio.h` readers fetch past the nominal end (FFmpeg-style), relying on padded input buffers — the packet path does free/alloc with +64 slack (`src/SMS_MSMPEG4.c`, `SMS_RingBufferFree(apInput, lpPkt->m_Size + 64)`). No in-scope site was found feeding it an unpadded buffer.
