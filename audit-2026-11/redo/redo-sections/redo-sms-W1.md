# SMS Blind Redo — W1: EE Container / Demuxer Parsers

Pin: `e65ced39a27eee8d996e9257e21594d1cac1c93d` (clone `~/workspace/audit-clones-2026-11-13/Simple-Media-System`, read-only).
Rule applied throughout: LESS, NOT MORE — remove / simplify / consolidate / fix; nothing new is proposed except where a finding's Justification says why the addition is a net win.

---

## (a) Coverage note — every file read in full, with line counts (by `wc -l`)

Primary scope — sources (6,206 lines total):

| File | Lines |
|---|---|
| src/SMS_Container.c | 393 |
| src/SMS_ContainerAAC.c | 223 |
| src/SMS_ContainerAC3.c | 135 |
| src/SMS_ContainerASF.c | 723 |
| src/SMS_ContainerAVI.c | 1,189 |
| src/SMS_ContainerFLAC.c | 151 |
| src/SMS_ContainerJPG.c | 94 |
| src/SMS_ContainerM3U.c | 532 |
| src/SMS_ContainerMOV.c | 1,551 |
| src/SMS_ContainerMP3.c | 145 |
| src/SMS_ContainerMPEG_PS.c | 942 |
| src/SMS_ContainerOGG.c | 128 |

Primary scope — headers (438 lines total): include/SMS_Container.h (115), include/SMS_ContainerAAC.h (28), include/SMS_ContainerAC3.h (28), include/SMS_ContainerASF.h (28), include/SMS_ContainerAVI.h (27), include/SMS_ContainerFLAC.h (28), include/SMS_ContainerJPG.h (36), include/SMS_ContainerM3U.h (35), include/SMS_ContainerMOV.h (29), include/SMS_ContainerMP3.h (28), include/SMS_ContainerMPEG_PS.h (27), include/SMS_ContainerOGG.h (29).

Supporting reads used only to verify a mechanism (not audited as scope): include/SMS_FileContext.h (143 — `File_Get*`/`Seek`/`Read` signatures, `FILE_EOF`), include/SMS_FLAC.h (37 — `FLACData` layout), include/SMS.h (`SMS_AudioInfo`, `SMS_Realloc` decl.), src/SMS_FileDir.c:87-185 (`SMS_ContID`/`SMS_SubContID` return values), src/SMS_EE.c:45-53 (`SMS_Realloc`) and :93-130 (`SMS_Rescale`), src/SMS_FileContext.c (`File_Skip`, `STIO_Seek`), src/SMS_AC3.c:374-415 (`AC3_SyncInfo` frame-size range), src/SMS_Codec.c:132-149 (`SMS_CodecDestroy` is NULL-safe).

---

## (b) Candidate findings — sorted by benefit, biggest first

### CAND-W1-01 — Unknown extension makes the probe chain call `s_CCreator[-1]`
Category: 6 (bugs/security)
Locations: src/SMS_Container.c:149-167 (test at :152, table read at :158)
Problem: The extension-hint branch in `SMS_GetContainer` is taken when `( lID = SMS_ContID ( path ) )` is non-zero and `lID < 13`. `SMS_ContID` returns **-1** for every unrecognised extension (src/SMS_FileDir.c:124-185, `retVal` initialised to -1) — and -1 is both truthy and `< 13`, so the branch runs with `lCC[0] = s_CCreator[-1]`: an out-of-bounds read of the creator table whose value is then *called* as the first probe. Depending on the word the linker placed before the table, an ordinary file with an unlisted extension (e.g. `.wav`, `.ts`, `.mkv`, no extension) either crashes/wild-jumps or silently gets no probes at all. The `lID == 0` (AVI) case already falls through to the default order, which starts with AVI anyway, so excluding non-positive IDs loses nothing.
Fix: Take the reorder branch only for `0 < lID < 12` (the number of real creators); all other values keep the existing default-order `memcpy`.
Justification: n/a — guard only, nothing added.
Benefit: Removes a wild function-pointer call / silent no-probe path reachable by simply opening a file with an unlisted extension — the most ordinary untrusted input there is.
Effort: S
Risk: Low
Confidence: High (READ — both the test in SMS_Container.c and the -1 return in SMS_ContID were read)

### CAND-W1-02 — Stream/codec/user-data allocations are dereferenced without a NULL check in nine creators
Category: 6 (bugs/security)
Locations: src/SMS_Container.c:349-353 (`SMSContainer_GetWAVHeader`: `malloc` at :351, `Read` into it at :352); src/SMS_ContainerAAC.c:197-218 (callocs :197/:200, `malloc(16)` :212, write through it :218); src/SMS_ContainerAC3.c:109-112 (first derefs follow immediately); src/SMS_ContainerASF.c:157-159 (context calloc + `memset` of it), :212 (codec calloc; stream calloc at :206 *is* checked, codec is not; first codec deref :261); src/SMS_ContainerAVI.c:102-107 (`_NewStream`: stream checked, codec calloc at :107 not; derefed e.g. in `strh` handling) and :1164-1166 (container calloc, then `MYCONT(apCont)->m_RiffEnd` written); src/SMS_ContainerFLAC.c:123-126 and :138-146 (`malloc` :138, struct copy through it :146); src/SMS_ContainerJPG.c:64-84 (sub-container calloc :64, fields written from :70); src/SMS_ContainerM3U.c:308-312 and :366-370 (entry + path `malloc`s, then `strcpy`); src/SMS_ContainerMP3.c:121-124; src/SMS_ContainerOGG.c:105-108
Problem: One mechanism, repeated: `calloc`/`malloc` results are used on the next line with no test. On a 32 MB EE heap that also holds ring buffers and (for ASF/AVI) per-entry index growth, allocation failure during probing is a realistic runtime event, and every site converts it into a NULL dereference (or a `Read`/`strcpy` into NULL) instead of a clean probe rejection. The tree already contains the correct pattern twice — MOV checks both allocations (src/SMS_ContainerMOV.c:202-207) and MPEG-PS checks both (src/SMS_ContainerMPEG_PS.c:795-796) — so this is drift, not style.
Fix: At each site, test the result and fail the probe the way the neighbouring code already does (`return 0`, or ASF's `goto fail`); for the two-step stream+codec sites, free the stream if the codec allocation fails, mirroring src/SMS_ContainerMOV.c:206.
Justification: n/a — checks only, nothing added.
Benefit: Converts a whole class of OOM crashes across nine files into ordinary "cannot open" outcomes; also gives the five near-identical audio creators (AAC/AC3/FLAC/MP3/OGG) one consistent failure shape.
Effort: M
Risk: Low
Confidence: High (READ — every site read; the two checked counter-examples read)

### CAND-W1-03 — AVI index loaders write through `SMS_Realloc`'s result unchecked
Category: 6 (bugs/security)
Locations: src/SMS_ContainerAVI.c:169-176 (`_Load_idx1`), src/SMS_ContainerAVI.c:281-288 (`_LoadODMLIdx`)
Problem: Both loaders do `lpIndices = SMS_Realloc ( lpStm->m_pIdx, &lpStm->m_IdxAllocSize, (m_nIdx+1)*sizeof(_AVIdxEntry) ); lpStm->m_pIdx = lpIndices; lpIndex = &lpIndices[m_nIdx++]; lpIndex->m_fKey = ...`. `SMS_Realloc` (src/SMS_EE.c:45-53) returns `realloc`'s result directly *and has already overwritten `*apSize` with the grown size* — so on failure the caller gets NULL with a size field claiming success, stores NULL over the old table pointer (leaking/losing it), and immediately writes through `&NULL[m_nIdx]`. A large idx1/ODML index on a fragmented 32 MB heap is exactly where `realloc` fails. MOV's index builder already demonstrates the safe pattern (local size variable, test, keep old pointer — src/SMS_ContainerMOV.c:919-929).
Fix: Copy MOV's pattern: grow into a local, return 0 from the loader if it is NULL, and only then publish pointer + count.
Justification: n/a — reuses an in-tree pattern, nothing new.
Benefit: A big-index AVI (long movie, many entries) stops being a crash-on-load when memory is tight; failure degrades to "no index" instead.
Effort: S
Risk: Low
Confidence: High (READ — both call sites and `SMS_Realloc`'s publish-on-failure behaviour read)

### CAND-W1-04 — ASF packet sync divides by a file-controlled `packet_size` that is never validated non-zero
Category: 6 (bugs/security)
Locations: src/SMS_ContainerASF.c:192 (`lpASF->packet_size = lpASF->hdr.max_pktsize`), src/SMS_ContainerASF.c:368 (`( CurPos - data_offset ) % apASF->packet_size`)
Problem: `packet_size` comes straight from the ASF file-properties object (`max_pktsize`, a `File_GetUInt` with no range check), and `asf_read_header` never requires a file-properties object to have appeared at all before the data object ends the header walk. A crafted or truncated ASF with `max_pktsize == 0` (or no file header) reaches `_ReadPacket` → `asf_get_packet`, whose first computation is a modulo by `packet_size` — integer divide-by-zero with a file-controlled divisor.
Fix: Reject the header (ASF's existing `goto fail`) when `packet_size == 0` after the header loop; one test at the single place the value is established.
Justification: n/a — one guard, nothing added.
Benefit: Closes a one-field crash on an untrusted format; also rejects a class of header-less ASF files that could only malfunction later.
Effort: S
Risk: Low
Confidence: High (READ — assignment, absence of any validation, and the modulo site all read)

### CAND-W1-05 — `SMSContainer_DefReadPacket` dereferences a possibly-NULL packet, computes a clamp it never uses, and unallocates from stream 0 regardless of index
Category: 6 (bugs/security)
Locations: src/SMS_Container.c:308-335 (allocation :312-315, dead clamp :320-321, `Read` with full `lPackSize` :322, `SMS_RingBufferUnalloc ( m_pStm[0] ... )` :325-326)
Problem: Three defects in the shared packet reader used by FLAC/MP3/OGG. (1) `AllocPacket` returns NULL when the ring buffer cannot fit `lPackSize + 64` — a routine back-pressure event — and the non-EOF path immediately reads into `lpPkt->m_pData`. The sibling readers (AAC, AC3, ASF, MOV, MPEG-PS) all test their packet pointer; this one doesn't. (2) `lnRead` is computed as the bytes actually remaining in the file and then never used — the `Read` still asks for the full `lPackSize` (behaviour happens to be equivalent because the underlying read returns short, so the variable is pure dead code that invites a future mis-edit). (3) On a zero read it unallocates `lPackSize + 64` from `m_pStm[0]` even though the packet was allocated from `m_pStm[*apIdx]`; today every user sets `m_DefPackIdx = 0`, so this is latent, but the function is written generically and the mismatch corrupts the wrong ring if that ever changes.
Fix: Return -1 when `lpPkt` is NULL; delete the `lnRead` computation and keep the full-size `Read` (or use `lnRead` — one of the two, not both); unallocate from `m_pStm[*apIdx]`.
Justification: n/a — fix is partly a deletion.
Benefit: Removes a crash on ordinary ring-buffer back-pressure in three audio formats and a latent wrong-ring corruption.
Effort: S
Risk: Low
Confidence: High (READ)

### CAND-W1-06 — ASF fragment reassembly assigns `realloc`'s result and the new capacity before checking either
Category: 6 (bugs/security)
Locations: src/SMS_ContainerASF.c:537 (in `_ReadPacket`)
Problem: `lpASFStm->pkt.data = realloc ( lpASFStm->pkt.data, lpASFStm->pkt.alloc = lpASFStm->pkt.size );` — the capacity field is assigned inside the call, so on failure `pkt.data` becomes NULL (the old buffer is lost) while `pkt.alloc` claims the new size. `pkt.size` here is the file-controlled object size (bounded to < 2^24 by src/SMS_ContainerASF.c:433, i.e. up to ~16 MB — a plausible failure on this heap). The later `!pkt.data` guard (src/SMS_ContainerASF.c:565-568) prevents a write through NULL, but the stream is left with a lying capacity and a leaked buffer, and any future edit that trusts `pkt.alloc` becomes an overflow.
Fix: Reallocate into a local; only on success store pointer and capacity; on failure drop the fragment (reset `pkt.size`/`frag_offset`, which the surrounding code already does in its other reset paths).
Justification: n/a — check only.
Benefit: OOM during ASF playback degrades to a dropped object instead of heap-state corruption.
Effort: S
Risk: Low
Confidence: High (READ)

### CAND-W1-07 — Packet-duration and FLAC-duration math divide by file-controlled values that can be zero
Category: 6 (bugs/security)
Locations: src/SMS_Container.c:239 (`SMSContainer_CalcPktFields`: `( pktSize * 8 * m_SampleRate ) / m_BitRate` when `m_FrameSize <= 1`); src/SMS_ContainerFLAC.c:77 (`apBlockSz[5] = lnSamples / m_SampleRate`)
Problem: One mechanism in two places: a divisor taken from file data is used with no zero test. (1) `m_BitRate` is the WAV-header average-bytes-per-second × 8 (src/SMS_Container.c:341, `SMSContainer_GetWAVHeader`) or an esds value — a file that says 0 makes every audio packet whose codec reports `m_FrameSize <= 1` (ASF non-MP3 audio sets `m_FrameSize = 1` at src/SMS_ContainerASF.c:290-292; AVI audio never sets it, so it stays 0) divide by zero while computing duration. (2) A FLAC STREAMINFO block encodes the sample rate in 20 bits; 0 is encodable, `_do_probe` computes the duration division at :77 *before* the acceptance test that requires a non-zero rate (src/SMS_ContainerFLAC.c:96), so a zero-rate STREAMINFO divides by zero during probing itself. On the EE's MIPS core integer divide-by-zero does not trap — it yields an undefined result — so the observable failure is garbage durations / `m_CurDTS` drift rather than a clean crash (INFERRED impact; the missing guards are READ).
Fix: In `SMSContainer_CalcPktFields`, skip the duration computation when `m_BitRate == 0` (the `lDen && lNum` guard two lines later already establishes the "leave duration unset" outcome for the video case); in `_do_probe`, compute the duration field only when the sample rate is non-zero.
Justification: n/a — two guards, nothing added.
Benefit: Zero-bitrate WAV headers and zero-rate FLAC files stop corrupting every subsequent timestamp of the stream.
Effort: S
Risk: Low
Confidence: High for the missing guards (READ); Med for impact severity (INFERRED, MIPS divide-by-zero semantics)

### CAND-W1-08 — `SMSContainer_SetName` scans backwards for a path separator with no lower bound
Category: 6 (bugs/security)
Locations: src/SMS_Container.c:284-303 (scan loop :292-296)
Problem: `lpSlash` starts at the last character of `m_pPath` and is decremented until it hits `'\\'` or `'/'`, with no stop at the start of the string (and no empty-string case: `strlen - 1` on an empty path starts one byte *before* the buffer). Any path containing no separator is walked off the front of its allocation, and `lpSlash + 1` is then pushed into the playlist as the display name. Reachability is via sub-containers: M3U opens entries through `_open_file` (src/SMS_ContainerM3U.c:88-110), which concatenates `g_CWD` + entry name — an entry opened from a root-level playlist can produce a constructed path whose final component has no separator of its own in the scanned string if the prefix contributes none (INFERRED for that specific path shape; the missing bound itself is READ).
Fix: Stop the scan at `apFileCtx->m_pPath` and treat "no separator" as "whole string is the name" — the outcome the code already produces for a leading separator.
Justification: n/a — bound only.
Benefit: Removes an out-of-bounds read (and a wild playlist pointer) on separator-less paths, for every container that calls `SetName` — which is all of them.
Effort: S
Risk: Low
Confidence: High for the missing bound (READ); Med for reachability via playlist sub-files (INFERRED)

### CAND-W1-09 — A Vorbis file with ≥3 channels returns -1 from the OGG creator and aborts the entire probe chain
Category: 6 (bugs/security)
Locations: src/SMS_ContainerOGG.c:70 (`retVal = lInfo.channels < 3 ? 1 : -1`), src/SMS_ContainerOGG.c:96-128 (creator returns the probe value verbatim), src/SMS_Container.c:199-251 (probe loop breaks on any non-zero; :246-251 destroys the container when the status is negative)
Problem: Every other creator returns 0 for "not mine / cannot handle" and a positive value for success. OGG alone returns -1 for a *positively identified* Vorbis stream whose only fault is having 3+ channels. In `SMS_GetContainer` a negative status is treated as fatal: probing stops (later creators — M4A, FLAC, AAC, MP3, AC3, MPEG-PS, JPG, M3U, MOV — never run) and the container is destroyed. The same -1 is harmless in the M3U sub-probe loop, which tests truthiness and moves on (src/SMS_ContainerM3U.c:485-491), so the two callers of the same value already disagree about its meaning.
Fix: Return 0 from `SMS_GetContainerOGG` for the too-many-channels case (keep the probe's internal -1 if wanted, translate at the creator boundary, where the chain contract lives).
Justification: n/a — value change only.
Benefit: One unsupported-channel-count file can no longer veto every other format's probe; behaviour matches the other eleven creators.
Effort: S
Risk: Low
Confidence: High (READ — return value, creator pass-through, and the fatal-negative handling all read)

### CAND-W1-10 — ASF stream-bitrate table is parsed into one array and applied from a different, never-written one
Category: 1 (dead code)
Locations: src/SMS_ContainerASF.c:140 and :149 (local `lBitRate[128]`, zeroed), :322-330 (STREAM_BITRATE handler writes `lpASF->stream_bitrates[lStmID]`), :346-352 (final loop applies `lBitRate[i]`), :90 (`stream_bitrates` field — written at :328, read nowhere; verified by grep over the file)
Problem: The header reader maintains two bitrate stores. File data lands in `ASFContext.stream_bitrates[]`; the post-header loop that fills in missing codec bitrates reads the local `lBitRate[]`, which nothing ever writes except the initial `memset` to zero — and the assignment is guarded by `if (!lpCodec->m_BitRate)`, so it writes 0 over 0. The whole STREAM_BITRATE path is therefore dead: parsed, stored, never applied. It also costs a 512-byte zeroed array on the EE stack for the duration of the header parse.
Fix: Apply from `lpASF->stream_bitrates[i]` in the final loop (the loop index `i` is already the ASF stream ID, which is how the handler keyed the array) and delete the local `lBitRate[128]` and its `memset`.
Justification: n/a — net deletion (one array, one memset) plus a one-token correction.
Benefit: ASF streams whose WAV header carries no bitrate get the file's declared bitrate (which feeds the duration math of CAND-W1-07), and 512 bytes of stack come back.
Effort: S
Risk: Low
Confidence: High (READ + grep-verified write-only/read-only pairing)

### CAND-W1-11 — AVI ODML super-index loading recurses on file-controlled offsets with no depth bound
Category: 6 (bugs/security)
Locations: src/SMS_ContainerAVI.c:226-330 (`_LoadODMLIdx`; recursive call at :299, target offset from `File_GetULong` at :291-298)
Problem: For a type-0 (super-index) entry, the loader seeks to a file-supplied 64-bit offset and calls itself. Nothing limits depth and nothing remembers visited offsets, so a crafted ODML index whose entries point at each other (or at themselves) recurses until the EE stack is exhausted. Real ODML nesting is two levels (super-index → index), so the unbounded recursion buys nothing. The per-entry index allocation inside the recursion has the CAND-W1-03 defect as well, but that is listed there; this finding is only the recursion.
Fix: Thread a depth counter through the existing `apNum` parameter's call chain (or a second parameter) and stop descending past a small bound (e.g. 4 — twice the legitimate depth); entries beyond it are ignored like other malformed ODML data already is in this function.
Justification: The fix *adds* a depth parameter and one comparison — justified because there is no existing bound of any kind on a recursion whose depth is fully attacker-controlled, and the bound is set well above every legitimate file's depth, so no valid input changes behaviour.
Benefit: A malicious or corrupt AVI index can no longer overflow the stack during load.
Effort: S
Risk: Low
Confidence: Med (mechanism READ — offset source, recursion, absence of any guard; INFERRED that cyclic offsets are accepted, as no visited-set or EOF check distinguishes them)

### CAND-W1-12 — MOV `mdhd` handler dereferences the stream that the M4A `hdlr` handler may already have freed
Category: 6 (bugs/security)
Locations: src/SMS_ContainerMOV.c:297-320 (`_mov_read_hdlr_m4a`: frees codec + stream and NULLs the slot at :311-313, without decrementing `m_nStm`), src/SMS_ContainerMOV.c:224-240 (`_mov_read_mdhd`: `lpStm = m_pStm[m_nStm-1]`, dereferenced with no NULL test)
Problem: On the audio-only (M4A) path, meeting a video `trak` frees its stream in place and leaves a NULL slot that `_mov_read_header` only compacts away later. Most sibling handlers defend against exactly this — `stsd`, `esds`, `stts`, `stsc`, `stsz`, `stco`, `ctts`, `stss` and `elst` all test the slot (the `esds` comment at src/SMS_ContainerMOV.c:429-435 documents the hazard explicitly). `mdhd` does not. Atom order inside `mdia` is dispatched in file order by `_mov_read_dflt` and is not enforced, so a crafted file that places `hdlr` before `mdhd` inside the same `mdia` makes `mdhd` write through NULL. (`tkhd` at :215-223 has the same shape but sits at `trak` level before `mdia`, so it cannot follow its own trak's `hdlr`; it is listed for completeness of the pattern, not as a reachable site.)
Fix: Give `_mov_read_mdhd` the same guard its siblings have: if the slot is NULL, skip the atom body and return.
Justification: n/a — one guard matching eight existing in-file precedents.
Benefit: Closes the one remaining hole in an otherwise systematically defended pattern, on the format (MP4/M4A) whose files are most likely to be reordered or corrupt.
Effort: S
Risk: Low
Confidence: Med (free/NULL and missing guard READ; INFERRED that `mdia` child order is attacker-controllable, based on the order-agnostic dispatcher in `_mov_read_dflt`, src/SMS_ContainerMOV.c:795-860)

### CAND-W1-13 — MOV index builder resets every stream's chunk cursor per chunk, making derived chunk sizing quadratic
Category: 5 (performance, structural)
Locations: src/SMS_ContainerMOV.c:1000-1040 (per-chunk search over all streams' chunk-offset tables; cursor reset loop at :1033-1037)
Problem: In `_mov_build_index`'s per-chunk audio path, when a chunk's size must be derived from neighbouring chunk offsets, the search records where it stopped in each stream's `m_NextChunk` — and the block immediately afterwards resets *every* stream's `m_NextChunk` to 0. The next chunk therefore rescans all offset tables from the beginning: O(chunks² × streams) offset comparisons to build an index that is linear with the cursors kept. The reset also makes the `m_NextChunk` writes inside the search pure waste.
Fix: Delete the reset loop; the cursors are per-stream monotonic because chunks are visited in offset order within this walk (the search itself only ever looks for the first offset *greater* than the current one).
Justification: n/a — pure deletion.
Benefit: Index build for affected audio MP4s (fixed/unknown sample size, no samples-per-frame info) drops from quadratic to linear; on a long file that is the difference between instant and a visible hang at open.
Effort: S
Risk: Med (behaviour depends on the monotonic-visit argument above; needs one real-file test per affected sub-case)
Confidence: Med (reset and search READ; INFERRED reachability — the derived-size branch requires `m_SampleSize <= 1` with no per-sample table and no samples-per-frame data)

### CAND-W1-14 — MOV carries two-to-three parallel copies of the `hdlr`/`stsd`/`esds` parsers that have already drifted apart
Category: 2 (duplication)
Locations: src/SMS_ContainerMOV.c:275-295 (`_mov_read_hdlr`), :297-320 (`_mov_read_hdlr_m4a`), :705-720 (`_mov_read_hdlr_m4ap`); :322-427 (`_mov_read_stsd`), :722-766 (`_mov_read_stsd_m4ap`); :429-485 (`_mov_read_esds`), :768-810 (`_mov_read_esds_m4ap`)
Problem: The full demuxer and the M4A probe each implement the same three ISO-BMFF atoms, differing mainly in where results are stored (a real `SMS_Stream` vs a bare `SMS_AudioInfo`). The copies have demonstrably drifted: the probe's `esds` bounds-checks its descriptor length against the atom (src/SMS_ContainerMOV.c:784-786) while the demuxer's `esds` needed a separate `_tbl_fits` retrofit (src/SMS_ContainerMOV.c:460-470); the demuxer's `stsd` walks child atoms, the probe's doesn't; the probe's AAC-specific config extraction exists only in its copy. Every future fix to one copy must be remembered for the other — the file's own comments show that has already failed once.
Fix: Parameterise the three demuxer handlers by their result sink only where the bodies are truly identical (the `hdlr` subtype test and the `esds` descriptor walk), and leave genuinely different bodies (audio-info extraction) separate; do not build a general atom framework.
Justification: This *consolidates* rather than adds: the net is fewer parser bodies for the same atoms. It is justified only for the identical stretches — a wholesale merge would be a new abstraction and is explicitly not proposed.
Benefit: One place to fix the next `esds`/`stsd` bounds issue instead of two; removes roughly 100 lines of near-duplicate parsing (estimate from the ranges above).
Effort: L
Risk: Med
Confidence: High (READ — all seven functions read in full)

### CAND-W1-15 — MOV/M4A creators select their parser by mutating a shared global parse table at runtime
Category: 3 (overengineering)
Locations: src/SMS_ContainerMOV.c:1529 (`SMS_GetContainerMOV` writes `s_MOVParseTbl[0].m_Func = _mov_read_hdlr`), src/SMS_ContainerMOV.c:1544 (`SMS_GetContainerM4A` writes `s_MOVParseTbl[0].m_Func = _mov_read_hdlr_m4a`), table definition src/SMS_ContainerMOV.c:812-837
Problem: Both creators pass the *same* table to `_GetContainerMOV` after overwriting its first entry to select the `hdlr` behaviour. That forces the table to live in writable RAM (it can never be `const`/ROM), makes the selection depend on whichever creator ran last, and means the table's content at any moment is invisible from its definition. The file already shows the cleaner pattern: the M4A *probe* has its own separate table (`s_M4AProbeTbl`, src/SMS_ContainerMOV.c:839-851).
Fix: Give the full M4A demuxer its own complete parse table differing only in the `hdlr` entry (copy the MOV table, change one line), mark both `const`, and delete both runtime writes; `_GetContainerMOV` already takes the table as a parameter.
Justification: The fix adds one static table (~24 entries) — justified because it *removes* a runtime-mutation mechanism, restores const-ness (ROM instead of RAM on the EE), and eliminates an order-dependence between two creators; net mechanism count goes down, not up.
Benefit: Parse tables become link-time facts; a future second container or probe reordering cannot silently inherit the wrong `hdlr`.
Effort: M
Risk: Low
Confidence: High (READ)

### CAND-W1-16 — ASF index support is scaffolding for a feature that was never built
Category: 8 (half-finished features)
Locations: src/SMS_ContainerASF.c:82-85 (`ASFIndex` struct), :96 (`index_read`), :116 (`index_ptr` in `ASFContext`); container setup src/SMS_ContainerASF.c:695-723 (sets `ReadPacket` and `Destroy` only — no `Seek`, never sets `SMS_CONT_FLAGS_SEEKABLE`)
Problem: The context reserves an index pointer, an index-read flag and an index record type, and nothing in the file (verified by grep) ever allocates, reads, or tests any of them. ASF seeking was apparently planned and abandoned, leaving dead state that suggests to every future reader that an index path exists.
Fix: Delete the struct, both fields, and nothing else. If ASF seeking is ever actually built, it will re-derive what it needs from the spec, not from these three declarations.
Justification: n/a — pure deletion.
Benefit: ~10 lines of misleading state gone; the container's real capability set (sequential read only) becomes obvious from its code.
Effort: S
Risk: Low
Confidence: High (READ + grep-verified zero uses)

### CAND-W1-17 — Write-only state: fields and struct members that are stored and never read
Category: 1 (dead code)
Locations: src/SMS_ContainerAVI.c:43 + :109 (`_AVIStream.m_Idx` — assigned the stream index in `_NewStream`, read nowhere); src/SMS_ContainerASF.c:71-72 + :182/:185 (`ASFMainHeader.create_time`, `play_time` — parsed and stored, read nowhere); src/SMS_ContainerASF.c:187 (`hdr.ignore`, same); src/SMS_ContainerASF.c:64 + :538 (`ASFStream.seq` — written from `packet_seq` on every assembled packet, read nowhere)
Problem: One mechanism: values are computed/stored and never consumed (each verified by grep over its file: the only occurrences are the declaration and the write). They cost struct space (the ASF header fields sit in the heap-allocated context), a per-packet store in ASF's hot path, and — worse — they read as if something depends on them, which discourages touching the surrounding code. Note the `File_Get*` calls that *fill* the ASF header fields must stay (they advance the cursor); only the stores/fields go.
Fix: Delete the four members; replace the ASF header stores with bare `File_GetULong`/`File_GetUInt` calls (return value discarded, as dozens of other skip-reads in the same function already do); delete the `seq` assignment.
Justification: n/a — pure deletion.
Benefit: Small memory/stack saving and, mainly, the structs stop lying about what the demuxers use.
Effort: S
Risk: Low
Confidence: High (READ + grep-verified write-only)

### CAND-W1-18 — `SMS_GetContainer`'s explicit-format path indexes the creator table with an unchecked caller value
Category: 6 (bugs/security)
Locations: src/SMS_Container.c:198 (`} else s_CCreator[ aPrefCont ] ( retVal );`)
Problem: When `aPrefCont >= 0`, the value is used as a direct index into the function-pointer table with no range test. No current caller passes a bad value — the only in-tree explicit callers are M3U's two loops, which pass the seven initialised `m_ContID` constants, and the player passes `0x80000000`, which is negative as an `int` and therefore takes the auto-probe path (all verified by grep over `src/`) — so this is a latent trap for the next caller, not a live bug. It is listed last for exactly that reason.
Fix: Treat an out-of-range `aPrefCont` like the auto-probe case (or return NULL): one range test beside the existing one at :152-154.
Justification: n/a — one test, nothing added.
Benefit: The public entry point's contract stops depending on every future caller remembering an undocumented precondition.
Effort: S
Risk: Low
Confidence: High (READ — call sites enumerated by grep; no live trigger found)

---

## (c) Checked-clean — areas examined, nothing found (one line each)

- MOV sample-table readers (`stts`/`stsc`/`stsz`/`stco`/`ctts`/`stss`, src/SMS_ContainerMOV.c:487-700): entry counts are cross-checked against the declared atom size via `_tbl_fits` and every allocation is tested before its count is published — no unchecked count→malloc path remains there.
- MOV `esds` decoder-specific-info (src/SMS_ContainerMOV.c:460-470): length is `_tbl_fits`-bounded and the `calloc` result is tested before the read — clean.
- MPEG-PS `_fill_audio_parameters` (src/SMS_ContainerMPEG_PS.c:352-470 approx.): PES length is range-checked (16-bit, ≥ per-codec minimum) and the heap buffer allocation is tested — the VLA-to-heap conversion is done correctly.
- MPEG-PS stream creation (src/SMS_ContainerMPEG_PS.c:786-830 approx.): stream count is capped at `SMS_MAX_STREAMS - 1` to preserve the NULL sentinel, and both stream and codec allocations are checked with cleanup — clean.
- AC3 frame handling (src/SMS_ContainerAC3.c:51-97): `AC3_SyncInfo` only returns 0 or a frame size ≥ 128 (verified in src/SMS_AC3.c:374-415), so the `lFrameSize - 8` read-back can never underflow, and the packet pointer is tested.
- AAC ADTS handling (src/SMS_ContainerAAC.c:145-197): frame length is the 13-bit ADTS field (≤ 8191) and is required > 7 before the `lSize - 8` read; sample-rate/channel table indexes are masked to their table sizes and zero entries are rejected — no OOB path found.
- ASF stream-header cap (src/SMS_ContainerASF.c:203): stream creation is refused at `SMS_MAX_STREAMS` *before* allocating, and the `ds_span` deinterleave parameters are validated (non-zero chunk size, exact divisibility) at :284-289 before the deinterleave `memcpy` walk — clean.
- ASF frame-header bounds (src/SMS_ContainerASF.c:425-470): `packet_obj_size` is required in (0, 2^24), `packet_frag_size` is required ≤ remaining packet size, and the fragment write in `_ReadPacket` is range-checked against the assembly buffer at :565-568 — clean.
- AVI `_Load_idx1` count clamp (src/SMS_ContainerAVI.c:126-141): the entry count from the chunk size is clamped to the entries the file's remaining bytes can actually hold before the loop — clean (the allocation *inside* the loop is CAND-W1-03).
- AVI `_Seek` keyframe walk (src/SMS_ContainerAVI.c:1050-1100 approx.): past-the-end is handled asymmetrically on purpose (forward fails, backward lands on the last frame) and the walk terminates at both ends — no OOB index found.
- M3U `_open_file` (src/SMS_ContainerM3U.c:88-110): prefix/slash/name lengths are each checked against the 1024-byte stack buffer with overflow-safe comparisons before the `memcpy`s — clean.
- M3U playlist parsing (src/SMS_ContainerM3U.c:330-420 approx.): lines are read with `File_GetString` into a 512-byte buffer, `#EXTINF` titles are `strcpy`d only from within that same bounded buffer into an equal-size buffer — no overflow found.
- FLAC metadata walk (src/SMS_ContainerFLAC.c:28-105): non-STREAMINFO blocks are skipped by their declared 24-bit length, STREAMINFO is required to be exactly 34 bytes and read with a checked 34-byte read — clean apart from CAND-W1-07.
- OGG probe cleanup (src/SMS_ContainerOGG.c:25-95): `ogg_stream`/`vorbis_info`/`vorbis_comment` are cleared on every exit path including the `goto end` error exits, and the file is re-seeked to 0 — no leak found.
- JPG probe (src/SMS_ContainerJPG.c:38-60): the negative-pointer list convention is tested before dereference and the SOI check consumes exactly the 2 bytes the generic probe loop re-seeks over — no issue found beyond the allocation check in CAND-W1-02.
- SMS_Container.c probe-reorder loop itself (src/SMS_Container.c:158-167): apart from the CAND-W1-01 entry condition, the reorder correctly copies every creator including the NULL terminator for every in-range ID — verified by hand-trace for lID = 1 and lID = 11.
- File-size arithmetic in `SMSContainer_DefReadPacket`'s EOF handling and MPEG-PS `_ReadPacket`'s 16-byte alignment tail (`m_Buff[15]`, remainder provably ≤ 15): both bound-checked by construction — clean.
