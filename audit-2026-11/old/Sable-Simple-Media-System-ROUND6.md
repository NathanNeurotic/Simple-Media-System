# Sable — Simple-Media-System — ROUND 6 (FINAL SWEEP)

**Repo:** NathanNeurotic/Simple-Media-System · **Pin:** `e65ced39a27eee8d996e9257e21594d1cac1c93d` — **held**: clone HEAD == `git ls-remote origin master` == pin, verified at start and finish; no delta, no revalidations owed. `git status` clean; read-only throughout (the clone's own `AGENTS.md` was read when surfaced — it is a GitHub attribution policy for contributions; this audit makes no commits, PRs or GitHub content of any kind, so it changed nothing).

**Scope (round 5's verdict residue, verbatim):** (1) end-to-end read of the last five unread EE files — SMS_Container.c (393), SMS_Locale.c (965), SMS_History.c (200), SMS_Spectrum.c (132), SMS_VideoBuffer.c past its frame-buffer init (384) — 2,074 lines; (2) SMS_DSP.c kernels (:879–2166) read for Category 6/5 defects only (bounds, divides, unchecked sizes — no style/duplication opinions on kernel bodies); (3) the formal exhaustion verdict.

**Method:** all six files read first-hand, end-to-end, at the pin. Every finding's mechanism was verified at its cited lines; caller/provenance questions were settled by grep across the tree (container creators, `SMS_ListPushBackBuf` callers, `m_BitRate` assignments, `SMS_ContID` range, DCT4_32 index ceiling checked mechanically). Prior-round coverage of these files (SMS-013/027/028/038/045/047/052/053, the round-2 string classification, round 3's ID3 verdict, round 4's frame-buffer geometry verdict) was re-checked against the text and is **not** repeated.

**Result: 10 new findings, SMS-093…102, plus 2 location-completing revisions (R6-REV-1 → SMS-057, R6-REV-2 → SMS-068).** SMS-035 stays settled (docs-only), untouched.

---

## Findings

### SMS-093 — History load has no record cap, and its allocator drops the NULL check its sibling has
- **Category:** 6 (bugs/security)
- **Locations:** `src/SMS_History.c:40-60` (CWD load loop; alloc at `:48`), `:76-96` (BDM memory-card loop; alloc at `:85`), `:98-118` (non-BDM loop; alloc at `:103`); the cap that load ignores: `src/SMS_History.c:20` (`HIST_SIZE 32`), enforced only on the write side at `:141`; root contract: `src/SMS_List.c:57-73` (`SMS_ListPushBackBuf` — no NULL check) vs `src/SMS_List.c:36-55` (`SMS_ListPushBack` — checks at `:40`).
- **Problem:** `SMS_HistoryLoad` reads records until EOF in all three backend loops. Each record's length is a file-supplied `unsigned short`, and each record allocates `lSize + 1` bytes via `SMS_ListPushBackBuf` — with **no count limit**. The 32-entry cap that defines the structure exists only in `SMS_HistoryAdd` (:141). A corrupt or oversized `SMS.hst` — the CWD copy next to the ELF loads from host/USB with no size bound at all — therefore drives unbounded list growth at startup until the 32 MB heap is gone. Compounding it, `SMS_ListPushBackBuf` itself dereferences its `calloc` result unconditionally while linking the node (`lpNode -> m_pPrev = ...`, `src/SMS_List.c:57-73`), so the first allocation failure under that growth is a NULL dereference inside the callee — its sibling `SMS_ListPushBack` has the `if ( !lpNode ) return NULL;` check (:40) and this one simply never got it. History is `SMS_ListPushBackBuf`'s only caller in the tree (grep-verified: the three sites above are the complete caller set), so the defect and its blast radius are exactly this loader. (The payload `fioRead`/`MC_ReadS` returns in the same loops are also unchecked, but the buffers are calloc-zeroed, so a truncated record degrades to a NUL-padded string and a zero PTS — benign, noted only for completeness.)
- **Fix:** Two deletions of asymmetry, no new machinery: give `SMS_ListPushBackBuf` the same one-line NULL check `SMS_ListPushBack` already has, make the three load loops break on NULL, and stop each load loop once `s_pHst -> m_Size` reaches `HIST_SIZE` — the list is a 32-entry structure by design, so records past 32 are definitionally not loadable state.
- **Benefit:** A corrupt history file degrades to "no/fewer resume points" instead of an out-of-memory NULL dereference during startup. **Effort:** S · **Risk:** Low · **Confidence:** High — READ (all three loops, the write-side-only cap, and the sibling asymmetry verified at the pin).

### SMS-094 — `DSP_MDCTInit`'s only error path is dead code: `!DSP_iFFTInit(...) < 0` can never be true
- **Category:** 6 (bugs/security)
- **Locations:** `src/SMS_DSP.c:972` (the test); the return convention it misreads: `src/SMS_DSP.c:892` (`return 0` on allocation failure) and the success return at the end of `DSP_iFFTInit` (`:879-931`, returns 1); callers that ignore the result regardless: `src/SMS_AAC.c:5993-5994`, `src/SMS_OGG.c:3979-3980`, `src/SMS_WMA.c:1539`.
- **Problem:** `DSP_MDCTInit` ends with `if ( !DSP_iFFTInit ( &s -> m_FFT, s -> m_nBits - 2 ) < 0 ) { DSP_MDCTDestroy ( s ); return 0; }`. By C precedence this parses as `(!DSP_iFFTInit(...)) < 0` — `!x` is 0 or 1, never negative, so the condition is **always false**. It would be dead even without the stray `!`: `DSP_iFFTInit` returns 1 on success and 0 on failure, never a negative value (this is a transcription of the FFmpeg idiom `if (ff_fft_init(...) < 0)` against a function whose convention is the opposite shape). Consequence: when the inner FFT table allocation fails (the realistic trigger is heap exhaustion on a 32 MB machine, mid-codec-init), `DSP_MDCTInit` returns 1 — success — with `m_FFT.m_pExpTab`/`m_pRevTab` NULL, and the `m_pCos` block allocated at :952 leaks because the destroy call lives in the dead branch. Every MDCT consumer (AAC, Vorbis, WMA) ignores the return value anyway, so this false success is the only thing between an allocation failure and the first transform running through NULL tables.
- **Fix:** Delete two tokens: `if ( !DSP_iFFTInit ( &s -> m_FFT, s -> m_nBits - 2 ) )` — the branch as written (destroy + return 0) is then exactly right.
- **Benefit:** Codec init failure becomes a detectable, cleaned-up failure instead of a latent NULL-table dereference in the decode loop. **Effort:** S · **Risk:** Low · **Confidence:** High — READ (precedence, both return sites, and the caller set verified at the pin).

### SMS-095 — `DSP_MDCTInit` never checks its `m_pCos` allocation before writing through it
- **Category:** 6 (bugs/security)
- **Locations:** `src/SMS_DSP.c:952` (`s -> m_pCos = ( float* )memalign ( 16, n4 * sizeof ( float ) * 2 + n2 * sizeof ( float ) )`), the derived pointers `m_pSin = m_pCos + n4` / `m_pTmp = m_pSin + n4` immediately following, and the CosSin fill loop that writes `s -> m_pCos[ i ]` / `s -> m_pSin[ i ]` unconditionally; contrast `src/SMS_DSP.c:892` (`DSP_iFFTInit` checks its own memalign).
- **Problem:** If the `memalign` at :952 fails, `m_pSin` and `m_pTmp` become small offsets from NULL and the fill loop writes the cosine/sine tables through them — a write to near-zero addresses during codec init. The neighbouring init function checks its identical allocation (:892); this one checks nothing between the allocation and the writes.
- **Fix:** One check after :952 — `if ( !s -> m_pCos ) return 0;` — matching `DSP_iFFTInit`'s existing shape. (With SMS-094 fixed, failure teardown is then complete: nothing else has been allocated yet at this point in the function.)
- **Benefit:** Removes a write-through-NULL on the allocation-failure path of every MDCT codec's setup. **Effort:** S · **Risk:** Low · **Confidence:** High on the missing check — READ; the trigger (allocation failure) is INFERRED as the only path, as with all OOM findings in this audit.

### SMS-096 — `SMSContainer_DefReadPacket`: a dead clamp, a wrong-buffer unalloc, and an unchecked packet allocation
- **Category:** 6 (bugs/security), with a Category 1 element (the dead clamp)
- **Locations:** `src/SMS_Container.c:308-332` (the whole function): clamp computed at `:320-321`, ignored by the `Read` at `:322`; unalloc against stream 0 at `:326`; allocation at the function head via `_AllocPacket` (`src/SMS_Container.c:62-82`, NULL return at `:66` when `SMS_RingBufferAlloc` fails).
- **Problem:** Three defects in 25 lines. (a) `lnRead` is computed as the EOF-clamped read length (:320-321) and then **never used** — `Read` is called with the unclamped `lPackSize` (:322). As written this is dead code that documents a protection the function doesn't have; correctness currently rests on each FileContext `Read` implementation short-reading at EOF and on the memset pad that follows. (b) On the no-data path, the packet is returned to `apCont -> m_pStm[ 0 ] -> m_pPktBuf` (:326), but it was allocated from `apCont -> m_pStm[ *apIdx ] -> m_pPktBuf` where `*apIdx = apCont -> m_DefPackIdx`. Whenever `m_DefPackIdx != 0` the unalloc corrupts the wrong ring buffer's accounting. Today every `DefReadPacket` user (FLAC, MP3, OGG) leaves `m_DefPackIdx` at 0, so this is latent — but the helper's contract invites the nonzero case (M3U propagates subcontainers' `m_DefPackIdx` verbatim, `src/SMS_ContainerM3U.c:196,509`). (c) The `_AllocPacket` result is dereferenced at :322 (`lpPkt -> m_pData`) without a NULL check; `_AllocPacket` returns NULL exactly when the ring buffer cannot fit the packet.
- **Fix:** Use the stream the packet came from in the unalloc (`m_pStm[ *apIdx ]`), return -1 when `lpPkt` is NULL, and either use `lnRead` in the `Read` call or delete the two dead lines — the clamp is the safer survivor, since the function already computes it.
- **Benefit:** One shared packet reader stops being a latent ring-buffer corruption for any future nonzero-index stream and stops dereferencing a failed allocation. **Effort:** S · **Risk:** Low · **Confidence:** High — READ for all three sites; the (b) trigger is INFERRED-latent (no current caller sets a nonzero index for these containers).

### SMS-097 — `SMSContainer_CalcPktFields` divides by the file's bit rate, in 32-bit arithmetic
- **Category:** 6 (bugs/security)
- **Locations:** `src/SMS_Container.c:239-240` (`lNum = lpCodecCtx -> m_FrameSize <= 1 ? ( apPkt -> m_Size * 8 * lpCodecCtx -> m_SampleRate ) / lpCodecCtx -> m_BitRate : ...`); callers `src/SMS_ContainerASF.c:619`, `src/SMS_ContainerAVI.c:837`, `src/SMS_ContainerMOV.c:1271`, `src/SMS_ContainerMPEG_PS.c:574`; zero-capable provenance (all READ): `src/SMS_ContainerASF.c:350,676`, `src/SMS_ContainerMOV.c:449`, and the WAV path in `SMSContainer_GetWAVHeader` (`src/SMS_Container.c`) — all assign `m_BitRate` from raw file fields with no nonzero gate.
- **Problem:** For an audio stream with `m_FrameSize <= 1`, packet duration is computed by dividing by `m_BitRate`, which three container parsers take straight from the file — a header that declares bit rate 0 divides by zero here. On the EE's MIPS core that yields an undefined quotient rather than a trap, and the `if ( lDen && lNum )` guard two statements later cannot catch a garbage *nonzero* quotient, so the stream's `m_CurDTS` accumulates a garbage duration and A/V sync drifts or jumps. The same expression overflows on ordinary input: `m_Size * 8 * m_SampleRate` is `int` arithmetic, and a 64 KiB packet at 48 kHz is ≈ 2.5 × 10¹⁰ — past `INT_MAX` before the division even happens, so large-packet streams get negative durations without any crafted file.
- **Fix:** Guard the divisor (`m_BitRate` 0 → treat duration as 0, the value the guard already falls back to) and widen the product to `int64_t` — `SMS_Rescale`, used three lines later, already takes 64-bit arguments, so this adds no helper.
- **Benefit:** Crafted zero-bit-rate headers stop poisoning stream timing, and legitimate large-packet audio gets correct durations. **Effort:** S · **Risk:** Low · **Confidence:** Med — the division site and the zero-capable provenance are READ; which shipping codecs present `m_FrameSize <= 1` streams to these callers (the PCM family is the candidate) is INFERRED.

### SMS-098 — `SMS_GetContainer`'s explicit-probe path skips the bounds check and throws away the result
- **Category:** 6 (bugs/security)
- **Locations:** `src/SMS_Container.c:198` (`} else s_CCreator[ aPrefCont ] ( retVal );`); the automatic path in the same function validates the same table index before use (the `SMS_ContID` result is checked nonzero and `< sizeof ( s_CCreator ) / sizeof ( s_CCreator[ 0 ] )` before the table is touched); sole explicit-path callers: `src/SMS_ContainerM3U.c:186,485` with the constant table at `src/SMS_ContainerM3U.c:45,469-474`.
- **Problem:** The explicit path indexes the creator table with `aPrefCont` unchecked and discards the creator's return value, so the function's own `lSts < 0` teardown never fires for an explicit probe. It is safe today only by caller discipline: M3U passes compile-time constants 1–7, and `SMS_ContID`'s range is −1..11 (`src/SMS_FileDir.c:124-185`), both inside the table's 0..11 creators + NULL at 12. One future caller passing a computed index gets a call through the NULL terminator (12), a wild pointer (≥13), or a before-table read (any negative value other than the auto-probe sentinel) — in the function whose automatic half demonstrates the check was considered necessary.
- **Fix:** Give the explicit call the same two properties the automatic path has: range-check `aPrefCont` against the table size (fall through to the `m_pName == NULL` rejection if out of range) and assign its result to `lSts`. No new code — the check already exists in the function.
- **Benefit:** The container dispatcher stops depending on every future caller re-deriving the table bounds. **Effort:** S · **Risk:** Low · **Confidence:** High — READ (site, sibling check, and the complete caller set verified); the trigger is INFERRED (no current caller misbehaves).

### SMS-099 — `SMSContainer_SetName`'s backward slash scan has no start-of-string stop
- **Category:** 6 (bugs/security)
- **Locations:** `src/SMS_Container.c:284-306`; the scan at `:293-300` (`lpSlash` starts at the last character and decrements until it finds `'\\'` or `'/'`); callers `src/SMS_ContainerAAC.c:217`, `src/SMS_ContainerAC3.c:125`, `src/SMS_ContainerASF.c:714`, `src/SMS_ContainerFLAC.c:143`, `src/SMS_ContainerMOV.c:1532,1547`, `src/SMS_ContainerMP3.c:137`, `src/SMS_ContainerOGG.c:120`.
- **Problem:** The loop's only exit is finding a slash. A path containing neither `'/'` nor `'\\'` walks `lpSlash` off the front of the string (an empty path starts it at `path[-1]`), reading backward through whatever memory precedes the buffer until a slash-valued byte turns up; the pointer it lands on is then pushed as the playlist display name and copied by the list code. All current callers pass browser-built device paths, which always contain a '/', so this is latent — but `m_pPath` is the one string every container trusts, and nothing in this function states or enforces the assumption it depends on.
- **Fix:** One comparison: stop the walk at `apFileCtx -> m_pPath` and treat "no slash" as "the whole string is the name". Deletes the unbounded read; adds no path.
- **Benefit:** The name extractor becomes safe for any path shape its callers might grow, not just today's. **Effort:** S · **Risk:** Low · **Confidence:** High on the mechanism — READ; the trigger is INFERRED (no current slash-less caller found).

### SMS-100 — The history record-read loop exists three times
- **Category:** 2 (duplication)
- **Locations:** `src/SMS_History.c:40-60`, `:76-96`, `:98-118`.
- **Problem:** The same ~10-line record loop (read 2-byte length → `SMS_ListPushBackBuf` → read string → read 8-byte param) is pasted once per storage backend. The CWD and BDM-card loops are identical except for how the fd was obtained — both are plain `fioRead` loops; only the non-BDM copy genuinely differs (libmc sync wrappers). Three copies is why SMS-093's missing cap has three homes instead of one.
- **Fix:** Extract the fio loop into one static helper taking the opened fd; the two fio call sites shrink to open-and-call. The libmc copy stays (different API). Net deletion ≈ 12 lines.
- **Benefit:** One place to cap, check and fix the history parser — SMS-093 becomes a one-site fix. **Effort:** S · **Risk:** Low · **Confidence:** High — READ (all three copies read in full this round).

### SMS-101 — The locale loader's read-whole-file block exists three times, its retry twice
- **Category:** 2 (duplication)
- **Locations:** `src/SMS_Locale.c:830-848` (CWD, fio), `:864-880` (BDM card, fio), `:883-894` (non-BDM card, libmc); lowercase-name retry duplicated at `:834-835` and `:868-869`.
- **Problem:** `SMS_LocaleInit` (:813) slurps `SMS.lng` with the same seek-END / size / `malloc ( lSize + 1 )` / seek-SET / read / close sequence three times, and the lowercase-filename retry (open, measure, rewrite last 7 chars, reopen) twice. The two fio copies differ only in the path string they start from. This is the same shape as SMS-100, in the loader that runs at the same startup moment.
- **Fix:** One static fio slurp helper (path in, buffer+size out) serves both fio sites including the retry; the libmc copy keeps its own API calls. Net deletion ≈ 20 lines.
- **Benefit:** The `.lng` load path — already the subject of SMS-022 and SMS-046 — stops drifting in triplicate. **Effort:** S · **Risk:** Low · **Confidence:** High — READ (all three blocks read in full this round).

### SMS-102 — Probe-order array is dimensioned in bytes, not elements: 156 stack bytes for nothing
- **Category:** 4 (size/memory)
- **Locations:** `src/SMS_Container.c:149` (`ContainerCreator lCC[ sizeof ( s_CCreator ) ];`).
- **Problem:** `sizeof ( s_CCreator )` is a byte count (13 pointers × 4 = 52), so the stack array has 52 slots for a 13-entry table — 39 unused pointers, 156 bytes of stack, on every container open. Nothing overruns (the reorder loop in the same function uses the correct `sizeof/sizeof` bound, and the `memcpy` use of the byte size is correct); it is pure waste plus a trap for the next reader, who must re-derive that the declaration is the one wrong use of the three.
- **Fix:** Delete one word's worth of confusion: dimension it `sizeof ( s_CCreator ) / sizeof ( s_CCreator[ 0 ] )`, as the loop below already does.
- **Benefit:** 156 stack bytes back and one less sizeof idiom to misread. **Effort:** S · **Risk:** Low · **Confidence:** High — READ (declaration, loop bound and memcpy use all verified at the pin).

---

## Revisions to earlier findings (location completions — no new defects)

### R6-REV-1 — SMS-057 gains the `m_BrowserIBCIdx` sinks
SMS-057 (round 5) filed the palette-rotate wrap defect and the `m_pColor`-pointer sinks in `SMS_GUIFileMenu.c`. The same `_rotate_palette` also rotates `m_BrowserIBCIdx` (`src/SMS_GUIMenuSMS.c:2215`), and that field's **direct** read sinks were not enumerated. Round 6's read of SMS_Spectrum.c completes the list: `src/SMS_Spectrum.c:51,55` (baked into the persistent spectrum DMA packet by `SMS_SpectrumInit` — so a stored 0 is re-read every frame until re-init), `src/SMS_GUIDesktop.c:756`, `src/SMS_GUInfoPanel.c:141`, `src/SMS_PlayerControl.c:431` — all `g_Palette[ g_Config.m_BrowserIBCIdx - 1 ]`. SMS-057's fix (`lVal < 1`) covers all of them unchanged; no new finding is created.

### R6-REV-2 — SMS-068 gains a twelfth site
SMS-068 (round 5) filed eleven `SMS_ListPushBack(...)->m_Param` dereferences of the documented NULL-on-failure return. Round 6's read adds the twelfth: `src/SMS_History.c:140` (`SMS_ListPushBack ( s_pHst, apPath ) -> m_Param = aPTS;` in `SMS_HistoryAdd`). Same contract, same fix shape; SMS-093 covers the distinct `SMS_ListPushBackBuf` asymmetry in the same file and is not part of this revision.

---

## Checked and clean this round (read in full; no finding)

- **SMS_Container.c — ID3 skip (:360-393 region):** re-verified against round 3's verdict; the syncsafe validation still precedes any trust in the size, and the assembled value cannot overflow. Remains the best-written probe in the container set.
- **SMS_Container.c — `SMS_DestroyContainer` / probe-reuse convention:** consistent with the self-clean contract the creators rely on; no leak path found that a creator doesn't already own.
- **SMS_Container.c — `SMSContainer_GetWAVHeader`:** SMS-045 (unchecked extradata allocation) and SMS-052 (the size-17 clamp inversion) already own this function's defects; the trailing seek arithmetic is sound once the clamp has run (the residual is ≥ 0 by construction). Nothing new.
- **SMS_Spectrum.c — DMA geometry:** the init and update loops step 9 qwords × 16 bars from `g_SPCPkt + 6`, ending at +141 with writes to +145 — inside the packet region (`include/SMS_Sounds.h:38-39` places `g_FFTWsp` 1,184 qwords past `g_SPCPkt`). `s_lIdx`, `lpH` and `s_PrevH` indexing is bounded 0..15 by construction in both Calc and Update. The per-frame cost question is SMS-038's, already filed; nothing new in the file itself.
- **SMS_VideoBuffer.c — remainder (`SMS_VideoBufferInit`, destroy, CSC glue):** the geometry guards (INT_MIN rejection, sign normalisation, ≤ 65504 caps, 64-bit product ceiling) are consistent with the DMA size actually allocated; failure paths destroy the partial buffer and return NULL; `m_VFrm[2].m_pNext`'s one-past-end formation is overwritten with NULL before any use. The CSC routines (`SMS_CSC`, `_csc_handler`, Resume/Suspend/Sync) are inline assembly — inventoried by name, per the audit's framing.
- **SMS_DSP.c — `DCT4_32` (:1726-2166):** fixed-size generated kernel; every `x[ ]` read and `y[ ]` write verified ≤ 31 (checked mechanically across the whole body), the middle stage contains no stores at all, and there are no divisions. `DSP_DCT4Kernel` (:1066-1724) is inline assembly and `s_DCT4Const` (:995-1064) is data — both inventoried, not audited, per the kernel framing.
- **SMS_DSP.c — `DSP_iFFTInit` (:879-931):** its memalign *is* checked (:892); `m_pRevTab` is formed from the pointer before the check but is not dereferenced on the failure path (the return precedes all use). The table-fill loop is FFmpeg-derived and internally consistent. No finding — the defects are in its caller, SMS-094/095.
- **SMS_Locale.c — parse loop and default table:** the `*lpPtr = '\x00'` write at end-of-buffer lands on the `malloc ( lSize + 1 )` spare byte by design; the `s_SMStringUDF[ 167 ]` probe reads stay inside the file image because a successful 311-line parse guarantees line 167 is never the last line; the empty-line early break is SMS-022's exact-count strictness, not a new defect. `SMS_LocaleSet`'s table copy is size-matched by construction (both arrays are dimensioned from `s_SMStringDef`).

---

## Formal verdict (Task 3)

**Coverage by area, all rounds:**

| Area | State |
|---|---|
| EE — GUI (menus, framework, desktop, device/file/context menus, mini-browser, clock, icons) | **Read in full, round 5.** |
| EE — rendering (GS font/GS/IPU glue, artwork, progress indicator, ball sim, subtitle context) | **Read in full, round 5.** |
| EE — player core (SMS_Player.c, SMS_PlayerControl.c, SMS_PlayerMenu.c) | **Read in full, round 5.** |
| EE — file backends (SMS_FileContext.c STIO + CDDA + prefetch) | **Read in full, round 5.** |
| EE — platform (SMS_EE.c, SMS_MC.c, SMS_PAD.c, SMS_RC.c, main.c, SMS_IOP.c, SMS_CDDA.c, SMS_CDVD.c, SMS_SIF.c, SMS_VSync.c, DMA/VIF glue) | **Read in full across rounds 3–5.** |
| EE — config/files/lists (SMS_Config.c, SMS_FileDir.c, SMS_List.c, SMS_DirTree.c, SMS_CopyTree.c) | **Read in full, round 2** (SMS_List.c's PushBackBuf contract re-read round 6 for SMS-093). |
| EE — media plumbing (SMS_JPEG.c, SMS_Rescale.c, SMS_MPEGInit.c, SMS_IPU.c, SMS_SPU.c, SMS_FileMapping.c, SMS_RingBuffer.c, SMS_VLC.c, SMS_PCM.c, SMS_Codec.c) | **Read in full, round 4.** |
| EE — containers (AVI, ASF, MOV, MPEG_PS, M3U, MP3, OGG, AAC, AC3, FLAC, JPG parsers) | **Read in full across rounds 2–4.** |
| EE — container base + glue residue (SMS_Container.c, SMS_Locale.c, SMS_History.c, SMS_Spectrum.c, SMS_VideoBuffer.c) | **Read in full, round 6 — this report.** The last unread EE C source in the repo. |
| EE — DSP (SMS_DSP.c) | Tables/init inventoried round 4 (the GMC chroma anomaly at :290 remains an unnumbered vendored note, by framing); **kernel range :879–2166 read for Cat 6/5, round 6**; the one assembly kernel (DSP_DCT4Kernel) inventoried by name. |
| Codecs (DTS, FAAD2/AAC, mpg123 core glue, libvorbis, AC3, WMA, FLAC, H263, MPEG family) | **Inventory by framing** (all rounds): provenance verified as unmodified upstream snapshots (import `d704063` + mechanical commits); allocation verdicts delivered round 3; decode internals deliberately not line-audited. |
| IOP — locally owned (smbman-sms, SMSUdpfs local mods, SMSCDVD) | **Line-audited rounds 2–3**: smbman-sms clean (best-hardened code in the repo), udpfs mods clean, SMSCDVD one residual (SMS-049). |
| IOP — dead (SMSSMB tree) | SMS-036; built by nothing. |
| IOP — pristine upstream inventory (SMSTCPIP, SMSMAP, SMSUMS, SMSPWROFF, USBD, SMSDev9 as ABI reference) | **Inventory by framing**; not line-audited, by design. |
| Assembly (SMS_DSP_MMI.S, SMS_DSP_FFT.S, SMS_DSP_QPel.S, SMS_MP123Core.S, SMS_GS_0.S, SMS_IPU_0.s, SMS_RC_0.s, SMS_DMA_0.s, SMS_Timer.s, SMS_Utils.S, libmpeg_core.S, About_Data.s; inline asm in SMS_EE.c/SMS_GS_1.c/SMS_PgInd.c/SMS_VideoBuffer.c/SMS_DSP.c) | **Inventory by name** — named here as such, not counted as read and not counted as a gap. |
| Host tooling (tools/, sms-converter) | SMS-019/020; out of the EE/IOP frame. |

**Verdict:** Simple-Media-System **is exhausted at source level in this environment.** Round 6 closed the exact residue round 5 named — the 2,074 lines of EE glue and the DSP kernel range — and with it, every EE C source file in the repo has now been read end-to-end at the pin `e65ced39a27e` across rounds 1–6, and every locally owned IOP source has been line-audited. The round's yield (10 findings, including a startup-reachable history-loader OOM/NULL pair and an error path in `DSP_MDCTInit` that has never once executed) shows the residue was worth the read, and its shape — single-line defects, duplicated loader blocks, latent contract breaks, no new defect *families* — is the signature of a swept tree. What remains is explicitly **not unread source**: (a) the *not-source residue*, by the audit's standing framing — vendored codec decode internals (provenance- and allocation-audited only), the pristine-upstream IOP trees, and all assembly, inventoried by name in the table above; (b) the *un-auditable-here* classes — there is no PS2 build in this VM (no PS2SDK) and no hardware, so no finding in any round is build- or hardware-verified, every performance magnitude is structural, and every runtime-dependent consequence is labelled INFERRED where it appears. A round 7 for SMS is productive only after a pin move; on this pin, further reading would be re-reading.

*End of round 6 — Simple-Media-System. 10 new findings (SMS-093…102), 2 location-completing revisions (SMS-057, SMS-068). SMS six-round total: **102 findings, SMS-001…102 contiguous.***
