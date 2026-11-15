# SMS Blind Redo — W8: Headers, Build, CI, Docs, Repo Clutter

Repo: `NathanNeurotic/Simple-Media-System` clone at pin `e65ced39a27eee8d996e9257e21594d1cac1c93d` (verified `git rev-parse HEAD`). Read-only; nothing in the clone was edited. Nothing under `~/workspace/Sable-audit/` was read, listed, or grepped.

## (a) Coverage note

- **Headers:** all 89 files in `include/` censused by script (12,827 lines total, matches task figure). Include-guard check on all 89 (ifndef/define pairing + tail `#endif`). Declaration census: 649 unique function prototypes parsed from `include/*.h` after comment stripping; each name was then counted by a token census over every `.c/.h/.s/.S` in `src/`, `include/`, `iop/`, and definitions were searched in all `.c` files. Every candidate dead/uncalled name below was then re-verified individually with word-boundary `grep -rnw` over `src/ include/ iop/ tools/` (including `.py` host tests), so assembly-defined (`.s/.S`), inline-asm-defined (in `.c`), and header-inline functions are excluded from the dead lists — see checked-clean notes.
- **Build:** root `Makefile` (157 lines) read in full; the 17 other Makefiles/build files were listed (iop ×11, docs, tools probes ×2, utils/Loader) but their bodies are IOP/other-scope except as noted. `.gitignore`, `.gitattributes`, `.clang-format`/`.editorconfig` (listed only) read where relevant.
- **CI:** all 7 files under `.github/` read: workflows `compilation.yml` (308), `sms-converter.yml` (327), `docs.yml` (27), `wiki.yml` (72), `smbman.yml` (48), `test-media.yml` (46), and `actions/setup-ffmpeg-windows/action.yml` (skimmed via workflow output). Cross-checked the host-test lists against `tools/test_*.py` by script (59 files).
- **Aux directories:** full file census of `patches/` (2 files), `tools/` (118 files), `utils/` (4 files), `vu/` (7 files), `fonts/`, `images/`, `theme/`, `ship/`, `lng/`, `irx/`; purpose/reference checks by repo-wide grep for each suspect item.
- **Docs:** `README.md` (297 lines), `AGENTS.md`/`CLAUDE.md` (byte-diffed), `MX4SIO_SOLUTION.md` (365 lines) read in full; `CHANGELOG.md` (1,411 lines) read at head + heading census; `docs-site/build.py` (330) and `docs-site/sync_wiki.py` (listed) read; `docs/README.md`, `docs/Makefile` head, `docs-site/README.md` role established from `docs/README.md`. Factual claims spot-verified against code (see checked-clean): MX4SIO chunk/batch/defaults, README language-pack count, BDM default. `docs/` is 23 MB / 327 tracked files across `docs/ docs-site/ wiki/`; generated-output identity checked by MD5 (`docs-site/static/` vs `docs/`: 21 counterparts, 21 identical).
- **Tracked artefacts:** full `git ls-files` census (1,012 files) with per-extension counts and largest-file listing; `git check-ignore` / `ls-files --error-unmatch` used to separate tracked from working-tree-only files.

## (b) Candidates (sorted by benefit)

### CAND-W8-1 — Delete the dead generated header `include/s_iop_image.h` (3,820 lines, ~373 KB)
Category: 1 dead code (also 7 clutter)
Locations: `include/s_iop_image.h` (whole file); only references in the tree are commented out: `src/SMS_IOP.c:228` (`//#include "s_iop_image.h"`) and `src/SMS_IOP.c:437` (`//while(!SifIopReset(s_iop_image, 0)){};`).
Problem: A 3,820-line hex-array dump of a legacy IOP image (`static unsigned char s_iop_image[]`) is carried in `include/` — 30% of all header lines — but no translation unit includes it; the two historical uses survive only as comments. It inflates every clone, grep, and header audit, and risks being mistaken for the live IOP-load path (the live path is the committed `irx/*.irx.xz` blobs via the root Makefile's bin2c rule).
Fix: Delete `include/s_iop_image.h` and the two commented-out lines in `src/SMS_IOP.c`. No replacement.
Justification (if ADDS): n/a — pure removal.
Benefit: −3,820 lines / −373 KB; removes the single largest misleading artefact in `include/`.
Effort: Trivial (one `git rm`, two comment lines).
Risk: Very low — no live includer exists (verified by grep over the whole tree). The commented code is recoverable from git history.
Confidence: READ

### CAND-W8-2 — Remove ten defined-but-never-called EE functions (≈200 lines, incl. a 105-line IPU routine)
Category: 1 dead code
Locations (declaration / definition; word-boundary census over `src/ include/ tools/` found only these plus the definition's own end-comment):
- `include/SMS_IPU.h:140` / `src/SMS_IPU_1.c:220-324` — `IPU_UnpackImage` (105 lines)
- `include/SMS_CDDA.h:83` / `src/SMS_CDDA.c:220` — `CDDA_Pause`
- `include/SMS_CDDA.h:85` / `src/SMS_CDDA.c:251` — `CDDA_SetMediaMode`
- `include/SMS_FileContext.h:135` / `src/SMS_FileContext.c:887` — `CDDA_GetPicture`
- `include/SMS_FileContext.h:136` / `src/SMS_FileContext.c:907` — `CDDA_GetDiskPicture`
- `include/SMS_CDVD.h:63` / `src/SMS_CDVD.c:114` — `CDVD_QueryDVDV`
- `include/SMS_PlayerControl.h:39` / `src/SMS_PlayerControl.c:876` — `PlayerControl_SwitchSubs`
- `include/SMS_RC.h:83` / `src/SMS_RC.c:454` — `RC_Shutdown`
- `include/SMS_RC.h:84` / `src/SMS_RC.c:484` — `RCX_Shutdown`
- `include/SMS_PAD.h:20` / `src/SMS_PAD.c:118` — `ReadCombinedPadStatus_raw`
Problem: Each function is compiled and linked into the EE ELF but has no caller anywhere — not in EE code, not via function pointer tables found by name, and not in the host-test harnesses in `tools/`. `IPU_UnpackImage` alone is a full 105-line DMA/IPU routine. They mislead readers about which CDDA/IPU/remote-control paths are live.
Fix: Delete each definition and its header declaration. No replacement.
Justification (if ADDS): n/a.
Benefit: ≈200 fewer lines of EE code; smaller ELF; the live CDDA/IPU/RC surfaces become exactly the called ones.
Effort: Small (ten independent deletions; host tests + PS2 build confirm).
Risk: Low — census covered name references in C, asm, and Python harnesses; the one residual risk is a caller constructed by token pasting, which this codebase does not use for these names (grep for the name stems shows no macro construction).
Confidence: READ

### CAND-W8-3 — Remove six declaration-only prototypes with no definition anywhere
Category: 1 dead code
Locations:
- `include/SMS.h:305` — `SMS_Align` (only occurrence in the tree; distinct from the live `SMS_AlignBits`)
- `include/SMS.h:306` — `SMS_SetSifCmdHandler` (the real function was renamed: declaration `include/SMS_IOP.h:76`, definition `src/SMS_IOP.c:192`, callers `src/SMS_GUI.c:978-982` — all as `SMS_IOPSetSifCmdHandler`; the stale end-comment at `src/SMS_IOP.c:196` still says `/* end SMS_SetSifCmdHandler */`)
- `include/SMS.h:308` — `SMS_StartNetwork` (only occurrence in the tree)
- `include/SMS.h:309` — `SMS_ResetIOP` (only occurrence in the tree)
- `include/SMS_IOP.h:95` — `SMS_HDDUMount` (only occurrence in the tree)
- `include/SMS_MPEG.h:286` — `SMS_MPEG_SCB` (only occurrence in the tree)
Problem: Prototypes promise APIs that do not exist. Anyone calling one gets a link error only after writing code against it; `SMS_SetSifCmdHandler` is worse — it looks like the canonical name while the tree actually exposes the differently-named `SMS_IOPSetSifCmdHandler` from a second header.
Fix: Delete the six prototypes; fix the stale end-comment at `src/SMS_IOP.c:196` to the current function name while there.
Justification (if ADDS): n/a.
Benefit: Header surface matches reality; removes a rename trap in the central `SMS.h`.
Effort: Trivial.
Risk: Very low — a declaration with no definition cannot be called successfully today.
Confidence: READ

### CAND-W8-4 — Move non-site binaries out of the GitHub Pages tree (`docs/downloads/`, `docs/investigations/` ≈ 11.9 MB)
Category: 7 build/CI/clutter (also 4 size)
Locations: `docs/downloads/` (9.9 MB: `SMS-manual.pdf` 9,321,329 B, `Rad-HOST-Client-1.8a.zip` 891,537 B, `SMS_lng09.rar`, `pic2smi.rar`, `smspal.rar`); `docs/investigations/` (2.0 MB, incl. `2026-11-11-handoff/samples/` — 19 test-media files: `.mp3/.wma/.wmv/.asf/.aac/.bin`, 1.7 MB for the handoff dir alone).
Problem: `docs/README.md` states GitHub Pages publishes `master:/docs`, and `docs-site/build.py` generates only the top-level HTML/assets/search data — `downloads/` and `investigations/` are not generated site content, yet they sit inside the published tree and in every clone. The investigations handoff is dated working material (2026-11-11) with binary crash/sample media; the downloads are legacy third-party archives that the release system (rolling release assets, already used for the test-media pack per `.github/workflows/test-media.yml`) is designed to carry.
Fix: Move `docs/downloads/*` to release assets (or a single `downloads` release) and update the links that point at them; move `docs/investigations/` out of `docs/` (e.g. repo-root `investigations/` if it must stay tracked, or an archive branch/release). Keep the generated site itself in `docs/` — Pages needs it.
Justification (if ADDS): n/a — relocation, no new mechanism; releases already exist for exactly this.
Benefit: ≈11.9 MB out of the Pages tree and out of the docs surface every doc link-checker and clone traverses; published site contains only site content.
Effort: Medium (link updates in `docs-site/content/` + regeneration via `docs-site/build.py`, which CI validates with `--check`).
Risk: Medium — external deep links to the old `…/docs/downloads/…` URLs will break; mitigate by keeping the filenames identical on the release that replaces them.
Confidence: READ

### CAND-W8-5 — Delete the thirteen never-called DS34 RPC client stubs
Category: 1 dead code
Locations: declarations `include/libds34bt.h`, `include/libds34usb.h`; definitions `src/libds34bt.c`, `src/libds34usb.c`. Never called (word-boundary census over `src/ include/ tools/`; the live calls are only `*_init`, `*_get_status`, `*_get_data`, `*_get_bdaddr`, `ds34usb_set_bdaddr` from `src/SMS_PAD.c` and `src/SMS_GUIMenuSMS.c`):
- libds34bt (8): `ds34bt_deinit`, `ds34bt_reinit_ports`, `ds34bt_init_charging`, `ds34bt_set_rumble`, `ds34bt_set_led`, `ds34bt_reset`, `ds34bt_get_version`, `ds34bt_get_features`
- libds34usb (5): `ds34usb_deinit`, `ds34usb_reinit_ports`, `ds34usb_set_rumble`, `ds34usb_set_led`, `ds34usb_reset`
Problem: Half of the vendored DS3/DS4 client API is dead weight in the ELF: each stub is an SIF RPC wrapper that is compiled, linked, and maintained as if SMS could deinit/reinit/rumble/reset the pad drivers, which no code path does.
Fix: Delete the thirteen stubs and their declarations, keeping the five used entry points. (If upstream-parity with OPL's libds34 is ever wanted again, the functions are recoverable from OPL/git history.)
Justification (if ADDS): n/a.
Benefit: ≈150 lines removed across the two client libs; the DS34 surface becomes exactly what SMS uses.
Effort: Small.
Risk: Low — no caller exists; risk is only to hypothetical future rumble/LED features, which would re-add one stub on demand.
Confidence: READ

### CAND-W8-6 — Give `libds34bt.h` / `libds34usb.h` include guards and `(void)` prototypes
Category: 6 bugs/security (header correctness)
Locations: `include/libds34bt.h` (35 lines), `include/libds34usb.h` (21 lines) — the only 2 of 89 headers with no include guard and no `#pragma once`; all prototypes use K&R empty parentheses, e.g. `int ds34bt_init();`, `int ds34usb_reset();`, where the other 87 headers uniformly use `(void)`.
Problem: Both headers are included by multiple translation units (`src/SMS_PAD.c:26-27`, `src/SMS_GUIMenuSMS.c:36-37`). Today double inclusion in one TU happens not to bite only because the contents are declarations and an enum; the enum definitions (`eDS34BTStatus`, `eDS34USBStatus`) will hard-error on any future double include. In C, `int f();` declares a function with unspecified parameters, so callers get no argument checking — inconsistent with the tree's own convention and a real footgun for the RPC wrappers that take pointers.
Fix: Add the tree-standard `#ifndef __libds34bt_H`-style guards and change `()` to `(void)` on the zero-argument prototypes in both headers. (Do this for the surviving prototypes if CAND-W8-5 lands first.)
Justification (if ADDS): Adds only the guard idiom every other header in the same directory already has — consolidation onto the existing mechanism, not a new one.
Benefit: Removes the only unguarded headers in `include/`; restores compile-time argument checking for the DS34 API.
Effort: Trivial.
Risk: Very low.
Confidence: READ

### CAND-W8-7 — Replace the hand-maintained 64-line host-test list in CI with discovery
Category: 7 build/CI/clutter (also 3 overengineering of process)
Locations: `.github/workflows/compilation.yml`, step "Host playback and file I/O regression checks" — ≈64 individually named `python3 tools/test_*.py` invocations.
Problem: The list is a second, manual registry of what already exists on disk. It is currently complete — verified by script: all 59 `tools/test_*.py` files are referenced across the workflows — but nothing enforces that: the next `tools/test_*.py` a contributor adds will silently never run in CI unless they also edit this YAML list in the right step. The failure mode is invisible (a missing line is not an error).
Fix: Replace the enumerated lines with a loop over `tools/test_*.py` (keeping the two non-`test_` scripts `tools/check_devmenu.py` / its test explicitly), or add a CI step that fails when a `tools/test_*.py` file is not referenced. The loop is the LESS option: one mechanism, no registry.
Justification (if ADDS): n/a — removes a registry rather than adding one.
Benefit: Test coverage can no longer silently drift from the filesystem; ≈60 lines of YAML deleted.
Effort: Small.
Risk: Low — a loop changes failure granularity (first failure stops the step, as today with `run:` default shell behaviour); keep per-file echo so logs still name the failing test.
Confidence: READ

### CAND-W8-8 — Fold the redundant explicit object rules in the root Makefile
Category: 2 duplication (also 7)
Locations: `Makefile:128-147`. `SMS_GS_1.o` and `SMS_GS_2.o` have explicit rules byte-identical in effect to the generic `$(EE_OBJ_DIR)%.o : $(EE_SRC_DIR)%.c` rule at `Makefile:125-126` (same command, no extra prerequisites). The `SMS_MPEG4.o` / `SMS_MSMPEG4.o` / `SMS_MPEG.o` / `SMS_H263.o` rules differ from the generic rule only by adding the prerequisite `$(EE_INC_DIR)SMS_MPEG.h`.
Problem: Six explicit rules restate the generic rule; two of them add nothing at all. Anyone changing compile flags must check whether the explicit copies shadow the generic rule (they do — that is how make resolves them), so the duplication is a standing flag-drift hazard.
Fix: Delete the `SMS_GS_1.o`/`SMS_GS_2.o` rules outright. Replace the four MPEG rules with one grouped prerequisite-only line: `$(EE_OBJ_DIR)SMS_MPEG4.o $(EE_OBJ_DIR)SMS_MSMPEG4.o $(EE_OBJ_DIR)SMS_MPEG.o $(EE_OBJ_DIR)SMS_H263.o: $(EE_INC_DIR)SMS_MPEG.h` (a prerequisite-only rule merges with the generic pattern rule instead of replacing it).
Justification (if ADDS): n/a.
Benefit: ≈20 lines removed; one compile-command definition for all EE objects.
Effort: Trivial; verify with a clean `make` in the ps2dev container (the CI build does exactly this).
Risk: Low — prerequisite-only rules are standard make; CI's BDM=0 and BDM=1 builds exercise every object.
Confidence: READ

### CAND-W8-9 — Fold the eleven one-declaration `SMS_Container*.h` stub headers into `SMS_Container.h`
Category: 2 duplication (also 3 overengineering)
Locations: `include/SMS_ContainerAAC.h` (28 lines), `SMS_ContainerAC3.h` (28), `SMS_ContainerASF.h` (28), `SMS_ContainerAVI.h` (27), `SMS_ContainerFLAC.h` (28), `SMS_ContainerJPG.h` (36), `SMS_ContainerM3U.h` (35), `SMS_ContainerMOV.h` (29), `SMS_ContainerMP3.h` (28), `SMS_ContainerMPEG_PS.h` (27), `SMS_ContainerOGG.h` (29) — ≈323 lines total carrying 1-2 prototypes each (e.g. `SMS_ContainerAVI.h` carries only `int SMS_GetContainerAVI ( SMS_Container* );`), each already `#include`-ing `SMS_Container.h`. 48 include sites across 16 source files.
Problem: Eleven files exist to declare one function each for the same container registry that `SMS_Container.h` already owns (`SMS_GetContainer`, the `SMS_Container` struct and its function-pointer table). The per-format split buys nothing: every stub includes the central header anyway, so there is no compile-time isolation.
Fix: Move the ≈15 prototypes into `SMS_Container.h`, delete the eleven stubs, and point the 48 include sites at `SMS_Container.h` (most already include it transitively/directly).
Justification (if ADDS): n/a — consolidation onto the existing central header.
Benefit: −11 files / ≈300 lines; one place declares the whole container surface.
Effort: Medium (mechanical include edits in 16 files; build + host container tests in CI cover the fallout).
Risk: Low-medium — purely mechanical, but touches many files; do it as its own commit.
Confidence: READ

### CAND-W8-10 — Remove the orphan blob `irx/bdmfs_vfat.irx.xz`
Category: 1 dead code (also 7 clutter)
Locations: `irx/bdmfs_vfat.irx.xz` — the only `.irx.xz` in `irx/` with no plain `.irx` counterpart, and zero references: not in the root Makefile's `IOP_OBJS` (which embeds `bdmfs_fatfs_irx.o`), and no match for `bdmfs_vfat`/`vfat` in `src/`, `include/`, `tools/`, `.github/`, or `docs-site/content/`.
Problem: Every other blob in `irx/` is either embedded by the Makefile or is the documented input/output of a `tools/build_*.sh` / `tools/compress_irx.py` step. This one is neither — a superseded FAT driver variant (replaced by `bdmfs_fatfs`) that a reader can easily mistake for a shipped module.
Fix: Delete `irx/bdmfs_vfat.irx.xz`.
Justification (if ADDS): n/a.
Benefit: `irx/` becomes exactly the set of modules the build embeds or the tools regenerate.
Effort: Trivial.
Risk: Very low — nothing references it; recoverable from history.
Confidence: READ

### CAND-W8-11 — Delete the unreferenced `utils/` directory (prebuilt Win32 exe + orphan Loader)
Category: 1 dead code (also 7 clutter)
Locations: `utils/DDrawSrv.exe`, `utils/Loader/Makefile`, `utils/Loader/linkfile`, `utils/Loader/loader.c` (4 files, 56 KB).
Problem: Zero references to `DDrawSrv`, `utils/Loader`, or the Loader anywhere else in the repo (grep over docs, docs-site content, README, Makefile, tools, workflows returned nothing). `DDrawSrv.exe` is a committed prebuilt Windows binary with no documented purpose, source, or build path in the current tree — exactly the kind of artefact that cannot be audited or rebuilt.
Fix: Delete `utils/`. If the Loader is still wanted, it belongs with the code that launches it, documented — neither exists today.
Justification (if ADDS): n/a.
Benefit: Removes an unauditable binary and a dead build from the repo root surface.
Effort: Trivial.
Risk: Low — no in-repo consumer; external historical use is possible but the files remain in git history.
Confidence: READ (absence of references: INFERRED from repo-wide grep — no external consumer can be ruled out from inside the tree)

### CAND-W8-12 — Deduplicate `AGENTS.md` / `CLAUDE.md` (byte-identical files)
Category: 2 duplication
Locations: `AGENTS.md` and `CLAUDE.md` — `diff` reports no differences; both are the same 1,254-byte GitHub Attribution Policy.
Problem: Two root files carry the same policy text; any future edit must be made twice or the two agent-facing files will silently diverge — and divergence here is specifically bad, because the file's whole point is a rule agents follow.
Fix: Keep `AGENTS.md` as the canonical text and reduce `CLAUDE.md` to a one-line pointer (`See AGENTS.md`), or vice versa — whichever the maintainer's tooling reads first.
Justification (if ADDS): n/a.
Benefit: One canonical policy file; no divergence risk.
Effort: Trivial.
Risk: Very low — some tools read only one of the two filenames; a pointer line is still read by both.
Confidence: READ

### CAND-W8-13 — Drop stale `src/lzma2` references from `.gitignore` and the Makefile link path
Category: 7 build/CI/clutter
Locations: `.gitignore` (`*.a` exception block: `!/src/lzma2/` and `!/src/lzma2/libxz.a`); `Makefile:13` (`EE_LDFLAGS = … -L$(EE_SRC_DIR)/lzma2`).
Problem: There is no `src/lzma2/` directory and no `libxz.a` in the tree (`git ls-files` shows `src/lzma2.c` plus the `src/xz_*.c` sources, which the Makefile compiles directly as `lzma2.o xz_crc32.o xz_dec_lzma2.o xz_dec_stream.o`). Both references are leftovers from a layout where XZ was a prebuilt library; today they are inert but misleading — the `.gitignore` exception in particular invites a future contributor to commit a prebuilt `.a` there.
Fix: Delete the two `.gitignore` exception lines and the `-L$(EE_SRC_DIR)/lzma2` flag.
Justification (if ADDS): n/a.
Benefit: Build/config describe the layout that actually exists.
Effort: Trivial.
Risk: Very low — the directory does not exist, so the `-L` path contributes nothing to linking today (verified by the file census, not by a local build).
Confidence: READ

### CAND-W8-14 — Gate `test-media.yml` regeneration on media-relevant paths
Category: 7 build/CI/clutter (CI cost)
Locations: `.github/workflows/test-media.yml:12-15` — triggers on every `push` to `master` with no `paths:` filter, then runs `tools/gen-test-media.sh` (ffmpeg generation) and re-uploads `SMS-test-media.zip` to the `test-media` release.
Problem: The media pack depends only on `tools/gen-test-media.sh` and the format support it samples, but a docs-only or GUI-only push still pays a full ffmpeg generation + release upload cycle. Unlike `docs.yml`, which is path-filtered, this workflow has no filter at all.
Fix: Add a `paths:` filter (`tools/gen-test-media.sh`, the workflow itself, and the converter/format sources the pack samples — or conservatively `tools/**` + `src/SMS_Container*`), keeping `workflow_dispatch` for manual regeneration.
Justification (if ADDS): n/a — narrows an existing trigger.
Benefit: Fewer pointless CI runs and release-asset churn on unrelated pushes.
Effort: Trivial.
Risk: Low — a too-narrow filter could skip a needed regeneration; `workflow_dispatch` remains as the manual escape hatch.
Confidence: INFERRED (trigger behaviour is READ from the YAML; the cost saving is inferred, not measured from Actions logs)

## (c) Checked-clean notes

- **Include guards:** 87 of 89 headers have matching `#ifndef`/`#define` guards with a closing `#endif`; the only exceptions are `libds34bt.h`/`libds34usb.h` (CAND-W8-6). Guard-name anomalies that are harmless and not flagged as findings: `SMS_ContainerFLAC.h`'s guard is spelled `__SMS_CountainerFLAC_H` (ifndef/define agree, so it works), `SMS_SPU.h` uses the short `__SPU_H`, `SMS_SubtitleContext.h` uses `__SubtitleContext_H` — inconsistent naming only.
- **No duplicated declarations:** across the 649 parsed prototypes, no function is declared in two different headers (apparent duplicates in the raw parse were parser artefacts from `__attribute__`/enum lines, verified by re-grep). The one real name collision is the stale `SMS_SetSifCmdHandler` prototype vs the live `SMS_IOPSetSifCmdHandler` (CAND-W8-3).
- **Apparent dead declarations that are alive:** the census's no-C-definition list for `SMS_DSP.h` (~150 `DSP_*`), `SMS_DMA.h` (`DMA_*`), `SMS_GS.h` (`GS_*`), `SMS_IPU.h` (`IPU_FDEC` etc.), `libmpeg_internal.h` (`_MPEG_*`), `SMS_Timer.h`, `SMS_DateTime.h`, `SMS_EE.h`, `SMS_VideoBuffer.h` (`SMS_CSC*`), `SMS_FileMapping.h` resolves to definitions in `.s`/`.S` assembly (`src/SMS_DSP_MMI.S`, `SMS_DSP_QPel.S`, `SMS_GS_0.S`, `SMS_DMA_0.s`, `SMS_Timer.s`, `libmpeg_core.S`, …), to inline-asm definitions inside `.c` files (e.g. `SMS_SetExecPath` at `src/SMS_EE.c:742`, `SMS_SummerTime` at `src/SMS_DateTime.c:40-47`, `SMS_FileMappingDestroy` at `src/SMS_FileMapping.c:234-240`), or to header-inline bodies (e.g. `GSFont_CharWidth` at `include/SMS_GS.h:1307`). Spot-verified by grep; none of these are flagged.
- **Vendored XZ surface:** `xz_crc64`/`xz_crc64_init` (`include/xz.h:291,298`) have no compiled definition in the default build, but this is the vendored XZ Embedded configuration working as intended — `tools/compress_irx.py` and `tools/bake_icons_rgba.py:86` both document that SMS uses CRC32 only and `xz_crc64` is NOT linked; the call site at `src/xz_dec_stream.c:260` is under the any-check configuration. Not flagged; do not edit vendored headers for this.
- **Host tests are all wired up:** all 59 `tools/test_*.py` files are referenced in `.github/workflows/` (verified by script) — the CI-list finding (CAND-W8-7) is about the manual mechanism, not a current gap. `tools/sms_converter/test_*.py` run in `sms-converter.yml`.
- **Plain `.irx` + `.irx.xz` pairs are intentional:** `tools/compress_irx.py:26` globs `irx/*.irx` to produce the `.xz` blobs the Makefile embeds, and `tools/build_*.sh` scripts document the rebuild flow (e.g. `tools/build_libsd.sh:83`). Not clutter. Sole exception: `bdmfs_vfat.irx.xz` (CAND-W8-10).
- **`vu/*.o` are provenance, not build outputs:** the five VU microcode objects are the sources whose XZ dumps are embedded as `s_IDCT`/`s_CSC*` in `src/SMS_JPEGData.c:13-321`, regenerated by `vu/build.sh`; `.gitignore` explicitly excepts them (`!/vu/*.o`). Small (28 KB total). Not flagged.
- **`lng/` is a deliberate archive:** `lng/README.txt` states the 269-entry packs are the original SMS 2.9 packs, kept unchanged as their authors' work, and that they do not load in this build (311 entries required); the live packs are `docs/translation/`. The two sets differing is documented intent, not drift. Not flagged.
- **Docs generation is disciplined:** `docs/` top-level HTML/assets and `wiki/` are generated from `docs-site/` by `docs-site/build.py`, CI enforces freshness with `--check` in both `docs.yml` and `wiki.yml`, and `docs-site/static/` → `docs/` copies are byte-identical (21/21 MD5-checked). The generated-in-git layout is required by GitHub Pages (`master:/docs` per `docs/README.md`). Not flagged as duplication; only the non-generated binaries inside the tree are (CAND-W8-4).
- **Doc claims verified against code (no contradiction found):**
  - `MX4SIO_SOLUTION.md` §4.1 / `patches/README.md`: `SPISD_MAX_BATCH 8` is at `patches/mx4sio_bd-sms.patch:173-174`; the patch does add `mx_sio2_xfer_done`, the `PollEventFlag` watchdog, and `I_PollEventFlag` to `imports.lst` — all present in the patch file.
  - `MX4SIO_SOLUTION.md` §4.2: `MX4SIO_RD_CHUNK 4096` is defined at `src/SMS_FileContext.c:26` with the stated rationale at `:14-23` (line numbers have drifted from the doc's `:14-23`/`:1338-1352` citations — the doc's own header banner declares its file:line references snapshots, and the substance matches).
  - `MX4SIO_SOLUTION.md` §6 clean-boot default: substance verified at `src/SMS_Config.c:604` (`g_Config.m_NetworkFlags = 0; /* no-settings default: auto-start NO device (clean boot) … */`); the doc's `:211` line citation is stale, again under its snapshot disclaimer.
  - `MX4SIO_SOLUTION.md` §10 / `README.md`: `BDM ?= 1` at `Makefile:3` — BDM defaults on, as claimed.
  - `README.md` "Current packs have 311 strings": `docs/translation/SMS.lng` has exactly 311 lines. Verified.
  - `MX4SIO_SOLUTION.md` §9's statement that the legacy `iop/SMSSMB/` tree is dead (not built by the root Makefile, which builds no IOP modules and embeds committed blobs) is consistent with the Makefile; the IOP trees themselves (`iop/SMSSMB`, `SMSTCPIP`, `SMSMAP`, etc.) are IOP-scope and were censused (10 dirs, 204 files) but not audited here.
- **`AGENTS.md`/`CLAUDE.md` content** is a coherent attribution policy with no code claims to contradict (duplication only, CAND-W8-12).
- **Working-tree `.pyc`:** `tools/__pycache__/check_devmenu.cpython-312.pyc` exists on disk but is NOT tracked (`git ls-files --error-unmatch` fails; `.gitignore` covers `__pycache__/`) — not a tracked-artefact finding.
- **No secret-like values** were encountered in scope beyond CI's standard `GH_TOKEN: ${{ github.token }}` env wiring in the workflows, which is normal GitHub Actions usage and carries no literal value.

## (d) Explicitly NOT read

- `src/SMS_Data.c` — excluded by the task; also the bodies of all other `src/*.c` files (read only at grep-hit lines and the specific ranges cited above) — EE-code scope belongs to other workers.
- `iop/` source bodies (all 10 subdirectories) — censused only; IOP scope.
- `tools/sms_converter/*.py` bodies, `tools/mpeg_color_probe/` and `tools/mpeg_motion_probe/` bodies, `tools/libsd/*.c` bodies — censused and reference-checked only.
- `docs-site/content/*.html` full text (roles established via `docs-site/build.py`, `docs-site/manifest.json` structure, and `docs/README.md`); `docs/*.html` generated output text; `docs/src/**` DocBook XML bodies (censused as historical material per `docs/README.md`).
- `CHANGELOG.md` beyond the head sections and full heading census; `docs/investigations/**/*.md` bodies (censused by size/files only).
- Vendored header bodies beyond guard/declaration checks: `include/xz*.h`, `include/lzma2.h`, `include/mbstring.h`, and the body of `include/SMS_GS.h` (1,314 lines; composition sampled — 241 `#define`s, 13 struct typedefs, 1 header-inline function — but not line-audited).
- `.github/actions/setup-ffmpeg-windows/action.yml` beyond a skim of its structure via workflow output; `ship/`, `fonts/`, `images/`, `theme/` beyond census + reference checks (asset scope).
- Per-directory IOP Makefiles (`iop/**/Makefile`, 11 files), `docs/Makefile` beyond its head, `utils/Loader/Makefile` body — listed and counted only.
