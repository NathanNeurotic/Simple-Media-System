# SMS Round 7 — Section: Assembly (Scope A) + Header Macros (Scope B)

Repo: `Simple-Media-System` @ `e65ced39a27eee8d996e9257e21594d1cac1c93d` (HEAD verified, `git status` clean before and after; read-only).
Method: every in-scope `.S`/`.s` file read (ledger at the end states exactly how); a scripted census over all `.c/.h/.S/.s` in the tree counted references to every exported symbol — a header declaration alone does **not** count as a caller. Claims are labelled READ (seen in the source) vs INFERRED (mechanism/consequence reasoning).

---

## Scope A — Assembly

### Candidate ASM-1 — `DSP_FFTGet` loop count comes from an uninitialised register (READ mechanism, INFERRED consequence)

- `src/SMS_DSP_FFT.S:337` `DSP_FFTGet` entry; at `:363` the first loop-count operation is `addiu $v0, $v0, 32` — executed **before `$v0` is set anywhere in the routine**. The loop at `:364–515` runs while `$v0 > 0` (`addiu $v0,$v0,-1` at `:512`, `bgtz $v0, 1b` at `:514`), advancing `$a0` by 128 and the scratch pointer (0x7000…) by 64 per iteration.
- Prototype `include/SMS_DSP.h:82`: `void DSP_FFTGet ( void* )` — no return value, so `$v0` is not an argument register the caller must set. Sole caller `src/SMS_Spectrum.c:82–84` calls `DSP_FFTInit (…)`, `DSP_FFTRun (…)` and `DSP_FFTGet ( g_FFTWsp )` as three separate C calls; nothing in the C ABI guarantees `$v0` on entry to the third call.
- Intended count is evidently 32 (workspace is 512 complex × 8 B = 4,096 B; 32 × 128 B = 4,096 B). INFERRED: with garbage in `$v0`, the loop under- or over-runs the 4,096-byte FFT workspace — under-run leaves stale spectrum data, over-run writes past `g_FFTWsp` (which sits at `&g_SMSounds[64032]`, `include/SMS_Sounds.h:39`). It evidently works in practice because the preceding call chain happens to leave a benign `$v0`; that is luck, not a contract. Fix is one instruction (`li $v0, 32` instead of the `addiu`).

### Candidate ASM-2 — Asm `s_GSParams` layout diverges from C `GSParams` after the 576P slot (READ both sides; latent today)

- Asm: `src/SMS_GS_0.S:21–22` `s_GSParams: .space 32`, followed by PAR words at `:23–29` in the order NTSC, PAL, 480P, **576P**, VESA, 720P, 1080I (offsets 32, 36, 40, **44**, 48, 52, 56).
- C: `include/SMS_GS.h:1173–1189` `GSParams` — fields end at offset 31, then `m_PARNTSC` 32, `m_PARPAL` 36, `m_PAR480P` 40, **`m_PARVESA` 44, `m_PAR720P` 48, `m_PAR1080I` 52** — the C struct has **no 576P field**, so from offset 44 on, every asm PAR word sits one slot earlier than the C field of the same name, and asm's 1080I word at 56 lies beyond the C struct's PAR fields entirely.
- Why latent, not active (READ): all seven asm PAR words are `0x3F800000` (1.0f), the only C writes are `m_PARNTSC`/`m_PARPAL` at `src/SMS_GS_1.c:94–95` (offsets 32/36 — these match), and no C code reads `m_PAR480P/VESA/720P/1080I`. `GS_Reset` (`src/SMS_GS_0.S:74–141`) writes offsets 0, 2, 4, 6, 8, 12 and copies the selected PAR into 16/20/24/28. The first future C read or per-mode write of `m_PARVESA` or later will silently hit the wrong mode's value. Related C-side config: `g_Config.m_PAR[12]` at `include/SMS_Config.h:99`, defaults at `src/SMS_Config.c:623–624`.

### Candidate ASM-3 — Dead routine: `GS_XYZF` (READ, census)

- `.globl` at `src/SMS_GS_0.S:52`, body at `:449`. Only other reference in the tree is its declaration in `include/SMS_GS.h`. No C caller; no asm-internal caller (the routine tail-calls `_gs_xyz`; nothing calls it back). Whole routine is deletable together with the declaration.

### Candidate ASM-4 — Dead routine: `SMS_iTimerReset` (READ, census)

- `.globl` at `src/SMS_Timer.s:28`, body at `:154`. Only other reference is its declaration in `include/SMS_Timer.h`; no caller anywhere in EE or IOP code. (Contrast `SMS_TimerReset`, same file, which has callers in `src/SMS_GUI.c`, `src/SMS_Player.c`, `src/SMS_PlayerControl.c`.)

### Candidate ASM-5 — Dead routine: `inet_ntoa` (READ, census)

- `.globl` at `iop/SMSTCPIP/src/inet.s:18`, body `:618–667` (~49 lines). Only other reference is its declaration in `iop/SMSTCPIP/include/lwip/inet.h`; no C caller and no asm-internal caller — the only fully dead routine in `inet.s`. (Its static `s_Str: .space 16` at `:25` is exactly sized for "255.255.255.255\0"; not itself a defect.)

### Candidate ASM-6 — Dead export, live body: `GS_RRV` (READ)

- `.globl` at `src/SMS_GS_0.S:60`; label `GS_RRV:` at `:828` shares the body with `_gs_rrv:` at `:829`. No C caller of the `GS_RRV` name (declaration only in `include/SMS_GS.h`); the body is live via `_gs_rrv`, called by `GS_RenderRoundRect_asm` (`:1037`). Remedy is deleting the export + declaration, not the body.

### Candidate ASM-7 — Dead export, live body: `_MPEG_AlignBits` (READ)

- `.globl` at `src/libmpeg_core.S:23`, body at `:501` under the shared label `_ipu_align_bits:` (`:502`). No C caller of the `_MPEG_AlignBits` name (declaration only in `include/libmpeg_internal.h`); the body is live via `_ipu_align_bits`, called by `_MPEG_NextStartCode` at `src/libmpeg_core.S:524`.

### Candidate ASM-8 — Dead export, live body: `ip_cksum` (READ)

- `.globl` at `iop/SMSTCPIP/src/inet.s:12`; label `ip_cksum:` at `:53` shares the body with `_ip_cksum:` at `:54`. No caller of the `ip_cksum` name anywhere; the body is live via `_ip_cksum`, called internally by `inet_chksum` (`:227`), `inet_chksum_pseudo` (`:266`) and `inet_chksum_pbuf` (`:356`).

### Candidate ASM-9 — Dead export, live body: `inet_aton` (READ)

- `.globl` at `iop/SMSTCPIP/src/inet.s:16`; label `inet_aton:` at `:397` shares the body with `_inet_aton:` at `:398`. No C caller of the `inet_aton` name (declaration only in `iop/SMSTCPIP/include/lwip/inet.h`); the body is live via `_inet_aton`, called by `inet_addr` at `:607`.

### Per-file results (symbols, callers, constants)

**`src/SMS_GS_0.S` (1,180)** — Exports: `g_XShift` (data), `GS_Reset`, `GS_Params`, `GS_InitDC`, `GS_SetDC`, `GS_XYZ`, `GS_XYZF`, `GS_XYZv`, `GS_InitGC`, `GS_SetGC`, `GS_InitClear`, `GS_Clear`, `GS_InitLoadImage`, `GS_LoadImage`, `GS_RRV`, `GS_RenderRoundRect_asm`, `GS_VSync`, `GS_L2P`, `GS_VSync2`, `GS_VMode2Index`. All have C callers except ASM-3/ASM-6 above. Constants checked: `GSRoundRectPacket.m_XYZ[28]` (`include/SMS_GS.h:1125+`) vs the round-rect writer's entry count — consistent; packet structs `GSDC/GSGC/GSClearPacket/GSStoreImage/GSTexSpritePacket/GSLoadImage` (`include/SMS_GS.h:1050–1171`) vs asm packet writes — no mismatch found; `GS_VMode2Index` (`:1149`) mapping NTSC 0, PAL 1, 480P 2, 576P 3, 720P 4, 1080I 5, VESA60 6, VESA75 7, default 8 — consistent with the `g_Config.m_DispWH` indexing at `src/SMS_GS_1.c:102–107`. Plus ASM-2.

**`src/SMS_Utils.S` (1,307)** — Exports: `MUL64`, `PowerOf2`, `PowF`, `mips_memcpy`, `mips_memset`, `memcpy_swap16`, `pcm_syn1`, `pcm_syn2`, `pcm_synN`, `strlen`, `strcat`, `strcpy`, `strncmp`, `strncpy`, `strchr`, `strcmp`, `strrchr`, `strstr`, `strpbrk`, `memmove`. All have callers across the EE and IOP trees; no dead exports. Usage note (not a defect): `pcm_synN` is called only from `src/SMS_AAC.c` (×2), whereas `pcm_syn1`/`pcm_syn2` are used by both AAC and WMA paths.

**`src/SMS_DSP_MMI.S` (2,067)** — Exports (37): the `DSP_PutPixels*` / `DSP_PutNoRndPixels*` / `DSP_AvgPixels*` families (16/8/8_16 × plain/X/Y/XY), `DSP_GMC1_16`, `DSP_GMC1_8`, `DSP_PackMB`, `DSP_PackAddMB`. All 37 are live — each is referenced in C as a function-pointer assignment in `SMS_DSPContextInit` (`src/SMS_DSP.c`) plus its `include/SMS_DSP.h` declaration. Constants checked: the hard-coded `384` second-block stride throughout matches `sizeof(SMS_MacroBlock)` = 384 (`m_Y[16][16]` + `m_Cb[8][8]` + `m_Cr[8][8]`, `include/SMS.h:198–204`), as do the `+64`/`+448` chroma offsets in the 8-wide variants; the `SMS_DSPContext` table offsets commented at `include/SMS_DSP.h:38–50` (528 B total; QPel tabs 0/128/256, OpPix tabs 384/432/480) match `SMS_DSPContextInit`. Calling convention checked and **not** a defect: routines take the 5th argument in `$t0` (e.g. `DSP_PutPixels16`, `:330`), which is correct under the file's `#define ABI_EABI64` (`:28`); C prototypes pass 5 args (`include/SMS_DSP.h:93`) and call sites pass `g_MPEGCtx.m_LineStride` as arg 5 (`src/SMS_MPEG.c:522`, `:664–665`, `:774`, `:801`). Not cross-checked: `DSP_PackAddMB`'s fixed block-type offsets `0x0890…0x089A($a1)` (`:269` region) against the VU-side layout — no C-side structure defines them, so no mismatch can be asserted either way.

**`src/SMS_DSP_QPel.S` (4,728)** — Exports: the 90 `DSP_{Put,PutNoRnd,Avg}QPel{16,816}MC{0..3}{0..3}` wrappers. All 90 are live via the QPel function-pointer tables in `SMS_DSPContextInit` (`m_PutQPelPixTab` / `m_PutNoRndQPelPixTab` / `m_AvgQPelPixTab` assignments spanning `src/SMS_DSP.c:415–514`). Internal helpers (`_copy16`, `_copy8_16`, `_put_qpel*`, `_avg_qpel*`, lowpass variants) all have asm-internal callers. Sibling check: all 90 wrapper bodies were signature-clustered (helper call sequence + saved-register set); the pattern is fully systematic in the MCxy digits (x-frac selects horizontal pass, y-frac vertical), the `s3`-saving wrappers are exactly the `Avg…816MC{02,12,22,32}` group that needs the extra rounding register, and spot-read bodies (e.g. `DSP_PutQPel16MC02`, `DSP_AvgQPel816MC02`, `DSP_AvgQPel816MC03`) save/restore correctly — no divergent wrapper found. SPRAM filter constants at `0x70000220…` match the C table `s_QPelConst` (48 halfwords = 96 bytes, `src/SMS_DSP.c:362–369`), copied to `0x70000220` at `src/SMS_DSP.c:373`.

**`src/SMS_DSP_FFT.S` (617)** — Exports: `DSP_FFTInit`, `DSP_FFTRun`, `DSP_FFTGet` — all called from `src/SMS_Spectrum.c`. ASM-1 is in `DSP_FFTGet`. Sizes checked clean: `s_BitRev` table (`:163–226`) is 512 halfwords; `DSP_FFTInit` (`:237`) loops 128 × 4 entries writing 4 complex (32 B) per iteration = 512 complex out, matching the 4,096-byte workspace.

**`src/SMS_MP123Core.S` (1,045)** — Exports: `MP123_CoreInit`, `MP123_IMDCT36`, `MP123_IMDCT12`, `MP123_Synth` — all called from `src/SMS_MP123.c` (CoreInit ×2, IMDCT36 ×5, IMDCT12 ×3, Synth ×10). Constants noted, no mismatch: `MP123_CoreInit` (`:189`) memsets 4,352 bytes at `g_pSynthBuffer` (`.extern` at `:24`; C decl `include/SMS.h:25`); `MP123_Synth` (`:538`) uses offsets 2,176 and 1,088 (= 4352/2, /4) and returns `16 << (a3>>1)` at `:1042–1045`; tables `s_Cos64/s_Cos36/s_Cos12/s_DecWin` at `:28–181`.

**`src/libmpeg_core.S` (2,324)** — Exports (45): `_MPEG_Initialize`, `_MPEG_Destroy`, `_MPEG_Flush`, `_MPEG_GetBits`, `_MPEG_ShowBits`, `_MPEG_AlignBits`, `_MPEG_NextStartCode`, `_MPEG_SetDefQM`, `_MPEG_SetQM`, `_MPEG_GetMBAI`, `_MPEG_GetMBType`, `_MPEG_GetMotionCode`, `_MPEG_GetDMVector`, `_MPEG_SetIDCP`, `_MPEG_SetQSTIVFAS`, `_MPEG_SetPCT`, `_MPEG_BDEC`, `_MPEG_WaitBDEC`, `_MPEG_put_block_{fr,fl,il}`, `_MPEG_add_block_{frfr,ilfl,frfl}`, `_MPEG_dma_ref_image`, `_MPEG_do_mc`, `_MPEG_{put,avg}_{luma,chroma}{,_X,_Y,_XY}`, `_MPEG_CSCImage`, `_MPEG_Suspend`, `_MPEG_Resume`, `_MPEG_Set16`. All have C callers (nearly all from `src/libmpeg.c`; `_MPEG_GetBits` ×112) except ASM-7. Constants checked: `s_IPUState: .space 32` (`:78`) — `_ipu_suspend` writes exactly offsets 0,4,…,28 (32 B) and `_ipu_resume` reads them back; `s_DMAPack: .space 128` (`:75`) — `_MPEG_dma_ref_image` writes 32 B per entry with the count clamped to 4 (`pminw $a2, $a2, 4` at `:1399`) = 128 B exactly; `s_DefQM` (`:67–71`) is 5×16 B and `_MPEG_SetDefQM` loads exactly the quads at 0/16/32/48/64; `s_CSCParam: .space 12` (`:83`) vs the DMA handler's 0/4/8 accesses via the pointer installed in `_MPEG_Initialize` (`:121`) — consistent. `_MPEG_do_mc`'s field offsets 0…36 and its `+256`/`+768` chroma adjustments are internal to the libmpeg calling convention in `src/libmpeg.c`; no C struct defines them independently, so no mismatch is asserted. `_MPEG_Set16` (`:2300`) is deliberate self-modifying code (patches the `s_16_0…s_16_3` instruction bytes in `_mpeg_dmac_handler`/`_MPEG_CSCImage`, then `FlushCache`/`sync.l`) — by design, called from `src/SMS_MPEG12.c`; noted, not a defect.

**`src/SMS_DMA_0.s` (47)** — Export: `DMA_Stop`. Called from C. No constants to cross-check.

**`src/SMS_IPU_0.s` (108)** — Exports: `IPU_FDEC`, `IPU_IDEC`, `IPU_FRST`. Called from C (`src/SMS_IPU*.c`).

**`src/SMS_RC_0.s` (142)** — Exports: `g_RCData` (data), `RC_ReadDummy`, `RC_ReadX`, `RC_ReadI`, `RC_SetTranslator`, `RC_TransPAD`, `RC_TransRC`. All referenced from C.

**`src/SMS_Timer.s` (277)** — Exports: `g_Timer` (data), `SMS_TimerInit`, `SMS_TimerDestroy`, `SMS_TimerSet`, `SMS_iTimerSet`, `SMS_TimerReset`, `SMS_iTimerReset`, `SMS_TimerWait`, `SMS_SetAlarm`. ASM-4 is the only dead one. Constants checked: `s_Handlers: .space 16 * 4` = 64 B (`:38`); all four setters/resetters index with `sll $a0,$a0,4` (16 B/entry, `:102/:122/:133/:158`) and `_timer_handler` (`:178`) walks exactly the 4 slots at 0/16/32/48 with countdown fields at +8/+24/+40/+56 — all inside the 64 B.

**`src/About_Data.s` (189)** — Data only: a `.byte` table under `g_AboutData`. No code; one-line disposition, no audit performed or needed.

**`iop/SMSMAP/smap_opt.s` (87)** — Exports: `SMAP_CopyFromFIFO`, `SMAP_CopyToFIFO`. Called from the SMAP IOP driver C code.

**`iop/SMSTCPIP/src/inet.s` (667)** — Exports: `ip_cksum`, `inet_chksum`, `inet_chksum_pseudo`, `inet_chksum_pbuf`, `inet_aton`, `inet_addr`, `inet_ntoa`, `htons`, `ntohs`, `htonl`, `ntohl`. ASM-5/ASM-8/ASM-9 above; the rest have C callers in the lwIP tree. Constants checked: `inet_chksum_pseudo` (`:244`) and `inet_chksum_pbuf` (`:342`) read payload at +4, len at +10 (`lhu`), and advance via next at +0 — matching the lwIP `struct pbuf` layout (next 0, payload 4, tot_len 8, len 10).

### Ledger (Scope A)

| File | Lines | Read |
|---|---|---|
| src/SMS_GS_0.S | 1,180 | end-to-end |
| src/SMS_Utils.S | 1,307 | end-to-end |
| src/SMS_DSP_MMI.S | 2,067 | end-to-end |
| src/SMS_DSP_QPel.S | 4,728 | helpers `:1–855` end-to-end; `:855–2344` control flow, offsets and loop structure read in full with parallel-SIMD arithmetic lines elided from view; all 90 wrappers `:2345–4728` signature-clustered by script + representative bodies read end-to-end |
| src/SMS_DSP_FFT.S | 617 | end-to-end |
| src/SMS_MP123Core.S | 1,045 | end-to-end |
| src/libmpeg_core.S | 2,324 | `:1–1000` end-to-end; `:1000–2324` control flow, offsets and loop structure read in full with parallel-SIMD arithmetic lines elided from view |
| src/SMS_DMA_0.s | 47 | end-to-end |
| src/SMS_IPU_0.s | 108 | end-to-end |
| src/SMS_RC_0.s | 142 | end-to-end |
| src/SMS_Timer.s | 277 | end-to-end |
| src/About_Data.s | 189 | end-to-end (data only) |
| iop/SMSMAP/smap_opt.s | 87 | end-to-end |
| iop/SMSTCPIP/src/inet.s | 667 | end-to-end |
| **Total** | **14,785** | |

Total matches the brief's 14,785 lines.

---

## Scope B — Header macros

Extraction: every function-like `#define NAME(args) …` in first-party `include/SMS_*.h` was enumerated by grep (≈130 definitions across 23 headers). Vendored/out-of-scope per the brief, noted and skipped: `include/xz.h`, `xz_config.h`, `xz_lzma2.h`, `xz_private.h`, `xz_stream.h`, `lzma2.h`, `libmpeg.h`, `libmpeg_internal.h` (vendored), `include/s_iop_image.h` (generated data). Boundary (first-party but not `SMS_*.h`, not extracted): `libds34bt.h`, `libds34usb.h`, `mbstring.h`.

**HDR candidates: none.** No function-like macro in the first-party headers meets the candidate bar (overflow in an allocation/index context, a side-effect argument double-evaluated at a real hot/critical call site, or missing parentheses that change meaning at a real call site). The hazards below were each chased to their call sites and are latent only; they are recorded as checked, explicitly **not** as findings.

### Checked-clean by family

- **Packet-size arithmetic** — `GS_TXT_PACKET_SIZE(n)` (`include/SMS_GS.h:1249`): `((n<<2)+6+((g_GSCtx.m_FontTexFmt != GSPixelFormat_PSMT4HL)<<3))`; `n` evaluated once, fully parenthesised; all call sites pass plain lengths (e.g. `src/About.c:108`, `src/SMS_GUIDesktop.c:946` — the latter passes `s_lLen = lLen`, safe precisely because of the single evaluation). Overflow would need a ~2²⁹-character string. `PC_GSP_SIZE(n)` (`include/SMS_PlayerControl.h:24`): `((n<<2)+6)`, same verdict. Constant packet sizes `GS_RRT/BBT/TSP/VGR/LDI_PACKET_SIZE()` (`SMS_GS.h:1250–1254`) are literals.
- **GS register packing** — `GS_SET_*` family (`include/SMS_GS.h:219–352`) and register accessors (`:16–19`): every argument cast to `u64`, parenthesised, evaluated once, whole expression parenthesised. Representative call site `src/SMS_GS_1.c:371` passes field reads and literals only.
- **DMA/GIF/VIF packing** — `DMA_TAG` (`include/SMS_DMA.h:68`), `GIF_TAG` (`include/SMS_GS.h:134`), `VIF_CODE`/`VIF_DIRECT`/`VIF_DIRECTHL` (`include/SMS_VIF.h:29–39`): arguments parenthesised and cast before shifting. (`DMA_SET_CTRL`, `SMS_DMA.h:74`, and `GIF_TAG` lack an outer parenthesis pair, but every call site in the tree is a plain assignment — `src/SMS_EE.c:199`; `src/SMS_GS_1.c:367` et al. — so the hazard is unrealised; latent only.)
- **Bit-reader / VLC (hot path)** — `SMS_OPEN_READER`, `SMS_CLOSE_READER`, `SMS_UPDATE_CACHE`, `SMS_SKIP_COUNTER`, `SMS_SHOW_UBITS/SBITS`, `SMS_SKIP_BITS`, `SMS_GET_CACHE` (`include/SMS_Bitio.h:21–46`) and `SMS_GET_VLC` / `SMS_GET_RL_VLC` (`include/SMS_VLC.h:56`, `:80`): the `name`/`gb` arguments are token-pasted identifiers and plain pointers at every call site; `num`/`bits` arguments are constants or plain variables (representative: `SMS_GetVLC` in `include/SMS_VLC.h:104+` passes `apVLC->m_Bits`); `SMS_SKIP_COUNTER`'s unparenthesised `num` is only ever a literal/variable. No side-effect argument exists at any call site found (grep for `++`/`--`/assignment arguments across `src/` returned none). The statement-block form (no `do{}while(0)`) is used only as standalone statements.
- **SMS.h arithmetic helpers** — `SMS_MAX`/`SMS_MIN` (`include/SMS.h:61–62`), `SMS_RSHIFT` (`:66`), `SMS_ROUNDED_DIV` (`:64`), `SMS_ROUND` (`:57`), `SMS_NEG_USR32/SSR32` (`:68–69`): these do evaluate arguments more than once, but a tree-wide grep found no call site passing a side-effect argument (representative: `src/SMS_MPEG4.c:1643`, `:1655` pass pure expressions). Latent only.
- **IPU command macros** — `IPU_SET_CMD`, `IPU_CMD_IDEC/FDEC/SETTH/CSC` (`include/SMS_IPU.h:36–49`): wrapper arguments are unparenthesised in the shift terms, but all reachable call sites pass literals or plain variables (`src/SMS_IPU_1.c:132`, `:232`), and `IPU_CMD_IDEC`/`IPU_CMD_CSC` have no call sites at all. `IPU_RESET()` (`:56`) is a multi-statement macro without a `do{}while(0)` guard; every call site is a standalone statement (`src/SMS_IPU.c:173`; `src/SMS_IPU_1.c:130`, `:199`, `:230`, `:322`). Latent only.
- **AC3 codec macros** — `SAMPLE`/`LEVEL`/`CONVERT` (`include/SMS_AC3.h:59–61`), `COEFF`/`COMPUTE_MASK`/`UPDATE_LEAK` (`:148–170`), `MUL_C`/`DIV` (`:171–172`): arguments parenthesised or bound to locals first (`COEFF` assigns `(_t)`, `(_l)`, `(e)` to locals before use); block macros operate on fixed locals of the single decoder function. Clean.
- **Misc single-purpose** — `SMS_MKTAG` (`include/SMS_FourCC.h:27`): arguments parenthesised; all call sites pass character literals (e.g. `src/SMS_Codec.c:33–42`). `FILE_EOF(f)` (`include/SMS_FileContext.h:26`) evaluates `f` twice, but every call site passes a plain variable (e.g. `src/SMS_Config.c:344`, `src/SMS_Container.c:317`). `SMS_TLB_VALID` (`SMS_EE.h:14`), `_STR` (`SMS_List.h:16`), `SMS_STRING` (`SMS_Locale.h:22`), `_IOC` (`SMS_ioctl.h:14`), `SMS_IS_*`/`SMS_USES_LIST` (`SMS_MPEG.h:72–78`), `SMS_RingBufferName` (both definitions, `SMS_RingBuffer.h:44–46`), `MC_Sync`/`MC_Init` (`SMS_MC.h:18`, `:42` — the variadic `MC_Init` discarding its arguments is the BDM-branch design), `PAD_*` aliases/no-ops (`SMS_PAD.h:29–33`), `DECLARE_GUI_OBJECT` (`SMS_GUI.h:54`, a declaration block, not an expression), `SMS_SAME_HEADER_MASK` (`SMS_MP123.h:38`, constant). Clean.
- **Not verified, no claim made** — `SMS_DXSB_DP_*` fixed-offset accessors (`include/SMS_DXSB.h:66–77`, offsets 36…264 into `SMS_DXSBDrawPack`): the offsets are plausible against the struct's member sequence, but independently re-deriving each byte offset requires the packed sizes of every `GSReg*`/`GIFTag`/`DMATag` member; no mismatch is asserted. Object-like (non-function-like) size chains in `include/SMS_Data.h:33–56` (IRX image offset chain, `SMS_AUDIO_BUFFER`) are outside the function-like extraction scope and were not evaluated.
