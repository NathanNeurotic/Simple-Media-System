# SMS Redo Verification — W6 (RE-SMS-121 … RE-SMS-144)

Verifier: adversarial, refute-first. Clone: `~/workspace/audit-clones-2026-11-13/Simple-Media-System`
at pin `e65ced39a27eee8d996e9257e21594d1cac1c93d` (`git rev-parse HEAD` confirmed, clean tree).
Every "never read / never called" claim was re-grepped over the whole tree; every numeric
claim was re-derived by script/grep, not eyeballed. Byte-identity claims were re-hashed/diffed.

---

## RE-SMS-121 — VERIFIED-DRIFT

Evidence:
- AVI: `src/SMS_ContainerAVI.c:43` `int32_t m_Idx;` (file-local `_AVIStream`), sole write
  `src/SMS_ContainerAVI.c:109` `MYSTRM(retVal)->m_Idx = apCont->m_nStm;`. Whole-tree grep for
  `m_Idx` returns this field only at :43/:109 in the AVI file; all other hits are unrelated
  structs (`SMS_Bitio.h`, `SMS_FileContext.h`, AAC/FLAC/MP123 locals). No read exists.
- ASF header: `src/SMS_ContainerASF.c:71` `create_time`, `:72` `play_time`, `:75` `ignore`;
  writes at `:182`, `:185`, `:187` respectively. Grep of every `hdr.` use in the file shows the
  only fields ever *read* are `flags` (:169,:221), `preroll` (:219), `send_time` (:221) and
  `min_pktsize` (:409) — never `create_time`/`play_time`/`ignore`.
- ASF `seq`: write `src/SMS_ContainerASF.c:538` `lpASFStm->seq = lpASF->packet_seq;` is the only
  use besides the declaration; `packet_seq` itself is read only to be stored there.

Corrected value: the `seq` declaration is at `src/SMS_ContainerASF.c:59`
(`unsigned char seq;`), **not :64** as filed (:64 is `int64_t packet_pos;`). All other cited
lines are exact. Core claim — four write-only members, `File_Get*` calls must stay as
cursor-advancing skip-reads — stands. Fix is a pure deletion, LESS-compliant.

## RE-SMS-122 — VERIFIED

Evidence: `grep -n` in `src/SMS_DTS.c` returns `dts_sample_rates`/`dts_bit_rates` only at their
declarations `:127`/`:134`, and the live copies `sl_SampleRate`/`sl_BitRate` only inside
`_dts_syncinfo` at `:10105`/`:10112`, used at `:10142`/`:10150`. Script extraction of all four
initialiser lists: file-scope and function-local lists are value-identical for both tables
(16 sample rates, 32 bit rates). 48 ints = 192 bytes as filed. No reference anywhere else in
the tree. Deletion fix is safe and net-negative.

## RE-SMS-123 — VERIFIED

Evidence: `src/SMS_AC3_imdct.c:533` `_ac3_imdct_512(sample_t*, sample_t*, sample_t aBias)` and
`:579` `_ac3_imdct_256` (same signature). Script count of `aBias` inside each function
(signature through its `/* end … */` comment) = **1 each — the signature only**; neither body
references it. Call sites in `src/SMS_AC3.c`: `:2401`, `:2432`, `:2434`, `:2475-2476`,
`:2479-2480`, passing `s_AC3Ctx.m_Bias`/`lBias`. Callers apply bias directly, e.g.
`src/SMS_AC3.c:2440` `(lpSamples + 256*i)[j] = lBias;` and `:2426`. Dropping the parameter
touches no behaviour; `m_Bias` field correctly stays.

## RE-SMS-124 — VERIFIED

Evidence: `src/SMS_AC3.c:318-321`:
```c
int s_SAB[ 20 ] __attribute__((aligned(16), section(".sdata"), unused)) = {
 0x38504E62, 0x38504E62, 0x38504E62, 0x38504E62,
 0x3CCC2D41, 0x3CCC2D41, 0x3CCC2D41, 0x3CCC2D41
};
```
Script count of initialisers = **8 exactly** (4 + 4); remaining 12 are implicit zeros.
Sole consumer: grep returns only `:1697` (`la $t0, s_SAB` in `_ac3_mix32toS` asm), which does
exactly two loads — `lq $v0, 0($t0)` (entries 0–3) and `lq $a2, 16($t0)` (entries 4–7) — i.e.
all 8 initialised entries, carrying the **2 distinct values** the finding describes.
`[20]`→`[8]` preserves both loads; saving (20−8)×4 = 48 bytes as filed. (Caution for whoever
applies it: "reads 2" means 2 distinct values / 8 entries — shrinking below `[8]` would break
the `lq` at offset 16.)

## RE-SMS-125 — VERIFIED

Evidence: `src/lzma2.c:10-26` `encode`, `:29-49` `decode`. Whole-tree word grep
(`-w encode` / `-w decode` over `*.c`/`*.h`): in C code, `encode` occurs **only** at its
definition — zero callers anywhere; `decode` occurs only at its definition and at
`src/lzma2.c:68,73,76`, all inside `lzma2_decode_size` in the same file. Neither name appears
in `include/lzma2.h` (header declares only `lzma2_get_uncompressed_size` and
`lzma2_uncompress`). Both have external linkage as filed. Fix accounting checks out:
deleting `encode` plus its comment/blank (lines 9–27) = −19 lines; making `decode` `static`
removes the second collision-prone export without touching its 3 internal calls.

## RE-SMS-126 — VERIFIED

Evidence: generated header `src/SMS_IconsRGBA.c:1-6` says `11 dev 48x48`;
`tools/bake_icons_rgba.py:75` (main comment) and `:97` (header template string) both say 11.
Authoritative counts, script-counted from the tool itself: `BROWSER` = 9, `MISC` = 12,
`DEV` = **13** names (`s_IconUSB … s_IconCDVDERROR`, bake tool lines 30–32).
Consumer agrees: `src/SMS_GUIcons.c:1372` `s_BrowserDeviceIcons[13]`, `:1402` comment says
13 dev, `:1435` `for (i = 0; i < 13; ++i)`. Arithmetic re-derived:
9×4096 + 12×4096 + 13×9216 = **205,824**, which equals `g_RGBA_RawSize = 205824` at
`src/SMS_IconsRGBA.c:1748`; with 11 dev icons it would be 187,392. Comment-only fix in both
the template and the generated file is correct.

## RE-SMS-127 — VERIFIED

Evidence: `src/SMS_Menu.c` is 7 lines total (`wc -l`), entire body is
`GUI_CreateMenu(void) { return 0; }`. Real implementation `src/SMS_GUIMenu.c:403-414`
(callocs `GUIMenu`, installs Render/Cleanup/HandleEvent). Root `Makefile` `EE_OBJS`
(lines 22–47) contains `SMS_GUIMenu.o` and **no** `SMS_Menu.o` (grep `SMS_Menu` in Makefile:
no hits). Live callers (`SMS_GUICmdProc.c:332`, `SMS_GUIFileCtxMenu.c:497`,
`SMS_GUIMiniBrowser.c:155`, `SMS_PlayerMenu.c:238`, `SMS_GUIMenuSMS.c:3029`) all resolve to
the GUIMenu version via `include/SMS_GUIMenu.h:90`. Deleting the stub removes a real
symbol-collision trap at zero cost.

## RE-SMS-128 — VERIFIED

Evidence: `iop/USBD/src/interface.c:216-229` verbatim:
`UsbRegisterAutoloader` (:216), `UsbUnregisterAutoloader` (:221),
`UsbChangeThreadPriority` (:226) — each body is only
`dbg_printf("… stub\n"); return 0;`. They are genuinely exported, not internal dead code:
`iop/USBD/src/exports.tab` has `DECLARE_EXPORT(UsbRegisterAutoloader)` (slot 14),
`DECLARE_EXPORT(UsbUnregisterAutoloader)` (15), `DECLARE_EXPORT(UsbChangeThreadPriority)`
(16), and `iop/USBD/include/usbd.h:249-251` declares all three. Silent-success claim stands;
returning a negative error is the LESS fix as filed.

## RE-SMS-129 — VERIFIED

Evidence (`src/SMS_FileMapping.c`):
- `:270-272` `SMS_FileMappingMap`: `void* retVal = (void*)0x32000000; /* implement VAddr allocation here */` — fixed address, no allocation, as filed.
- `:276` `calloc` result `lpMap` is dereferenced unconditionally from `:280` on
  (`s_pMapChain = lpMap`, then `lpMap->m_Start` at :290) — no NULL check.
- `:294` `memalign` result `lpMap->m_pWindow` is used unchecked at `:295-296` to build
  `m_EntryLo0/1`.
- `:304-337` `SMS_FileMappingUnMap`: head removal at `:315-316` sets
  `s_pMapChain = lpIt->m_pNext` but never clears the new head's `m_pPrev`, which still points
  at the node freed at `:334`; a later non-head removal of that node would write through it
  at `:318`.
- Sole consumer confirmed by tree grep: `SMS_FileMappingMap`/`UnMap` are called only from
  `src/SMS_JPEG.c:1733`/`:1749` (plus init/destroy at :1096/:1170). Fix option (a) — enforce
  single-mapping, check both allocations, fix head `m_pPrev` — is the LESS path and is sound.

## RE-SMS-130 — VERIFIED

Evidence (`iop/SMSUMS/src/ums_fat.c`, `FATFS_Init` at :985): allocation
`:1002` `lpFS = AllocSysMemory(0, sizeof(FATFS) + lSecSize, NULL)` (NULL-checked at :1004).
Early returns bypassing cleanup, verbatim: `:1043`
`if (*(unsigned short*)&apDev->m_pCacheBuf[510] != 0xAA55) return retVal;` and
`:1044-1046` zero-total-sectors `return retVal;` — `retVal` is still 0 there, so both leak
`lpFS`. The cleanup they skip is `:1131` `if (!retVal) FreeSysMemory(lpFS);`, which the
BPB-whitelist-mismatch path does reach (falls through from :1048's `if`). Routing both
returns to :1131 is exactly the filed fix. (The pre-allocation `return 0` at :996 leaks
nothing — allocation has not happened yet.)

## RE-SMS-131 — WEAKENED

Evidence for the core claim (stands): three `hdlr` bodies
(`src/SMS_ContainerMOV.c:275-295` `_mov_read_hdlr`, `:297-320` `_mov_read_hdlr_m4a`,
`:705-720` `_mov_read_hdlr_m4ap`), two `stsd` bodies (`:322-382` demuxer, `:722-766` probe),
two `esds` bodies (`:429-469` demuxer, `:768-809` probe), storing into a real `SMS_Stream`
vs a bare `SMS_AudioInfo`. Real drift exists: probe `esds` bounds-checks with an explicit
test and `return -1` at `:784-785`
(`lLen > apAtom->m_Size - lUsed`), demuxer `esds` instead uses the `_tbl_fits` retrofit at
`:460`; probe-only AAC AudioSpecificConfig extraction at `:789-808` has no demuxer
counterpart; demuxer `hdlr` keeps `m1a ` as audio/MP2 and sets video type, `hdlr_m4a`
*frees* video streams, `hdlr_m4ap` only flags `0x80000000` — three genuinely different
behaviours for one atom.

Why weakened — one filed drift example is false, and two ranges are inflated:
1. **"The demuxer's `stsd` walks child atoms, the probe's doesn't" is refuted by the code.**
   Probe `_mov_read_stsd_m4ap` at `:758-765` contains the same child-walk block as the
   demuxer (`lAtom.m_Size = lSize - …; if (lAtom.m_Size > 8) _mov_read_dflt(…); else if
   (> 0) File_Skip(…)`; demuxer equivalent at `:375-381`). Both walk children.
2. Cited demuxer ranges overstate the bodies: `_mov_read_stsd` is `:322-382`, not
   `:322-427` (:384-427 are the shared `_mp4_read_descr*` helpers and `_mp4_get_id`, used by
   both copies — not part of either copy); `_mov_read_esds` is `:429-469`, not `:429-485`.

True scope: duplication + drift are real (esds bounds strategy, AAC config extraction,
hdlr stream disposition), but the stsd copies are *more* alike than filed, not less — the
 "~100 lines removable" estimate should be treated as an upper bound, and any consolidation
must preserve the probe's child-walk that already exists. Fix direction (parameterise only
truly identical stretches) remains sound and net-deleting.

## RE-SMS-132 — VERIFIED

Evidence: Skin — `src/SMS_GUIDesktop.c:155-317` `DrawSkin` contains three source-level
open→seek-end→capped-malloc→read→verify blocks: CWD/fio (:165-190, cap `lSize <= 0xFFFF0`
at :176, `fioRead != lSize` check at :180), BDM mc/fio (:195-220, same cap/check at
:206/:210), non-BDM `MC_*` (:222-245, cap at :227 with `MC_SeekS`/`MC_ReadS`). Language —
`src/SMS_Locale.c` `SMS_LocaleInit` (:813-; load blocks inside the filed :826-897) repeats
the same three-way split: CWD/fio (:838-848), BDM fio (:871-881), non-BDM `MC_*` (:883-895).
Divergence as filed: locale blocks have **no size cap and discard the read result**
(`fioRead(lF, lpAlloc, lSize);` at :844/:877, return value ignored) — grep confirms `0xFFFF0`
occurs only in `SMS_GUIDesktop.c`, never in `SMS_Locale.c`. (Nuance: BDM/non-BDM are
`#ifdef` alternatives, so two of the three blocks compile per build — the triplication is
at source level, which is where the fix-drift the finding describes happens.) Per-file
helper is a net deletion in both files (≈17 lines × 3 blocks vs one helper + 3 call sites).

## RE-SMS-133 — VERIFIED

Evidence (`iop/SMSTCPIP/src/api_msg.c`): script census `grep -c` = `udp_new()` ×**9**,
`tcp_new()` ×**3** in this one file, in three 4-arm switches — `do_newconn` (function :277,
switch :288-330), `do_bind` (function :387, switch :389-416), `do_connect` (function :455,
switch :457-505) — each arm doing create + `udp_setflags`/`udp_recv` or `setup_tcp`.
Divergence verified by reading all three: `do_newconn` NULL-checks every allocation
(:293-330, `ERR_MEM` on NULL), `do_connect` NULL-checks every allocation (:461-500, with
early `sys_mbox_post` + return), **`do_bind` checks none** (:393-410 — result passed
straight to `udp_setflags`/`udp_recv`/`setup_tcp`). That is exactly the RE-SMS-042 hole,
produced by the triplication. One `create_pcb()` helper (~35 lines replacing ~129 lines of
switch) is a genuine net deletion of roughly the filed ~60 lines.

## RE-SMS-134 — VERIFIED-DRIFT

Evidence (core stands): the eleven headers exist with exactly the filed line counts —
script `wc -l`: AAC 28, AC3 28, ASF 28, AVI 27, FLAC 28, JPG 36, M3U 35, MOV 29, MP3 28,
MPEG_PS 27, OGG 29 = **323 lines total** — and every one `#include`s `SMS_Container.h`
(each at its line 15). Distinct source files including them: 16 (script count over `src/`).

Corrected values:
- Prototypes: script extraction over the eleven stubs finds **19 prototypes, not ≈15** —
  AAC 2, AC3 2, ASF 2, AVI 1, FLAC 2, JPG 1, M3U 1, MOV **3**, MP3 2, MPEG_PS 1, OGG 2.
  The filed "1–2 prototypes each" is wrong for MOV (it carries `SMS_GetContainerMOV`,
  `SMS_GetContainerM4A` and `SMS_M4AProbe`).
- Include sites: total `#include "SMS_Container…"` lines in `src/`+`include/` = 48, but
  only **31** of them include one of the eleven stub headers (36 include lines sit in
  `src/`, 12 in `include/`); "48 sites" conflates central-header includes with stub
  includes. The fix only needs to retarget the 31 stub sites.
- Two stubs are **not** prototype-only, and the filed fix (move prototypes, delete stubs)
  omits them: `SMS_ContainerJPG.h:18-25` defines `typedef struct SMS_ContainerJPEG`
  (used at `SMS_ContainerJPG.c:24,64` and `SMS_JPEG.c:1836`) and `SMS_ContainerM3U.h:18-24`
  defines the seven `SMS_SUBCONTAINER_*` macros (used across `SMS_ContainerM3U.c` and
  `SMS_FileDir.c:99-114`). Both must move to `SMS_Container.h` too, or the deletion breaks
  the build. (The probe prototypes use `SMS_AudioInfo`, defined in `include/SMS.h:245`,
  which `SMS_Container.h` already pulls in via `SMS.h` — no ordering blocker there.)

With those corrections the consolidation is still a net deletion (−11 files, ~300 lines)
and LESS-compliant; as filed it undercounts the work and would break two consumers.

## RE-SMS-135 — VERIFIED

Evidence: table `src/SMS_ContainerMOV.c:811-834` `s_MOVParseTbl[]` (script count: 23 tagged
entries + NULL terminator = 24 lines); `:1529` `SMS_GetContainerMOV` writes
`s_MOVParseTbl[0].m_Func = _mov_read_hdlr;`; `:1544` `SMS_GetContainerM4A` writes
`s_MOVParseTbl[0].m_Func = _mov_read_hdlr_m4a;`; both then call
`_GetContainerMOV(apCont, s_MOVParseTbl)` (:1531/:1546). `_GetContainerMOV` (:1432) already
takes the table as a parameter (`const MOVParseEntry*`), and the separate-table pattern
already exists in-file: `s_M4AProbeTbl` at `:838-851`. The array is non-`const` (writable
RAM) precisely because of the two writes, as filed.
Fix-sanity flag (per instructions): the filed fix **adds more lines than it removes** —
a second ~24-entry table (+~25 lines) against two deleted one-line writes (net ≈ +23
lines). It is justified only on the grounds the finding itself gives (mechanism removal,
`const`/ROM placement, order-independence), not as a line-count saving; it should not be
sold as a deletion.

## RE-SMS-136 — VERIFIED-DRIFT

Evidence (core stands): six explicit rules restate the generic rule — script/grep over
`Makefile`: `SMS_MPEG4.o` (:113), `SMS_MSMPEG4.o` (:116), `SMS_MPEG.o` (:119),
`SMS_H263.o` (:122) each add only the prerequisite `$(EE_INC_DIR)SMS_MPEG.h` to the
identical compile command; `SMS_GS_1.o` (:128) and `SMS_GS_2.o` (:131) add nothing at all.
Deleting the GS rules and replacing the four MPEG rules with one prerequisite-only line is
sound — the file already uses that pattern at :125-126 for `libmpeg.o`/`libmpeg_core.o`.

Corrected values: the generic pattern rule is at **`Makefile:110-111`**
(`$(EE_OBJ_DIR)%.o : $(EE_SRC_DIR)%.c`), **not :125-126** as filed — :125-126 is the
libmpeg grouped-prerequisite rule. The explicit rules span **:113-132**, not :128-147
(:134-147 are the unrelated `%.s`/`%.S` pattern rules and the `$(EE_BIN)` link rule).
Net saving with corrected ranges is still ≈20 lines as filed.

## RE-SMS-137 — VERIFIED

Evidence: `diff iop/SMSDev9/include/dev9.h iop/SMSUdpfs/smap/include/dev9.h` — no
differences; `md5sum` identical (`01ad1498f67f3767c420a6da286d0cc7` both); `wc -l` = 53
lines each, as filed. The load-bearing caveat is real: `tools/build_udpfs.sh:12-22`
documents that the SMAP build deliberately shadows the SDK header with SMS's dev9 **v1.1**
ABI header dropped into `smap/include/` (v1.9 ordinal trap), and
`tools/build_ata_bd_dev9v11.sh` exists for the same ABI skew. Comment-in-each-copy is the
correct LESS fix; deleting a copy would break the include-order trick.

## RE-SMS-138 — VERIFIED

Evidence: `diff AGENTS.md CLAUDE.md` — no differences; `md5sum` identical
(`f93ecc9893762f8ecb6c96f5c85d777b` both); `wc -c` = **1,254 bytes each**, as filed.
Content is the GitHub Attribution Policy in both. Pointer-file fix is sound and minimal.

## RE-SMS-139 — VERIFIED

Evidence (`src/SMS_IPU.c`): body extraction (signature/comment stripped) —
`IPU_Suspend` (:584) body = `DisableDmac(DMAC_I_GIF);`, `IPU_DummySuspend` (:605) body =
identical string; `IPU_Resume` (:590) body = `DMAC->m_STAT = 4; EnableDmac(DMAC_I_GIF);`,
`IPU_DummyResume` (:611) body = identical string. The stale comment is present verbatim:
DummySuspend closes with `} /* end IPU_Suspend */` at :609. Dummy slots are live:
`:844-845` `g_IPUCtx.Suspend = IPU_DummySuspend; g_IPUCtx.Resume = IPU_DummyResume;`
(real slots at :806-807). Pointing the dummy slots at the real functions is
behaviour-identical; signatures match (`static void (void)`).

## RE-SMS-140 — WEAKENED

Evidence for the core claim (stands): `src/SMS_ContainerMOV.c` `_mov_build_index` (:936-1057),
derived-size branch in the audio `else` walk at `:1014-1038`: per chunk, the search
(`:1016-1027`) scans every stream's chunk-offset table starting at `lpMyStm->m_NextChunk`,
records the stop index at `:1023` (`m_NextChunk = k; break;`) — and the loop at
`:1034-1037` then unconditionally (brace-verified: inside the derived-size `else`, outside
the `lChunkSize == INT_MAX` MDAT fallback at :1028-1033) resets **every** stream's
`m_NextChunk = 0`. The next chunk therefore rescans from 0: with interleaved, increasing
chunk offsets the scan-to-first-greater offset is O(position) per chunk, i.e.
O(chunks² × streams) overall, and the in-search cursor writes are pure waste. Reachability
matches the filed inference: outer gate `:954` sends audio streams with no per-sample
table here, and the derived branch requires `m_SampleSize <= 1` (:1010 fails) and no
usable samples-per-frame (:1012 fails).

Why weakened — the filed fix ("delete the reset loop") is **not safe as stated**:
1. Cross-call invariant: `_mov_read_header` calls `_mov_build_index` once per stream
   (`:1124`, loop at :1072). The only `m_NextChunk` writes in the tree are :1023 and :1036
   (grep-verified) — there is no per-call initialisation; streams start zeroed only by
   `calloc`. Today the per-chunk reset is what guarantees all cursors are 0 between calls.
   Deleting it leaves cursors at the last found positions after stream 1's build, so
   stream 2's search starts mid-table and misses earlier offsets greater than its (low,
   restarting) `lCurOffset` — producing oversized, merged chunk sizes. The fix needs a
   one-time cursor reset at the start (or end) of `_mov_build_index` — still a net
   deletion, but not the filed pure deletion.
2. Monotonicity is assumed, not enforced: nothing sorts or validates `m_pChunkOffsets`
   (read raw at `:614-619`); for a crafted file with non-monotonic `stco`, a kept cursor
   can skip a later-listed smaller offset. The current rescan is wasteful but robust to
   that; the linear version is correct only for well-formed (offset-ordered) tables,
   which is the normal ISO-BMFF layout but should be stated as the fix's precondition.

True scope: quadratic behaviour on the derived-size audio path is real; the remedy is
"move the reset from per-chunk to per-build (and document the offset-order assumption)",
not simply "delete the reset loop".

## RE-SMS-141 — VERIFIED

Evidence: `iop/smbman-sms/src/smb_fio.c` `smb_GetShareList` (:991-1046): first enum at
`:1016` `smb_NetShareEnum(UID, TID, &ShareList, 0, 0)` to count, then the loop at
`:1024-1035` calls `smb_NetShareEnum(UID, TID, &ShareList, i, 1)` **once per share**
(filed range :1015-1035 exact). Each call is a full RPC: `smb_NetShareEnum`
(`iop/smbman-sms/src/smb.c:684`) builds a NetShareEnum transaction, awaits
`GetSMBServerReply`, and parses **all** entries in the reply, copying only the
`index`/`maxEntries` window (window logic at smb.c:~792-802: `count < maxEntries &&
i >= index`). So the data for a batched fetch already arrives in the first reply and is
discarded n times. n+1 round trips as filed; the proposed bounded-array fetch uses the
existing window parameters, no protocol change.

## RE-SMS-142 — VERIFIED-DRIFT

Evidence (core stands): `.github/workflows/compilation.yml` step "Host playback and
file I/O regression checks" (:61-120) is a hand-enumerated `run: |` list with no
discovery mechanism; nothing in any workflow fails if a new `tools/test_*.py` is
unreferenced. Script census: `tools/test_*.py` = **59 files**; distinct test files
referenced across all workflows = **59** (complete across workflows, as filed —
`test_converter_publication.py` is referenced only by `sms-converter.yml`,
`test_rolling_release.py` only by compilation.yml's separate step at :59).

Corrected values: the step's list is **58 lines, not ≈64** — lines 63–120 = 57
`test_*.py` invocations plus 1 non-test script (`tools/check_devmenu.py` at :63);
compilation.yml references 58 distinct test files in total (the step's 57 +
`test_rolling_release.py`). Benefit is therefore ≈58 YAML lines, consistent with the
filed "≈60 deleted". Fix-sanity note: a naive `for f in tools/test_*.py` loop in this
step would also pull `test_converter_publication.py` into the EE build workflow and
run `test_rolling_release.py` a second time (it has its own step at :58-59) — the loop
should exclude or the separate steps should be folded in deliberately, otherwise the
"one mechanism" fix silently duplicates two runs.

## RE-SMS-143 — VERIFIED

Evidence: `.gitignore:10-11` = `!/src/lzma2/` and `!/src/lzma2/libxz.a`;
`Makefile:13` `EE_LDFLAGS = … -L$(EE_SRC_DIR)/lzma2`. `ls src/lzma2` — no such directory;
`git ls-files` shows `src/lzma2.c` plus `src/xz_crc32.c`, `src/xz_dec_lzma2.c`,
`src/xz_dec_stream.c`, and `Makefile:45` compiles them directly
(`lzma2.o xz_crc32.o xz_dec_lzma2.o xz_dec_stream.o`). No `libxz.a` exists in the tree.
Both references are inert leftovers as filed; deleting them changes no link result
(a `-L` pointing at a nonexistent directory contributes nothing).

## RE-SMS-144 — VERIFIED-DRIFT

Evidence (core stands): `.github/workflows/test-media.yml` triggers on `push` to
`master` with **no `paths:` filter** (grep confirms `paths:` occurs in `docs.yml` and
`wiki.yml` but nowhere in `test-media.yml`), then runs
`bash tools/gen-test-media.sh` (:37, ffmpeg generation) and uploads
`SMS-test-media.zip` to the `test-media` release with `--clobber`. `docs.yml` is
path-filtered as the filed contrast states. `workflow_dispatch` is present, so the
filed escape hatch for a path-filtered version already exists.

Corrected value: the trigger block is at **`:9-12`** (`on:` :9, `push:` :10,
`branches: [master]` :11, `workflow_dispatch:` :12), not `:12-15` as filed (:14-15 are
the `permissions:` block). The cost claim remains, as the finding itself labels it,
INFERRED from the YAML — no Actions-log measurement was made by the finding or by this
verification.

---

## Mechanism-duplicate pairs inside this range (body-level check)

- **RE-SMS-131 ↔ RE-SMS-135 — related, not the same finding twice.** 135's runtime
  table mutation is the *selection mechanism* that forces the `hdlr` half of 131's
  duplication (two demuxer `hdlr` bodies exist only because one table must serve both
  creators). Fixing 135 (two const tables) does not deduplicate any parser body;
  fixing 131 does not remove the mutation. Both stand independently, but they should be
  landed together — parameterising `hdlr` (131) while the table is still mutated (135)
  leaves the order-dependence intact.
- **RE-SMS-122** is a single mechanism filed once (dead tables that are also duplicates
  of live locals) — no second finding in this range covers the same tables.
- RE-SMS-137 (dev9.h, IOP ABI header) and RE-SMS-138 (AGENTS/CLAUDE, policy text) are
  both byte-identity duplications but of unrelated content/mechanisms — not a pair.
- RE-SMS-133's divergence *is* RE-SMS-042 (outside this range) by the finding's own
  cross-reference — noted, not double-counted here.

## Fix-sanity / LESS-NOT-MORE summary

- Pure deletions, verified net-negative: 121, 122, 123, 124, 125, 127, 130 (routing to
  existing cleanup), 133, 136 (with corrected ranges), 139, 143.
- Net-negative consolidations, verified: 132 (per-file helper), 134 (with the corrected
  19 prototypes + JPG struct + M3U macros moved), 142 (with the duplicate-run caveat).
- **Adds more than it removes: RE-SMS-135** (+~25 lines for a second parse table vs
  −2 write lines) — justified only by const-ness/mechanism removal, flagged above.
- Fix incomplete as filed: RE-SMS-140 (needs per-build cursor reset, see block),
  RE-SMS-134 (omits JPG struct / M3U macros, see block), RE-SMS-131 (stsd estimate is an
  upper bound, see block).

## Candidates (new defects noticed — not findings)

- **CANDIDATE-W6-1** — `include/SMS_ContainerFLAC.h:11-12,28`: include guard is
  misspelled `__SMS_CountainerFLAC_H` ("Countainer") in `#ifndef`/`#define`/`#endif`
  comment. Internally consistent so it still works, but any file guarding on the
  correctly-spelled name would not be protected. Noticed while verifying RE-SMS-134.
- **CANDIDATE-W6-2** — `src/SMS_ContainerMOV.c` `_mov_build_index` cursor invariant
  (see RE-SMS-140): `m_NextChunk` correctness between per-stream calls currently
  depends entirely on the per-chunk reset loop at :1034-1037 executing at least once
  in the preceding call's derived branch. Any future refactor of that branch (not just
  the filed fix) can silently corrupt chunk sizes for later streams. A per-build
  initialisation at `_mov_build_index` entry (:936) would make the invariant explicit
  regardless of the RE-SMS-140 decision.

## Tally

| Verdict | Count | Findings |
|---|---|---|
| VERIFIED | 17 | 122, 123, 124, 125, 126, 127, 128, 129, 130, 132, 133, 135, 137, 138, 139, 141, 143 |
| VERIFIED-DRIFT | 5 | 121 (seq decl :59 not :64), 134 (19 prototypes not ≈15; 31 stub-include sites; JPG/M3U non-prototype content omitted), 136 (generic rule :110-111 not :125-126; rules span :113-132 not :128-147), 142 (step = 58 lines / 57 test invocations, not ≈64), 144 (trigger :9-12 not :12-15) |
| WEAKENED | 2 | 131, 140 |
| REFUTED | 0 | — |
| BLOCKED-ENV | 0 | — |
| **Total** | **24** | RE-SMS-121 … RE-SMS-144 |

Candidates: 2 (W6-1 FLAC guard typo; W6-2 MOV cursor cross-call invariant).
