# REDO — Simple-Media-System — W4: EE Player, GUI, and Support Code

Pin: `e65ced39a27eee8d996e9257e21594d1cac1c93d` (clone `~/workspace/audit-clones-2026-11-13/Simple-Media-System`, read-only).
Blind redo: no prior audit material was read.

## (a) Coverage note

Files read in full (47 source files, 33,857 lines by `wc -l`):

`src/SMS_Player.c` (1,899), `src/SMS_PlayerControl.c` (1,467), `src/SMS_PlayerMenu.c` (349), `src/SMS_PlayerBallSim.c` (123), `src/SMS_GUI.c` (1,054), `src/SMS_GUIClock.c` (293), `src/SMS_GUICmdProc.c` (405), `src/SMS_GUIDesktop.c` (992), `src/SMS_GUIDevMenu.c` (690), `src/SMS_GUIFileCtxMenu.c` (540), `src/SMS_GUIFileMenu.c` (1,662), `src/SMS_GUIMenu.c` (414), `src/SMS_GUIMenuSMS.c` (3,052), `src/SMS_GUIMiniBrowser.c` (198), `src/SMS_GUISMBrowser.c` (875), `src/SMS_GUIcons.c` (1,546), `src/SMS_GUInfoPanel.c` (163), `src/SMS_Menu.c` (7), `src/SMS_SubtitleContext.c` (1,001), `src/SMS_Artwork.c` (141), `src/SMS_History.c` (200), `src/SMS_Config.c` (1,132), `src/SMS_FileContext.c` (2,208), `src/SMS_FileDir.c` (668), `src/SMS_FileMapping.c` (337), `src/SMS_List.c` (337), `src/SMS_RingBuffer.c` (224), `src/SMS_MC.c` (317), `src/SMS_CDDA.c` (344), `src/SMS_CDVD.c` (132), `src/SMS_Spectrum.c` (132), `src/SMS_DSP.c` (2,166), `src/SMS_PCM.c` (95), `src/SMS_SPU.c` (330), `src/SMS_SIF.c` (54), `src/SMS_IOP.c` (1,880), `src/SMS_EE.c` (979), `src/SMS_OS.c` (1,980), `src/SMS_PAD.c` (492), `src/SMS_RC.c` (583), `src/SMS_DateTime.c` (401), `src/SMS_Locale.c` (965), `src/SMS_DirTree.c` (266), `src/SMS_CopyTree.c` (387), `src/SMS_Codec.c` (168), `src/main.c` (105), `src/mbstring.c` (104).

Headers read (36 files, 3,265 lines): the matching `include/SMS*.h` for the above plus `include/SMS.h` and `include/mbstring.h` — 37,122 lines total in scope (counts by `wc -l` only).

Method: full reads plus scripted counts for repeated mechanisms (e.g. `SMS_ListPushBack (…) -> m_Param` dereferences: 18 by grep; `g_Palette[… - 1]` uses: 26 by grep). No build or hardware run — every claim below is code-read only.

---

## (b) Candidates (sorted by benefit)

### CAND-W4-1 — Raw config blob is trusted as array indices with almost no validation
Category: 6 (bugs/security)
Locations: `src/SMS_Config.c:696-702` and `:727-732` (load: 4-byte version check, then 892 raw bytes into `g_Config`); the only post-load guards are `src/SMS_Config.c:773` (`m_SkinName` last byte NUL) and `:775` (`m_PlayerAC3RL` clamp). Consumers: `src/SMS_CDVD.c:126` (`sl_Speeds[g_Config.m_CDVDSpeed]`, table of 3), `src/SMS_GUIMenuSMS.c:619` and `:1806` (`s_Speeds[g_Config.m_CDVDSpeed]`), and 26 script-counted `g_Palette[g_Config.m_*Idx - 1]` reads, e.g. `src/SMS_GUI.c:674-675`, `src/SMS_Player.c:144-147`, `src/SMS_PlayerControl.c:205`, `src/SMS_Spectrum.c:51`.
Problem: A corrupt or crafted `SMS.cfg` (memory card / next to the ELF — user-copyable files) sets the unsigned colour indices and `m_CDVDSpeed` arbitrarily. Index `0` reads `g_Palette[-1]`; a large index reads far out of bounds; `m_CDVDSpeed > 2` indexes past a 3-entry table. The same unvalidated blob also supplies display-mode/geometry fields. This is uninitialised-data/OOB consequence from an external file, not a theoretical style issue.
Fix: In `SMS_LoadConfig`, after a successful read, clamp/validate the whole field set in one place: colour indices to `1..17`, `m_CDVDSpeed` to `0..2`, display mode and scroll-bar fields to their enum ranges — resetting an out-of-range field to its default, exactly as `m_PlayerAC3RL` already is. No new config version, no new fields.
Benefit: Removes a whole OOB class reachable from a single user-supplied file, at the one choke point every boot passes through.
Effort: Small. Risk: Low (defaults already exist in the same function). Confidence: READ.

### CAND-W4-2 — `sprintf` into the 64-byte `g_SaveDiag` with a 128-byte path overflows it
Category: 6 (bugs/security)
Locations: `src/SMS_Config.c:69` (`char g_SaveDiag[64]`), `:943` (`sprintf(g_SaveDiag, "Save: CWD write %d %s", …, s_pMC0SMC)`), `:945` (same for open), `:1082-1087` (`"Save: mc open %d %s"` with `s_pMC0SMC`); `s_pMC0SMC` is `char[128]` at `src/SMS_Config.c:64`. The buffer and its messages are self-described TEMP diagnostics at `src/SMS_Config.c:66-68`, `:883`, and consumed at `src/SMS_GUIMenuSMS.c:1007`.
Problem: `"Save: CWD open -1 "` (19 chars) plus a config path of up to 127 chars is up to ~146 bytes written into a 64-byte global — a straightforward overflow on exactly the error path (failed save) the diagnostic exists for. The same TEMP pattern appears for IPCONFIG in `src/SMS_GUIMenuSMS.c:2031-2090` into `char lDiag[48]` (there the format carries no path, so it fits — checked).
Fix: Either delete the TEMP diagnostic (preferred under LESS, NOT MORE — it was added to chase a save bug) or switch the three path-carrying sites to `snprintf(g_SaveDiag, sizeof(g_SaveDiag), …)`. Do not enlarge the buffer and keep unbounded `sprintf` elsewhere.
Benefit: Removes a guaranteed overflow on a routine failure path; deleting it also removes a global and its extern.
Effort: Small. Risk: Low. Confidence: READ.

### CAND-W4-3 — History load: unbounded, unchecked, and triplicated record loop
Category: 6 (bugs/security)
Locations: `src/SMS_History.c:41-52` (CWD/fio loop), `:78-89` (BDM mc/fio loop — near-identical), `:97-108` (non-BDM `MC_ReadS` loop — third copy); cap `HIST_SIZE 32` at `:20` is enforced only on add (`:141`), never on load.
Problem: Each loop reads a file-supplied `unsigned short lSize`, allocates `lSize + 1` via `SMS_ListPushBackBuf`, and ignores the return values of the payload and `m_Param` reads (`fioRead(lFD, _STR(lpNode), lSize)` / `fioRead(lFD, &lpNode->m_Param, 8)`). Nothing limits the number of records: a crafted `SMS.hst` grows the list until memory is exhausted (each record also costs a list node), and `SMS_ListPushBackBuf` itself does not check its `calloc` (CAND-W4-4), so exhaustion becomes a NULL dereference mid-load. Short/truncated reads are silently accepted as entries.
Fix: Collapse the three loops into one loop over a small read-callback (or load once via fio in both BDM and non-BDM paths, as the neighbouring palette/locale code already migrated to fio), stop after `HIST_SIZE` records, require both payload and param reads to return in full, and stop on a failed allocation.
Justification (if ADDS): The callback/helper replaces three copies with one — net deletion.
Benefit: Bounds a user-file-driven allocation loop, removes ~40 duplicated lines, and makes truncated history files stop instead of inventing entries.
Effort: Medium. Risk: Medium (three device paths; keep the mc fallback behaviour identical). Confidence: READ.

### CAND-W4-4 — List allocation contract is inconsistent: `Push`/`PushBackBuf` unchecked, 18 call sites dereference the result
Category: 6 (bugs/security)
Locations: `src/SMS_List.c:17-21` (`SMS_ListPush`: `calloc` result `strcpy`'d with no NULL check), `:57-71` (`SMS_ListPushBackBuf`: `calloc` result linked and dereferenced with no NULL check — contrast `SMS_ListPushBack` at `:36-40`, which does check), `:319-325` (`SMS_ListAppend` dereferences `SMS_ListPushBack(...) -> m_Param`). Script-counted 18 direct `SMS_ListPushBack (…) -> m_Param = …` dereferences, e.g. `src/SMS_History.c:140`, `src/SMS_Config.c:366`, `src/SMS_FileDir.c:311,340,392,437,453,519,593`, `src/SMS_GUISMBrowser.c:563,571`, `src/SMS_GUI.c:929`, `src/SMS_GUIMenu.c:381`. Also `src/SMS_SubtitleContext.c:278,334` assign through `apList->m_pTail` immediately after a push.
Problem: `SMS_ListPushBack` already returns NULL on allocation failure; every `-> m_Param` call site converts ordinary memory exhaustion (large directory, large SMB share list, long playlist) into a NULL-pointer crash instead of a dropped entry. `Push` and `PushBackBuf` are worse: they crash inside the list code itself, so no caller can defend against them.
Fix: Make the contract uniform and minimal: add the same NULL check `PushBack` already has to `Push` and `PushBackBuf` (return without linking / return NULL), and at the 18 dereference sites capture the node and set `m_Param` only if non-NULL. No new API, no wrapper layer.
Benefit: One mechanism fixed everywhere it occurs; turns OOM crashes across browsing, history, SMB and menus into graceful truncation.
Effort: Medium (many small sites, all mechanical). Risk: Low. Confidence: READ.

### CAND-W4-5 — Subtitle loaders: unchecked node allocation and unbounded node count from the subtitle file
Category: 6 (bugs/security)
Locations: `src/SMS_SubtitleContext.c:451-452` (`_load_sub`: `lpNode = malloc(sizeof(SubNode)); lpNode->m_pList = SMS_ListInit();` — neither checked), `:592-593` (`_load_srt`: same pattern), `:714-716` (`_produce_packets`: `memalign` and `calloc(m_Cnt, …)` results used unchecked), `:278,334` (`_add_line` via CAND-W4-4).
Problem: Subtitle files are external data (downloaded `.srt`/`.sub` next to media). Both parsers allocate one `SubNode` plus a list per entry with no count cap and no allocation check: a many-line file exhausts EE memory, and the first failed `malloc`/`SMS_ListInit` is dereferenced on the next line. `_produce_packets` then trusts `m_Cnt` and unchecked DMA/packet allocations.
Fix: Check the node and its list before linking (free and fail the load on NULL, as other loaders in this tree already do), and cap accepted entries at a fixed maximum consistent with `SUB_MAXLEN`-era memory (drop further entries, keep playing). No new format support.
Benefit: Removes crash/OOM from a routine untrusted input — subtitles are the most commonly user-supplied file in the player.
Effort: Small–Medium. Risk: Low. Confidence: READ.

### CAND-W4-6 — Unbounded whole-file loads sized by the file itself (language, font, asset copy, IRX module)
Category: 4 (size/memory)
Locations: `src/SMS_Locale.c:841-845` (CWD `.lng`), `:874-878` (BDM mc `.lng`), `:888-892` (non-BDM mc `.lng`) — all `malloc(lSize + 1); fioRead/MC_ReadS(…, lSize)` with `lSize` from seek-to-end and no cap; `src/SMS_Config.c:286-307` (`_load_font`: `if (lSize > lFontSize) lpBuff = malloc(lSize)` then read the whole `.mtf`); `src/SMS_GUIFileCtxMenu.c:143-196` (`_copy_file_to_mc`: whole browsed file `malloc(lSize)` + read, purely to write it back out); `src/SMS_RC.c:82-138` (`_load_rmman`: `malloc(lSize)` with **no NULL check at all**, unchecked `fioRead`, then the buffer is patched and executed as an IOP module).
Problem: One mechanism in four places: allocation and read length are whatever the file says. A large `.lng`/`.mtf`/asset does not fail cleanly — it consumes all EE RAM (or, in `_load_rmman`, dereferences NULL and then hands a short/garbage buffer to `SifExecModuleBuffer`). `_copy_file_to_mc` is the worst offender functionally: a streaming copy already exists in the same tree (`SMS_CopyFile`, `src/SMS_CopyTree.c`), so the whole-file buffer buys nothing. Contrast `DrawSkin` (`src/SMS_GUIDesktop.c:155-317`), which caps skins at `0xFFFF0` and checks read length — proof the tree already knows the pattern.
Fix: Cap each load at a format-sane maximum (language/font: a fixed ceiling well above any real pack; skin's existing `0xFFFF0` is the in-tree precedent), check the allocation and the read length, and rewrite `_copy_file_to_mc` on top of the existing streaming copy instead of a whole-file buffer (preserving its read-before-truncate self-copy guard).
Benefit: Bounds four file-driven allocations, deletes the duplicate copy implementation, and closes the NULL-deref in the IRX loader.
Effort: Medium. Risk: Medium for `_copy_file_to_mc` (self-copy case must stay guarded); Low for the caps. Confidence: READ.

### CAND-W4-7 — CDDA `_read_string` does `strlen` on an unterminated stack buffer filled from disc data
Category: 6 (bugs/security)
Locations: `src/SMS_FileContext.c:803-822` (`_read_string`: PS2 branch declares VLA `char lBuf[aMaxLen]`, `apCtx->Read(apCtx, lpBuf, aMaxLen)`, then `strlen(lpBuf)`); callers at `:845-846` (64/512 bytes) and in the directory walk (~`:859+`, 32 bytes) — all data from a CDDA filesystem image.
Problem: Nothing NUL-terminates `lpBuf`. If the stored string fills its field (no NUL in `aMaxLen` bytes), `strlen` reads past the stack VLA, and the subsequent `malloc(lLen + 1)` / `strcpy` pair copies that over-read length. The read result is also used only as a boolean — a short read is treated as a full string.
Fix: Bound it in place: read at most `aMaxLen - 1`, force `lpBuf[aMaxLen - 1] = '\0'` (or use `strnlen`-equivalent loop), and treat a short read as failure. No new helper needed beyond this function.
Benefit: Removes a stack over-read driven by disc/image data at the single function all CDDA strings pass through.
Effort: Small. Risk: Low. Confidence: READ.

### CAND-W4-8 — File mapping is half-finished: every map returns the same fixed virtual address
Category: 8 (half-finished)
Locations: `src/SMS_FileMapping.c:270-302` (`SMS_FileMappingMap`: `void* retVal = (void*)0x32000000; /* implement VAddr allocation here */`), `:276` (`calloc` unchecked before linking), `:294` (`memalign` unchecked before building TLB entries from it), `:304-337` (`SMS_FileMappingUnMap`: removing the head node leaves the new head's `m_pPrev` pointing at the freed node). Sole consumer is JPEG decode (`src/SMS_JPEG.c`, outside this scope).
Problem: The stub comment is literal: there is no virtual-address allocation, so a second concurrent map would alias the first mapping's address and TLB window. Today that is latent (one JPEG at a time in practice), but the API advertises a general mapping service it does not implement, and its error paths (failed `calloc`/`memalign`) corrupt the map chain instead of failing.
Fix: Prefer removal/simplification over completing it: either (a) document and enforce the single-mapping reality — return NULL if `s_pMapChain` is non-empty, check both allocations, fix the head-removal `m_pPrev` — or (b) if no second mapping is ever planned, fold the single window into the JPEG path and delete the chain entirely. Do not build a general VAddr allocator for one caller.
Benefit: Turns a silent aliasing landmine into an explicit, checked contract (or deletes the mechanism).
Effort: Small for (a). Risk: Medium (TLB/refill asm in the same file must not change). Confidence: READ.

### CAND-W4-9 — Fixed-buffer `strcpy` of paths and names that other code allows to be longer
Category: 6 (bugs/security)
Locations: `src/SMS_Player.c:1502-1504` (`char lPath[512]; strcpy(lPath, …->m_pPath)` — `m_pPath` is heap-sized from the full file path, and CWD paths in this tree are up to `SMS_CWD_CAPACITY` 1024, `include/SMS_Sounds.h:24`); `src/SMS_IOP.c:1871-1876` (`SMS_HDDMount`: `strcpy(lMountInfo.m_Path, aPath)` into `char m_Path[124]`, `include/SMS_ioctl.h:74`; caller builds the path from a partition name at `src/SMS_GUIFileMenu.c:1024-1033` and also `strcpy(g_Config.m_Partition /* char[256] */, _STR(apMenu->m_pCurrent))` at `:1038`); `src/SMS_MC.c:145,172` (`strcpy(s_MCmd.m_Name /* char[1024] */, apName)` in `MC_GetDir`/`MC_Open`, non-BDM only); `src/SMS_Config.c:575` (`SMS_ConfigUseFSPath`: `strcpy(s_pMC0SMC /* char[128] */, apPath)` — the header comment at the call sites in `src/SMS_IOP.c:1291-1292` assumes `<= 15` bytes, nothing enforces it).
Problem: One mechanism: a destination size is asserted by a comment or by habit while the source length is controlled elsewhere (file path, partition name from disc, caller-supplied path). Each is a latent stack/global overflow; the player one is on the normal stop/playback-end path (`_Destroy` → history save).
Fix: Length-check at each sink and fail/skip the operation (`sizeof(dst) - 1` bound, `strncpy`+force-NUL or an explicit `strlen` gate like `SMS_ConfigAssetPath` already uses at `src/SMS_Config.c:259-276`). No central string library.
Benefit: Closes four overflow sinks with local, auditable checks.
Effort: Small. Risk: Low. Confidence: READ for the size mismatches; INFERRED that a >512-char media path / >124-char mount path is reachable in normal use (paths are not length-limited upstream).

### CAND-W4-10 — Object constructors and menu builders dereference `calloc`/`malloc` results without a check
Category: 6 (bugs/security)
Locations: `src/SMS_GUIMenu.c:405-414` (`GUI_CreateMenu`), `:379` (`GUI_MenuPushState`), `src/SMS_GUIDesktop.c:132` (`GUI_CreateVersion`), `:730`, `:799`, `src/SMS_GUICmdProc.c:85`, `src/SMS_GUIFileMenu.c:1645` (`GUI_CreateFileMenu`), `src/SMS_GUIMenuSMS.c:3027-3029` (consumes `GUI_CreateMenu` result) — all `retVal = calloc(...); retVal->Render = …` unchecked. Same pattern for menu arrays: `src/SMS_GUICmdProc.c:363-364` (`_playlist_menu`: both `calloc`s unchecked, then indexed; also `&lpItems[lCount - 1]` when `lCount == 0`), `src/SMS_GUIMiniBrowser.c:132-133` (same unchecked pair). Support objects: `src/SMS_RingBuffer.c:21-36` (`SMS_RingBufferInit`: `malloc` result field-written immediately), `src/SMS_PCM.c:24-31` (`SMS_Codec_PCM_Open`: `calloc` result field-written immediately).
Problem: This is the GUI's universal construction idiom, so under memory pressure (the normal state after a large media file) opening a menu crashes instead of failing. The playlist case additionally underflows its item array on an empty selection.
Fix: One idiom, applied mechanically: check the allocation, free any sibling allocation, and return NULL; callers of the `GUI_Create*` family already tolerate NULL objects in several paths — where they do not, the NULL return at least localises the failure to creation. Guard the `lCount == 0` playlist case before `&lpItems[lCount - 1]`.
Benefit: Removes the crash-on-open class across every menu/desktop object at once.
Effort: Medium (many sites, one pattern). Risk: Low–Medium (NULL-object handling at a few call sites needs the check, not a redesign). Confidence: READ.

### CAND-W4-11 — Directory-tree scan/walk/destroy recurse to filesystem depth, and tree init is unchecked
Category: 6 (bugs/security)
Locations: `src/SMS_DirTree.c:46-65` (`SMS_DirTreeInit`: `calloc` result passed straight to `SMS_DirAdd(&retVal->m_Root, …)`; also VLA `char lRoot[strlen(apRoot) + 2]` and `lRoot[lLen - 1]` read with no empty-string guard), recursion at `:153` (`Scan`), `:224` (`Walk`), `:258` (`Destroy`); callers `src/SMS_CopyTree.c:150` (`SMS_CopyTree`: result used unchecked, `strrchr(apSrc, '/') + 1` at `:151` with no NULL check if the path has no slash) and `:252` (`SMS_DeleteTree`).
Problem: Copy/delete of a deeply nested tree (user media on HDD/USB/SMB) recurses once per directory level in three separate walks, on the EE's small stacks, with no depth bound — a deep-but-legitimate tree crashes the browser action. The unchecked init and `strrchr` are the same "assume success" pattern at the entry point.
Fix: Check `SMS_DirTreeInit`'s result at both callers (and inside init before `SMS_DirAdd`), NULL-check the `strrchr`, and add one depth counter threaded through Scan/Walk/Destroy with a sane ceiling that sets `m_Error` (the tree already has that error channel) instead of recursing further.
Benefit: Bounds a user-data-driven stack exhaustion in the only copy/delete path.
Effort: Medium. Risk: Medium (three recursive functions must share the ceiling consistently). Confidence: READ for the recursion/unchecked patterns; INFERRED for the exact depth at which a real stack overflows (stack sizes are set outside this scope).

### CAND-W4-12 — `SMS_Realloc` publishes the new size even when `realloc` fails
Category: 6 (bugs/security)
Locations: `src/SMS_EE.c:45-53` (`*apSize = 17 * aMinSize / 16 + 32; return realloc(apData, *apSize);`). Companion oddity in the same file: `:55-89` (`SMS_ReallocWithAlign` shrinks a misaligned result back to the old size with a second `realloc` before copying — correct only because it re-publishes `*apSize = aMinSize` at `:85` after the copy).
Problem: On failure `realloc` returns NULL but `*apSize` already claims the larger size; a caller that keeps the returned NULL with the published size (the AVI/MOV index growers are the known callers, outside this scope) will write against a size the buffer does not have. The size must move only when the allocation does.
Fix: Realloc into a temporary, publish `*apSize` and return only on success; on failure return the original pointer with the original size (callers already treat a too-small buffer via their own bounds). Three lines changed, no signature change.
Benefit: Removes a size/pointer desynchronisation at the root of every growable index buffer.
Effort: Small. Risk: Low. Confidence: READ.

### CAND-W4-13 — DSP MDCT init: unchecked table allocation and a dead error test
Category: 6 (bugs/security)
Locations: `src/SMS_DSP.c:933-972` (`DSP_MDCTInit`: `s->m_pCos = memalign(...)` at `:945` unchecked, then `s->m_pSin`/`s->m_pTmp` derived by pointer arithmetic and written in the loop at `:965-968`; error test at `:972`: `if (!DSP_iFFTInit(&s->m_FFT, s->m_nBits - 2) < 0)`). Contrast `DSP_iFFTInit` itself at `:879-895`, which does check its `memalign`.
Problem: Two defects in one init: (1) a failed `memalign` writes the cosine table through NULL-derived pointers; (2) `!x < 0` is always false (`!x` is 0 or 1, neither is `< 0`), so the FFT-init failure branch — destroy and return 0 — can never execute; a failed FFT init is reported as success with a half-initialised context.
Fix: Check `m_pCos` like `DSP_iFFTInit` checks its table (return 0), and write the test as the intended `if (!DSP_iFFTInit(...))` (its return is 1/0 per `:931`). No other change.
Benefit: Makes audio-codec init fail cleanly instead of corrupting memory or lying about success.
Effort: Small. Risk: Low. Confidence: READ.

### CAND-W4-14 — `MC_SeekS` can return an uninitialised value
Category: 6 (bugs/security)
Locations: `src/SMS_MC.c:307-316` (`int retVal; MC_Seek(...); MC_Sync(&retVal); return retVal;`); `MC_Sync` at `:75-92` writes `*apRes` only when `s_LastCmd` is set. Non-BDM only (`#ifndef BDM` file). Sibling wrappers initialise theirs: `MC_ReadS` at `:265-274` starts from `0`.
Problem: If `MC_Seek` could not queue the RPC (a command already in flight — it returns 0 without setting `s_LastCmd`), `MC_Sync` leaves `retVal` untouched and the caller (config/palette/font seeks on the non-BDM path) acts on stack garbage as a file position/result.
Fix: Initialise `retVal = 0;` exactly as `MC_ReadS` does. One line.
Benefit: Removes an uninitialised return on a path used by every non-BDM asset load.
Effort: Trivial. Risk: None. Confidence: READ.

### CAND-W4-15 — `SMS_SubContID` can never return FLAC: it compares the wrong suffix
Category: 6 (bugs/security)
Locations: `src/SMS_FileDir.c:88-118` (`SMS_SubContID`: `lpExt = apName + lLen - 4`, then at `:112-114` `if (!stricmp(lpExt, s_pFLAC))` where `s_pFLAC` is `".flac"` — 5 chars compared against a 4-char suffix pointer). The sibling functions get it right: `SMS_ContID` at `:160-176` and `SMS_FileID` at `:214-231` both do `--lpExt` before the 5-char comparisons.
Problem: `lpExt` points at `"flac"` (no dot); `stricmp("flac", ".flac")` never matches, so the FLAC sub-container branch is dead and `.flac` files fall through to generic probing instead of the direct sub-container ID the other two classifiers grant them.
Fix: Add the missing `--lpExt` in the `lLen > 5` branch, mirroring the two sibling functions verbatim. One character of mechanism, no consolidation needed (the three classifiers differ deliberately in their tables).
Benefit: Restores an intended, documented classification that is currently unreachable.
Effort: Trivial. Risk: Low. Confidence: READ.

### CAND-W4-16 — Whole-file asset loading is triplicated (skin) and triplicated again (language)
Category: 2 (duplication)
Locations: Skin: `src/SMS_GUIDesktop.c:155-317` (`DrawSkin` contains three near-identical open → seek-end → capped `malloc` → read → verify-length blocks: CWD/fio, BDM mc/fio, non-BDM `MC_*`). Language: `src/SMS_Locale.c:826-897` (same three-way split, same block shape, differing only in open/read/close calls). History's three loops are the third instance of the pattern and are counted in CAND-W4-3.
Problem: Every fix to the load pattern (size cap, read-length check, allocation check) must be made three times per asset — and the record shows it has been: the skin blocks carry the `0xFFFF0` cap and read checks, the language blocks (CAND-W4-6) do not. The divergence is the bug farm, not the line count.
Fix: One static load-whole-file helper per file (open callback or a fio-only path where the BDM migration already made `MC_*` unnecessary — locale and palette already argue this in their own comments), with the cap/check inside it; the three call sites keep only their path construction.
Justification (if ADDS): The helper is justified by three concrete existing copies per file and is a net deletion in both files.
Benefit: Single place to cap/check every asset load; removes ~120 duplicated lines across the two files.
Effort: Medium. Risk: Medium (non-BDM `MC_*` path must keep working or be explicitly retired like its siblings). Confidence: READ.

### CAND-W4-17 — `src/SMS_Menu.c` is a dead duplicate stub that is not in the build
Category: 1 (dead code)
Locations: `src/SMS_Menu.c:1-7` (entire file: `GUI_CreateMenu` returning `0`). The real implementation is `src/SMS_GUIMenu.c:403-414`; `SMS_Menu.o` does not appear in `EE_OBJS` in the root `Makefile` (lines 22-47), while `SMS_GUIMenu.o` does.
Problem: A second definition of a live symbol sitting in `src/` invites a future build-list edit (or a wildcard object list) to link the stub — which would silently break every menu — and misleads any reader grepping for `GUI_CreateMenu` to a 7-line fake.
Fix: Delete the file. Nothing references it and it is not compiled.
Benefit: Removes a symbol-collision trap and a false reading target for zero functional cost.
Effort: Trivial. Risk: None. Confidence: READ.

---

## (c) Checked-clean notes

- `src/SMS_Artwork.c` (whole file): bounded by design — name builder validates stem/capacity and rejects path separators (`:37-47`), JPEG preflight rejects oversized dimensions before allocation (`:52-75`), thumbnail/poster validate dimensions and use fixed output sizes. No candidate.
- `File_GetString` (`src/SMS_FileContext.c:2176-2208`): truncation-aware, always NUL-terminates, treats a stalled refill as EOF. The SMB config loader built on it (`SMS_LoadSMBInfo`) is bounded per-field; its remaining defects are the allocation ones in CAND-W4-4/W4-10, not the parsing.
- `SMS_ConfigAssetPath` (`src/SMS_Config.c:259-276`) and `_follow_cfg_pointer` (`:186-257`): length-gated, device-validated, BOM/line handling explicit. Model examples for CAND-W4-9's fix style.
- `DrawSkin`'s load blocks (`src/SMS_GUIDesktop.c:155-317`): size-capped (`0xFFFF0`), allocation- and read-length-checked — flagged only for triplication (CAND-W4-16), not correctness.
- Artwork loading in `src/SMS_GUIFileMenu.c:141-200`: file-size cap (`SMS_ARTWORK_MAX_FILE`), allocation check, full-read check, JPEG preflight before decode. Clean.
- `_append_directory_path` (`src/SMS_FileDir.c:242-262`) and the directory scan's probe-path guard (`:540-560` approx.): CWD appends are capacity-checked against `SMS_CWD_CAPACITY`. The unchecked path copies are elsewhere (CAND-W4-9).
- SMB share listing (`src/SMS_FileDir.c:355-400` approx.): network-supplied share name/comment are force-NUL-terminated at `[255]` before any `strlen`/`strcpy`, and the concatenation buffer is heap-sized from the actual lengths. Clean.
- `SIF_BindRPC` (`src/SMS_SIF.c`): bounded retry (2,000) with a documented hardware diagnosis; callers treat 0 as "no server". Clean.
- `File_Skip` (`src/SMS_FileContext.c`): large skips seek, read loop stops on short read — underflowed skip counts terminate at EOF instead of hanging. Clean.
- STIO PS2 init (`src/SMS_FileContext.c:2043-2160` approx.): allocation chain checked, `m_pPath` failure jumps to cleanup, fd closed on failure. (The `_WIN32` twin branch lacks the `m_pPath` check, but it is not compiled for the PS2 target — noted, not counted.)
- `SMS_LoadPalette` / `SMS_LoadXLT` (`src/SMS_Config.c`): fixed-size reads (64 / 256 bytes) into fixed buffers. Clean.
- `SMS_PlayerBallSim.c`, `SMS_Spectrum.c`, `SMS_GUIClock.c`, `SMS_DateTime.c`, `SMS_OS.c`, `SMS_PAD.c`, `src/mbstring.c`, `src/SMS_Codec.c` (dispatch tables): read in full; no mechanism in the hunt list found beyond the shared patterns already counted (palette-index use in Spectrum is CAND-W4-1; codec dispatch guarantees the PCM IDs whose `_Decode` selection is CAND-W4-10-adjacent but not separately counted).
