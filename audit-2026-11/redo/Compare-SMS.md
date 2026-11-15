# SMS (Simple-Media-System) — OLD vs REDO audit comparison

**Pin (both audits):** `e65ced39a27eee8d996e9257e21594d1cac1c93d` — clone `~/workspace/audit-clones-2026-11-13/Simple-Media-System`, verified at that HEAD and `git status` clean before and after this comparison (read-only throughout).

**Corpora compared (actionable findings only):**
- **OLD final** — `~/workspace/Sable-audit/Sable-MASTER-INDEX.md` §2.4: SMS-001…188, of which 184 actionable (161 STANDING + 23 REVISED). Excluded as non-actionable: SMS-013, SMS-015, SMS-089, SMS-123 (all WITHDRAWN by the old audit's own verification phases). Bodies in `Sable-Simple-Media-System*.md`, `Sable-VERIFICATION-NEW-FINDINGS.md`, `Sable-VERIFY2-NEW-FINDINGS.md`; verification in `Sable-VERIFY-SMS-CAT6.md` / `Sable-VERIFY2-SMS.md`.
- **REDO final** — `~/workspace/Sable-audit-redo/Redo-MASTER-INDEX.md`: RE-SMS-001…150, of which 149 actionable (110 STANDING + 20 STANDING-DRIFT + 13 REVISED + 6 NEW). Excluded: RE-SMS-001 (WITHDRAWN by the redo's own Stage B verification). Bodies in `Redo-SMS.md` + `Redo-NEW-FINDINGS.md`; verification in `Redo-VERIFY-SMS.md`.

**Method.** Mechanism-level matching with full bodies read on both sides — never titles alone. REVISED findings on both sides were matched against their final (revised/narrowed) claims. Table/bounds findings were matched on the actual array and mechanism; every numeric disagreement was recomputed from the code by script (§5). Every Category-6 NONE/PARTIAL in both directions, both high-benefit (Risk-High) non-Cat-6 PARTIALs, and two redo-only Category-4 findings were adjudicated by re-reading the code at the pin: **REAL** (mechanism confirmed), **NOT-REAL** (defeating evidence cited), or **COVERAGE** (mechanism sits inside the other side's declared files-not-read). Remaining non-Cat-6 NONEs are classified **COVERAGE** vs **UNEXPLAINED** against the declared scopes. Full adjudication evidence is in the Appendix.

**Declared files-not-read used for COVERAGE rulings.**
- REDO (from `Redo-SMS.md` "Files NOT read" + `Redo-SUMMARY.md`): EE/IOP assembly structure-scanned only; `include/SMS_GS.h` body composition-sampled; `src/SMS_GS_1.c` not line-audited and most of `src/SMS_GS_2.c` not line-audited; smbman-sms `des.c`/`md4.c`/`auth.c` structure-only; ~888 lines of USBD structure-only; vendored `xz*.h`/`lzma2.h`/`mbstring.h` bodies; `tools/sms_converter`, `mpeg_color_probe`, `mpeg_motion_probe`, `libsd` bodies census-only; generated docs HTML, DocBook XML, `CHANGELOG.md` beyond head+census, `docs/investigations/**/*.md` bodies not read; `src/SMS_Data.c` hex verified by script, not read as text.
- OLD (from `Sable-MASTER-INDEX.md` §1.2 + round-9 residue): every EE C source read end-to-end and every locally owned IOP source line-audited; declared not-read classes are only assembly bodies (inventoried by name), binary contents/disassembly, and spec-only/external-contract items.

---

## 1. Counts

### 1.1 Old findings (184 actionable), by comparison outcome

| Outcome | Count | Share |
|---|---|---|
| MATCH (redo has the same mechanism) | 66 | 35.9% |
| PARTIAL (redo covers part of the mechanism / one limb) | 22 | 12.0% |
| NONE (no redo counterpart) | 96 | 52.2% |

### 1.2 Redo findings (149 actionable), by comparison outcome

| Outcome | Count | Share |
|---|---|---|
| MATCH (counterpart to ≥1 old finding at full scope) | 56 | 37.6% |
| PARTIAL (counterpart only to old PARTIAL findings) | 13 | 8.7% |
| NONE — redo-only | 80 | 53.7% |

The two corpora overlap on 69 redo findings / 88 old findings (MATCH+PARTIAL). Combined distinct actionable mechanisms across both finals: **264** (184 old + 80 redo-only), with 22 old and 13 redo findings in partial-overlap relationships inside that union.

### 1.3 Adjudication of Category-6 NONEs/PARTIALs — old direction (68 items: 55 NONE + 13 PARTIAL missing limbs)

| Ruling | Count |
|---|---|
| REAL (redo missed a true defect) | 63 |
| COVERAGE (inside redo's declared files-not-read) | 5 — SMS-059, SMS-061, SMS-066 (GS rendering internals: `SMS_GS_1.c` / `SMS_GS_2.c` ranges redo never line-audited); SMS-154, SMS-155 (assembly, structure-scan only) |
| NOT-REAL | **0** |

### 1.4 Adjudication of redo-only findings (52 items: all 50 redo-only Cat-6 + Cat-4 truth checks RE-SMS-054, RE-SMS-105)

| Ruling | Count |
|---|---|
| REAL (old missed a true defect) | 51 |
| NOT-REAL | 1 — **RE-SMS-061** (AC3 `s_LATab` index-256 mechanism defeated for parsed data: `_ac3_parse_exponents` steps exponents only −2..+2 with the >24 abort, capping the index at ≤255; the final claim's residual rests on unproven EXP_REUSE stale-boundary data) |
| COVERAGE | 0 (every redo-only mechanism sits in C source the old audit claims to have line-read) |

### 1.5 Non-Cat-6 NONE classification

**Old direction — 41 non-Cat-6 NONEs:**
- COVERAGE (4): SMS-020 (tools bodies census-only in redo), SMS-156 (dead assembly routines — structure-scan only), SMS-157 (dead assembly exports), SMS-172 (docs prose bodies not read by redo).
- UNEXPLAINED (37), of which 4 are deliberate redo **checked-clean rejections** — the redo examined the same material and judged it intentional, the opposite of the old finding: SMS-003 (plain `.irx` files deemed intentional compress_irx inputs), SMS-004 (generated docs deemed disciplined), SMS-005 (`lng/` deemed a deliberate archive), SMS-033 (`vu/*.o` deemed provenance). The other 33 (SMS-010, 012, 017, 022, 023, 025, 026, 027, 028, 034, 038, 042, 043, 047, 048, 074, 076, 077, 078, 079, 080, 081, 082, 083, 085, 086, 087, 102, 174, 175, 176, 177, 178) sit in EE/IOP C source the redo read; the redo simply did not file them.
- The only two Risk-High non-Cat-6 items in the old corpus, SMS-001 and SMS-009, are PARTIAL (not NONE); their missing limbs are documented in the mapping table (§2) and both findings were VERIFIED with concrete evidence in the old audit's own full verification (`Sable-VERIFY2-SMS.md`: SMS-001's blob constants sum to exactly 266,425 B by script; SMS-009's 88 `#if/#ifdef BDM` sites across 16 files plus the CI "never shipped" quote), so the limbs stand as REAL on that evidence.

**Redo direction — 28 non-adjudicated redo-only findings, all non-Cat-6** (RE-SMS-068, 084, 108, 112, 113, 114, 115, 116, 117, 119, 120, 121, 122, 124, 125, 126, 127, 129, 130, 131, 133, 134, 135, 136, 139, 140, 141, 143): **all UNEXPLAINED, 0 COVERAGE** — every one sits in C source, docs, or build files inside the old audit's claimed full read; none falls in the old audit's declared assembly/binary/spec-only residue. (The two redo-only Cat-4 items that were truth-checked, RE-SMS-054 and RE-SMS-105, are both REAL — §1.4.)

### 1.6 Checked-clean conflicts (2)

In two places the redo's "Checked-clean" section asserts the opposite of an old finding. Both were adjudicated against the code; **the old finding is right in both**:
- **SMS-124** (AAC `section_data` write-before-check): `sect_cb[g][i]` is written at `SMS_AAC.c:6303` and `sect_start/sect_end[g][i]` at :6313-6314, *before* the `i >= 8*15` check at :6317; with `sect_len==0`, `i` reaches 120 and writes one past the 120-wide arrays. The redo's checked-clean note ("section_data checks precede the writes they protect") is contradicted by the code.
- **SMS-160** (AAC two-step Huffman declared size arrays): script count gives declared 114/86/185 and 126/83/210/373 vs actual 113/85/184 and 125/83/209/374 — 5 of 7 non-trivial entries wrong, in both directions, exactly as filed. The redo's checked-clean verified a different property (first-stage maxima vs second-stage sizes), not the declared-vs-actual mismatch.

---

## 2. Mapping table — old → redo

Adjudication column: for Cat-6 NONE/PARTIAL rows, the §1.3 ruling on the finding (NONE) or its missing limb (PARTIAL); for non-Cat-6 NONE rows, the §1.5 classification; "—" otherwise.

| Old ID | Cat | Verdict | Redo counterpart | Adjudication / classification | Scope delta / mechanism note |
|---|---|---|---|---|---|
| SMS-001 | 2 | PARTIAL | RE-SMS-106, RE-SMS-149 | — (non-Cat-6 partial; limb in note) | Redo covers only dead g_DataBuffer segments in the default build — RE-106: UMS/USBD/SIO2MAN 23,488 B consumed only in non-BDM branches; RE-149: SMSSMB segment 19,641 B dead in every build — not old's whole dual-delivery claim (266,425 B payload sum of 17 segments, redo aligned total 266,560 B; ATAD/HDD/FS/SMAP/CDVD duplication and pick-one-system fix not covered; RE-106 explicitly preserves BDM=0) |
| SMS-002 | 1 | MATCH | RE-SMS-103 | — | Same mechanism: include/s_iop_image.h dead, only refs commented at SMS_IOP.c:228 and :437; numbers agree — old 373,784 B / 3,820 lines, redo ~373 KB / 3,820 lines |
| SMS-003 | 7 | NONE | — | UNEXPLAINED — redo checked-clean rejection | No redo finding on tracked plain .irx (19 files, 323,438 B) alongside the .irx.xz the Makefile embeds; redo checked-clean instead deems plain+.xz pairs intentional compress_irx inputs/outputs and md5-consistent |
| SMS-004 | 7 | NONE | — | UNEXPLAINED — redo checked-clean rejection | No redo finding on generated docs committed three times (docs-site/ source + docs/ + wiki/, duplicated assets/search-index); redo checked-clean accepts the generated docs/ and wiki/ as disciplined with --check freshness; RE-104 covers only the non-site binaries inside docs/ (see SMS-008/SMS-032), a different mechanism |
| SMS-005 | 2 | NONE | — | UNEXPLAINED — redo checked-clean rejection | No redo finding on lng/ vs docs/translation/ duplicate language packs (19 packs, all 19 diverged, only docs/ copy tested/shipped); redo checked-clean instead deems lng/ a deliberate archive of the original 269-entry SMS 2.9 packs (live packs 311 entries in docs/translation/) |
| SMS-006 | 1 | MATCH | RE-SMS-110 | — | Same two never-loaded blobs: bdmfs_vfat.irx.xz (old 12,540 B) is RE-110's main body (not in IOP_OBJS, superseded by bdmfs_fatfs), and mmcedrv.irx.xz (old 3,904 B) is covered by RE-110's Stage-B fix-scope extension CAND-13 (mmcedrv.irx + .irx.xz pair, only mention the SMS_IOP.c:1023 'not required' comment) |
| SMS-007 | 7 | MATCH | RE-SMS-111 | — | Same file/mechanism: images/jellyfish.jpg.bak-512 (25,390 B in both) tracked despite *.bak ignore, no reference found; RE-111 additionally bundles two unreferenced source PNGs (see SMS-029) |
| SMS-008 | 7 | MATCH | RE-SMS-104 | — | Same folder/mechanism: docs/investigations/2026-11-11-handoff/ with mutated/crash-test sample media sitting in the published docs tree; scope/fix delta — old: delete folder, ~1 MB est.; redo: move docs/investigations/ (2.0 MB, handoff dir 1.7 MB, 19 sample files) out of the Pages tree to non-docs storage |
| SMS-009 | 8 | PARTIAL | RE-SMS-106 | — (non-Cat-6 partial; limb in note) | Overlap only via the non-BDM legacy segments RE-106 splits out for BDM=0; redo does not cover old's mechanism — the never-shipped BDM=0 path's ~90 #ifdef sites across 16 files plus the build-legacy CI job — and explicitly does not propose deleting the BDM=0 flavour, calling it a supported ?= switch |
| SMS-010 | 4 | NONE | — | UNEXPLAINED (classification) | No redo finding on hex-dump C arrays for sounds/font/icons (SMS_Sounds.c, SMS_GSFont.c, SMS_IconsRGBA.c, SMS_Data.c) inflating source/compile time despite the existing bin2c pipeline; redo checked-clean instead verifies the icon bake pipeline reproduces the committed files byte-identically |
| SMS-011 | 3 | PARTIAL | RE-SMS-015 | — (non-Cat-6 partial; limb in note) | Same code (g_SaveDiag[64] TEMP diagnostic in SMS_Config.c, second IPCONFIG diagnostic in SMS_GUIMenuSMS.c:2031-2090) but a different claim: redo files it as the sprintf overflow (old SMS-040's mechanism) with delete-the-TEMP-diagnostic as preferred fix; old's TEMP-permanence/duplicated-save-path claim as such is not filed |
| SMS-012 | 2 | NONE | — | UNEXPLAINED (classification) | No redo finding on the duplicated config-save/mc-icon code inside SMS_Config.c (first copy :813-878, second copy :1007-1101, icon fields :841-854 duplicated at :1021-1034); RE-132's triplication is DrawSkin (GUIDesktop) and the locale loader, not SMS_Config.c |
| SMS-014 | 6 | PARTIAL | RE-SMS-015, RE-SMS-063 | REAL | Matched against the FINAL revised claim (R2-1: EE population 238 call lines — 7 A-class real-risk / 48 bounded-but-fragile / 183 safe; M3U and DirTree examples are NOT overflows). Redo covers A-class subsets individually: RE-015 = the 3 g_SaveDiag sprintfs (SMS_Config.c:943,945,1082-1087), RE-063 = the SMS_Player.c:1502-1504 lPath[512] strcpy among 4 fixed-buffer sinks; redo does not file the CDDA |
| SMS-016 | 6 | NONE | — | REAL | No redo finding on unchecked SBR allocations in SMS_AAC.c (:3689 calloc sbr_info; :3729-3743 eight malloc(64*sizeof(float)) for G_temp_prev/Q_temp_prev dereferenced unchecked); redo AAC findings (RE-022/023/030/031/087) and its container-creator alloc finding RE-013 cover different sites, and checked-clean notes only that SBR trees walk in-bounds |
| SMS-017 | 4 | NONE | — | UNEXPLAINED (classification) | No redo finding on SMS_CopyTree.c BUF_SIZE (4096*96 = 393,216 B) or its allocation sites; matched against the FINAL revised claim (R2 correction: two allocation sites :211,:314 are mutually exclusive, not simultaneous — the simultaneous cost is SMS's stream buffers); RE-040 touches CopyTree only as a DirTree-init caller and RE-054 only cites SMS_CopyFile as an existing streaming copy |
| SMS-018 | 2 | PARTIAL | RE-SMS-137 | — (non-Cat-6 partial; limb in note) | Redo covers exactly one of old's three header duplications: dev9.h verbatim in iop/SMSDev9/include and iop/SMSUdpfs/smap/include (53 lines each, diff-identical; redo fix is cross-referencing comments, not deletion, because the shadowing is intentional); stdint.h x3, mprintf.h x2, and the two-vendored-network-stacks (lwIP SMSTCPIP vs SMSUdpfs ministack) claim are not covered |
| SMS-019 | 7 | PARTIAL | RE-SMS-142 | — (non-Cat-6 partial; limb in note) | Same CI mechanism: hand-maintained per-file host-test list in compilation.yml that can silently omit a new test — old ~59 invocations, redo ≈64 individually named invocations, both agree 59 tools/test_*.py files; redo does not cover old's second limb (tools/ ~14,204 lines of Python; harnesses like test_mov_parser.py re-declaring the C structs they test) |
| SMS-020 | 3 | NONE | — | COVERAGE (classification) | No redo finding on tools/sms_converter's Windows 7 Qt5/Python 3.8 shim (qt_compat.py PySide6->PySide2 fallback, requirements-win7.txt) or its second OS build matrix in sms-converter.yml; redo lists sms_converter bodies as censused/reference-checked only |
| SMS-021 | 1 | PARTIAL | RE-SMS-118, RE-SMS-103 | — (non-Cat-6 partial; limb in note) | Redo covers the SMS_MPEG.c:905 block — RE-118 files SMS_MPEG.c:904-935 as the dead #if 0 H263 DCTUnquantizeInter body (uncompilable, wired but never called) — and RE-103 covers the SMS_IOP.c:228,437 commented include/reset; the SMS_IOP.c:457 dead RC/GUI loop (an EE fix site in old) is not covered, and old's vendored #if 0 sites were explicitly out of its fix scope |
| SMS-022 | 3 | NONE | — | UNEXPLAINED (classification) | No redo finding on the exact-count language-pack format (311 indexed strings; one added string invalidates all 20 packs; proposed fix is loader fallback to the built-in English table for missing trailing indices); RE-054 (unbounded whole-file .lng loads) and RE-132 (triplicated locale loading) are different mechanisms in the same loader area |
| SMS-023 | 7 | NONE | — | UNEXPLAINED (classification) | No redo finding on branch/tag clutter (50 remote branches incl. checkpoint/pre-*/claude/project-thread-*, 13 tags incl. mutable latest/rolling/test-media); the redo audit is confined to the tracked tree at the pin |
| SMS-024 | 7 | PARTIAL | RE-SMS-144 | — (non-Cat-6 partial; limb in note) | Redo covers one of old's three republish-on-every-push workflows exactly: test-media.yml regenerating the ffmpeg media pack and re-uploading the test-media release on every master push with no paths: filter (old :1-46, redo :12-15); the duplicate docs-site/build.py --check in docs.yml+wiki.yml and the smbman.yml/compilation.yml cost claims are not covered |
| SMS-025 | 8 | NONE | — | UNEXPLAINED (classification) | No redo finding on SMS_DTS.c as the second-largest file (11,801 lines, 535,335 B) carried for the least-common audio format with a measure-then-remove recommendation; redo DTS findings (RE-005/010/011/084/122) are decoder-correctness/dead-table findings inside the file, not its feature-removal cost |
| SMS-026 | 8 | NONE | — | UNEXPLAINED (classification) | No redo finding on iLink/IEEE 1394 support carried end-to-end (iLinkman.irx + IEEE1394_bd.irx and .xz twins in IOP_OBJS, device-menu/config references in five EE files, docs-site ilink.html page); iLink appears in redo only as a name in RE-126's device-icon slot list |
| SMS-027 | 8 | NONE | — | UNEXPLAINED (classification) | Matched against the FINAL claim (R2-3: replaced by SMS-038, Cat 5 — the audio-only renderer runs SMS_SpectrumCalc's FFT and the ball simulation every frame in SMS_Player.c even when both visuals are off); no redo finding covers per-frame spectrum/ball cost or its gating — RE-090 is DSP MDCT-init allocation/error-test in a different path, and SMS_Spectrum.c appears in redo only as a palette-index c |
| SMS-028 | 5 | NONE | — | UNEXPLAINED (classification) | Matched against the FINAL narrowed claim (R2-2: SMS_HistorySave runs only at player teardown, once per playback end; remaining fix is skipping the save on user-stop when SMS_HistoryAdd changed nothing); no redo finding covers the history SAVE rewrite — RE-037/RE-132 cover the history LOAD record loops (old SMS-093/SMS-100 territory), a different function and mechanism |
| SMS-029 | 4 | PARTIAL | RE-SMS-111 | — (non-Cat-6 partial; limb in note) | Redo covers the splash/background limb: RE-111 files theme/splash_composited.png (232,705 B) and images/main_bg.png (230,952 B) as unreferenced large PNGs with no bake step; the icon-alias limb is not covered — redo checked-clean explicitly deems s_IconCDROM==s_IconDVD and s_IconHost==s_IconSMB deliberate fixed-offset-slot reuse, not a finding, and the loading/error theme-tree pairs are not filed |
| SMS-030 | 7 | PARTIAL | RE-SMS-138 | — (non-Cat-6 partial; limb in note) | Redo covers the first limb exactly: AGENTS.md/CLAUDE.md byte-identical (1,254 B in both audits, diff-clean in redo) with a keep-one-plus-pointer fix; the second limb — MX4SIO_SOLUTION.md (17,672 B stale point-in-time write-up at repo root, move under docs/investigations/) — is not covered (redo read the file in full, no finding) |
| SMS-031 | 7 | MATCH | RE-SMS-109 | — | Same mechanism: utils/DDrawSrv.exe prebuilt Win32 binary with no source and zero in-repo references; scope delta — old scoped to the 44 KB exe only (Loader had source and was kept), redo deletes the whole utils/ directory (4 files, 56 KB, exe + orphan Loader) |
| SMS-032 | 7 | MATCH | RE-SMS-104 | — | Same mechanism/files: docs/downloads/ legacy archives in the tree — Rad-HOST-Client-1.8a.zip (891,537 B in both), SMS_lng09.rar, pic2smi.rar, smspal.rar, plus SMS-manual.pdf — moved to release assets; subtotal delta is accounting, not a dispute: old ~1.1 MB excluding the PDF (counted under SMS-004), redo 9.9 MB for docs/downloads/ including the 9,321,329 B PDF, and redo's scope also includes docs/ |
| SMS-033 | 7 | NONE | — | UNEXPLAINED — redo checked-clean rejection | No redo finding on the five prebuilt VU microcode objects in vu/ (CSC1.o/CSC2h.o/CSC2v.o/CSC4.o/IDCT.o, 8,880 B) having no .vcl/assembly source in the repo (vu/build.sh only compresses the .o files); redo checked-clean instead treats vu/*.o as provenance for the XZ dumps in SMS_JPEGData.c, regenerated by vu/build.sh — the missing-source/provenance-note claim is not filed |
| SMS-034 | 7 | NONE | — | UNEXPLAINED (classification) | Matched against the FINAL weakened claim (vendored font packs with space-laden vendor paths in fonts/ are proven — CyrFonts_by_Deank/ 12 .mtf, Hebrew/ 3, 'Turkish by nethunter'/ 4 — but 'unused members ride along' was never proven and per-file use is indeterminable); no redo finding covers the wholesale/space-laden vendoring — RE-112 is a different defect inside the Turkish pack (latin1.mtf == Win |
| SMS-035 | 6 | NONE | — | REAL | No redo finding on SMB credentials stored in plaintext (SMS_Config.c:337 password read via File_GetString; :397-414 SMS.smb written with the password in plaintext on mc/CWD; old recommends no code fix, documentation only); redo smbman findings (RE-051/052/102) are wire-parsing/tree-deadness mechanisms, not credential storage |
| SMS-036 | 1 | MATCH | RE-SMS-102 | — | Same mechanism: dead iop/SMSSMB tree built by nothing, ps2smb.c near-duplicate of SMSMB.c (~600 diff lines both sides), shipped client is smbman-sms; line count differs slightly (old 3,859 lines / 106,583 B, redo 3,894 lines), redo adds unpatched-bug inventory inside the dead tree |
| SMS-037 | 4 | MATCH | RE-SMS-106, RE-SMS-149 | — | Same segments/numbers split across two redo findings: old SMSSMB 19,641 B (19,648 aligned, dead in every build) = RE-SMS-149; old UMS 8,504 B + USBD 9,140 B (dead in shipped BDM=1 build) = RE-SMS-106; old total 37,312 B, redo total larger only because RE-SMS-106 also adds SIO2MAN 5,817 B (23,488 B in RE-SMS-106) which old did not claim — no per-segment number disagreement |
| SMS-038 | 5 | NONE | — | UNEXPLAINED (classification) | No redo finding on _sms_dummy_video_renderer (SMS_Player.c:393) running SMS_SpectrumCalc FFT + SMS_PlayerBallSim_Update + SMS_SpectrumUpdate unconditionally while SMS_PF_ASD/SMS_PF_ANIM packets are gated off; RE-SMS-090 is DSP_MDCTInit in SMS_DSP.c, a different mechanism |
| SMS-039 | 6 | NONE | — | REAL | No redo finding at the AVI strn/IAS allocation sites; RE-SMS-013 covers other AVI allocations (_NewStream/container calloc), not these chunk-name allocations |
| SMS-040 | 6 | MATCH | RE-SMS-015 | — | Identical mechanism and sites: sprintf into g_SaveDiag[64] (SMS_Config.c:69) at :943/:945/:1082-1087 with s_pMC0SMC char[128] path; both sides 64-byte dest vs 128-byte source, redo quantifies up to ~146 bytes written |
| SMS-041 | 6 | MATCH | RE-SMS-063 | — | Matched against FINAL revised claim: only the :1504 half stands — strcpy of m_pPath into lPath[512] at SMS_Player.c:1502-1504 vs 1024-byte SMS_CWD_CAPACITY — and RE-SMS-063 carries exactly that site/size pair as its lead item (bundled with three other strcpy sinks); old's refuted :1060 lBuff[128] half (container m_pName is a ≤8-char format constant) is correctly absent from redo |
| SMS-042 | 5 | NONE | — | UNEXPLAINED (classification) | No redo finding on SMS_FileDir.c scan costs: per-entry fioOpen/fioDopen device probe for untyped entries (:543-570) or the quadratic subtitle match (up to three linear SMS_ListFindI scans per AVI node, :610-650); RE-SMS-140 is a different quadratic in MOV index building, RE-SMS-040 is DirTree recursion |
| SMS-043 | 5 | NONE | — | UNEXPLAINED (classification) | No redo finding on SMS_GUIFileMenu.c page/selection redraw synchronously reading + JPEG-decoding every missing artwork thumbnail via SMS_ArtworkGet/_artwork_load (:428-429, :516-520) before drawing; redo's checked-clean note only vouches _artwork_load's size/alloc safety, not the per-draw decode stall or a cold-load cap |
| SMS-044 | 6 | NONE | — | REAL | No redo finding at the CDDA CWD append; RE-SMS-063 covers Player/IOP/MC/Config strcpy sinks, RE-SMS-057 covers SMSContainer_SetName — neither is SMS_FileDir.c's CDDA branch |
| SMS-045 | 6 | PARTIAL | RE-SMS-013 | REAL | Matched against FINAL revised scope: RE-SMS-013 covers two of the three surviving site groups exactly — M3U pair SMS_ContainerM3U.c:308-312/:366-370 and SMS_Container.c:351-352 (GetWAVHeader malloc then Read) — but omits the surviving SMS_FileContext.c sites (:1233-1238 and :1976-1978, m_pPath malloc then strcpy); old's withdrawn MOV six / FileContext :2069 are correctly absent from redo |
| SMS-046 | 6 | NONE | — | REAL | No redo finding on language-pack strings used as sprintf format strings (sites incl. SMS_Player.c:1003/1060, SMS_ContainerAVI.c:269, SMS_CopyTree.c:52, SMS_IOP.c:210) or on extending tools/test_language_packs.py to validate % conversion sequences; RE-SMS-054 is language-file load sizing, a different mechanism |
| SMS-047 | 5 | NONE | — | UNEXPLAINED (classification) | No redo finding on the boot splash in SMS_GUIDesktop.c:545-580: SMS_TimerWait(1000) static hold at :558 between two 17-frame VSync fades; no redo item touches GUIDesktop splash timing |
| SMS-048 | 7 | NONE | — | UNEXPLAINED (classification) | No redo finding on the seven still-live legacy IOP blob source trees (iop/SMSCDVD, SMSDev9, SMSMAP, SMSPWROFF, SMSUMS, USBD, SMSTCPIP) having no build path in tools/Makefile/workflows and no provenance/blob-match note; redo treats individual trees' defects/deadness but never this documentation/provenance gap for the live set |
| SMS-049 | 6 | NONE | — | REAL | No redo finding at the FindPath pathname-append sites; RE-SMS-008 is TocEntryCopy's terminator write past its 129-byte field and RE-SMS-093 is FindPath's sector-count copy-paste error — same file/function area, different defects/sinks |
| SMS-050 | 6 | MATCH | RE-SMS-028 | — | Identical mechanism and sites: packet_size assigned only from max_pktsize at SMS_ContainerASF.c:192, never validated non-zero / file-properties object never required before the data object, first use is modulo at :368 in asf_get_packet; both sides cite :192 + :368 |
| SMS-051 | 6 | NONE | — | REAL | No redo finding on the EE-side starters in src/SMS_IOP.c; RE-SMS-064 is IOP-module-internal init (_start/CreateThread/AllocSysMemory inside USBD/SMSUMS/smbman-sms/SMSCDVD etc.), a different layer and different functions |
| SMS-052 | 6 | PARTIAL | RE-SMS-013 | REAL | Overlap at the exact site only: RE-SMS-013 lists SMS_Container.c:349-353 GetWAVHeader malloc/Read as an unchecked-NULL-allocation site, but old's mechanism is the distinct size defect at the same lines — the aSize==17 clamp inversion producing a huge Read into a 0-byte allocation — which no redo finding states |
| SMS-053 | 6 | NONE | — | REAL | No redo finding on negative AVI/ASF dimensions reaching MPEG-4 geometry init; RE-SMS-034 is unchecked MPEG picture allocations, and redo's checked-clean video note covers Rescale/VideoBuffer/IPU caps, not the SMS_Player 1024 gate or SMS_MPEGInit |
| SMS-054 | 6 | PARTIAL | RE-SMS-069 | REAL | Redo covers the second half exactly: _LoadODMLIdx publishes/writes through the unchecked SMS_Realloc result (redo sites SMS_ContainerAVI.c:281-288, root defect SMS_EE.c:45-53 publishing *apSize before realloc succeeds); redo does not cover old's first half — the type-1 entry loop's unbounded file-supplied count with no remaining-file clamp/EOF test |
| SMS-055 | 6 | MATCH | RE-SMS-029 | — | Identical mechanism and site: _LoadODMLIdx type-0/index-of-indexes recursion at SMS_ContainerAVI.c:299 (redo range :226-330, recursive call :299) with no depth limit or visited set, self-referential indx exhausts the EE stack; both propose a small depth bound (~4); minor framing delta only — old says real files nest one level, redo says two levels (super-index → index) |
| SMS-056 | 6 | NONE | — | REAL | No redo finding on AVI _ReadPacket's movi-boundary test; RE-SMS-056 is ASF _ReadPacket fragment realloc, RE-SMS-028 is ASF packet_size — neither touches SMS_ContainerAVI.c _ReadPacket or m_MoviEnd |
| SMS-057 | 6 | NONE | — | REAL | Palette-rotate wrap bug (lVal<0 not <1) has no redo counterpart; RE-SMS-014 covers g_Palette[-1] only via corrupt-config trigger, not the in-menu rotate store of 0 (final scope incl. R6 m_BrowserIBCIdx direct-read sinks, e.g. SMS_Spectrum.c:51,55) |
| SMS-058 | 6 | PARTIAL | RE-SMS-014 | REAL | Same raw-config-as-subscript mechanism for s_Speeds (old table of 3, redo table of 3, shared site GUIMenuSMS.c:619) and palette indices, but redo misses old's s_Charsets[4], s_ExitTo[3] (BrowserFlags>>28 yields 0..15, wild-pointer deref at :1039) and s_ClrRes[2]; redo instead clamps at SMS_LoadConfig and adds CDVD sl_Speeds |
| SMS-059 | 6 | NONE | — | COVERAGE | GSFont_UnpackChr glyph-offset indexing not in redo (redo GS_2.c coverage was font-size/GSFont_Load ranges only, per its Files-NOT-read note) |
| SMS-060 | 6 | MATCH | RE-SMS-006 | — | Identical mechanism and lines: SMS_EE.c:946-965, size-mismatch guard frees/NULLs retVal then unconditionally calls lzma2_uncompress into UNCACHED_SEG(retVal); redo adds ignored uncompress return |
| SMS-061 | 6 | NONE | — | COVERAGE | GSFont_UnpackChr RLE overrun of 512-byte destination not in redo |
| SMS-062 | 6 | NONE | — | REAL | CDDA_InitFileContext NULL deref not in redo (RE-SMS-146 is a different _read_string empty-field defect in the same file, explicitly distinct) |
| SMS-063 | 6 | MATCH | RE-SMS-062 | — | Identical mechanism: _read_string (SMS_FileContext.c:805-822) strlen on unterminated stack VLA; caller sizes agree both sides — old 64,512,32,32, redo 64/512 and 32-byte directory walk |
| SMS-064 | 6 | PARTIAL | RE-SMS-097 | REAL | Same GUI_LoadIcons lzma call site (SMS_GUIcons.c:1405-1406) but different defect: redo files hand-declared lzma2_uncompress prototype + ignored return, not old's unchecked allocations; old's second allocation (SMS_SyncMalloc 18*4096+12*4096+9216*13 at :1412, memcpy/slice writes :1417-1444) has no redo counterpart |
| SMS-065 | 6 | MATCH | RE-SMS-039 | — | Same GUI constructor idiom (calloc->vtable write unchecked) at shared sites GUIMenu.c:405, GUIDesktop.c:132/730/799, GUICmdProc.c:85/363-364, GUIFileMenu.c:1645, GUIMiniBrowser.c:132-133; scope delta: redo adds RingBuffer/PCM/MenuPushState, old additionally has _create_sample:425, RenderItem SyncMalloc:333, GUIDevMenu, GUIFileCtxMenu:373-377 and GUIClock capture memalign:169 not enumerated in redo |
| SMS-066 | 6 | NONE | — | COVERAGE | GS display-list unchecked allocations not in redo (redo states SMS_GS_1.c was not line-audited; RE-SMS-069's ReallocWithAlign note is a different shrink mechanism) |
| SMS-067 | 6 | NONE | — | REAL | GUI_Status sticky realloc-failure buffer loss not in redo |
| SMS-068 | 6 | MATCH | RE-SMS-036 | — | Identical mechanism: SMS_ListPushBack(...)->m_Param deref of documented NULL-on-failure return; counts differ by census — old 11 sites plus R6-REV-2 twelfth site SMS_History.c:140 (final), redo script-counts 18 direct derefs incl. History:140, GUI.c:929, GUIMenu.c:381; scope delta: redo also covers ListPush/PushBackBuf internals and sites old split into SMS-187, old's GUIFileMenu render groups and |
| SMS-069 | 6 | MATCH | RE-SMS-038 | — | Identical site/mechanism included in redo: SMS_SubtitleContext.c:714-716 _produce_packets memalign(64,lnAlloc<<1) and calloc(m_Cnt,...) used unchecked, derefs :722-761; scope delta: redo broader (also _load_sub/_load_srt unchecked node allocations and unbounded node count) |
| SMS-070 | 6 | NONE | — | REAL | NULL language/subtitle list-node cycling not in redo (RE-SMS-038 is subtitle-file loader allocations, different function) |
| SMS-071 | 6 | NONE | — | REAL | _find_signature 1.2 MB stack buffer not in redo; old size 2352*512=1,204,224 bytes, no redo number exists |
| SMS-072 | 6 | NONE | — | REAL | CDDA_Fill last-sector modulo defect not in redo |
| SMS-073 | 6 | NONE | — | REAL | Transient-menu m_pState leak not in redo (RE-SMS-039 cites GUI_MenuPushState:379 only as an unchecked calloc, not the missing free) |
| SMS-074 | 1 | NONE | — | UNEXPLAINED (classification) | Dead legacy IPU/MPEG icon data set in GUIcons (28 arrays, SMS_GUIcons.c:36-1334, ~19,230 bytes .data) not in redo; redo icon findings are dead functions/blobs/header text, not these arrays/tables |
| SMS-075 | 1 | MATCH | RE-SMS-107 | — | IPU_UnpackImage (SMS_IPU_1.c:220-324, ~105 lines, SMS_IPU.h:140) listed verbatim in redo's ten never-called EE functions; scope delta: redo broader (~200 lines, 10 functions) |
| SMS-076 | 1 | NONE | — | UNEXPLAINED (classification) | Dead SMS-logo remnants in GUIDesktop (s_LogoXY[51] 408-byte table read only as element [50].m_Y=4, 768-byte lBP[96] stack array never read/written, dead lX/lY stores) not in redo |
| SMS-077 | 1 | NONE | — | UNEXPLAINED (classification) | Empty _file_handler (SMS_GUICmdProc.c:312-315, assigned per playlist item at :373) not in redo's dead-function lists; selection actually works via CtxMenu_HandleEvent + lpState->m_pCurr readback, Handler==NULL would be skipped identically |
| SMS-078 | 5 | NONE | — | UNEXPLAINED (classification) | _is_video_artwork quadratic directory fill (SMS_GUIFileMenu.c:241-268, per-JPEG full g_pFileList rescan, O(P x V)) not in redo (redo Cat 5 is only MOV index + smb_GetShareList) |
| SMS-079 | 5 | NONE | — | UNEXPLAINED (classification) | Decrement-by-one GSFont_WidthEx width fitting (GUIMenu.c:111/156-157, GUIDesktop.c:823-824/864-865/939, PlayerControl.c:1058-1059, full re-measure per step, quadratic) not in redo |
| SMS-080 | 5 | NONE | — | UNEXPLAINED (classification) | Artwork-mode cursor step re-walking m_pFiltList ~5 times (SMS_GUIFileMenu.c:108-132 _artwork_position/_artwork_move plus render re-invocations) not in redo |
| SMS-081 | 2/5 | NONE | — | UNEXPLAINED (classification) | DevMenu removal double-redraw at wrong 55 vs 54 spacing (SMS_GUIDevMenu.c:396-408, discarded by HandleMount Redraw :198) plus duplicate RenderSelRect build (:180 vs :209) not in redo |
| SMS-082 | 2 | NONE | — | UNEXPLAINED (classification) | Selection-rectangle GS packet block copy-pasted in all three FileMenu renderers (GUIFileMenu.c:458-467/549-558/650-661) and verbatim filtered-list build loop (:830-838 vs :1618-1626) not in redo's duplication set |
| SMS-083 | 2 | NONE | — | UNEXPLAINED (classification) | Palette-rotate logic in four copies (_rotate_palette GUIMenuSMS.c:2190-2205, _rotate_palette_2 :2348-2362, inline :2219-2239/:2241-2258) not in redo — the dedup companion to SMS-057, absent even though the bug it spreads is also absent |
| SMS-084 | 2 | MATCH | RE-SMS-132 | — | Identical triplication: DrawSkin (SMS_GUIDesktop.c:155-317) three near-identical open/seek/capped-malloc/read/verify blocks; scope delta: redo pairs it with the language loader's triplication in SMS_Locale.c:826-897 as one whole-file-loading finding |
| SMS-085 | 2 | NONE | — | UNEXPLAINED (classification) | JPEG decode->expand->upload pipeline duplicated between jellyfish background and boot splash (GUIDesktop.c expand :349-366 vs :485-501, banded upload :398-416 vs :512-532) not in redo (RE-SMS-132 is DrawSkin/Locale whole-file loading, different blocks) |
| SMS-086 | 2 | NONE | — | UNEXPLAINED (classification) | USB connect/disconnect SIF handlers as mirror copies (SMS_GUI.c:255-274 vs :276-295, same unit->mask switch and opposite-flag toggle) not in redo |
| SMS-087 | 2 | NONE | — | UNEXPLAINED (classification) | GUIMenu_SelectItemByName and GUIMenu_SelectItem duplicating the whole walk (SMS_GUIMenu.c:236-260 vs :263-281, only stop predicate differs) not in redo |
| SMS-088 | 6 | MATCH | RE-SMS-095 | — | Identical numbers both sides, no dispute: extern declares 4572, definition is 5416, GSFont_Get(0) under-reports by 844 bytes; scope delta: redo fix extends unsized declarations to all five fonts |
| SMS-090 | 6 | NONE | — | REAL | Subtitle _reset unguarded packet write not in redo (RE-SMS-038 covers _produce_packets allocations at :714-716, not the Reset write at :917-922) |
| SMS-091 | 6 | NONE | — | REAL | GUIMenu_ShowItem unbounded hide-walks not in redo |
| SMS-092 | 6 | NONE | — | REAL | _smb_logon destination-bound strncpy not in redo (redo SMB findings are the separate smbman-sms IOP module) |
| SMS-093 | 6 | MATCH | RE-SMS-037 | — | Same mechanism: SMS_HistoryLoad's three loops read file-supplied lSize, alloc lSize+1 via SMS_ListPushBackBuf with no record cap (HIST_SIZE 32 enforced only on add at :141) and PushBackBuf's unchecked calloc (also RE-SMS-036); redo bundles cap+unchecked+triplication in one finding, numbers agree (32) |
| SMS-094 | 6 | MATCH | RE-SMS-090 | — | Same mechanism: DSP_MDCTInit's `if (!DSP_iFFTInit(...) < 0)` at SMS_DSP.c:972 is always false (!x is 0/1), dead destroy/return-0 branch; redo files it jointly with SMS-095 in one MDCT-init finding |
| SMS-095 | 6 | MATCH | RE-SMS-090 | — | Same mechanism: DSP_MDCTInit's m_pCos memalign unchecked before m_pSin/m_pTmp derived and written in fill loop (old cites alloc :952, redo :945 — citation drift only); second half of RE-SMS-090 |
| SMS-096 | 6 | MATCH | RE-SMS-055 | — | Identical triple defect in SMSContainer_DefReadPacket (SMS_Container.c:308-332): dead lnRead clamp computed :320-321 unused by Read :322, unalloc against m_pStm[0] instead of m_pStm[*apIdx] :326, unchecked _AllocPacket NULL deref; redo matches all three |
| SMS-097 | 6 | PARTIAL | RE-SMS-082 | REAL | Redo covers only the divide-by-zero half (CalcPktFields :239, (size*8*SampleRate)/m_BitRate with file-controlled m_BitRate==0); old's second half — 32-bit overflow of m_Size*8*m_SampleRate (≈2.5e10 for 64KiB @48kHz, negative durations on ordinary input) — is absent from redo |
| SMS-098 | 6 | MATCH | RE-SMS-101, RE-SMS-150 | — | Old's two defects at SMS_Container.c:198 (explicit path indexes s_CCreator[aPrefCont] unchecked + discards creator return so lSts<0 teardown never fires) are split by redo into two findings: RE-SMS-101 (unchecked index) and RE-SMS-150 (discarded return); combined scope identical |
| SMS-099 | 6 | MATCH | RE-SMS-057 | — | Same mechanism: SMSContainer_SetName (SMS_Container.c:284-306) backward slash scan with no start-of-string stop, walks off front on separator-less/empty path; redo adds an INFERRED M3U reachability path, old rated trigger latent — scope note only |
| SMS-100 | 2 | MATCH | RE-SMS-037 | — | Same triplication: history record-read loop (read 2-byte length → SMS_ListPushBackBuf → read string → read 8-byte param) pasted at SMS_History.c:40-60/:76-96/:98-118; redo folds the duplication into its Cat-6 history finding (fix collapses three loops to one) and cross-notes it in RE-SMS-132 |
| SMS-101 | 2 | MATCH | RE-SMS-132 | — | Same triplication: SMS_LocaleInit's seek-END/size/malloc(lSize+1)/seek-SET/read/close slurp at SMS_Locale.c:830-848/:864-880/:883-894 (redo cites :826-897, grouped with DrawSkin triplication); redo does not single out old's duplicated lowercase-filename retry sub-detail (:834-835/:868-869) |
| SMS-102 | 4 | NONE | — | UNEXPLAINED (classification) | Probe-order stack array `ContainerCreator lCC[sizeof(s_CCreator)]` at SMS_Container.c:149 dimensioned in bytes (52 slots for 13-entry table, 156 wasted stack bytes); no redo finding addresses the array sizing (RE-SMS-001 touches lCC contents/probe order, not its dimensioning) |
| SMS-103 | 6 | MATCH | RE-SMS-004 | — | Same mechanism: SMSUMS ums_fat.c:421 lOff=((seq&0x3F)-1)*13 unchecked into fat_direntry.m_Name[128] on callers' stacks, +_set_dir strcpy onward; numbers agree (seq 63 → offset 806, writes to 818); redo omits old's negative arm (seq 0x40 → writes at −13) |
| SMS-104 | 6 | MATCH | RE-SMS-005 | — | Same mechanism: SMS_DTS.c:10756 prim_channels = 3-bit field +1 (1–8) written into state/stack arrays sized DTS_PRIM_CHANNELS_MAX=5; numbers agree both sides (1–8 into 5) |
| SMS-105 | 6 | MATCH | RE-SMS-002 | — | Same mechanism: SMS_MPEG4.c:933 6-bit m_nSpriteWarpPts (0–63) unclamped, _mpeg4_decode_sprite_trajectory writes pairs into stack int lD[4][2]; overflow extent agrees (63 count → 59 pairs past) |
| SMS-106 | 6 | NONE | — | REAL | No Vorbis residue/booklist finding anywhere in redo (grep: booklist/res0/residue all zero hits) |
| SMS-107 | 6 | NONE | — | REAL | No Vorbis floor1/postlist finding in redo (grep zero hits) |
| SMS-108 | 6 | NONE | — | REAL | No Vorbis residue begin/end/grouping validation finding in redo (grep zero hits) |
| SMS-109 | 6 | MATCH | RE-SMS-016 | — | Same mechanism: ip_reass (ip_frag.c) trusts wire IPH_LEN over actual pbuf chain (pbuf_realloc shrink-only at ip.c:224-230), copy_from_pbuf :58-77 loops on claimed len with no p!=NULL stop and derefs NULL at chain end; redo additionally bundles the 5760-byte bitmap index-90 defect (old files that separately as SMS-152, outside this slice) |
| SMS-110 | 6 | MATCH | RE-SMS-020 | — | Same mechanism: SMSUMS UmsGetCapacity (ums_driver.c:312-313) stores READ CAPACITY m_SectorSize verbatim, zero/huge values divide by zero downstream (UmsRead, ums_cache.c:40); redo covers div-by-zero + geometry desync + BPB alloc, but not old's specific >128 KiB → 131,072-byte cache-buffer overflow consequence (ums.c:66, first read via ums_fat.c:989,996) |
| SMS-111 | 6 | NONE | — | REAL | No AAC PCE channel-total finding in redo (grep: program_config_element/internal_channel-as-PCE-sink/channel_position all absent; RE-SMS-087's internal_channel mention is a different pcm_synN phantom-copy mechanism) |
| SMS-112 | 6 | NONE | — | REAL | No Vorbis decodemap index finding in redo (grep: decodemap/partvals zero hits) |
| SMS-113 | 6 | NONE | — | REAL | No MPEG-4 BUG_EDGE finding in redo (grep: BUG_EDGE/SMS_BUG zero hits) |
| SMS-114 | 6 | PARTIAL | RE-SMS-010 | REAL | Redo covers old's first instance (downmix table[11][10] at SMS_DTS.c:10261 indexed by flags&0x3F row and 6-bit amode column, both 0–63) inside a bundled DTS table-index finding (with bitalloc/LFE/scale-sum sinks = old SMS-137/139/138); old's second instance is absent from redo |
| SMS-115 | 6 | MATCH | RE-SMS-032 | — | Matched against FINAL revised claim (weakened: shipped Player flow defeats 'valid 5.1 suffices' — _check_ac3 clamp + SPDIF routing; survives for non-SPDIF with m_Channels==0 or non-Player caller); redo's combined AC3/DTS finding states the same mechanism (SMS_AC3.c:2585 s_FloatToInt[anChannels-1], 2-entry table, index to 5) and the same surviving interleaving (clamp no-ops while container reports  |
| SMS-116 | 6 | MATCH | RE-SMS-032 | — | Matched against FINAL revised claim (same Player guard chain as SMS-115; survives non-SPDIF m_Channels==0 / non-Player caller); redo RE-SMS-032 covers the DTS twin (SMS_DTS.c:11565 s_FloatToInt[anChannels-1], 2-entry table, DTS_Channels 2/4/5/6) jointly with SMS-115 |
| SMS-117 | 6 | MATCH | RE-SMS-023 | — | Matched against FINAL revised claim (exclude_mask[64] walk stands, passes 63 at 9th round; n advances by 1 per round at SMS_AAC.c:7557, not 7, so additional_excluded_chns[64] needs ~64 rounds); redo covers same excluded_channels loop :7549-7562 writing 7 mask entries per continuation-bit round past the 64-entry masks |
| SMS-118 | 6 | NONE | — | REAL | No Vorbis codebook entries*dim / maptype-1 accumulation overflow finding in redo (grep: quantvals/entries*dim zero hits) |
| SMS-119 | 6 | NONE | — | REAL | No Vorbis res0_look partvals overflow finding in redo (grep zero hits) |
| SMS-120 | 6 | NONE | — | REAL | No Vorbis ordered-codebook length / marker[33] / mask[33] finding in redo (grep: oggpack/marker/mask zero hits) |
| SMS-121 | 6 | NONE | — | REAL | No Vorbis blocksize/vwin finding in redo (grep: vwin zero hits; blocksize hits are FLAC only) |
| SMS-122 | 6 | MATCH | RE-SMS-022 | — | Same mechanism: AAC sf_index 12–15 stored unchecked (old sources ADTS :5953/ADIF PCE :5821; redo ADTS :6041 + ASC) and window_grouping_info :6194-6249 indexes 12-entry num_swb/swb_offset tables by it; redo status REVISED-narrowed (ASC main index rejected, exposure survives via unvalidated SBR-extension sub-index 12–15) vs old's ADTS/ADIF + INFERRED ASC path — same sink, minor source-path scope del |
| SMS-124 | 6 | NONE | — | REAL | No redo finding for section_data write-before-check (redo's coverage note instead asserts section_data checks precede the writes — a checked-clean claim, not a finding; matching only, not adjudicated) |
| SMS-125 | 6 | NONE | — | REAL | No redo finding for WMA bit_offset re-base (grep: bit_offset/MAX_CODED zero hits; RE-SMS-060 is the same function wma_decode_superframe but a different defect — block_align advance — which per method is not a match) |
| SMS-126 | 6 | NONE | — | REAL | No redo finding for the DTS-private bit reader (grep: bitstream_fill/buffer_start zero hits; RE-SMS-099 covers only the shared SMS_Bitio.h reader — DTS uses its own bitstream_* in SMS_DTS.c with no end pointer in dts_state_t, a different reader/function) |
| SMS-127 | 6 | MATCH | RE-SMS-099 | — | Same root mechanism: shared reader include/SMS_Bitio.h (SMS_UPDATE_CACHE :26-40 unconditional 4-byte load; SMS_GetBits/GetBit :70-97) never compares m_Idx to m_szInBits; old files it as the AAC reader defect (init SMS_AAC.c:8183, only check between elements :7884), redo files it as the tree-wide root mechanism under most stream over-reads — identical mechanism, broader framing |
| SMS-128 | 6 | MATCH | RE-SMS-053 | — | Same mechanism: lzma2_get_uncompressed_size (src/lzma2.c:80-122) — size<12 footer-pointer underflow, backward_size drives index pointer before buffer, lzma2_decode_size record loop walks with no length (old also notes 32-bit size_t wrap; redo adds a 4th sub-defect, decode() 0-return ignored); same three callers (SMS_EE.c:950, SMS_GS_2.c:567, SMS_IOP.c:237) |
| SMS-129 | 6 | PARTIAL | RE-SMS-049 | REAL | Root-only overlap: redo covers the shared root (pbuf.c:291-315 RAM/POOL arm of pbuf_header wraps u16 len/tot_len on forward move, no length check) but via the ps2ip.c:245 Ethernet −14 caller; old's specific caller and consequence are absent from redo |
| SMS-130 | 6 | PARTIAL | RE-SMS-049 | REAL | Root-only overlap: same pbuf_header root as SMS-129 covered by redo via the Ethernet caller; old's UDP-specific dead drop-branch and delivery path are absent from redo (grep: UDP_HLEN/65,528 zero hits in redo findings) |
| SMS-131 | 6 | NONE | — | REAL | No redo finding on SMAP RX BD length vs 1540 pool slot; redo SMAP items are TX gather (RE-SMS-070), _start (RE-SMS-064), UDPFS PHY (RE-SMS-094) |
| SMS-132 | 6 | NONE | — | REAL | No redo USBD descriptor-walk finding; redo USBD items are init family (RE-SMS-064) and success stubs (RE-SMS-128) |
| SMS-133 | 6 | NONE | — | REAL | Matched against FINAL revised claim; redo has no oggpack finding (RE-SMS-099 is EE SMS_Bitio.h, a different reader; Vorbis uses oggpack in SMS_OGG.c) |
| SMS-134 | 6 | NONE | — | REAL | No redo Vorbis codebook dim==0 finding |
| SMS-135 | 6 | NONE | — | REAL | No redo Vorbis mode-index finding |
| SMS-136 | 6 | NONE | — | REAL | No redo Vorbis floor0/LSP finding |
| SMS-137 | 6 | MATCH | RE-SMS-010 | — | Same sink: bitalloc_huffman==7 into bitalloc_12[5] at SMS_DTS.c:11345, values 5/6 diverted, 7 not; numbers agree (table 5 entries both sides). Redo bundles it with downmix/scale/LFE sinks |
| SMS-138 | 6 | MATCH | RE-SMS-010 | — | Same sink: cumulative scale_sum indexes scale_table into quant6[64]/quant7[128] with no bound at SMS_DTS.c:11386/:11402; numbers agree both sides |
| SMS-139 | 6 | MATCH | RE-SMS-010 | — | Same sink: scale_factor_quant7[bitstream_get(8)] 0-255 into 128-entry table at SMS_DTS.c:11440; numbers agree both sides |
| SMS-140 | 6 | NONE | — | REAL | Matched against FINAL revised claim; RE-SMS-010 covers downmix/bitalloc/LFE/scale_sum only, RE-SMS-005 mentions subband_activity only as a clamped neighbour — neither files the joint-intensity index |
| SMS-141 | 6 | PARTIAL | RE-SMS-024 | REAL | Same sink (decode_exp_vlc n==0 do-while underflow write, SMS_WMA.c:1646-1648) but split coverage: redo 024's trigger is computed zero-width v1 entries (pos==lpos, row 0), old's trigger is unfilled v1 rows 1+ under variable block length — that trigger is redo RE-SMS-145's separate root-cause finding |
| SMS-142 | 6 | MATCH | RE-SMS-085 | — | Same mechanism: wma_init divides by container sample_rate with no zero check (SMS_WMA.c:1352/:1383/:1400-1402) |
| SMS-143 | 6 | NONE | — | REAL | No redo MSMPEG4 slice-height finding (redo MPEG items are allocation checks RE-SMS-034/035 and sprite RE-SMS-002) |
| SMS-144 | 6 | NONE | — | REAL | No redo MPEG-4 direct-MV field-time finding |
| SMS-145 | 6 | MATCH | RE-SMS-033 | — | Same mechanism/array: gain base at SMS_MP123.c:5422 plus scalefactor index walks past s_GainPow2[378]; table size 378 agrees both sides |
| SMS-146 | 6 | NONE | — | REAL | No redo FLAC wasted-bits finding (RE-SMS-082 is container/FLAC duration divide-by-zero, RE-SMS-100 is channel-count synth table — different mechanisms) |
| SMS-147 | 6 | MATCH | RE-SMS-059 | — | Identical: unconditional decoded[pred_order-1..-4] warm-up in decode_subframe_fixed (SMS_FLAC.c:180-185) reads decoded[-1..-4] for orders 0-3; numbers agree both sides |
| SMS-148 | 6 | MATCH | RE-SMS-012 | — | Identical: DXSB _decode never checks aSize>=sizeof(header), header width drives memset(lpTmp+lX) past scratchpad free region; numbers agree (free region 9,424 B from 0x70001B30 both sides) |
| SMS-149 | 6 | NONE | — | REAL | No redo MP123 LongIdx finding (RE-SMS-033 is the gain table, a different array/mechanism) |
| SMS-150 | 6 | NONE | — | REAL | No redo SMSUMS chain-error finding (redo UMS items are LFN RE-SMS-004, path copy RE-SMS-018, LUN RE-SMS-019, geometry RE-SMS-020, FATFS_Init leak RE-SMS-130) |
| SMS-151 | 6 | NONE | — | REAL | Matched against FINAL revised claim; no redo SMSUMS direntry-start-cluster finding |
| SMS-152 | 6 | MATCH | RE-SMS-016 | — | Same bitmap sink: ip_reassbitmap index (offset+len)/64 == 90 into a 90-byte (indices 0-89) bitmap at exactly 5,760 bytes, ip_frag.c:174/:195; numbers agree both sides. Redo also bundles the IPH_LEN chain-walk half (old SMS-109's territory) |
| SMS-153 | 6 | PARTIAL | RE-SMS-030 | REAL | Same function and root defect — huffman_getescape (SMS_AAC.c:6559-6575) escape counter i is uint8_t, uncapped, wraps at 256 (both sides agree) — but different sink: old claims i as GetBits width / 1<<i shift UB, redo claims the manufactured value indexing the 8,192-entry IQ table in vquant asm |
| SMS-154 | 6 | NONE | — | COVERAGE | No redo DSP_FFTGet/asm-register finding (RE-SMS-090 is DSP_MDCTInit in C, a different function) |
| SMS-155 | 6 | NONE | — | COVERAGE | No redo GSParams C/asm layout finding |
| SMS-156 | 1 | NONE | — | COVERAGE (classification) | Redo dead-code findings cover different items: ten C functions (RE-SMS-107), SMSTCPIP C dead code (RE-SMS-113), six C prototypes (RE-SMS-117) — none names GS_XYZF, SMS_iTimerReset or inet_ntoa |
| SMS-157 | 1 | NONE | — | COVERAGE (classification) | No redo finding on dead duplicate asm exports GS_RRV/_MPEG_AlignBits/ip_cksum/inet_aton (bodies live under sibling labels) |
| SMS-158 | 6 | MATCH | RE-SMS-072 | — | Identical: do_connect discards tcp_connect's return (api_msg.c:517-519), synchronous ERR_MEM failure never posts conn->mbox, netconn_connect blocks; redo additionally covers the discarded udp_connect arm |
| SMS-159 | 6 | MATCH | RE-SMS-009 | — | Identical: get_socket indexes sockets[NUM_SOCKETS] with unchecked fd (sockets.c:104-115); numbers agree (table 15 entries both sides; redo adds FD_SETSIZE 16 observation) |
| SMS-160 | 6 | NONE | — | REAL | No redo finding on the Huffman size arrays; redo's checked-clean instead script-verified the two-step tables' first-stage maxima <= second-stage sizes (hcb11 exactly 373 of 374) without filing the declared-size mismatch |
| SMS-161 | 6 | NONE | — | REAL | No redo finding on DTS quantizer zero tails (RE-SMS-122 is different dead DTS tables — sample/bit rates; RE-SMS-010 is bounds, not stored values) |
| SMS-162 | 6 | MATCH | RE-SMS-064 | — | Redo family finding cites the exact sites: hcd.c:340-341 AllocSysMemory memset unchecked, hcdInit return ignored, usbd.c _start returns 0 — old's unchecked-arena + twice-hidden mechanism verbatim, bundled across seven IOP modules |
| SMS-163 | 6 | MATCH | RE-SMS-041 | — | Identical: lwip_close unconditionally runs sock_set_errno(sock,...) at sockets.c:235-250 after get_socket returned NULL for an in-range unopened/double-closed fd — NULL-pointer errno write |
| SMS-164 | 6 | MATCH | RE-SMS-042 | — | Same mechanism: do_bind feeds udp_new()/tcp_new() results unchecked into udp_setflags/setup_tcp/bind (api_msg.c:393-410) unlike checked siblings. Scope delta: old revised claim narrows in-tree reachability to the TCP re-bind branch (UDP latent); redo files both branches un-narrowed |
| SMS-165 | 6 | NONE | — | REAL | No redo USBD done-queue/IsoTd finding |
| SMS-166 | 6 | MATCH | RE-SMS-045 | — | Identical: lwip_accept passes netconn_accept's possibly-NULL result (err_tcp posts NULL to acceptmbox) to netconn_peer and dereferences addr/addrlen unconditionally (sockets.c:159-186), so accept(s,NULL,NULL) crashes |
| SMS-167 | 6 | MATCH | RE-SMS-048 | — | Identical: netbuf_ref writes through unchecked pbuf_alloc result (api_lib.c:71-79); pool size 40 agrees both sides |
| SMS-168 | 6 | NONE | — | REAL | RE-SMS-094 is a different module and mechanism (SMSUdpfs smap InitPHY retry guard fires on first poll), not SMSMAP's discarded PHY-reset result / never-cleared LINK flag |
| SMS-169 | 6 | MATCH | RE-SMS-064 | — | Redo family finding cites the exact sites: SMSMAP main.c:338-341 CreateEventFlag/CreateMutex unchecked, StartThread(CreateThread(...)) inline unchecked, resident-success pattern — old's false-resident _start mechanism, bundled across modules |
| SMS-170 | 6 | MATCH | RE-SMS-077 | — | Identical half of redo's combined finding: netconn_addr (api_lib.c:236-260) dereferences conn->pcb with no NULL check unlike netconn_peer; redo bundles it with old SMS-183's discarded-return half |
| SMS-171 | 6 | MATCH | RE-SMS-070 | — | Same mechanism: SMap_Send chained-pbuf gather into static sl_TXBuf gated only by m_TXFree FIFO credit 4096 (smap.c:886-921); redo likewise narrows to latent (no in-tree chain >MTU) |
| SMS-172 | 7 | NONE | — | COVERAGE (classification) | No redo docs-vs-code finding on Devices/Settings Start rows (redo docs item RE-SMS-104 is moving binaries out of the Pages tree, a different mechanism) |
| SMS-173 | 1 | MATCH | RE-SMS-123 | — | Identical: _ac3_imdct_512/_ac3_imdct_256 aBias parameter threaded through all calls but never used in either body (SMS_AC3_imdct.c:533/:579); redo keeps m_Bias as caller fill value where old also proved the 384>>15=0 inertness — scope note only, same dead item |
| SMS-174 | 1 | NONE | — | UNEXPLAINED (classification) | RE-SMS-122 deletes different dead DTS tables (file-scope dts_sample_rates/dts_bit_rates duplicates); the integer lossy_quant/lossless_quant pair (256 B, only _d float twins consumed) is not filed in redo |
| SMS-175 | 1 | NONE | — | UNEXPLAINED (classification) | No redo finding on the do_accept/API_MSG_ACCEPT dead dispatch slot (RE-SMS-113/116 cover different SMSTCPIP dead/half-finished items; do_accept is never named) |
| SMS-176 | 1 | NONE | — | UNEXPLAINED (classification) | No redo finding on the dead _smap_emac3_ready static-inline helper in iop/SMSMAP/smap.h:60-72 |
| SMS-177 | 1 | NONE | — | UNEXPLAINED (classification) | No redo finding on dead _ifft2/BUTTERFLY_ZERO instructions in SMS_AC3_imdct.c (redo IMDCT item RE-SMS-123 is the aBias parameter only) |
| SMS-178 | 2 | NONE | — | UNEXPLAINED (classification) | No redo finding on _ifft128 open-coding _ifft64's body in SMS_AC3_imdct.c:520-525 |
| SMS-179 | 6 | MATCH | RE-SMS-074 | — | Identical: err_to_errno macro's unsigned sizeof comparison makes every negative err_t convert huge, so all errors map to EIO and the table is unreachable (sockets.c:75-91); numbers agree (11-entry table both sides; redo adds the ERR_IF -11 latent edge) |
| SMS-180 | 6 | MATCH | RE-SMS-064 | — | Redo family finding cites the exact site: USBD driver.c:246-247 callback CreateThread/StartThread unchecked — old's initCallbackThread false-success mechanism (hcdInit discards the return), bundled across modules |
| SMS-181 | 8 | MATCH | RE-SMS-128 | — | Identical three stubs (UsbRegisterAutoloader/UsbUnregisterAutoloader/UsbChangeThreadPriority, interface.c:216-229) returning success 0; matched against FINAL revised claim — the defect is the dishonest return value (header disclosure at usbd.h:248 already noted in the revision, and redo likewise files only the false success) |
| SMS-182 | 6 | MATCH | RE-SMS-078 | — | Identical: tcp_next_iss = static 6510 + tcp_ticks only (tcp.c:909-913); seed 6510 agrees both sides |
| SMS-183 | 6 | MATCH | RE-SMS-077 | — | Identical half of redo's combined finding: lwip_getpeername/lwip_getsockname discard netconn_peer/netconn_addr returns (sockets.c:827/:861) and return success with uninitialised naddr; redo's distinct RE-SMS-147 (*namelen NULL deref) and RE-SMS-148 (netconn_addr no-default) are different aspects, not this match |
| SMS-184 | 6 | MATCH | RE-SMS-064 | — | Redo family finding cites the exact sites: hcd.c:472,482 CreateThread unchecked in hcdInit (old's hcdTid IRQ-thread mechanism, incl. resident return 0), bundled across modules |
| SMS-185 | 6 | MATCH | RE-SMS-008 | — | Identical: TocEntryCopy terminator write dst[lFNameLen]=0 with disc-supplied length up to 255 past char[129] field (SMSCDVD.c:1085-1091); numbers agree (field 129, up to ~126 bytes past, both sides). Redo additionally covers the _copy_name twin site |
| SMS-186 | 6 | MATCH | RE-SMS-069 | — | Same site and pattern: _Load_idx1 (SMS_ContainerAVI.c:169-176) publishes unchecked SMS_Realloc result and writes through it; redo is broader — root SMS_Realloc size-publication defect plus the _LoadODMLIdx loader as second site |
| SMS-187 | 6 | PARTIAL | RE-SMS-036 | REAL | Same mechanism (SMS_ListPushBack(...)->m_Param deref of documented NULL-on-failure return) and redo names 11 of old's 14 further sites verbatim (Config:366, FileDir:311/340/392/437/453/519/593, GUISMBrowser:563/571, List.c Append), but M3U:317/394/433 and GUIFileCtxMenu:394 are not evidenced in the redo body and the totals disagree |
| SMS-188 | 6 | NONE | — | REAL | No redo subtitle string-width finding (redo SubtitleContext items are TranslateUTF8 RE-SMS-021, list/subtitle loaders RE-SMS-036/038 — different mechanisms) |

---

## 3. Mapping table — redo → old

Adjudication column: for redo-only rows, the §1.4 ruling (Cat-6 and the two Cat-4 truth checks) or the §1.5 classification (remaining non-Cat-6). RE-SMS-001 is WITHDRAWN in the redo final and omitted. RE-SMS-147 and RE-SMS-148 are distinct mechanisms from old SMS-183 (which matches RE-SMS-077): SMS-183 is the ignored `netconn_peer`/`netconn_addr` return value; RE-SMS-147 is a NULL `*namelen` dereference (`sockets.c:836/870`) and RE-SMS-148 is `netconn_addr`'s missing default case (`api_lib.c:236-251`) — both adjudicated redo-only REAL (Appendix F2).

| Redo ID | Cat | Redo status | Comparison | Old counterpart(s) | Adjudication / classification |
|---|---|---|---|---|---|
| RE-SMS-002 | 6 | STANDING | MATCH | SMS-105 (MATCH) | — |
| RE-SMS-003 | 6 | STANDING | NONE (redo-only) | — | REAL |
| RE-SMS-004 | 6 | STANDING | MATCH | SMS-103 (MATCH) | — |
| RE-SMS-005 | 6 | STANDING-DRIFT | MATCH | SMS-104 (MATCH) | — |
| RE-SMS-006 | 6 | STANDING | MATCH | SMS-060 (MATCH) | — |
| RE-SMS-007 | 6 | STANDING | NONE (redo-only) | — | REAL |
| RE-SMS-008 | 6 | STANDING | MATCH | SMS-185 (MATCH) | — |
| RE-SMS-009 | 6 | STANDING-DRIFT | MATCH | SMS-159 (MATCH) | — |
| RE-SMS-010 | 6 | STANDING-DRIFT | MATCH | SMS-114 (PARTIAL), SMS-137 (MATCH), SMS-138 (MATCH), SMS-139 (MATCH) | — |
| RE-SMS-011 | 6 | STANDING-DRIFT | NONE (redo-only) | — | REAL |
| RE-SMS-012 | 6 | STANDING | MATCH | SMS-148 (MATCH) | — |
| RE-SMS-013 | 6 | STANDING-DRIFT | PARTIAL | SMS-045 (PARTIAL), SMS-052 (PARTIAL) | — |
| RE-SMS-014 | 6 | STANDING-DRIFT | PARTIAL | SMS-058 (PARTIAL) | — |
| RE-SMS-015 | 6 | STANDING | MATCH | SMS-011 (PARTIAL), SMS-014 (PARTIAL), SMS-040 (MATCH) | — |
| RE-SMS-016 | 6 | STANDING | MATCH | SMS-109 (MATCH), SMS-152 (MATCH) | — |
| RE-SMS-017 | 6 | REVISED | NONE (redo-only) | — | REAL |
| RE-SMS-018 | 6 | STANDING | NONE (redo-only) | — | REAL |
| RE-SMS-019 | 6 | STANDING | NONE (redo-only) | — | REAL |
| RE-SMS-020 | 6 | STANDING | MATCH | SMS-110 (MATCH) | — |
| RE-SMS-021 | 6 | STANDING | NONE (redo-only) | — | REAL |
| RE-SMS-022 | 6 | REVISED | MATCH | SMS-122 (MATCH) | — |
| RE-SMS-023 | 6 | STANDING-DRIFT | MATCH | SMS-117 (MATCH) | — |
| RE-SMS-024 | 6 | STANDING | PARTIAL | SMS-141 (PARTIAL) | — |
| RE-SMS-025 | 6 | REVISED | NONE (redo-only) | — | REAL |
| RE-SMS-026 | 6 | STANDING | NONE (redo-only) | — | REAL |
| RE-SMS-027 | 6 | STANDING | NONE (redo-only) | — | REAL |
| RE-SMS-028 | 6 | STANDING | MATCH | SMS-050 (MATCH) | — |
| RE-SMS-029 | 6 | STANDING | MATCH | SMS-055 (MATCH) | — |
| RE-SMS-030 | 6 | STANDING | PARTIAL | SMS-153 (PARTIAL) | — |
| RE-SMS-031 | 6 | STANDING | NONE (redo-only) | — | REAL |
| RE-SMS-032 | 6 | STANDING | MATCH | SMS-115 (MATCH), SMS-116 (MATCH) | — |
| RE-SMS-033 | 6 | STANDING-DRIFT | MATCH | SMS-145 (MATCH) | — |
| RE-SMS-034 | 6 | STANDING | NONE (redo-only) | — | REAL |
| RE-SMS-035 | 6 | STANDING | NONE (redo-only) | — | REAL |
| RE-SMS-036 | 6 | STANDING-DRIFT | MATCH | SMS-068 (MATCH), SMS-187 (PARTIAL) | — |
| RE-SMS-037 | 6 | STANDING | MATCH | SMS-093 (MATCH), SMS-100 (MATCH) | — |
| RE-SMS-038 | 6 | STANDING | MATCH | SMS-069 (MATCH) | — |
| RE-SMS-039 | 6 | STANDING | MATCH | SMS-065 (MATCH) | — |
| RE-SMS-040 | 6 | STANDING | NONE (redo-only) | — | REAL |
| RE-SMS-041 | 6 | STANDING | MATCH | SMS-163 (MATCH) | — |
| RE-SMS-042 | 6 | STANDING | MATCH | SMS-164 (MATCH) | — |
| RE-SMS-043 | 6 | STANDING | NONE (redo-only) | — | REAL |
| RE-SMS-044 | 6 | STANDING | NONE (redo-only) | — | REAL |
| RE-SMS-045 | 6 | STANDING | MATCH | SMS-166 (MATCH) | — |
| RE-SMS-046 | 6 | REVISED | NONE (redo-only) | — | REAL |
| RE-SMS-047 | 6 | STANDING | NONE (redo-only) | — | REAL |
| RE-SMS-048 | 6 | STANDING | MATCH | SMS-167 (MATCH) | — |
| RE-SMS-049 | 6 | STANDING | PARTIAL | SMS-129 (PARTIAL), SMS-130 (PARTIAL) | — |
| RE-SMS-050 | 6 | STANDING | NONE (redo-only) | — | REAL |
| RE-SMS-051 | 6 | STANDING | NONE (redo-only) | — | REAL |
| RE-SMS-052 | 6 | STANDING | NONE (redo-only) | — | REAL |
| RE-SMS-053 | 6 | STANDING | MATCH | SMS-128 (MATCH) | — |
| RE-SMS-054 | 4 | STANDING | NONE (redo-only) | — | REAL (Cat-4 truth check) |
| RE-SMS-055 | 6 | STANDING | MATCH | SMS-096 (MATCH) | — |
| RE-SMS-056 | 6 | STANDING | NONE (redo-only) | — | REAL |
| RE-SMS-057 | 6 | STANDING | MATCH | SMS-099 (MATCH) | — |
| RE-SMS-058 | 6 | STANDING | NONE (redo-only) | — | REAL |
| RE-SMS-059 | 6 | STANDING | MATCH | SMS-147 (MATCH) | — |
| RE-SMS-060 | 6 | REVISED | NONE (redo-only) | — | REAL |
| RE-SMS-061 | 6 | REVISED | NONE (redo-only) | — | NOT-REAL |
| RE-SMS-062 | 6 | STANDING | MATCH | SMS-063 (MATCH) | — |
| RE-SMS-063 | 6 | STANDING | MATCH | SMS-014 (PARTIAL), SMS-041 (MATCH) | — |
| RE-SMS-064 | 6 | STANDING | MATCH | SMS-162 (MATCH), SMS-169 (MATCH), SMS-180 (MATCH), SMS-184 (MATCH) | — |
| RE-SMS-065 | 6 | STANDING | NONE (redo-only) | — | REAL |
| RE-SMS-066 | 6 | STANDING | NONE (redo-only) | — | REAL |
| RE-SMS-067 | 6 | STANDING | NONE (redo-only) | — | REAL |
| RE-SMS-068 | 3 | STANDING | NONE (redo-only) | — | UNEXPLAINED (classification) |
| RE-SMS-069 | 6 | STANDING | MATCH | SMS-054 (PARTIAL), SMS-186 (MATCH) | — |
| RE-SMS-070 | 6 | REVISED | MATCH | SMS-171 (MATCH) | — |
| RE-SMS-071 | 6 | REVISED | NONE (redo-only) | — | REAL |
| RE-SMS-072 | 6 | STANDING | MATCH | SMS-158 (MATCH) | — |
| RE-SMS-073 | 6 | STANDING | NONE (redo-only) | — | REAL |
| RE-SMS-074 | 6 | STANDING | MATCH | SMS-179 (MATCH) | — |
| RE-SMS-075 | 6 | STANDING | NONE (redo-only) | — | REAL |
| RE-SMS-076 | 6 | STANDING | NONE (redo-only) | — | REAL |
| RE-SMS-077 | 6 | STANDING | MATCH | SMS-170 (MATCH), SMS-183 (MATCH) | — |
| RE-SMS-078 | 6 | STANDING | MATCH | SMS-182 (MATCH) | — |
| RE-SMS-079 | 6 | STANDING | NONE (redo-only) | — | REAL |
| RE-SMS-080 | 6 | STANDING | NONE (redo-only) | — | REAL |
| RE-SMS-081 | 6 | REVISED | NONE (redo-only) | — | REAL |
| RE-SMS-082 | 6 | STANDING | PARTIAL | SMS-097 (PARTIAL) | — |
| RE-SMS-083 | 6 | STANDING | NONE (redo-only) | — | REAL |
| RE-SMS-084 | 8 | STANDING | NONE (redo-only) | — | UNEXPLAINED (classification) |
| RE-SMS-085 | 6 | STANDING | MATCH | SMS-142 (MATCH) | — |
| RE-SMS-086 | 6 | STANDING-DRIFT | NONE (redo-only) | — | REAL |
| RE-SMS-087 | 6 | STANDING | NONE (redo-only) | — | REAL |
| RE-SMS-088 | 6 | STANDING | NONE (redo-only) | — | REAL |
| RE-SMS-089 | 6 | STANDING | NONE (redo-only) | — | REAL |
| RE-SMS-090 | 6 | STANDING | MATCH | SMS-094 (MATCH), SMS-095 (MATCH) | — |
| RE-SMS-091 | 6 | STANDING | NONE (redo-only) | — | REAL |
| RE-SMS-092 | 6 | STANDING | NONE (redo-only) | — | REAL |
| RE-SMS-093 | 6 | STANDING | NONE (redo-only) | — | REAL |
| RE-SMS-094 | 6 | STANDING | NONE (redo-only) | — | REAL |
| RE-SMS-095 | 6 | STANDING | MATCH | SMS-088 (MATCH) | — |
| RE-SMS-096 | 6 | STANDING | NONE (redo-only) | — | REAL |
| RE-SMS-097 | 2 | STANDING | PARTIAL | SMS-064 (PARTIAL) | — |
| RE-SMS-098 | 6 | STANDING-DRIFT | NONE (redo-only) | — | REAL |
| RE-SMS-099 | 6 | STANDING | MATCH | SMS-127 (MATCH) | — |
| RE-SMS-100 | 6 | REVISED | NONE (redo-only) | — | REAL |
| RE-SMS-101 | 6 | STANDING | MATCH | SMS-098 (MATCH) | — |
| RE-SMS-102 | 1 | STANDING | MATCH | SMS-036 (MATCH) | — |
| RE-SMS-103 | 1 | STANDING | MATCH | SMS-002 (MATCH), SMS-021 (PARTIAL) | — |
| RE-SMS-104 | 7 | STANDING | MATCH | SMS-008 (MATCH), SMS-032 (MATCH) | — |
| RE-SMS-105 | 4 | STANDING | NONE (redo-only) | — | REAL (Cat-4 truth check) |
| RE-SMS-106 | 4 | STANDING | MATCH | SMS-001 (PARTIAL), SMS-009 (PARTIAL), SMS-037 (MATCH) | — |
| RE-SMS-107 | 1 | STANDING | MATCH | SMS-075 (MATCH) | — |
| RE-SMS-108 | 1 | STANDING-DRIFT | NONE (redo-only) | — | UNEXPLAINED (classification) |
| RE-SMS-109 | 1 | STANDING-DRIFT | MATCH | SMS-031 (MATCH) | — |
| RE-SMS-110 | 1 | STANDING-DRIFT | MATCH | SMS-006 (MATCH) | — |
| RE-SMS-111 | 7 | STANDING | MATCH | SMS-007 (MATCH), SMS-029 (PARTIAL) | — |
| RE-SMS-112 | 2 | STANDING | NONE (redo-only) | — | UNEXPLAINED (classification) |
| RE-SMS-113 | 1 | STANDING | NONE (redo-only) | — | UNEXPLAINED (classification) |
| RE-SMS-114 | 1 | REVISED | NONE (redo-only) | — | UNEXPLAINED (classification) |
| RE-SMS-115 | 4 | STANDING | NONE (redo-only) | — | UNEXPLAINED (classification) |
| RE-SMS-116 | 8 | STANDING-DRIFT | NONE (redo-only) | — | UNEXPLAINED (classification) |
| RE-SMS-117 | 1 | STANDING | NONE (redo-only) | — | UNEXPLAINED (classification) |
| RE-SMS-118 | 1 | STANDING | PARTIAL | SMS-021 (PARTIAL) | — |
| RE-SMS-119 | 1 | STANDING | NONE (redo-only) | — | UNEXPLAINED (classification) |
| RE-SMS-120 | 8 | STANDING | NONE (redo-only) | — | UNEXPLAINED (classification) |
| RE-SMS-121 | 1 | STANDING-DRIFT | NONE (redo-only) | — | UNEXPLAINED (classification) |
| RE-SMS-122 | 1 | STANDING | NONE (redo-only) | — | UNEXPLAINED (classification) |
| RE-SMS-123 | 1 | STANDING | MATCH | SMS-173 (MATCH) | — |
| RE-SMS-124 | 4 | STANDING | NONE (redo-only) | — | UNEXPLAINED (classification) |
| RE-SMS-125 | 1 | STANDING | NONE (redo-only) | — | UNEXPLAINED (classification) |
| RE-SMS-126 | 7 | STANDING | NONE (redo-only) | — | UNEXPLAINED (classification) |
| RE-SMS-127 | 1 | STANDING | NONE (redo-only) | — | UNEXPLAINED (classification) |
| RE-SMS-128 | 8 | STANDING | MATCH | SMS-181 (MATCH) | — |
| RE-SMS-129 | 8 | STANDING | NONE (redo-only) | — | UNEXPLAINED (classification) |
| RE-SMS-130 | 4 | STANDING | NONE (redo-only) | — | UNEXPLAINED (classification) |
| RE-SMS-131 | 2 | REVISED | NONE (redo-only) | — | UNEXPLAINED (classification) |
| RE-SMS-132 | 2 | STANDING | MATCH | SMS-084 (MATCH), SMS-101 (MATCH) | — |
| RE-SMS-133 | 2 | STANDING | NONE (redo-only) | — | UNEXPLAINED (classification) |
| RE-SMS-134 | 2 | STANDING-DRIFT | NONE (redo-only) | — | UNEXPLAINED (classification) |
| RE-SMS-135 | 3 | STANDING | NONE (redo-only) | — | UNEXPLAINED (classification) |
| RE-SMS-136 | 2 | STANDING-DRIFT | NONE (redo-only) | — | UNEXPLAINED (classification) |
| RE-SMS-137 | 2 | STANDING | PARTIAL | SMS-018 (PARTIAL) | — |
| RE-SMS-138 | 2 | STANDING | PARTIAL | SMS-030 (PARTIAL) | — |
| RE-SMS-139 | 2 | STANDING | NONE (redo-only) | — | UNEXPLAINED (classification) |
| RE-SMS-140 | 5 | REVISED | NONE (redo-only) | — | UNEXPLAINED (classification) |
| RE-SMS-141 | 5 | STANDING | NONE (redo-only) | — | UNEXPLAINED (classification) |
| RE-SMS-142 | 7 | STANDING-DRIFT | PARTIAL | SMS-019 (PARTIAL) | — |
| RE-SMS-143 | 7 | STANDING | NONE (redo-only) | — | UNEXPLAINED (classification) |
| RE-SMS-144 | 7 | STANDING-DRIFT | PARTIAL | SMS-024 (PARTIAL) | — |
| RE-SMS-145 | 6 | NEW | PARTIAL | SMS-141 (PARTIAL) | — |
| RE-SMS-146 | 6 | NEW | NONE (redo-only) | — | REAL |
| RE-SMS-147 | 6 | NEW | NONE (redo-only) | — | REAL |
| RE-SMS-148 | 6 | NEW | NONE (redo-only) | — | REAL |
| RE-SMS-149 | 4 | NEW | MATCH | SMS-001 (PARTIAL), SMS-037 (MATCH) | — |
| RE-SMS-150 | 6 | NEW | MATCH | SMS-098 (MATCH) | REAL (spot adjudication, adj-F2) |

---

## 4. Most consequential one-sided findings (adjudicated REAL)

### 4.1 Old-only — the redo's five largest misses

1. **SMS-188** — `SMS_SubtitleContext.c` `_sb_string_width` loops `while (apStr <= lpEnd)`, so the terminator byte is mapped to index 224 and passed to `GSFont_CharWidth`, which reads past the 96-glyph header table. Triggered by *every* SB-mode subtitle width computation — no crafted file needed.
2. **SMS-124** — AAC `section_data` writes `sect_cb`/`sect_start`/`sect_end` before the bounds check (see §1.6); `i` reaches 120, one past the 120-wide arrays, from a crafted AAC stream.
3. **SMS-160** — AAC two-step Huffman declared size arrays disagree with the actual table counts in both directions (5 of 7 entries; §1.6). Any bounds check written against the declared sizes — including the natural SMS-123-style fix — still admits reads 1–2 entries past the end.
4. **SMS-131** — SMAP RX: the BD-supplied length `(m_Len+3)&~3` is never clamped to the 1,540-byte pool slot before `pbuf_alloc` + FIFO copy; an over-long/corrupt BD length overwrites adjacent pool pbufs.
5. **SMS-165** — USBD `processDoneQueue_IsoTd` halted-isochronous cleanup re-clears the outer TD's LUT slot (already cleared) instead of the current TD's, leaving stale `IoRequest` pointers that later LUT scans dereference.

### 4.2 Redo-only — the old audit's five largest misses

1. **RE-SMS-007** — `SifExecDecompModuleBuffer` treats any `xz_ret > 0` as success, so every XZ decode error (codes 2–8) still gets its undefined output buffer executed as an IOP module (~25 call sites).
2. **RE-SMS-003** — SMSTCPIP `ip_reass` passes the byte-swapped total length to `pbuf_alloc`/memcpy: a 300-byte reassembled datagram becomes an ~11 KB copy into a single 1,540-byte pool pbuf — remotely triggerable IOP pool corruption on ordinary fragmented traffic.
3. **RE-SMS-066** — `ps2ip_getconfig()` `strcpy()`s an unbounded interface name into a 4-byte field (and `netif_find()` shares the pattern).
4. **RE-SMS-017** — SMSCDVD UDF: the allocation-descriptor offset `p = 176 + lLEA` is unbounded (OOB read into the 18,576-byte `s_Buffer`), disc-supplied; the redo's own verification correctly narrowed out the filed `UDFDecode` write-overflow limb, and the surviving claim is REAL.
5. **RE-SMS-096** — `UnicodeToCP`'s binary search can never reach table index 0 (proven by simulation), so the first entry of every inverse codepage table is unreachable.

---

## 5. Number disputes — recomputed from the code (full working in Appendix G)

| # | Finding pair | Question | Winner | Correct figure |
|---|---|---|---|---|
| 1 | SMS-105 vs RE-SMS-002 | MPEG-4 sprite warp: reject counts >3 or >4? | **OLD** | `lD[4][2]` stores 4 points and first overflows at 5, but the switch handles only cases 0–3 and reads only `lD[0..2]` — count 4 is in-bounds yet unhandled, so the fix threshold is >3. Redo's >4 finds the overflow start but admits the unsupported count 4. |
| 2 | SMS-117 vs RE-SMS-023 | AC3 `exclude_mask[64]`: first OOB at 9th or 10th round? | **OLD** | Python simulation of `excluded_channels` (`SMS_AAC.c:7548-7561` — AC3 bit allocation in the AAC file's shared code): round 9 writes indices 63–69; first OOB (64) at the 9th round. (The redo's own verification also corrected to 9th.) |
| 3 | SMS-142 vs RE-SMS-085 | Integer divisions in `wma_init`: 2 or 3? | **OLD** | Exactly 2 integer divisions by `sample_rate` (:1352, :1383). The third site the redo counts is a float division (`high_freq` is `float`). |
| 4 | SMS-145 vs RE-SMS-033 | MP123 max reachable table index (table = 378 agreed) | **REDO (its verification)** | Script-recomputed maxima: direct 382, pretab 394, FullGain **438** = global max — exactly the redo verification's 382/438. Old's 436 misses the +2 MS-stereo term; the redo as filed (~508) was also wrong. |
| 5 | SMS-171 vs RE-SMS-070 | SMAP `sl_TXBuf` size: 1,520 or 1,518 B? | **OLD** | `sl_TXBuf[(1514+4+3)/4]` = 380 u32 words = **1,520 B** allocated. Redo's 1,518 is the pre-rounding nominal sum (1514+4) — a different measure of the same buffer. |
| 6 | SMS-187/SMS-068 vs RE-SMS-036 | Unchecked `SMS_ListPushBack(...)->m_Param` deref sites: 26 or 18? | **OLD** | Multi-line-aware grep under a stated definition: **26 sites**. Redo's 18 undercounts; its verification's 21 is exactly the single-line count, missing the 5 multi-line `GUIFileMenu` sites (:602, 618, 636, 832, 1620). |
| 7 | SMS-036 vs RE-SMS-102 | SMSSMB legacy tree line count: 3,859 or 3,894? | **BOTH WRONG** | Whole tree `wc -l iop/SMSSMB/` = **3,926** (SMSMB.c 2,002 + ps2smb.c 1,857 + smsutils.h 35 + Makefile 32). Old's 3,859 counts only the two .c files (its own verification had already corrected to 3,926); redo's 3,894 is exact for the 3 source files but omits the Makefile. |

Score: OLD 5, REDO 1, both-wrong 1. Every other matched table/bounds pair's numbers agree on both sides (1540 pool size, 129-byte TOC field, 5 DTS channels, 64/128 table sizes, 90@5760, 15 socket-table entries, 11, 6510, 9,424 — confirmed during matching).

---

## Appendix — adjudication evidence (verbatim working files)

### A. Old direction, SMS-001…056 slice (adj-A)
SMS-014 | REAL | Missing limb confirmed: src/SMS_FileDir.c:465-466 CDDA branch does strcpy(g_CWD,g_pCDDAFS); strcat(g_CWD,apPath) with no length check, bypassing the capacity-checked helper _append_directory_path at src/SMS_FileDir.c:242-261 (SMS_CWD_CAPACITY 1024, include/SMS_Sounds.h:24-25). This is the surviving A-class site of the revised 238-line census; the 48 B-class sites are the same fragility population, safe only via caps elsewhere.
SMS-016 | REAL | src/SMS_AAC.c:3689 sbr=calloc(1,sizeof(sbr_info)) is dereferenced immediately at :3690 (sbr->id_aac=...) with no NULL check; G/Q_temp_prev mallocs at :3729-3732 and :3742-3743 (64*sizeof(float) each) are likewise unchecked and later used unchecked, e.g. memcpy into G/Q_temp_prev at :4981-4982,4992-4993.
SMS-035 | REAL | src/SMS_Config.c:337 reads lInfo.m_Password via File_GetString; SMS_SaveSMBInfo opens SMS.smb (g_SMSSMB) at :397 and writes it in plaintext via _smb_put_line(lFD,lpInfo->m_Password) at :414, where _smb_put_line at :380-386 is a raw fioWrite of the string plus newline — no encryption/obfuscation anywhere in the path.
SMS-039 | REAL | In _ReadHeader, lSize is the raw file chunk size from File_GetUInt at src/SMS_ContainerAVI.c:393; the strn case at :635 does malloc((lSize+=lSize&1)+1) then Read and lpStm->m_pName[lSize]=0 at :636-637, and the IAS1-9 case at :653-657 does the same malloc/Read/NUL-write — no clamp to remaining file and no NULL check at either site.
SMS-044 | REAL | Same site as SMS-014's A-class remainder, ruled on its own: src/SMS_FileDir.c:465-466 strcpy(g_CWD,g_pCDDAFS); strcat(g_CWD,apPath) is unbounded, while every other append in the function uses _append_directory_path at :242-261 which rejects lAppend >= SMS_CWD_CAPACITY-lLength-lSeparator (1024, include/SMS_Sounds.h:24).
SMS-045 | REAL | Missing limb confirmed: src/SMS_FileContext.c:1233-1238 retVal->m_pPath=malloc(strlen(apFileName)+1) then strcpy(retVal->m_pPath,apFileName) with no NULL check (retVal itself comes from CDDA_InitFileContextInternal, which can return NULL); identical pattern at :1976-1978 malloc(strlen(aFileName)+1) then strcpy at :1978, unchecked.
SMS-046 | REAL | Locale strings are used directly as sprintf formats: src/SMS_Player.c:1003-1006 sprintf(lBuff,STR_SUB_ERROR.m_pStr,...), :1060 sprintf(lBuff,STR_BUFFERING_FILE.m_pStr,...), src/SMS_ContainerAVI.c:269 sprintf(lBuf,STR_LOADING_ODML.m_pStr,...), src/SMS_CopyTree.c:52 sprintf(lpSts,STR_READING_FMT.m_pStr,apName), src/SMS_IOP.c:210 sprintf(lBuff,STR_LOADING.m_pStr,...). tools/test_language_packs.py (136 lines) checks entry count/device names/width only — no % conversion-sequence validation — so a pack containing %n/mismatched %s reaches these sites ungated.
SMS-049 | REAL | Final revised claim confirmed: iop/SMSCDVD/SMSCDVD.c:772-773 FindPath does strcat(CachedDirInfo.m_Pathname,"/")+strcat(...,lDirName) into m_Pathname[1024] (:168) with no accumulated-length check, fed by strcpy(s_DirName,apPath) at :685 and strcpy(s_DirName,apPath+lPathLen) at :657 into s_DirName[1024] (:195). Components are ≤129 B (TocEntryCopy strncpy 128 at :1087), so m_Pathname overflow requires a >1023-byte apPath — which itself first overflows s_DirName at :657/:685; the ISO_DOpen caller at :875 passes apName directly with no _splitpath truncation.
SMS-051 | REAL | src/SMS_IOP.c:642-647 (BDM) sets SMS_IOPF_USB/SMS_IOPF_UMS after SifExecDecompModuleBuffer calls whose return is never read (guard at :639 keys on UMS, so a failed load wedges retry); :792-793,802 SMS_IOPStartILINK sets SMS_IOPF_ILINK unchecked; :835-836,845 SMS_IOPStartATA sets s_AtaBusOwner=2 and SMS_IOPF_ATA unchecked; :1150-1152 SMS_IOPStartHDD assigns s_AtaBusOwner=1 before the status check at :1155-1157 — unlike MX4SIO/MMCE starters in the same file which flag only on success (e.g. :722-724 comment + <0 return).
SMS-052 | REAL | Missing limb (size defect, distinct from RE-SMS-013's NULL-check at the same lines): src/SMS_Container.c:347-353 — gate aSize>16 admits aSize==17, then if(lLen>0){ if(lLen>aSize-18) lLen=aSize-18; } assigns lLen=-1, followed by malloc((lLen+15)&~15)=malloc(0) at :351 and Read(...,lLen) at :352 where -1 converts to 4294967295 as unsigned. Callers pass file-supplied sizes: src/SMS_ContainerASF.c:265, src/SMS_ContainerAVI.c:599.
SMS-053 | REAL | src/SMS_ContainerAVI.c:570-571 and src/SMS_ContainerASF.c:302-303 assign unsigned File_GetUInt width/height into int32_t m_Width/m_Height (include/SMS_Codec.h:70-71), so values >=2^31 become negative; the only gate, src/SMS_Player.c:1726-1731, tests only >1024, so negatives pass to the codec Init at :1734 and into SMS_MPEGContext_Init (src/SMS_MPEGInit.c:17,25-35) which computes MBW/MBH/strides/MBNum in signed arithmetic and callocs from them at :49-60. Redo checked-clean covers Rescale/VideoBuffer/IPU caps, not this gate — no conflict tag.
SMS-054 | REAL | Missing limb confirmed: src/SMS_ContainerAVI.c:232 lnEntries=File_GetUInt is consumed by the type-1 loop at :274-291 (for i<lnEntries) with no clamp to remaining file/indx chunk and no EOF test, one SMS_Realloc per iteration at :281-285 — in contrast to _Load_idx1 in the same file at :134-136 which clamps lnIdx to (m_Size-m_CurPos)/16. At EOF the inline readers return 0 without advancing, so a lying indx synthesises entries.
SMS-056 | REAL | src/SMS_ContainerAVI.c:726 uint32_t i=lpBuf->m_CurPos is captured once at _ReadPacket entry and never updated inside the scan loop (while at :740, consuming chunks via File_Skip/Read); the boundary test at :779 lCond=lSize>=0 && i+lSize<=(uint32_t)m_MoviEnd, used at :784 and :797, therefore weakens per chunk (and is lenient by the 8 header bytes even for the first), accepting chunks crossing m_MoviEnd as packets/skips.

### B. Old direction, SMS-057…092 slice (adj-B)
SMS-057 | REAL | Final (R6-REV-1) claim confirmed: all rotate copies test `lVal < 0` not `< 1` (src/SMS_GUIMenuSMS.c:2198,2228,2249,2356), so Left at 1 stores 0; inline handlers then read g_Palette[idx-1] at :2231/:2253, and the m_BrowserIBCIdx direct sink at src/SMS_Spectrum.c:51,55 reads g_Palette[g_Config.m_BrowserIBCIdx-1] (unsigned wrap to UINT_MAX).
SMS-058 | REAL | Missing limb confirmed: s_Charsets[4] at src/SMS_GUIMenuSMS.c:541 subscripted unchecked at :556, s_ExitTo[3] at :778 subscripted by BrowserFlags>>28 (0..15) at :816 and dereferenced at :1039/:1114, s_ClrRes[2] at :2598 subscripted at :2649/:2718; SMS_LoadConfig (src/SMS_Config.c:696-702,727-775) reads 892 raw bytes and guards only m_SkinName NUL and m_PlayerAC3RL (BrowserFlags top-nibble clear only #ifdef EMBEDDED) — none of these three fields is clamped.
SMS-059 | COVERAGE | Mechanism verified personally — src/SMS_GS_2.c:128 `(lpFont+11)[aChr]` and :154 `lpOffs[aChr]` with no aChr<m_nGlyphs check — but GSFont_UnpackChr (:124-154) sits in redo's Files-NOT-read entry "GS rendering internals": src/SMS_GS_2.c was read only at the ranges cited in RE-SMS-053/095/096 (font size table, GSFont_Get, GSFont_Load), not end-to-end; UnpackChr is outside those ranges.
SMS-061 | COVERAGE | Mechanism verified personally — same function: src/SMS_GS_2.c:142 memset(apBuf,0,512), :152 lEnd=lWidth*m_GlyphHeight with no clamp, row advance lpBuff+=16, destination lCharBuf[512] at :214 in GSFont_Init — but it sits in the same redo Files-NOT-read entry as SMS-059 (src/SMS_GS_2.c not line-audited end-to-end, only RE-SMS-053/095/096 ranges read).
SMS-062 | REAL | src/SMS_FileContext.c:747-803 CDDA_InitFileContextInternal returns NULL when its first Fill fails (lfSuccess==0 → free, retVal=NULL); caller at :1228 assigns it and at :1233-1238 unconditionally writes retVal->m_pPath/m_StreamSize/m_pOpenParam/Open and strcpy's into the also-unchecked m_pPath malloc, with no NULL test (contrast checked caller at :824).
SMS-064 | REAL | Missing limb (second allocation) confirmed: src/SMS_GUIcons.c:1412 `SMS_SyncMalloc(18*4096+12*4096+9216*13)` (~242 KB) assigned to s_pIconData/lpPtr unchecked, then memcpy and pointer-slice writes at :1417-1444; SMS_SyncMalloc itself (src/SMS_EE.c:730-738) is a bare memalign wrapper with no failure handling, so a NULL result is written through.
SMS-066 | COVERAGE | Mechanism verified personally — src/SMS_GS_1.c:167-170 Init memalign→InvalidDCache, :213-215 NewPacket via SMS_ReallocWithAlign (src/SMS_EE.c:55-89 returns NULL and zeroes size on failure) then DMA-tag writes, :288-293 NewList retVal[0]=0 at :292, :315-317/:338-340 CallList/CallList2, all unchecked — but the whole file sits in redo's Files-NOT-read entry "GS rendering internals": src/SMS_GS_1.c was not line-audited.
SMS-067 | REAL | src/SMS_GUIDesktop.c:833-846 GUI_Status: s_pDMASL assigned the SMS_ReallocWithAlign result and s_nDMASL=lWidth raised unconditionally at :836, then s_pDMASL[0..1] dereferenced at :842-845; on realloc failure the pointer is NULL and the raised s_nDMASL makes later same/shorter calls skip the realloc and deref NULL again (sticky loss).
SMS-070 | REAL | src/SMS_PlayerControl.c:357-405 builds s_pLang/s_pSubLang only from AUDIO/SUBTL streams, so a no-audio container leaves s_pCurLang=s_pLang->m_pHead=NULL at :404; _do_change_lang at :840-850 reads apListNode->m_pNext with no NULL guard, and _STR(n)=(char*)(n+1) (include/SMS_List.h:16) makes _update_lang (src/SMS_PlayerMenu.c:109-115) strlen a near-NULL pointer on menu open.
SMS-071 | REAL | src/SMS_FileContext.c:322 `u8 lBuf[2352*512]` = 1,204,224-byte automatic array in _find_signature, called from CDDA_InitContext at :1020 on the GUIDevMenu mount path (src/SMS_GUIDevMenu.c:314); repo-provisioned thread stacks are only 4-16 KB, and the function needs only one 2352-byte sector at a time by its own algorithm.
SMS-072 | REAL | Final revised claim confirmed: src/SMS_FileContext.c:482 (non-streaming CDDA_Fill, last-sector non-start branch) sets m_pEnd=m_pPos+(m_Size+m_Offset)%2352, so an exact multiple yields 0 and drops the last 2352 B while m_Pos=m_Size; the start-sector branch at :474-477 (m_pEnd=m_pPos+m_Size, no modulo) saves single-sector files, so the surviving trigger is exactly the revised one — modulo==0 AND at least two sectors.
SMS-073 | REAL | src/SMS_GUIMenu.c:377-385 GUI_MenuPushState allocs a GUIMenuState plus a list node; MiniBrowser (:167,173), CmdProc (:349,379), FileCtxMenu (:513,525) and PlayerMenu (:264,269) each do m_pState=SMS_ListInit()+GUI_MenuPushState(), but teardown is only GUI_DeleteObject (src/SMS_GUI.c:948-965: Cleanup+free) and GUIMenu_Cleanup (src/SMS_GUIMenu.c:199-211) deletes only the GS packet — no GUI_MenuPopState or SMS_ListDestroy at any of the four sites, leaking list+node+state per invocation.
SMS-090 | REAL | Final revised claim confirmed: src/SMS_SubtitleContext.c:919-922 _reset unconditionally writes s_SubCtx.m_pPackets[s_SubCtx.m_Idx].m_Pad[0]|=0x80000000 with no NULL/range guard (neighbours _display :874 and _destroy :901 do guard); _display can leave m_Idx==m_Cnt after the last packet expires, and Reset callers src/SMS_PlayerControl.c:774,816,1348 guard only on m_pSubCtx — the one-past write is reachable. The withdrawn 'Prepare never run ⇒ NULL' limb is not ruled on.
SMS-091 | REAL | Final revised (latent-only) claim confirmed: src/SMS_GUIMenu.c:362-375 GUIMenu_ShowItem walks `while((--m_pFirst)->m_Type&HIDDEN)` at :369 and `while((++m_pLastV)->m_Type&HIDDEN)` at :371 with no m_pItems floor / m_pLast ceiling and aPos unchecked — genuinely unbounded in code; for the sole caller (src/SMS_GUIMenuSMS.c:2342, aPos=4) both walks stop after one step onto a never-hidden neighbour, so as revised this is latent hardening only, with no reachable OOB walk in the shipped menu.
SMS-092 | REAL | Latent mechanism as filed confirmed: src/SMS_GUIDevMenu.c:575-576 strncpy(lLogOn.User,lpInfo->m_UserName,255) / strncpy(lLogOn.Password,lpInfo->m_Password,255) use the destination bound (User[256]/Password[256], include/SMS_SMB.h:57-60) instead of the source fields m_UserName[32]/m_Password[64] (include/SMS_SMB.h:90-91), so a non-NUL-terminated full source field would over-read into following struct fields; current producer SMS_LoadSMBInfo (src/SMS_Config.c:336-337) always NUL-terminates via File_GetString, so no live trigger — exactly the filed latent scope.

### C. Old direction, SMS-093…130 slice (adj-C)
# SMS adjudication C — pin e65ced39a27eee8d996e9257e21594d1cac1c93d, clone read first-hand

SMS-097 | REAL | Missing limb (overflow) confirmed: src/SMS_Container.c:239 computes (apPkt->m_Size * 8 * m_SampleRate) / m_BitRate into int lNum (declared int at :228) with no widening. m_Size is uint32_t (include/SMS.h:237) and m_SampleRate int32_t (include/SMS_Codec.h:72), so the product is 32-bit: 65536*8*48000 ≈ 2.5e10 wraps before the division, corrupting duration on ordinary large packets.

SMS-106 | REAL | src/SMS_OGG.c:3527-3536 res0_unpack: partitions = 6-bit+1 (≤64), cascade is 3 bits optionally extended by 5 bits (popcount ≤8), acc += icount(cascade) reaches 512. Fill loop :3536 writes info->booklist[j] for j<acc into int booklist[256] (src/SMS_OGG.c:90, struct at :89-99); the booklist>=books validation (:3540-3541 in current numbering, after the fill) checks values, not count.

SMS-107 | REAL | src/SMS_OGG.c:3413-3458 floor1_unpack: partitions ≤31 (5 bits), class_dim = 3-bit+1 (≤8), count accumulates to ≤248 and writes info->postlist[k+2] into int postlist[VIF_POSIT+2]=65 (src/SMS_OGG.c:64, VIF_POSIT 63 at :36) with no count>VIF_POSIT check. floor1_look (:3574, :3588-3597) repeats it: stack sortpointer[VIF_POSIT+2] and look forward/reverse/sorted_index[65] written for i<n where n = summed dims + 2.

SMS-108 | REAL | src/SMS_OGG.c:3522-3525 res0_unpack reads begin/end/grouping as unchecked 24-bit fields (+1 for grouping); no validation against blocksizes anywhere in unpack/look. Sinks verified: _01inverse offset = info->begin + i*samples_per_partition passed to decodepart writing into appIn[j]+offset (:4740-4750 region), res2_inverse same at :4908-4911; PCM buffers are calloc'd at blocksizes[1] floats (:3994-3999, ≤32768 even at max blocksize), so begin=1,000,000 writes far past; end<begin also makes partvals = (end-begin)/grouping negative at :4719/:4887.

SMS-111 | REAL | src/SMS_AAC.c:5812-5909 program_config_element: front/side/back counts are 4-bit (≤15 elements, ≤30 channels each) and LFE 2-bit (≤3), so pce->channels reaches 93 while per-element values are stored into cpe_channel/sce_channel during construction (:5845-5890). Only check is if (pce->channels > MAX_CHANNELS) return 22 at :5905, after construction; MAX_CHANNELS=64 (:110). Returns ignored at :5931 (get_adif_header, void caller) and :7917 (raw_data_block ID_PCE). Sinks: decode_cpe :7808-7809 indexes internal_channel[64] by pce.cpe_channel[tag] (its :7791 guard checks fr_channels, not the PCE value); create_channel_config :7984-8020 writes channel_position[64] (:480) chpos++ per PCE channel count (~93).

SMS-112 | REAL | src/SMS_OGG.c:4734 (_01inverse) and :4900 (res2_inverse): temp = vorbis_book_decode(phrasebook) is used directly as look->decodemap[temp], with only temp==-1 and decodemap[temp]==NULL tests — the NULL test itself reads the unchecked index, and the fetched pointer is dereferenced as partword/partbooks input. decodemap is sized by partvals in res0_look (:3659-3672); phrasebook entries (24-bit setup field) are independent of partvals, so e.g. partvals=2 with a 100-entry phrasebook returns temp up to 99.

SMS-113 | REAL | src/SMS_MPEG4.c:2733-2736: under SMS_BUG_EDGE, m_HEdgePos = m_Width and m_VEdgePos = m_Height (pixel counts), vs init src/SMS_MPEGInit.c:33-34 m_MBW-1 / m_MBH-1 (macroblock counts). Flag set from file version fields at src/SMS_MPEG4.c:3324 (XviD build ≤12) and :3329 (DivX <500). CLIP_MV/CLIP_MVC (src/SMS_MPEG.c:441-470) clamp MB coordinates lMBX/lMBY against those edge values; sink e.g. :574-578 CLIP_MV() then lpMB = apRefPic + lMBX + lMBY*m_LineSize and dereference, so pixel bounds address macroblocks far beyond the (MBW+2)x(MBH+2) reference allocation.

SMS-114 | REAL | Missing limb (second instance) confirmed: src/SMS_DTS.c:145 dts_channels is a 16-entry table, indexed at :11299, :11301 and :11309 by state->output & DTS_CHANNEL_MASK with DTS_CHANNEL_MASK=0x3F (:33) — index range 0-63, no clamp. state->output is the return of _dts_downmix_init (:10741), itself the unchecked table[flags&0x3F][input] read at :10261, so its value is not bounded to the in-range table values (<16); a crafted amode/flags pair producing output ≥16 reads up to 47 entries past dts_channels and steers the upmix/downmix/LFE branch choice.

SMS-118 | REAL | src/SMS_OGG.c:3160-3161 (vorbis_staticbook_unpack): dim = oggpack_read 16 bits, entries = 24 bits, both int (include/SMS_OGG.h:163-164). Maptype 2 at :3217 quantvals = entries * dim is a signed-int product (max entries × dim≥129 exceeds INT_MAX), sizing the quantlist malloc/fill at :3223ff, while _book_unquantize :3822-3840 indexes quantlist[j*dim+k] for all j<entries. Maptype 1 _book_maptype1_quantvals :3128-3151 accumulates acc *= vals / acc1 *= vals+1 dim times with no overflow check, and _book_unquantize accumulates indexdiv *= quantvals the same way (:3808ff).

SMS-119 | REAL | src/SMS_OGG.c:3631-3675 res0_look: dim = phrasebook->dim (unvalidated 16-bit-derived int, :3644), parts = info->partitions (≤64). :3657 look->partvals = rint(pow((float)parts, (float)dim)) assigned to int partvals (:128) — 64^6 = 2^36, float-to-int conversion out of range yields negative/truncated partvals. :3659 malloc(partvals * sizeof(*decodemap)) then wraps to a small/zero allocation on 32-bit, while fill loops (:3661-3672) and consumers trust the logical partvals.

SMS-120 | REAL | src/SMS_OGG.c:3178-3195 ordered unpack: length starts at 5-bit+1 and ++length per run, runs bounded only by entries (24-bit), so lengthlist entries exceed 32. Sink 1 _make_words :3721-3768: entry = marker[length] at :3732 is read BEFORE the length<32 test at :3733, and the marker[j] update loops run to j=length, into stack unsigned int marker[33]. Sink 2: those lengths become dec_codelengths/dec_maxlength (:3917-3921), and decode_packed_entry_number :4340-4366 calls oggpack_look(apBuf, dec_maxlength), which at :4305 indexes mask[anBits] — mask is a 33-entry table (:148-156) — out of range for anBits >32.

SMS-121 | REAL | src/SMS_OGG.c:3109-3110 _vorbis_unpack_info: blocksizes = 1<<4-bit (8..32768); only guards are blocksizes[0] < 8 and blocksizes[1] < blocksizes[0] at :3115-3116, so 8/16/32 and 16384/32768 pass. _vds_shared_init :3982-3983 window[i] = ilog(blocksizes[i]-1)-6 = -3,-2,-1,8,9 for those values (ilog verified :2543-2551). _vorbis_window_get :4922 returns vwin[aN] from the 8-entry table at :2449, and vorbis_synthesis_blockin :4980ff dereferences the result as the window table.

SMS-124 | REAL — CHECKED-CLEAN-CONFLICT | Code contradicts the redo checked-clean assertion that section_data checks precede the writes. src/SMS_AAC.c section_data: sect_cb[g][i] is written at :6303 and sect_start/sect_end[g][i] at :6313-6314, and only afterwards do the EIGHT_SHORT_SEQUENCE checks run at :6316-6317 (k+sect_len > 8*15; i >= 8*15). Arrays are sect_cb/sect_start/sect_end[8][15*8]=120 wide (:413-415). With sect_len==0 the while (k < max_sfb) loop at :6299ff does not advance k but increments i each iteration, so at i==120 the writes land one past all three arrays before the :6317 check returns 15.

SMS-125 | REAL | src/SMS_WMA.c wma_decode_superframe: bit_offset read at :2059 with byte_offset_bits+3 bits and never bounded on the re-base path; the :2061 bound check runs only inside if (last_superframe_len > 0), so the first superframe skips it. Sink :2073-2074: pos = bit_offset+4+4+byte_offset_bits+3; SMS_InitGetBits(lpBitCtx, lpBuf+(pos>>3), (MAX_CODED_SUPERFRAME_SIZE-(pos>>3))*8) with MAX_CODED_SUPERFRAME_SIZE=16384 (:39). SMS_InitGetBits takes uint32_t aBitSize (include/SMS_Bitio.h:143-147), so pos>>3 > 16384 makes the int size negative and converts to a huge bit count with the reader pointer past the packet; byte_offset_bits itself is derived from file-controlled m_BitRate at :1293-1295 with no clamp.

SMS-126 | REAL | src/SMS_DTS.c:10007-10009 bitstream_fill_current does *state->buffer_start++ unconditionally. dts_state_t (:67-125) carries buffer_start, current_word and bits_left but no end pointer or remaining-length field, so no fill/get path can stop at the frame end. Frame copy is bounded (syncinfo frame_size 14-bit+1 at :10130, ≤16384, < BUFFER_SIZE 24576 at :23), but the subframe/subsubframe/channel/subband decode loops draw via bitstream_get with no end comparison — a truncated/inconsistent frame walks buffer_start past m_InBuf indefinitely.

SMS-129 | REAL | Missing limb (TCP caller) confirmed: iop/SMSTCPIP/src/tcp_in.c:157-158 hdrlen = TCPH_HDRLEN(tcphdr) (4-bit wire field, 0-15, include/lwip/tcp.h:176, never checked ≥5 or vs segment length) and pbuf_header(p, -(hdrlen*4)) with return ignored — contrast :121 where the IP-strip call's return is honoured. Root verified in iop/SMSTCPIP/src/pbuf.c:291-315: the RAM/POOL arm of pbuf_header only checks the payload does not move before the pbuf struct, then len/tot_len (u16_t, include/lwip/pbuf.h:80-83) += header_size wraps (20-byte segment, hdrlen 15 → ≈65,492). inseg.len = p->tot_len at tcp_in.c:284 and tcp_parseopt (:1162ff) walks by the claimed hdrlen.

SMS-130 | REAL | Missing limb (UDP caller) confirmed: iop/SMSTCPIP/src/udp.c:172 udp_input strips with pbuf_header(p, -((s16_t)(UDP_HLEN + IPH_HL(iphdr)*4))) and its drop-short branch relies on that call failing — for POOL pbufs it cannot fail (same pbuf.c:291-315 root as SMS-129: forward move has no length check), so an IP payload < UDP_HLEN (8, include/lwip/udp.h:41) wraps len/tot_len to ≈65,528+u and the ports are read from attacker bytes at payload-UDP_HLEN. udphdr->len is never consulted on the input path (only in the UDPLite checksum branch). On PCB match, after restore/strip at :309/:341, pcb->recv is called at :344 with the wrapped tot_len.

### D. Old direction, SMS-131…188 slice (adj-D)
# Adjudication D — SMS OLD-vs-REDO, NONE/PARTIAL items (pin e65ced39a27e, clone read-only, all evidence personally read/scripted)

SMS-131 | REAL | iop/SMSMAP/smap.c:1010 `lLen=(lpRXBD->m_Len+3)&~3` gated only by the RX error-bit test at :1012; :1014 pbuf_alloc(PBUF_RAW,lLen,PBUF_POOL) and :1018 SMAP_CopyFromFIFO copy tot_len bytes with no clamp anywhere. Pool slot is PBUF_POOL_BUFSIZE 1540 (iop/SMSTCPIP/include/lwipopts.h:63). No lLen>1540 check exists in the RX path.

SMS-132 | REAL | iop/USBD/src/hcd.c:176-196 doGetDeviceStaticDescriptor: walk condition is only `descBuf<staticDeviceDescEndPtr && bLength>=2`, returns the descriptor on type match with no bLength>=sizeof(struct) check. Consumers read fixed offsets immediately: hub.c:635/639/643/653 (bNumInterfaces/bNumEndpoints) and hub.c connect path reading endpDesc->bmAttributes/wMaxPacketSizeHB/LB.

SMS-133 | REAL | Final revised claim only: src/SMS_OGG.c oggpack_read (~:2948) and oggpack_look (~:4310) — when endbyte==storage and a 0-bit read is requested, the total-bits test `endbyte*8+anBits > storage*8` is false, execution falls through to the unconditional `ret = ptr[0] >> endbit`, reading one byte past the packet. The original up-to-4-byte claim is correctly withdrawn (the test bounds ptr[1..4]); the surviving 0-bit/ptr[0] touch is in the code as revised.

SMS-134 | REAL | src/SMS_OGG.c: vorbis_staticbook_unpack reads dim (16-bit) / entries (24-bit) at ~:3160-3161 and rejects only entries==-1; dim==0 is never rejected. _book_maptype1_quantvals (:3128-3151) computes pow(entries,1.0F/dim) (divide-by-zero) and its while(1) search never terminates because the `for(i<dim)` accumulation leaves acc==acc1==1. floor0_inverse1's `for(j=0;j<look->m;j+=b->dim)` (:4415) likewise never advances at dim==0.

SMS-135 | REAL | src/SMS_OGG.c vorbis_synthesis: `mode = oggpack_read(opb, b->modebits)` (:4171, modebits=ilog(modes-1) at :3972) is checked only for ==-1, never against ci->modes, then `ci->mode_param[mode]->blockflag` is dereferenced immediately. When modes is not a power of two, packet values >=modes hit calloc-NULL mode_param slots — NULL deref as filed (in-array, not an overflow).

SMS-136 | REAL | src/SMS_OGG.c: floor0 order read at :3388, guard at :3395 is only `order < 1`, so order==1 passes; look->m=order (:3557). In vorbis_lsp_to_curve (~:4505-4512) aM=1 gives `c = aM>>1 = 0`, and `do{...}while(--c)` executes once, yields -1 (truthy) and loops indefinitely reading past the lsp allocation (sized look->m + dim + 1 at :4415).

SMS-140 | REAL | Final revised claim only: src/SMS_DTS.c joint_intensity[j] is a 3-bit field stored at :10771 into joint_intensity[5]; at :11404-11407 the `>0` guard defeats the -1 arm (correctly dropped in revision), but values 6/7 give source_channel=5/6 and read `subband_activity[source_channel]` past the 5-entry array. The OOB value bounds the following bit-consuming loop (inverseq/bitstream_get arms) — intra-struct OOB read / stream desync, not a steered write, exactly as revised.

SMS-141 | REAL | Missing limb (old trigger) confirmed, distinct from redo RE-SMS-024's row-0 trigger: src/SMS_WMA.c:1646-1648 `n=*ptr++; do{*q++=v}while(--n)` (and the v1 twin above it) underflows on n==0. v1 init fills only exponent_bands[0] (:1348-1361, writes [0][i] inside the k loop, never [k]); the context is zeroed (memset at :1509), so rows 1+ stay 0; decode selects the row by `frame_len_bits-block_len_bits` at :1635, reachable >0 under use_variable_block_len (flags2&0x0004, :1535) with use_exp_vlc (:1533) — zero rows supply n==0.

SMS-143 | REAL | src/SMS_MSMPEG4.c:2151 `m_SliceHeight = m_MBH / (lCode - 0x16)` with lCode>=0x17 enforced, divisor 1-9, quotient 0 whenever m_MBH<divisor; the decode loop then evaluates `m_MBY % m_SliceHeight` (:2332 region, read in the while loop after the first slice) — modulo-by-zero as filed.

SMS-144 | REAL | src/SMS_MPEG4.c _mpeg4_set_direct_mv interlaced branch (:1582-1588): lTimePP = m_PPFieldTime ± lFieldSelect ± i is used unguarded as the divisor in `* lTimePB / lTimePP` (four divisions); m_PPFieldTime is itself a rounded-div difference (derived ~:2610) that can cancel to 0, and ±field terms can also cancel it. The non-interlaced sibling path guards its time values (~:2602-2617); no equivalent guard exists here.

SMS-146 | REAL | src/SMS_FLAC.c:276-278 `_decode_subframe`: `wasted=1; while(!SMS_GetBit) ++wasted; m_CurBPS -= wasted;` with no lower-bound check; a zero-bit run drives m_CurBPS arbitrarily negative, and it is then passed as the bit count to SMS_GetSBits at :282-284 (SMS_NEG_USR32 shift macros, include/SMS.h:68). No `m_CurBPS<0` rejection exists at the site.

SMS-149 | REAL | src/SMS_MP123.c:5462-5463: lR0C=GetBits(4)+1 (1-16), lR1C=GetBits(3)+1 (1-8), `m_Reg2Start = s_BandInfo[].m_LongIdx[lR0C+lR1C]>>1` — sum range 2-24 against m_LongIdx[23] (struct field declared [23] at :27). Indices 23/24 read into the following m_LongDiff field (intra-struct) and the value bounds dequantize loops; no clamp on the sum.

SMS-150 | REAL | iop/SMSUMS/src/ums_fat.c _set_dir_chain (:482-501): _get_cluster_chain returns -1 on FAT read failure (walkers return -1 at :270/:312/:343 region), but the loop handles only lChainSize==0; -1 exits as a completed chain with i==0, and :500 unconditionally executes `apDir->m_LastCluster = lpFS->m_ClusterBuf[i-1]` = m_ClusterBuf[-1]. The sibling _get_direntry_sector_data does check `lChainSize<=0` (:401-403) — this site missed it.

SMS-151 | REAL | Final revised claim only: iop/SMSUMS/src/ums_fat.c _get_direntry_sector_data writes *apStartSec at :400 BEFORE the failing chain call and returns -1 at :403 without writing *apDirSec; _get_direntry_start_cluster discards that return at :566 and loops `for(i=0;i<lDirSec && lCont;++i)` CacheRead(lStartSec+i) with lDirSec uninitialised (declared uninit at :557) — only the loop extent is garbage, not the start sector, exactly as revised. Same ignored-return pattern at _next_direntry :652.

SMS-153 | REAL | Missing limb (old sink) confirmed, distinct from redo RE-SMS-030's IQ-table sink: src/SMS_AAC.c huffman_getescape — `uint8_t neg, i` (:6560), uncapped `for(i=4;;++i) if(!SMS_GetBit) break;` (:6570), then `off=SMS_GetBits(ld,i); j=off|(1<<i)`. i wraps at 256 as revised, so the UB window is exit i in [32,255]: SMS_GetBits (include/SMS_Bitio.h:70-78) computes SMS_NEG_USR32 = (uint32)>>(32-i) (include/SMS.h:68), a negative/oversize shift, and 1<<i is a >=32 shift. Called from the codebook-11 path (:6598-6602) on sp==±16.

SMS-154 | COVERAGE | Mechanism verified as filed in the asm text: src/SMS_DSP_FFT.S DSP_FFTGet entry (:337) sets no $v0 before `addiu $v0,$v0,32` at :363, which counts the workspace loop; prototype is void DSP_FFTGet(void*) (include/SMS_DSP.h:82), sole caller src/SMS_Spectrum.c:84. Ruling is COVERAGE per instructions: the defect lives in first-party assembly, which redo declared structure-scan only — no redo counterpart could have been produced.

SMS-155 | COVERAGE | Mechanism verified as filed: src/SMS_GS_0.S s_GSParams (:21-29) has .space 32 then PAR words NTSC 32/PAL 36/480P 40/576P 44/VESA 48/720P 52/1080I 56, while C GSParams (include/SMS_GS.h:1182-1187) has m_PARNTSC 32/PAL 36/480P 40/VESA 44/720P 48/1080I 52 — no 576P field, so every asm PAR word from offset 44 sits one slot early vs its C namesake. Latent as filed: the shifted slots (576P/VESA/720P/1080I) are all 0x3F800000 and the only C writes are m_PARNTSC/m_PARPAL (src/SMS_GS_1.c:94-95, offsets 32/36, matching). Ruling COVERAGE: mechanism lives in src/SMS_GS_0.S assembly, redo structure-scan only.

SMS-160 | REAL | CHECKED-CLEAN-CONFLICT: redo's checked-clean script-verified only first-stage maxima vs second-stage sizes; the filed defect is the declared size arrays vs actual table counts, which I script-counted at the pin: declared (src/SMS_AAC.c:2307-2309, :2746-2748) quad 114/86/185, pair 126/83/210/373 vs actual entry counts hcb1_2=113, hcb2_2=85, hcb4_2=184, hcb6_2=125, hcb8_2=83, hcb10_2=209, hcb11_2=374 — 5 of 7 non-trivial entries wrong, in both directions, exactly as filed. Any bounds check moved before the read (the SMS-123 fix) using these arrays would still admit 1-2 entries past the end (and cb11's -1 sits at its max reachable offset).

SMS-161 | REAL | src/SMS_DTS.c scale_factor_quant6[64] (:1108-1125) ends `..., 0x007EEAC6, 0x00000000` and scale_factor_quant7[128] (:1127-1160) ends `..., 0x007EEAC6, 0, 0, 0`; my script confirms the ONLY decreases in either table are at index 62->63 and 124->125 respectively (tables are non-decreasing, with duplicate steps early — the finding's "strictly increasing" wording is imprecise, the zero-tail defect is exact). Indices are directly bitstream-reachable in-bounds: mode 5 `scale_sum=bitstream_get(6)` and mode 6 `bitstream_get(7)` (:11380-11382, table selected :11369-11370), LFE `scale_factor_quant7[bitstream_get(8)]` at :11440 — top scale factors decode as 0/silence.

SMS-165 | REAL | iop/USBD/src/usbd.c processDoneQueue_IsoTd: outer TD's LUT slot cleared at :76 (`hcIsoTdToIoReqLUT[arg-...]=NULL`); inside the halted-ED cleanup loop the TD being freed is read via curTd at :104 but the clear at :106 again writes `hcIsoTdToIoReqLUT[arg - hcIsoTdBuf] = NULL` instead of curTd's slot, leaving each walked TD's stale IoRequest pointer in the LUT. The generic twin at :181-183 indexes correctly by tdListPos.

SMS-168 | REAL | iop/SMSMAP/smap.c: _smap_phy_reset is declared `int` (:311) but SMap_Reset (void, :692-697) calls it at :695 and discards the result. SMAP_F_LINK (smap.h:25) is only ever OR-set on the asm success path (~:510-513, lw/or/sw of 1<<1 into m_Flags offset 0) — grep shows no clear of m_Flags anywhere in smap.c/main.c — and SMap_Send at :904 returns SMap_Con only when the flag is clear, so after one successful reset a later dead-PHY reset (main.c:167 re-reset path) leaves link reported up and sends returning SMap_OK.

SMS-187 | REAL | Missing limb confirmed verbatim: contract at src/SMS_List.c:36-41 (SMS_ListPushBack returns NULL on calloc failure), and the four sites absent from redo RE-SMS-036 all dereference it unchecked via `->m_Param =`: src/SMS_ContainerM3U.c:317, :394, :433 and src/SMS_GUIFileCtxMenu.c:394 (grep-verified at the pin, pattern identical to the redo-covered sites). NULL-calloc failure at any of them is an immediate NULL write.

SMS-188 | REAL | Defect mechanism lives in the redo-READ caller, not only the sampled header: src/SMS_SubtitleContext.c _sb_string_width loops `while(apStr <= lpEnd)` at :108 where lpEnd=apStr+aLen, while both renderers in the same file use `<` (:58, :142). The extra iteration maps the terminator '\0' via `lChr -= ' '` to (unsigned char)224 and calls GSFont_CharWidth(lpHdr,224); the sink inline at include/SMS_GS.h:1307-1309 reads `((unsigned char*)apHdr+11)[aChr]` with no bound against the 96-glyph header. Because the off-by-one loop bound is in SMS_SubtitleContext.c (read by redo), this is REAL, not COVERAGE.

---
Counts: REAL 21 (incl. PARTIAL missing limbs 141/153/187 ruled REAL, and 160 tagged CHECKED-CLEAN-CONFLICT) · COVERAGE 2 (154, 155 — assembly, mechanisms verified as filed) · NOT-REAL 0.

### E. Redo-only, RE-SMS-003…052 (adj-E)
# Adjudication E — REDO-only SMS items (pin e65ced39a27e)

RE-SMS-003 | REAL | iop/SMSTCPIP/src/ip_frag.c:198-199 swaps ip_reasslen, :201 uses it only for IPH_LEN_SET but :213 passes the swapped value to pbuf_alloc and :222 uses it as the copy bound.
pbuf.c:176-185: PBUF_POOL returns a single pool pbuf and sets p->len=p->tot_len=length blindly; lwipopts.h:63 PBUF_POOL_BUFSIZE 1540, so e.g. 300 (0x012C) becomes 11265 and overruns it. All C source, so no COVERAGE.

RE-SMS-007 | REAL | src/SMS_IOP.c:237-243: ret=lzma2_uncompress(...); `if (ret > 0) ret = SifExecModuleBuffer(irx_data, irx_size, ...)`; memalign at :239 unchecked.
include/xz.h:112-122: XZ_OK=0, XZ_STREAM_END=1, and every error (UNSUPPORTED_CHECK=2 through BUF_ERROR=8) is >0, so corrupt/truncated blobs are executed. All C source, so no COVERAGE.

RE-SMS-011 | REAL | Final (drift-corrected) claim: src/SMS_DTS.c:11644 aLen=DTS_SyncInfo(...) (header-controlled, max 16384), :11645 retVal=lnSamples<<2, with no reconciliation between them.
:11693 `memset(lpOutBuf + 8 + aLen, 0, retVal - 8 - aLen)` — when aLen > retVal-8 the int length goes negative and becomes a huge size_t; the preceding byte-swap copy at :11666-11691 also copies aLen bytes into the retVal-sized buffer. All C source, so no COVERAGE.

RE-SMS-017 | REAL | Final REVISED scope only: iop/SMSCDVD/SMSCDVD_UDFS.c:293-295 lLEA/lLAD are disc u32s, p=176+lLEA unchecked; :297-347 loop reads UDFAD(&apData[p],...) with p+=lLAD and no bound (sum can wrap).
apData points into s_Buffer[9*2064]=18576 bytes (SMSCDVD.c:196), not 2048 — a u32 lLEA still reaches far past it. The filed UDFDecode write overflow is NOT part of the final claim: :138 is constant 128 exact-fit and :399 is u8 lLFI<=255 into s_ScanTemp[2048] (UDFS.c:69); surviving decode defects are the latent contract plus the OOB source read via 38+lLIU at :399 (UDFDecode itself, :84-101, takes no destination size). All C source, so no COVERAGE.

RE-SMS-018 | REAL | iop/SMSUMS/src/ums_fat.c:607 `unsigned char lName[257]` in _file_start_cluster; :623 `lName[lOff++] = apName[i++]` in the component loop with no bound on lOff.
:626 then writes the terminator at lName[lOff]; a single path component over 256 bytes overruns the stack buffer. All C source, so no COVERAGE.

RE-SMS-019 | REAL | iop/SMSUMS/src/ums_driver.c:214 lnLUN=UmsGetMaxLUN(apDev) (device GET MAX LUN byte, function at :283-298); :216 `for (i = 0; i <= lnLUN; ++i)` writes apDev->m_LUName[i] at :232-233 and :247-248.
iop/SMSUMS/include/ums.h:54: `char m_LUName[16][19]` — lnLUN > 15 writes past the 16-row table into the rest of USBMDevice. All C source, so no COVERAGE.

RE-SMS-021 | REAL | src/SMS_InverseCodePages.c:145 loop runs `while (length < bufSize)` writing one byte per iteration (:152/:175), i.e. up to bufSize bytes, then :184 unconditionally writes `*outBuff = '\0'` at outBuff[bufSize] — bufSize+1 bytes total.
Callers do not compensate: src/SMS_SubtitleContext.c:483 passes SUB_MAXLEN for `char lBuff[SUB_MAXLEN]`, and :657-659 passes the remaining space in lLine[SUB_MAXLEN] in place. All C source, so no COVERAGE.

RE-SMS-025 | REAL | Final REVISED scope only (UDP cross-struct headline refuted: ip.h:75-82 IP_PCB puts so_options/tos/ttl in the common prefix of both PCB types, and sockets.c:955 returns early for non-TCP at IPPROTO_TCP level).
Surviving defect confirmed: sockets.c:1004, :1037, :1041 (getsockopt) and :1197, :1210, :1214 (setsockopt) dereference sock->conn->pcb.tcp->... at SOL_SOCKET/IPPROTO_IP level with no NULL check, while api_msg.c:183 err_tcp sets conn->pcb.tcp=NULL — a post-error NULL dereference. All C source, so no COVERAGE.

RE-SMS-026 | REAL | iop/SMSTCPIP/src/api_msg.c:240 setup_tcp(newconn) installs recv/sent/err callbacks with arg=newconn on the live PCB (setup_tcp at :207-218) before any allocation that can fail.
:241-257: on sys_mbox_new/sys_sem_new failure each arm does memp_free(MEMP_NETCONN, newconn) and returns ERR_MEM — the PCB is never closed and its callbacks never cleared, leaving it pointing at freed memory (trigger, alloc failure plus later traffic, is INFERRED as filed). All C source, so no COVERAGE.

RE-SMS-027 | REAL | iop/SMSTCPIP/src/tcp_in.c:523-527 SYN_SENT branch: the debug print and the accept condition both dereference pcb->unacked->tcphdr->seqno with no NULL check.
tcp.c:516 sets state=SYN_SENT before :525 tcp_enqueue(...) and the state persists on enqueue error (return at :529); loopif.c:86-95 in-tree comment documents pcb->unacked still being NULL in tcp_process. All C source, so no COVERAGE.

RE-SMS-031 | REAL | src/SMS_AAC.c:5030 reachable overwrite: `if (sbr->bs_extension_id == 3 && sbr->bs_extension_data == 42) Xsbr[...][0] = 16428320;` in hf_assembly — a stream payload forcibly replaces a QMF sample.
Dead remnants confirmed: :6552-6557 huffman_codebook() returns the two halves of 16428320 and :6589-6594 case 12 overwrites the decoded pair with it, but :6304 section_data returns error 32 for sect_cb==12 before spectral decoding; :3809 `if (16428320 <= samplerate) return 11;` is unreachable because every rate >= that already matched an earlier >= test (e.g. :3808 >=9391). Not SMS-123's mechanism (two-step Huffman offset-before-check) — no withdrawn-overlap. All C source, so no COVERAGE.

RE-SMS-034 | REAL | src/SMS_MPEG.c:42-50 _mpeg_alloc_picture performs the callocs (MBSkip/QScale/MBType, MotionValBase, RefIdx) unchecked and immediately derives m_pMotionVal[i] = m_pMotionValBase[i] + 2 at :49 (NULL base becomes a 0x2-based array).
:130-143 SMS_MPEGContext_FindUnusedPic explicitly returns NULL when no picture is free; :181-185 SMS_MPEG_FrameStart assigns its result to lpPic and dereferences lpPic->m_Ref and calls _mpeg_alloc_picture(lpPic) with no NULL test. All C source, so no COVERAGE.

RE-SMS-035 | REAL | src/SMS_VLC.c:223-227 SMS_RL_Init mallocs m_pMaxLevel/m_pMaxRun and memcpy's into them on the next line, unchecked; :268-272 SMS_VLC_RL_Init mallocs m_pRLVLC[q] for q=0..31 and immediately fills m_TableSize entries, unchecked.
src/SMS_MPEG4.c:3391-3392: `g_MPEGCtx.m_pBSBuf = realloc(g_MPEGCtx.m_pBSBuf, ...)` then memcpy into the result unchecked — failure loses the old buffer and NULL-dereferences. All C source, so no COVERAGE.

RE-SMS-040 | REAL | src/SMS_DirTree.c:50 SMS_DirTreeInit callocs retVal and :61 passes &retVal->m_Root to SMS_DirAdd unchecked; :49-53 VLA lRoot[strlen+2] and lRoot[lLen-1] read with no empty-string guard (index -1 for an empty root).
Unbounded recursion confirmed at :153 (SMS_DirTreeScan self-call), :224 (SMS_DirTreeWalk self-call) and :258 (SMS_DirTreeDestroy self-call), no depth counter anywhere; callers src/SMS_CopyTree.c:150-151 use the init result unchecked and do strrchr(apSrc,'/')+1 with no NULL check, likewise :252. All C source, so no COVERAGE.

RE-SMS-043 | REAL | iop/SMSTCPIP/src/icmp.c:136 q=pbuf_alloc(PBUF_IP, 8+IP_HLEN+8, PBUF_RAM); :141 `idur = q->payload` dereferences q with no NULL check.
:145 `mips_memcpy(q->payload + 8, p->payload, IP_HLEN + 8)` with no check that p->tot_len >= IP_HLEN+8, over-reading a short offending packet; the same unchecked pattern repeats at :169 (icmp_time_exceeded, compiled out under IP_FORWARD=0 as filed). All C source, so no COVERAGE.

RE-SMS-044 | REAL | iop/SMSTCPIP/src/ip_frag.c:262-263 rambuf=pbuf_alloc(PBUF_LINK, 0, PBUF_REF) then `rambuf->tot_len = rambuf->len = mtu` on the next line, unchecked.
:310-312 header=pbuf_alloc(PBUF_LINK, 0, PBUF_RAM) then pbuf_chain(header, rambuf) and netif->output(netif, header, dest) unchecked — pbuf_cat tolerates a NULL head but the output/pbuf_header path on a NULL header does not. All C source, so no COVERAGE.

RE-SMS-046 | REAL | Final REVISED scope only: iop/SMSTCPIP/src/api_lib.c:403 netconn_recv dereferences conn->pcb.tcp->state; :399's conn->err check defeats a sequential post-reset recv, so the deref needs the narrow race the revision states.
The race is real in code: api_msg.c:183 err_tcp sets conn->pcb.tcp=NULL before :186 sets conn->err, so a recv that passed :399 while err==ERR_OK can be NULLed before :403 executes. The filed UDP NULL-buf half is not part of the final claim (no in-tree poster). All C source, so no COVERAGE.

RE-SMS-047 | REAL | iop/SMSTCPIP/src/sockets.c:355-363 lwip_recvfrom: addr=netbuf_fromaddr(buf), then :363 `sin.sin_addr.s_addr = addr->addr` with no NULL check on addr.
api.h:115 netbuf_fromaddr is the bare macro (b->fromaddr), and api_lib.c:431-434 the TCP arm of netconn_recv explicitly sets buf->fromaddr=NULL — so recvfrom with a from buffer on any TCP socket NULL-dereferences; UDP buffers always carry a fromaddr. All C source, so no COVERAGE.

RE-SMS-050 | REAL | iop/SMSUdpfs/udpfs/src/udpfs_core.c:72-80 _request returns the raw received count (only ret<0 becomes -EIO); g_rx_buf is the shared static 312-byte buffer at :35, so an unreceived tail is the previous reply.
Callers check only ret<0 then read the full reply struct: _path_op :166-171 (neither length nor msg_type, returns reply->result), open :373-383, lseek :519-532, getstat :622-642; dread :582-587 compares name_len only with name_max, never with ret-sizeof(reply), and returns success on the clamp path. Contrast in-file: _recv_with_result :89-136 clamps a wire count to bytes actually received. All C source, so no COVERAGE.

RE-SMS-051 | REAL | iop/smbman-sms/src/smb.c:910 and :923 smb_QueryPathInformation casts &SMB_buf.smb.u8buff[QPIRsp->smbTrans.DataOffset] to Basic/StandardFileInfo and reads fields at :912-929 without relating DataOffset/DataCount to the received length r.
:799-838 smb_QueryInformationDisk checks only r<=0 (:814), magic (:818) and status (:821), then reads TotalUnits/BlocksPerUnit/BlockSize/FreeUnits at :832-835 from a possibly truncated reply — while sibling functions in the same file (NetShareEnum, FindFirstNext2, ReadAndX) validate offsets/counts against r. All C source, so no COVERAGE.

RE-SMS-052 | REAL | iop/smbman-sms/src/smb.c:1648-1664 smb_Echo: `memcpy(&ER->ByteField[0], echo, (u16)len)` at :1661 and ByteCount=(u16)len at :1662, with no cap against the SMB buffer.
Buffer extent re-derived from smb.h:892-893 (MAX_SMB_BUF 4096 + MAX_SMB_BUF_HDR 128 = 4224); the len is EE-supplied via the devctl SMB_DEVCTL_ECHO path (smb_fio.c), and the (u16) cast truncates larger values so copied count and caller intent can disagree. Internal fixed 18/4-byte callers are safe, as filed — exposure is the devctl entry. All C source, so no COVERAGE.

### F1. Redo-only, RE-SMS-056…086 (adj-F1)
RE-SMS-056 | REAL | src/SMS_ContainerASF.c:537 `pkt.data=realloc(pkt.data, pkt.alloc=pkt.size)` assigns capacity inside the call — on failure data becomes NULL (old buffer lost) while alloc claims the new size; pkt.size is file-controlled, bounded only <2^24 at :433. Downstream !pkt.data guard at :565ff prevents a NULL write today, exactly as the final claim scopes it. No overlap with withdrawn SMS-013/015/089/123.
RE-SMS-058 | REAL | src/SMS_ContainerMOV.c:311-313 `_mov_read_hdlr_m4a` frees codec+stream and NULLs `m_pStm[m_nStm-1]` without decrementing m_nStm; :225-226 `_mov_read_mdhd` takes that slot and dereferences `lpStm->m_pCtx` with no NULL test (contrast guarded `_mov_read_elst` :242). Compaction only later at :1072-1085, and M4A swaps hdlr into the same parse table at :1544. Distinct from withdrawn SMS-015 (MOV entry-count malloc / _tbl_fits), not an overlap.
RE-SMS-060 | REAL | Final REVISED claim only: src/SMS_WMA.c:1247 `block_align=apCtx->m_BlockAlign` with no validation in wma_init; superframe loop :2047 `lFrmSize=block_align`, :2053 `SMS_InitGetBits(lpBitCtx,lpBuf,lFrmSize*8)` never reconciled with remaining lBufSize (partial-final-frame over-read), and :2093-2094 advance by lFrmSize so 0 gives provable non-advance. Hang is conditional on wma_decode_frame not erroring first, as the revision states.
RE-SMS-061 | NOT-REAL | Defeating bound in parsed data: src/SMS_AC3.c:293 s_LATab[256] indexed unclamped at :1170/:1176, and index 256 needs lDelta exactly -512, but _ac3_parse_exponents (:628-703) steps exponents only via s_Exp1/2/3 (valid deltas -2..+2; the 25 entries abort via `anExp>24` returns :638/:659/:680), capping the case -1 index at <=255. The final claim's residual EXP_REUSE stale-boundary path is expressly unproven with no evidence it lands in-band at the required value — mechanism not confirmed at the pin.
RE-SMS-065 | REAL | iop/SMSTCPIP/src/mem.c:113-130 mem_free() has only a NULL/range check, then :126 `mem->used=0` and :130 plug_holes(mem) with no test that the block was in use; plug_holes (:64-90) merges neighbours on that cleared flag, so a double free re-runs merging on stale offsets and corrupts the free list. Finding asserts no in-tree double free — hardening claim as filed.
RE-SMS-066 | REAL | iop/SMSTCPIP/src/ps2ip.c:93 `strcpy(pInfo->netif_name, pszName)` into `char netif_name[4]` (iop/SMSTCPIP/include/ps2ip_internal.h:22), overwriting ipaddr/netmask/gw in the caller's struct for any >3-char name that netif_find matches; iop/SMSTCPIP/src/netif.c:121 `num=name[2]-'0'` is read before any length validation, so a 0-2 char name is read past its end.
RE-SMS-067 | REAL | iop/SMSTCPIP/src/api_msg.c:177-186 err_tcp does `conn=arg; conn->pcb.tcp=NULL; conn->err=err;` and further recvmbox/mbox/sem derefs with no NULL check, while siblings in the same file guard: recv_tcp :114 `if(conn)`, poll_tcp :151, sent_tcp :165, do_connected returns ERR_VAL on NULL. Only unchecked PCB callback, as claimed.
RE-SMS-071 | REAL | Final REVISED (export-surface hardening) claim confirmed: iop/SMSDev9/src/ps2dev9.c:85-87 fixed arrays dev9_intr_cbs[7], pre/post_dma_cbs[2]; :170-186 registration functions index them by intr/aDev with no range check; :301/:309 dev9DmaTransfer uses ctrl raw for the callback arrays despite masking it (:296-297) for the hardware register. All in-tree callers pass constants — no in-tree reachability claimed in the final scope.
RE-SMS-073 | REAL | iop/SMSTCPIP/src/api_lib.c:146 netconn_new_with_proto_and_callback sets `conn->sem=SYS_SEM_NULL` and never creates it, while netconn_write waits unguarded at :521 and :545 `WaitSema(conn->sem)`; accepted conns do get one (api_msg.c:252 `newconn->sem=sys_sem_new(0)`), netconn_close guards its wait (:573), and poll_tcp/sent_tcp skip SYS_SEM_NULL semaphores so client conns can never be signalled.
RE-SMS-075 | REAL | iop/SMSTCPIP/src/tcpip.c:110-122 tcpip_apimsg() on MEMP_TCPIP_MSG exhaustion does `else memp_free(MEMP_API_MSG, apimsg)` — silently dropping the request; api_lib.c callers (e.g. netconn_new :158-159, netconn_delete :180-181, netconn_write :538-539) then block in sys_mbox_fetch(conn->mbox) with no timeout for a reply only the dropped do_* handler would post.
RE-SMS-076 | REAL | iop/SMSTCPIP/src/sockets.c:454-455 lwip_sendto dereferences `to` (`((sockaddr_in*)to)->sin_addr/sin_port`) with no NULL check, and discards all re-connect results: netconn_connect :461, restore netconn_connect :468, netconn_disconnect :470 — a failed first connect still sends via lwip_send :463 to the previous/unconnected peer and returns its result.
RE-SMS-079 | REAL | iop/SMSTCPIP/src/tcp_out.c:538-567 tcp_keepalive fills src/dest/seqno/ackno/wnd/urgp/chksum but never sets flags — no TCPH_FLAGS_SET in the function (siblings at :219, :365, :492); :564 TCPH_HDRLEN_SET preserves existing flags (include/lwip/tcp.h:179 `...|TCPH_FLAGS(phdr)`), and the PBUF_RAM buffer comes from mem_malloc, whose heap is zeroed only in mem_init (mem.c:97), so recycled flags (SYN/RST/FIN) leak into the probe.
RE-SMS-080 | REAL | iop/SMSTCPIP/src/udp.c:431-440 udp_send chains a new header pbuf q in front of p when pbuf_header fails, then :455-458 `if(ip_route(...)==NULL) return ERR_RTE;` with no free; the normal exit :500-504 does `if(q!=p) pbuf_free(q)`. Leak is exactly the chained case (q!=p), as claimed.
RE-SMS-081 | REAL | Final REVISED claim (mbox leak only) confirmed: iop/SMSTCPIP/src/api_lib.c:138-164 netconn_new_with_proto_and_callback's `error:` label frees only the netconn (memp_free), reached after mbox creation from the MEMP_API_MSG failure :152 and the `conn->err!=ERR_OK` check :162 — conn->mbox is never sys_mbox_free'd on either path. The withdrawn PCB half is correctly excluded: do_newconn (api_msg.c:287ff) sets err only where udp_new()/tcp_new() returned NULL.
RE-SMS-083 | REAL | src/SMS_ContainerOGG.c:70 probe returns `channels<3 ? 1 : -1`; SMS_GetContainerOGG :96 assigns that value and returns it verbatim (:126) even though it only builds on >0. src/SMS_Container.c:192 probe loop breaks on any non-zero status and :209-214 destroys the container when lSts<0, so a positively-identified >=3-channel Vorbis file vetoes all later creators — unlike the other creators' 0-for-not-mine contract.
RE-SMS-086 | REAL | Final STANDING-DRIFT scope (m_BitRate only) confirmed: src/SMS_AC3.c:2673 declares uninitialised `lSampleRate,lBitRate`; on SyncInfo failure (:2712, lLen==0) neither is written, yet :2735 `apCtx->m_BitRate=lBitRate` sits outside the if/else (m_SampleRate is correctly inside success at :2720). Identical structure in src/SMS_DTS.c:11763, with m_SampleRate inside success at :11750. No overlap with withdrawn SMS-123 (AAC Huffman offset), SMS-013, SMS-015 or SMS-089.

### F2. Redo-only, RE-SMS-087…150 + Cat-4 truth checks (adj-F2)
RE-SMS-087 | REAL | src/SMS_AAC.c:7952-7957 default case fills lppChan[0..4] from internal_channel[0..4] and calls pcm_synN; internal_channel is memset 0 at SMS_AAC.c:8167, so 3-4ch frames leave slots 3/4 = 0 (channel 0). src/SMS_Utils.S pcm_synN loads all five pointers (0,4,8,12,16($a1)) unconditionally with fixed weights, no channel-count arg. No overlap with withdrawn old SMS-013/015/089/123.
RE-SMS-088 | REAL | src/SMS_MPEG4.c:3362-3379 scan loops `for (i=lCurPos; i<lBufSize-3; ++i)` but tests `SMS_unaligned32(lpBuf)==0xB6010000` with no `+i` offset every iteration. Same four bytes re-tested; packed-B stash only fires if start code at offset 0. No overlap with withdrawn old items.
RE-SMS-089 | REAL | src/SMS_DXSB.c: grep shows m_EndPTS written nowhere except use at :318 `SMS_Rescale(lpFrame->m_EndPTS,...)`; _decode (:195-232) memaligns (uninitialised) and sets m_StartPTS/dimensions/pixmap but never m_EndPTS. Distinct from withdrawn old SMS-089 (JPEG marker walk in SMS_Artwork.c) — no overlap.
RE-SMS-091 | REAL | src/SMS_MC.c:307-316 `int retVal; MC_Seek(...); MC_Sync(&retVal); return retVal;` uninitialised; MC_Sync :75-92 writes *apRes only `if (s_LastCmd)`, and MC_Seek returns 0 without setting s_LastCmd when a command is in flight/RPC fails. Sibling MC_ReadS :265 starts `retVal=0`.
RE-SMS-092 | REAL | src/SMS_FileDir.c:88-114 SMS_SubContID sets `lpExt=apName+lLen-4` (points at "flac") and in lLen>5 branch compares `stricmp(lpExt,s_pFLAC)` where s_pFLAC=".flac" (:65) — never equal; siblings SMS_ContID :168 and SMS_FileID :224 do `--lpExt` first. FLAC sub-container ID unreachable.
RE-SMS-093 | REAL | iop/SMSCDVD/SMSCDVD.c:794 subdirectory: `m_SectorNum=(localTocEntry.m_FileSize>>11)+((CDVolDesc.m_RootToc.tocSize&2047)!=0)` — numerator is subdirectory's, remainder tests root's size; root init at :674 correctly uses root size in both terms. Copy-paste slip confirmed.
RE-SMS-094 | REAL | iop/SMSUdpfs/smap/src/smap.c:137-147 InitPHY: `for(i=0; read&SMAP_PHY_BMCR_RST; i++){ if(i<=0) return -1; DelayThread(1000); }` — guard true on first poll, loop can never wait for reset to clear. Hardware timing INFERRED as filed, code defect READ.
RE-SMS-096 | REAL | src/SMS_InverseCodePages.c:114-136 UnicodeToCP starts `index=64, step=64` and does `step>>=1` before index update inside loop; stepping left from 64 reaches 1 with step 0, index 0 never probed. Tables (:22ff, first entry 0x0081 etc.) are monotonic — walk, not data, is wrong, matching simulation claim.
RE-SMS-098 | REAL | include/libds34bt.h (35 lines) and include/libds34usb.h (21 lines) are the only 2 of 89 headers with no guard (grep -L ifndef/pragma returns only these two); final narrowed claim holds: only 7 zero-arg prototypes use K&R `()` (bt 4 at grep count, usb 3), e.g. `int ds34bt_init();`, `int ds34usb_reset();`, vs (void) elsewhere.
RE-SMS-100 | REAL | FINAL narrowed claim only: src/SMS_FLAC.c:438-441 `_synth_indep[2]` indexed by `m_nChannels-1` with no decoder-internal bound — latent only, gated by container SMS_ContainerFLAC.c:114 (`m_nChannels<3`) and _decode_frame assignment check SMS_FLAC.c:322-326 limiting index to 0/1. Divide limb correctly dropped in final claim: channels are 3-bit (ContainerFLAC.c:66, max 8) so `4096>>nChannels` at SMS_FLAC.c:523 is >=16, never 0.
RE-SMS-146 | REAL | src/SMS_FileContext.c:805-820 _read_string: on Read success, `if(lLen && (*apBuf=malloc(lLen+1))) strcpy(...)` and else-branch only for Read failure — empty field (lLen==0) never assigns *apBuf; caller at :863 passes &lpNode->m_pName where lpNode is bare malloc(sizeof(CDDADirectory)) uninitialised. Distinct from old SMS-185 (iop/SMSCDVD/SMSCDVD.c:1062 TocEntryCopy terminator past m_Filename[129]) — different file/function/mechanism, no overlap; also distinct from RE-SMS-062 unterminated strlen.
RE-SMS-147 | REAL | iop/SMSTCPIP/src/sockets.c:810,836-839 lwip_getpeername and :844,870-873 lwip_getsockname both do `if(*namelen>sizeof(sin)) *namelen=...; memcpy(name,&sin,*namelen)` with no NULL check on namelen (or name). DISTINCT from old SMS-183: SMS-183 is ignored netconn_peer/netconn_addr return value copying garbage sin as success — different argument, sink, and fix.
RE-SMS-148 | REAL | iop/SMSTCPIP/src/api_lib.c:236-251 netconn_addr switch handles only NETCONN_UDP*/TCP cases, no default, then `return (conn->err=ERR_OK)` at :251 with *addr/*port unset for other types. DISTINCT from old SMS-183 (caller ignores that return) and from RE-SMS-077 NULL-PCB deref inside the UDP/TCP branches — this is unset outputs on success for non-UDP/TCP types.
RE-SMS-150 | REAL | src/SMS_Container.c:198 explicit path `} else s_CCreator[aPrefCont](retVal);` discards return; lSts initialised 0 at :133 and only assigned on auto-probe path (:192), so teardown `if(retVal && lSts<0)` at :209 can never fire for explicit probes. Distinct from RE-SMS-101 bounds claim at same line.
RE-SMS-054 | REAL | Unbounded whole-file loads confirmed: src/SMS_Locale.c:841-845,874-892 `malloc(lSize+1); fioRead/MC_ReadS(...,lSize)` with lSize from seek-end, no cap; src/SMS_Config.c:286-307 _load_font `if(lSize>lFontSize) lpBuff=malloc(lSize)` then whole-file read, no cap; src/SMS_GUIFileCtxMenu.c:143-196 _copy_file_to_mc whole-file malloc(lSize)+read despite streaming SMS_CopyFile in-tree; src/SMS_RC.c:82-138 _load_rmman `lpData=malloc(lSize); fioRead(lpData,lSize); _patch_version; SifExecModuleBuffer` with no NULL check at all. No overlap with withdrawn old SMS-013/015/089/123.
RE-SMS-105 | REAL | src/SMS_AC3.c:323 `static sample_t s_Samples[16*256*12]` = 49,152 floats = 192 KB BSS; uses only at :360 memset, :2305 base (+256 LFE), :2401-2480 max `lpSamples+1536+256*i` i<5 plus 256-wide IMDCT write, :2612 converter — highest index 256+1536+1024+255=3,071 floats (~12 KB) as claimed. Proposed 4,096-float shrink retains headroom. No overlap with withdrawn old items.

### G. Number disputes (adj-G)
# SMS number-dispute adjudication — pin e65ced39a27e (Simple-Media-System, read-only clone)

DISPUTE 1 (SMS-105 vs RE-SMS-002) | WINNER: OLD | Method: read src/SMS_MPEG4.c:933,1174,1183-1201,1213-1227,1236-1352 — `lD[4][2]` stores indices 0-3 (4 points), so the write first overflows at count 5 (i=4), but the code only *uses* counts 0-3: the switch at :1236 has cases 0,1,2,3 only and the lSpriteRef computation reads only lD[0],lD[1],lD[2]; count 4 is in-bounds yet unhandled (no switch case, stale SpriteOffset/Delta, RealSpriteWarpPts=4 passed on). Correct fix threshold is therefore reject `m_nSpriteWarpPts > 3` (OLD); REDO's `>4` correctly identifies the overflow start (5) but would admit the unsupported count 4, and its "at most 4 warping points" is the total including the implicit origin, i.e. field value 3.

DISPUTE 2 (SMS-117 vs RE-SMS-023) | WINNER: OLD | Method: extracted `excluded_channels` (src/SMS_AAC.c:7548-7561; `exclude_mask[64]`/`additional_excluded_chns[64]` at :333-334, MAX_CHANNELS=64 at :110) and simulated it in Python with continuation bit always 1 — initial write 0-6, then rounds write 7-13, 14-20, 21-27, 28-34, 35-41, 42-48, 49-55, 56-62, and round 9 writes 63-69, i.e. the first out-of-bounds index (64) occurs in the 9th continuation round/iteration, not after the 10th. Correct figure: 9th round (OLD, final revised; REDO's own verification in Redo-VERIFY-SMS.md independently corrected RE-SMS-023 to the same 9th).

DISPUTE 3 (SMS-142 vs RE-SMS-085) | WINNER: OLD | Method: read every `sample_rate` division in `wma_init`, src/SMS_WMA.c (note: both bodies cite SMS_WMA.c, not SMS_DTS.c) — exactly 2 integer-division sites by `sample_rate`: :1352 `pos = ((block_len*2*a)+(b>>1))/b` and :1383 `pos = ((block_len*2*a)+(b<<1))/(4*b)`, with `b = lpCtx->sample_rate` (int); the third site REDO counts, `high_band_start` at :1396-1398 `(block_len*2*high_freq)/lpCtx->sample_rate + 0.5F`, is a float division because `high_freq` is declared `float` (set to `sample_rate * 0.5F` at :1275), as is the `bps` division at :1291 which neither side claims. Correct figure: 2 integer divisions (OLD, final revised); REDO's 3 misclassifies the float division.

DISPUTE 4 (SMS-145 vs RE-SMS-033) | WINNER: REDO | Method: script-parsed src/SMS_MP123.c tables and index formula — `s_GainPow2` = 378 entries (:190, script-counted); base `m_pPow2Gain = s_GainPow2 + 256 - GetBits(8)` (:5422, max 256 at GetBits=0) + 2 for MS-stereo (:5424) = 258; `m_pFullGain` adds `GetBits(3)<<3` ≤ 56 (:5435-5437); scalefactor lengths from `sl_Len` (_1, max 4 bits → 15) and from script-decoded 3-bit chunks of `s_nSLen2[512]` (max chunk 4) / `s_iSLen2[256]` (max chunk 5 → scalefactor max 31, reachable on the intensity-stereo channel), with `lShift = 1 + ScaleFacScale` ≤ 2 (:5661) and pretab max 3 (s_PreTab1). Recomputed maxima: direct `m_pPow2Gain` read 258 + (31<<2) = 382; pretab read 258 + ((31+3)<<2) = 394; FullGain read 258 + 56 + (31<<2) = 438 — the global maximum reachable index is 438. OLD's 436 is the FullGain figure without the +2 MS-stereo term (256+56+124); REDO as filed (~508) wrongly assumed 6-bit/63 scalefactors, but REDO's verification (382 direct / 438 FullGain) is exactly right, so REDO's final position wins.

DISPUTE 5 (SMS-171 vs RE-SMS-070) | WINNER: OLD | Method: read the declaration, iop/SMSMAP/smap.c:888 `static u32 sl_TXBuf[(SMAP_TXMAXSIZE + SMAP_TXMAXTAILPAD + 3)/4]` with SMAP_TXMAXSIZE = 6+6+2+1500 = 1514 (:17) and tail pad 4 (:18) — (1514+4+3)/4 = 380 u32 words (integer truncation), so `sizeof(sl_TXBuf)` = 380 × 4 = 1,520 B; no `sizeof(sl_TXBuf)` use exists in the code, the buffer is used via cast at :913/:920. Correct figure: 1,520 B allocated (OLD). REDO's 1,518 B is a different, pre-rounding measure — the nominal 1514 + 4 sum before the +3/word-rounding in the declaration (REDO's verification itself notes "1518 requested, but the u32 array rounds the allocation to 1520 bytes").

DISPUTE 6 (SMS-187/SMS-068 vs RE-SMS-036) | WINNER: OLD | Method: multi-line-aware script grep of src/*.c for the stated definition — a `SMS_ListPushBack(...)` call whose return is immediately dereferenced as `) -> m_Param =` with no NULL check (the checked assignment-then-test site at SMS_GUIFileMenu.c:795 is excluded, as are the distinct `m_pTail->m_Param` tail variants in SMS_PlayerControl.c:382,393 and SMS_SubtitleContext.c:279,335) — count = 26 sites: SMS_Config.c 1 (:366), SMS_ContainerM3U.c 3 (:317,394,433), SMS_FileDir.c 7 (:311,340,392,437,453,519,593), SMS_GUI.c 1 (:929), SMS_GUIDevMenu.c 1 (:330), SMS_GUIFileCtxMenu.c 1 (:394), SMS_GUIFileMenu.c 7 (:457,547,602,618,636,832,1620), SMS_GUIMenu.c 1 (:381), SMS_GUISMBrowser.c 2 (:563,571), SMS_History.c 1 (:140), SMS_List.c 1 (:325, in SMS_ListAppend). Correct total: 26, supporting OLD's claimed total (12 + 14). The difference: REDO's filed 18 undercounts, and REDO verification's 21 is exactly the single-line grep count — it misses the 5 multi-line GUIFileMenu calls (:602,618,636,832,1620) where `) -> m_Param` falls on a following line.

DISPUTE 7 (SMS-036 vs RE-SMS-102) | WINNER: BOTH-WRONG | Method: `wc -l` on every file in iop/SMSSMB/ at the pin — SMSMB.c 2,002, ps2smb.c 1,857, smsutils.h 35, Makefile 32; whole tree (the 4 files both bodies propose deleting) = 3,926 lines / 108,596 B. OLD's 3,859 = 2,002 + 1,857, i.e. only the two .c files, despite its own text saying "4 files" (it omits smsutils.h 35 + Makefile 32 = 67 lines; OLD's own verification, Sable-VERIFY2-SMS.md, already corrected the whole-tree total to 3,926). REDO's 3,894 = 2,002 + 1,857 + 35, exact for the three source files it enumerates, but it omits the Makefile's 32 lines, so it is also not the whole-tree total. Correct whole-tree figure: 3,926 lines.
