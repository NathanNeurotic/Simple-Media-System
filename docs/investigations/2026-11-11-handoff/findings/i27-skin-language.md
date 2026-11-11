# i27-skin-language

Issue #27 (skin SMS.smi 800x600 not applied after restart) + language packs, verified read-only against master c275476. Nothing in the repo was modified; all trial work is in (investigator scratch, not kept)/i27-skin-language/ (boot_skin_sim.py = current-tree simulation, boot_skin_sim2.py = same for the patched copy, lngcheck.py = pack checker, tree/ = private patched export, pre/ = 9cfe235^ baseline, upstream/ = 2.9rev4). No PS2/emulator was available: everything below is host simulation / host test / code reading / PS2 compile only, never hardware acceptance.

WHAT THE CLAIMED FIX DOES (confirmed on host only): the simulation compiles the REAL SMS_ConfigSetCWD, SMS_LoadConfig, SMS_ConfigClearFS, SMS_EEScanDir, DrawSkin, IPU_ImageInfo, IPU_InitLoadImage and the REAL main.c guard expression against a fake fio with per-device mount state. On 9cfe235^ a USB or MMCE boot with Default display mode never repaints after the late config load (skin NOT visible); on c275476 the same scenarios show the skin. So commit 9cfe235 does fix the modelled USB/MMCE + Default-mode case. It is still unconfirmed by the reporter.

TWO REMAINING WAYS #27 STILL REPRODUCES ON MASTER (both reproduced in the simulation, both fixed in the private copy, which also builds SMS.elf with ps2dev/ps2dev:v1.0, rc=0, no new warnings): (1) the main.c re-init guard requires SMS_ConfigOnFS(), so every boot whose late config load came from the memory-card fallback (every HDD/pfs boot, a 'mass:' the loader mounted but SMS cannot re-mount, unreachable udpfs) never repaints and never applies the saved display mode; (2) _bgTexUnsafe() treats any canvas wider than 640 as HD, but SD width is user-adjustable 640..720 in the Display menu, so a user who widened the picture loses skin, wallpaper and splash on every boot device including mc. With both patches all 12 simulated scenarios show the skin; the extended tools/test_skin_loading.py fails on master (sd-width-720 aborts) and passes on the patched copy.

RULED OUT for #27: the 8-byte .smi header check and dimension limits (800x600 passes: 800%16==0, height unconstrained to macroblocks, texture 0x1EE0 blocks < 0x4000, lands at 0x2120 above the largest SD framebuffer end 0x1BC0); the 0xFFFF0 size cap (equals upstream's implicit 16-bit QWC limit); IPU decode differences versus 2.9rev4 (none besides types); the skin-list scan (identical to upstream, case-sensitive '.smi' exactly as 2.9rev4); skin-name save/load round trip (v14 config, m_SkinName[64]); path resolution on mass/mmce/mc boots including card fallback mc<slot>:/SMS/Skins and legacy mc config migration; language-pack packaging.

LANDED ON MASTER AFTER THE REPORTER'S LAST TEST (v3.0.0-rev1; last comment 2026-10-26 'Perfect, I'll try it') AND STILL AWAITING RETEST: 9cfe235 'Skins: repaint the desktop after the late config load on FS boots (#27)' (PR #30); 69420d4 'fix(skin): bound image loading and handle allocation failures'; 266851d Spanish pack integration. Unknown reporter facts that decide which path he is on: boot device (mc / USB / other), Display width setting, and whether SMS.smi sits in <ELF dir>/Skins or mc0:/SMS/Skins. Issue #27 should stay open until he retests a build containing the two fixes below.

LANGUAGE PACKS: the built-in table has 311 strings (indices 0..310, last s_pNLNetSwitch; include/SMS_Locale.h max index 310). All 20 files in docs/translation (template + 19 packs) parse to exactly 311 lines with the loader's own algorithm, are CRLF, ASCII or single-byte codepage (none UTF-8), and have zero printf-specifier mismatches. docs/translation/SMS.lng_ES is 311 lines with 0 of the 42 newest strings left in English and was integrated by 266851d; I did not byte-compare it with the reporter's SMS_es.zip attachment because downloading it needs the owner's permission. PRs #52/#53 (54db532..2bb121e) touched no locale file and added no table string. The release workflow (.github/workflows/compilation.yml:133) ships docs/translation correctly. Defects found: the legacy lng/ directory (19 packs of 269 lines, all rejected by the current loader, with a README telling users to install them) and a two-line drift between the English template and the built-in table.

Also found along the way (code reading unless noted): boot-time SELECT+R1/R2 video-mode rescue is undone on USB/MMCE boots; the 1.6 s boot splash replays on every desktop rebuild; late-loaded picture offsets/size/charset are not applied on FS boots with Default mode and no skin; the Language row breaks on cdrom/host/SMB boots; long FAT skin names overflow m_SkinName; oversized skins overlap the SD framebuffer.

## #1 Saved skin and saved display mode are never applied when the late config load comes from the memory-card fallback (every HDD/pfs boot, phantom mass:, unreachable udpfs)
- issue: 27
- severity: medium
- category: bug
- files: ['src/main.c', 'src/SMS_IOP.c']
- reproduced: True
- confidence: 0.8
- risk: Adds one GUI_Initialize(0) on HDD / card-fallback boots that have a skin or non-default mode saved; it is the same call USB boots already make since 9cfe235 and that every return from playback makes. A saved mode the TV cannot show is now applied on HDD boots too (previously silently ignored), so it should land together with the boot-override finding below. Not hardware-tested.

### evidence
src/main.c:107-110 re-inits the GUI only if SMS_ConfigOnFS() && (mode != Default || m_SkinName[0]) && boot dir is not mc. In SMS_IOPInit (src/SMS_IOP.c:1502-1517) a pfs/hdd boot sets lFSGone in what the code itself calls 'the normal case' (boot partition is never mounted), and a mass: root that cannot be re-opened does the same (1498-1500); lines 1554-1559 then call SMS_ConfigClearFS() (s_CfgOnFS=0) and SMS_LoadConfig() reads the card config. The early SMS_LoadConfig in GUI_Initialize(1) could not read it (path was pinned to the unreachable device), so the first paint used defaults; the guard is now false because OnFS==0, so nothing repaints. Host simulation with the real functions and the real guard expression (boot_skin_sim.py, scenarios I 'HDD (pfs) boot, cfg+skin on card', J 'phantom mass:', K 'pfs boot, non-default mode + skin'): 'main.c guard FALSE -> desktop is NOT repainted (OnFS=0 Fallback=1)', RESULT skin NOT visible, saved mode not applied. 2.9rev4 always loaded the card config before the first paint, so this is a fork regression.

### fix
Record that a config was loaded after the first paint instead of inferring it from SMS_ConfigOnFS(). (a) src/SMS_IOP.c after line 50 'unsigned int g_IOPFlags;' add 'int g_CfgLate;'. (b) src/SMS_IOP.c:1559 'SMS_LoadConfig ();' -> 'g_CfgLate = SMS_LoadConfig ();'. (c) src/SMS_IOP.c:1581 (fallback branch) 'SMS_LoadConfig ();' -> 'g_CfgLate = SMS_LoadConfig () && SMS_ConfigOnFS ();' (card-only fallback was already applied by the early load; only a config newly pinned to USB/MMCE by _cfg_resolve_fallback is late). (d) src/main.c: add 'extern int g_CfgLate;' next to the g_pBootDir extern and replace the 4-line guard at 107-110 with 'if (  g_CfgLate && ( g_Config.m_DisplayMode != GSVideoMode_Default || g_Config.m_SkinName[ 0 ] )  ) GUI_Initialize ( 0 );' (the mc clause is no longer needed: mc boots skip the late-load block, so g_CfgLate stays 0). Trim the now-stale comment block above/below the guard. Net +4/-4 lines. One-token alternative if a new global is unwanted: '( SMS_ConfigOnFS () || SMS_ConfigFallback () )' in the guard, at the cost of a redundant re-init on cdrom/host/SMB boots with a card.

### test
Applied to the private copy: boot_skin_sim2.py shows all 12 scenarios VISIBLE (I, J, K flip from NOT visible; A-E, L unchanged; mc boots C/D/H still take no re-init; host boot L takes no re-init). PS2 build of the patched copy with ps2dev/ps2dev:v1.0: make all rc=0, bin/SMS.elf produced, no new warnings (log: (investigator scratch, not kept)/build/i27-trial.log). Hardware check still needed: boot from internal HDD (and from USB) with a saved skin and with a saved non-default mode; confirm the skin/mode appear without a visible glitch.

### VERDICT diagnosis
{
 "real": true,
 "fix_sound": true,
 "reproduced": true,
 "reasoning": "I could not refute it. The defect is real in the current tree, and I reproduced it on the host with the production code; there is no hardware confirmation.\n\n**What I read (current tree, c275476)**\n- `src/main.c:56-58`: `SMS_ConfigSetCWD(argv[0])` runs for every boot. For `pfs`/`hdd`/`mass`/`mmce`/`udpfs`/`mc` it pins `s_pMC0SMC` to `<bootdir>SMS.cfg` and sets `s_CfgOnFS=1` (`src/SMS_Config.c:123-173`).\n- `src/SMS_GUI.c:726,766`: the early `SMS_LoadConfig()` inside `GUI_Initialize(1)` does `fioOpen(s_pMC0SMC)`. On a pfs/hdd/phantom-mass/udpfs boot no driver is loaded yet, so it fails and the first paint uses defaults (empty skin name, Default mode).\n- `src/SMS_IOP.c:1459-1573`: the pfs/hdd branch calls `SMS_IOPStartHDD(1)` (1135-1160: loads modules and does a status ioctl only). The only `PFS_IOCTL_MOUNT` in `src/` is at `SMS_IOP.c:1866`, the interactive browser mount. So `fioOpen(lpCfg)` fails and `lFSGone=1`, which the code's own comment calls \"the normal case\".\n- A mass root that cannot be re-opened (1498-1500) and a failed udpfs probe (1546-1548) set `lFSGone` the same way.\n- `SMS_IOP.c:1554-1559`: `SMS_ConfigClearFS()` sets `s_CfgOnFS=0`, `s_CfgFallback=1`, path `mc0:/SMS/SMS.cfg`, and clears `g_pBootDir` (`SMS_Config.c:516-522`). `_cfg_resolve_fallback()` returns immediately when a card is present (1237). `SMS_LoadConfig()` then loads the card config, so `g_Config.m_SkinName` and `m_DisplayMode` now hold the saved values.\n- `src/main.c:107-110`: the guard starts with `SMS_ConfigOnFS() &&`, which is now 0, so `GUI_Initialize(0)` is skipped.\n- Nothing else repaints: `Desktop_Render` only calls `DrawSkin` when `m_pGSPacket` is NULL (`SMS_GUIDesktop.c:588-599`), and only `GUI_Redraw(InitClearAll)` from `GUI_Initialize` clears it. Between `SMS_IOPInit` and `GUI_Run` there is only `GUI_SetColors` and `GUI_DeleteObject` (no redraw).\n- Side effect: `g_Config.m_DisplayMode` holds the saved mode while GS is still in the default mode. Skin and mode only appear at the next `GUI_Initialize(0)`, i.e. after the first playback return or a settings change.\n\n**Upstream comparison**\n2.9rev4 has a fixed `s_pMC0SMC = \"mc0:/SMS/SMS.cfg\"` (its `SMS_Config.c:62`) and a single early load in `GUI_Initialize` (its `SMS_GUI.c:658/698`), with no late load in `main.c`. An HDD boot there paints the card skin on the first paint, so this is a fork regression.\n\n**What I ran**\n- The reporter's `boot_skin_sim.py` against the real tree reproduces scenarios I/J/K: \"main.c guard FALSE -> desktop is NOT repainted (OnFS=0 Fallback=1)\".\n- That sim hand-mirrors the `SMS_IOPInit` block, so I wrote a stricter one: `(investigator scratch, not kept)/verify-i27-skin-language/verify_sim.py`. It extracts the `SMS_IOPInit` config block (`SMS_IOP.c:1459-1584`) and `_cfg_resolve_fallback` verbatim and compiles them with the real `SMS_ConfigSetCWD`, `SMS_ConfigClearFS`, `SMS_LoadConfig`, `SMS_MCPresent`, `SMS_ConfigUseFSPath`, `DrawSkin` and the regex-extracted `main.c` guard (gcc, ASan/UBSan, stub fio and device mounts).\n- Output on the real tree for `pfs0:/SMS/SMS.ELF`, `hdd0:+SMS:pfs:/SMS.ELF`, phantom `mass:` and unreachable `udpfs:`: \"after SMS_IOPInit block: cfg path=mc0:/SMS/SMS.cfg OnFS=0 Fallback=1 bootdir='' skin='SMS' mode=0xFF\", then \"guard FALSE\", then \"RESULT: skin NOT visible\". Scenario K shows `mode=0x3` loaded but never applied.\n- USB, MMCE, mc and host boots behave as before.\n\n**Fix check**\n- On the reporter's patched private tree (`g_CfgLate`), the same verbatim-block sim shows I/J/K/M/N flip to \"guard TRUE -> SKIN DRAWN\". mc boots (C/D/H) and the host boot (L) still take no re-init; USB and MMCE are unchanged.\n- Equivalence by case analysis: wherever the old guard was true, a config had been loaded late, so `g_CfgLate=1`. If the late load fails, `g_Config` is defaults and the mode/skin clause is false either way. mc boots never enter the block, so `g_CfgLate` stays 0. A cdrom/host/SMB boot with a card gives `&& SMS_ConfigOnFS()` = 0, same as before.\n- The only behaviour change is the intended one. It is small (one int, two assignments, one simplified guard).\n- I did not rebuild for PS2. The reporter's `build/i27-trial.log` shows no new warnings in `main.c` or `SMS_IOP.c` beyond the existing ones.\n\n**Caveats**\n- The reporter's private tree carries extra changes beyond this finding (`SMS_LocaleInit`/`SMS_LocaleSet` added to the fallback branch, and a `SMS_GUIDesktop.c` change). Those belong to other findings and should not be applied with this one.\n- On HDD boots the boot-time SELECT+R1/R2 mode override currently survives until the first playback return, by accident. After this fix the saved mode is applied at boot, as already happens on USB/MMCE boots. If the saved mode is one the TV cannot display, that removes the accidental recovery window, so land this with (or after) the boot-override fix, as the reporter notes.\n- Issue #27's reporter never stated a boot device. This is a real regression but is not proven to be that reporter's cause.",
 "corrected_fix": ""
}

### VERDICT fix
{
 "real": true,
 "fix_sound": true,
 "reproduced": true,
 "reasoning": "I could not refute the defect or the fix. Everything below is host simulation, static reading and cross-builds; nothing ran on a console.\n\n**Defect (code reading, current tree c275476)**\n- `src/main.c:107-110` re-inits only when `SMS_ConfigOnFS()` is true.\n- On a pfs/hdd boot, `SMS_ConfigSetCWD` sets `s_CfgOnFS=1` and pins the path to `pfs0:/...`, so both early `SMS_LoadConfig` calls in `GUI_Initialize(1)` (`SMS_GUI.c:726/766`) fail and the first paint uses defaults.\n- `SMS_IOPInit` (`SMS_IOP.c:1502-1517`) then sets `lFSGone`; lines 1554-1559 call `SMS_ConfigClearFS()` (`s_CfgOnFS=0`, `s_CfgFallback=1`, boot dir cleared) and load the card config.\n- The guard is now false. `Desktop_Render` only calls `DrawSkin` when its packet was dropped by `GUI_Redraw(InitClearAll)`, which during boot is reached only through `GUI_Initialize`. Nothing else repaints.\n- A phantom `mass:` (1498-1500) takes the same path.\n- This is adjacent to #27 rather than the reporter's own case, which was probably a USB boot handled by 9cfe235 and still awaiting retest.\n\n**Reproduction (host)**\n- I ran the reporter's `boot_skin_sim.py` against the real repo. It uses the real `SMS_ConfigSetCWD`, `SMS_ConfigClearFS`, `SMS_LoadConfig`, `DrawSkin` and the real guard expression; I checked its hand-mirrored `SMS_IOPInit` portion against `SMS_IOP.c:1459-1584`.\n- Scenarios I, J and K print `main.c guard FALSE -> desktop is NOT repainted (OnFS=0 Fallback=1)` and `RESULT: skin NOT visible`.\n\n**Fix applied in my own private tree**\n- Tree: `(investigator scratch, not kept)/verify-fix-i27-skin-language/cfglate/tree`. Steps (a)-(d) applied exactly as proposed; diff is `SMS_IOP.c` +3/-2, `main.c` +2/-4.\n- `boot_skin_sim2.py` on the patched tree: I, J and K flip to `guard TRUE -> GUI_Initialize(0)` and `SKIN DRAWN` from `mc0:/SMS/Skins`. A, B, E, F are unchanged. mc boots C, D, H and host boot L still take no re-init.\n- G and H stay NOT visible. That is the separate SD-width finding, so the claim that all 12 scenarios become visible holds only with that other fix also applied.\n- Full CI host list on the patched tree: `PASS=44 FAIL=0`.\n- PS2 builds with ps2dev/ps2dev:v1.0 all returned rc=0 and produced `bin/SMS.elf`: default, `SMS_DIAG=1` (-G4096) and `BDM=0`. The default build's warning set is identical to baseline apart from a one-line shift, and the `SMS_DIAG=1` and `BDM=0` warning counts (18 and 16) match earlier baseline builds of those variants.\n- Link map: `g_CfgLate` is a 4-byte common symbol in `.sbss`, next to `g_IOPFlags` and after `_fbss`, so stock crt0 zeroes it. It is gp-addressable under both -G8192 and -G4096.\n\n**Equivalence check, old guard vs new**\n- mc boot: neither late-load branch runs, so `g_CfgLate` stays 0. Same as before.\n- No-argv boot: same, stays 0.\n- cdrom/host/SMB boot with a card: `load && OnFS` is 0. Same as before, so no redundant mode flash.\n- Fallback boot with no card, resolved to USB/MMCE: `OnFS=1`, so same as before.\n- Reachable USB/MMCE/udpfs boot: same whenever the load succeeds. When it fails, `g_Config` is defaults, so the old guard was false too.\n- Only behavioural difference besides the fix: a truncated `SMS.cfg` (short read) no longer triggers the re-init. Negligible.\n- `&&` ordering guarantees `SMS_LoadConfig` runs before `SMS_ConfigOnFS` is read. The flag is written and read on the main thread only. It is plain C89.\n\n**Hardware risks**\n- The added `GUI_Initialize(0)` is the same call USB boots have made at this exact point since 23a95e5/9cfe235, and every player return makes it (`SMS_GUICmdProc.c:200/301`). There is no new thread, semaphore or RPC interaction.\n- A saved non-default mode on the card is now applied at boot on HDD boots. Before, it was applied only at the first later re-init, such as the first return from playback.\n- A SELECT+R1/R2 boot override is therefore defeated immediately on HDD boots, as it already is on USB boots. Before, it lasted until the first later re-init. The separate boot-override fix (`g_BootDisplayMode` in `SMS_LoadConfig`) composes cleanly with this and should land together.\n- If the saved skin cannot be drawn (file missing on card, or an HD mode), the second `Desktop_Render` replays the splash. USB boots already behave this way.\n\n**Alternatives considered, not better**\n- `SMS_ConfigOnFS() || SMS_ConfigFallback()` in the guard: forces a redundant re-init on every cdrom/host/SMB boot with a card and defeats the boot override there too.\n- Dropping pfs/hdd from the CWD list in `SMS_ConfigSetCWD`: fixes HDD only, misses phantom `mass:`, and changes more boot behaviour.\n- A `main.c`-only snapshot of `SMS_ConfigOnFS()` before `SMS_IOPInit`: about the same size and less clear.\n\n**When applying**\n- Rewrite the two comment blocks around the guard in `main.c`; they will be stale.\n- Do not copy the reporter's `patch2.py` verbatim: it also adds `SMS_LocaleInit`/`SMS_LocaleSet` to the fallback branch, which belongs to a different finding.",
 "corrected_fix": ""
}

## #2 _bgTexUnsafe treats SD canvas widths 641..720 as HD: skin, wallpaper and splash vanish when the user widens the picture
- issue: 27
- severity: medium
- category: bug
- files: ['src/SMS_GUIDesktop.c', 'tools/test_skin_loading.py', 'src/SMS_GUIMenuSMS.c']
- reproduced: True
- confidence: 0.85
- risk: Low. Restores upstream behaviour for SD widths; also re-enables jellyfish wallpaper and splash at 641..720 (256x256 and splash textures are far smaller than the skin). HD modes stay gated. Not hardware-tested at width 720.

### evidence
src/SMS_GUIDesktop.c:149-153 'return g_GSCtx.m_Width > 640;' with the comment 'Every SD-class mode ... is exactly 640 wide' (commit 1c67b63). That premise is false: src/SMS_GUIMenuSMS.c:2652-2673 (_dispw_handler) lets the user set SD width from 640 to 720 (s_lMin 640, s_lMax 720; HD starts at 1090), and GSContext_Init copies it into g_GSCtx.m_Width (src/SMS_GS_1.c:90). DrawSkin returns 0 at line 163, _DrawSplash returns at 469, and Desktop_Render paints the HD gradient at 614. VRAM arithmetic shows no collision at SD: widest SD framebuffer 768x592x32bit = 0x1BC0 blocks, 800x600 skin sits at 0x4000-0x1EE0 = 0x2120. 2.9rev4 had no such gate. Host simulation scenarios G (USB, width 720) and H (mc boot, width 656): skin NOT visible on every boot device. Extended host test: new case sd-width-720 aborts (DrawSkin()==0) on master sources.

### fix
src/SMS_GUIDesktop.c:151: 'return g_GSCtx.m_Width > 720;   /* SD modes are user-adjustable 640..720 ( _dispw_handler ); only 720p ( >= 1090 ) / 1080i ( >= 1400 ) collide */' and correct the sentence in the comment at 147-148. One token.
> Review note (updated at merge): DrawSkin now also rejects any skin whose texture would start below g_GSCtx.m_VRAMPtr (the first free block after the framebuffer and font), the exact bound from finding 8. The skin loading test pins it (tall-skin-overlap rejected, skin-exact-fit accepted). Still not run on hardware.

### test
Add to tools/test_skin_loading.py: after the card-fallback line 'else if(!strcmp(argv[1],"sd-width-720")) g_GSCtx.m_Width=720;' and 'else if(!strcmp(argv[1],"hd-width-1090")) g_GSCtx.m_Width=1090;'; add sd-width-720 to the 'expected' list and both names to 'cases'. Run in scratch: FAILS on master sources (sd-width-720 SIGABRT), PASSES on the patched copy with BDM on and off (25+8 cases). Simulation G/H become VISIBLE.

### VERDICT diagnosis
{
 "real": true,
 "fix_sound": true,
 "reproduced": true,
 "reasoning": "I could not refute it: the defect is real and is a fork regression, but nothing ties it to the #27 reporter, who never says they changed the display width.\n\n**What I read (current tree, c275476)**\n- `src/SMS_GUIDesktop.c:149-153`: `_bgTexUnsafe()` is `return g_GSCtx.m_Width > 640;`, and its comment claims every SD mode \"is exactly 640 wide\".\n- Four callers: `DrawSkin` (line 163, returns 0), `_DecodeJellyfish` (328, picks the 256x256 mini background), `_DrawSplash` (469, returns), and `Desktop_Render` (614, paints the gradient instead of the wallpaper).\n- `src/SMS_GUIMenuSMS.c:2652-2673`: `_dispw_handler` steps the width by +/-1 (`SMS_GUIMenu.c:316/320`) and clamps SD modes to 640..720; HD starts at 1090.\n- \"Apply settings\" (`_apply_handler` -> `_reinitialize`) applies it live, and `GSContext_Init` (`src/SMS_GS_1.c:89-90`) copies `g_Config.m_DispWH` into `m_Width`. The value is saved in the config, so it also applies at every boot.\n- So 641..720 is a supported, reachable SD width, and the comment's premise is false. Defaults are all 640 (`SMS_Config.c:557-572`), so only users who widened the picture are affected.\n\n**Upstream comparison (2.9rev4 clone)**\n- `DrawSkin` has no gate at all; the gate arrived in fork commits 1d2f98f / 1c67b63.\n- `_dispw_handler` has the same 640..720 SD range.\n- `GSContext_Init` and `GS_InitGC` (`SMS_GS_0.S`) are unchanged in the fork apart from 720p centring and register-name syntax, so the VRAM layout at SD is identical to upstream.\n\n**VRAM check (replicated the `GS_InitGC` maths in Python for all 8 modes, both colour depths, min/max width and height; GUI runs with ZTest off)**\n- Worst SD case is 576p at 720x592, 24-bit: framebuffer ends at 0x1C80. Other SD modes at 720x576 end at 0x1B00; 16-bit including the 496-block font ends at 0x10F0 or lower.\n- An 800x600 skin sits at 0x2120, so there is no collision at any SD width up to 720.\n- Two details in the finding are wrong but do not change the conclusion: the framebuffer end is 0x1C80, not 0x1BC0, and the jellyfish and splash are 512x512 (texture at 0x3000), not 256x256. Both are still well clear of the framebuffer.\n- 720p and 1080i at 24-bit do collide (framebuffer ends at 0x3A80 or higher), so they must stay gated; `> 720` keeps them gated.\n\n**Host reproduction**\nI copied `tools/test_skin_loading.py`, `src/SMS_GUIDesktop.c` and `src/SMS_IPU_1.c` into `(investigator scratch, not kept)/verify-i27-skin-language/{master,patched}` and added a `w=<N>` case that sets `g_GSCtx.m_Width` and prints the result of the real `DrawSkin` (gcc 14, ASan/UBSan, WSL Debian).\n\n| Width | master `DrawSkin()` | patched (`> 720`) |\n|---|---|---|\n| 640 | 1 (TexPtr 0x2120) | 1 (TexPtr 0x2120) |\n| 641, 656, 720 | 0 | 1 (TexPtr 0x2120) |\n| 721, 1090, 1216, 1820 | 0 | 0 |\n\nThe stock 29-case suite passes on both trees with BDM on and off.\n\n**Fix assessment**\n- The one-token change to `> 720` is minimal and restores upstream behaviour for SD.\n- An out-of-range SD width above 720 from a corrupt config stays gated, which is the safe side.\n- The proposed `sd-width-720` / `hd-width-1090` test cases are consistent with the harness invariants: the gate returns before any GS context change, open or allocation.\n\n**Caveats**\n- Not hardware-tested; the evidence is host execution of the production function plus VRAM arithmetic.\n- The other three effects (splash skipped, gradient instead of wallpaper) are confirmed by code reading only; the host test exercises `DrawSkin` alone.\n- This should be filed as a regression found while investigating #27, not as #27's root cause. Severity is low to medium.",
 "corrected_fix": ""
}

### VERDICT fix
{
 "real": true,
 "fix_sound": true,
 "reproduced": true,
 "reasoning": "I tried to refute both the defect and the fix and could not. The defect is real and the one-token fix is correct, minimal and safe as far as host tests, a PS2 build and VRAM arithmetic can show. Nothing was run on hardware.\n\n**Defect (confirmed by code and host test)**\n- `src/SMS_GUIDesktop.c:151` is `return g_GSCtx.m_Width > 640;`, with a comment claiming every SD mode is exactly 640 wide.\n- That premise is false. `_dispw_handler` (`src/SMS_GUIMenuSMS.c:2652-2673`) clamps SD modes to 640..720 (`s_lMin` 640, `s_lMax` 720; 720p starts at 1090, 1080i at 1400).\n- `GSContext_Init` (`src/SMS_GS_1.c:89-90`) copies `g_Config.m_DispWH[idx][0]` straight into `m_Width`. `SMS_Config.c` only sets defaults; I found no clamp on load.\n- So at SD widths 641..720 `DrawSkin` returns 0 (line 163), `_DrawSplash` returns (line 469) and `Desktop_Render` paints the HD gradient instead of the wallpaper (line 614).\n- Upstream 2.9rev4 has the same 640..720 SD range and no gate in `DrawSkin`, so this is a fork regression from commits 1d2f98f/1c67b63. Both commit messages describe the gate as reasoning only (\"Blind fix ... needs hardware confirmation\").\n- Caveat on issue linkage: the #27 reporter never said they widened the picture, so this is a genuine defect but not shown to be the cause of #27.\n\n**What I ran**\nAll in an isolated copy at `(investigator scratch, not kept)/verify-fix-i27-skin-language/bgtex720/` (`pre`, `post`, `postgit`).\n- **Unpatched source, extended test:** with `sd-width-720` and `hd-width-1090` added to `tools/test_skin_loading.py`, the run fails: `Assertion 'DrawSkin()==expected' failed ... 'sd-width-720' died with SIGABRT`.\n- **Patched source, same test:** `PASS=1 FAIL=0`, BDM on and off.\n- **Full CI host list on the patched tree:** `PASS=44 FAIL=0`.\n- **PS2 builds with the pinned toolchain:** `make all`, `make BDM=0 all` and `make SMS_DIAG=1 all` each returned rc=0. The only `SMS_GUIDesktop.c` warning is the existing \"unused variable `i'\" at line 590.\n- **Binary diff:** the patched standard `SMS.elf` (1810056 bytes) differs from the baseline in exactly one byte, offset 354989, 0x81 to 0xD1. That is the `sltiu` immediate going from 641 to 721. No other code or layout change.\n\nA first attempt in the shared `verify-fix-i27-skin-language/tree` directory was contaminated by another agent's concurrent `g_CfgLate` patch, so I discarded those results and redid everything in `bgtex720`.\n\n**Why re-enabling the textures at 641..720 is safe**\n- **Framebuffer extent:** `GS_InitGC` (`src/SMS_GS_0.S:510+`) uses a single framebuffer at FBP=0. Worst SD case is 720x592 in 24-bit: FBW 12 \u00d7 19 page rows \u00d7 32 = 0x1C80 blocks. In 16-bit it is 0xF00 plus the 496-block font, ending at 0x10F0. At 640 the figures are 0x17C0 and 0x0E70.\n- **Texture placement:** the wallpaper and splash are both 512x512 (ffprobe on `images/jellyfish.jpg` and `images/splash.jpg`), so they sit at 0x3000. The decoded size does not depend on screen size. An 800x600 skin sits at 0x2120. All are above 0x1C80.\n- **Other VRAM users:** icons are transient at 0x3FC0, and the skin is blitted once then captured into `m_pDBuf`, as upstream does.\n- **Render path:** `GSContext_RenderTexSprite` matches upstream apart from the `u64` typedef.\n- **HD unchanged:** HD widths are at least 1090, so 720p and 1080i stay gated.\n- **Scope of change:** `_bgTexUnsafe` is static with four callers, all in `SMS_GUIDesktop.c`. No thread, IOP, alignment or C89 implications. Extra EE memory at 720 is only the larger `m_pDBuf` (about 142 KB at most), which is already allocated today.\n\n**Corrections to the reporter's evidence (conclusion unaffected)**\n- The widest SD framebuffer is 0x1C80, not 0x1BC0; 592 rows round up to 608.\n- The SD wallpaper is the 512x512 jellyfish at 0x3000, not the 256x256 mini image, which is only used in HD.\n\n**Residual, pre-existing and not introduced by the fix**\n- `DrawSkin` only checks `lTexSize < 0x4000`, not overlap with the framebuffer. An oversized skin (for example 1024x576, placed at 0x1C00) would overlap a 720x592 24-bit framebuffer.\n- Upstream has the same behaviour, and the same class of overlap exists at width 640 with slightly larger skins. A precise framebuffer-end check would be a separate, larger change.\n\nA mode-based gate (720p or 1080i) would be semantically exact but is more code, not less; `> 720` is the smallest correct change. The proposed test additions pin the bug and pass.",
 "corrected_fix": ""
}

## #3 Boot-time video-mode rescue (SELECT+R1 = NTSC, SELECT+R2 = PAL) is undone on USB/MMCE boots
- issue: 0
- severity: medium
- category: bug
- files: ['src/SMS_GUI.c', 'src/main.c', 'src/SMS_IOP.c']
- reproduced: False
- confidence: 0.65
- risk: Low; only changes behaviour when the pad combo is held. As on mc boots today, the overridden mode is what a later Save Settings persists.

### evidence
src/SMS_GUI.c:747-772 stores the pad override only in g_Config.m_DisplayMode. On a non-mc FS boot SMS_IOPInit calls SMS_LoadConfig again (src/SMS_IOP.c:1559), which resets g_Config and re-reads the saved mode; the main.c guard then sees mode != Default and GUI_Initialize(0) switches to the saved mode. A user who saved 1080i/VESA on a TV that cannot display it therefore cannot recover with the pad on a USB/MMCE boot (black screen until SMS.cfg is deleted on a PC). The comment at src/SMS_IOP.c:1461-1468 acknowledges that a second SMS_LoadConfig 'would CLOBBER the boot-time video-mode override' and excludes only mc boots for that reason. Code reading; not simulated.

### fix
Keep the override outside g_Config: in src/SMS_GUI.c add a file-scope 'int g_BootVMode = GSVideoMode_Default;', set it where lVideoMode is set today (lines 749/754, or simply assign after the loop at 772), and in src/main.c right after 'SMS_IOPInit ();' add 'if ( g_BootVMode != GSVideoMode_Default ) g_Config.m_DisplayMode = g_BootVMode;'. The existing guard then re-inits into the same mode already on screen (or skips). About 3 lines.

### test
Host: extend the boot simulation with a saved non-default mode plus an override and assert the mode passed to the second GUI_Initialize equals the override. Hardware: save 1080i, reboot from USB holding SELECT+R1, expect NTSC.

### VERDICT diagnosis
{
 "real": true,
 "fix_sound": true,
 "reproduced": true,
 "reasoning": "Could not refute. The defect exists in the current tree, and I reproduced it in a host simulation; it is not hardware-verified.\n\n**What I read (current tree, c275476)**\n- `src/SMS_GUI.c:712-772` (`GUI_Initialize(1)`): the SELECT+R1 / SELECT+R2 result lives only in the local `lVideoMode` and is written once to `g_Config.m_DisplayMode` at line 772. Nothing else remembers it.\n- `src/SMS_Config.c:524-536` (`SMS_LoadConfig`): unconditionally resets `g_Config.m_DisplayMode = GSVideoMode_Default`, then reads 892 bytes from `SMS.cfg` starting at `m_DisplayMode` (line 627).\n- `src/SMS_IOP.c:1459-1559`: on any config-on-FS boot whose path does not start with \"mc\" (mass, mmce, pfs/hdd, udpfs), the boot device is mounted and `SMS_LoadConfig()` runs again. Line 1581 does the same for SMB/host/cdrom fallback boots. The comment at 1460-1467 says outright that this reload would clobber the boot-time video-mode override, and gives that as the reason mc boots are excluded.\n- `src/main.c:107-110`: the guard `SMS_ConfigOnFS() && (m_DisplayMode != Default || m_SkinName[0]) && !mc` then calls `GUI_Initialize(0)`, which runs `GSContext_Init(g_Config.m_DisplayMode)` at `SMS_GUI.c:815` with the saved mode.\n- `Makefile:3` has `BDM ?= 1`, so this path is in the default build.\n\n**Upstream comparison**\nIn 2.9rev4, `SMS_LoadConfig` is called only inside `GUI_Initialize` (`SMS_GUI.c:658` and `:698`); `SMS_IOPInit` never reloads the config and there is no second `GUI_Initialize` in `main.c`. The override cannot be lost there, so this is a fork regression from the late FS config reload (history: b5e82c3, 9cfe235).\n\n**What I ran**\n`(investigator scratch, not kept)/verify-i27-skin-language/vmode_sim.py`, adapted from the reporter's boot sim. It compiles the real `SMS_ConfigSetCWD`, `SMS_LoadConfig` and the real `main.c` guard expression with gcc + ASan/UBSan against an in-memory fio with per-device mount state. The `GUI_Initialize` pad lines and the `SMS_IOPInit` reload are mirrored by hand, not extracted.\n\n| Scenario (current code) | First init | After late reload | Guard | On screen |\n|---|---|---|---|---|\n| USB boot, saved 1080i, SELECT+R1 | NTSC | 1080i | TRUE | 1080i, override lost |\n| MMCE boot, saved 1080i, SELECT+R2 | PAL | 1080i | TRUE | 1080i, override lost |\n| mc boot, saved 1080i, SELECT+R1 | NTSC | no reload | FALSE | NTSC, ok |\n| host boot, card cfg 1080i, SELECT+R1 | NTSC | 1080i | FALSE | NTSC for now |\n\nIn the host-boot row `g_Config.m_DisplayMode` is left at 1080i, so the override is lost at the next `GUI_Initialize(0)` or player `GSContext_Init`. The same latent mismatch appears on a USB boot with saved Default plus an override (screen NTSC, config Default).\n\n**Proposed fix**\nWith the fix emulated in the same harness (`g_BootVMode` set from `lVideoMode`, re-applied after `SMS_IOPInit`), all six scenarios end with on-screen mode equal to the override and `g_Config.m_DisplayMode` equal to the override. The no-pad case still switches to the saved 1080i, so normal boots are unchanged. The fix is sound and minimal, with three notes for whoever applies it:\n- `main.c` needs `extern int g_BootVMode;` (or a declaration in `include/SMS_GUI.h`); `m_DisplayMode` is `int`, so `int` is the right type.\n- Assign it unconditionally after the pad loop (`g_BootVMode = lVideoMode;` next to line 772).\n- The restore must sit after `SMS_IOPInit()` and before the guard at `main.c:107`. One site in `main.c` covers both reload sites (`SMS_IOP.c:1559` and `:1581`), which patching inside `SMS_IOPInit` would not.\n\nA variant with no global (snapshot `g_Config.m_DisplayMode` in `main.c` after `GUI_Initialize(1)`) would be wrong: on SMB/host/cdrom boots the early load already reads the card's saved mode, which would then be mistaken for an override.\n\n**Risk and limits**\n- With an override held on an FS boot, the guard is now always true, so `GUI_Initialize(0)` re-inits into the mode already on screen. That is the same benign repaint the skin-only path does today.\n- A later Save Settings persists the overridden mode, as on mc boots and upstream.\n- The reporter's \"black screen until SMS.cfg is deleted on a PC\" consequence is plausible but not verified on hardware. It also rests on the fork's own comments that the pre-mount `fioOpen` of `mass:`/`mmce:` fails in `GUI_Initialize`.",
 "corrected_fix": ""
}

## #4 Boot splash (fade in, 1 s hold, fade out) replays on every full desktop rebuild; upstream's first-render-only gate was lost
- issue: 0
- severity: low
- category: bug
- files: ['src/SMS_GUIDesktop.c']
- reproduced: False
- confidence: 0.7
- risk: Low. If the owner intends the splash after every playback this is a behaviour change; the comment and upstream both say otherwise. _DrawJellyfish(1) still fades the wallpaper in.

### evidence
src/SMS_GUIDesktop.c:612 calls _DrawSplash() unconditionally whenever the desktop packet is rebuilt without a skin. That happens on every GUI_Initialize(0): return from playback (src/SMS_GUICmdProc.c:200, 301), applying display settings (src/SMS_GUIMenuSMS.c:1108), and the FS-boot re-init in main.c (double splash at boot with a non-default mode). _DrawSplash is 17 vsyncs + SMS_TimerWait(1000) + 17 vsyncs, about 1.6 s each time, and its own comment (line 455) says 'Shown once at boot'. 's_Init' (line 76) is set at 669 and never read; in 2.9rev4 the same variable gated the slow logo animation: 'if ( !s_Init ) SMS_TimerWait ( 30 );' (upstream src/SMS_GUIDesktop.c:274). Code reading; no hardware.

### fix
src/SMS_GUIDesktop.c:612: 'if ( !s_Init ) _DrawSplash ();'. One token; gives the dead variable its upstream purpose back.

### test
Hardware/emulator only: play a file and stop; the browser should return without the splash. Boot from USB with a saved non-default mode: one splash, not two.

### VERDICT

Not independently checked (below the verification cut-off, or its checker did not finish).

## #5 Late-loaded picture offsets, display width/height, colour depth and charset are not applied at boot on USB/MMCE when mode is Default and no skin is set
- issue: 0
- severity: low
- category: bug
- files: ['src/main.c', 'src/SMS_GUI.c', 'src/SMS_IOP.c']
- reproduced: False
- confidence: 0.6
- risk: Adds a GUI re-init to every FS boot that has a saved config (today only those with a skin or non-default mode). The existing comment claims a same-mode re-init does not flash; that is unverified on hardware, so treat as needs-validation.

### evidence
Only GUI_Initialize applies g_Config.m_DispDXDY (src/SMS_GUI.c:805-808), m_DisplayCharset (809) and m_DispWH / m_ColorDepth (via GSContext_Init, src/SMS_GS_1.c:79-92). After the late config load SMS_IOPInit calls only GUI_SetColors (src/SMS_IOP.c:1560), and the main.c guard re-inits only for a non-default mode or a skin. So on the most common FS boot (auto PAL/NTSC, no skin) a saved screen-centering offset, widened picture or non-Latin1 charset stays at defaults until something else triggers GUI_Initialize(0), typically the first playback. Code reading.

### fix
After the g_CfgLate change above, simplify the guard to 'if ( g_CfgLate ) GUI_Initialize ( 0 );' (drop the mode/skin clause). Land it only together with the splash-once fix, otherwise every USB boot with a saved config shows the splash twice.

### test
Hardware: on a USB boot save a non-zero image offset with Default mode, reboot, confirm the picture is offset immediately. Host: the simulation's gs_init can assert m_Width after boot for a saved 720 width.

### VERDICT

Not independently checked (below the verification cut-off, or its checker did not finish).

## #6 cdrom/host/SMB boots: late config reload clears SMS_BF_UDFL without re-running the locale init, so the Language row reports a valid pack as bad
- issue: 0
- severity: low
- category: bug
- files: ['src/SMS_IOP.c', 'src/SMS_Config.c', 'src/SMS_GUIMenuSMS.c']
- reproduced: False
- confidence: 0.7
- risk: Very low; same two calls the FS branch already makes at the same point.

### evidence
SMS_LoadConfig ends with 'g_Config.m_BrowserFlags &= ~SMS_BF_UDFL;' (src/SMS_Config.c:691); the flag is set again only by SMS_LocaleInit. The FS branch re-runs SMS_LocaleInit/SMS_LocaleSet after its reload (src/SMS_IOP.c:1561-1562) but the fallback branch (1580-1582) does not. _lang_handler (src/SMS_GUIMenuSMS.c:1180-1215) then takes the 'UDFL clear and g_LngLines > 0' path and shows s_pBadLng ('found but has %d lines; this version needs %d') with 311 and 311, and the user-language toggle is unavailable. Code reading.

### fix
src/SMS_IOP.c after line 1582 'GUI_SetColors  ();' add 'SMS_LocaleInit ();' and 'SMS_LocaleSet  ();' (mirrors 1561-1562). Two lines; included in the private copy that builds.

### test
Hardware/emulator: boot from host: or disc with a valid mc0:/SMS/SMS.lng, open the Language row, expect the pack name instead of the line-count message.

### VERDICT

Not independently checked (below the verification cut-off, or its checker did not finish).

## #7 Skin / MBF names longer than 63 characters overflow g_Config.m_SkinName / m_MBFName
- issue: 0
- severity: low
- category: bug
- files: ['src/SMS_EE.c', 'src/SMS_GUIMenuSMS.c', 'include/SMS_Config.h']
- reproduced: False
- confidence: 0.7
- risk: None beyond hiding over-long file names from the two pickers.

### evidence
SMS_EEScanDir (src/SMS_EE.c:752-784) accepts any entry with 'lLen > 4' and lists the name minus extension. _usebg_handler does 'strcpy ( g_Config.m_SkinName, _STR( lpNode ) );' (src/SMS_GUIMenuSMS.c:2089-2113) into a 64-byte field, and the MBF handler does the same at 2564. This was harmless on memory cards (32-char names) but the fork now scans <boot dir>/Skins on FAT, where long file names are legal; the bytes after m_SkinName are m_SMBIP, the deltas, m_MBFName and then the list pointers m_pSkinList/m_pSMBList/m_pMBFList (include/SMS_Config.h), and the corrupted struct is what SMS_SaveConfig writes. Code reading.

### fix
src/SMS_EE.c: change 'if ( lLen > 4 )' to 'if ( lLen > 4 && lLen < 68 )' so a name that cannot fit (63 chars + extension) is simply not listed. One condition, covers both skins and MBF fonts.

### test
Host: compile SMS_EEScanDir against the fake fio from boot_skin_sim.py with a 100-character '.smi' entry and assert the list stays empty; ASan on the strcpy path before the fix.

### VERDICT

Not independently checked (below the verification cut-off, or its checker did not finish).

## #8 DrawSkin places the skin at 0x4000 - size without checking it clears the framebuffer; a large skin (e.g. 1024x768) overlaps the SD framebuffer
- issue: 0
- severity: low
- category: bug
- files: ['src/SMS_GUIDesktop.c']
- reproduced: False
- confidence: 0.5
- risk: Rejects skins that previously 'loaded' corrupted; needs the test stub change. Low priority.

### evidence
src/SMS_GUIDesktop.c:259-267 only rejects lTexSize >= 0x4000. A 1024x768 skin gives lTexSize = (1024*768*4)>>8 = 0x3000, so it is uploaded at block 0x1000 while a 640x512 32-bit framebuffer occupies blocks 0..0x1400: the IPU upload writes into the visible framebuffer and the sprite then samples what the frame overwrites. Same arithmetic as upstream (which had no checks at all), so not a regression; arithmetic only, not run.

### fix
Optional hardening, one condition: 'if ( lTexSize >= 0x4000 || 0x4000 - lTexSize < g_GSCtx.m_VRAMPtr ) {' (m_VRAMPtr is the first free block after the framebuffer, and after the font in 16-bit mode). This exact bound would also make the width heuristic in DrawSkin unnecessary, but the one-token '> 720' fix is the smaller change.

### test
tools/test_skin_loading.py would need m_VRAMPtr added to its g_GSCtx stub (set 0x1400) and a 1024x768 case expecting DrawSkin()==0.

### VERDICT

Not independently checked (below the verification cut-off, or its checker did not finish).

## #9 Legacy lng/ directory: all 19 packs are rejected by the current loader, and its README tells users to install them
- issue: 0
- severity: low
- category: cleanup
- files: ['lng/README.txt', 'lng', 'docs/translation/TRANSLATING.txt']
- reproduced: True
- confidence: 0.9
- risk: None to the build (unreferenced). Attribution must be preserved somewhere before deleting.

### evidence
lngcheck.py emulates SMS_LocaleInit's parser against the current table (311 entries): every lng/SMS.lng_* parses to 269 lines (the Turkish one to 270, stopping at an empty line, and it is multi-byte UTF-8), so each is rejected ('needs 311'). lng/README.txt still says to rename one to SMS.lng and copy it to mc0:/SMS. git grep finds no reference to lng/ in the Makefile, CI or docs; the release zips docs/translation only (.github/workflows/compilation.yml:133), whose 20 files all parse to exactly 311 lines with zero format-specifier mismatches.

### fix
Delete lng/ (the translator credits in lng/README.txt are already carried in CHANGELOG.md; if the owner wants them kept verbatim, move the author list into docs/translation/TRANSLATING.txt first). Smaller alternative: replace lng/README.txt with a one-line pointer to docs/translation. Removing credited files is the owner's call under the repository attribution policy.

### test
python lngcheck.py <repo> from the scratch directory prints one line per pack with parsed count, encoding and mismatches; after removal only docs/translation remains and all report parsed=311.

### VERDICT

Not independently checked (below the verification cut-off, or its checker did not finish).

## #10 English template docs/translation/SMS.lng drifted from the built-in table on two lines (and every pack carries the older wording of one of them)
- issue: 0
- severity: low
- category: docs
- files: ['docs/translation/SMS.lng', 'src/SMS_Locale.c']
- reproduced: True
- confidence: 0.9
- risk: None.

### evidence
lngcheck.py 'template vs built-in table: 2 differing lines' (0-based table indices 39 and 272): template 'Start HDD support' versus built-in 'Start HDD APA support', and template 'Start HDD BDM' versus built-in 'Start HDD BDM support'. Translators working from the template translate the stale wording, and a user who installs the English template gets labels that differ from the built-in ones. All other 309 lines are byte-identical.

### fix
Edit the two lines of docs/translation/SMS.lng to match src/SMS_Locale.c (keep CRLF, keep exactly 311 lines). Optionally add lngcheck's template-equals-table assertion to an existing locale host test so the template cannot drift again.

### test
python lngcheck.py <repo> should report '0 differing lines' for the template.

### VERDICT

Not independently checked (below the verification cut-off, or its checker did not finish).

