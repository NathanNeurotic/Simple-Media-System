# hunt-file-io

Read-only hunt of the file I/O layer (src/SMS_FileContext.c, SMS_FileDir.c, SMS_CDVD.c, SMS_CDDA.c, SMS_RingBuffer.c, SMS_FileMapping.c, plus the one STIO consumer that turned out to matter, src/SMS_CopyTree.c). Nothing in the repository was modified; no PS2 or emulator run was possible, so nothing here is hardware-accepted.

Result: the fork's new USB read-ahead and synchronous fileXio paths came out clean. Four defects were found elsewhere: two in "Copy to HDD", one in over-range repositioning, one cache-ordering issue in the SMB share list. None maps to an open GitHub issue.

What was run
- Existing host tests (WSL Debian, `setarch -R`): test_stio_usb_readahead, test_stio_refill, test_stio_stream_failure, test_stio_device_reads, test_file_skip, test_file_string, test_io_diagnostics, test_cdda_stream_failure: PASS=8 FAIL=0.
- Randomized stress of the real STIO code extracted from the repo (up to 3 concurrent files, fake fileXio with FatFs-style clamping lseek, per-fd error injection, optional short reads; ops: Read, byte/word getters, Seek, Stream(n), Stream(0), File_Skip, Destroy):
  - With over-range repositioning excluded: ASan+UBSan seeds 1-16 x 20000 ops, ASan+SMS_DIAG with short reads seeds 1-8 x 20000, TSan seeds 1-8 x 6000, TSan+SMS_DIAG with short reads seeds 20-26 x 6000: 0 failures.
  - With over-range repositioning included: 24 of 24 runs fail on current code, 0 of 24 with the proposed two-line fix (finding 3).
- Real SMS_CopyFile + real STIO against a model of ps2sdk fio (one global block mode, one shared result word, fioSync a no-op outside NOWAIT). The model's premises were checked by disassembling fio_sync.o and fio_write.o from libkernel.a in docker ps2dev/ps2dev:v1.0.

Ruled out (no defect found)
- Helper-thread data races, deadlocks and semaphore over-signal in STIO_PfThread / STIO_FillPf / STIO_PfStart / STIO_PfStop (the kernel shim asserts max_count; TSan clean).
- Slot reuse while the reader still holds a slot; teardown with a read in flight; single-owner handoff between files; fallback when read-ahead setup fails.
- Leaks of semaphores, descriptors or buffers on Destroy and on STIO_InitFileContext error paths.
- SIF DMA alignment: every stream buffer is memalign(64, (n+63)&~63) and read-ahead slots do not share cache lines.
- Stale-cache exposure in the read-ahead path: fileXioRead flushes before the DMA and the reader does not touch a slot until signalled.
- Helper stack: estimated about 1.3 KiB used of 4 KiB (estimate from frame sizes, not measured on hardware).
- Short-read handling (loops continue until 0), EOF and error latching in STIO_Read, File_Skip, File_GetString, the inline getters.
- All Stream(pos, n) call sites are correctly paired with Stream(pos, 0) or start from plain mode.
- SMS_CDVD.c and SMS_CDDA.c are byte-identical to upstream SMS 2.9rev4; SMS_FileMapping.c and SMS_RingBuffer.c differ only by register-name macros and SMS_DIAG.

Observed but not reported as findings (upstream legacy or unprovable on host)
- CDDA_InitFileContext dereferences the result of CDDA_InitFileContextInternal without a NULL check; _read_string can leave its output pointer unassigned when the length is 0; unchecked mallocs in CDDA_GetFileList; a roughly 1.2 MB stack array in _find_signature.
- SMS_RingBufferInit leaves ee_sema_t.max_count uninitialized; I could not establish whether the EE kernel enforces it.
- SMS_FileMapping UnMap does not repair m_pNext->m_pPrev when removing the head.
- STIO_Fill issues one extra, harmless read after a short read at EOF.
- The 2 GiB signed-seek limit is already documented. m_Size is 32-bit, so files of 4 GiB or more wrap. fileXioLseek64 exists in the pinned SDK, so a 2-4 GiB lift for fileXio devices looks feasible but cannot be verified without hardware; not proposed.
- Seeking past EOF also leaves any loop that is bounded only by FILE_EOF and reads only with File_GetByte unable to advance (File_GetByte returns 0 without moving on a failed refill). I did not find a concrete reachable hang; the AVI header loop I traced ends in its error path.

Scratch artefacts
- (investigator scratch, not kept)/hunt-file-io/copy_fio.py
- (investigator scratch, not kept)/hunt-file-io/run_copy.sh
- (investigator scratch, not kept)/hunt-file-io/stress_xio.py
- (investigator scratch, not kept)/hunt-file-io/run_fixtrial.sh
- (investigator scratch, not kept)/hunt-file-io/run_many.sh

## #1 Copy to HDD aborts after the first 384 KiB chunk: SMS_CopyFile compares against an lnWrite that fioSync never writes
- issue: 0
- severity: medium
- category: bug
- files: ['src/SMS_CopyTree.c', 'src/SMS_GUIFileCtxMenu.c']
- reproduced: True
- confidence: 0.9
- risk: Very low. One assignment of a value the function already receives; the NOWAIT path is unchanged. Copy to an internal HDD still needs a hardware check, since no console run was available.

### evidence
src/SMS_CopyTree.c:337-365:
  int lnWrite;
  int lnRead = apFile -> Read ( apFile, lpBuf, BUF_SIZE );
  ...
  if (  fioWrite ( lFD, lpBuf, lnRead ) < 0  ) { error: lfError = 1; break; }
  ...
  fioSync ( FIO_WAIT, &lnWrite );
  if ( lnWrite != lnRead ) goto error;

lnWrite is assigned only by fioSync. Disassembly of fio_sync.o from libkernel.a in docker ps2dev/ps2dev:v1.0 shows fioSync loads _fio_block_mode, compares it with 1 (FIO_NOWAIT) and otherwise returns 0xFFFF29FB (lui v1,0xffff / ori v1,v1,0x29fb / jr ra) before the only store through the result pointer (sw a0,0(s1)).
> Review note: this describes the baseline c275476, where only fioSync assigns lnWrite. The branch head already assigns fioWrite to lnWrite (src/SMS_CopyTree.c), so the proposed assignment is applied there.

fio is put in NOWAIT mode only by STIO_Stream for a legacy-fio source larger than one 384 KiB buffer (SMS_FileContext.c:1915). So for every fileXio source (mass/USB, MX4SIO, mmce, smb, udpfs: STIO_Stream returns at lines 1873-1879 without touching the block mode), for any fio file of 384 KiB or less, and by the same logic for CDDA sources, lnWrite is stack garbage and the copy takes the error path after the first chunk.

Upstream 2.9rev4 called a three-argument per-descriptor fioSync ( lFD, FIO_WAIT, &lnWrite ) from its own SDK; the port to the two-argument ps2sdk call changed the semantics.

Reproduced on host with the real SMS_CopyFile and real STIO code against a model of ps2sdk fio, built with -ftrivial-auto-var-init=pattern (scratch run_copy.sh, variant cur):
  mass:/x.avi (USB, read-ahead)  size  1000000 -> ret 3, dest   393216 bytes  ** BAD **
  mass:/x.avi (MX4SIO, sync)     size  3000001 -> ret 3, dest   393216 bytes  ** BAD **
  cdfs:/x.avi (fio)              size     1000 -> ret 3, dest     1000 bytes  ** BAD **
All 16 fileXio cases and both fio cases of 384 KiB or less return 3.

The single-file path ignores the result (SMS_GUIFileCtxMenu.c:102: SMS_CopyFile ( lpPath, lpFileCtx, 0, NULL, NULL );), so the user gets a silently truncated file. The tree path reports error 3. The menu item is offered for any non-HDD medium when an HDD partition is mounted (SMS_GUIFileMenu.c:1115-1117). Not run on a PS2; on hardware the outcome depends on a stack slot that would have to equal the byte count by chance.

### fix
src/SMS_CopyTree.c, SMS_CopyFile, line 342. Take the synchronous result from fioWrite itself.

Before:
    if (  fioWrite ( lFD, lpBuf, lnRead ) < 0  ) {
After:
    if (  ( lnWrite = fioWrite ( lFD, lpBuf, lnRead ) ) < 0  ) {

Leave lines 363-365 as they are. In WAIT mode fioSync is a no-op and lnWrite keeps the synchronous byte count, so the short-write (disk full) check still works. In NOWAIT mode fioWrite returns 0 and fioSync then overwrites lnWrite with the completed count, as today.

With only this change the model run (variant fixcopy) gives OK for all 16 fileXio cases and the small fio cases. The remaining fio failures belong to the next finding.

### test
Add tools/test_copy_file.py modelled on the scratch harness (investigator scratch, not kept)/hunt-file-io/copy_fio.py: compile the real SMS_CopyFile and STIO code with -ftrivial-auto-var-init=pattern against a fio stub whose fioSync returns -E_LIB_UNSUP without writing unless the block mode is NOWAIT, copy files of 1000, 393216, 393217 and 1000000 bytes from a mass: source and a cdfs: source, and assert return 0 and a byte-identical destination.

### VERDICT diagnosis
{
 "real": true,
 "fix_sound": true,
 "reproduced": true,
 "reasoning": "I tried to refute this and could not. The defect is real in the current tree and the one-line fix is correct. Not run on a PS2.\n\n**What I checked**\n\n1. **Source (src/SMS_CopyTree.c:337-365).** `int lnWrite;` is declared uninitialised inside the loop. The `fioWrite` result is only tested `< 0`. The only possible writer of `lnWrite` is `fioSync ( FIO_WAIT, &lnWrite )` at line 363, followed by `if ( lnWrite != lnRead ) goto error;`.\n\n2. **Compiled PS2 object (baseline build, `ee-objdump` of obj/SMS_CopyTree.o).** The frame is 320 bytes and `lnWrite` lives at `128(sp)`. The only two references to that slot in the whole function are `addiu a1,sp,128` (argument to `fioSync`) and `lw v1,128(sp)` right after the call. Nothing in `SMS_CopyFile` ever stores to it.\n\n3. **Pinned toolchain `fioSync` (libkernel.a `fio_sync.o` from ps2dev/ps2dev:v1.0).** It loads `_fio_block_mode`, compares with 1, and otherwise returns `0xFFFF29FB` before reaching the only store, `sw a0,0(s1)`. SMS.map confirms SMS.elf links this object, with no override in the repo or libpatches.\n\n4. **`fioInit` and `fioWrite` from the same library.** `fioInit` sets `_fio_block_mode` to 0 (WAIT). `fioWrite` returns `_fio_recv_data[0]` in WAIT mode and 0 in NOWAIT mode, so the model used in the harness is faithful.\n\n5. **Block mode.** The only `fioSetBlockMode ( FIO_NOWAIT )` is src/SMS_FileContext.c:1915, on the legacy-fio path and only when the first read filled the buffer and more data remains. The fileXio branch returns earlier. The mode is reset to WAIT on destroy (line 1317) and on `Stream(\u2026,0)`. So for fileXio sources, fio files of 384 KiB or less, and CDDA, the mode is WAIT at line 363 and `lnWrite` is never written.\n\n6. **Callers.** `_copy2hdd_handler` (src/SMS_GUIFileCtxMenu.c:102) ignores the return value. `CopyFileCB` stores it in `m_Error`, and 3 is reported as \"not enough space\". The menu action is reachable for any non-HDD medium when a partition is configured (src/SMS_GUIFileMenu.c:1115-1117).\n\n7. **Upstream.** 2.9rev4 has the three-argument `fioSync ( lFD, FIO_WAIT, &lnWrite )`. The two-argument form came in with commit d22bcbd (2016-01-20, \"Make it compilable with latest PS2SDK\"). So this is a regression against 2.9rev4, but inherited from the ps2homebrew SDK port rather than introduced by this fork's recent work.\n\n**Hardware outcome does not depend on luck for the common case.** The stale slot is constant for the whole call, since callees use lower stack addresses. A file larger than 384 KiB that is not an exact multiple has a full chunk (393216) and a shorter last chunk, and one constant cannot equal both. So for those sources the function must return 3 whatever the slot holds. Truncation after the first chunk happens unless the slot happens to hold 0x60000.\n\n**Host reproduction (my own run, scratch label verify-hunt-file-io).** I ran the reporter's harness (real `SMS_CopyFile` plus real STIO code against the fio model) with both `-ftrivial-auto-var-init=pattern` and `=zero`, under ASan/UBSan.\n\n| Variant | fileXio sources (16 cases) | fio source of 384 KiB or less | fio source over 384 KiB |\n|---|---|---|---|\n| current | all ret 3; dest 393216 bytes for sources over 384 KiB (e.g. mass USB 1000000, MX4SIO 3000001) | ret 3 (cdfs 1000 and 393216) | separate defect, see below |\n| fixcopy | all ret 0, byte-identical | ret 0 | unchanged |\n\nThe remaining cdfs failures above 384 KiB (ret 0 with an over-long destination, e.g. 1000000 -> 1179648) are the separate NOWAIT shared-result defect. This fix neither causes nor cures them.\n\n**Fix soundness.** `( lnWrite = fioWrite (\u2026) ) < 0` is minimal. In WAIT mode `fioWrite` returns the synchronous byte count and `fioSync` leaves it alone, so the short-write check still works. In NOWAIT mode `fioWrite` returns 0 and `fioSync` overwrites `lnWrite` as before. The same `fioWrite(...) == size` idiom is already used at src/SMS_GUIFileCtxMenu.c:167.\n\n**On the proposed test.** A host test is optional. If added, it must assert only the fileXio cases and fio cases of 384 KiB or less, unless the NOWAIT finding is fixed in the same change.\n\nFiles: src/SMS_CopyTree.c, src/SMS_GUIFileCtxMenu.c, src/SMS_FileContext.c. Scratch evidence: (investigator scratch, not kept)/verify-hunt-file-io/fio_sync.dis, (investigator scratch, not kept)/verify-hunt-file-io/fio_write.dis, (investigator scratch, not kept)/verify-hunt-file-io/copytree.dis, (investigator scratch, not kept)/verify-hunt-file-io/run.sh.",
 "corrected_fix": ""
}

## #2 Copy to HDD from CD/DVD/host appends up to 384 KiB of junk: the queued write overwrites the read-ahead's result that STIO_FillStm collects
- issue: 0
- severity: medium
- category: bug
- files: ['src/SMS_FileContext.c', 'src/SMS_CopyTree.c']
- reproduced: True
- confidence: 0.8
- risk: Very low for playback: the clamp only takes effect when the reported count exceeds the bytes remaining, which a correct read never does. Hardware behaviour of CD/DVD-to-HDD copy is unverified.

### evidence
For a legacy-fio source larger than 384 KiB, STIO_Stream leaves fio in NOWAIT mode with a read-ahead queued (SMS_FileContext.c:1915-1918). STIO_FillStm later collects that read's byte count with
  if (  fioSync ( FIO_WAIT, &lnRead ) == FIO_COMPLETE  ) { ... apCtx -> m_pEnd = apCtx -> m_pPos + lnRead;   (lines 1748-1759)

ps2sdk fio keeps one result word for all calls. Disassembly of fio_write.o and fio_sync.o in the pinned SDK shows both reference the same _fio_recv_data and _fio_block_mode. SMS_CopyFile issues fioWrite between the queued read and the next fill, so fioSync hands STIO_FillStm the write's count (always a full 393216) in place of the read's. On the last, partial buffer STIO therefore publishes a full buffer; SMS_CopyFile writes it and the destination is rounded up to the next multiple of 384 KiB with stale or uninitialized buffer contents. No error is raised.

Upstream 2.9rev4 used a per-descriptor fioSync ( lpPriv -> m_FD, FIO_WAIT, &lnRead ), where this could not happen.

Reproduced on host with the real code against the fio model (scratch run_copy.sh, variant cur; identical in fixcopy):
  cdfs:/x.avi (fio)  size   500000 -> ret 0, dest   786432 bytes  ** BAD **
  cdfs:/x.avi (fio)  size  1000000 -> ret 0, dest  1179648 bytes  ** BAD **
  cdfs:/x.avi (fio)  size  3000001 -> ret 0, dest  3145728 bytes  ** BAD **
  cdfs:/x.avi (fio)  size   786432 -> ret 0, dest   786432 bytes  OK   (exact multiple)
The first size bytes of each copy are correct; only the tail is extra.

The model rests on SDK code reading plus disassembly, not on a PS2 run. Realistic sources are cdfs: data discs and host:.

### fix
src/SMS_FileContext.c, STIO_FillStm, insert one line after the existing range check that ends at line 1755 and before `lOldBuf = apCtx -> m_CurBuf;` (line 1756):

   if ( ( unsigned int )lnRead > apCtx -> m_Size - apCtx -> m_CurPos ) lnRead = apCtx -> m_Size - apCtx -> m_CurPos;

m_CurPos < m_Size is already guaranteed by the enclosing if at line 1739. A stream buffer can never legitimately hold more than the bytes left in the file, so playback is unaffected.

With this line plus the lnWrite fix from the previous finding, all 24 model cases copy byte-exact (variant fixboth, exit 0).

Limit of this fix: the read-ahead's own result is still lost during a copy, so a failed or short final read on the source is not detected. Before the fix that case ended as a silently truncated copy, so detection does not get worse. Detecting it properly would need the destination write on a separate RPC channel (fileXio), which I did not propose because it cannot be validated without hardware.

### test
Same test file as the previous finding (tools/test_copy_file.py from scratch copy_fio.py): the cdfs: cases of 393217, 500000, 1000000 and 3000001 bytes must give a destination of exactly the source size. Optionally add a direct STIO case to tools/test_stio_stream_failure.py: complete a foreign fio op between Stream and the final fill and assert m_CurPos never exceeds m_Size.

### VERDICT diagnosis
{
 "real": true,
 "fix_sound": true,
 "reproduced": true,
 "reasoning": "I could not refute it. The defect is real in the current tree and the one-line clamp fixes it. Evidence is disassembly of the pinned SDK plus a host model; nothing was run on a PS2.\n\n**What I read**\n- `src/SMS_FileContext.c:1913-1919` (`STIO_Stream`): for a non-fileXio file larger than one buffer it sets `fioSetBlockMode ( FIO_NOWAIT )` and queues a read-ahead.\n- `src/SMS_FileContext.c:1748-1759` (`STIO_FillStm`): takes the byte count from `fioSync ( FIO_WAIT, &lnRead )` and publishes `m_pEnd = m_pPos + lnRead`. The only bound is `lnRead <= m_BufSize`.\n- `src/SMS_FileContext.c:1337-1376` (`STIO_Read`): no clamp to `m_Size`.\n- `src/SMS_CopyTree.c:334-365` (`SMS_CopyFile`): `Stream ( apFile, 0, 96 )`, then per loop `Read` of `BUF_SIZE` (4096*96, equal to the stream buffer), `fioWrite`, `fioSync ( FIO_WAIT, &lnWrite )`. So exactly one write completes between every queued read-ahead and the fill that collects it.\n- Callers: `SMS_GUIFileCtxMenu.c:85-102` (single file) and `SMS_CopyTree.c:130-134` (tree) both use `STIO_InitFileContext`. `lXio` (`SMS_FileContext.c:2011`) covers only mass/smb/mmce/udpfs, so data CD (`g_CMedia` 1), DVD (5) and host: (4) take the legacy fio path. USB, SMB and MMCE are not affected.\n\n**SDK check (pinned `ps2dev/ps2dev:v1.0` `libkernel.a`, disassembled in docker)**\n- `fio_read.o` (RPC func 2) and `fio_write.o` (func 3) both pass `_fio_recv_data` with size 4 as the receive buffer and both wait on the same `_fio_completion_sema`.\n- `fio_sync.o` returns `*(int*)UNCACHED(_fio_recv_data)`.\n- `__fio_internals.o` has a single `_fio_recv_data` symbol; an existing `SMS.map` shows SMS links it from `libkernel.a`.\n- So the write's count (the previous `Read` size, always 393216 in stream mode) is what `STIO_FillStm` sees. On the last partial buffer it publishes a full buffer, and the copy is rounded up to a multiple of 384 KiB with stale or uninitialised bytes.\n\n**Host reproduction (my own harness, not the reporter's)**\n- `(investigator scratch, not kept)/verify-hunt-file-io/diag/vcopy.py` and `vrun.sh` compile the real `STIO_*` and `SMS_CopyFile` with ASan/UBSan against a threaded fio model: an \"IOP\" thread, io and completion semaphores, one result word, random jitter.\n- Current code:\n\n| Source size | Return | Destination size |\n|---|---|---|\n| 393217 | 0 | 786432 |\n| 500000 | 0 | 786432 |\n| 1000000 | 0 | 1179648 |\n| 3000001 | 0 | 3145728 |\n| 786432 | 0 | 786432 (OK) |\n| 1179648 | 0 | 1179648 (OK) |\n\n- The first `size` bytes are correct in every case.\n- With only the proposed clamp, every size above 384 KiB copies byte-exact.\n- Sizes of 384 KiB or less still return 3 in both variants. That is the separate uninitialised-`lnWrite` finding, not this one.\n\n**Fix check**\n- The clamp bound is exact: `m_CurPos` is only ever advanced by consumed bytes (I grepped every writer), so at fill time it equals the new buffer's file offset. The enclosing `if` already guarantees `m_CurPos < m_Size`, and the same function already relies on `m_CurPos + lnRead < m_Size`.\n- A genuine read can never exceed the remaining bytes, so playback is unchanged.\n- Private patched tree: the 10 host tests that compile `SMS_FileContext.c` pass, and the PS2 build with the pinned toolchain produces `SMS.elf` with no new warnings.\n- A 4-line case 15 added to `tools/test_stio_stream_failure.py` fails (SIGABRT) without the clamp and passes with it in the legacy, BDM and Win32 variants. That is a smaller regression test than the proposed new `tools/test_copy_file.py`. The case (also bump `range(1, 15)` to `range(1, 16)`):\n  ```c\n  actual_size=ctx.m_Size=4096+100;\n  assert(STIO_Stream(&ctx,0,1)==4096);consume(&ctx);completed_length=4096;\n  assert(ctx.Fill(&ctx)>0 && ctx.m_pEnd-ctx.m_pPos==100);\n  ```\n\n**Corrections to the report**\n- The claim that upstream 2.9rev4 \"could not\" hit this is unsubstantiated. Upstream's `STIO_Read`, `STIO_FillStm` and `SMS_CopyFile` logic is identical; only the `fioSync` call has an extra fd argument, and that 3-argument form's implementation is not in the upstream repo. The 2-argument form entered the fork in the first porting commit, 78b5ed0 (\"Removed first fioSync's argument since it isn't used anymore\"). Treat this as a long-standing bug, not a demonstrated fork regression.\n- Severity is low to medium: the tail is padded by up to 384 KiB of junk and the leading data is intact.\n\n**Residual limit (acknowledged by the reporter, not a regression)**\nDuring a copy the read-ahead's own result is still lost, so a short or failed source read mid-copy remains undetected, exactly as before the fix.",
 "corrected_fix": ""
}

## #3 Seek or Stream(pos,0) past end of file leaves the logical cursor and the device descriptor out of step
- issue: 0
- severity: low
- category: bug
- files: ['src/SMS_FileContext.c']
- reproduced: True
- confidence: 0.7
- risk: Low, with one behaviour change to weigh: after change 1 a parser that ignores the -1 keeps reading from the old position with a working descriptor, where today it runs into EOF once the buffer drains. A malformed file could therefore take longer to be rejected by a loop that walks chunk headers. Devices whose lseek does not clamp (cdfs, smb) change from 'cursor moves past EOF' to 'seek refused'. Not verified on hardware.

### evidence
STIO_Seek (SMS_FileContext.c:1378-1400) calls the device before validating the target:
  if ( lpPriv -> m_fXio ) lPos = fileXioLseek ( lpPriv -> m_FD, aPos, SEEK_SET ); else lPos = fioLseek ( ... );
  if ( lPos == 0xFFFFFFFFU || lPos != aPos ) return -1;
For aPos > m_Size on a FAT volume the driver clamps to the file size (ps2sdk bdmfs_fatfs fs_lseek -> f_lseek on a read-only file, read from ps2sdk master; the repo ships a prebuilt irx whose version I could not verify). STIO_Seek then returns -1 with m_CurPos and the buffer unchanged, but the descriptor now sits at EOF, so the stream ends as soon as the buffered bytes run out.

STIO_Stream with anBlocks == 0 (lines 1846-1847) discards the buffer and then returns without repositioning:
  apCtx -> m_pPos = apCtx -> m_pEnd = apCtx -> m_pBuff[ 0 ];
  if ( aStartPos > apCtx -> m_Size ) return 0;
The descriptor is left wherever the buffering or read-ahead had taken it, while m_CurPos still names the old position, so the next read returns bytes from the wrong offset.

STIO_SeekPf already guards this case (line 1801: if ( aPos > apCtx -> m_Size ) return -1;). Upstream set the cursor to aPos unconditionally, so a past-EOF seek simply read as EOF.

Reachable from container parsers that seek by sizes taken from the file, for example SMS_ContainerAVI.c:715 lpFileCtx -> Seek ( lpFileCtx, lpFileCtx -> m_CurPos + lSize ); on a truncated or malformed file, and via STIO_SeekStm during fio streaming.

Reproduced on host with the real STIO code and a clamping fake lseek (scratch run_fixtrial.sh, 12 seeds each under ASan+UBSan x 20000 ops and TSan x 5000 ops): current code fails 24 of 24, for example
  read: want 36287 exp 36287 got 2771 pos 5421 size 196608 pf 0
  read: want 34427 exp 34427 got 0 pos 0 size 700001 pf 0
With the fix below: failures=0 of 24.

Practical effect observed by reading: a malformed file is misparsed or hits early EOF instead of failing cleanly. No crash or hang was demonstrated.

### fix
src/SMS_FileContext.c, two lines.

1. STIO_Seek, immediately after the declarations at lines 1380-1381:
   if ( aPos > apCtx -> m_Size ) return -1;
   This matches STIO_SeekPf and keeps the descriptor where it was.

2. STIO_Stream, anBlocks == 0 branch, line 1847.
   Before:
     if ( aStartPos > apCtx -> m_Size ) return 0;
   After:
     if ( aStartPos > apCtx -> m_Size ) { apCtx -> Seek ( apCtx, apCtx -> m_CurPos ); return 0; }
   Seek is STIO_Seek at that point, so the descriptor is put back at the logical cursor after the buffer was dropped.

### test
Extend tools/test_stio_stream_failure.py (or test_file_skip.py) with a device stub whose lseek clamps to the file size: (a) Seek(size+1) in plain mode returns -1 and a following Read returns the correct bytes through to EOF; (b) Stream(size+1, 0) after a partial buffered read returns 0 and the following Read returns the bytes at m_CurPos. The scratch generator (investigator scratch, not kept)/hunt-file-io/stress_xio.py exercises both with the model check.

### VERDICT

Not independently checked (below the verification cut-off, or its checker did not finish).

## #4 SMB share list: D-cache is flushed after the IOP DMA instead of before it
- issue: 0
- severity: low
- category: bug
- files: ['src/SMS_FileDir.c', 'iop/smbman-sms/src/smb_fio.c']
- reproduced: False
- confidence: 0.5
- risk: Low. Same call, same range, earlier position; this is the ordering upstream and ps2sdk use. Needs one SMB browse on a console to confirm, which I could not do.

### evidence
src/SMS_FileDir.c:345-368:
  lpShare = ( ShareEntry_t* )memalign (  64, lMaxEnt * sizeof ( ShareEntry_t )  );
  lIn.EE_addr = lpShare; lIn.maxent = lMaxEnt;
  lnShares = fileXioDevctl (  g_pSMBS, SMB_DEVCTL_GETSHARELIST, &lIn, sizeof ( lIn ), NULL, 0  );
  /* ... the EE wrote nothing here so the writeback half is a no-op */
  if ( lnShares > 0 ) SyncDCache (  lpShare, ( char* )lpShare + lMaxEnt * sizeof ( ShareEntry_t )  );

smbman writes the entries with sceSifSetDma straight into EE_addr (iop/smbman-sms/src/smb_fio.c, DMA_sendEE). SyncDCache is writeback plus invalidate. The comment's premise holds only if no dirty cache line covers the block, but the block is recycled heap memory: free() writes its list pointers into the first bytes of a freed chunk, and the previous call of this same code frees a block of the same size. If such a line is still dirty when the post-DMA SyncDCache runs, it is written back over the freshly delivered first entry.

Upstream flushed before the call (upstream SMS_FileDir.c:282, SyncDCache on the buffer ahead of the share enumeration), and ps2sdk's own fileXioRead flushes before its RPC for the same reason.

Code reading only. Cache contents cannot be modelled on host and no console was available, so I have not observed a corrupted share name. Whether a dirty line survives between two visits to the share list is unlikely but not excluded.

### fix
src/SMS_FileDir.c: move the flush ahead of the devctl and make it unconditional.

Before the `lnShares = fileXioDevctl ( ... )` call (inside the existing BDM block or just above it):
    SyncDCache (  lpShare, ( char* )lpShare + lMaxEnt * sizeof ( ShareEntry_t )  );

Delete the post-call line 368 (`if ( lnShares > 0 ) SyncDCache ( ... );`) and correct the comment at 364-367: after a pre-call writeback+invalidate no line of the block is cached, the devctl is synchronous, and nothing touches the block until it returns, so the reads that follow fetch the DMA'd data.

### test
Not testable on host. On hardware: open the SMB share list repeatedly (leave and re-enter) against a server with several shares and confirm the first entry's name stays intact.

### VERDICT

Not independently checked (below the verification cut-off, or its checker did not finish).

