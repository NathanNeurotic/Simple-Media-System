# Verification — SMS Group A (adversarial re-verification)

- **Repo:** Simple-Media-System (read-only clone `~/workspace/audit-clones-2026-11-13/Simple-Media-System`)
- **Pin checked at start:** `git rev-parse HEAD` = `e65ced39a27eee8d996e9257e21594d1cac1c93d` ✔ (matches the assigned pin; clone git-clean at start)
- **Method:** every claim re-derived from source at the pin; finding prose used only as a list of claims to test. Arithmetic claims re-run in a small script. Guard hunt performed for every finding (caller-side validation, sibling clamps, setup-time rejection).

## Summary

| ID | Verdict | One-line evidence |
|---|---|---|
| SMS-015 | **REFUTED** | `_tbl_fits` (`src/SMS_ContainerMOV.c:143-146`) runs before every cited malloc and bounds `count × on-disk width` against the atom's remaining bytes; atom sizes are themselves bounded by the parent chain (`:883`) rooted at the real file size (`:1063`), and every malloc result is NULL-checked before the count is published. |
| SMS-052 | **VERIFIED** | `src/SMS_Container.c:347-356` exactly as cited; script-confirmed `aSize==17, cbSize≥1 → lLen=-1 → malloc(0)`, `Read` takes `unsigned int` (`include/SMS_FileContext.h:79`) so −1 arrives as 4,294,967,295, and `STIO_Read` (`src/SMS_FileContext.c:1337-1376`) streams until EOF into the 0-byte allocation. |
| SMS-053 | **VERIFIED** | AVI `src/SMS_ContainerAVI.c:570-571` / ASF `src/SMS_ContainerASF.c:302-303` store `File_GetUInt` into `int32_t` fields (`include/SMS_Codec.h:70-71`); the only gate (`src/SMS_Player.c:1726-1731`) tests only `> 1024`; the ≤0 guards in `SMS_VideoBufferInit`/`SMS_FrameBufferInit` run under HWCtl *after* codec `Init` has already consumed the values in `SMS_MPEGContext_Init` (`src/SMS_MPEGInit.c:25-31,48-60`). |
| SMS-057 | **VERIFIED** | All four rotate copies (`src/SMS_GUIMenuSMS.c:2190-2200, 2219-2239, 2241-2258, 2348-2362`) wrap at 17 but test `lVal < 0`, so `1 + (−1) = 0` is stored; original sinks, the inline sinks at `:2231/:2253`, and the R6-REV-1 sinks (`src/SMS_Spectrum.c:51,55`, `src/SMS_GUIDesktop.c:756`, `src/SMS_GUInfoPanel.c:141`, `src/SMS_PlayerControl.c:431`) all read `g_Palette[idx − 1]` with `idx` an `unsigned int` field, i.e. index `UINT_MAX` ≡ address `g_Palette − 4`. |
| SMS-060 | **VERIFIED** | `src/SMS_EE.c:952-962` exactly as cited: on `lSize != apData->m_Size` the buffer is freed and set NULL, then `lzma2_uncompress(..., UNCACHED_SEG(retVal), lSize)` runs unconditionally — with `retVal == NULL` that destination is the uncached alias of physical address 0 (in-tree convention: `include/SMS.h:112`, `_U(p) = (uint32_t)(p) \| 0x20000000`); sole caller `SMS_InitVU` (`:967-979`) already handles a NULL return. |

Verdict counts: **VERIFIED 4 · REFUTED 1 · VERIFIED-DRIFT 0 · WEAKENED 0.** New candidate findings filed: **none** (see the closing note).

---

## SMS-015 — MOV container trusts file-supplied entry counts for malloc size

**Verdict: REFUTED** (defeating guard present at the pin, at every cited site).

### Locations checked (claimed vs actual)

| Claimed | Actual at pin | Result |
|---|---|---|
| `src/SMS_ContainerMOV.c:495,546,576,614,651,680` — `malloc(lnEntries * sizeof(...))` for stts/stsc/stsz/stco/ctts/stss | The six mallocs are at exactly those lines: `:495` stts, `:546` stsc, `:576` stsz, `:614` stco/co64, `:651` ctts, `:680` stss; `lnEntries` is read from the file at `:490,542,566,604,647,673` (`_read_int_be`) | Locations exact — but the finding stopped reading one line too early at each site (see Mechanism) |
| `src/SMS_ContainerMOV.c:129,454,952` — comments describing a count that "asks malloc for hundreds of megabytes on a 32 MB heap" and tables that must be NULL-checked | `:128-142` is a comment block that begins with that historical description and then states, in the present tense, "Both halves are closed here: the count is cross-checked against the space the atom actually declares … and each allocation is checked." `:454`-area and `:947-956` comments are the same genre (invariant documentation for `_mov_build_index`) | The comments describe a *closed* bug class, not a live one |

### Mechanism re-derivation

The proposed fix in the finding — "reject a table whose `count × entry size` exceeds the containing atom's remaining bytes" — **already exists, verbatim in effect**, as a shared helper:

```c
/* src/SMS_ContainerMOV.c:143-146 */
static int _tbl_fits ( const MOVAtom* apAtom, unsigned int anEntries, unsigned int aWidth, unsigned int aConsumed ) {
 int64_t lAvail = apAtom -> m_Size - aConsumed;
 return lAvail >= 0 && ( int64_t )anEntries * aWidth <= lAvail;
}
```

Every cited allocation is preceded by it, with the correct on-disk width and consumed count:

- stts `:494` — `!_tbl_fits(apAtom, lnEntries, 8, 8)` → `return 0` before the `:495` malloc;
- stsc `:545` — width 12, consumed 8, before `:546`;
- stsz `:572-575` — width 4, consumed 12, before `:576` (variable-size path only; the fixed-size path at `:569-570` returns before any allocation);
- stco/co64 `:609-613` — width 8 for `co64`, 4 for `stco`, consumed 8, before `:614`;
- ctts `:650` — width 8, consumed 8, before `:651`;
- stss `:679` — width 4, consumed 8, before `:680`.

Two further layers the finding did not credit:

1. **Multiply-overflow guard** immediately before each `_tbl_fits` call: `if (lnEntries >= UINT_MAX / sizeof(<table type>)) return -1;` (`:491,543,567,605,648,674`).
2. **NULL check + publish-after-fill:** each malloc result is tested (`if (!lpMyStm->m_pStts) return 0;` etc. at `:496,547,577-580,615,652,681`) and the count field is only assigned after the allocation succeeds — the exact invariant the `:947-956` comment warns maintainers to preserve.

### Reachability / guard hunt (the decisive part)

A sceptic's next move is: "_tbl_fits trusts `apAtom->m_Size`, which is also file-supplied — declare a giant atom in a tiny file and the bound is meaningless." That escape is closed by the dispatcher:

- `_mov_read_header` roots the parse at the real file: `MOVAtom lAtom = { 0, 0, lpFileCtx->m_Size };` (`src/SMS_ContainerMOV.c:1063`).
- `_mov_read_dflt` rejects any child atom that overruns its parent: `if (lAtom.m_Size < 0 || lAtom.m_Size > apAtom->m_Size - lTotalSize) break;` (`:883`), applied recursively down moov → trak → stbl → the six table atoms.

So a table atom's declared size cannot exceed the bytes its ancestors declare, which cannot exceed the actual file size. Worst case at the pin, per table, in-memory allocation vs on-disk width: stts 8/8, stsc 12/12, stsz 4/4, ctts 8/8, stss 4/4 — ratio 1×; stco is the worst at 8 in-memory / 4 on-disk = 2×. A crafted file can therefore ask for at most roughly **its own size (×2 for stco)** in table memory — not "hundreds of megabytes" from a small file, and any allocation failure is handled by the NULL checks above. The claimed primitive (huge allocation attempt before any validation against actual file size) does not exist at this pin.

**Why this is REFUTED rather than WEAKENED:** the finding's Problem sentence is categorical ("A crafted MP4 can still drive a huge allocation attempt before any sample is validated against the actual file size") and its Fix is precisely the `_tbl_fits` check that is already implemented at all six sites. Nothing narrower survives: the residual behaviour (allocation proportional to file size, failure-checked) is ordinary parser behaviour, not the filed defect. (Historical note, not part of the verdict: the round-1 pin and the round-2 pin differ only by docs/CSS commits per `PINS-ROUND2.md`, so the guard was already present when the finding was written — the original read appears to have quoted the malloc lines and the historical comments without reading the `_tbl_fits` calls on the immediately preceding lines.)

---

## SMS-052 — WAV-header helper: format size of exactly 17 turns its own clamp negative

**Verdict: VERIFIED.**

### Locations checked (claimed vs actual)

| Claimed | Actual at pin | Result |
|---|---|---|
| `src/SMS_Container.c:347-356` — gate `aSize > 16` at `:347`, clamp at `:350`, `malloc((lLen+15)&~15)` at `:351`, `Read` at `:352` | Identical: `:347 if (aSize > 16) {`, `:348 int lLen = File_GetUShort(...)`, `:350 if (lLen > aSize - 18) lLen = aSize - 18;`, `:351 malloc((lLen + 15) & ~15)`, `:352 apFileCtx->Read(apFileCtx, apCodecCtx->m_pUserData, lLen)`, trailing seek at `:355` | Exact, no drift |
| `include/SMS_FileContext.h:79` — `Read` function pointer takes `unsigned int` | `:79 int (*Read)(struct FileContext*, void*, unsigned int);` | Exact |
| `src/SMS_ContainerASF.c:265` — caller with file-supplied `type_specific_size` | `:265 SMSContainer_GetWAVHeader(lpFileCtx, lpStm->m_pCodec, type_specific_size);` with `type_specific_size = File_GetUInt(lpFileCtx)` at `:241`, declared `int` at `:197` | Exact |
| `src/SMS_ContainerAVI.c:599` — caller with the `strf` chunk size | `:599 SMSContainer_GetWAVHeader(lpFileCtx, lpCodecCtx, lSize);` with `lSize` a `uint32_t` RIFF chunk size read at `:393` | Exact |

### Mechanism re-derivation (script-checked)

`aSize` is the function's `int` parameter. The branch runs for `aSize > 16`; `lLen` is the file's cbSize (`File_GetUShort`, 0–65535), and the clamp only fires inside `if (lLen > 0)`. For `aSize == 17`: `aSize - 18 == -1`, and any `lLen ≥ 1` satisfies `lLen > -1`, so the clamp *assigns* `lLen = -1`. Recomputed in a script for cbSize ∈ {1, 2, 100, 65535}: all yield `lLen = -1`, allocation size `(-1 + 15) & ~15 = 14 & ~15 = 0` → `malloc(0)`, and the `Read` length argument converts to `unsigned int` 4,294,967,295. For every other admitted size the clamp is sound (`aSize == 16` never enters the branch; `aSize ≥ 18` gives `aSize - 18 ≥ 0`), confirming the finding's "exactly one value" framing. The trailing seek at `:355` (`aSize - lLen - 18 = 17 - (-1) - 18 = 0`) does not rescue anything — it runs after the read.

One secondary point the finding did not need: `malloc(0)`'s result is also never NULL-checked before the read (the SMS-045 family), but with `lLen = -1` that is moot — the length alone is the defect.

### Reachability / guard hunt

- **ASF:** `type_specific_size` is a raw `File_GetUInt` from the stream-properties object (`:241`); no caller-side check rejects 17 before `:265`. ✔ reachable with a 17-byte audio type-specific block and cbSize ≥ 1.
- **AVI:** `lSize` is the raw `strf` chunk size (`:393`); the audio branch at `:597-599` passes it unchecked. A 17-byte `strf` chunk is structurally admissible to the dispatcher (it seeks by `lSize` elsewhere, never validates a minimum for audio). ✔ reachable.
- **Does `Read` actually deliver file bytes up to the absurd count?** This was the finding's one INFERRED link; re-derived here: the STIO implementation `STIO_Read` (`src/SMS_FileContext.c:1337-1376`) loops `while (aSize > 0)`, copying each buffered span and refilling via `Fill`, and only breaks when a refill yields 0 at EOF (`:1353-1357`). It therefore streams real file bytes into the destination until EOF — into a 0-byte heap allocation, i.e. a heap overwrite whose length is bounded by the bytes remaining in the file after the header, exactly as the finding stated ("until EOF or exhaustion"). No sibling clamp, no caller validation, and no length sanity check inside `STIO_Read` defeats this.

No defeating guard found. The proposed fix (gate `aSize >= 18`) is consistent with the format: the fixed WAVEFORMAT part is 16 bytes and cbSize itself occupies 2 more, so no legitimate 17-byte block exists.

---

## SMS-053 — The 1024-pixel gate is one-sided: negative AVI/ASF dimensions reach MPEG-4 geometry initialisation

**Verdict: VERIFIED.**

### Locations checked (claimed vs actual)

| Claimed | Actual at pin | Result |
|---|---|---|
| `src/SMS_ContainerAVI.c:570-571` | `:570 lpCodecCtx->m_Width = File_GetUInt(lpFileCtx);` `:571 ...m_Height = File_GetUInt(...)` (BITMAPINFOHEADER `biWidth`/`biHeight` in the `strf` video branch) | Exact |
| `src/SMS_ContainerASF.c:302-303` | `:302 lpStm->m_pCodec->m_Width = File_GetUInt(lpFileCtx);` `:303 ...m_Height = ...` (video stream-properties) | Exact |
| `include/SMS_Codec.h:70-71` — fields are `int32_t` | `:70 int32_t m_Width;` `:71 int32_t m_Height;` | Exact |
| `src/SMS_Player.c:1726-1731` — the gate | `:1726-1728 if (lpVideoCtx->m_Width > 1024 \|\| lpVideoCtx->m_Height > 1024)`, refusal `STR_RESOLUTION_TOO_BIG` + `goto destroy` at `:1729-1730` | Exact |
| `src/SMS_MPEGInit.c:25-31 and :49-60` | `:25-35` geometry (`m_MBW = (aWidth + 15) / 16` at `:28`, strides `:30-32`, `m_MBNum = m_MBW * m_MBH` at `:35`); `:48` the `m_pMBIdx2XY` calloc sized `(m_MBNum + 1) * sizeof(int)`, `:56` its unconditional sentinel write, `:58-60` the remaining unchecked callocs | Exact (finding cited the calloc family as `:49-60`; the first calloc is at `:48` — a one-line under-citation inside the cited block's immediate context, not drift in any load-bearing line) |

### Mechanism re-derivation

`File_GetUInt` returns `unsigned int` (`include/SMS_FileContext.h`, File_GetUInt inline). Assigning it to an `int32_t` field converts values ≥ 2³¹ to negatives (script-checked: raw `0xFFFFFFFF → -1`, `0x80000000 → -2147483648`, `0xFFFFFC18 → -1000` — the last being the legitimate top-down-AVI encoding of height 1000, confirming the finding's "not crafted-only" point). A negative value trivially passes the player's only test, `> 1024`.

Downstream, with no intervening sanitisation (`SMS_CodecOpen`, `src/SMS_Codec.c:92-123`, is a bare dispatch on `m_ID`, as claimed): `SMS_Player.c:1734` calls the codec's `Init`, and both software paths call `SMS_MPEGContext_Init(apCtx->m_Width, apCtx->m_Height)` directly — `src/SMS_MPEG4.c:684` and `src/SMS_MSMPEG4.c:1575`. In `SMS_MPEGContext_Init` the signed arithmetic then misbehaves by sign combination, exactly as the finding hedged:

- e.g. width raw `0xFFFFFC18` (−1000), height 480: `m_MBW = (-1000 + 15) / 16 = -61` (C truncation toward zero, script-checked), `m_MBH = 31`, `m_MBNum = -1891`; the `:48` calloc size `(m_MBNum + 1) * 4` converts to a huge `size_t` and fails → NULL table; the fill loops at `:50-54` don't execute (negative bound), but the sentinel write at `:56`, `m_pMBIdx2XY[m_MBNum] = ...`, dereferences NULL at a negative index unconditionally.
- Both-negative inputs give a positive `m_MBNum` paired with negative strides (`m_MBStride`, `m_B8Stride`, `g_pSPRTop` advance at `:66`), i.e. the "walks frame memory backwards" family.

The exact failing instruction is sign-combination-dependent — the finding said so (INFERRED), and the re-derivation agrees; at least one concrete combination (above) reaches an unconditional NULL/negative-index write inside `Init` itself.

### Reachability / guard hunt (two candidate guards examined and rejected)

1. **`SMS_VideoBufferInit` rejects non-positive dimensions** (`src/SMS_VideoBuffer.c:32-34`: `INT_MIN` rejected, negative height negated, `aWidth <= 0 || aHeight <= 0 || > 65504` → NULL), and **`SMS_FrameBufferInit` likewise** (`src/SMS_VideoBuffer.c:291`: `aWidth <= 0 || aHeight <= 0 ... return 0`). These look like defeating guards — but both run only under `MPEG4_CommonHWCtl(SMS_HWC_Init)` (`src/SMS_MPEG4.c:563,566`), which is invoked from `_prepare_ipu_context` (`src/SMS_Player.c:179`) inside `_sms_play` (`src/SMS_Player.c:1110`). The call order is: `SMS_InitPlayer` (invoked from `src/SMS_GUICmdProc.c:97,186,279` when a file is selected) runs `SMS_CodecOpen` → the `:1726` gate → codec `Init` at `:1734` — **before** `s_Player.Play` (`_sms_play`) is ever called. The HWCtl guards therefore execute strictly after `SMS_MPEGContext_Init` has already consumed the negative dimensions. They do not defeat the claim; they are, if anything, evidence the ≤0 case was considered in the buffer layer and missed in the geometry layer.
2. **MOV immunity** as claimed: `src/SMS_ContainerMOV.c:345-346` reads width/height with `File_GetShortBE` (signed 16-bit, max 32767 positive / negative only down to −32768 — and a negative MOV dimension would hit the same gate hole in principle, but the finding scoped itself to AVI/ASF and correctly identified MOV's 16-bit reads as not producing the ≥2³¹ wrap; no revision needed to the finding's scope statement). The IPU/MPEG-1/2 path (`SMS_MPEG12.c`) takes dimensions from the bitstream sequence header, not the container fields, as claimed.

No `abs()`, no range check, and no other gate exists between the container reads and `SMS_MPEGContext_Init` — grep-verified across `SMS_Codec.c`, `SMS_Player.c`, `SMS_MPEG4.c`, `SMS_MSMPEG4.c`. No defeating guard found.

---

## SMS-057 — Palette rotate wraps at 17 but not at 0: pressing Left at colour 1 stores 0, and the next render reads `g_Palette[-1]`

**Verdict: VERIFIED** (including the R6-REV-1 completed sink list).

### Locations checked (claimed vs actual)

| Claimed | Actual at pin | Result |
|---|---|---|
| `src/SMS_GUIMenuSMS.c:2190-2200` `_rotate_palette` | `:2190-2205`, body exactly `lVal += aDir; if (lVal == 17) lVal = 1; else if (lVal < 0) lVal = 16; *apVal = lVal;` | Exact |
| `src/SMS_GUIMenuSMS.c:2219-2239` `_txtclr_handler`, OOB read at `:2231` | `:2219-2239`; `:2231 lColor = g_Palette[g_Config.m_BrowserTxtIdx - 1];` with the same inline rotate at `:2224-2228` | Exact |
| `src/SMS_GUIMenuSMS.c:2241-2258` `_sltxt_handler`, OOB read at `:2253` | `:2241-2258`; `:2253 GSContext_SetTextColor(0, g_Palette[g_Config.m_BrowserSBCIdx - 1]);`, inline rotate at `:2245-2249` | Exact |
| `src/SMS_GUIMenuSMS.c:2348-2362` `_rotate_palette_2` | `:2348-2362`, identical wrap logic, `int*` parameter (callers cast the `unsigned int` config fields, `:2366-2396`) | Exact |
| Sinks `src/SMS_GUIFileMenu.c:432,459,524,550,591,647` | All six present: `:432,524,591` are `g_Palette[*m_pColor - 1]` (`m_pColor` is `unsigned int*`, `src/SMS_GUIFileMenu.c:65`, bound to `&g_Config.m_BrowserIBCIdx` at `:1654` / ABC/IBC at `:760`); `:459,550,647` are `g_Palette[g_Config.m_BrowserSCIdx - 1]` (a `_rotate_palette` field, via `_sclr_handler` at `src/SMS_GUIMenuSMS.c:2260-2264`) | Exact |
| Handlers called with +1/−1 at `src/SMS_GUIMenu.c:347,351` | `:347 Handler(lpMenu, 1)` (Cross/Enter/Play), `:351 Handler(lpMenu, -1)` (Circle) | Exact |
| R6-REV-1 added sinks: `src/SMS_Spectrum.c:51,55` | `:51 lpDMA[1] = g_Palette[g_Config.m_BrowserIBCIdx - 1];` `:55 lpDMA[5] = g_Palette[g_Config.m_BrowserIBCIdx - 1];` inside `SMS_SpectrumInit` (`:25-69`), baked into the persistent spectrum DMA packet | Exact |
| R6-REV-1 added sinks: `src/SMS_GUIDesktop.c:756`, `src/SMS_GUInfoPanel.c:141`, `src/SMS_PlayerControl.c:431` | All three present verbatim, each `g_Palette[g_Config.m_BrowserIBCIdx - 1]` (`GUIDesktop.c:756` in a `GS_RenderRoundRect` call; `GUInfoPanel.c:141` likewise; `PlayerControl.c:431` likewise) | Exact |

### Mechanism re-derivation (script-checked)

All palette index fields are `unsigned int` (`include/SMS_Config.h:101-122`: `m_BrowserABCIdx`, `m_BrowserIBCIdx`, `m_BrowserTxtIdx`, `m_BrowserSCIdx`, `m_BrowserSBCIdx`, and the six player-colour fields), stored values are meant to be 1..16, and `g_Palette` is `unsigned int g_Palette[16]` (`src/SMS_Config.c:51`, declared `include/SMS_Config.h:147`). Defaults are in-range (`src/SMS_Config.c:601-615`: IBCIdx 10, TxtIdx 16, SBCIdx 16, SCIdx 12).

Wrap arithmetic, recomputed for all boundary cases: current 16, dir +1 → 17 → wrapped to 1 ✔; current 0, dir −1 → −1 → wrapped to 16 ✔ (unreachable in practice); **current 1, dir −1 → 0 → `0 == 17` false, `0 < 0` false → stored unwrapped** ✘. That is the single hole, exactly as filed: the lower test should be `lVal < 1`.

Sink arithmetic: with the stored field 0 (unsigned), `idx - 1` evaluates in unsigned arithmetic to `UINT_MAX` (4,294,967,295). Because the element type is 4-byte `unsigned int`, the byte offset `UINT_MAX × 4` wraps modulo 2³² to −4, so the effective address is literally `g_Palette - 4` — the finding's "`g_Palette[-1]`" and its "wraps to `UINT_MAX`, a wild read" are the same address stated two ways; on the 32-bit EE both descriptions are accurate, and the read lands 4 bytes before the palette in `.bss`. (For the `_txtclr_handler`/`_sltxt_handler` inline copies the read happens in the same call that stored 0, at `:2231`/`:2253` — no redraw needs to intervene.)

### Reachability / guard hunt

Ordinary menu use only, as claimed: open the SMS settings menu, move to any colour row (active border, inactive border, text, selected text, selection, or any `_rotate_palette_2` subtitle/player colour), press Circle (−1) while the colour is 1. The stored 0 then poisons every sink above — including `SMS_SpectrumInit`, which bakes the bad read into a persistent DMA packet re-sent every frame until re-init (R6-REV-1's point, confirmed at `src/SMS_Spectrum.c:39-62`: the packet is built once in a `while (lpDMA < lpDMAEnd)` loop over `g_SPCPkt`).

Guard hunt: no clamp exists in any of the four rotate copies beyond the buggy test; no load-time range check on these fields was found in `SMS_Config.c` (the raw-config exposure is SMS-058's separate finding, and defaults are valid, so SMS-057 needs no corrupt config); no sink masks or bounds its index (grep over all `g_Palette[` sites in `src/` — every consumer uses the raw `idx - 1` form). No defeating guard found. The filed fix (`lVal < 0` → `lVal < 1` in the rotate logic) covers the original and R6-REV-1 sinks alike, since all of them consume fields written only by these rotate copies (or by valid defaults).

---

## SMS-060 — `SMS_LZMA2Decompress`'s size guard frees the buffer, then decompresses into it anyway

**Verdict: VERIFIED.**

### Locations checked (claimed vs actual)

| Claimed | Actual at pin | Result |
|---|---|---|
| `src/SMS_EE.c:946-965`, guard at `:952-955`, unconditional `lzma2_uncompress` at `:957-962` | `:946 void* SMS_LZMA2Decompress(SMS_LZMA2Data* apData) {`; `:948 retVal = SMS_SyncMalloc(apData->m_Size);`; `:950 lSize = lzma2_get_uncompressed_size(...)`; `:952-955 if (lSize != apData->m_Size) { free(retVal); retVal = NULL; }`; `:957-961 lzma2_uncompress(apData->m_pData, apData->m_PackSize, UNCACHED_SEG(retVal), lSize);`; `:963 return retVal;` | Exact, no drift |
| Sole caller chain `SMS_InitVU` at `:967-979` | `:967-979 SMS_InitVU`: `:969 lpData = SMS_LZMA2Decompress(apData);` `:971 if (lpData) { DMA_SendChainT(...); ... free(lpData); }` `:977 return (int)lpData;` — NULL is a handled, clean failure. Tree-wide grep confirms `SMS_LZMA2Decompress` has no other caller, and `SMS_InitVU`'s callers are `src/SMS_JPEG.c:320,351` with the embedded `g_JPEGData[5]` table (`src/SMS_JPEGData.c:316`) | Exact |

### Mechanism re-derivation (task item (d): the guard's failure path, end to end)

1. `retVal` is allocated at `apData->m_Size` bytes via `SMS_SyncMalloc` (`src/SMS_EE.c:730-738`: `memalign(64, (aSize + 63) & ~63)`).
2. `lSize` is re-derived from the compressed blob itself by `lzma2_get_uncompressed_size`. The guard compares the two: on **any** mismatch it frees the buffer and sets `retVal = NULL` — and then falls through. There is no `return`, no `else`, no re-check.
3. The value actually passed onward as the decompression destination is `UNCACHED_SEG(retVal)` with `retVal == NULL`. `UNCACHED_SEG` is the PS2SDK macro that ORs the address with `0x20000000` to reach the uncached alias of the same physical memory — the in-tree statement of the identical convention is `include/SMS.h:112`: `#define _U(p) ((uint64_t*)(((uint32_t)(p)) | 0x20000000))`. Hence the destination actually written is `0x20000000`, i.e. **physical address 0 through its uncached mirror**, and `lzma2_uncompress` writes `lSize` bytes of decompressed VU microcode there — low memory, not a NULL-page fault the MMU would catch (the EE has no such protection on this segment). The finding's phrasing ("writes the decompressed output to the uncached alias of address 0") is precise.
4. The function then `return`s NULL, so `SMS_InitVU` takes its clean-failure branch — after the corruption has already happened. The guard converts nothing; it only changes *which* bad state follows (freed/NULL destination instead of a merely wrong-sized one).
5. Secondary claim also confirmed: if `SMS_SyncMalloc` itself returns NULL (heap exhaustion) and `lSize == apData->m_Size` (guard doesn't fire), the same unconditional call writes `lSize` bytes to `UNCACHED_SEG(NULL)` = physical 0. (Additionally, `SMS_SyncMalloc` passes its possibly-NULL result straight to `SyncDCache` at `:734` — part of the same unchecked-allocation family, already inside this finding's stated scope; not filed separately.)

### Reachability / guard hunt

The data is embedded, as the finding states: `SMS_LZMA2Data` (`include/SMS.h:271-275`) entries in `g_JPEGData` (`src/SMS_JPEGData.c:316`) describe xz/LZMA2-compressed VU microcode blobs baked into the ELF, consumed on the JPEG viewer's VU setup (`src/SMS_JPEG.c:320,351`). There is no file/network attacker path, and the finding does not claim one — the trigger is a build/data inconsistency (an `m_Size` in the table that disagrees with the blob's own header, e.g. after a blob is re-compressed without regenerating the table) or allocation failure. Guard hunt: no caller pre-validates `m_Size` against the blob; `SMS_InitVU` cannot intervene because the write happens inside the callee before the NULL return reaches it; no other size check exists in `SMS_LZMA2Decompress`. No defeating guard found. The filed fix (return NULL after the free — one line, caller already handles it) is exactly sufficient.

---

## New candidate findings

**None filed.** The re-reads performed for this group (the MOV table readers and dispatcher, `SMSContainer_GetWAVHeader` and both callers, the AVI/ASF dimension chain through `SMS_Player`/`SMS_MPEGInit`/`SMS_VideoBuffer`, the palette rotate copies and every `g_Palette[` consumer, and `SMS_LZMA2Decompress`/`SMS_SyncMalloc`/`SMS_InitVU`) surfaced no defect that is not already owned by an existing SMS finding: the unchecked-allocation behaviour inside `SMS_SyncMalloc` is within SMS-060's stated scope, the raw-config palette exposure is SMS-058's, and the MOV stsz fixed-size path's early count publication is neutralised by the header-level clamp at `src/SMS_ContainerMOV.c:1141-1142`. The SMS-183…189 range is left unused rather than filled speculatively.

## Pin status

- Start: `e65ced39a27eee8d996e9257e21594d1cac1c93d` ✔, working tree clean.
- End: re-checked after all reads (see the report accompanying this file); no edits, commits, or other writes were made in the clone — the only deliverable written is this section file, plus disposable scratch under `/tmp`.
