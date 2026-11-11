# hunt-gui-boot-config

Boot/config/GUI hunt (read-only; repo untouched, git status unchanged). Read in full: src/main.c, SMS_IOP.c, SMS_Config.c, SMS_GUI.c, SMS_GUIMenuSMS.c, SMS_History.c, SMS_PAD.c (+include/SMS_PAD.h), SMS_GUIFileCtxMenu.c; SMS_GUIDesktop.c second half; targeted string/buffer review of SMS_GUIFileMenu.c, SMS_GUIDevMenu.c, SMS_GUISMBrowser.c, SMS_Locale.c. Compared against upstream 2.9rev4 where relevant.

Empirical work (all in scratch): (investigator scratch, not kept)/hunt-gui-boot-config/repro.py extracts the PRODUCTION function text and runs it on host (WSL Debian, gcc 14, ASan/UBSan) -> 4 findings reproduced (import self-destruct, IPCONFIG CRLF parse, IP editor stale/overflow, config-field overflow layout). (investigator scratch, not kept)/hunt-gui-boot-config/apply_fixes.py applies every proposed fix (except the s_PadBuf0, SMS_PAD.c dead-function, MX4SIO-probe and UXH items) to a private tree copy (.../hunt-gui-boot-config/tree); that copy builds with the pinned ps2dev v1.0 toolchain (rc=0, SMS.elf same 1810056 bytes, warnings 18 -> 12) and the six host tests touching the patched files pass (check_devmenu, test_browser_artwork_lifecycle, test_browser_reload, test_display_limits, test_playlist_menu, test_skin_loading). No console/emulator run: nothing here is hardware-accepted.

Answers to the specific questions: (1) `_autonet_handler` is NOT lost wiring: it is the non-BDM `s_DevMenu` row handler (SMS_GUIMenuSMS.c:258); BDM reaches the same function through _autohost/_autosmb/_autoudpfs handlers. Guard it with #ifndef BDM. (2) `s_pSMSCfg` is a plain leftover (config path now comes from s_pMC0SMC/SMS_ConfigPath) -> delete. (3) SMS_IOP.c:1419-1420 unused `i`,`lFD`,`lBuff` are leftovers of the IPCONFIG read being extracted into _read_ipconfig -> delete. (4) History: no leftovers; it is live (SMS_Player.c Look/Add/Save/Remove, main.c Load). (5) `s_PadBuf0` is unused because BDM defines PAD_OpenPort() to nothing.

Ruled out (no defect found): path-buffer overruns in SMS_ConfigSetCWD/_derive_smb_path/SMS_ConfigAssetPath/SMS_HistorySave/_copy_cwd_or_mc/_copy_bim_handler/_do_sms_action (bounded by the 119-char boot dir and 255-byte dirent names); file-browser path code (_make_path/_fill/_action_folder/artwork/subtitle lookup all length-checked); BDM s_DevMenu row indices vs _switch_flag calls (2..13 match, also covered by tools/check_devmenu.py); SMS_ConfigNetMode/_switch_netmode logic; SMB add/edit form buffers (sscanf %3, 16-byte fields, >=14-char name rejected); SMB/DevMenu sprintf buffers; _saveipc_handler buffers.

Noted but not filed as findings (minor / unverifiable): SMS_LocaleInit reads the uninitialised byte lpAlloc[lSize] when a short .lng ends in a bare CR (one-line hardening: lpAlloc[lSize]=0 after the read); SMS_LoadConfig re-allocates the skin/MBF lists on each of its 2-3 boot-time calls (tiny one-off leak); only m_SkinName gets a post-load NUL guard (m_MBFName/m_Language/m_Partition/m_SMBIP do not); _exit_handler indexes s_ExitTo[flags>>28] unguarded against a corrupt config (upstream-identical); _netprot_handler lets SMB be chosen with no server list and SMS_LoadConfig silently turns it back into HOST next boot; _saveipc_handler leaves the 'Saving' status text on the no-card CWD success path (cosmetic); g_SaveDiag 'TEMP DIAGNOSTIC' is still live in _save_handler; g_PadBuf1 (256 bytes) is effectively unused in BDM.

## #1 File-menu import (language / palette / SMB list / skin) destroys the file when it is already at its install location, and a failed import deletes the previously installed file
- issue: 0
- severity: high
- category: bug
- files: ['src/SMS_GUIFileCtxMenu.c']
- reproduced: True
- confidence: 0.9
- risk: Low. Same fio calls, only reordered; peak memory unchanged (one buffer of the file size, as before). Behaviour change limited to failure paths and the self-copy case. Needs a quick console check of language/skin import on USB and memory card since fio semantics on real devices were only modelled on host.

### evidence
_copy_file_to_mc (SMS_GUIFileCtxMenu.c:143-189) opens the source, then opens the destination with O_CREAT|O_WRONLY|O_TRUNC (O_TRUNC added by b19371d) BEFORE reading the source, and on any failure runs fioRemove(apDst)/fioRmdir(apDst) unconditionally. In this fork the destination is the browsable boot folder (_copy_cwd_or_mc:217-233 -> '<boot dir>SMS.lng'/'SMS.pal'; _copy_smb_handler -> g_SMSSMB; _copy_bim_handler -> '<boot dir>Skins/<name>'), so the documented flow 'drop SMS.lng next to the ELF, then highlight it in the browser and use the file menu to install it' (the app's own hint, SMS_GUIMenuSMS.c:1173-1175) has source == destination. Upstream could not hit this (destination was the memory card, not browsable; no O_TRUNC). Host run of the production function text (scratch repro.py, POSIX fio shims): self-import on a permissive FS -> ret=1, file size 24 -> 0 (reported as success); self-import on a share-locking FS (second open refused) -> ret=0 and the file is DELETED; import whose source cannot be opened -> the existing good destination is DELETED. With the proposed fix all three keep the 24-byte file and the normal import still truncates a longer stale destination (size 5).

### fix
Rewrite _copy_file_to_mc so the source is read fully into memory and closed BEFORE the destination is opened with O_TRUNC, and remove the destination only when it was opened by us and the write failed (also rejects a 0-byte source instead of installing an empty file). Same size function, no new helpers; exact text is in scratch apply_fixes.py / repro.py (FIXED_COPY). Verified: compiles in the patched private tree with the PS2 toolchain.

### test
New host test (e.g. tools/test_ctxmenu_import.py) built from scratch repro.py: extract _copy_file_to_mc, POSIX fio shims, assert (a) dst==src keeps contents, with and without a write-open lock, (b) unreadable source leaves an existing destination intact, (c) normal import truncates a longer destination. It fails on current master and passes with the fix.

### VERDICT diagnosis
{
 "real": true,
 "fix_sound": true,
 "reproduced": true,
 "reasoning": "Verdict: the defect is real and I reproduced it against the real FatFs code rather than only POSIX shims. Severity is medium rather than high: it needs the user to run an import on a file that already sits in its install location, and the lost asset can be recopied from a PC. No console run was possible.\n\n**What I read (current tree, c275476)**\n- `src/SMS_GUIFileCtxMenu.c:143-189`, `_copy_file_to_mc`: opens the source, then opens the destination with `O_CREAT | O_WRONLY | O_TRUNC` (line 155) before reading the source. Any failure runs `fioRemove ( apDst ); fioRmdir ( apDst );` unconditionally (lines 182-185).\n- Destinations on a filesystem boot are browsable folders:\n  - `_copy_cwd_or_mc` (217-233) writes `<g_pBootDir>SMS.lng` / `SMS.pal`.\n  - `_copy_smb_handler` (302) writes `g_SMSSMB`, which follows `SMS.cfg` into the boot directory (`SMS_Config.c:111-171`).\n  - `_copy_bim_handler` (313-330) writes `<g_pBootDir>Skins/<name>`.\n- Trigger path: `SMS_GUIFileMenu.c:1100-1156`, `_context_action_file`, offers action 4/5/7/8 for any highlighted file named `SMS.lng`, `SMS.pal`, `SMS.smb` or `*.smi`, in any folder, with no check that it is not already the install target.\n- `SMS_ConfigSetCWD` sets `g_pBootDir` for mass/mmce/pfs/hdd/udpfs/mc boots, so source == destination is reachable by browsing to the ELF folder or its `Skins/` folder.\n- The cited hint (`SMS_GUIMenuSMS.c:1173-1175`) actually says to copy to `mc%d:/SMS/` \"or highlight a .lng file in the browser and use the file menu\". The reporter's \"drop it next to the ELF\" paraphrase is a stretch, but the flow is still plausible, most of all for skins already in `<boot dir>Skins/`.\n\n**Upstream 2.9rev4**\n- Same function with the same unconditional remove on failure, but no `O_TRUNC`, and the destination was always `mc?:/SMS/...`.\n- The \"failed import deletes the installed file\" half is therefore inherited, not a fork regression.\n- The self-import destruction is fork-made: boot-folder destinations (bf3ee98, 0f79d10) plus `O_TRUNC` (b19371d).\n\n**What I ran**\n- Harness: `(investigator scratch, not kept)/verify-hunt-gui-boot-config/h/harness.c`.\n- It compiles the production function text (extracted with awk from the repo) against real FatFs R0.16 from `fjtrujy/FatFs` branch `iop-r0.16`, the source ps2sdk's `bdmfs_fatfs` is built from. `FF_FS_LOCK` is 0 as shipped. It runs on a RAM disk with fio shims mirroring ps2sdk `fs_driver.c` (flag mapping, `SEEK_END` from the cached `obj.objsize`). Built with gcc and ASan/UBSan in WSL.\n\nProduction output, self-import (source path == destination path):\n\n| Volume | 24 B | 3000 B | 20000 B | 300000 B |\n|---|---|---|---|---|\n| FAT16, 4 KiB clusters | intact | intact | ret=0, FILE GONE | ret=0, FILE GONE |\n| FAT32, 512 B clusters | intact | ret=0, FILE GONE | ret=0, FILE GONE | ret=0, FILE GONE |\n| exFAT, 32 KiB clusters | intact | intact | intact | intact |\n\n- Mechanism on FAT16/FAT32: the truncating open frees the cluster chain while the read handle still caches the old size, so `f_lseek` hits a freed FAT entry, `lSize` goes negative, `malloc` fails and the function then unlinks the file.\n- Any file larger than one cluster is deleted. That covers every `.smi` skin, and a ~5.5 KB `SMS.lng` on a FAT32 stick with 4 KiB clusters.\n- Single-cluster files and contiguous exFAT files survive only by accident, by reading freed clusters.\n- \"Source cannot be opened, good 20000-byte destination\": ret=0, FILE GONE on all three filesystems.\n- Normal import of 5000 bytes over 20000: ret=1, size 5000, content correct.\n\n**Corrections to the reporter's evidence**\n- The \"permissive FS: file becomes 0 bytes with ret=1\" result is a POSIX artefact. Real FatFs deletes the file and shows Error, or survives by accident.\n- The \"locking FS\" model does not match ps2sdk, where `FF_FS_LOCK` is 0.\n- The destructive conclusion holds regardless.\n\n**Not verified**\n- Whether the prebuilt `irx/bdmfs_fatfs.irx` in the repo was built with the same FatFs configuration.\n- `mcman` behaviour for an mc boot.\n\n**The fix**\n- I applied the reporter's exact `FIXED_COPY` text to the same harness: every self-import size on all three filesystems returns ret=1 with content intact, the missing-source case leaves the 20000-byte destination intact, and the normal import still truncates to 5000.\n- I also applied it to a private tree and built it with the pinned PS2 toolchain (`ps2make.sh`, build name `verify-hgbc-diag`): `make all` rc=0, `SMS.elf` produced, no warnings in `SMS_GUIFileCtxMenu.c`.\n- It uses the same fio calls reordered, one buffer of the same size, block-start declarations that ee-gcc 3.2.3 accepts, and the `lSize > 0` guard also removes the old `malloc` of a negative size.\n- Behaviour changes: a 0-byte source is now rejected, and the destination is removed only when we opened it and the write failed.\n- The `EMBEDDED` caller `_copy_sms_handler` uses distinct paths and is unaffected.\n\nThe proposed POSIX-shim host test is acceptable. It pins the ordering bug, since the POSIX model fails on master by truncating to 0.",
 "corrected_fix": ""
}

### VERDICT fix
{
 "real": true,
 "fix_sound": true,
 "reproduced": true,
 "reasoning": "I could not refute the defect or the fix. The defect is reproduced on a host model only; real PS2 driver behaviour is not verified.\n\n**What I read**\n- `src/SMS_GUIFileCtxMenu.c:143-189` (current master c275476): `_copy_file_to_mc` opens the source, then opens the destination with `O_CREAT|O_WRONLY|O_TRUNC` before reading anything, and runs `fioRemove(apDst); fioRmdir(apDst);` on every failure, including a source that never opened.\n- `src/SMS_GUIFileMenu.c:1135-1153`: the import action is chosen purely by file name (`SMS.lng`, `SMS.pal`, `SMS.smb`, `*.smi`), whatever the directory. Highlighting the already-installed `<boot dir>SMS.lng` or `<boot dir>Skins/x.smi` on a writable BDM device therefore gives source == destination (`_copy_cwd_or_mc:217-233`, `_copy_bim_handler:313-330`, `_copy_smb_handler`).\n- Upstream 2.9rev4 has the same unconditional remove but no `O_TRUNC` and a memory-card-only destination, so the self-import case is fork-specific (b19371d plus the CWD-first import work).\n- Fix text: `(investigator scratch, not kept)/hunt-gui-boot-config/apply_fixes.py`, section A.\n\n**What I ran**\n1. Host harness `(investigator scratch, not kept)/verify-fix-hunt-gui-boot-config/vtest.py`: extracts the production function text, POSIX fio shims, gcc `-std=gnu89 -pedantic` with ASan/UBSan, 10 cases.\n\n| Case | Master | Fixed |\n|---|---|---|\n| 1 self-import, permissive FS | ret=1, size 24 -> 0, two fds open | ret=1, size 24, one fd |\n| 2 self-import, write-open refused | ret=0, file deleted | ret=1, size 24 |\n| 3 source missing, good destination | destination deleted | kept, 0 removes |\n| 4 normal import over longer file | size 5 | size 5 |\n| 5 destination absent | size 5 | size 5 |\n| 6 0-byte source | ret=1, destination emptied | ret=0, destination kept |\n| 7 write error / 8 short write | destination removed | destination removed |\n| 9 destination open refused | existing destination deleted | kept |\n| 10 lseek error | `malloc(-5)`, destination deleted | ret=0, destination kept |\n\nNo fd leaks in any case, and no sanitizer findings on the fixed version.\n\n2. Applied only fix A to a private tree (`.../scratchpad/verify-fix-hunt-gui-boot-config/tree`). The diff is confined to the function, CRLF preserved.\n3. PS2 build with the pinned ee-gcc 3.2.3: `make all` rc=0 and `make BDM=0 all` rc=0. `SMS_GUIFileCtxMenu.o` was built in both, with no warning from that file; the warning list is otherwise the baseline one.\n4. `tools/test_browser_reload.py`, the only existing host test that reads this file, passes on the patched tree.\n\n**Safety review of the fix**\n- **C89:** all declarations sit at block starts; `free(NULL)` is fine.\n- **Types:** the `s64` to `int`/`size_t` conversions are the same as before. A negative `fioLseek` result is now rejected instead of reaching `malloc`.\n- **Resources:** the same fio calls on the same GUI thread, with no new RPC or semaphore interaction. Peak heap is unchanged (one buffer of the file size) and there is one fewer simultaneously open fd.\n- **Flags and cleanup:** destination open flags are unchanged, so memory-card behaviour is the same. The `fioRemove`+`fioRmdir` pair is kept, but only for a destination we truncated and failed to write.\n- **Other callers:** `_copy_sms_handler` is under `EMBEDDED`, which the Makefile never defines, and its semantics would be unchanged anyway.\n- **Build variants:** the function is not inside any `BDM` ifdef, and both variants build. I did not build `SMS_DIAG=1`.\n\n**Caveats**\n- The fix adds one behaviour change: a 0-byte source is now an error with the old file kept, where master \"succeeded\" and installed an empty file. I consider that benign.\n- The rewrite drops the `O_TRUNC` rationale comment; a one-line version should be kept next to the open.\n- The fix cannot be made meaningfully smaller. A `strcmp(apDst, apSrc)` guard would miss device aliases and FAT case-insensitivity, and would not address the failed import deleting the previous install.\n- I could not confirm whether `bdmfs_fatfs` has `FF_FS_LOCK` enabled (two fetches of ps2sdk `ffconf.h` returned 404). Both modelled outcomes are destructive, and the fix removes the dual open in either case.\n- Severity reads as medium rather than high: it needs the user to run import on the already-installed file or to hit a failed import, and the loss is a re-copyable asset.\n- No console or emulator run was done; a quick language and skin import check on USB and memory card is still needed.\n\nThe proposed host test is sound: the same extraction fails on master in cases 1-3 and passes with the fix. It must be added to the CI list in `.github/workflows/compilation.yml` to run.",
 "corrected_fix": ""
}

## #2 Boot-time video rescue (SELECT+R1 = NTSC, SELECT+R2 = PAL) is discarded on every non-memory-card filesystem boot
- issue: 0
- severity: medium
- category: bug
- files: ['src/SMS_GUI.c', 'src/SMS_Config.c', 'include/SMS_Config.h', 'src/SMS_IOP.c', 'src/main.c']
- reproduced: False
- confidence: 0.8
- risk: Low code risk, but display-mode handling can only be accepted on a console. With the fix the forced mode is what a later 'Save settings' writes, which matches upstream behaviour.

### evidence
Code reading, not run. GUI_Initialize(1) applies the override once to g_Config.m_DisplayMode (SMS_GUI.c:772). On USB/MX4SIO/MMCE/HDD/UDPFS boots SMS_IOPInit then calls SMS_LoadConfig() again (SMS_IOP.c:1559; fallback branch :1581), and SMS_LoadConfig starts by resetting m_DisplayMode = GSVideoMode_Default (SMS_Config.c:536) before reading the saved value (:627/:657). main.c:107-110 then calls GUI_Initialize(0) whenever the reloaded mode != Default (or a skin is set), switching the display to the SAVED mode. So a user whose saved mode their TV cannot show cannot recover with the documented combo (docs/src/usage/controls.xml:37-49) and must delete SMS.cfg on a PC. The code's own comment at SMS_IOP.c:1459-1467 states the reload 'would CLOBBER the boot-time video-mode override' and exempts only mc boots. Upstream 2.9rev4 loaded config once, so this is a fork regression.

### fix
Keep the override in one global and re-apply it at the tail of SMS_LoadConfig: `int g_BootDisplayMode = GSVideoMode_Default;` in SMS_Config.c (+ extern in SMS_Config.h); SMS_GUI.c:772 becomes `g_Config.m_DisplayMode = g_BootDisplayMode = lVideoMode;`; after `g_Config.m_BrowserFlags &= ~SMS_BF_UDFL;` in SMS_LoadConfig add `if ( g_BootDisplayMode != GSVideoMode_Default ) g_Config.m_DisplayMode = g_BootDisplayMode;`. Four lines; SMS_LoadConfig is only called at boot, so later menu changes are unaffected. Compiles in the patched private tree.

### test
No honest host test (GUI_Initialize/SMS_LoadConfig pull in GS, pad and font code). Hardware acceptance: save 1080i (or any non-default mode) on a USB boot, reboot holding SELECT+R1 and confirm the session stays in NTSC through the post-IOPInit re-init; repeat on an mc boot to confirm unchanged behaviour.

### VERDICT diagnosis
{
 "real": true,
 "fix_sound": true,
 "reproduced": true,
 "reasoning": "I could not refute it: the parser defect is real and reproduces on the host, and the proposed fix is correct and small. The severity is overstated, though: I would call it low, not medium.\n\n**What I read**\n- `src/SMS_IOP.c:1367-1415` (`_read_ipconfig`): lines 1402-1404 turn every space, CR and LF into NUL, then lines 1406-1410 step over exactly one NUL per field (`i = strlen ( g_pDefIP ) + 1; ... i += strlen ( g_pDefMask ) + 1;`). Any run of two separators therefore yields an empty field and shifts the rest.\n- Callers: `SMS_IOPInit` calls it twice (lines 1436 and 1572). The globals feed the SMAP arguments (lines 538-543) and the IP editor (`src/SMS_GUIMenuSMS.c:1744-1746`).\n- Upstream 2.9rev4 `src/SMS_IOP.c:405-413` has the identical five lines, so this is not a fork regression. The fork only added the boot-folder probe.\n\n**What I ran**\nMy own harness, `(investigator scratch, not kept)/verify-hunt-gui-boot-config/v.py`, extracts the production `_read_ipconfig` text and the exact 16-byte global initialisers from `src/SMS_Locale.c:33-35`, and builds with gcc ASan+UBSan in WSL Debian.\n\n| Input | Production result |\n|---|---|\n| SMS's own save (LF-separated) | correct |\n| single spaces | correct |\n| spaces with trailing `\\n\\r` | correct |\n| CRLF lines | ip=[192.168.1.10] mask=[] gw=[255.255.255.0] |\n| double space | ip=[192.168.1.10] mask=[] gw=[255.255.255.0] |\n| leading blank line | ip=[] mask=[] gw=[192.168.1.10] |\n| IP only | mask=[] gw=[] |\n| whitespace only | all three blanked |\n\nNo sanitizer errors in any case; this is wrong values, not memory corruption.\n\n**Why severity is low**\n- **Not a documented workflow:** `docs/network.html` and `docs/configuration.html` only describe the on-screen editor. Triggering this needs a hand-authored file with CRLF or doubled separators.\n- **The card-less UDPFS case is unaffected:** it passes only `ip=%s` from `g_pDefIP` (line 960), and the IP field parses correctly for CRLF and double-space files.\n- **It self-heals:** saving once from the on-screen editor rewrites the file in the LF format.\n- **SMB impact is inferred, not tested:** an empty mask reaches SMSMAP as an empty argument, and `inet_addr(\"\")` returns -1 (`iop/SMSTCPIP/src/inet.s:604-611`, `iop/SMSMAP/main.c:312-318`). That would give netmask 255.255.255.255 and gateway 255.255.255.0. This assumes the IOP loader counts an empty argument string; no hardware was run.\n\n**Fix assessment**\nThe same harness with the proposed loop applied parses all of the above correctly, with ASan/UBSan clean:\n- CRLF, double-space, leading-blank and leading-space files give the right three fields.\n- A 15-character field in each slot fits; an over-long field is truncated to 15 with the NUL at byte 15 preserved, as before.\n- An embedded NUL or a file truncated at 63 bytes stops cleanly.\n- The loop is bounded by the data length, so it cannot read past `lBuff`.\n- The reporter's patched tree linked `bin/SMS.elf` with the pinned ee-gcc toolchain (`obj/SMS_IOP.o` is newer than the patched source; no errors in `tree.log`).\n\n**Corrections to the report**\n- The risk note says SMS writes single spaces. It actually writes `ip\\nmask\\ngw\\n` (`src/SMS_GUIMenuSMS.c:1947/1960/1974`, same as upstream). The conclusion holds: that format parses identically before and after.\n- The fix deliberately changes one behaviour: a missing field now keeps its previous value instead of being blanked. In the two-call boot sequence, a boot-folder file with only an IP inherits mask and gateway from the memory-card file read earlier (confirmed in the harness). That is better than an empty mask, but it is a semantic change worth a line in the commit message.\n- Tab-separated files are still misparsed after the fix. This is pre-existing and out of scope.\n\nA host test over LF, single-space, CRLF, double-space and IP-only inputs would pin the bug. There is currently no IPCONFIG test under `tools/`.",
 "corrected_fix": ""
}

## #3 IPCONFIG.DAT written on a PC (CRLF lines or double spaces) parses to an empty netmask and gateway = netmask
- issue: 0
- severity: medium
- category: bug
- files: ['src/SMS_IOP.c']
- reproduced: True
- confidence: 0.9
- risk: Very low: files SMS itself writes (single spaces) parse exactly as before. Network bring-up itself not exercised.

### evidence
_read_ipconfig (SMS_IOP.c:1367-1415) turns ' ', CR and LF into NULs and then assumes exactly ONE separator between fields (`i = strlen(g_pDefIP)+1` ...). Host run of the production text: '192.168.1.10\r\n255.255.255.0\r\n192.168.1.1\r\n' -> ip=[192.168.1.10] mask=[] gw=[255.255.255.0]; double-spaced line -> same; LF-only and single-space files parse correctly; an IP-only file blanks mask and gateway. These values go straight into the SMAP arguments (SMS_IOP.c:538-543) and the IP editor. The parser is upstream-identical, but the fork now reads IPCONFIG.DAT from the ELF folder for card-less consoles (README.md:42, docs/configuration.html), i.e. a file typically created in a Windows editor.

### fix
Replace the five strncpy/strlen lines (SMS_IOP.c:1406-1410) with a 3-iteration loop that skips a run of NULs before each field, stops when the buffer is exhausted (missing fields keep their current value) and strncpy()s 15 bytes into g_pDefIP/g_pDefMask/g_pDefGW. Exact text in scratch apply_fixes.py; host run of the fixed text parses all five sample files correctly; compiles in the patched private tree.

### test
Host test extracting _read_ipconfig (as in scratch repro.py): single-space, LF, CRLF, double-space and IP-only inputs must all yield the expected ip/mask/gw.

### VERDICT diagnosis
{
 "real": true,
 "fix_sound": true,
 "reproduced": true,
 "reasoning": "I tried to refute this and could not. The defect is real, it is a fork regression, and the proposed fix removes it. Nothing was run on a console.\n\n**What I read in the current tree (c275476)**\n- `src/SMS_GUI.c:772` applies the pad override once, inside the cold path of `GUI_Initialize(1)`: `if ( lVideoMode != GSVideoMode_Default ) g_Config.m_DisplayMode = lVideoMode;`. It is stored nowhere else.\n- `src/SMS_Config.c:123-173`: every mass/mmce/pfs/hdd/udpfs boot sets `s_CfgOnFS = 1` and points the config path at the boot device. The early `SMS_LoadConfig` calls (`SMS_GUI.c:726` and `:766`) therefore miss, because the device is not mounted after `SMS_IOPReset`.\n- `src/SMS_IOP.c:1459`: `if ( SMS_ConfigOnFS () && strncmp ( SMS_ConfigPath (), \"mc\", 2 ) != 0 )` mounts the device, then calls `SMS_LoadConfig ()` at `:1559`. The SMB/host/cdrom fallback branch does the same at `:1581`.\n- `src/SMS_Config.c:536` unconditionally sets `m_DisplayMode = GSVideoMode_Default`, then `:627` reads 892 bytes starting at `m_DisplayMode` from the file. Nothing restores the override.\n- `src/main.c:107-110` then calls `GUI_Initialize ( 0 )` when the reloaded mode is not Default (or a skin is set), which runs `GSContext_Init ( g_Config.m_DisplayMode, ... )` at `SMS_GUI.c:815` with the saved mode.\n- The comment at `SMS_IOP.c:1460-1467` itself says the reload \"would CLOBBER the boot-time video-mode override\" and exempts only mc boots.\n- `BDM ?= 1` in the Makefile, so this is the shipped configuration.\n- `SMS_LoadConfig` has exactly four callers: `SMS_GUI.c:726`, `:766`, `SMS_IOP.c:1559`, `:1581`. `SMS_IOPInit` is called once, from `main.c:71`.\n\n**Upstream comparison**\nThe 2.9rev4 clone has only the two `SMS_LoadConfig` calls inside `GUI_Initialize` (`SMS_GUI.c:658` and `:698`), with the override applied after them at `:704`. There is no reload in `SMS_IOPInit`. The reload arrived in fork commit 9256407 (2026-07-09, \"USB/MMCE boot: mount the boot device from argv[0] to load config\") and 017d3d7.\n\n**Host reproduction**\nScript: `(investigator scratch, not kept)/verify-hunt-gui-boot-config/repro.py`. It extracts the production text of `SMS_ConfigSetCWD`, `_derive_smb_path` and `SMS_LoadConfig` from `src/SMS_Config.c` plus the real `SMSConfig` struct, and compiles them with gcc + ASan/UBSan. The fio model makes `mass0:` unreachable until \"mounted\", and a real 896-byte v14 `SMS.cfg` sits on disk. The caller sequence (`SMS_GUI.c:726/766/772/815`, `SMS_IOP.c:1459/1559`, `main.c:107`) is transcribed by hand, so this exercises the real load function but a modelled boot order.\n\n| Case | Baseline final GS mode | With fix |\n|---|---|---|\n| saved 1080i, SELECT+R1 | 0x51 (1080i): override lost | 0x02 (NTSC) |\n| saved 1080i, no combo | 0x51 | 0x51 (unchanged) |\n| saved auto, SELECT+R1 | GS 0x02 but `g_Config.m_DisplayMode` 0xFF | 0x02 / 0x02 |\n\nIn the baseline first case the script printed \"GS after GUI_Initialize(1) = 0x02\", then \"after SMS_IOPInit reload sts=1, g_Config.m_DisplayMode = 0x51\", then \"GUI_Initialize(0) RUNS, FINAL GS mode = 0x51\".\n\nThe third case is a variant the finding did not mention. The config value is clobbered back to Default, so the forced mode is dropped at the next `GSContext_Init` (for example `SMS_Player.c:170` on first playback).\n\n**Fix check**\n- I applied the four-line fix to a private tree (`.../scratchpad/verify-hunt-gui-boot-config/tree`): the global in `SMS_Config.c`, the extern in `SMS_Config.h`, the assignment at `SMS_GUI.c:772`, and the re-apply after `g_Config.m_BrowserFlags &= ~SMS_BF_UDFL;`.\n- The PS2 build with the pinned ee-gcc 3.2.3 docker image produced `bin/SMS.elf` with no new warnings.\n- Host tests `test_display_limits.py` and `test_skin_loading.py`, the only two that read `SMS_Config.c`, gave PASS=2 FAIL=0.\n- The fix is correct for both reload sites. A local save/restore in `SMS_IOP.c` would be wrong for the `:1581` fallback branch, where the early load may already have read a real mc0 config.\n- A plain initialised int global matches existing practice (`g_RandSeed = 1` in `SMS_EE.c`).\n- Later menu changes to the display mode are unaffected, because `SMS_LoadConfig` never runs after boot.\n\n**Caveats**\n- I could not verify that the pad combo is actually detected at that point on real hardware, though that step is identical for mc boots.\n- A forced-mode FS boot now also satisfies the `main.c:107` predicate, so it runs one redundant `GUI_Initialize(0)` to the same mode. That is the same path every saved non-default FS boot already takes.\n- Medium severity is slightly generous: this is a rarely used recovery combo, and it only matters when the saved mode is undisplayable.\n- Unrelated: LeakSanitizer showed `SMS_LoadConfig` leaks its two `SMS_ListInit` heads on every repeat call. That is 8 bytes per reload on PS2, boot-only, and not worth a change here.",
 "corrected_fix": ""
}

## #4 IP editor _set_ips never terminates or bounds the octet cells: stale digits after an unsaved edit, write past the 4-byte cell on a long octet
- issue: 0
- severity: low
- category: bug
- files: ['src/SMS_GUIMenuSMS.c']
- reproduced: True
- confidence: 0.9
- risk: Negligible; only affects what the editor shows/saves.

### evidence
_set_ips (SMS_GUIMenuSMS.c:1723-1737) copies digits into s_IP/s_MSK/s_GW (char[4][4]) with no NUL and no limit on j, and steps past the terminator of a short source. Host run of the production text: after the user rolls the last octet 10 -> 100 (_roll_ip_number sprintf) and leaves without saving, re-opening the editor shows [100] while the live address is still .10 (saving then writes .100); an octet longer than 3 characters from a malformed IPCONFIG.DAT trips UBSan 'index 4 out of bounds for type char[4]'. With the CRLF bug above the empty mask leaves s_MSK untouched (stale). Upstream-identical code.

### fix
In the loop: `for ( ; *apSrc && *apSrc != '.'; ++apSrc ) if ( j < 3 ) apDst[ i ][ j++ ] = *apSrc; apDst[ i ][ j ] = '\x00'; if ( *apSrc ) ++apSrc;` (3 lines). Fixed text passes the same host run and compiles in the patched tree.

### test
Fold into the IPCONFIG host test: extract _set_ips, assert a shorter octet replaces a longer one and a 5-digit octet stays inside its cell under ASan/UBSan.

## #5 Skin / subtitle-font names of 64+ characters overflow g_Config.m_SkinName / m_MBFName when selected in the menu
- issue: 0
- severity: low
- category: bug
- files: ['src/SMS_EE.c', 'src/SMS_GUIMenuSMS.c']
- reproduced: True
- confidence: 0.8
- risk: Very low; over-long skin/font files simply stop appearing in the picker instead of corrupting the config.

### evidence
_usebg_handler (SMS_GUIMenuSMS.c:2106) and _smbcs_handler (:2564) strcpy a list entry into char[64] config fields. The lists are filled by SMS_EEScanDir (SMS_EE.c:752-784), which pushes the dirent name minus its 4-char extension with no length limit, and the fork now scans '<boot dir>Skins' and the boot dir for .mbf on FAT/exFAT long-filename devices (SMS_Config.c:683-688, main.c:86, SMS_GUIFileCtxMenu.c:328); upstream only scanned the memory card (32-char names). Host check with the production SMSConfig struct text: m_SMBIP sits directly after m_SkinName and m_MBFName ends 4 bytes before the m_pSkinList/m_pSMBList/m_pMBFList pointers; copying an 80-char name the way strcpy does overwrites m_SMBIP ('AAAAAAAAAAAAAAAA') and the saved-config fields behind it. Only the struct effect was run on host; the unbounded name source is code reading.

### fix
One condition in SMS_EEScanDir (its only users are these two lists): `if ( lLen > 4 && lLen < 68 )` so a base name that cannot fit the 64-byte fields never enters the list (such a skin could not be stored in SMS.cfg anyway). Compiles in the patched tree; tools/test_skin_loading.py still passes.

### test
Host test extracting SMS_EEScanDir with fioDopen/fioDread stubs: a 63-char base name is listed, a 64-char one is not.

## #6 Dead code and unused statics in boot/config/GUI files (zero behaviour change)
- issue: 0
- severity: low
- category: cleanup
- files: ['src/SMS_IOP.c', 'src/SMS_Config.c', 'src/SMS_GUIDesktop.c', 'src/SMS_GUIMenuSMS.c', 'src/SMS_GUI.c', 'src/SMS_PAD.c', 'include/SMS_PAD.h']
- reproduced: True
- confidence: 0.9
- risk: None for the warning removals (build-verified). The SMS_PAD.c deletion should be checked against the non-BDM configuration before landing, since include/SMS_PAD.h has a separate non-BDM section.

### evidence
Baseline build warnings: SMS_IOP.c:1419-1420 unused locals `i`, `lFD`, `lBuff[64]` in SMS_IOPInit (left behind when the IPCONFIG read moved to _read_ipconfig); SMS_Config.c:61 `s_pSMSCfg` defined but not used; SMS_GUIDesktop.c:590 unused `i` in Desktop_Render; SMS_GUIMenuSMS.c:1452 `_autonet_handler` unused in BDM (only referenced by the non-BDM s_DevMenu row at :258, prototype :74); SMS_GUI.c:90 `s_PadBuf0` unused because BDM's PAD_OpenPort macro (include/SMS_PAD.h:32) expands to nothing. Repo-wide grep (src, include, tools, Makefile): ReadPadStatus, ReadCombinedPadStatus, ReadCombinedPadStatus_raw and `old_pad` are referenced only inside SMS_PAD.c and its header (PAD_Read maps to ReadPadStatus_raw). Verified in the patched private tree: removing the IOPInit locals, s_pSMSCfg, the Desktop_Render `i`, and guarding _autonet_handler with #ifndef BDM builds clean and removes 6 warnings. The s_PadBuf0 guard and the SMS_PAD.c deletions were NOT build-verified here.

### fix
Delete the three IOPInit locals, s_pSMSCfg and the Desktop_Render `i`; wrap the _autonet_handler prototype and body in #ifndef BDM; wrap s_PadBuf0 in #ifndef BDM; delete ReadPadStatus/ReadCombinedPadStatus/ReadCombinedPadStatus_raw/old_pad and their three prototypes (also drop the two old_pad resets in the pad init path).

### test
Existing build is the test: the listed warnings disappear and no new ones appear in a BDM build (and non-BDM, if that configuration is still built in CI, for the #ifndef guards).

## #7 First-ever USB boot (no SMS.cfg on the drive yet) loads the MX4SIO driver and locks MMCE out for the session
- issue: 0
- severity: low
- category: bug
- files: ['src/SMS_IOP.c']
- reproduced: False
- confidence: 0.55
- risk: Medium: touches device bring-up order on the SIO2 bus and USB enumeration timing; must not land without console validation on USB-only and MX4SIO-only setups.

### evidence
Code reading, not run. In the 'mass' boot branch (SMS_IOP.c:1476-1485) the decision 'is this USB or MX4SIO' is made by trying to open the CONFIG FILE: `lCfgFD = fioOpen(lpCfg, O_RDONLY); if (lCfgFD >= 0) fioClose(lCfgFD); else SMS_IOPStartMX4SIO(1);`. A USB user who has never saved settings has no SMS.cfg, so every boot loads mx4sio_bd onto the SIO2 bus, runs its multi-second busy-wait (SMS_IOP.c:721-725) and sets SMS_IOPF_MX4SIO (:731) even with no MX4SIO present; SMS_IOPStartMMCE then refuses for the whole boot (:1025). The root-directory probe that actually answers 'is the device there' already exists a few lines below (:1496-1500).

### fix
Key the MX4SIO fallback on the device root instead of the config file: after SMS_IOPStartUSB(1) build lDev, `lRootFD = fioDopen(lDev); if (lRootFD < 0) { SMS_IOPStartMX4SIO(1); lRootFD = fioDopen(lDev); }` then keep the existing close / lFSGone handling; drop the lCfgFD probe. Net code shrinks. Not built or tested here.

### test
Hardware only: USB boot with and without SMS.cfg, MX4SIO-only boot, and USB+MMCE with no saved config (MMCE must be startable from the device menu).

## #8 'Use XH' browser setting is evaluated before the filesystem config is loaded, so it never takes effect on non-memory-card boots
- issue: 0
- severity: low
- category: bug
- files: ['src/main.c']
- reproduced: False
- confidence: 0.6
- risk: High if moved blindly (kernel-level patch timing); zero if only documented.

### evidence
Code reading, not run. main.c:69 tests `g_Config.m_BrowserFlags & SMS_BF_UXH` right after GUI_Initialize(1). On USB/MX4SIO/MMCE/HDD/UDPFS boots the boot device is not mounted yet, so g_Config still holds SMS_LoadConfig's defaults (SMS_Config.c:532: SDFX|SORT|AVIF, no UXH); the saved value only arrives in SMS_IOPInit (SMS_IOP.c:1559), after the only SMS_OSInit call site. The menu toggle (SMS_GUIMenuSMS.c:2171) therefore saves a flag that is honoured only on memory-card boots.

### fix
Owner decision needed: either move the `if ( ... SMS_BF_UXH ) SMS_OSInit ( argv[ 0 ] );` line below SMS_IOPInit() (one-line move, but SMS_OSInit copies a kernel patch to 0x00082000 and its safety after IOP module loading is unverified), or leave as is and document that the option only applies to memory-card boots. Do not move it without a console test.

### test
Hardware only: enable the option on a USB boot, save, reboot, and check the behaviour it controls with the line in both positions.

