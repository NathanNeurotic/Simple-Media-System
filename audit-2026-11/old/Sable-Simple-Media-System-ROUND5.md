# Sable Audit — Simple-Media-System (SMS) — ROUND 5

**Repo:** NathanNeurotic/Simple-Media-System · **Pin:** `e65ced39a27eee8d996e9257e21594d1cac1c93d` (master) — clone HEAD == `git ls-remote origin master` at start and at finish; the round-3/4 pin has not moved, so there is no delta and no revalidation of SMS-001…056 is owed. Working tree clean at start and finish. Read-only throughout; nothing built (no PS2SDK in this VM), nothing hardware-verified — Category 5 magnitudes are structural only.

**Scope (round 4's still-unread list, verbatim):** (1) GUI logic end-to-end — SMS_GUIMenuSMS.c, SMS_GUIFileMenu.c, SMS_GUIcons.c, SMS_GUICmdProc.c, SMS_GUIMenu.c, SMS_GUIDevMenu.c, SMS_PlayerControl.c beyond round 2's classified string sites; (2) rendering logic — SMS_GSFont.c, SMS_GS_1.c, SMS_GS_2.c, SMS_IPU_1.c; (3) remainders — SMS_FileContext.c backends, SMS_EE.c, SMS_Player.c, SMS_Sounds.c logic, SMS_MC.c, SMS_PAD.c, SMS_RC.c, main.c; (4) the formal verdict.

**Method note:** the three file groups (GUI menus, GUI framework/player-control, rendering) were read end-to-end by delegated readers; every headline mechanism below was then spot-checked against the clone at the pin by the coordinating reader before numbering (palette wrap arithmetic, `g_ASCII` sizes, `IPU_UnpackImage` caller grep, dead-icon reference grep, `SMS_ListPushBack` NULL contract, `GUI_Status` realloc block, `GSFont_UnpackChr` indexing, Artwork marker walk, and the FileContext/EE candidates read first-hand). The Task-3 remainders (SMS_FileContext.c, SMS_EE.c, SMS_Player.c, SMS_MC.c, SMS_PAD.c, SMS_RC.c, main.c, SMS_Sounds.c) were read first-hand.

**Result: 36 new findings, SMS-057…092.** No revisions to SMS-001…056 were forced. SMS-035 stays settled (docs-only), untouched.

---

## Findings

### SMS-057 — Palette rotate wraps at 17 but not at 0: pressing Left at colour 1 stores 0, and the next render reads `g_Palette[-1]`
- **Category:** 6 (correctness)
- **Locations:** `src/SMS_GUIMenuSMS.c:2190-2200` (`_rotate_palette`), `:2219-2239` (`_txtclr_handler`, inline copy; OOB read at `:2231`), `:2241-2258` (`_sltxt_handler`, inline copy; OOB read at `:2253`), `:2348-2362` (`_rotate_palette_2`); sinks `src/SMS_GUIFileMenu.c:432,459,524,550,591,647` (`g_Palette[*idx - 1]`)
- **Problem:** Palette indices are stored 1..16 (`g_Palette[16]`) and always consumed as `g_Palette[idx-1]`. All four rotate copies do `lVal += aDir; if (lVal == 17) lVal = 1; else if (lVal < 0) lVal = 16;`. With `aDir = -1` (handlers are called with +1/−1, `src/SMS_GUIMenu.c:347,351`) and current value 1, the result is 0 — not `< 0`, so it is stored unwrapped. The two inline handlers then immediately read `g_Palette[0-1]` in the same call; for the browser colours, `_rotate_palette`'s redraw reaches `GUIFileMenu_Render`'s `g_Palette[*m_pColor - 1]` where `m_pColor` is `unsigned int*`, so `0-1` wraps to `UINT_MAX` — a wild read, not merely one element low. Reachable by ordinary menu use; no corrupt input needed.
- **Fix:** Change the lower test from `lVal < 0` to `lVal < 1` in the rotate logic (once, if SMS-083's dedup lands first). No new code paths.
- **Benefit:** Removes a user-reachable out-of-bounds/wild read in a settings menu. **Effort:** S · **Risk:** Low · **Confidence:** High — READ (wrap arithmetic and the `[-1]` read verified at the pin).

### SMS-058 — Raw file-loaded config values are used as array subscripts with no range check in the menu layer
- **Category:** 6
- **Locations:** `src/SMS_GUIMenuSMS.c:553-556` (`s_Charsets[g_Config.m_DisplayCharset]`, table of 4), `:619` (`s_Speeds[g_Config.m_CDVDSpeed]`, table of 3), `:816`, `:1033-1039`, `:1114` (`s_ExitTo[g_Config.m_BrowserFlags >> 28]`, table of 3, shift yields 0..15; `:1039` dereferences the result), `:2649` (`s_ClrRes[g_Config.m_ColorDepth]`, table of 2); palette sinks `src/SMS_GUIFileMenu.c:432,524,591`
- **Problem:** `SMS_LoadConfig` reads a raw 892-byte struct directly from `SMS.cfg` (`src/SMS_Config.c:700,730` — `fioRead(lFD, &g_Config.m_DisplayMode, 892)`) and at the pin guards only `m_SkinName` NUL-termination and `m_PlayerAC3RL`; the `m_BrowserFlags` top-nibble clear exists only under `#ifdef EMBEDDED`. A corrupt or foreign-version config therefore flows unchecked into these subscripts on first menu open / browser redraw, before any handler can repair it. Worst case: `s_ExitTo[lIdx]->m_pStr` at `:1039` dereferences whatever pointer-sized value sits past the 3-entry table.
- **Fix:** Clamp/mask at the use sites in these files (`>> 28` masked to the 0..2 range `_exit_2_handler` already enforces; speed/charset/colour-depth bounded to their table sizes). No new tables, no config-format change.
- **Benefit:** A corrupt config degrades to defaults instead of a wild-pointer deref. **Effort:** S/M · **Risk:** Med · **Confidence:** High READ for the subscript sites and the raw read; Med on per-field severity (ExitTo is a wild deref; the others read adjacent `.data`).

### SMS-059 — `GSFont_UnpackChr` indexes the glyph-offset table with an unvalidated character code
- **Category:** 6
- **Locations:** `src/SMS_GS_2.c:124-154` (index reads at `:128`, `:153-154`); helper `_next_block` at `:69-122`
- **Problem:** `lWidth = (lpFont+11)[aChr]` and `lpOffs[aChr]` are read with no `aChr < apHdr->m_nGlyphs` check, then `lpOffset` is computed from that unchecked offset and `_next_block` dereferences it. In-tree `GSFont_Init` callers are bounded (0–95 / 0–127), but the subtitle renderers pass file-derived bytes: the MB path passes a raw subtitle byte (0–255) as the glyph index into a sub-font header, and the SB path maps subtitle bytes 0x00–0x1F to indices 224–255 against fonts with 96/128 glyphs. `File_GetString` strips `\r` only before `\n`, so tab / lone-`\r` / control bytes in a subtitle line survive to the renderer.
- **Fix:** One guard at the top of `GSFont_UnpackChr`: `if (aChr >= apHdr->m_nGlyphs) return 0;` — callers already treat width 0 as an empty glyph.
- **Benefit:** Closes a file-data → wild-offset read in the text renderer. **Effort:** S · **Risk:** Low · **Confidence:** mechanism READ (indexing verified at the pin); subtitle trigger INFERRED.

### SMS-060 — `SMS_LZMA2Decompress`'s size guard frees the buffer, then decompresses into it anyway
- **Category:** 6
- **Locations:** `src/SMS_EE.c:946-965` (guard at `:952-955`; unconditional `lzma2_uncompress` at `:957-962`); sole caller chain `SMS_InitVU` at `:967-979`
- **Problem:** On `lSize != apData->m_Size` the function frees `retVal`, sets it NULL — and falls through to `lzma2_uncompress(..., UNCACHED_SEG(retVal), lSize)`, i.e. it writes the decompressed output to the uncached alias of address 0. The guard exists precisely for this case and does not guard. The same call also runs unchecked when `SMS_SyncMalloc` itself returns NULL. The data is embedded (VU microcode blobs), so the trigger is a programming/data error rather than hostile input — but the failure mode converts a clean NULL return (which `SMS_InitVU` already handles) into a low-memory write.
- **Fix:** Return NULL after the free (one line); the caller already treats NULL as failure. No addition.
- **Benefit:** A corrupt embedded blob becomes a clean init failure instead of a memory-corrupting write. **Effort:** S · **Risk:** Low · **Confidence:** High — READ (verified first-hand at the pin).

### SMS-061 — `GSFont_UnpackChr`'s RLE decode can overrun its fixed 512-byte destination; the geometry comes from the font file
- **Category:** 6
- **Locations:** `src/SMS_GS_2.c:142-202` (`memset(apBuf,0,512)` at `:142`; `lEnd = lWidth * m_GlyphHeight` at `:152`; row advance `lpBuff += 16` at `:193`); destinations `src/SMS_GS_2.c:214` (`lCharBuf[512]` in `GSFont_Init`) and the subtitle packet image slot (512 B by construction)
- **Problem:** The decoder assumes the font's glyph width/height keep the unpack inside 512 bytes (built-ins: height 0x20 = 32 rows × 16 B = exactly 512). Nothing enforces it. Fonts are replaceable from files with no header validation — `GSFont_Set` installs `.mtf` data loaded by `SMS_Config.c` `_load_font` (`:752`) and `.mbf` data from `GSFont_Load`. A crafted/corrupt font with `m_GlyphHeight` up to 255 drives up to ~4,080 B into the 512 B stack buffer in `GSFont_Init`; a width byte up to 255 puts up to 128 B in a row against the hard-coded 16 B stride. The block loop also trusts RLE block lengths, so source reads advance past the font data with no end bound.
- **Fix:** Clamp in `GSFont_UnpackChr`: reject/zero glyphs whose `m_GlyphHeight > 32` or `lWidth > 32` (the destination's actual geometry), and stop the block loop at the font's known size. No new tables or allocators.
- **Benefit:** A replaceable font file can no longer smash the stack of the font initialiser. **Effort:** S–M · **Risk:** Low (built-in fonts are exactly 32-row) · **Confidence:** missing clamp READ; file-font trigger READ (unvalidated load path traced in-tree).

### SMS-062 — `CDDA_InitFileContext` dereferences the internal init result without a NULL check
- **Category:** 6
- **Locations:** `src/SMS_FileContext.c:1228-1240` (call at `:1228`; unconditional `retVal->m_pPath` malloc at `:1233`, `strcpy` follows)
- **Problem:** `CDDA_InitFileContextInternal` returns NULL on ordinary failure — including its first `Fill` failing on a disc read error, not just OOM (its contract is honoured by `CDDA_LoadDirectoryList` at `:824`, which checks). `CDDA_InitFileContext` instead writes `retVal->m_pPath`, `m_StreamSize`, `m_pOpenParam`, `Open` and `strcpy`s into `m_pPath` unconditionally, and the `m_pPath` malloc itself is unchecked before the `strcpy`.
- **Fix:** The same NULL test the neighbouring caller already uses; return NULL on failure.
- **Benefit:** A scratched/misread CDDA image returns "cannot open" instead of crashing the browser. **Effort:** S · **Risk:** Low · **Confidence:** High — READ (verified first-hand).

### SMS-063 — `_read_string` runs `strlen` on a stack buffer the disc image never has to NUL-terminate
- **Category:** 6
- **Locations:** `src/SMS_FileContext.c:805-820` (VLA at `:809`, `strlen` at `:814`); callers `:846` (64), `:847` (512), `:863` (32), `:1147` (32)
- **Problem:** `_read_string` reads `aMaxLen` raw bytes from the CDDA disc image into a stack VLA and calls `strlen` on it. If the image's name/description field fills the field with no NUL — a malformed or hostile CDDA image — `strlen` runs past the VLA across the stack. The `Read` return count is checked only for non-zero, never used to bound the scan.
- **Fix:** Bound the scan by the byte count `Read` returned (or NUL-force the last byte before scanning) — a one-line change in one helper; all four callers inherit it.
- **Benefit:** Disc-supplied strings can no longer over-read the stack during directory load. **Effort:** S · **Risk:** Low · **Confidence:** High READ on the mechanism; the trigger requires a non-terminated field in a user-loaded disc image.

### SMS-064 — `GUI_LoadIcons` dereferences two unchecked allocations, one of them through the LZMA decompressor
- **Category:** 6
- **Locations:** `src/SMS_GUIcons.c:1405-1406` (`memalign(64, g_RGBA_RawSize)` → `lzma2_uncompress(..., lpBlob, ...)`), `:1412,1417-1444` (`SMS_SyncMalloc(18*4096 + 12*4096 + 9216*13)` → `memcpy` and pointer-slice writes)
- **Problem:** Neither allocation is tested. On failure (the icon buffer is 242,688 B plus the raw-size scratch blob, on a 32 MB PS2, plausibly after skins/backgrounds are loaded), the decompressor writes the whole raw icon set through NULL and the `memcpy` loop writes ~237 KB through NULL — a crash at GUI initialisation instead of any recoverable error.
- **Fix:** One guard after each allocation (`if (!lpBlob) return;` / free-and-return for the second) — callers already tolerate icons not drawing. No new allocation or fallback art.
- **Benefit:** GUI startup survives low memory instead of writing a quarter-megabyte through NULL. **Effort:** S · **Risk:** Low · **Confidence:** High — READ.

### SMS-065 — Unchecked GUI object-creation and row-render allocations, each dereferenced on the next lines
- **Category:** 6
- **Locations:** `src/SMS_GUIMenuSMS.c:425-428` (`_create_sample` calloc → vtable write; NULL propagates to `s_pSample` and is derefed at `:437`); `src/SMS_GUIFileMenu.c:1645-1655` (`GUI_CreateFileMenu` calloc → vtable), `:333,344-347` (`GUIFileMenu_RenderItem` `SMS_SyncMalloc` → `UNCACHED_SEG` packet use); `src/SMS_GUIMenu.c:405-407` (`GUI_CreateMenu` calloc → vtable); `src/SMS_GUIDevMenu.c:679-681`, `:309` → `:332-334` (`_DevMenuItem`); `src/SMS_GUICmdProc.c:85-87`, `:363-364` → `:369-373`; `src/SMS_GUIDesktop.c:132-134`, `:730-732`, `:799-801`; `src/SMS_GUIFileCtxMenu.c:373-377`; `src/SMS_GUIMiniBrowser.c:132-133` → `:140-145`; `src/SMS_GUIClock.c:169` → `:185,240` (clock capture `memalign` → DMA `REF` built on it)
- **Problem:** Every `GUI_Create*` constructor writes vtable fields into the calloc result unconditionally; render/clock paths write through theirs within a few lines. The realistic trigger is a fragmented 32 MB EE heap after a player session. The inconsistency is provable inside GUIFileMenu itself: the poster/artwork renderers *do* check the identical allocation (`if (!lpPack) break;` at `:448,536`); the ordinary list renderer does not — under memory pressure the common path is the crashing one. Distinct from SMS-045 (container/open paths): these are GUI creation/render sites, each with a concrete deref line.
- **Fix:** One guard per site — return NULL / skip the menu / skip the clock capture — reusing the test the neighbouring renderers already have. No new allocation scheme.
- **Benefit:** Low memory degrades the GUI instead of crashing it, uniformly. **Effort:** M (many sites, 1–2 lines each) · **Risk:** Low · **Confidence:** High READ (missing checks); the OOM trigger itself is INFERRED.

### SMS-066 — GS display-list allocations are dereferenced unchecked, including through a realloc helper whose contract is "NULL on failure"
- **Category:** 6
- **Locations:** `src/SMS_GS_1.c:213-215` → derefs `:223,233-237` (`GSContext_NewPacket`); `:315-317` → `:320-322` (`GSContext_CallList`); `:338-340` → `:343-345` (`GSContext_CallList2`); `:288-293` (`GSContext_NewList` memalign → `retVal[0]=0`); `:167-170` (`GSContext_Init` memalign for `m_pDBuf` → cache op on it)
- **Problem:** `SMS_ReallocWithAlign` (`src/SMS_EE.c:55-89`) explicitly returns NULL on allocation failure and zeroes the size. None of the GS_1 call sites check: on failure `m_pDisplayList[aCtx]` becomes NULL with `m_nAlloc` already grown, and the next statements write DMA tags through the NULL pointer. This is the renderer's per-frame packet growth path, not an open path (SMS-045).
- **Fix:** Check once per site and bail (return NULL / skip the append / skip the cache op); callers tolerate a missing packet better than a NULL write. No new allocation strategy.
- **Benefit:** The hottest allocation path in the renderer fails soft. **Effort:** S · **Risk:** Low · **Confidence:** High — READ (helper's NULL return and every deref verified at the pin).

### SMS-067 — `GUI_Status` loses the status-line DMA buffer permanently on realloc failure
- **Category:** 6
- **Locations:** `src/SMS_GUIDesktop.c:833-846`
- **Problem:** `GUI_Status` assigns the `SMS_ReallocWithAlign` result to `s_pDMASL`, then unconditionally raises `s_nDMASL = lWidth` (`:836`) and dereferences `s_pDMASL[0..1]` (`:844-845`) plus `SyncDCache`. On failure the pointer is NULL — and because `s_nDMASL` was already raised, later calls with same/shorter strings fail the `s_nDMASL < lWidth` test and dereference the NULL buffer again: the failure is sticky until a longer string happens to reallocate.
- **Fix:** Keep the old pointer/size on NULL (the realloc-style contract): assign `s_pDMASL`/`s_nDMASL` only on success and return early when the grow fails.
- **Benefit:** One failed grow stops poisoning every subsequent status message. **Effort:** S · **Risk:** Low · **Confidence:** High — READ (verified at the pin).

### SMS-068 — `SMS_ListPushBack(...)->m_Param` dereferences a documented NULL-on-failure return at eleven sites
- **Category:** 6
- **Locations:** `src/SMS_GUIFileMenu.c:457,547,602-604,618-620,636-638,832-834,1620-1622`; `src/SMS_GUIMenu.c:381` (`GUI_MenuPushState`); `src/SMS_GUI.c:929` (`GUI_PostMessage`); `src/SMS_GUIDevMenu.c:330`; tail variant `src/SMS_PlayerControl.c:382,393`
- **Problem:** `SMS_ListPushBack` returns NULL when its node calloc fails (`src/SMS_List.c:38-41`, explicit in source), and `_push_state` in GUIFileMenu itself honours that (`:795-796`). The eleven sites above write `->m_Param` directly on the result. On failure that is a NULL write — mid-redraw in the render paths (which also leak the row pack just built). The PlayerControl variant is worse than a crash: after a failed push it writes the stream index into `s_pLang->m_pTail->m_Param` — the *previous* node's param (silent stream-index corruption), or a NULL-tail deref if the list was empty.
- **Fix:** Capture the node and assign `m_Param` only if non-NULL — the idiom `_push_state` already uses in the same file. No abstraction added.
- **Benefit:** One allocation failure stops being a crash or silent index corruption across every menu. **Effort:** S · **Risk:** Low · **Confidence:** High — READ (contract and representative sites verified at the pin).

### SMS-069 — `_produce_packets` dereferences unchecked `memalign`/`calloc` in subtitle Prepare
- **Category:** 6
- **Locations:** `src/SMS_SubtitleContext.c:714-716` (allocs), derefs `:722-746` (packet header build) and `:754-761` (packet-array field writes)
- **Problem:** The subtitle packet double-buffer (`memalign(64, lnAlloc<<1)`) and the packet array (`calloc(m_Cnt, …)`) are written unconditionally on the next lines. `m_nAlloc` is Σ strlen×640, so a large `.srt` makes this the largest single allocation in the subtitle path; either failure NULL-derefs during `Prepare`, before any packet exists to display.
- **Fix:** Two NULL checks after `:714-716` that free-and-return; `_display` already no-ops when no packet is marked ready.
- **Benefit:** A big subtitle file on a full heap fails to "no subtitles" instead of crashing playback setup. **Effort:** S · **Risk:** Low · **Confidence:** High — READ (verified at the pin).

### SMS-070 — Language/subtitle cycling dereferences a NULL list node for containers with no such stream
- **Category:** 6
- **Locations:** `src/SMS_PlayerControl.c:840-850` (`_do_change_lang`, deref at `:842`), `:404-405` (cursors initialised to list heads); consumers `src/SMS_PlayerMenu.c:109-115` (`_update_lang`), `:128-139` (`_lang_handler`), `src/SMS_PlayerControl.c:1441-1445`
- **Problem:** `PlayerControl_Init` builds `s_pLang`/`s_pSubLang` only from streams flagged AUDIO/SUBTL, so a file-supplied container with no audio stream (silent video) leaves `s_pCurLang = NULL`. `_STR(n)` is `(char*)(n+1)` (`include/SMS_List.h:16`), so `_update_lang` runs `strlen` on a near-NULL pointer when the player menu is merely opened, and `_do_change_lang` reads `NULL->m_pNext` on the first language keypress. (The `sprintf` at `:1445` is the known round-2 class-B site and is not the claim — the claim is the NULL node feeding it.)
- **Fix:** Guard in `_do_change_lang` (`if (!apListNode) return NULL;`) and skip the language row when the list is empty. Two small guards, no new paths.
- **Benefit:** Silent-video files stop crashing the player menu. **Effort:** S · **Risk:** Low · **Confidence:** Medium — mechanism READ; reachability INFERRED (depends on a no-audio container reaching the player menu; the player wiring lives in SMS_Player.c, read this round — no guard observed there either).

### SMS-071 — `_find_signature` puts a 1,204,224-byte buffer on the stack
- **Category:** 6
- **Locations:** `src/SMS_FileContext.c:322` (`u8 lBuf[2352 * 512]`), called from `CDDA_InitContext` (`:1020`), called from `src/SMS_GUIDevMenu.c:314` (mount path)
- **Problem:** The PS2 variant reads 512 CD sectors at once into an *automatic* array of 1.2 MB. Every thread stack this repo provisions itself is 4–16 KB (GUI thread 4,096 B, `src/SMS_GUI.c:92`; player threads 16,384 B, `src/SMS_Player.c:1839-1860`; prefetch 4,096 B). The only stack that could hold it is the ps2sdk crt0 main-thread stack, whose size is set outside this repo (Makefile:140 links the stock crt0) — the file cannot rely on it, and if the mount handler is ever dispatched on a repo-provisioned thread the overflow is unconditional and massive. The Win32 variant of the same function proves the algorithm needs only a 2,352-byte buffer, reading sector by sector.
- **Fix:** Read sector-by-sector like the Win32 twin (or make the buffer static). Pure subtraction of stack demand.
- **Benefit:** Removes a 1.2 MB stack liability from the disc-mount path. **Effort:** S · **Risk:** Low · **Confidence:** code facts READ (array size, storage class, call path verified first-hand); the identity of the executing thread at runtime is INFERRED, and no hardware behaviour is claimed.

### SMS-072 — `CDDA_Fill` drops the final sector whenever (size + offset) is an exact multiple of 2352
- **Category:** 6
- **Locations:** `src/SMS_FileContext.c:482` (last-sector branch of `CDDA_Fill`)
- **Problem:** In the non-streaming fill, the final sector's available length is computed as `(m_Size + m_Offset) % 2352`. When that sum is an exact multiple of 2352 the remainder is 0, so `m_pEnd = m_pPos` and the reader sees EOF — while `m_Pos` is simultaneously set to `m_Size`. Trace (offset 0, size 4704 = 2 sectors): fill 1 delivers 2352 B; fill 2 takes the last-sector branch and delivers 0 B; total delivered 2352 of 4704. The last 2352 bytes of such a file are silently dropped in non-stream (probe/header) reads. Streaming mode (`CDDA_FillStm`) does not share this computation.
- **Fix:** In words: when the modulo is 0 the final sector is full — use 2352 (the branch already knows it read a complete sector). One conditional in one branch.
- **Benefit:** Exact-multiple CDDA files stop losing their last sector to pre-stream reads. **Effort:** S · **Risk:** Low · **Confidence:** Medium — arithmetic READ and traced by hand at the pin; the set of callers that consume a full file through the non-stream path is INFERRED.

### SMS-073 — Transient menus never free their `m_pState` list, node, or `GUIMenuState`
- **Category:** 6 (leak)
- **Locations:** alloc `src/SMS_GUIMenu.c:377-385` (`GUI_MenuPushState`); setup/teardown pairs `src/SMS_GUIMiniBrowser.c:167,173`/`:184`, `src/SMS_GUICmdProc.c:349,379`/`:398`, `src/SMS_GUIFileCtxMenu.c:513,525`/`:536`, `src/SMS_PlayerMenu.c:264,269`/`:327`
- **Problem:** Each of the four transient menus does `m_pState = SMS_ListInit()` + `GUI_MenuPushState()`, but teardown is only `GUI_DeleteObject` (`src/SMS_GUI.c:948-965`), which calls `Cleanup` + `free(lpObj)`; `GUIMenu_Cleanup` (`src/SMS_GUIMenu.c:199-211`) deletes only the GS packet. Nobody calls the existing `GUI_MenuPopState` or destroys the list, so every subtitle-picker, playlist-menu, file-context-menu and player-menu invocation permanently leaks one `SMS_List`, one list node and one `GUIMenuState`. The long-lived SMS menu does pop its states (`src/SMS_GUIMenuSMS.c:502,1121,…`) — the intended discipline exists and is used elsewhere.
- **Fix:** At the four teardown sites, call `GUI_MenuPopState(lpMenu)` + `SMS_ListDestroy(lpMenu->m_pState, 1)` before `GUI_DeleteObject` (not in `GUIMenu_Cleanup` — that is a per-redraw packet cleanup and the state must survive redraws).
- **Benefit:** Menu churn stops leaking heap on a 32 MB machine for the whole session. **Effort:** S · **Risk:** Low · **Confidence:** High — READ.

### SMS-074 — The entire legacy IPU/MPEG icon set in GUIcons is unreferenced: ≈19,230 bytes of `.data` in every ELF
- **Category:** 1 (dead data)
- **Locations:** `src/SMS_GUIcons.c:36-1334` (28 icon arrays: `g_IconBall[316]` at `:36`, `s_IconCDROM[1060]` at `:59`, `s_IconHost`, `s_IconAVI`, `s_IconAVIS`, `s_IconFile`, `s_IconFolder`, `s_IconHDD`, `s_IconCDDA`, `s_IconPart`, `s_IconUSB`, `s_IconDVD`, `s_IconError`, `s_IconDisplay`, `s_IconHelp`, `s_IconNetwork`, `s_IconBrowser`, `s_IconPlayer`, `s_IconOn`, `s_IconOff`, `s_IconSave`, `s_IconExit`, `s_IconFinish`, `s_IconMP3`, `s_IconM3U`, `s_IconShare`, `s_IconSMB`, `s_IconPicture[617]` at `:1294`), `:1336-1368` (six pointer/size tables), `:16` (now-unused `#include "SMS_IPU.h"`)
- **Problem:** The file's own header (`:24-34`) states icons are now baked to `SMS_IconsRGBA.c` and decompressed from the XZ RGBA blob in `GUI_LoadIcons` (`:1388-1447`), which slices only `g_RGBA_Blob`. Tree-wide grep confirms the 28 arrays and 6 tables are referenced only by each other — no live code reads them in any build variant (all `static` except `g_IconBall`, whose only outside trace is an unused `extern` in `src/SMS_PlayerBallSim.c:26`; that file actually loads `g_IconBallRGBA`). The old device table even has 8 entries vs the 13 the live loader fills — abandonment is structural, ≈1,300 source lines.
- **Fix:** Delete `:36-1368` and the `SMS_IPU.h` include; delete the companion unused `extern g_IconBall` in `SMS_PlayerBallSim.c:26`. No addition.
- **Benefit:** ≈19 KB of `.data` and 1,300 lines gone from every build. **Effort:** M (large but purely subtractive; one rebuild + icon display check) · **Risk:** Low · **Confidence:** High — READ (grep-verified across the tree at the pin, re-verified at integration).

### SMS-075 — `IPU_UnpackImage` is dead code (~105 lines including a large inline-asm DMA loop)
- **Category:** 1
- **Locations:** `src/SMS_IPU_1.c:220-324` (definition), `include/SMS_IPU.h:140` (declaration)
- **Problem:** Repo-wide search at the pin finds no caller — only the definition and the header declaration (contrast `IPU_LoadImage`, called from `src/SMS_GUIDesktop.c:279`, and `IPU_ImageInfo`, called at `:247`). It is a second, unvalidated IPU decode path (no width/height guard of the kind `IPU_InitLoadImage:36` has) that nothing can reach. Its helper `_darken_image` is **not** dead (used by `IPU_LoadImage`) and must stay.
- **Fix:** Delete the function and its header declaration. Pure subtraction.
- **Benefit:** ~105 lines of unreachable decode path gone. **Effort:** S · **Risk:** Low · **Confidence:** High — READ (full-tree grep at the pin, re-verified at integration).

### SMS-076 — Dead SMS-logo remnants in GUIDesktop: a 408-byte table, a 768-byte stack array, dead stores
- **Category:** 1
- **Locations:** `src/SMS_GUIDesktop.c:44-57` (`s_LogoXY[51]`), `:117` (sole read), `:595-596` (dead `lX/lY` initialisers in `Desktop_Render`), `:604` + `:643` (`lBP[96]` and its `(void)` cast)
- **Problem:** Tree-wide, `s_LogoXY` is read exactly once — element `[50].m_Y`, the constant 4 — by `_Version_Render`; the other 50 entries and the whole `m_X` field are residue of the removed ball-logo watermark (removal noted at `:643-644`). `lBP[96]` is a 768-byte stack array that is never written or read, only `(void)`-cast. `Desktop_Render`'s initial `lX/lY` are overwritten from `lXYXY` before any use.
- **Fix:** Delete the table (inline the constant 4), delete `lBP` and the dead initialisers. Pure deletion.
- **Benefit:** ~1.2 KB and a misleading table gone. **Effort:** S · **Risk:** None · **Confidence:** High — READ.

### SMS-077 — The empty `_file_handler` assigned to every playlist-menu item does nothing and is not needed
- **Category:** 1
- **Locations:** `src/SMS_GUICmdProc.c:312-315` (empty body), `:373` (assignment)
- **Problem:** Selection in `_playlist_menu` works because `CtxMenu_HandleEvent` intercepts Cross/Circle, calls the base menu handler and quits; the chosen item is then read back from `lpState->m_pCurr` (`:388`). With `Handler == NULL`, `GUIMenu_HandleEvent` simply skips the call and the flow is unchanged — the empty function and its per-item assignment are dead weight, and misleading (it looks like the selection hook).
- **Fix:** Delete `_file_handler` and the assignment line.
- **Benefit:** Small, and removes a false affordance for the next reader. **Effort:** S · **Risk:** Low · **Confidence:** High — READ.

### SMS-078 — `_is_video_artwork` makes every directory fill quadratic: the full listing is rescanned for every JPEG
- **Category:** 5 (performance, structural)
- **Locations:** `src/SMS_GUIFileMenu.c:241-268` (inner scan `:257-266`), called from `_filter_item` at `:289`, per node from `_fill` at `:830-838` and the refill handler at `:1618-1626`
- **Problem:** For each `GUICON_PICTURE` node, `_is_video_artwork` walks the entire `g_pFileList` looking for a matching AVI/AVIS stem, building a candidate name (`SMS_ArtworkName`) and doing a case-insensitive compare per video entry. A fill of a directory with P pictures and V videos costs O(P×V) name builds/compares on the GUI thread (e.g. 2,000 JPEGs × 500 videos = 1M iterations with string work inside) — during ordinary navigation, in plain list mode, before any thumbnail is ever shown. Distinct from SMS-042 (FileDir probes + subtitle matching) and SMS-043 (synchronous JPEG decode; this scan involves no decoding and runs in list mode too).
- **Fix:** Invert the work inside `_fill`/refill: one pass collecting the video stems into a temporary list first, then test pictures against that prebuilt set. Same result set, one pass instead of one per picture; no persistent new structure.
- **Benefit:** Directory-open cost stops growing quadratically with folder size. **Effort:** M · **Risk:** Med (must preserve the exact case-insensitive `.jpg`/`.jpeg` sidecar semantics) · **Confidence:** High — READ (scan verified at the pin; magnitude is arithmetic, not benchmarked).

### SMS-079 — Decrement-by-one width fitting re-measures the whole string every step (quadratic on long names)
- **Category:** 5
- **Locations:** `src/SMS_GUIMenu.c:111`, `:156-157`; `src/SMS_GUIDesktop.c:823-824`, `:864-865`, `:939`; `src/SMS_PlayerControl.c:1058-1059` (plus a further full re-measure at `:1063`)
- **Problem:** `while (GSFont_WidthEx(str, lLen, dx) > width) --lLen;` — `WidthEx` walks all `lLen` chars per call, so fitting a 255-char file/device-supplied name down to ~40 visible chars costs ~200 full walks. The worst site is `PlayerControl_GSPacket`, which runs this per playlist node on every scroll-step repaint; the menu sites run it per visible item per redraw (i.e. per cursor move).
- **Fix:** Binary-search the length (`WidthEx` is monotonic in length), or accumulate per-char widths once. Same output, logarithmic/linear cost; no new structures.
- **Benefit:** Long-name redraws stop costing quadratic text measurement on the GUI/player threads. **Effort:** M · **Risk:** Low · **Confidence:** High — READ (cost magnitude is arithmetic, not benchmarked — no hardware in VM).

### SMS-080 — Every artwork-mode cursor step re-walks the filtered list ~5 times
- **Category:** 5
- **Locations:** `src/SMS_GUIFileMenu.c:108-124` (`_artwork_position`), `:126-132` (`_artwork_move`), renders re-invoke `_artwork_position` at `:420` and `:512`; entry `GUIFileMenu_HandleEvent` at `:1426-1428`
- **Problem:** One D-pad step in grid/poster mode performs: scan-to-current in `_artwork_move`, an `SMS_ListAt` walk to the new index, scan-to-current again plus another `SMS_ListAt` in `_artwork_position`, then `_redraw` → render → `_artwork_position` again (scan + `SMS_ListAt`) — ~5 linear walks of `m_pFiltList` per step, where plain list mode's `_step_down`/`_step_up` are O(1) pointer hops. On a multi-thousand-entry share this is per-keypress latency proportional to cursor depth. The cost exists even when all artwork is cached (pure list walking; not SMS-043).
- **Fix:** Have `_artwork_move` compute the new index once and pass it to `_artwork_position` (or have `_artwork_position` return the index it already computes) so move+position share one scan, and drop `_artwork_move`'s private duplicate scan. Deletion of redundant walks; the one added parameter replaces three scans with one shared result.
- **Benefit:** Artwork-mode navigation cost per step drops ~5×. **Effort:** M · **Risk:** Low · **Confidence:** High — READ.

### SMS-081 — DevMenu removal redraws every successor icon twice, once at a wrong offset; Render builds the selection arrow twice
- **Category:** 2/5
- **Locations:** `src/SMS_GUIDevMenu.c:396-408` (removal loop), `:198-206` (Render rebuild), `:460` (HandleMount returns `GUIDevMenu_Redraw`), `:180` vs `:209` (duplicate `GUIDevMenu_RenderSelRect`)
- **Problem:** On unmount, the loop shifts each successor `m_XOffset -= 55` and re-renders its icon packet — then `HandleMount` unconditionally returns `GUIDevMenu_Redraw`, whose Render recomputes every offset as `m_XOffset + 54 * idx` (`:198`) and re-renders every icon packet, discarding the loop's work entirely. The 55 also contradicts the 54 spacing used at both assignment sites (`:198`, `:333`), so the intermediate packets are not just wasted but wrong. Separately, first-build Render calls `RenderSelRect` at `:180` and again with identical arguments at `:209`; the second overwrites the first, and when the list is empty the `:180` guard (`m_pActive`) prevents it running at all — the first call is dead in all cases.
- **Fix:** Delete the removal loop's shift+`GUI_DrawIcon` (keep the list surgery) and delete the `:180` call. Pure deletion.
- **Benefit:** Unmount stops doing discarded, wrongly-spaced redraw work. **Effort:** S · **Risk:** Low · **Confidence:** High — READ (removal loop verified at the pin).

### SMS-082 — Selection-rectangle GS packet block copy-pasted in all three FileMenu renderers; filtered-list build loop duplicated verbatim
- **Category:** 2
- **Locations:** rect block `src/SMS_GUIFileMenu.c:458-467` (`_poster_render`), `:549-558` (`_artwork_render`), `:650-661` (`GUIFileMenu_Render`); filter build `:830-838` (`_fill`) vs `:1618-1626` (refill handler)
- **Problem:** Five copies of two fragile idioms. The rect block depends on the `lpDMA-2`/`SelRect-2` packet-header convention; a fix to alpha, offset or the VIF tag must currently be made three times identically (the copies already differ in the alpha constant `0x18` vs `0x10` and geometry, making drift invisible). The filter loop is the single definition of "what the browser shows" and currently lives in two places.
- **Fix:** Extract the rect sequence into one small static helper taking (x, y, w, h) and the loop into one `_rebuild_filtlist` static used by both callers; net deletion of ~30 lines, no new state.
- **Benefit:** One place to fix packet-header and filtering behaviour. **Effort:** M · **Risk:** Med (packet-header offsets are easy to break; needs a visual check of all three modes) · **Confidence:** High — READ.

### SMS-083 — Palette-rotate logic exists in four copies (two helpers + two inline)
- **Category:** 2
- **Locations:** `src/SMS_GUIMenuSMS.c:2190-2205` (`_rotate_palette`), `:2348-2362` (`_rotate_palette_2`), inline copies `:2219-2239` and `:2241-2258` (shares lines with SMS-057)
- **Problem:** The wrap arithmetic is character-identical in all four; only pointer signedness and the post-redraw differ. This is exactly how SMS-057's off-by-one survived in four places: there is no single site to fix. The browser/player split is not semantic — player indices are the same 1..16 palette domain stored in `unsigned int` config fields (the `_2` callers cast `unsigned int*` to `int*` at `:2366-2396`).
- **Fix:** One `_rotate_palette` taking the value pointer plus a redraw-mode flag (or a shared `_wrap_palette_idx` with the one-line redraw left at each handler); delete `_rotate_palette_2` and the two inline copies (~35 lines removed, one 6-line helper added — justified because it collapses four copies of the same arithmetic into one).
- **Benefit:** The palette wrap has exactly one home. **Effort:** S · **Risk:** Low · **Confidence:** High — READ.

### SMS-084 — `DrawSkin` triplicates the same size-check / alloc / pad / read block
- **Category:** 2
- **Locations:** `src/SMS_GUIDesktop.c:155-255`; identical validation core at `:176`, `:206`, `:227`
- **Problem:** Three copies of one algorithm (FS-boot fio, memory-card fio under BDM, synchronous MC wrappers otherwise) differing only in the open/seek/read/close primitives: `lSize >= 8 && lSize <= 0xFFFF0`, align-up allocation, pad `memset`, short-read → free/NULL. The file-size bound is device/file-supplied data, so drift between copies is a safety issue, not just style — and this function's history (per its own comments) is a series of bound fixes that each had to be made three times.
- **Fix:** One static helper holding validation+alloc+read, parameterised by the three primitives. Net deletion of ~50 lines.
- **Benefit:** Skin-load bounds get fixed once, everywhere. **Effort:** M · **Risk:** Med (three boot paths must keep identical behaviour) · **Confidence:** High — READ.

### SMS-085 — JPEG decode→expand→upload pipeline duplicated between the jellyfish background and the boot splash
- **Category:** 2
- **Locations:** expand loop `src/SMS_GUIDesktop.c:349-366` (`_DecodeJellyfish`) vs `:485-501` (`_DrawSplash`); banded 32-row upload loop `:398-416` vs `:512-532`; plus repeated TBW/`m_VRAMTexPtr`/`PowerOf2` blocks in both
- **Problem:** The 24-bit→RGBA expansion and the QWC-safe banded GS upload are verbatim copies; the splash version's own comment says "Same decode+upload+blit path as the jellyfish background." A fix to either (band size, VRAM placement, cache sync) silently diverges.
- **Fix:** Extract one `_expand_and_upload(lpTex, lW, lH)` helper used by both; the splash keeps only its fade loops, the background keeps its cache.
- **Benefit:** One upload path to keep correct. **Effort:** M · **Risk:** Low · **Confidence:** High — READ.

### SMS-086 — USB connect/disconnect SIF handlers are mirror copies
- **Category:** 2
- **Locations:** `src/SMS_GUI.c:255-274` and `:276-295`, registered at `:979-980`
- **Problem:** Both handlers contain the same unit→mask switch and the same "clear opposite flag else set mine" toggle, with connect/disconnect roles swapped. ~40 lines where ~20 suffice; a mask change must be edited twice, symmetrically, by hand.
- **Fix:** One handler taking a connect/disconnect flag (the two registrations become one-line wrappers or a parameterised registration).
- **Benefit:** Half the handler to keep right. **Effort:** S · **Risk:** Low · **Confidence:** High — READ.

### SMS-087 — `GUIMenu_SelectItemByName` and `GUIMenu_SelectItem` duplicate the whole walk
- **Category:** 2
- **Locations:** `src/SMS_GUIMenu.c:236-260` and `:263-281`
- **Problem:** Identical reset (`m_pFirst = m_pCurr = m_pItems`), `_update_state`, scroll-adjust (`m_pCurr + 1 == m_pLastV → ++m_pFirst`) and advance loop; only the stop predicate differs (name `strcmp` vs counter) — the second's own comment says "Same walk by position." Any fix to the scroll-adjust logic must be mirrored manually.
- **Fix:** One walker with a stop parameter (target name or remaining count; NULL name + count −1 covers both). Net deletion ~15 lines.
- **Benefit:** Menu selection/scroll logic in one place. **Effort:** S · **Risk:** Low–Med (used by every menu; behaviour identical by construction) · **Confidence:** High — READ.

### SMS-088 — `g_ASCII` size mismatch: the extern declares 4572, the definition is 5416, so `GSFont_Get(0)` under-reports by 844 bytes
- **Category:** 6
- **Locations:** `src/SMS_GS_2.c:35` (extern) vs `src/SMS_GSFont.c:11` (definition); consumed at `src/SMS_GS_2.c:50-53` (`s_FontSizes[0] = sizeof(g_ASCII)` = 4572 in this TU) and `:502-508` (`GSFont_Get`); sole consumer `src/SMS_Config.c:292-304`
- **Problem:** The other four font externs (`src/SMS_GS_2.c:36-39`) match their definitions exactly — only ASCII drifted. The consumer (`_load_font`) uses the reported size as the capacity threshold for writing a replacement `.mtf` in place into the embedded font buffer: `lSize > lFontSize` → malloc, else `fioRead` directly over `g_ASCII`. With the threshold understated by 844 bytes, fonts of 4573–5416 bytes take an unnecessary malloc-and-repoint path, and any future consumer trusting `GSFont_Get`'s size under-copies the ASCII font by 844 bytes.
- **Fix:** Delete the stale `4572` — declare `g_ASCII[]` unsized in GS_2.c so `sizeof` tracks the definition. One-token change, no new code.
- **Benefit:** The font-size contract becomes true by construction. **Effort:** S · **Risk:** Low · **Confidence:** High — READ (both sizes and the consumer's threshold verified at the pin).

### SMS-089 — `SMS_ArtworkJPEGSize` treats standalone JPEG markers as length-bearing segments
**Verification note (2026-11-14, full verification): WITHDRAWN — REFUTED on adversarial re-derivation: the JPEG preflight walk returns at SOF0 (SMS_Artwork.c:64-68) or SOS (:60) and never reaches entropy data, where RST/TEM markers only exist; DRI (0xDD) is length-bearing and walked correctly — valid restart-interval JPEGs are not rejected; see Sable-VERIFY2-SMS.md. Do not act on this finding.**
- **Category:** 6
- **Locations:** `src/SMS_Artwork.c:54-71`
- **Problem:** The preflight loop assumes every marker after SOI carries a 2-byte length (`:61-63`, `:71`). Restart markers RST0–RST7 (0xD0–0xD7) and TEM (0x01) have no length field and no payload. A baseline JPEG written with a restart interval (DRI) therefore desynchronises the walk: the first entropy bytes are read as a length, `lPos += lLength` skips into entropy data, and the walk almost always returns 0 — the preflight rejects a valid, decoder-supported file and the artwork silently never loads. All other arithmetic in the function was verified sound (length includes its own 2 bytes; SOF0 field offsets at `:65-67` are correct; `lLength > aSize - lPos` bounds every read).
- **Fix:** In the marker dispatch, `if ((lMarker >= 0xD0 && lMarker <= 0xD7) || lMarker == 0x01) continue;` before the length read — two clauses, no restructure.
- **Benefit:** Restart-interval JPEGs stop being silently rejected as artwork. **Effort:** S · **Risk:** Low · **Confidence:** mechanism READ (walk verified at the pin); prevalence INFERRED (depends on the encoder having used restart markers).

### SMS-090 — Subtitle `_reset` writes `m_pPackets[m_Idx]` with no NULL/range guard
- **Category:** 6
- **Locations:** `src/SMS_SubtitleContext.c:917-922`
- **Problem:** `_reset` unconditionally does `s_SubCtx.m_pPackets[s_SubCtx.m_Idx].m_Pad[0] |= 0x80000000`. Its neighbours guard: `_display` (`:870ff`) loops only `while (m_Idx < m_Cnt)` and `_destroy` (`:897ff`) NULL-checks `m_pPackets`. But `_display` itself can leave `m_Idx == m_Cnt` after the last packet expires, and if `Prepare` was never run (load produced no packets) `m_pPackets` is NULL — a seek/reset in either state writes through NULL or one packet past the calloc'd array.
- **Fix:** One guard mirroring `_display`: `if (s_SubCtx.m_pPackets && s_SubCtx.m_Idx < s_SubCtx.m_Cnt) …`.
- **Benefit:** Subtitle seek/reset stops writing outside the packet array in the two unguarded states. **Effort:** S · **Risk:** Low · **Confidence:** INFERRED (in-file guard asymmetry is READ; reachability of Reset in the bad state depends on the player seek caller, outside the rendering files — SMS_Player.c, read this round, calls the subtitle context through its vtable without adding a guard).

### SMS-091 — `GUIMenu_ShowItem` hide-walks have no array bounds
- **Category:** 6
- **Locations:** `src/SMS_GUIMenu.c:362-375`
- **Problem:** Hiding does `while ((--m_pFirst)->m_Type & HIDDEN);` with no `m_pItems` floor, and `while ((++m_pLastV)->m_Type & HIDDEN);` with no `m_pLast` ceiling; `aPos` itself is also unchecked before `&m_pItems[aPos]`. The sole in-tree caller passes a literal 4 (`src/SMS_GUIMenuSMS.c:2342`), so triggering needs `m_pFirst == &items[4]` (menu scrolled) with every item below it hidden, or the symmetric `m_pLastV` case — then the walk reads out of the items array and keeps going on garbage flag bits.
- **Fix:** Bound both walks (`m_pFirst > m_pItems`, `m_pLastV < m_pLast`) — two condition edits.
- **Benefit:** A latent out-of-bounds walk in the generic menu becomes bounded. **Effort:** S · **Risk:** Low · **Confidence:** Low — INFERRED (no reachable all-hidden prefix verified in the caller's menu).

### SMS-092 — `_smb_logon` copies credentials with the destination's bound, not the source fields'
- **Category:** 6 (latent)
- **Locations:** `src/SMS_GUIDevMenu.c:575-576` (`strncpy(..., 255)`); sources `include/SMS_SMB.h:90-91` (`m_UserName[32]`, `m_Password[64]`); destinations `User[256]`/`Password[256]` at `include/SMS_SMB.h:57-60`
- **Problem:** The write side is safe (255 < 256, struct pre-zeroed at `:570`). The read side is correct only by producer discipline: `strncpy` scans the source for up to 255 bytes, so a producer that ever fills a 32/64-byte field without a NUL makes the logon packet sent to the IOP contain the following struct fields (password bytes in the User field, share name in the Password field). The config-file producer is safe today — `File_GetString` always NUL-terminates inside the field — but the bound expresses the wrong contract. Not a re-file: round 2's three classified GUIDevMenu sites are `sprintf:292`, `strcpy:310`, `sprintf:643`; `strncpy` was not among them. (SMS-035's plaintext-storage point stays settled and is not re-opened by this finding, which is about a copy bound, not storage.)
- **Fix:** Change the two bounds to `sizeof(lpInfo->m_UserName)` / `sizeof(lpInfo->m_Password)`. No behaviour change for any current producer.
- **Benefit:** The copy contract becomes true regardless of producer. **Effort:** S · **Risk:** Low · **Confidence:** Medium — INFERRED (latent; no live producer verified to break it).

---

## Checked and clean (this round)

**GUI menus (SMS_GUIMenuSMS.c, SMS_GUIFileMenu.c, SMS_GUIcons.c):** extension trim in `GUIFileMenu_RenderItem` (`:325-327`) cannot underflow (SMS_FileDir assigns types only when the dot sits at −4/−5); `_poster_names` (`:356-386`) length-clamped to 319 with bounded second line; `_artwork_load` path construction length-checked against the 1024-byte path, `memalign` checked, source capped by `SMS_ARTWORK_MAX_FILE` (decode mechanism itself is SMS-043); `_push_state`/`_pop_state` (`:782-811`) fully checked — the file's own good idiom; `_make_path` (`:860-874`) capacity-checked before every copy; `_action_partition` VLA sized `strlen(name)+6` fits `"hdd0:"` exactly; empty `_context_action_part`/`_context_action_share` are load-bearing no-op entries in a table dispatched without a NULL check — not dead; `_device_handler` dynamic Start rows counted: worst case index 24 of 28 (BDM) / 11 of 12 (non-BDM) — no overflow; skin/MBF list cycling is safe because config init seeds a sentinel node; `GUIMenuSMS_UpdateStatus`'s 1024-byte buffer sees only fixed locale strings at depth ≤ 3 (INFERRED safe); the other wrap handlers (`_tvsys_handler`, `_cdvd_spd_handler`, `_roll_ip_number`, `_roll_mask_number`, `_opmode_handler`, `_mp3_adp_handler`) verified correct — the palette handlers of SMS-057 are the exception; `GUI_DrawIcon` index ranges fit their slot counts for all FileDir-assigned types; `_saveipc_handler`'s 64-byte buffer fits the roll handlers' worst case (48 + NUL); all round-2 classified string sites in these files stand — no mis-classification found.

**GUI framework / player control (SMS_GUIMenu.c, SMS_GUIDevMenu.c, SMS_GUICmdProc.c, SMS_PlayerControl.c, SMS_GUI.c, SMS_GUIDesktop.c, SMS_GUIFileCtxMenu.c, SMS_GUIMiniBrowser.c, SMS_GUIClock.c, SMS_PlayerMenu.c):** CmdProc mask dispatch (`0x80000000`/`0xC0000000`) is the intentional negative-pointer playlist encoding — `SMS_GetContainer` guards with `(int)apFileCtx > 0`; `_playlist_menu` zero-count is unreachable (its sole source gates on `lnMedia > 0`); clock capture size math `(2 + (PSM == PSMCT24))` is correct for both reachable framebuffer formats (GS_1 selects only PSMCT16/24); PlayerMenu index arithmetic worst case writes index 10 of the 11-entry table and `m_pLast` matches; menu hidden-skip navigation (GUIMenu.c:296-345) is bounds-safe (outer guards plus in-loop checks precede every deref — contrast SMS-091's ShowItem walks); scrollbar geometry divisor is `m_ScrollBarNum + 1 ≥ 33` by construction of its wrap cycle — no zero-divide; `SMS_SyncMalloc` call sites inherit that helper's own failure handling (`src/SMS_EE.c`); empty CmdProc Render/Cleanup stubs and the empty `_printf_handler` are load-bearing dispatch targets, not dead code (contrast SMS-077, where the handler is genuinely unneeded); `GUI_Progress`'s length-keyed cache is safe because its only variable-text caller forces update; playlist packet sizing `PC_GSP_SIZE(n) = (n<<2)+6` bounds `GSPacket`'s writes; the OSD timer cycle folds the unknown-duration case back to OSD 0 explicitly; the GUI focus-change walk terminates because the SetFocus objects are always registered.

**Rendering (SMS_GSFont.c, SMS_GS_1.c, SMS_GS_2.c, SMS_IPU_1.c, SMS_Artwork.c, SMS_PgInd.c, SMS_PlayerBallSim.c, SMS_SubtitleContext.c):** SMS_GSFont.c contains **zero code** — five font-data arrays only (the GSFont *logic* lives in SMS_GS_2.c); four of five array sizes match their GS_2.c externs exactly (ASCII is SMS-088); `GSFont_Load` (`:510-581`) checks file size, GUID and LZMA uncompressed size before the malloc, and the malloc at `:571` before decompressing — the file's model citizen; `GSFont_Get/Set` have no index check but their only caller loops 0–4; `GSFont_Width/Render` families carry the documented signedness fix, verified correct; `GSFont_Init` glyph uploads exactly fill the 512×448 region of the font texture; `GS_RenderRoundRect` writes exactly the 36-u64 packet in both paths; `GSContext_Init`'s LWidth alignment loop terminates for both pixel sizes and its DBuf free/realloc bookkeeping is orderly (the alloc *check* is SMS-066); `GS_StoreImage` remainder/QWC arithmetic is self-consistent for all three PSMs; `IPU_InitLoadImage`'s new guard (width multiple of 16, 16–1024) makes packet count, DMA size and the trailing EOP/REFE patch agree exactly, malloc checked; `IPU_LoadImage`'s TRXPOS rewrite loop writes exactly width/16 slots, matching Init's layout; PgInd's QWC factor equals bytes-per-pixel for both reachable formats, its 16 KiB bitmap covers the reachable grab, and its packet rewrite slots match the REGLIST layout; PlayerBallSim has no division by zero (`m_Z` reset to 100 before use, floor 2), its balls array ends exactly at `g_BallPkt` and the packet fits the 70,816 B arena exactly; Artwork thumbnail/poster index math is bounded by construction and `SMS_ArtworkMove` handles an INT_MIN delta correctly; SubtitleContext's `_load_sub` `lpPtr == (char*)1` checks are **not** dead (each `sscanf` format guarantees the `strchr` targets exist — verified per format); `_update_size`'s per-glyph budget (640 B) is exactly the renderers' 80 u64/glyph and `_mbstrlen` counts glyphs, matching the MB renderer; `_add_line`'s truncation loops strictly shrink and terminate; `_create_gs_packet`'s header offsets land on the correct slots; `_load_srt`'s colour-tag strip is bounds-checked.

**Remainders (first-hand):** `src/main.c` (105 lines) is a clean entry shim. `src/SMS_MC.c` (317) is entirely `#ifndef BDM` — the legacy libmc RPC client; its two string sites were round-2 class B, and its legacy-path status is already SMS-009/SMS-037. `src/SMS_PAD.c` (492): the BDM half is a carefully maintained libpad wrapper with a documented SIO2 self-heal and a semaphore-serialised DS34 poller; the `#else` half is the legacy padman RPC client (BDM=0 builds only, SMS-009 territory) — no new finding. `src/SMS_RC.c` (583): logic is the remote-control loader/RPC wrappers around the embedded RMMAN array (data; SMS-010 territory); string sites round-2 class B. `src/SMS_Sounds.c` (4,447) contains **no functions at all** — `g_SMSound[4]` descriptors plus the 70,816-byte `g_SMSounds` bank whose header overlay map (g_CWD at offset 512, packet scratch areas) is the established scratch-RAM design. `src/SMS_EE.c` (979): SMS_Rescale's 64-bit path, SMS_ReverseString, SMS_EEScanDir's `<68` guard (round-2), SMS_InitBitBlt (its 16-slot concern does not arise — strip count and remainder fix agree), and the TLB asm block (inventory) were read; the one defect is SMS-060. `src/SMS_Player.c` (1,899) read in full: the dummy renderer's 16-word DMA tag array exactly fits its worst case (8 tags × 2 words — counted); the pulldown-2:2 reduction is guarded by an exact timebase-equality test; thread stacks are the documented OSD-RAM reuse (`g_OS` offsets in non-EMBEDDED builds); all previously cited anchors (SMS-013 rings `:1741-1777`, SMS-028 HistorySave `:1559`, SMS-038 renderer `:416-486`, SMS-041 buffers, SMS-053 gate `:1726-1731`) confirmed in place, none re-filed. `src/SMS_FileContext.c` (2,208) read in full: the BDM/STIO half is the most heavily hardened code in the repo (documented MX4SIO/USB chunking, the USB prefetch thread with a correct stop/resync protocol, `File_AllocStreamBuffers`' overflow guard, `File_Skip`'s seek fast-path and EOF stop, `File_GetString`'s failure-as-EOF handling) — no new finding there; the CDDA half produced SMS-062/063/071/072.

## Noted, not promoted

- `SMS_RC.c` `_load_rmman` (~`:113-125`): `malloc(lSize)` unchecked before `fioRead` + module exec. Same family as SMS-045, and the input is a trusted local IOP module rather than file-supplied media data — recorded here, not numbered, per round 4's precedent for this family.
- `SMS_CDDA.c` / CDDA sector accounting beyond SMS-072's single mechanism: the streaming fill's sector bookkeeping was read and no second defect was isolated; not promoted on suspicion.
- `SMS_Player.c` `_setup_video_pulldown22`'s `a % 5` reduction loop: examined; the subsequent GCD normalisation makes the result correct for the guarded inputs (the function only proceeds on an exact timebase-equality test). Not a defect on the evidence read.
- GSFont `GSFont_Width` has one dead first-char store; PlayerBallSim's `g_IconBall` extern is a dead declaration (its data-side fix is inside SMS-074). Below filing threshold individually.

## Coverage ledger (round 5, exact)

| File | Read | Notes |
|---|---|---|
| src/SMS_GUIMenuSMS.c | 1–3052 (EOF) | full |
| src/SMS_GUIFileMenu.c | 1–1662 (EOF) | full |
| src/SMS_GUIcons.c | 1–1546 (EOF) | full; :36–1334 is icon byte data, reference-verified by grep |
| src/SMS_GUIMenu.c | 1–414 (EOF) | full |
| src/SMS_GUIDevMenu.c | 1–690 (EOF) | full |
| src/SMS_GUICmdProc.c | 1–405 (EOF) | full |
| src/SMS_PlayerControl.c | 1–1467 (EOF) | full |
| src/SMS_GUI.c | 1–1054 (EOF) | full |
| src/SMS_GUIDesktop.c | 1–992 (EOF) | full |
| src/SMS_GUIFileCtxMenu.c | 1–540 (EOF) | full |
| src/SMS_GUIMiniBrowser.c | 1–198 (EOF) | full |
| src/SMS_GUIClock.c | 1–293 (EOF) | full |
| src/SMS_PlayerMenu.c | 1–349 (EOF) | full |
| src/SMS_GSFont.c | 1–2297 (EOF) | data only — no logic exists in this file |
| src/SMS_GS_1.c | 1–717 (EOF) | full |
| src/SMS_GS_2.c | 1–590 (EOF) | full |
| src/SMS_IPU_1.c | 1–324 (EOF) | full |
| src/SMS_Artwork.c | 1–141 (EOF) | full |
| src/SMS_PgInd.c | 1–290 (EOF) | full |
| src/SMS_PlayerBallSim.c | 1–123 (EOF) | full |
| src/SMS_SubtitleContext.c | 1–1001 (EOF) | font path :45–209 was round-2 territory (re-read for data flow); remainder :1–44, :210–1001 fresh |
| src/SMS_FileContext.c | 1–2208 (EOF) | full (first-hand) |
| src/SMS_EE.c | 1–979 (EOF) | full (first-hand); embedded `s_Loader` blob is data |
| src/SMS_Player.c | 1–1899 (EOF) | full (first-hand) |
| src/SMS_MC.c | 1–317 (EOF) | full; entirely `#ifndef BDM` |
| src/SMS_PAD.c | 1–492 (EOF) | full |
| src/SMS_RC.c | 1–583 (EOF) | full; embedded RMMAN array is data |
| src/main.c | 1–105 (EOF) | full |
| src/SMS_Sounds.c | 1–4447 (EOF) | verified data-only (no functions exist) |

## Formal verdict (Task 4)

**Coverage by area, all rounds:**

| Area | State |
|---|---|
| EE — GUI (menus, framework, desktop, device/file/context menus, mini-browser, clock, icons) | **Read in full, round 5.** |
| EE — rendering (GS font/GS/IPU glue, artwork, progress indicator, ball sim, subtitle context) | **Read in full, round 5** (GSFont.c is data; its logic lives in GS_2.c, also full). |
| EE — player core (SMS_Player.c, SMS_PlayerControl.c, SMS_PlayerMenu.c) | **Read in full, round 5.** |
| EE — file backends (SMS_FileContext.c STIO + CDDA + prefetch) | **Read in full, round 5.** |
| EE — platform (SMS_EE.c, SMS_MC.c, SMS_PAD.c, SMS_RC.c, main.c, SMS_IOP.c, SMS_CDDA.c, SMS_CDVD.c, SMS_SIF.c, SMS_VSync.c, SMS_DMA/VIF glue) | Read in full across rounds 3–5. |
| EE — config/files/lists (SMS_Config.c, SMS_FileDir.c, SMS_List.c, SMS_DirTree.c, SMS_CopyTree.c) | Read in full, round 2. |
| EE — media plumbing (SMS_JPEG.c, SMS_Rescale.c, SMS_MPEGInit.c, SMS_IPU.c, SMS_SPU.c, SMS_FileMapping.c, SMS_RingBuffer.c, SMS_VLC.c, SMS_PCM.c, SMS_Codec.c, SMS_VideoBuffer.c frame-buffer init) | Read in full, round 4 (SMS_JPEG.c cleared as the most validated parser in the repo). |
| EE — containers (ASF/AVI/MOV/MP3/JPG/M3U/AAC/AC3/OGG demuxers, MPEG-PS) | Read in full, rounds 3–4. |
| EE — residue (see below) | **SMS_Container.c (393), SMS_Locale.c (965), SMS_History.c (200), SMS_Spectrum.c (132), SMS_VideoBuffer.c beyond frame-buffer init (384)** — read in cited sections across rounds 2–4 (every defect-bearing site found there is filed: SMS-013/027/028/038/047/052/053 touch these files), but not read line-by-line end-to-end. **SMS_DSP.c DSP kernels :879–2166** — structure-inventoried round 4 (function tables + framing), kernels not line-audited. |
| EE — data, not logic (SMS_Data.c, SMS_OS.c, SMS_Sounds.c, SMS_Menu.c — 7 lines, icon/RGBA/JPEG-data arrays, About.c) | Verified as data; nothing to line-audit. |
| Codecs (DTS, FAAD2/AAC, mpg123 core glue, libvorbis, AC3, WMA, FLAC, H263, MPEG family) | **Inventory by framing** (all rounds): provenance verified as unmodified upstream snapshots (import `d704063` + mechanical commits); allocation verdicts delivered round 3; decode internals deliberately not line-audited. SMS_MPEG.c's unchecked picture callocs were sized and proven non-inflatable round 4. |
| IOP — locally owned (smbman-sms, SMSUdpfs local mods, SMSCDVD) | Line-audited rounds 2–3: smbman-sms clean (best-hardened code in the repo), udpfs mods clean, SMSCDVD one residual (SMS-049). |
| IOP — dead (SMSSMB tree) | SMS-036; built by nothing. |
| IOP — pristine upstream inventory (SMSTCPIP, SMSMAP, SMSUMS, SMSPWROFF, USBD, SMSDev9 as ABI reference) | Inventory by framing; not line-audited, by design. |
| Assembly (SMS_DSP_MMI.S, SMS_DSP_FFT.S, SMS_DSP_QPel.S, SMS_MP123Core.S, SMS_GS_0.S, SMS_IPU_0.s, SMS_RC_0.s, SMS_DMA_0.s, SMS_Timer.s, SMS_Utils.S, libmpeg_core.S, About_Data.s; inline asm inside SMS_EE.c/SMS_GS_1.c/SMS_PgInd.c) | **Inventory by name/framing** — named here as such, not counted as read and not counted as a gap. |
| Host tooling (tools/, sms-converter) | SMS-019/020; out of the EE/IOP frame. |

**Verdict:** Simple-Media-System is **not** declared exhausted at source level — but round 5 closes the last large unread region. Everything round 4 left open by name — the whole GUI family, the whole rendering family, the player core, the file backends, and the platform remainders (EE/MC/PAD/RC/main/Sounds) — has now been read end-to-end at the pin, and the round's yield (36 findings, including a user-reachable palette out-of-bounds read, a decompressor guard that decompresses into address 0 on its own failure path, ≈19 KB of dead icon data, and a quadratic directory fill) shows the region was worth the read. What remains unread at line level is exact and small: **SMS_Container.c (393 lines), SMS_Locale.c (965), SMS_History.c (200), SMS_Spectrum.c (132) and SMS_VideoBuffer.c past its frame-buffer init (384)** — 2,074 lines of EE glue that every round has sampled at its defect-bearing sites without a full pass — plus the **SMS_DSP.c kernels (:879–2166)**, which are structure-inventoried only. Beyond those, the audit's frame deliberately stops at inventory for three named classes, which are **not gaps**: the vendored codec decode internals (provenance- and allocation-audited, rounds 3–4), the pristine-upstream IOP trees, and all assembly, which is inventoried by name above. No PS2 build or hardware verification exists in this environment; every performance magnitude in this audit is structural, and every Category 5/6 consequence that depends on runtime state is labelled INFERRED where it appears.
