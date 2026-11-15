# REDO SMS W7 — Data, Tables & Binary Assets (BLIND)

- **Repo / pin:** Simple-Media-System @ `e65ced39a27eee8d996e9257e21594d1cac1c93d` (verified `git rev-parse HEAD` in the clone). Clone read-only; nothing edited.
- **Scope:** `src/SMS_Data.c`, `src/SMS_JPEGData.c`, `src/SMS_Sounds.c`, `src/SMS_IconsRGBA.c`, `src/SMS_LoadingRGBA.c`, `src/SMS_BallRGBA.c`, `src/SMS_ConfigIcon.c`, `src/About.c`, `src/SMS_InverseCodePages.c`, `src/SMS_GSFont.c`, directories `fonts/ images/ lng/ theme/ ship/ irx/`, and `src/lzma2.c` + vendored `src/xz_dec_lzma2.c`, `src/xz_dec_stream.c`, `src/xz_crc32.c`.
- **Method:** every count below is a script output (python3 regex/brace-matching extraction of initialisers, `md5sum`, `du`, python `lzma` decompression, PIL re-bakes, exact ports of C algorithms simulated in python). No count was eyeballed. Replacement values are never invented: table claims rest only on counts, monotonicity, simulation, or hash equality.
- **Licences (settled):** scoped SMS files carry the PS2DEV Academic Free License 2.0 header; `xz_dec_*.c` / `xz_crc32.c` are Lasse Collin / Igor Pavlov public-domain XZ Embedded (file headers read); `src/lzma2.c` is the locally written wrapper (no upstream licence header). No licence finding.

---

## (a) Coverage note — script census

### Tables verified (declared vs actual)

Extraction method: python brace-matching from each `= {`, counting `0x…` tokens in the body.

| Table | Declared | Actual (script) | Verdict |
|---|---|---|---|
| `g_PCMClamps` (SMS_Data.c:15) | 8 floats | 8 (`32767.0F`×4, `−32768.0F`×4) | OK |
| `g_DataBuffer` (SMS_Data.c:20) | `SMS_DATA_BUFFER_SIZE` = 266,560 (computed from SMS_Data.h chain) | 266,544 initialiser bytes; final 16 bytes implicit-zero per C semantics | OK — see per-segment table below |
| `g_SMSound[4]` (SMS_Sounds.c:13) | 4 entries | 4 entries (8 hex words) | OK — partition verified below |
| `g_SMSounds` (SMS_Sounds.c:20) | 70,816 | 70,816 | OK |
| `s_IDCT` (SMS_JPEGData.c:13) | — (unsized) | 928 compressed | XZ-decompresses to 2,016 = `g_JPEGData[0].m_Size` OK |
| `s_CSC1` (SMS_JPEGData.c:74) | — | 668 compressed | decompresses to 1,232 = declared OK |
| `s_CSC2h` (SMS_JPEGData.c:119) | — | 1,100 compressed | decompresses to 2,128 = declared OK |
| `s_CSC2v` (SMS_JPEGData.c:191) | — | 900 compressed | decompresses to 1,632 = declared OK |
| `s_CSC4` (SMS_JPEGData.c:251) | — | 980 compressed | decompresses to 1,872 = declared OK |
| `g_JPEGData[5]` (SMS_JPEGData.c:316) | 5 | 5 `{raw, sizeof(), ptr}` rows, all `sizeof()` match the counts above | OK |
| `g_RGBA_Blob` (SMS_IconsRGBA.c:8) | 27,764 | 27,764 | OK — decompresses to 205,824 = `g_RGBA_RawSize` (SMS_IconsRGBA.c:1748); re-bake from `theme/icons/*.png` via `tools/bake_icons_rgba.py` is **byte-identical** |
| `g_IconLoadingRGBA` (SMS_LoadingRGBA.c:5) | 4,096 | 4,096 (= 32×32×4) | OK — re-bake via `tools/bake_loading_rgba.py` byte-identical |
| `g_IconBallRGBA` (SMS_BallRGBA.c:5) | 4,096 | 4,096 (= 32×32×4) | OK — direct PIL conversion of `theme/icons/g_IconBall.png` (same algorithm as the loading bake) byte-identical |
| `g_IconSMS` (SMS_ConfigIcon.c:11) | 2,020 | 2,020 | OK — written whole via `sizeof()` at SMS_Config.c:867,1047 |
| `g_ASCII` (SMS_GSFont.c:11) | 5,416 | 5,416 | count OK — **but the extern declaration disagrees** (CAND-W7-6) |
| `g_1250` (SMS_GSFont.c:353) | 7,736 | 7,736 | OK |
| `g_1251` (SMS_GSFont.c:840) | 7,856 | 7,856 | OK |
| `g_1252` (SMS_GSFont.c:1334) | 7,732 | 7,732 | OK |
| `g_1253` (SMS_GSFont.c:1821) | 7,588 | 7,588 | OK |
| `_invCodeTables[4]` (SMS_InverseCodePages.c:20) | 4 × (128 uni + 128 idx) | 4 × (128 + 128); every `m_uniCharacter` strictly monotonic; every `m_uniIndex` is a permutation of 0..127 | structure OK — **search algorithm has one unreachable entry** (CAND-W7-7) |
| `g_AboutData` (src/About_Data.s, consumed by About.c) | `NSTR` = 60 records (About.c:20) | exact `UnHash` simulation consumed exactly 60 records, 1,295 of 1,322 bytes, 27 trailing zero bytes; all 60 strings decode to readable credits text, max length 82 | OK |

`SMS_GSFont.c` contains **no rendering code** — grep for function definitions returns only the five array declarations (lines 11/353/840/1334/1821); rendering lives in `src/SMS_GS_2.c` (`GSFont_RenderEx`, `GSFont_Get/Set/Load`). All five font blobs start with the `FON\x01` magic.

### `g_DataBuffer` per-segment verification (SMS_Data.c vs include/SMS_Data.h)

Method: split the file on its `/* NAME.IRX */` marker comments, count hex tokens per segment, compare payload to the header `*_SIZE` and payload+padding to the 16-byte-aligned size.

| Segment | Declared size | Actual payload | Padding | Format (first bytes) |
|---|---|---|---|---|
| PS2ATAD.IRX | 11,821 | 11,821 | 3 | ELF |
| PS2HDD.IRX | 24,953 | 24,953 | 7 | ELF |
| SMSPFS.IRX | 50,405 | 50,405 | 11 | ELF |
| UMS.IRX | 8,504 | 8,504 | 8 | **XZ** → decompresses to 19,749 ELF |
| USBD.IRX | 9,140 | 9,140 | 12 | **XZ** → decompresses to 25,621 ELF |
| SMSTCPIP.IRX | 67,061 | 67,061 | 11 | ELF |
| SMSMAP.IRX | 7,909 | 7,909 | 11 | ELF |
| PS2HOST.IRX | 16,349 | 16,349 | 3 | ELF |
| VU0 IDCT microprogram | 1,376 | 1,376 | 0 | VU code |
| VU0 IDCT coefficients | 256 | 256 | 0 | float data |
| SMSSMB.IRX | 19,641 | 19,641 | 7 | ELF |
| SMSUTILS.IRX | 3,733 | 3,733 | 11 | ELF |
| SMSAUDRV.IRX | 8,989 | 8,989 | 3 | ELF |
| PS2DEV9.IRX | 8,149 | 8,149 | 11 | ELF |
| POWEROFF.IRX | 2,229 | 2,229 | 11 | ELF |
| SMSCDVD.IRX | 20,093 | 20,093 | 3 | ELF |
| SIO2MAN.IRX | 5,817 | 5,817 | 7 | ELF |

All 17 segments: payload == declared, payload+padding == aligned size. **Verdict: exact.** The embedded XZ USBD/UMS modules are *not* duplicates of `irx/usbd.irx` / `irx/usbmass_bd.irx` (decompressed 25,621 / 19,749 bytes, md5s differ from the `irx/` files' 26,241 / 10,177 bytes — different module builds for the non-BDM path).

### `g_SMSound` partition (SMS_Sounds.c:13, struct `{m_Sound, m_Size}` in include/SMS_SPU.h:14-19; `m_Sound` is the SPU-RAM offset, `SPU_LoadData(g_SMSounds, sizeof)` at SMS_IOP.c:1732 loads the array linearly)

Sorted by offset: [0] 0x0000+0x60BF ends 24,767 · [1] 0x60D0+0x04BF ends 25,999 · [3] 0x65A0+0x574F ends 48,367 · [2] 0xBD00+0x578F ends 70,799 of 70,816. No overlaps; uniform 17-byte gaps between segments and at the tail. **Verdict: exact partition** (table order is semantic — Error/PAD/Mount/UMount — not address order, which is fine).

### Directory census (`find` + `du -sb` + `md5sum`)

| Dir | Files | Bytes | Duplicate hashes |
|---|---|---|---|
| fonts/ | 19 (17 `.mtf`, 2 `.txt`) | 151,811 | 1 pair (CAND-W7-11) |
| images/ | 5 (3 `.jpg`, 1 `.png`, 1 `.bak-512`) | 367,447 | 0 |
| lng/ | 20 (19 `SMS.lng_*`, 1 README) | 99,330 | 0 |
| theme/ | 39 (35 `theme/icons/*.png`, 3 `sms-bdm_assets_3/*.png`, `splash_composited.png`) | 303,765 | 4 pairs — see checked-clean |
| ship/ | 5 (`icon.sys`, 3 `.icn`, KIT-README) | 58,454 | 0 |
| irx/ | 43 (19 `.irx`, 24 `.irx.xz`) | 494,378 | 0 |

- `lng/`: all 19 language files are exactly **269 lines / 268 non-empty** — positional format perfectly aligned across languages. Lines byte-identical to EN range 25 (CA) to 56 (SE) of 268; without per-line linguistic evidence this is **not** claimed as untranslated (proper nouns/format strings legitimately match).
- `irx/`: every plain `.irx` also has a `.irx.xz`; five `.xz` have no plain sibling (`bdmfs_vfat`, `ds34bt`, `ds34usb`, `libsd`, `smbman`). The build embeds only the `.xz` forms (`Makefile:94-100`, `bin2c` → `SifExecDecompModuleBuffer`); each `.irx.xz` script-decompresses to exactly its `.irx` sibling where one exists (e.g. `usbd.irx.xz` → 26,241 bytes, md5 equal to `usbd.irx`).
- RGBA invariant script check: in `g_IconLoadingRGBA`, `g_IconBallRGBA` and the decompressed icon blob, every alpha byte (offset 3 mod 4) is ≤ 0x80 (max seen 0x80) and no pixel has non-zero RGB with alpha 0. **Verdict: format invariant holds.**

### Vendored XZ (mechanism scan only, per scope)

`xz_dec_lzma2.c` (1,175 lines), `xz_dec_stream.c` (847), `xz_crc32.c` (59) are unmodified-style XZ Embedded: `include/xz_config.h` leaves `XZ_USE_CRC64` and all `XZ_DEC_*` BCJ decoders commented out, which matches what the in-tree producers emit (`tools/bake_icons_rgba.py`: `CHECK_CRC32`, LZMA2-only filter chain; `tools/compress_irx.py` outputs are accepted by the same config — all in-tree `.xz` blobs script-decompress cleanly under CRC32/LZMA2-only rules). No SMS-specific patches, `FIXME`/`HACK` markers, or local edits were found in the three vendored files. The locally written wrapper `src/lzma2.c` is where every defect below lives.

---

## (b) Candidates (sorted by benefit)

### CAND-W7-1 — `SifExecDecompModuleBuffer` executes the output buffer on *any* XZ decode error
Category: 6 bugs/security
Locations: `src/SMS_IOP.c:237-243` (the wrapper's principal caller); `include/xz.h:112-122` (`enum xz_ret`); `src/lzma2.c:124-155` (`lzma2_uncompress`)
Problem: `ret = lzma2_uncompress(...); if (ret > 0) ret = SifExecModuleBuffer(irx_data, irx_size, ...)`. In this vendored enum, `XZ_STREAM_END` is 1 — but so is every error greater than it: `XZ_UNSUPPORTED_CHECK`=2, `XZ_MEM_ERROR`=3, `XZ_MEMLIMIT_ERROR`=4, `XZ_FORMAT_ERROR`=5, `XZ_OPTIONS_ERROR`=6, `XZ_DATA_ERROR`=7, `XZ_BUF_ERROR`=8. In `XZ_SINGLE` mode a successful decode returns exactly `XZ_STREAM_END`; anything else means the output buffer's contents are undefined (stated in `include/xz.h:209-217`). The `ret > 0` test therefore passes corrupt, truncated, or mis-sized IRX blobs straight to `SifExecModuleBuffer` for execution on the IOP. The same function also never checks `memalign(64, irx_size)` (line 239) for NULL before decoding into it, and stores the `size_t` uncompressed size in an `int irx_size` (line 235), truncating any size the (unbounded, CAND-W7-3) index walk invents.
Fix: Require `ret == XZ_STREAM_END` before executing; return an error otherwise. Check `irx_data != NULL`; keep the size in `size_t` (or reject `> INT_MAX`). Three-line change, no addition of mechanism.
Benefit: A single corrupt/truncated embedded module (build-artifact damage, bad media read on the file-loaded paths) currently becomes *executed garbage on the IOP* instead of a clean load failure — for every one of the ~25 call sites of this function.
Effort: Trivial.
Risk: Low — call sites already treat a negative return as failure; making decode errors return negative matches the contract they assume (e.g. `SMS_IOP.c:724,1032` check `< 0`).
Confidence: READ (code and enum values read directly; consequence follows from the vendored header's own single-call contract note).

### CAND-W7-2 — `SMS_LZMA2Decompress` decompresses into the buffer it just freed (NULL) on size mismatch
Category: 6 bugs/security
Locations: `src/SMS_EE.c:946-965` (sole decompressor for `g_JPEGData`, `src/SMS_JPEGData.c:316-322`); `src/lzma2.c:84-122`
Problem: Read verbatim: `retVal = SMS_SyncMalloc(m_Size)`; `lSize = lzma2_get_uncompressed_size(...)`; `if (lSize != m_Size) { free(retVal); retVal = NULL; }` — and then `lzma2_uncompress(..., UNCACHED_SEG(retVal), lSize)` is called **unconditionally**, with the NULL/freed pointer as the output buffer and the untrusted `lSize` as the output size. The return value of `lzma2_uncompress` is also discarded, so even on the match path a failed decode returns a buffer of undefined contents to `SMS_InitVU`, which DMA-sends it to a VU. The same path fires if the initial `SMS_SyncMalloc` fails (retVal already NULL).
Fix: `return NULL` inside the mismatch branch (and on malloc failure); check `lzma2_uncompress(...) == XZ_STREAM_END`, freeing and returning NULL otherwise. Net change: a guard, not new machinery.
Benefit: Removes a guaranteed write-through-NULL (via the uncached-segment alias) on any size disagreement between a blob's XZ index and its table entry — the exact failure the mismatch check was written to catch, currently converted from a clean NULL return into a crash/corruption.
Effort: Trivial.
Risk: Low — callers already handle NULL (`SMS_InitVU` checks `if (lpData)`).
Confidence: READ.

### CAND-W7-3 — `lzma2_get_uncompressed_size` walks the XZ index with no bounds at all
Category: 6 bugs/security
Locations: `src/lzma2.c:51-82` (`lzma2_decode_size`), `src/lzma2.c:84-122` (`lzma2_get_uncompressed_size`), `src/lzma2.c:29-49` (`decode`); callers `src/SMS_IOP.c:237`, `src/SMS_EE.c:950`, `src/SMS_GS_2.c:567`
Problem: Four structural defects in one mechanism: (1) no `size >= 12` check — `footer = (xz_footer_t*)(buf + size - 12)` reads before the buffer for a short input; (2) `footer->backward_size` is trusted unchecked, so `index = (unsigned char*)footer - ((i+1)*4)` can point arbitrarily far *before* the buffer; (3) `lzma2_decode_size` receives no end pointer and sums `records` index records by walking forward without limit; (4) every `buf += decode(...)` ignores `decode()`'s 0-on-error return, so a malformed varint makes the walk re-read the same bytes as successive fields instead of failing. Most inputs are compile-time blobs, but one is a **user-supplied file**: `GSFont_Load` (`src/SMS_GS_2.c:567`) runs this walk over the body of an `.mtf` font file read from disk, and its only validation is equality with a size stored in that same file's header — a crafted font makes the walk read out of bounds and can make the subsequent `malloc(lDataSize)` / decode sizes arbitrary.
Fix: Thread the buffer end through: reject `size < 12`, reject a computed index outside `[buf, footer]`, make `lzma2_decode_size` stop at the footer and fail (return 0) when any `decode()` returns 0. All inside the existing two functions.
Benefit: Bounds the one uncompressed-size oracle every XZ consumer in the tree trusts for allocation sizes (IRX modules, JPEG VU programs, fonts, icons).
Effort: Small.
Risk: Low — all in-tree blobs script-verified well-formed above, so strict bounds change nothing for valid inputs.
Confidence: READ (absence of checks verified by reading both functions whole; the font-file caller verified at SMS_GS_2.c:540-580).

### CAND-W7-4 — Default (BDM) build carries 23,488 bytes of `g_DataBuffer` segments nothing can reach
Category: 4 size/memory
Locations: `src/SMS_Data.c` segments UMS.IRX (8,504 + 8 pad), USBD.IRX (9,140 + 12 pad), SIO2MAN.IRX (5,817 + 7 pad) = 23,488 aligned bytes; consumers `src/SMS_IOP.c:504,674,688`; build switch `Makefile:3` (`BDM ?= 1`), `Makefile:49-57` (`IOP_OBJS`)
Problem: The only references to `SMS_USB_MASS_OFFSET`, `SMS_USBD_OFFSET` and `SMS_SIO2MAN_OFFSET` outside `include/SMS_Data.h` are in the `#else` (non-BDM) branches of `SMS_IOP.c` (lines 504, 674, 688). The default build is `BDM=1`, where those modules load from the `bin2c`'d `irx/*.irx.xz` objects instead (SMS_IOP.c:493,642,646). Because the three segments live inside the single `g_DataBuffer` array, the linker cannot strip them: every default ELF — and the EE RAM it occupies — carries 23,488 dead bytes (~8.8% of the 266,560-byte buffer).
Fix: Move the three legacy segments (with their header size/offset entries) into a separate `SMS_DataLegacy.c` compiled only for `BDM=0`, keeping the non-BDM build bit-identical in layout. Deletion of the flavour is *not* proposed — it is a supported `?=` switch.
Benefit: 23,488 bytes of `.data` and resident RAM back in the default build, on a 32 MB console.
Effort: Medium (segment move + offset chain must stay inside the legacy TU; the remaining segments' offsets are computed from the chain in SMS_Data.h, so the split needs the legacy block kept contiguous at the same relative offsets or its own header chain).
Risk: Medium — offset arithmetic is header-chained; a careless split shifts every following segment. Mitigated by the per-segment script in section (a), which re-verifies payload/padding equality after any move.
Confidence: READ (references enumerated by grep over the whole tree; build default read from the Makefile).

### CAND-W7-5 — `TranslateUTF8` can write `bufSize + 1` bytes into the caller's buffer
Category: 6 bugs/security
Locations: `src/SMS_InverseCodePages.c:138-195` (loop at :145, final `*outBuff = '\0'` after the loop); callers `src/SMS_SubtitleContext.c:482-483`, `src/SMS_SubtitleContext.c:657-659`; safe caller `src/SMS_InverseCodePages.c:229-239` (`DisplayUTF8`)
Problem: The loop writes one output byte per iteration while `length < bufSize`, i.e. up to `bufSize` bytes, and then unconditionally writes a terminating NUL at `outBuff[bufSize]` — one byte past the contracted capacity. `DisplayUTF8` compensates by passing `aSize - 1`, but the subtitle callers do not: line 483 passes `SUB_MAXLEN` for `char lBuff[SUB_MAXLEN]`, and line 658 passes the remaining space in `lLine[SUB_MAXLEN]` in-place. Today they are saved only because subtitle input lines are length-bounded elsewhere (`File_GetString(..., SUB_MAXLEN)`), not by this function's contract — any future caller (or a longer-line path) gets a 1-byte stack overwrite.
Fix: Reserve the NUL inside `TranslateUTF8` (loop while `length + 1 < bufSize`, or treat `bufSize` as capacity-including-NUL and update the two subtitle call sites to match `DisplayUTF8`'s convention). One convention, applied in one place.
Benefit: Closes a latent 1-byte stack-buffer overflow in subtitle rendering of user-supplied subtitle files, and removes a contract that each caller currently has to rediscover.
Effort: Trivial.
Risk: Low — behaviour changes only for inputs that previously filled the buffer exactly, which then lose their last character to the NUL instead of corrupting the stack.
Confidence: READ (loop bound and post-loop write read directly; caller buffer sizes read at both call sites).

### CAND-W7-6 — `g_ASCII` is declared 4,572 in its consumer and defined 5,416 in its data file
Category: 6 bugs/security
Locations: `src/SMS_GSFont.c:11` (definition, 5,416 — script-counted exact); `src/SMS_GS_2.c:35` (extern, 4,572); `src/SMS_GS_2.c:50-53` (`s_FontSizes`); `src/SMS_GS_2.c:502-506` (`GSFont_Get`); `src/SMS_Config.c:292` (`_load_font`)
Problem: An extern array declaration with a different bound than the definition is undefined behaviour in C, and here it is load-bearing: `s_FontSizes[0]` is initialised with `sizeof(g_ASCII)` *in SMS_GS_2.c's translation unit*, so `GSFont_Get(0, ...)` reports 4,572 for a 5,416-byte font (the other four fonts' externs match their definitions exactly — script-verified). The one consumer, `_load_font`, uses that number as the embedded buffer's capacity when deciding whether a replacement `ascii.mtf` fits in place: fonts of 4,573–5,416 bytes needlessly take the `malloc` path (and, on malloc failure, are silently not loaded). The error is in the safe direction — it understates, so nothing overflows — which is presumably why it survived.
Fix: Declare the font arrays with unspecified bound (`extern unsigned char g_ASCII[];` etc.) in SMS_GS_2.c and export the sizes from SMS_GSFont.c's TU (or move `s_FontSizes` there). Removes the duplicate source of truth for all five fonts, not just the wrong one.
Benefit: `GSFont_Get` tells the truth for the ASCII font; the class of bug (hand-copied sizes drifting from generated data) is removed for all five fonts.
Effort: Small.
Risk: Low.
Confidence: READ (both declarations and the `sizeof` consumer read directly; counts script-verified).

### CAND-W7-7 — `UnicodeToCP`'s binary search can never reach table index 0 (proven by simulation)
Category: 6 bugs/security
Locations: `src/SMS_InverseCodePages.c:114-136` (`UnicodeToCP`); tables at `src/SMS_InverseCodePages.c:22-110`
Problem: The search starts `index = 64, step = 64` and halves `step` *before* moving (`step >>= 1` inside the loop before the index update). An exact python port of the algorithm, run against all four script-extracted tables, finds **exactly one unreachable entry per table — index 0**: in the 1250/1252/1253 tables U+0081 should yield byte 129 and in the 1251 table U+0098 should yield byte 152; all four return `'?'` (the port returns 63 in each failing case). Every other entry (127/128 per table) is found, both arrays are strictly monotonic, and each index array is a permutation of 0..127 — the tables are perfect; only the walk is wrong (repeatedly stepping left from 64 lands on 1 with `step` already 0, so index 0 is never probed; index 127 *is* reachable, so the walk is asymmetric).
Fix: Replace the hand-rolled walk with a standard half-open binary search over `[0, 128)`. No table data changes — per the table rule, no replacement values are proposed or needed.
Benefit: Small in practice (U+0081/U+0098 are C1 control codes, rare in filenames/subtitles) but the fix makes the function's contract — every table entry findable — actually true, verified by re-running the simulation.
Effort: Trivial.
Risk: Low — simulation gives a complete before/after test oracle for all 512 entries.
Confidence: READ (algorithm ported line-for-line and executed against the extracted tables; failure set is exactly `{index 0}` × 4 tables).

### CAND-W7-8 — Icon loader hand-declares `lzma2_uncompress` with the wrong type and ignores its result
Category: 2 duplication
Locations: `src/SMS_GUIcons.c:34` (declaration), `src/SMS_GUIcons.c:1405-1406` (call); real prototype `include/lzma2.h:33-34`
Problem: Every other consumer includes `<lzma2.h>` (SMS_EE.c:28, SMS_GS_2.c:24, SMS_IOP.c:44). `SMS_GUIcons.c` instead re-declares the function by hand as `extern int lzma2_uncompress(unsigned char*, unsigned int, unsigned char*, unsigned int)` — the real function returns `enum xz_ret` and takes `size_t` parameters. It happens to be ABI-compatible on this target (enum ≈ int, `size_t` = `unsigned int` on o32), so it links and runs, but it is a second, divergent copy of the truth — and the call discards the result entirely, so a failed decode of `g_RGBA_Blob` would silently slice and display garbage icons instead of failing. (The blob itself is verified good — section (a) — so this is latent.)
Fix: Delete the hand declaration, include `<lzma2.h>`, and check the return against `XZ_STREAM_END` (on failure, leave `s_pIconData` unset). Less code, one prototype.
Benefit: Removes a type-punning declaration that breaks silently if the wrapper's signature ever changes, and gives the icon path the same failure behaviour as the other three consumers.
Effort: Trivial.
Risk: Low.
Confidence: READ.

### CAND-W7-9 — `lzma2.c` exports dead, collision-prone functions named `encode` and `decode`
Category: 1 dead code
Locations: `src/lzma2.c:10-27` (`encode`), `src/lzma2.c:29-49` (`decode`)
Problem: Tree-wide grep shows `encode` has **no callers at all** — not even inside `lzma2.c` — and `decode` is used only by `lzma2_decode_size` in the same file. Neither is declared in `include/lzma2.h`. Both have external linkage under the two most generic names imaginable in a monolithically linked ELF that also links AAC/MP3/FLAC codec code: any future object defining `encode`/`decode` collides at link time, and today's dead `encode` is pure surface area.
Fix: Delete `encode`; make `decode` `static`. (If a varint encoder is ever needed for a writer path, it can come back with an `lzma2_`-prefixed name in the header.)
Benefit: −19 lines, two fewer exported symbols, one link-collision hazard gone.
Effort: Trivial.
Risk: None found — no caller exists to break (grep over `src/` + `include/`).
Confidence: READ.

### CAND-W7-10 — Generated icon header (and its generator) still say "11 dev" — there are 13 dev slots
Category: 7 build/CI/clutter
Locations: `src/SMS_IconsRGBA.c:1-6` (generated header); `tools/bake_icons_rgba.py` (header template string and the `main()` comment, both saying 11); authoritative counts: `tools/bake_icons_rgba.py` `DEV` list (13 names, script-counted), `src/SMS_GUIcons.c` `GUI_LoadIcons` (dev loop `i < 13`, comment says 13)
Problem: The arithmetic proves 13: the blob decompresses to 205,824 bytes = 9×4,096 + 12×4,096 + **13**×9,216; with 11 dev icons it would be 187,392. The generated file's own header — the text a maintainer reads first when touching the icon pipeline — understates the dev set by two slots (MX4SIO/ATA/iLink/MMCE/DVDVID/CDVDERROR were added after the text was written), and because the text lives in the bake tool's template, re-running the tool re-emits the wrong header.
Fix: Change "11 dev" to "13 dev" in the tool's template/comment and in the generated file's header (or regenerate). Two words in two places.
Benefit: The next person extending the device-icon set works from a header that matches the slicing code instead of silently mis-slicing by two icons.
Effort: Trivial.
Risk: None (comment-only).
Confidence: READ (slot lists counted by script; raw-size arithmetic verified by actual decompression).

### CAND-W7-11 — Turkish font pack ships the same font twice under two names
Category: 2 duplication
Locations: `fonts/Turkish by nethunter/latin1.mtf` and `fonts/Turkish by nethunter/WinLatin1.mtf` (both 8,328 bytes, md5 `621f497d0a5d0c5c62df649eda78559b`)
Problem: Byte-identical files in the same folder (the pack's third file, `ascii.mtf` at 6,224 bytes, is distinct, as is every other file in `fonts/` — full-tree md5 census in section (a)). One of the two names is redundant weight in every clone/ship of the font pack.
Fix: Remove one copy; if both filenames are load-bearing for different tools' expectations, say so in the pack's existing `Readme.txt` and keep both deliberately — otherwise delete `WinLatin1.mtf` (the `latin1.mtf` name matches the sibling packs' naming: `fonts/Hebrew/latin1.mtf`).
Benefit: 8,328 bytes and one confusing either/or choice removed from the pack.
Effort: Trivial.
Risk: Low — a user who copied the pack and references the removed name in a local config would need the other name; the Readme note covers discovery.
Confidence: READ (hash equality is exact).

### CAND-W7-12 — `images/` contains an editor backup and two large source PNGs nothing references
Category: 7 build/CI/clutter
Locations: `images/jellyfish.jpg.bak-512` (25,390 bytes); `images/main_bg.png` (230,952 bytes); `theme/splash_composited.png` (232,705 bytes)
Problem: The build embeds exactly three JPEGs from `images/` (`Makefile:95,103-104`: `jellyfish.jpg`, `splash.jpg`, `main_bg_mini.jpg` via `bin2c`, consumed at SMS_GUIDesktop.c:40-45). A repo-wide grep (Makefile, `src/`, `tools/`, docs) finds **no reference** to `jellyfish.jpg.bak-512` — it is a resized-variant backup left in the tree — nor to `main_bg.png` / `splash_composited.png`, which by name and size are the INFERRED master artwork for `main_bg_mini.jpg` / `splash.jpg` but are not wired into any bake step (unlike the icon PNGs, which have `tools/bake_*.py`).
Fix: Delete `jellyfish.jpg.bak-512`. For the two PNGs: either wire them into a documented bake step (as the icons are) or move them under `docs/`/artwork with a one-line note — do not leave 463,657 bytes of unreferenced binaries indistinguishable from build inputs.
Benefit: 25,390 bytes removed outright; the remaining two large binaries become discoverable as sources instead of looking like dead build inputs.
Effort: Trivial.
Risk: Low — the `.bak` has no consumer; the PNGs have no *build* consumer (a human re-editing artwork by hand is the only user, served by the note/move).
Confidence: READ for the `.bak` and the absence of references (grep over the tree); INFERRED for the two PNGs being the masters of the shipped JPEGs (name/size correspondence only).

---

## (c) Checked-clean notes

- **`g_DataBuffer` segment table is exact.** All 17 payloads match `include/SMS_Data.h` sizes and all paddings complete the 16-byte alignment (script, section (a)). The 16-byte difference between the 266,544-byte initialiser and the 266,560-byte declared buffer is ordinary C zero-fill of the tail, inside the region `SMS_AUDIO_BUFFER_SIZE` treats as scratch — not a shortfall.
- **All five JPEG VU blobs are valid XZ** and decompress to exactly the raw sizes declared in `g_JPEGData[5]` (python `lzma`, section (a)). `s_IDCT`/`s_CSC*` have no other consumers than `SMS_InitVU` via that table.
- **Icon pipeline is reproducible.** Re-running `tools/bake_icons_rgba.py` and `tools/bake_loading_rgba.py` in a scratch dir produces byte-identical output to the committed `SMS_IconsRGBA.c` / `SMS_LoadingRGBA.c`; `SMS_BallRGBA.c` matches a direct conversion of its PNG under the same algorithm. Alpha ≤ 0x80 and transparent-RGB-zeroed invariants hold in all three (script check).
- **`g_SMSound` partition is exact** — no overlaps, uniform 17-byte gaps, last sound ends 17 bytes before the array end (section (a)). The scratch overlays into `g_SMSounds` (`g_CWD`, `g_TOC`, stacks, packets — include/SMS_Sounds.h:25-41) are a documented dual use of the EE copy after `SPU_LoadData`; no conflicting live use was found in the consumers read.
- **Inverse-codepage tables themselves are flawless**: 4 × 128 Unicode entries strictly monotonic, 4 × 128 index entries each a permutation of 0..127 (script). The only defect is the search walk (CAND-W7-7). The `GSCodePage` enum has exactly the four values the designated initialisers cover — no zero-filled phantom codepage.
- **`About` obfuscation round-trips.** Simulating `UnHash` (About.c:31-56) over `src/About_Data.s` decodes exactly `NSTR`=60 readable lines and stops 27 zero bytes short of the end (alignment padding); every record's length byte is < 128, so the plain-`char` length read cannot go negative on current data. The XOR chain is eccentric but functional and load-bearing for the credits screen — not proposed for removal.
- **`lng/` files are uniformly shaped**: 19/19 files at 269 lines / 268 non-empty, no duplicates. No untranslated-content claim is made (see census caveat).
- **Theme duplicate PNGs are slot semantics, not waste to dedupe**: `s_IconCDROM.png` == `s_IconDVD.png` and `s_IconHost.png` == `s_IconSMB.png` occupy *distinct* fixed-offset slots in the bake order (DEV[1]/DEV[5], DEV[4]/DEV[6]); the consumer slices by position, so identical art for two device types is a deliberate reuse the fixed-offset format cannot express more cheaply without changing the slicer. `theme/sms-bdm_assets_3/{error,loading}.png` equal `theme/icons/{s_IconError,g_IconLoading}.png` but serve a separate asset bundle path. Noted, not counted as findings.
- **`irx/` plain+`.xz` pairs are consistent**: every `.xz` decompresses to its sibling `.irx` (where present) with equal md5; the five `.xz`-only modules are BDM-era modules whose sources live in the IOP build, not in this directory. The plain `.irx` files are the `tools/compress_irx.py` inputs — both forms have a role.
- **Vendored XZ core**: no local modifications, dead config, or licence issues found (mechanism scan; config in section (a)). `xz_crc32.c`'s table-free compact implementation matches the CRC32-only configuration.
- **`g_IconSMS` (SMS_ConfigIcon.c)** is a complete 2,020-byte PS2 memory-card icon, written whole by both call sites; it is *not* a duplicate of the `ship/APP_SMS3/*.icn` set (different sizes, different folder purpose — runtime-written SMS.icn vs shipped app-folder icons).
- **Fonts in `fonts/`** are user-loadable alternates consumed by `SMS_Config.c:_load_font` by name (`ascii`/`cyrillic`/`latin1` per pack); their sizes legitimately differ from the five embedded fonts — no embedded/pack duplication beyond CAND-W7-11.
