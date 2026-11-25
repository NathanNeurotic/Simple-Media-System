# SMS Round 8 — Section: AC3 IMDCT full read (Task 2) + Round-7 not-re-derived items settled (Task 5)

Repo: `Simple-Media-System` @ `e65ced39a27eee8d996e9257e21594d1cac1c93d` (HEAD re-verified; `git status` clean before and after; read-only throughout — the only writes in this section were throwaway host-side checks in `/tmp`, never in the clone).
Method: `src/SMS_AC3_imdct.c` read line by line, all 642 lines; every index, loop bound and table access traced to a constant or a caller contract, and the callers in `src/SMS_AC3.c` read at each call site. Task-5 items re-derived from primary in-tree sources (register headers, `SMS_Data.c` itself, SPRAM layout constants), not from round-7 summaries. Nothing here is build- or hardware-verified (no PS2SDK in this VM); where a host-gcc run was used as a measuring instrument it is labelled as such.
Existing findings SMS-001…157 are not repeated; SMS-148 (SMS_DXSB.c logic) is untouched — Task 5(ii) is the header offsets only, as briefed.

**Candidate count: 3, all from Task 2 (CAND-T2-1…3). Task 5 produced zero candidates: all four items SETTLED-consistent.**

---

## TASK 2 — `src/SMS_AC3_imdct.c` full line read (642 lines)

### Verdict on the round-7 claim ("fixed-size transforms — no file-controlled sizes")

**Verified, line by line.** The file contains no parameter, global, or stream-derived value that controls any loop bound or index:

- The only size-like parameter anywhere is `_ifft_pass(apBuf, apWeight, aN)` (`:464`), and its four call sites pass literal `aN` ∈ {4, 8, 16, 32} from the fixed `_ifft16/32/64/128` ladder (`:498, :507, :516/:525, :529`). Its `do { … } while (--i)` (`:480–489`, `i = aN − 1`) is safe at every reachable `aN` (all ≥ 4).
- The two public entry points take only pointers + the (dead, see CAND-T2-1) bias: `_ac3_imdct_512` (`:533`) loops `i < 128` / `i < 64`; `_ac3_imdct_256` (`:579`) loops `i < 64` / `i < 32` — all literal bounds.
- Every static table's declared size equals its initializer count, verified by script over the source: `s_IMDCTWindow[256]`=256, `s_Roots16[3]`=3, `s_Roots32[7]`=7, `s_Roots64[15]`=15, `s_Roots128[31]`=31, `s_Pre1[256]`=256, `s_Post1[128]`=128, `s_Pre2[128]`=128, `s_Post2[64]`=64, `s_FFTOrder[128]`=128.
- `s_FFTOrder` is a compile-time permutation of exactly the 128 even numbers 0…254 (script-verified: 128 unique values, all even, set == `range(0,256,2)`). It is data, not file input — a crafted AC3 stream cannot change it.
- Index ceilings, traced per function (sample units, `sample_t` = `int32_t`, `include/SMS_AC3.h:63`):
  - `_ifft_pass` weight access `apWeight[2*i − aN]` (`:482`): in each iteration the two indices used are `apWeight[0]` and `apWeight[2i − aN]` against a base that has advanced once per completed iteration, so the absolute table indices touched are exactly `[0, aN−2]` (first iteration's second index is `2(aN−1) − aN = aN−2`: aN=4→2, 8→6, 16→14, 32→30; the last iteration's is `−(aN−2)` relative to the advanced base = absolute 0). The tables are sized `aN−1` (3/7/15/31), so every access is in-bounds, the final negative relative index being exactly the table base. Fragile-looking, correct.
  - `_ac3_imdct_512`: `s_Pre1[j+1]` ≤ 255 ✓; `apData[k]`, `apData[255−k]` with `k` ∈ evens 0…254 ✓ (256-sample contract); `lBuf[j+1]` ≤ 255, `lBuf[254−j+1]` ≤ 255 ✓ (workarea ≥ 256 samples — see below); post-loop `s_Post1[j+1]` ≤ 127 ✓; window indices ≤ 255 ✓; `apDelay[j+1]` ≤ 127 ✓.
  - `_ac3_imdct_256`: for `i < 64` the first 64 `s_FFTOrder` entries max out at 252 (read from the table), so `apData[k+1]` ≤ 253 and `apData[254−k]` ≥ 2 ✓; `s_Pre2[j+1]` ≤ 127 ✓; `lBuf1/lBuf2` are the two 128-sample halves of the same workarea and every `126−j+1` ≤ 127 stays in its half ✓; post-loop `s_Post2[j+1]` ≤ 63 ✓; window indices ≤ `129+j` = 191 ✓; `apDelay` indices ≤ 127 ✓.
  - `_ifft2` touches bytes 0…31 of its block, `_ifft4` bytes 0…31, `_ifft8/16/32/64/128` spans are exactly 16/32/64/128/256 samples by the decomposition at `:454–531` (each `_ifftN` = `_ifft(N/2)` on the low half + two `_ifft(N/4)` + one `_ifft_pass`, ceilings checked at the pass: max touched index `6·aN−1` relative — e.g. `_ifft128`'s final pass reaches sample 255 and no further).
  - Workarea contract: both entry points write through `s_AC3Ctx.m_pWorkarea` (`:539, :585–586`), set once at `src/SMS_AC3.c:351` to `g_pSynthBuffer`, which is either `SMS_AUD_SPR` = `0x700008B0` (`include/SMS.h:118`; 1,024 B needed ends at scratchpad offset 0xCB0, far inside the 16 KB SPRAM) or `SMS_SYNTH_BUFFER` (Task 5(iii): the synth region is 4,352 B by construction at `include/SMS_Data.h:55`, and `src/SMS_MP123Core.S`'s 4,352-byte memset of the same buffer confirms the budget). 256 samples = 1,024 B fits both with margin.
  - Caller contract: all five call sites (`src/SMS_AC3.c:2401, :2432, :2434, :2475–2480`) pass 256-sample slots of `s_Samples[16*256*12]` (`src/SMS_AC3.c:323`) for both `apData` and `apDelay`, including the LFE site's `lpSamples − 256`, which is exactly the reserved LFE slot. Consistent.
- Dead-code census of the file: every static helper is called (`_ifft2←_ifft8`, `_ifft4←_ifft8/_ifft16`, `_ifft8←_ifft16/_ifft32`, `BUTTERFLY←_ifft_pass`, `BUTTERFLY_HALF←_ifft8`), every table is read, and the `BUTTERFLY_*` asm globals have no callers outside this file (they must be link-visible for the in-file C calls; not dead). The only dead items in the file are CAND-T2-1 and CAND-T2-2 below.
- Duplication census: one true duplicated transform block (CAND-T2-3). `s_Pre2/s_Post2` are re-ordered subsets of the `s_Pre1/s_Post1` constants — derived data tables, not duplicated code; not filed.

### CAND-T2-1 — `_ac3_imdct_512` / `_ac3_imdct_256` take a bias parameter that is never used; the caller's per-channel bias computation is silently dropped

- **Category:** 1 (dead code)
- **Locations:** `src/SMS_AC3_imdct.c:533` and `:579` (signatures; `aBias` occurs nowhere else in either body, `:533–577` and `:579–642` — full-text search of the file returns only the two signature lines); callers `src/SMS_AC3.c:2401, :2426–2434, :2475–2480`; bias plumbing `src/SMS_AC3.c:546, :586, :2593` and `include/SMS_AC3.h:94` (`m_Bias`).
- **Problem:** Both IMDCT entry points accept `sample_t aBias` and ignore it. The caller side maintains the fiction that it matters: `_ac3_frame` stores the literal `384` passed at `src/SMS_AC3.c:2593` into `s_AC3Ctx.m_Bias`, and `_ac3_block` computes a per-channel `lBias` (`:2424–2426`, from `_ac3_downmix_coeff`'s channel mask) solely to pass it into these two functions — for channels that actually have coefficients the value goes nowhere. The one place the bias value has any effect is the *no-coefficient* fill at `src/SMS_AC3.c:2440`, which writes `lBias` directly and does not involve the IMDCT at all. (Why the value is inert even there is arithmetic, READ from `src/SMS_AC3.c`'s own converter: `_ac3_float_to_int_1/2` only arithmetic-shifts samples right 15 and clamps, so a raw bias of 384 contributes `384 >> 15 = 0` to every converted sample; 384 is the float-era liba52 magic number carried into a fixed-point port — `sample_t` here is `int32_t` with `SAMPLE(x) = x·2³⁰`, `include/SMS_AC3.h:59–63` — as a raw integer.) The parameter is therefore vestigial in this file regardless of what upstream liba52 did with its float bias.
- **Fix:** Delete the third parameter from both functions and from the two extern declarations at `src/SMS_AC3.c:174–175`, and drop the argument at the five call sites. Whether to also remove the now-fully-dead `m_Bias`/`lChanBias` plumbing in `SMS_AC3.c` is the coordinator's call — it is outside this file, and the `:2440` fill is the one site that would change bytes (from 384 to whatever replaces it; converted output is identical either way, per the shift arithmetic above).
- **Benefit:** Removes a misleading contract: today a reader (or a future port) reasonably believes per-channel bias is applied inside the transform and will debug audio levels in the wrong place. Net deletion, no new code.
- **Effort:** S · **Risk:** Low (parameter removal is mechanical; output samples provably unchanged because the value is never read) · **Confidence:** High (READ — signatures, bodies, callers and the converter all read; the "no audible effect" consequence is arithmetic on values read in-tree, not inference about intent).

### CAND-T2-2 — Dead instructions in `_ifft2` (and one in `BUTTERFLY_ZERO`): two loads and two `pcpyld`s in `_ifft2` are overwritten before any read

- **Category:** 1 (dead code)
- **Locations:** `src/SMS_AC3_imdct.c:290–295` (`_ifft2` inline asm), second site `:376` (`BUTTERFLY_ZERO` asm block).
- **Problem:** Straight-line register dataflow, readable directly from the listing. In `_ifft2`: `ld T0, 0(apBuf)` (`:290`) feeds only `pcpyld T0, T0, T0` (`:292`), whose result is overwritten by `ld T0, 16(apBuf)` (`:293`) before any instruction reads `T0`; identically `ld T1, 8(apBuf)` (`:291`) → `pcpyld T1, T1, T1` (`:294`) → overwritten by `ld T1, 24(apBuf)` (`:295`). (`T0`/`T1` are `$8`/`$9`, `include/SMS.h:288–289`; an EE `ld` fully rewrites the register including its upper half, so no part of the `pcpyld` result survives.) Four instructions — two of them scratchpad loads — execute on every `_ifft2` call (i.e. once per `_ifft8`, hence pervasively in both IMDCTs) and affect nothing. A consequence of the same dataflow, stated at its exact weight: as written, every store in `_ifft2` derives solely from the loads at offsets 16/24, so the block's first 16 bytes contribute to no output — whether the intended 2-point stage needed those inputs cannot be settled from this file (the stage is a hand-port of djbfft-style code), and this finding does **not** assert a transform bug; it asserts only that the four instructions are dead as written. Second site, same class, trivial: `BUTTERFLY_ZERO`'s `pxor $at, $at, $at` (`:376`) is immediately overwritten by `nor $at, $zero, $zero` (`:377`), which does not read `$at`.
- **Fix:** Delete the four dead instructions in `_ifft2` (the two loads at `:290–291` and the two `pcpyld`s at `:292, :294`) and the dead `pxor` at `:376`. Do not "repair" the stage by inventing the missing term — if the maintainer believes the stage itself is wrong, that is a separate hardware A/B question against a reference IMDCT, not a cleanup.
- **Benefit:** Five dead instructions removed from the hottest inner loop of AC3 decode; the `_ifft2` listing afterwards shows exactly the dataflow it actually implements, which is the precondition for anyone ever auditing the stage's correctness.
- **Effort:** S · **Risk:** Low for the deletion itself (dead by dataflow — register state after the block is unchanged); the surrounding stage's correctness is explicitly out of claim · **Confidence:** High on deadness (READ — complete instruction trace of both blocks); INFERRED on nothing else — intent is not claimed.

### CAND-T2-3 — `_ifft128` open-codes `_ifft64`'s entire body instead of calling it

- **Category:** 2 (duplication)
- **Locations:** `src/SMS_AC3_imdct.c:520–525` (`_ifft128`, first half) vs `:511–518` (`_ifft64` definition).
- **Problem:** `_ifft128`'s first four statements — `_ifft32(apBuf); _ifft16(apBuf + 64); _ifft16(apBuf + 96); _ifft_pass(apBuf, s_Roots64, 16);` (`:522–525`) — are token-for-token `_ifft64`'s body (`:513–516`, same order, same offsets, same table, same `aN`). The decomposition ladder elsewhere in the file is strictly hierarchical (`_ifft64` calls `_ifft32`, `_ifft32` calls `_ifft16`, …); this one level breaks the pattern, so any future fix or instrumentation of the 64-point stage must be made twice, in two places that are not adjacent.
- **Fix:** Replace `:522–525` with the single call `_ifft64 ( apBuf );` — a net deletion of three lines, no new code.
- **Benefit:** One definition of the 64-point stage; the ladder becomes uniform (`_ifft128` = `_ifft64` + `_ifft32` + `_ifft32` + final pass), matching how every other level is written.
- **Effort:** S · **Risk:** Low (statement-for-statement identical; `_ifft64` is already a non-inlined static in this file, so the call is the same code path) · **Confidence:** High (READ — both bodies read side by side).

---

## TASK 5 — Round-7 not-re-derived items, settled

### T5(i) `DSP_PackAddMB` VU-side block offsets — **SETTLED-consistent** (EE-side offsets fully derived in-tree; VU data addresses precisely non-derivable, stated below)

Round 7 recorded: "`DSP_PackAddMB`'s fixed block-type offsets `0x0890…0x089A($a1)` (`src/SMS_DSP_MMI.S`, `:269` region) — no C-side structure defines them, so no mismatch can be asserted either way." That is superseded: a C-side definition **does** exist, one level of indirection away, and the offsets derive exactly.

**Derivation (all READ):**

1. The routine: `src/SMS_DSP_MMI.S:269–311` (`DSP_PackAddMB:` at `:269`). For block `b` = 0…5 it executes `lb $at, 0x0890+2b($a1)` and, if the byte is ≥ 0 (`bgezall`), calls `_dsp_pack_addY` (b ≤ 3) or `_dsp_pack_addC` (b ≥ 4) with VU data address `16 + 16b` (the `addiu $v1, $zero, 16/32/48/64/80/96` sequence) and destination `a2 + {0, 8, 128, 136, 256, 320}`.
2. The sole caller fixes `$a1`: `SMS_MPEG_MCB` (`src/SMS_MPEG.c:1208–1233`) sets `lui $a1, 0x7000` (`:1214`) immediately before `jal DSP_PackAddMB` (`:1215`), and `$a0` = the destination macroblock (the routine's first parameter, `include/SMS_DSP.h:85`). So the six bytes are SPRAM addresses `0x70000890…0x7000089A`, stride 2 bytes = one `short` each.
3. What lives at SPRAM 0x890 is defined in C: `SMS_MPEG_DecodeMB` (`src/SMS_MPEG.c:1138–1142`) copies `g_MPEGCtx.m_BlockLastIdx` into the SPRAM block area every macroblock — `(u64*)SMS_MPEG_SPR_BLOCKS)[98] = ((u64*)&m_BlockLastIdx)[0]` and `(u32*)SMS_MPEG_SPR_BLOCKS)[198] = ((u32*)&m_BlockLastIdx)[2]`. With `SMS_MPEG_SPR_BLOCKS = (SMS_DCTELEM*)0x70000580` (`include/SMS.h:117`), u64 index 98 is byte `0x580 + 98·8 = 0x580 + 0x310 = 0x890`, and u32 index 198 is byte `0x580 + 792 = 0x898` — together exactly the 12 bytes `0x890…0x89B`. (Cross-check: the block array itself, `m_pBlocks = SPR_BLOCKS + 8` elements = byte 0x590, 6 blocks × 64 `short`s × 2 B = 768 B, ends at 0x890 — the copy lands immediately after the last block, which is why `SMS_MPEGInit.c:76–87` also writes those same u64/u32 slots, indices 196–203 of the `u32` view = bytes 0x890…0x89F, as part of the SPRAM packet template.)
4. The array copied is `short m_BlockLastIdx[6]` (`include/SMS_MPEG.h:128`) — the last non-zero coefficient index per block, or −1 for an uncoded block (setters: `src/SMS_MPEG4.c:1826–1832` `i = −1; if (!aCoded) { m_BlockLastIdx[aN] = i; … }`, final store `:2026` with `m_BlockSum += (i != −1)`; same pattern `src/SMS_MSMPEG4.c:1843–1848, :1968–1969`). Its value range is exactly {−1} ∪ [0, 63] (a scan index into a 64-coefficient block; the AC-pred path forces 63 at `src/SMS_MPEG4.c:2024`).
5. The asm's test is therefore exact, not approximate: on little-endian EE, byte `0x890+2b` is the low byte of `m_BlockLastIdx[b]`; `lb` sign-extends that byte, and for the value range {−1} ∪ [0, 63] the low byte's sign **is** the value's sign (0xFF for −1, 0x00…0x3F otherwise). `bgezall` thus adds precisely the coded blocks and skips precisely the uncoded ones — which is the routine's documented purpose (Pack**Add** only blocks the VU IDCT actually produced). The pairing is also consistent on both other axes: block order 0…5 = 4 luma + Cb + Cr matches the destination offsets {0, 8, 128, 136} (the four 8×8 quadrants of `m_Y[16][16]`) and {256, 320} (`m_Cb`/`m_Cr` at byte 256/320 of `SMS_MacroBlock`, `include/SMS.h:198–204`, sizeof 384) in the destination macroblock `$a0`.

**What cannot be derived in-tree, stated precisely:** the VU0 data-memory addresses paired with the blocks (16, 32, 48, 64, 80, 96 qwords, loaded into `$vi1` via `ctc2`). The VU0 IDCT microprogram that writes that memory ships only as the precompiled object `vu/IDCT.o`; there is no VU source (`.vsm`/`.dsm`) anywhere in the tree (`vu/` contains only `CSC1.o, CSC2h.o, CSC2v.o, CSC4.o, IDCT.o` + build scripts), and no C structure, DMA packet, or header names VU data addresses for the IDCT output. The only in-tree checks available are the ones above: the addresses are six evenly spaced 16-qword slots in block order, the same order as the (fully derived) EE-side flag bytes and destination offsets. No mismatch is assertable, and now — unlike round 7 — the EE-side half of the question is closed by derivation, not by absence of evidence.

**Verdict: SETTLED-consistent. No candidate filed.**

### T5(ii) `SMS_DXSB_DP_*` offsets (`include/SMS_DXSB.h:66–77`) — **SETTLED-consistent** (all 12 re-derived exactly)

**Step 1 — member sizes from the register headers (all READ):**

- `DMATag` (`include/SMS_DMA.h:52–66`): packed bitfields 16+10+2+3+1+31+1 = 64 bits over a `u64` union member → **8 bytes** (alignment attribute 16 — see step 3 for why this does not move any offset).
- `GIFTag` (`include/SMS_GS.h:142–179`): first word 15+1+30+1+11+2+4 = 64 bits, register nibbles 16×4 = 64 bits, `m_HiLo` = 2×`u64` → **16 bytes**.
- Every `GSReg*` in the pack is a union over one `u64 m_Value` whose bitfield struct sums to exactly 64 — verified per register: BITBLTBUF 14+2+6+2+6+2+14+2+6+2+6+2 (`SMS_GS.h:740–761`), TRXPOS 11+5+11+5+11+5+11+2+3, TRXREG 12+20+12+20 (`:788–801`), TRXDIR, TEXFLUSH (single-`u64` struct), PRIM, TEST, RGBAQ, TEX0 14+6+6+4+4+1+2+14+4+1+5+3 (`:861–882`), UV 14+2+14+34 (`:994–1007`), XYZ 16+16+32 (`:974–986`) → **8 bytes each**. Plain `u64` members: 8 bytes.

**Step 2 — sequential layout of `SMS_DXSBDrawPack` (`include/SMS_DXSB.h:30–64`):** accumulating the sizes in declaration order gives: DMATagUpload 0, Pad0 8, GIFTagUpload 16, BitBlt **32**, BitBltID 40, TrxPos 48, TrxPosID 56, TrxReg **64**, TrxRegID 72, TrxDir 80, TrxDirID 88, GIFTagData **96**, DMATagData **112**, Pad1 120, DMATagDraw 128, Pad2 136, GIFTagDraw0 144, TexFlush 160, TexFlushID 168, Prim 176, PrimID 184, TestOn 192, TestOnID 200, GIFTagDraw1 208, RGBAQ 224, Tex0 **232**, UVLeftTop 240, XYZLeftTop **248**, UVRightBottom **256**, XYZRightBottom **264**, GIFTagDraw2 272, TestOff 280, TestOffID 288; total 304 bytes.

**Step 3 — the padding question (the one place this could have broken):** `DMATag`/`GIFTag` carry `aligned(16)` and the pack `aligned(64)`. No padding is in fact inserted anywhere in this struct: every 16-aligned member lands on a 16-multiple under the sequential layout anyway (GIFTagUpload 16, GIFTagData 96, DMATagData 112, DMATagDraw 128, GIFTagDraw0 144, GIFTagDraw1 208, GIFTagDraw2 272), by construction of the Pad fields. This was not left as reasoning: the exact header definitions (same packed bitfields, same `aligned` attributes) were compiled with host gcc 13.3 and `offsetof`/`sizeof` measured — `sizeof(DMATag)=8` (the packed members keep the union at 8 despite `aligned(16)`), `sizeof(GIFTag)=16`, `sizeof(SMS_DXSBDrawPack)=304`, and every member offset identical to step 2. EE gcc implements the same attribute/packing rules and the struct contains only fixed-width types, so the target layout is the measured one; the residual (no PS2SDK in this VM to compile with the target compiler) is labelled INFERRED, the derivation itself is READ.

**Step 4 — each macro vs the field it names:**

| Macro (header line) | Offset | Derivation | ✓ |
|---|---|---|---|
| `SMS_DXSB_DP_BB_VRAM` (:66) | 36 | BitBlt@32 + 4: DBP sits at register bits 32…45 (after SBP14+pad2+SBW6+pad2+SPSM6+pad2) → byte 36, `u16` covers DBP+pad2 | ✓ |
| `SMS_DXSB_DP_BB_TBW` (:67) | 38 | BitBlt@32 + 6: DBW at bits 48…53 → byte 38, `u8` covers DBW+pad2 | ✓ |
| `SMS_DXSB_DP_TX_W` (:68) | 64 | TrxReg@64 + 0: RRW at bits 0…11 → `u16` at 64 | ✓ |
| `SMS_DXSB_DP_TX_H` (:69) | 68 | TrxReg@64 + 4: RRH at bits 32…43 → byte 68 | ✓ |
| `SMS_DXSB_DP_QWC_GIF` (:70) | 96 | GIFTagData@96 + 0: NLOOP bits 0…14 — for this tag's FLG=IMAGE mode (initializer, `src/SMS_IPU.c:62`) NLOOP *is* the image qword count | ✓ |
| `SMS_DXSB_DP_QWC_DMA` (:71) | 112 | DMATagData@112 + 0: QWC bits 0…15 | ✓ |
| `SMS_DXSB_DP_PTR_DMA` (:72) | 116 | DMATagData@112 + 4: ADDR at bits 32…62 → byte 116; the `u32` write also covers SPR (bit 63), which a plain EE pointer correctly leaves 0 | ✓ |
| `SMS_DXSB_DP_TEX0` (:73) | 232 | Tex0@232 exactly, `u64` | ✓ |
| `SMS_DXSB_DP_XYZ_LT` (:74) | 248 | XYZLeftTop@248 exactly, `u64` | ✓ |
| `SMS_DXSB_DP_UV_R` (:75) | 256 | UVRightBottom@256 + 0: U at bits 0…13 → `u16` | ✓ |
| `SMS_DXSB_DP_UV_B` (:76) | 258 | UVRightBottom@256 + 2: V at bits 16…29 → byte 258 | ✓ |
| `SMS_DXSB_DP_XYZ_RB` (:77) | 264 | XYZRightBottom@264 exactly, `u64` | ✓ |

**Consumer consistency:** the single consumer, `IPU_DrawSub` (`src/SMS_IPU.c:313–338`), applies the macros to `lDst = (unsigned int)&s_DXSBDPack | 0x20000000` (uncached alias of the static pack whose designated initializer is at `src/SMS_IPU.c:51–124`) — i.e. `p` really is the struct base the offsets were derived against, and the fields it patches at runtime (TBW, TRXREG halves, both QWCs, DMA pointer, TEX0, XYZ/UV corners) are exactly the ones the static initializer leaves unset or per-frame (`m_TrxReg` has no `.m_Value` initializer — its two halves are the TX_W/TX_H macro writes). One observation, not a defect: `SMS_DXSB_DP_BB_VRAM` is the only one of the twelve with no call site (the initializer likewise never sets `m_BitBlt`'s DBP half); its offset is nevertheless correct per the derivation, and SMS_DXSB.c logic is SMS-148's territory, not this item's.

**Verdict: SETTLED-consistent — all 12 offsets exact. No candidate filed.**

### T5(iii) `include/SMS_Data.h:33–56` object-like chains — **SETTLED-consistent** (chain evaluated by hand; byte-exact against `SMS_Data.c`; consumers consistent)

**Hand evaluation of the chain** (each `OFFSETₙ = (OFFSETₙ₋₁ + SIZEₙ₋₁ + 15) & 0xFFFFFFF0`, i.e. align-16 of the previous segment's end; sizes from `SMS_Data.h:14–30`):

| Module | Offset (dec) | Size | End | Pad to next |
|---|---|---|---|---|
| PS2ATAD | 0 | 11,821 | 11,821 | 3 |
| PS2HDD | 11,824 | 24,953 | 36,777 | 7 |
| PS2FS | 36,784 | 50,405 | 87,189 | 11 |
| USB_MASS | 87,200 | 8,504 | 95,704 | 8 |
| USBD | 95,712 | 9,140 | 104,852 | 12 |
| PS2IP | 104,864 | 67,061 | 171,925 | 11 |
| PS2SMAP | 171,936 | 7,909 | 179,845 | 11 |
| PS2HOST | 179,856 | 16,349 | 196,205 | 3 |
| VU0_MPG | 196,208 | 1,376 | 197,584 | 0 |
| VU0_DATA | 197,584 | 256 | 197,840 | 0 |
| SMB | 197,840 | 19,641 | 217,481 | 7 |
| SMSUTILS | 217,488 | 3,733 | 221,221 | 11 |
| AUDSRV | 221,232 | 8,989 | 230,221 | 3 |
| PS2DEV9 | 230,224 | 8,149 | 238,373 | 11 |
| POWEROFF | 238,384 | 2,229 | 240,613 | 11 |
| CDVD | 240,624 | 20,093 | 260,717 | 3 |
| SIO2MAN | 260,720 | 5,817 | 266,537 | — |

`SMS_DATA_BUFFER_SIZE = (260,720 + 5,817 + 63) & ~63 = 266,560` (align-64 of the final end). Sum of the 17 SIZE constants = 266,425, agreeing with SMS-001's figure. Every offset is 16-aligned, segments are in declaration order, none overlaps its predecessor.

**Byte-exact check against the blob itself (stronger than the arithmetic):** counting the hex bytes between the per-module comments in `src/SMS_Data.c` reproduces the table exactly — each segment starts at its chain offset, contains exactly `SIZE` bytes, and is followed by a literal `/* padding for 16 byte alignment */` run of exactly the tabulated pad (3, 7, 11, 8, 12, 11, 11, 3, —, —, 7, 11, 3, 11, 11, 3), with no padding comment at the two zero-pad joints (VU0_MPG→VU0_DATA→SMB), as predicted. Total explicit initializer bytes = 266,544 = align-16 of the final end; the remaining 16 bytes to 266,560 are the array's implicit zero tail. The header chain and the 16,728-line data file are in exact agreement at this pin.

**Consumers use the pairs consistently:** every module is consumed as `(&g_DataBuffer[OFFSET], SIZE)` — the module table at `src/SMS_IOP.c:180–185` (AUDSRV, PS2DEV9, POWEROFF, PS2ATAD, PS2HDD, PS2FS), `SifExecModuleBuffer` sites at `src/SMS_IOP.c:472` (SMSUTILS), `:504` (SIO2MAN, non-BDM branch), `:576` (PS2IP), `:580` (PS2SMAP), `:601` (PS2HOST), `SifExecDecompModuleBuffer` at `:674` (USBD), `:688` (USB_MASS), `src/SMS_CDVD.c:77` (CDVD), and `memcpy` to VU0 memory at `src/SMS_MPEG4.c:520–521` (VU0_MPG, VU0_DATA). Sole exception, already on the record: `SMS_SMB_OFFSET/SMS_SMB_SIZE` have no consumer anywhere in `src/` — that is the dead-segment territory of SMS-037 (round 2), not a chain error, and is not re-filed here.

**The two derived buffers at `:54–56`, evaluated:** `SMS_SYNTH_BUFFER = &g_DataBuffer[217,488]` (the SMSUTILS offset); `SMS_AUDIO_BUFFER = align64(SYNTH + 4352) = &g_DataBuffer[221,888]`; `SMS_AUDIO_BUFFER_SIZE = 266,560 − 221,888 = 44,672`. Both derived regions deliberately overlap blob segments: the 4,352-byte synth span (217,488…221,840 — the 4,352 figure is corroborated by `SMS_MP123Core.S`'s memset of the same buffer, round 7) runs 619 bytes past the SMSUTILS segment's end, over its 11 pad bytes and the first 608 bytes of the AUDSRV segment; the audio buffer starts inside the AUDSRV segment (221,232…230,221) and runs to the end of the array. The consumers match that design exactly: `g_pSynthBuffer = SMS_SYNTH_BUFFER` only in the MPEG-PS container (`src/SMS_ContainerMPEG_PS.c:934`, the one path that does not use SPRAM for synthesis), and the player builds its audio ring over `SMS_AUDIO_BUFFER`/`SMS_AUDIO_BUFFER_SIZE` (`src/SMS_Player.c:300`) — i.e. both are playback-time reuses of EE-side blob space whose IOP modules are exec'd from it during `SMS_IOPReset`, before playback. SMS-037's fix constraint already records this layout relationship ("the audio ring buffer lives after this blob in the same array — SMS_Data.h's layout comments must be respected"); the chain arithmetic itself contains no error, and no consumer reads a module through a mismatched offset/size pair.

**Verdict: SETTLED-consistent. No candidate filed.**

### T5(iv) Boundary headers `include/libds34bt.h`, `include/libds34usb.h`, `include/mbstring.h` — **SETTLED-consistent** (zero function-like macros exist to evaluate; declarations match implementations and callers)

Exact paths (all in `include/`, as briefed): `include/libds34bt.h` (35 lines), `include/libds34usb.h` (21 lines), `include/mbstring.h` (27 lines). SMS ships the ds34 IOP side as precompiled `irx/ds34bt.irx.xz` / `irx/ds34usb.irx.xz` (Makefile `:52, :58`); the EE client libraries `src/libds34bt.c` / `src/libds34usb.c` are the implementations in scope alongside the headers.

**Macro bar — cannot trigger, by enumeration:** a full read of all three headers finds **zero function-like macros** (and zero object-like macros other than `mbstring.h`'s include guard). The ds34 headers contain no `#define` at all. There is therefore no macro argument to overflow an allocation/index context, no argument to double-evaluate, and no parenthesisation hazard at any call site — the round-7 macro bar has nothing to act on in these three files.

**Declarations vs implementations (each checked 1:1):**

- `libds34bt.h` declares 11 functions; `src/libds34bt.c` defines exactly those 11 with identical names, parameter types and order (`init/deinit/init_charging/reset` as `()`, `reinit_ports(u8)`, `get_status(int)`, `get_bdaddr(u8*)`, `set_rumble(int,u8,u8)`, `set_led(int,u8*)`, `get_data(int,u8*)`, `get_version(hci_information_t*)`, `get_features(u8*)` — header `:24–35` vs `src/libds34bt.c:29, 50, 56, 65, 73, 86, 102, 114, 128, 144, 152, 168`). The header's one type, `hci_information_t` (`:12–22`, `__attribute__((packed))`), sums to 1+2+1+2+2+2+2+2 = 14 bytes packed, and its only consumer uses it self-consistently through `sizeof`: `ds34bt_get_version` receives `1 + sizeof(hci_information_t)` bytes over RPC and `memcpy`s `sizeof(…)` (`src/libds34bt.c:159–162`) — the EE side never hand-assumes the 14.
- `libds34usb.h` declares 10 functions; `src/libds34usb.c` defines exactly those 10 identically (`:26, 47, 53, 62, 75, 94, 109, 121, 132, 148`). The visible asymmetries against the bt header are per-module API facts, not mismatches: usb `get_bdaddr` takes `(int port, u8*)` and usb has a `set_bdaddr` the bt module lacks, while bt `get_bdaddr` takes `(u8*)` only — and both implementations and both call sites agree with their own header (below).
- `mbstring.h` declares 4 functions; `src/mbstring.c:14, 28, 53, 78` defines exactly those 4 with identical signatures (`_mbstrlen(const char*)`, `_mbstrspnp(const char*, const char*)`, `_mbstrpbrk(const char*, const char*)`, `_mbstrtok(char*, const char*)`).

**Declarations vs callers:** `src/SMS_PAD.c:190–197, 225–227` calls `ds34usb_get_status(0)`, `ds34usb_get_data(0, buf)`, `ds34bt_get_status(0)`, `ds34bt_get_data(0, buf)`, `ds34usb_init()`, `ds34bt_init()` — all per the declared signatures, with the status enums from the same headers. `src/SMS_GUIMenuSMS.c:1659–1661` calls `ds34bt_get_bdaddr(lDongle)`, `ds34usb_get_bdaddr(0, lPad)`, `ds34usb_set_bdaddr(0, lDongle)` — the bt/usb signature difference is honoured correctly at the one call site that uses both. `src/SMS_SubtitleContext.c:968–969` stores `_mbstrtok`/`_mbstrlen` into function pointers declared at `:40–41` as `char* (*)(char*, const char*)` and `unsigned int (*)(const char*)` — an exact type match with `mbstring.h`, so the assignments need no cast and hide no signature drift. The empty-parens `()` declarations in the ds34 headers (unspecified parameters in C) match their `()` definitions in the vendored OPL-derived client libs; every in-tree caller passes the correct (zero) arguments. Changing them to `(void)` would be cosmetic churn against vendored headers with no call-site effect — under LESS-NOT-MORE that is not a finding.

**Verdict: SETTLED-consistent for all three headers. No candidate filed.**

---

## Section summary

- **Task 2:** fixed-size claim **verified line by line** (no file- or caller-controlled size reaches any index or loop; all tables exact-fit; all index ceilings traced). 3 candidates: CAND-T2-1 (dead `aBias` parameter in both IMDCT entry points), CAND-T2-2 (dead instructions in `_ifft2` + `BUTTERFLY_ZERO`), CAND-T2-3 (`_ifft128` duplicates `_ifft64`'s body).
- **Task 5:** (i) SETTLED-consistent — PackAddMB flag offsets derived exactly from `m_BlockLastIdx[6]` → SPRAM 0x890 copy; VU data addresses alone remain non-derivable (binary-only `vu/IDCT.o`, no VU source in tree), reason stated. (ii) SETTLED-consistent — all 12 `SMS_DXSB_DP_*` offsets re-derived exactly from register bitfield sums + sequential layout, host-gcc `offsetof`-confirmed. (iii) SETTLED-consistent — SMS_Data.h chain hand-evaluated and byte-exact against SMS_Data.c segments/padding; consumers consistent (SMB's missing consumer is SMS-037's, not re-filed). (iv) SETTLED-consistent — the three boundary headers contain no function-like macros at all; all 25 declarations match their EE implementations and every call site.
- **Candidates filed: 3 (CAND-T2-1…3). CAND-T5-*: none.**
